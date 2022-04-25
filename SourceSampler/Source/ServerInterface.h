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
#if USE_WEBSOCKETS
#include "server_wss.hpp"
#include <future>
#endif


class ServerInterface;  // Forward delcaration


#if USE_WEBSOCKETS

#ifdef CPPHTTPLIB_OPENSSL_SUPPORT
using WsServer = SimpleWeb::SocketServer<SimpleWeb::WSS>;
#else
using WsServer = SimpleWeb::SocketServer<SimpleWeb::WS>;
#endif

class WebSocketsServer: public juce::Thread
{
public:
   
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
#endif


class HTTPServer: public juce::Thread
{
public:
   
    HTTPServer(): juce::Thread ("SourceHttpServer"){}
   
    ~HTTPServer(){
        if (serverPtr != nullptr){
            serverPtr.release();
        }
    }
    
    void setInterfacePointer(ServerInterface* _interface){
        interface.reset(_interface);
    }
    
    inline void run(); // Defined below
    
    #ifdef CPPHTTPLIB_OPENSSL_SUPPORT
    std::unique_ptr<httplib::SSLServer> serverPtr;
    #else
    std::unique_ptr<httplib::Server> serverPtr;
    #endif
    bool connected = false;
    int port = HTTP_SERVER_LISTEN_PORT;
    std::unique_ptr<ServerInterface> interface;
    
    bool receivedRequestsRecently(){
        if ((lastTimeRequestReceived > 0) && (juce::Time::getMillisecondCounterHiRes() - lastTimeRequestReceived > 5000)){
            return false;
        }
        return true;
    }
    
    double lastTimeRequestReceived = 0;
    
};

class OSCServer: private juce::OSCReceiver,
                 private juce::OSCReceiver::Listener<juce::OSCReceiver::MessageLoopCallback>
{
public:
    
    OSCServer(){
        // Start listening on OSC port
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


class ServerInterface: public juce::ActionBroadcaster
{
public:
    ServerInterface ()
    {
        #if USE_HTTP_SERVER
        httpServer.setInterfacePointer(this);
        httpServer.startThread(10); // Max priority
        #endif
        
        #if ENABLE_OSC_SERVER
        oscServer.setInterfacePointer(this);
        #endif
        
        #if USE_WEBSOCKETS
        wsServer.setInterfacePointer(this);
        wsServer.startThread(0);
        #endif
    }
    
    ~ServerInterface ()
    {
        #if USE_HTTP_SERVER
        httpServer.interface.release();
        if (httpServer.serverPtr != nullptr){
            httpServer.serverPtr->stop();
        }
        httpServer.stopThread(5000);  // Give it enough time to stop the http server...
        #endif
        
        #if ENABLE_OSC_SERVER
        oscServer.interface.release();
        #endif
        
        #if USE_WEBSOCKETS
        if (wsServer.serverPtr != nullptr){
            wsServer.serverPtr->stop(); // some other method
        }
        wsServer.stopThread(5000);  // Give it enough time to stop the websockets server...
        #endif
    }
    
    void log(juce::String message){
        DBG(message);
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
    
    void sendMessageToWebSocketsClients(const juce::OSCMessage& message)
    {
        #if USE_WEBSOCKETS
        // Takes a OSC message object and serializes in a way that can be sent to WebSockets conencted clients
        for(auto &a_connection : wsServer.serverPtr->get_connections()){
            juce::String serializedMessage = serliaizeOSCMessage(message);
            a_connection->send(serializedMessage.toStdString());
        }
        #endif
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
    #if ENABLE_OSC_SERVER
    OSCServer oscServer;
    #endif
    #if USE_WEBSOCKETS
    WebSocketsServer wsServer;
    #endif
    juce::String serializedAppState;
    juce::String serializedAppStateVolatile;
};


void HTTPServer::run()
{
    #ifdef CPPHTTPLIB_OPENSSL_SUPPORT
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
    
    // Some server configuration
    
    juce::File tmpFilesLocation = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("SourceSampler/tmp");
    auto ret = server.set_mount_point("/sounds_data", static_cast<const char*> (tmpFilesLocation.getFullPathName().toUTF8()));
    if (!ret) {
        DBG("Can't serve sound files from directory as directory does not exist");
    }
    
    server.Get("/", [](const httplib::Request &, httplib::Response &res) {
        juce::String contents = juce::String::fromUTF8 (BinaryData::ui_plugin_html, BinaryData::ui_plugin_htmlSize);
        res.set_content(contents.toStdString(), "text/html");
    });
    
    server.Get("/send_osc", [this](const httplib::Request &req, httplib::Response &res) {
        // NOTE: this method is named like that for legacy reasons. It does not actually send any OSC messages.
        // Before having an http server intergated with the plugin, there was an external http server that would
        // send OSC messages to the plugin. The way we do it now is that the http server embeded in the plugin
        // simulates sending OSC messages to the plugin (so the same control API is used). We don't really send
        // OSC messages through the network, but we generate a "fake" OSC message and call the same plugin method
        // that would be called by the OSC receiver.
        
        if (interface != nullptr){
            // Parse get parameters and transform to OSC message that we feed to the
            // ServerInterface as if we received OSC. In this way the interface is unified
            juce::String oscAddress = "";
            juce::StringArray types;
            juce::StringArray values;
            
            std::multimap<std::string, std::string>::const_iterator it;
            for (it = req.params.begin(); it != req.params.end(); ++it){
                if ((juce::String)it->first == "address"){
                    oscAddress = (juce::String)it->second;
                } else if ((juce::String)it->first == "values"){
                    values.addTokens ((juce::String)it->second, ";", "");
                } else if ((juce::String)it->first == "types"){
                    types.addTokens ((juce::String)it->second, ";", "");
                }
            }
            juce::OSCMessage message = juce::OSCMessage(oscAddress);
            for (int i=0; i<types.size(); i++){
                if (types[i] == "int"){
                    message.addInt32(values[i].getIntValue());
                } else if (types[i] == "float"){
                    message.addFloat32(values[i].getFloatValue());
                } else if (types[i] == "str"){
                    message.addString(values[i]);
                }
            }
            interface->processActionFromOSCMessage(message);
        }
    });
    
    server.Get("/update_state", [this](const httplib::Request &, httplib::Response &res) {
        
        lastTimeRequestReceived = juce::Time::getMillisecondCounterHiRes();
        
        //DBG("updating state from server UI " << juce::Time::getCurrentTime().formatted("%Y%m%d %M:%S") );
        if (interface != nullptr){
            res.set_content((interface->serializedAppState).toStdString(), "text/xml");
        } else {
            res.set_content("", "text/xml");
        }
    });
    
    server.Get("/update_state_volatile", [this](const httplib::Request &, httplib::Response &res) {
        if (interface != nullptr){
            res.set_content((interface->serializedAppStateVolatile).toStdString(), "text/xml");
        } else {
            res.set_content("", "text/xml");
        }
    });
    
    #if !ELK_BUILD
        #if !JUCE_DEBUG
        // In desktop builds we want each instace to use a separate port so that each instance has its own interface
        port = server.bind_to_any_port("0.0.0.0");
        #else
        server.bind_to_port("0.0.0.0", port);
        #endif
    #else
    // In ELK build there will allways going to be a single instance for the plugin, run it at a port known to be free
    server.bind_to_port("0.0.0.0", port);
    #endif
    connected = true;
    serverPtr.reset(&server);
    if (interface != nullptr){
        interface->log("Started HTTP server, listening at 0.0.0.0:" + (juce::String)(port));
    }
    server.listen_after_bind();
}

#if USE_WEBSOCKETS
void WebSocketsServer::run()
{
    #ifdef CPPHTTPLIB_OPENSSL_SUPPORT
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
    
    auto &source_endpoint = server.endpoint["^/source/?$"];
    source_endpoint.on_message = [&server, this](std::shared_ptr<WsServer::Connection> /*connection*/, std::shared_ptr<WsServer::InMessage> in_message) {
        auto message = in_message->string();
        if (interfacePtr != nullptr){
            interfacePtr->processActionFromSerializedMessage(juce::String(message));
        }
    };
    
    server.start([this](unsigned short port) {
        assignedPort = port;
        DBG("Started Websockets Server listening at 0.0.0.0:" << port);
    });
}
#endif


void OSCServer::oscMessageReceived (const juce::OSCMessage& message)
{
    if (interface != nullptr){
        interface->processActionFromOSCMessage(message);
    }
}
