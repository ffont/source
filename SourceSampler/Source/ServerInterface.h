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
    
    std::string exec(const char* cmd) {
        std::array<char, 128> buffer;
        std::string result;
        std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
        if (!pipe) {
            throw std::runtime_error("popen() failed!");
        }
        while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
            result += buffer.data();
        }
        return result;
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
        if (message.getAddressPattern().toString() == OSC_ADDRESS_NEW_QUERY){
            if (message.size() == 5)  {
                String query = message[0].getString();
                int numSounds = message[1].getInt32();
                float minLength = message[2].getFloat32();
                float maxLength = message[3].getFloat32();
                int noteMappingType = message[4].getInt32();
                String separator = ";";
                String serializedParameters = query + SERIALIZATION_SEPARATOR +
                                              (String)numSounds + SERIALIZATION_SEPARATOR +
                                              (String)minLength + SERIALIZATION_SEPARATOR +
                                              (String)maxLength + SERIALIZATION_SEPARATOR +
                                              (String)noteMappingType + SERIALIZATION_SEPARATOR;
                String actionMessage = String(ACTION_NEW_QUERY_TRIGGERED_FROM_SERVER) + ":" + serializedParameters;
                sendActionMessage(actionMessage);
            }
        } else if (message.getAddressPattern().toString() == OSC_ADDRESS_SET_SOUND_PARAMETER_FLOAT){
            if (message.size() == 3)  {
                int soundIndex = message[0].getInt32();  // Index of the sound in SourceSamplerSynthesiser object (-1 means all sounds)
                String parameterName = message[1].getString();  // Name of the parameter to change
                float value = message[2].getFloat32();  // Value of the parameter to set
                String serializedParameters = (String)soundIndex + SERIALIZATION_SEPARATOR +
                                              parameterName + SERIALIZATION_SEPARATOR  +
                                              (String)value + SERIALIZATION_SEPARATOR;
                String actionMessage = String(ACTION_SET_SOUND_PARAMETER_FLOAT) + ":" + serializedParameters;
                sendActionMessage(actionMessage);
            }
        } else if (message.getAddressPattern().toString() == OSC_ADDRESS_SET_SOUND_PARAMETER_INT){
            if (message.size() == 3)  {
                int soundIndex = message[0].getInt32();  // Index of the sound in SourceSamplerSynthesiser object (-1 means all sounds)
                String parameterName = message[1].getString();  // Name of the parameter to change
                int value = message[2].getInt32();  // Value of the parameter to set
                String serializedParameters = (String)soundIndex + SERIALIZATION_SEPARATOR +
                                              parameterName + SERIALIZATION_SEPARATOR  +
                                              (String)value + SERIALIZATION_SEPARATOR;
                String actionMessage = String(ACTION_SET_SOUND_PARAMETER_INT) + ":" + serializedParameters;
                sendActionMessage(actionMessage);
            }
        } else if (message.getAddressPattern().toString() == OSC_ADDRESS_SET_REVERB_PARAMETERS){
            if (message.size() == 6)  {
                float roomSize = message[0].getFloat32();
                float damping = message[1].getFloat32();
                float wetLevel = message[2].getFloat32();
                float dryLevel = message[3].getFloat32();
                float width = message[4].getFloat32();
                float freezeMode = message[5].getFloat32();
                String serializedParameters = (String)roomSize + SERIALIZATION_SEPARATOR +
                                              (String)damping + SERIALIZATION_SEPARATOR +
                                              (String)wetLevel + SERIALIZATION_SEPARATOR +
                                              (String)dryLevel + SERIALIZATION_SEPARATOR +
                                              (String)width + SERIALIZATION_SEPARATOR +
                                              (String)freezeMode + SERIALIZATION_SEPARATOR;
                String actionMessage = String(ACTION_SET_REVERB_PARAMETERS) + ":" + serializedParameters;
                sendActionMessage(actionMessage);
            }
        } else if (message.getAddressPattern().toString() == OSC_ADDRESS_SAVE_CURRENT_PRESET){
            if (message.size() == 2)  {
                String presetName = message[0].getString();  // Name of the preset to save
                int index = message[1].getInt32();  // Preset index
                String serializedParameters = presetName + SERIALIZATION_SEPARATOR +
                                              (String)index + SERIALIZATION_SEPARATOR;
                String actionMessage = String(ACTION_SAVE_CURRENT_PRESET) + ":" + serializedParameters;
                sendActionMessage(actionMessage);
            }
        } else if (message.getAddressPattern().toString() == OSC_ADDRESS_LOAD_PRESET){
            if (message.size() == 1)  {
                int index = message[0].getInt32();  // Preset index
                String actionMessage = String(ACTION_LOAD_PRESET) + ":" + (String)index;
                sendActionMessage(actionMessage);
            }
        } else if (message.getAddressPattern().toString() == OSC_ADDRESS_SET_MIDI_IN_CHANNEL){
            if (message.size() == 1)  {
                int channel = message[0].getInt32();  // Midi channel to accept (-1 for all channels)
                String actionMessage = String(ACTION_SET_MIDI_IN_CHANNEL) + ":" + (String)channel;
                sendActionMessage(actionMessage);
            }
        } else if (message.getAddressPattern().toString() == OSC_ADDRESS_SET_MIDI_THRU){
            if (message.size() == 1)  {
                int midiThru = message[0].getInt32();
                String actionMessage = String(ACTION_SET_MIDI_THRU) + ":" + (String)midiThru;
                sendActionMessage(actionMessage);
            }
        } else if (message.getAddressPattern().toString() == OSC_ADDRESS_PLAY_SOUND){
            if (message.size() == 1)  {
                int soundIndex = message[0].getInt32();
                String actionMessage = String(ACTION_PLAY_SOUND) + ":" + (String)soundIndex;
                sendActionMessage(actionMessage);
            }
        } else if (message.getAddressPattern().toString() == OSC_ADDRESS_STOP_SOUND){
            if (message.size() == 1)  {
                int soundIndex = message[0].getInt32();
                String actionMessage = String(ACTION_STOP_SOUND) + ":" + (String)soundIndex;
                sendActionMessage(actionMessage);
            }
        } else if (message.getAddressPattern().toString() == OSC_ADDRESS_SET_POLYPHONY){
            if (message.size() == 1)  {
                int numVoices = message[0].getInt32();
                String actionMessage = String(ACTION_SET_POLYPHONY) + ":" + (String)numVoices;
                sendActionMessage(actionMessage);
            }
        } else if (message.getAddressPattern().toString() == OSC_ADDRESS_FINISHED_DOWNLOADING_SOUND){
            if (message.size() == 1)  {
                String soundTargetLocation = message[0].getString();
                String actionMessage = String(ACTION_FINISHED_DOWNLOADING_SOUND) + ":" + soundTargetLocation;
                sendActionMessage(actionMessage);
            }
        } else if (message.getAddressPattern().toString() == OSC_ADDRESS_DOWNLOADING_SOUND_PROGRESS){
            if (message.size() == 2)  {
                String soundTargetLocation = message[0].getString();
                int percentageCompleted = message[1].getInt32();
                String serializedParameters = soundTargetLocation + SERIALIZATION_SEPARATOR +
                                              (String)percentageCompleted + SERIALIZATION_SEPARATOR;
                String actionMessage = String(ACTION_UPDATE_DOWNLOADING_SOUND_PROGRESS) + ":" + serializedParameters;
                sendActionMessage(actionMessage);
            }
        } else if (message.getAddressPattern().toString() == OSC_ADDRESS_ADD_OR_UPDATE_CC_MAPPING){
            if (message.size() == 6)  {
                int soundIndex = message[0].getInt32();
                int randomID = message[1].getInt32();
                int ccNumber = message[2].getInt32();
                String parameterName = message[3].getString();
                float minRange = message[4].getFloat32();
                float maxRange = message[5].getFloat32();
                String serializedParameters = (String)soundIndex + SERIALIZATION_SEPARATOR +
                                              (String)randomID + SERIALIZATION_SEPARATOR +
                                              (String)ccNumber + SERIALIZATION_SEPARATOR +
                                              parameterName + SERIALIZATION_SEPARATOR +
                                              (String)minRange + SERIALIZATION_SEPARATOR +
                                              (String)maxRange + SERIALIZATION_SEPARATOR;
                String actionMessage = String(ACTION_ADD_OR_UPDATE_CC_MAPPING) + ":" + serializedParameters;
                sendActionMessage(actionMessage);
            }
        } else if (message.getAddressPattern().toString() == OSC_ADDRESS_REMOVE_CC_MAPPING){
            if (message.size() == 2)  {
                int soundIndex = message[0].getInt32();
                int randomID = message[1].getInt32();
                String serializedParameters = (String)soundIndex + SERIALIZATION_SEPARATOR +
                                              (String)randomID + SERIALIZATION_SEPARATOR;
                String actionMessage = String(ACTION_REMOVE_CC_MAPPING) + ":" + serializedParameters;
                sendActionMessage(actionMessage);
            }
        } else if (message.getAddressPattern().toString() == OSC_ADDRESS_SET_STATE_TIMER_HZ){
            if (message.size() == 1)  {
                int newHz = message[0].getInt32();
                String actionMessage = String(ACTION_SET_STATE_TIMER_HZ) + ":" + (String)newHz;
                sendActionMessage(actionMessage);
            }
        } else if (message.getAddressPattern().toString() == OSC_ADDRESS_REMOVE_SOUND){
            if (message.size() == 1)  {
                int soundIndex = message[0].getInt32();
                String actionMessage = String(ACTION_REMOVE_SOUND) + ":" + (String)soundIndex;
                sendActionMessage(actionMessage);
            }
        } else if (message.getAddressPattern().toString() == OSC_ADDRESS_ADD_OR_REPLACE_SOUND){
            if (message.size() == 8)  {
                int soundIdx = message[0].getInt32();
                int soundID = message[1].getInt32();
                String soundName = message[2].getString();
                String soundUser = message[3].getString();
                String soundLicense = message[4].getString();
                String oggDownloadURL = message[5].getString();
                String serializedSlices = message[6].getString();
                String assignedNotes = message[7].getString();
                String serializedParameters = (String)soundIdx + SERIALIZATION_SEPARATOR +
                                              (String)soundID + SERIALIZATION_SEPARATOR +
                                              soundName + SERIALIZATION_SEPARATOR +
                                              soundUser + SERIALIZATION_SEPARATOR +
                                              soundLicense + SERIALIZATION_SEPARATOR +
                                              oggDownloadURL + SERIALIZATION_SEPARATOR +
                                              serializedSlices + SERIALIZATION_SEPARATOR +
                                              assignedNotes + SERIALIZATION_SEPARATOR;
                String actionMessage = String(ACTION_ADD_OR_REPLACE_SOUND) + ":" + serializedParameters;
                sendActionMessage(actionMessage);
            }
        } else if (message.getAddressPattern().toString() == OSC_ADDRESS_REAPPLY_LAYOUT){
            if (message.size() == 1)  {
                int layoutType = message[0].getInt32();
                String actionMessage = String(ACTION_REAPPLY_LAYOUT) + ":" + (String)layoutType;
                sendActionMessage(actionMessage);
            }
        } else if (message.getAddressPattern().toString() == OSC_ADDRESS_SET_SOUND_SLICES){
            if (message.size() == 2)  {
                int soundIndex = message[0].getInt32();
                String serializedSlices = message[1].getString();
                String serializedParameters = (String)soundIndex + SERIALIZATION_SEPARATOR +
                                               serializedSlices + SERIALIZATION_SEPARATOR;
                String actionMessage = String(ACTION_SET_SOUND_SLICES) + ":" + serializedParameters;
                sendActionMessage(actionMessage);
            }
        }
    }
    
    #if ENABLE_EMBEDDED_HTTP_SERVER
    HTTPServer httpServer;
    #endif
    #if ENABLE_OSC_SERVER
    OSCServer oscServer;
    #endif
    String serializedAppState;
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
        String contents = String::fromUTF8 (BinaryData::index_html, BinaryData::index_htmlSize);
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
    
    server.Get("/get_system_stats", [this](const httplib::Request &, httplib::Response &res) {
        std::string stats = "";
        #if ELK_BUILD
            stats += "----------- System stats:\n";
            stats += "CPU temp: " + exec("sudo vcgencmd measure_temp");
            stats += "Memory used (%): " + exec("free | grep Mem | awk '{print $3/$2 * 100.0}'");
            stats += "CPU used (%): " + exec("grep 'cpu ' /proc/stat | awk '{usage=($2+$4)*100/($2+$4+$5)} END {print usage \"%\"}'");
            stats += "MSW:\nCPU  PID    MSW        CSW        XSC        PF    STAT       %CPU  NAME\n" + exec("more /proc/xenomai/sched/stat | grep sushi") + "\n";
            stats += "-----------\n";
        #else
            stats += "No system stats for this platform";
        #endif
        res.set_content(stats, "text/html");
    });
    
    server.Get("/update_state", [this](const httplib::Request &, httplib::Response &res) {
        if (interface != nullptr){
            res.set_content((interface->serializedAppState).toStdString(), "text/xml");
        } else {
            res.set_content("", "text/xml");
        }
    });
    
    #if !ELK_BUILD
    // In desktop builds we want each instace to use a separate port so that each instance has its own interface
    port = server.bind_to_any_port("0.0.0.0");
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
    DBG("Received OSC message at address: " + message.getAddressPattern().toString());
    if (interface != nullptr){
        interface->processActionFromOSCMessage(message);
    }
}
