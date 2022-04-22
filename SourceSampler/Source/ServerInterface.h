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
#if USE_WEBSOCKETS
#include "ws_server_certificate.hpp"
#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <string>
#include <thread>
namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace websocket = beast::websocket; // from <boost/beast/websocket.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
namespace ssl = boost::asio::ssl;       // from <boost/asio/ssl.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>
#endif
#if USE_HTTP_SERVER
#include "httplib.h"
#endif


class ServerInterface;  // Forward delcaration


#if USE_WEBSOCKETS

void handle_ws_session(tcp::socket socket, ssl::context& ctx, ServerInterface* interfacePtr);  // Forward delcaration

class WebSocketsServer: public Thread
{
public:
   
    WebSocketsServer(): Thread ("SourceWebsocketsServer")
    {
    }
   
    ~WebSocketsServer()
    {
    }
    
    void setInterfacePointer(ServerInterface* _interface){
        interfacePtr = _interface;
    }
    
    inline void run()
    {
        try
        {
            auto const address = net::ip::make_address("0.0.0.0");
            auto const port = static_cast<unsigned short>(WEBSOCKETS_SERVER_PORT);

            // The io_context is required for all I/O
            net::io_context ioc{1};

            // The SSL context is required, and holds certificates
            ssl::context ctx{ssl::context::tlsv12};

            // This holds the self-signed certificate used by the server
            load_server_certificate(ctx);
            
            DBG("Starting websockets server, listening at 0.0.0.0, port " << port);

            // The acceptor receives incoming connections
            tcp::acceptor acceptor{ioc, {address, port}};
            for(;;)
            {
                // This will receive the new connection
                tcp::socket socket{ioc};

                // Block until we get a connection
                acceptor.accept(socket);

                // Launch the session, transferring ownership of the socket
                std::thread(
                    &handle_ws_session,
                    std::move(socket),
                    std::ref(ctx),
                    interfacePtr).detach();
            }
        }
        catch (const std::exception& e)
        {
            DBG("Error: " << e.what());
        }
    }
    
    ServerInterface* interfacePtr;
    std::vector<websocket::stream<beast::ssl_stream<tcp::socket&>>*> wsSessions;
};
#endif


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
    
    bool receivedRequestsRecently(){
        if ((lastTimeRequestReceived > 0) && (juce::Time::getMillisecondCounterHiRes() - lastTimeRequestReceived > 5000)){
            return false;
        }
        return true;
    }
    
    double lastTimeRequestReceived = 0;
    
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
        //wsServer.interface.release();
        // wsServer run some beast stop methods?
        wsServer.stopThread(5000);  // Give it enough time to stop the websockets server...
        #endif
    }
    
    void log(String message){
        DBG(message);
    }
    
    void addWsSession(websocket::stream<beast::ssl_stream<tcp::socket&>>* s){
        const juce::ScopedLock sl (wsSessionAddRemoveLock);
        wsServer.wsSessions.push_back(s);
        DBG("Active WS sessions: " << (juce::String)wsServer.wsSessions.size());
    }
    
    void removeWsSession(websocket::stream<beast::ssl_stream<tcp::socket&>>* s){
        const juce::ScopedLock sl (wsSessionAddRemoveLock);
        int position = -1;
        for (int i=0; i<wsServer.wsSessions.size(); i++){
            if (wsServer.wsSessions[i] == s){
                position = i;
            }
        }
        if (position > -1){
            wsServer.wsSessions.erase(wsServer.wsSessions.begin() + position);
            DBG("Session " << position << " removed");
        } else {
            DBG("No session to remove");
        }
        DBG("Active WS sessions: " << (juce::String)wsServer.wsSessions.size());
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
    
    #if USE_HTTP_SERVER
    HTTPServer httpServer;
    #endif
    #if ENABLE_OSC_SERVER
    OSCServer oscServer;
    #endif
    #if USE_WEBSOCKETS
    WebSocketsServer wsServer;
    juce::CriticalSection wsSessionAddRemoveLock;
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
    
    // Some server configuration
    
    File tmpFilesLocation = File::getSpecialLocation(File::userDocumentsDirectory).getChildFile("SourceSampler/tmp");
    auto ret = server.set_mount_point("/sounds_data", static_cast<const char*> (tmpFilesLocation.getFullPathName().toUTF8()));
    if (!ret) {
        DBG("Can't serve sound files from directory as directory does not exist");
    }
    
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
        interface->log("Started HTTP server, listening at 0.0.0.0:" + (String)(port));
    }
    server.listen_after_bind();
}

#if USE_WEBSOCKETS
inline void handle_ws_session(tcp::socket socket, ssl::context& ctx, ServerInterface* interfacePtr)
{
    
    websocket::stream<beast::ssl_stream<tcp::socket&>> ws{socket, ctx};
    interfacePtr->addWsSession(&ws);
    
    try
    {
        DBG("Starting websockets session");
        
        // Construct the websocket stream around the socket
        //websocket::stream<beast::ssl_stream<tcp::socket&>> ws{socket, ctx};
        //interfacePtr->addWsSession(&ws);

        // Perform the SSL handshake
        ws.next_layer().handshake(ssl::stream_base::server);

        // Set a decorator to change the Server of the handshake
        ws.set_option(websocket::stream_base::decorator(
            [](websocket::response_type& res)
            {
                res.set(http::field::server,
                    std::string(BOOST_BEAST_VERSION_STRING) +
                        " websocket-server-sync-ssl");
            }));

        // Accept the websocket handshake
        ws.accept();

        for(;;)
        {
            // This buffer will hold the incoming message
            beast::flat_buffer buffer;

            // Read a message
            ws.read(buffer);

            // Echo the message back
            ws.text(ws.got_text());
            ws.write(buffer.data());
            OSCMessage message = OSCMessage("/test");
            message.addInt32(1);
            interfacePtr->processActionFromOSCMessage(message);
        }
    }
    catch(beast::system_error const& se)
    {
        // This indicates that the session was closed
        if(se.code() != websocket::error::closed)
            std::cerr << "Error: " << se.code().message() << std::endl;
    }
    catch(std::exception const& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
    }
    interfacePtr->removeWsSession(&ws);
}
#endif


void OSCServer::oscMessageReceived (const OSCMessage& message)
{
    if (interface != nullptr){
        interface->processActionFromOSCMessage(message);
    }
}
