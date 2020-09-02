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
            addListener (this, "/new_query");
        }
    }
    
    ~ServerInterface ()
    {
    }
        
    void oscMessageReceived (const OSCMessage& message) override
    {
        if (message.getAddressPattern().toString() == "/new_query"){
            if (message.size() == 1 && message[0].isString())  {
                String query = message[0].getString();
                String actionMessage = String(ACTION_NEW_QUERY_TRIGGERED_FROM_SERVER) + ":" + query;
                sendActionMessage(actionMessage);
            }
        }
    }

private:
        
    bool oscReveiverInitialized = false;
        
};
