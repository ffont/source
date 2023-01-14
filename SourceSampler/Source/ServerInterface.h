/*
  ==============================================================================

    ServerInterface.h
    Created: 2 Sep 2020 6:04:04pm
    Author:  Frederic Font Corbera

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "defines_source.h"
#if USE_SSL_FOR_HTTP_AND_WS
    #include "BinaryData.h"
    #include "server_wss.hpp"
    #if USE_HTTP_SERVER
    #include "server_https.hpp"
    #endif
#else
    #include "server_ws.hpp"
    #if USE_HTTP_SERVER
    #include "server_http.hpp"
    #endif
#endif
#include <future>
#include <fstream>


class ServerInterface;  // Forward delcaration


#if USE_SSL_FOR_HTTP_AND_WS
using WsServer = SimpleWeb::SocketServer<SimpleWeb::WSS>;
#else
using WsServer = SimpleWeb::SocketServer<SimpleWeb::WS>;
#endif

class SourceWebSocketsServer: public juce::Thread
{
public:
    // Websockets server is used for communication between UI<>plugin
   
    SourceWebSocketsServer(): juce::Thread ("SourceWebsocketsServer")
    {
    }
   
    ~SourceWebSocketsServer(){
        if (serverPtr != nullptr){
            serverPtr.release();
        }
    }
    
    void setInterfacePointer(ServerInterface* _interface){
        interfacePtr = _interface;
    }
    
    inline void run();  // Implemented after ServerInterface is defined

    int assignedPort = -1;
    ServerInterface* interfacePtr;
    std::unique_ptr<WsServer> serverPtr;
};


class SourceOSCServer: private juce::OSCReceiver,
                 private juce::OSCReceiver::Listener<juce::OSCReceiver::MessageLoopCallback>
{
public:
    // OSC server can be used as an alternative to websockets for communication between UI<>plugin
    
    SourceOSCServer(){
        if (! connect (OSC_LISTEN_PORT)){
            DBG("ERROR starting OSC server");
        } else {
            DBG("Started OSC server, listening at 0.0.0.0:" << OSC_LISTEN_PORT);
            addListener (this);
        }
    }
    
    ~SourceOSCServer(){
    }
    
    void setInterfacePointer(ServerInterface* _interface){
        interface.reset(_interface);
    }
    
    inline void oscMessageReceived (const juce::OSCMessage& message) override;
    
    std::unique_ptr<ServerInterface> interface;
    
};

#if USE_HTTP_SERVER
#if USE_SSL_FOR_HTTP_AND_WS
using HttpServer = SimpleWeb::Server<SimpleWeb::HTTPS>;
#else
using HttpServer = SimpleWeb::Server<SimpleWeb::HTTP>;
#endif
#endif


#if USE_HTTP_SERVER
class SourceHTTPServer: public juce::Thread
{
public:
    // HTTP server is used for serving sounds to the UI so waveforms can be drawn
   
    SourceHTTPServer(): juce::Thread ("SourceHttpServer"){}
   
    ~SourceHTTPServer(){
        if (serverPtr != nullptr){
            serverPtr.release();
        }
    }
    
    void setInterfacePointer(ServerInterface* _interface){
        interfacePtr = _interface;
    }
    
    inline void run(); // Implemented after ServerInterface is defined
    
    
    int assignedPort = -1;
    ServerInterface* interfacePtr;
    std::unique_ptr<HttpServer> serverPtr;
};
#endif

class ServerInterface: public juce::ActionBroadcaster
{
public:
    ServerInterface (std::function<GlobalContextStruct()> globalContextGetter)
    {
        getGlobalContext = globalContextGetter;
        
        #if USE_HTTP_SERVER
        httpServer.setInterfacePointer(this);
        httpServer.startThread(0);
        #endif
        
        #if USE_OSC_SERVER
        oscServer.setInterfacePointer(this);
        #endif
        
        wsServer.setInterfacePointer(this);
        wsServer.startThread(0);
    }
    
    ~ServerInterface ()
    {
        if (wsServer.serverPtr != nullptr){
            wsServer.serverPtr->stop();
        }
        wsServer.stopThread(5000);  // Give it enough time to stop the websockets server...
        
        #if USE_HTTP_SERVER
        if (httpServer.serverPtr != nullptr){
            httpServer.serverPtr->stop();
        }
        httpServer.stopThread(5000);  // Give it enough time to stop the http server...
        #endif
        
        #if USE_OSC_SERVER
        oscServer.interface.release();
        #endif
    }
    
    juce::String serliaizeOSCMessage(const juce::OSCMessage& message)
    {
        juce::String actionName = message.getAddressPattern().toString();
        juce::StringArray actionParameters = {};
        for (int i=0; i<message.size(); i++){
            if (message[i].isString()){
                actionParameters.add(message[i].getString());
            } else if (message[i].isInt32()){
                actionParameters.add((juce::String)message[i].getInt32());
            } else if (message[i].isFloat32()){
                actionParameters.add((juce::String)message[i].getFloat32());
            }
        }
        juce::String serializedParameters = actionParameters.joinIntoString(SERIALIZATION_SEPARATOR);
        juce::String actionMessage = actionName + ":" + serializedParameters;
        return actionMessage;
    }
    
    void sendMessageToWebSocketClients(const juce::OSCMessage& message)
    {
        if (wsServer.serverPtr == nullptr){
            // If ws server is not yet running, don't try to send any message
            return;
        }
        // Takes a OSC message object and serializes in a way that can be sent to WebSockets conencted clients
        for(auto &a_connection : wsServer.serverPtr->get_connections()){
            juce::String serializedMessage = serliaizeOSCMessage(message);
            a_connection->send(serializedMessage.toStdString());
        }
    }
        
    void processActionFromOSCMessage (const juce::OSCMessage& message)
    {
        // Trigger the OSC message as an Action Message in the plugin Processor
        juce::String actionMessage = serliaizeOSCMessage(message);
        sendActionMessage(actionMessage);
    }
    
    void processActionFromSerializedMessage (const juce::String& message)
    {
        sendActionMessage(message);
    }
    
    #if USE_HTTP_SERVER
    SourceHTTPServer httpServer;
    #endif
    #if USE_OSC_SERVER
    SourceOSCServer oscServer;
    #endif
    SourceWebSocketsServer wsServer;

    std::function<GlobalContextStruct()> getGlobalContext;
};


void SourceWebSocketsServer::run()
{
    #if USE_SSL_FOR_HTTP_AND_WS
    #if ELK_BUILD
    juce::File baseLocation = juce::File(ELK_SOURCE_TMP_LOCATION);
    #else
    juce::File baseLocation = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("SourceSampler/tmp");
    #endif
    if (!baseLocation.exists()){
        baseLocation.createDirectory();
    }
    juce::File certFile = baseLocation.getChildFile("localhost").withFileExtension("crt");
    certFile.replaceWithData(BinaryData::localhost_crt, BinaryData::localhost_crtSize);
    juce::File keyFile = baseLocation.getChildFile("localhost").withFileExtension("key");
    keyFile.replaceWithData(BinaryData::localhost_key, BinaryData::localhost_keySize);
    WsServer server(static_cast<const char*> (certFile.getFullPathName().toUTF8()), static_cast<const char*> (keyFile.getFullPathName().toUTF8()));
    #else
    WsServer server;
    #endif
    
    #if !ELK_BUILD
        #if !JUCE_DEBUG
        // In non debug desktop builds we want each instace to use a separate port so that each instance has its own interface
        server.config.port = 0;
        #else
        // In debug builds we also want a known port so it is easy to test from browser
        server.config.port = WEBSOCKETS_SERVER_PORT;
        #endif
    #else
    server.config.port = WEBSOCKETS_SERVER_PORT;  // Use a known port so python UI can connect to it
    #endif
    serverPtr.reset(&server);
    
    auto &source_coms_endpoint = server.endpoint["^/source_coms/?$"];
    source_coms_endpoint.on_message = [&server, this](std::shared_ptr<WsServer::Connection> /*connection*/, std::shared_ptr<WsServer::InMessage> in_message) {
        juce::String message = juce::String(in_message->string());
        if (interfacePtr != nullptr){
            interfacePtr->processActionFromSerializedMessage(message);
        }
    };
    
    server.start([this](unsigned short port) {
        assignedPort = port;
        DBG("Started Websockets Server listening at 0.0.0.0:" << port);
    });
}

#if USE_HTTP_SERVER
void SourceHTTPServer::run() {
    #if USE_SSL_FOR_HTTP_AND_WS
    #if ELK_BUILD
    juce::File baseLocation = juce::File(ELK_SOURCE_TMP_LOCATION);
    #else
    juce::File baseLocation = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("SourceSampler/tmp");
    #endif
    if (!baseLocation.exists()){
        baseLocation.createDirectory();
    }
    juce::File certFile = baseLocation.getChildFile("localhost").withFileExtension("crt");
    certFile.replaceWithData(BinaryData::localhost_crt, BinaryData::localhost_crtSize);
    juce::File keyFile = baseLocation.getChildFile("localhost").withFileExtension("key");
    keyFile.replaceWithData(BinaryData::localhost_key, BinaryData::localhost_keySize);
    HttpServer server(static_cast<const char*> (certFile.getFullPathName().toUTF8()), static_cast<const char*> (keyFile.getFullPathName().toUTF8()));
    #else
    HttpServer server;
    #endif

    #if !ELK_BUILD
        #if !JUCE_DEBUG
        // In non debug desktop builds we want each instace to use a separate port so that each instance has its own interface
        server.config.port = 0;
        #else
        // In debug builds we also want a known port so it is easy to test from browser
        server.config.port = HTTP_SERVER_PORT;
        #endif
    #else
    server.config.port = HTTP_SERVER_PORT;  // Use a known port so python UI can connect to it
    #endif
    serverPtr.reset(&server);
    
    // Configure serving WAV files from the tmp folder statically (this is where the sound files are placed)
    #if ELK_BUILD
    juce::String tmpFilesPathName = juce::File(ELK_SOURCE_TMP_LOCATION).getFullPathName();
    #else
    juce::String tmpFilesPathName = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("SourceSampler/tmp").getFullPathName();
    #endif
    server.resource["^/sounds_data/.*$"]["GET"] = [tmpFilesPathName](std::shared_ptr<HttpServer::Response> response, std::shared_ptr<HttpServer::Request> request) {
        try {
          juce::String path = (juce::String)request->path;
          path = tmpFilesPathName + path.replace("/sounds_data/", "/");
          
          SimpleWeb::CaseInsensitiveMultimap header;

          auto ifs = std::make_shared<std::ifstream>();
          ifs->open(path.toStdString(), std::ifstream::in | std::ios::binary | std::ios::ate);
            
          DBG("Serving file from HTTP server: " << path);

          if(*ifs) {
            auto length = ifs->tellg();
            ifs->seekg(0, std::ios::beg);

            header.emplace("Content-Length", std::to_string(length));
            header.emplace("Content-Type", "audio/wave");
            header.emplace("Access-Control-Allow-Origin", "*");
            response->write(header);

            // Trick to define a recursive function within this scope (for example purposes)
            class FileServer {
            public:
              static void read_and_send(const std::shared_ptr<HttpServer::Response> &response, const std::shared_ptr<std::ifstream> &ifs) {
                // Read and send 128 KB at a time, safe when server is running on one thread
                static std::vector<char> buffer(128 * 1024);
                  std::streamsize read_length;
                if((read_length = ifs->read(&buffer[0], static_cast<std::streamsize>(buffer.size())).gcount()) > 0) {
                  response->write(&buffer[0], read_length);
                  if(read_length == static_cast<std::streamsize>(buffer.size())) {
                    response->send([response, ifs](const SimpleWeb::error_code &ec) {
                      if(!ec)
                        read_and_send(response, ifs);
                      else
                        std::cerr << "Connection interrupted" << std::endl;
                    });
                  }
                }
              }
            };
            FileServer::read_and_send(response, ifs);
          }
          else
            throw std::invalid_argument("could not read file");
        }
        catch(const std::exception &e) {
          response->write(SimpleWeb::StatusCode::client_error_bad_request, "Could not open path " + request->path + ": " + e.what());
        }
    };

    server.start([this](unsigned short port) {
        assignedPort = port;
        DBG("Started HttpServer Server listening at 0.0.0.0:" << port);
    });

}
#endif

void SourceOSCServer::oscMessageReceived (const juce::OSCMessage& message)
{
    if (interface != nullptr){
        interface->processActionFromOSCMessage(message);
    }
}
