/*
  ==============================================================================

    This file was auto-generated!

    It contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#if SOURCE_WEB_UI
#include "PluginEditor.h"
#else
#include "UI/NativeEditor.h"
#endif

#if INCLUDE_SEQUENCER
#include "defines_shepherd.h"
#endif


//==============================================================================
SourceSamplerAudioProcessor::SourceSamplerAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
#endif
{
    source.setTotalNumOutputChannels(getTotalNumOutputChannels());
    
    #if JUCE_IOS
    // iOS can kill the standalone app at any time without letting it save its state, so keep the session autosaved
    // (plugin instances like the AUv3 have their state saved by the host)
    if (wrapperType == wrapperType_Standalone)
        source.enableSessionAutosave(source.sourceDataLocation.getChildFile("last_session.xml"));
    #endif
    #if INCLUDE_SEQUENCER
    sequencerCombinedBuffer.ensureSize(512 * 10);
    #endif
}

SourceSamplerAudioProcessor::~SourceSamplerAudioProcessor()
{
}


//==============================================================================
const juce::String SourceSamplerAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool SourceSamplerAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool SourceSamplerAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool SourceSamplerAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double SourceSamplerAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int SourceSamplerAudioProcessor::getNumPrograms()
{
   return 128;
}

int SourceSamplerAudioProcessor::getCurrentProgram()
{
    return source.currentPresetIndex;
}

void SourceSamplerAudioProcessor::setCurrentProgram (int index)
{
    // Hosts may call this from any thread, but loading presets modifies the state (message thread only)
    source.runOnMessageThread([index](SourceSampler& s){ s.loadPresetFromIndex(index); });
}

const juce::String SourceSamplerAudioProcessor::getProgramName (int index)
{
    return source.getPresetNameByIndex(index);
}

void SourceSamplerAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
    source.renamePreset(index, newName);
}


//==============================================================================
void SourceSamplerAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    source.prepareToPlay(sampleRate, samplesPerBlock);
    #if INCLUDE_SEQUENCER
    sequencer.prepareSequencer(samplesPerBlock, sampleRate);
    #endif
}

void SourceSamplerAudioProcessor::releaseResources()
{
    source.releaseResources();
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool SourceSamplerAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void SourceSamplerAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    #if INCLUDE_SEQUENCER
    int sliceNumSamples = buffer.getNumSamples();
    sequencer.getNextMIDISlice(sliceNumSamples);
    sequencerCombinedBuffer.clear();
    for (auto deviceData: *sequencer.getMidiOutDevices()){
        if (deviceData != nullptr && deviceData->name == INTERNAL_OUTPUT_MIDI_DEVICE_NAME){
            sequencerCombinedBuffer.addEvents(deviceData->buffer, 0, sliceNumSamples, 0);
        }
    }
    midiMessages.addEvents(sequencerCombinedBuffer, 0, sliceNumSamples, 0);  // Combine sequencer MIDI messages with plugin's input message stream
    #endif
    keyboardState.processNextMidiBuffer(midiMessages, 0, buffer.getNumSamples(), true);
    source.processBlock(buffer, midiMessages);
}

//==============================================================================
bool SourceSamplerAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* SourceSamplerAudioProcessor::createEditor()
{
    #if SOURCE_WEB_UI
    return new SourceSamplerAudioProcessorEditor (*this);
    #else
    return new SourceNativeEditor (*this);
    #endif
}

void SourceSamplerAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // Save current state information to memory block
    std::unique_ptr<juce::XmlElement> xml ( source.state.createXml());
    if (xml != nullptr)
        copyXmlToBinary (*xml, destData);
}

void SourceSamplerAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // Called by the plugin host to load state stored in host into plugin
    DBG("> Running setStateInformation");
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr){
        // Hosts may call this from any thread, but the state can only be modified in the message thread
        juce::ValueTree newState = juce::ValueTree::fromXml(*xmlState.get());
        source.runOnMessageThread([newState](SourceSampler& s){ s.loadPresetFromStateInformation(newState); });
    }
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SourceSamplerAudioProcessor();
}
