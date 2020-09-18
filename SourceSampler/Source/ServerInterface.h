/*
  ==============================================================================

    ServerInterface.h
    Created: 2 Sep 2020 6:04:04pm
    Author:  Frederic Font Corbera

  ==============================================================================
*/

#pragma once

#include "JuceHeader.h"
#include "defines.h"
#include "httplib.h"


class ServerInterface;


class HTTPServer: public Thread
{
public:
    HTTPServer(): Thread ("SourceHttpServer"){
    }
    
    ~HTTPServer(){
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
    
    httplib::Server svr;
    bool connected = false;
    int port = HTTP_SERVER_LISTEN_PORT;  // Will start attempting at this port and continue with the subsequent if can't connect
    
    std::unique_ptr<ServerInterface> interface;
    
};


class ServerInterface: private OSCReceiver,
                       private OSCReceiver::ListenerWithOSCAddress<OSCReceiver::MessageLoopCallback>,
                       public ActionBroadcaster
{
public:
    ServerInterface ()
    {         
        // Start listening on OSC port
        if (! connect (OSC_LISTEN_PORT)){
            DBG("ERROR setting OSC receiver");
            oscReveiverInitialized = false;
        } else {
            DBG("Listening for OSC messages");
            oscReveiverInitialized = true;
            addListener (this, OSC_ADDRESS_NEW_QUERY);
            addListener (this, OSC_ADDRESS_SET_SOUND_PARAMETER_FLOAT);
            addListener (this, OSC_ADDRESS_SET_SOUND_PARAMETER_INT);
            addListener (this, OSC_ADDRESS_SET_REVERB_PARAMETERS);
            addListener (this, OSC_ADDRESS_SAVE_CURRENT_PRESET);
            addListener (this, OSC_ADDRESS_LOAD_PRESET);
            addListener (this, OSC_ADDRESS_SET_MIDI_IN_CHANNEL);
            addListener (this, OSC_ADDRESS_SET_MIDI_THRU);
            addListener (this, OSC_ADDRESS_PLAY_SOUND);
            addListener (this, OSC_ADDRESS_STOP_SOUND);
            addListener (this, OSC_ADDRESS_SET_POLYPHONY);
        }
    }
    
    ~ServerInterface ()
    {
        # if ENABLE_HTTP_SERVER
        httpServer.interface.release();
        httpServer.svr.stop();
        httpServer.stopThread(5000);  // Give it enough time to stop the http server...
        # endif
    }
    
    void log(String message){
        DBG(message);
    }
        
    void oscMessageReceived (const OSCMessage& message) override
    {
        if (message.getAddressPattern().toString() == OSC_ADDRESS_NEW_QUERY){
            if (message.size() == 3)  {
                String query = message[0].getString();
                int numSounds = message[1].getInt32();
                float maxLength = message[2].getFloat32();
                String separator = ";";
                String serializedParameters = query + SERIALIZATION_SEPARATOR +
                                              (String)numSounds + SERIALIZATION_SEPARATOR  +
                                              (String)maxLength + SERIALIZATION_SEPARATOR;
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
        }
    }
    
    HTTPServer httpServer;
    String serializedAppState;
    
private:
        
    bool oscReveiverInitialized = false;
        
};


void HTTPServer::run() {
    
    svr.Get("/index", [](const httplib::Request &, httplib::Response &res) {
        String contents = String::fromUTF8 (BinaryData::index_html, BinaryData::index_htmlSize);
        res.set_content(contents.toStdString(), "text/html");
    });
    
    svr.Get("/send_osc", [this](const httplib::Request &req, httplib::Response &res) {
        
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
            interface->oscMessageReceived(message);
        }
        
    });
    
    svr.Get("/get_system_stats", [this](const httplib::Request &, httplib::Response &res) {
        std::string stats = "";
        #if ELK_BUILD
            stats += "----------- System stats:\n";
            stats += "CPU temp: " + exec("sudo vcgencmd measure_temp") + "\n";
            stats += "Memory used (%): " + exec("free | grep Mem | awk '{print $3/$2 * 100.0}'");
            stats += "CPU used (%): " + exec("grep 'cpu ' /proc/stat | awk '{usage=($2+$4)*100/($2+$4+$5)} END {print usage \"%\"}'") + "\n";
            stats += "MSW:\nCPU  PID    MSW        CSW        XSC        PF    STAT       %CPU  NAME\n" + exec("more /proc/xenomai/sched/stat | grep sushi_b64") + "\n";
            stats += "-----------\n";
        #else
            stats += "No system stats for this platform";
        #endif
        res.set_content(stats, "text/html");
    });
    
    svr.Get("/update_state", [this](const httplib::Request &, httplib::Response &res) {
        if (interface != nullptr){
            res.set_content((interface->serializedAppState).toStdString(), "text/xml");
        } else {
            res.set_content("", "text/xml");
        }
    });
    
    while (!connected){
        port += 1;
        connected = svr.listen("0.0.0.0", port - 1);
    }
    if (interface != nullptr){
        interface->log("Started HTTP server, listening at 0.0.0.0:" + (String)(port - 1));
    }
    
}
