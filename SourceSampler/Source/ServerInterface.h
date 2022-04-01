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
#include "httplib.h"
#include "BinaryData.h"


class ServerInterface;


class HTTPServer: public Thread
{
public:
   
    HTTPServer(): Thread ("SourceHttpServer"){}
   
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
    
};

class OSCServer: private OSCReceiver,
                 private OSCReceiver::Listener<OSCReceiver::MessageLoopCallback>
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
    
    inline void oscMessageReceived (const OSCMessage& message) override;
    
    std::unique_ptr<ServerInterface> interface;
    
};


class ServerInterface: public ActionBroadcaster
{
public:
    ServerInterface ()
    {
        #if ENABLE_EMBEDDED_HTTP_SERVER
        httpServer.setInterfacePointer(this);
        httpServer.startThread(0); // Lowest priority
        #endif
        
        #if ENABLE_OSC_SERVER
        oscServer.setInterfacePointer(this);
        #endif
    }
    
    ~ServerInterface ()
    {
        #if ENABLE_EMBEDDED_HTTP_SERVER
        httpServer.interface.release();
        if (httpServer.serverPtr != nullptr){
            httpServer.serverPtr->stop();
        }
        httpServer.stopThread(5000);  // Give it enough time to stop the http server...
        #endif
        
        #if ENABLE_OSC_SERVER
        oscServer.interface.release();
        #endif
    }
    
    void log(String message){
        DBG(message);
    }
        
    void processActionFromOSCMessage (const OSCMessage& message)
    {
        // Trigger the OSC message as an Action Message in the plugin Processor
        String actionName = message.getAddressPattern().toString();
        StringArray actionParameters = {};
        for (int i=0; i<message.size(); i++){
            if (message[i].isString()){
                actionParameters.add(message[i].getString());
            } else if (message[i].isInt32()){
                actionParameters.add((String)message[i].getInt32());
            } else if (message[i].isFloat32()){
                actionParameters.add((String)message[i].getFloat32());
            }
        }
        String serializedParameters = actionParameters.joinIntoString(SERIALIZATION_SEPARATOR);
        String actionMessage = actionName + ":" + serializedParameters;
        sendActionMessage(actionMessage);
    }
    
    #if ENABLE_EMBEDDED_HTTP_SERVER
    HTTPServer httpServer;
    #endif
    #if ENABLE_OSC_SERVER
    OSCServer oscServer;
    #endif
    String serializedAppState;
    String serializedAppStateVolatile;
};


void HTTPServer::run()
{
    #ifdef CPPHTTPLIB_OPENSSL_SUPPORT
    File sourceDataLocation = File::getSpecialLocation(File::userDocumentsDirectory).getChildFile("SourceSampler/");
    File certFile = sourceDataLocation.getChildFile("localhost").withFileExtension("crt");
    if (!certFile.existsAsFile()){
        // Write bundled binary resource to file so https server can load it
        certFile.replaceWithData(BinaryData::localhost_crt, BinaryData::localhost_crtSize);
    }
    File keyFile = sourceDataLocation.getChildFile("localhost").withFileExtension("key");
    if (!keyFile.existsAsFile()){
        // Write bundled binary resource to file so https server can load it
        keyFile.replaceWithData(BinaryData::localhost_key, BinaryData::localhost_keySize);
    }
    httplib::SSLServer server(static_cast<const char*> (certFile.getFullPathName().toUTF8()), static_cast<const char*> (keyFile.getFullPathName().toUTF8()));
    #else
    httplib::Server server;
    #endif
    
    server.Get("/", [](const httplib::Request &, httplib::Response &res) {
        String contents = String::fromUTF8 (BinaryData::ui_plugin_html, BinaryData::ui_plugin_htmlSize);
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
            String oscAddress = "";
            StringArray types;
            StringArray values;
            
            std::multimap<std::string, std::string>::const_iterator it;
            for (it = req.params.begin(); it != req.params.end(); ++it){
                if ((String)it->first == "address"){
                    oscAddress = (String)it->second;
                } else if ((String)it->first == "values"){
                    values.addTokens ((String)it->second, ";", "");
                } else if ((String)it->first == "types"){
                    types.addTokens ((String)it->second, ";", "");
                }
            }
            OSCMessage message = OSCMessage(oscAddress);
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
    if (interface != nullptr){
        interface->log("Started HTTP server, listening at 0.0.0.0:" + (String)(port));
    }
    connected = true;
    serverPtr.reset(&server);
    server.listen_after_bind();
}


void OSCServer::oscMessageReceived (const OSCMessage& message)
{
    //DBG("Received OSC message at address: " + message.getAddressPattern().toString());
    if (interface != nullptr){
        interface->processActionFromOSCMessage(message);
    }
}
