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


class ServerInterface: private OSCReceiver,
                       private OSCReceiver::ListenerWithOSCAddress<OSCReceiver::MessageLoopCallback>,
                       public ActionBroadcaster
{
public:
    ServerInterface ()
    {
        // Start listening on OSC port
        if (! connect (OSC_LISTEN_PORT)){
            std::cout << "ERROR setting OSC receiver" << std::endl;
            oscReveiverInitialized = false;
        } else {
            std::cout << "Listening for OSC messages" << std::endl;
            oscReveiverInitialized = true;
            addListener (this, OSC_ADDRESS_NEW_QUERY);
            addListener (this, OSC_ADDRESS_SET_MIDI_ROOT_OFFSET);
        }
    }
    
    ~ServerInterface ()
    {
    }
        
    void oscMessageReceived (const OSCMessage& message) override
    {
        if (message.getAddressPattern().toString() == OSC_ADDRESS_NEW_QUERY){
            if (message.size() == 3)  {
                String query = message[0].getString();
                int numSounds = message[1].getInt32();
                float maxLength = message[2].getFloat32();
                String separator = ";";
                String serializedParameters = query + SERIALIZATION_SEPARATOR + (String)numSounds + SERIALIZATION_SEPARATOR  + (String)maxLength + SERIALIZATION_SEPARATOR;
                String actionMessage = String(ACTION_NEW_QUERY_TRIGGERED_FROM_SERVER) + ":" + serializedParameters;
                sendActionMessage(actionMessage);
            }
        } else if (message.getAddressPattern().toString() == OSC_ADDRESS_SET_MIDI_ROOT_OFFSET){
            if (message.size() == 1 && message[0].isInt32())  {
                int newOffset = message[0].getInt32();
                String actionMessage = String(ACTION_SET_MIDI_ROOT_NOTE_OFFSET) + ":" + (String)newOffset;
                sendActionMessage(actionMessage);
            }
        }
    }

private:
        
    bool oscReveiverInitialized = false;
        
};
