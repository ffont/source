/*
  ==============================================================================

    ServerInterface.h
    Created: 2 Sep 2020 6:04:04pm
    Author:  Frederic Font Corbera

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "defines.h"
#include "BinaryData.h"
#include "httplib.h"
#include "server_wss.hpp"
#include <future>


class ServerInterface;  // Forward delcaration


#if USE_SSL_FOR_HTTP_AND_WS
using WsServer = SimpleWeb::SocketServer<SimpleWeb::WSS>;
#else
using WsServer = SimpleWeb::SocketServer<SimpleWeb::WS>;
#endif

class WebSocketsServer: public juce::Thread
{
public:
    // Websockets server is used for communication between UI<>plugin
   
    WebSocketsServer(): juce::Thread ("SourceWebsocketsServer")
    {
    }
   
    ~WebSocketsServer(){
        if (serverPtr != nullptr){
            serverPtr.release();
        }
    }
    
    void setInterfacePointer(ServerInterface* _interface){
        interfacePtr = _interface;
    }
    
    inline void run();

    int assignedPort = -1;
    ServerInterface* interfacePtr;
    std::unique_ptr<WsServer> serverPtr;
};


class OSCServer: private juce::OSCReceiver,
                 private juce::OSCReceiver::Listener<juce::OSCReceiver::MessageLoopCallback>
{
public:
    // OSC server can be used as an alternative to websockets for communication between UI<>plugin
    
    OSCServer(){
        if (! connect (OSC_LISTEN_PORT)){
            DBG("ERROR starting OSC server");
        } else {
            DBG("Started OSC server, listening at 0.0.0.0:" << OSC_LISTEN_PORT);
            addListener (this);
        }
    }
    
    ~OSCServer(){
    }
    
    void setInterfacePointer(ServerInterface* _interface){
        interface.reset(_interface);
    }
    
    inline void oscMessageReceived (const juce::OSCMessage& message) override;
    
    std::unique_ptr<ServerInterface> interface;
    
};


class HTTPServer: public juce::Thread
{
public:
    // HTTP server is used for serving sounds to the UI so waveforms can be drawn
   
    HTTPServer(): juce::Thread ("SourceHttpServer"){}
   
    ~HTTPServer(){
        if (serverPtr != nullptr){
            serverPtr.release();
        }
    }
    
    void setInterfacePointer(ServerInterface* _interface){
        interface.reset(_interface);
    }
    
    inline void run() {
        #if USE_SSL_FOR_HTTP_AND_WS
        // Write bundled binary SSL cert/key files server can load them
        juce::File sourceDataLocation = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("SourceSampler/");
        juce::File certFile = sourceDataLocation.getChildFile("localhost").withFileExtension("crt");
        certFile.replaceWithData(BinaryData::localhost_crt, BinaryData::localhost_crtSize);
        juce::File keyFile = sourceDataLocation.getChildFile("localhost").withFileExtension("key");
        keyFile.replaceWithData(BinaryData::localhost_key, BinaryData::localhost_keySize);
        httplib::SSLServer server(static_cast<const char*> (certFile.getFullPathName().toUTF8()), static_cast<const char*> (keyFile.getFullPathName().toUTF8()));
        #else
        httplib::Server server;
        #endif
            
        juce::File tmpFilesLocation = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("SourceSampler/tmp");
        auto ret = server.set_mount_point("/sounds_data", static_cast<const char*> (tmpFilesLocation.getFullPathName().toUTF8()));
        if (!ret) {
            DBG("Can't serve sound files from directory as directory does not exist");
        }
        
        server.set_file_request_handler([](const auto& req, auto& res) {
            res.set_header("Access-Control-Allow-Origin", "*");
        });
        
        #if !JUCE_DEBUG
        // In non debug desktop builds we want each instace to use a separate port so that each instance has its own interface
        port = server.bind_to_any_port("0.0.0.0");
        #else
        // In debug builds we also want a known port so it is easy to test from browser
        port = HTTP_SERVER_PORT;
        server.bind_to_port("0.0.0.0", port);
        #endif
        serverPtr.reset(&server);
        DBG("Started HTTP server, listening at 0.0.0.0:" + (juce::String)(port));
        server.listen_after_bind();
    }
    
    #if USE_SSL_FOR_HTTP_AND_WS
    std::unique_ptr<httplib::SSLServer> serverPtr;
    #else
    std::unique_ptr<httplib::Server> serverPtr;
    #endif
    int port = 0;
    std::unique_ptr<ServerInterface> interface;
};


class ServerInterface: public juce::ActionBroadcaster
{
public:
    ServerInterface ()
    {
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
            wsServer.serverPtr->stop(); // some other method
        }
        wsServer.stopThread(5000);  // Give it enough time to stop the websockets server...
        
        #if USE_HTTP_SERVER
        httpServer.interface.release();
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
    HTTPServer httpServer;
    #endif
    #if USE_OSC_SERVER
    OSCServer oscServer;
    #endif
    WebSocketsServer wsServer;
    juce::String serializedAppState;
    juce::String serializedAppStateVolatile;
};



void WebSocketsServer::run()
{
    #if USE_SSL_FOR_HTTP_AND_WS
    juce::File sourceDataLocation = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("SourceSampler/");
    juce::File certFile = sourceDataLocation.getChildFile("localhost").withFileExtension("crt");
    certFile.replaceWithData(BinaryData::localhost_crt, BinaryData::localhost_crtSize);
    juce::File keyFile = sourceDataLocation.getChildFile("localhost").withFileExtension("key");
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


void OSCServer::oscMessageReceived (const juce::OSCMessage& message)
{
    if (interface != nullptr){
        interface->processActionFromOSCMessage(message);
    }
}
