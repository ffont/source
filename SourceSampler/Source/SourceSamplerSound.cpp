/*
  ==============================================================================

    SourceSamplerSound.cpp
    Created: 23 Mar 2022 1:38:02pm
    Author:  Frederic Font Corbera

  ==============================================================================
*/

#include "SourceSamplerSound.h"


SourceSamplerSound::SourceSamplerSound (const juce::ValueTree& _state,
                                        SourceSound* _sourceSoundPointer,
                                        AudioFormatReader& source,
                                        bool _loadingPreviewVersion,
                                        double maxSampleLengthSeconds,
                                        double _pluginSampleRate,
                                        int _pluginBlockSize)
    : state(_state),
      loadedPreviewVersion (_loadingPreviewVersion),
      soundSampleRate (source.sampleRate),
      pluginSampleRate (_pluginSampleRate),
      pluginBlockSize (_pluginBlockSize)
{
    sourceSoundPointer = _sourceSoundPointer;
    bindState();
    
    // Load audio
    if (soundSampleRate > 0 && source.lengthInSamples > 0) {
        lengthInSamples = jmin ((int) source.lengthInSamples, (int) (maxSampleLengthSeconds * soundSampleRate));
        data.reset (new AudioBuffer<float> (jmin (2, (int) source.numChannels), lengthInSamples + 4));
        source.read (data.get(), 0, lengthInSamples + 4, 0, true, true);
    }
}

SourceSamplerSound::~SourceSamplerSound()
{
}

void SourceSamplerSound::bindState ()
{
    name.referTo(state, IDs::name, nullptr);
    soundId.referTo(state, IDs::soundId, nullptr);
    midiRootNote.referTo(state, IDs::midiRootNote, nullptr);
    midiNotesAsString.referTo(state, IDs::midiNotes, nullptr);
}

//==============================================================================

bool SourceSamplerSound::getLoadedPreviewVersion()
{
    return loadedPreviewVersion;
}

bool SourceSamplerSound::appliesToNote (int midiNoteNumber)
{
    return getMappedMidiNotes()[midiNoteNumber];
}

bool SourceSamplerSound::appliesToChannel (int /*midiChannel*/)
{
    return true;
}

float SourceSamplerSound::getParameterFloat(juce::Identifier identifier){
    return sourceSoundPointer->getParameterFloat(identifier, false);
}

int SourceSamplerSound::getParameterInt(juce::Identifier identifier){
    return sourceSoundPointer->getParameterInt(identifier);
}

int SourceSamplerSound::getLengthInSamples(){
    return lengthInSamples;
}

float SourceSamplerSound::getLengthInSeconds(){
    return (float)getLengthInSamples()/soundSampleRate;
}

int SourceSamplerSound::getNumberOfMappedMidiNotes()
{
    return getMappedMidiNotes().countNumberOfSetBits();
}

juce::BigInteger SourceSamplerSound::getMappedMidiNotes()
{
    juce::BigInteger midiNotes;
    midiNotes.parseString(midiNotesAsString.get(), 16);
    return midiNotes;
}

void SourceSamplerSound::setMappedMidiNotes(juce::BigInteger newMappedMidiNotes)
{
    midiNotesAsString = newMappedMidiNotes.toString(16);
}

int SourceSamplerSound::getMidiRootNote()
{
    return midiRootNote;
}

void SourceSamplerSound::setMidiRootNote(int newRootNote)
{
    midiRootNote = newRootNote;
}

void SourceSamplerSound::setOnsetTimesSamples(std::vector<float> onsetTimes){
    onsetTimesSamples.clear();
    for (int i=0; i<onsetTimes.size(); i++){
        onsetTimesSamples.push_back((int)(onsetTimes[i] * soundSampleRate));
    }
}

std::vector<int> SourceSamplerSound::getOnsetTimesSamples(){
    return onsetTimesSamples;
}
