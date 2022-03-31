/*
  ==============================================================================

    SourceSamplerSound.cpp
    Created: 23 Mar 2022 1:38:02pm
    Author:  Frederic Font Corbera

  ==============================================================================
*/

#include "SourceSamplerSound.h"


SourceSamplerSound::SourceSamplerSound (int _idx,
                                        const String& _soundName,
                                        AudioFormatReader& source,
                                        bool _loadingPreviewVersion,
                                        double maxSampleLengthSeconds,
                                        double _pluginSampleRate,
                                        int _pluginBlockSize)
    : idx (_idx),
      name(_soundName),
      loadedPreviewVersion (_loadingPreviewVersion),
      soundSampleRate (source.sampleRate),
      pluginSampleRate (_pluginSampleRate),
      pluginBlockSize (_pluginBlockSize)
{
    if (soundSampleRate > 0 && source.lengthInSamples > 0)
    {
        length = jmin ((int) source.lengthInSamples,
                       (int) (maxSampleLengthSeconds * soundSampleRate));

        data.reset (new AudioBuffer<float> (jmin (2, (int) source.numChannels), length + 4));

        source.read (data.get(), 0, length + 4, 0, true, true);

    }
}

SourceSamplerSound::~SourceSamplerSound()
{
}

//==============================================================================

bool SourceSamplerSound::getLoadedPreviewVersion()
{
    return loadedPreviewVersion;
}

bool SourceSamplerSound::appliesToNote (int midiNoteNumber)
{
    return midiNotes[midiNoteNumber];
}

bool SourceSamplerSound::appliesToChannel (int /*midiChannel*/)
{
    return true;
}

void SourceSamplerSound::setParameterByNameFloat(juce::Identifier identifier, float value){
    // TODO: we should replace calls to this function for the one in SourceSound
    sourceSoundPointer->setParameterByNameFloat(identifier, value, false);
}

void SourceSamplerSound::setParameterByNameFloatNorm(juce::Identifier identifier, float value0to1){
    // TODO: we should replace calls to this function for the one in SourceSound
    sourceSoundPointer->setParameterByNameFloat(identifier, value0to1, true);
}

void SourceSamplerSound::setParameterByNameInt(juce::Identifier identifier, int value){
    // TODO: we should replace calls to this function for the one in SourceSound
    sourceSoundPointer->setParameterByNameInt(identifier, value);
}

float SourceSamplerSound::getParameterFloat(juce::Identifier identifier){
    return sourceSoundPointer->getParameterFloat(identifier, false);
}

int SourceSamplerSound::getParameterInt(juce::Identifier identifier){
    return sourceSoundPointer->getParameterInt(identifier);
}

ValueTree SourceSamplerSound::getState(){
    ValueTree state = ValueTree(STATE_SAMPLER_SOUND);
    state.setProperty(STATE_SAMPLER_SOUND_MIDI_NOTES, midiNotes.toString(16), nullptr);
    state.setProperty(STATE_SAMPLER_SOUND_LOADED_PREVIEW, loadedPreviewVersion, nullptr);
    state.setProperty(STATE_SOUND_INFO_IDX, idx, nullptr);  // idx is set to the state sot hat the UI gets it, but never loaded because it is generated dynamically

    // Add midi mappings (sorted by cc number so are displayed accordingly in the interface
    std::vector<MidiCCMapping> midiMappingsSorted = getMidiMappingsSorted();
    for (int i=0; i<midiMappingsSorted.size(); i++){
        state.appendChild(midiMappingsSorted[i].getState(), nullptr);
    }
    
    // --> Start auto-generated code B
    state.appendChild(ValueTree(STATE_SAMPLER_SOUND_PARAMETER)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE, "int", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "soundType", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, gpi(IDs::soundType), nullptr),
                      nullptr);
    state.appendChild(ValueTree(STATE_SAMPLER_SOUND_PARAMETER)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE, "int", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "launchMode", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, gpi(IDs::launchMode), nullptr),
                      nullptr);
    state.appendChild(ValueTree(STATE_SAMPLER_SOUND_PARAMETER)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE, "float", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "startPosition", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, gpf(IDs::startPosition), nullptr),
                      nullptr);
    state.appendChild(ValueTree(STATE_SAMPLER_SOUND_PARAMETER)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE, "float", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "endPosition", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, gpf(IDs::endPosition), nullptr),
                      nullptr);
    state.appendChild(ValueTree(STATE_SAMPLER_SOUND_PARAMETER)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE, "float", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "loopStartPosition", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, gpf(IDs::loopStartPosition), nullptr),
                      nullptr);
    state.appendChild(ValueTree(STATE_SAMPLER_SOUND_PARAMETER)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE, "float", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "loopEndPosition", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, gpf(IDs::loopEndPosition), nullptr),
                      nullptr);
    state.appendChild(ValueTree(STATE_SAMPLER_SOUND_PARAMETER)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE, "int", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "loopXFadeNSamples", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, gpi(IDs::loopXFadeNSamples), nullptr),
                      nullptr);
    state.appendChild(ValueTree(STATE_SAMPLER_SOUND_PARAMETER)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE, "int", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "reverse", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, gpi(IDs::reverse), nullptr),
                      nullptr);
    state.appendChild(ValueTree(STATE_SAMPLER_SOUND_PARAMETER)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE, "int", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "noteMappingMode", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, gpi(IDs::noteMappingMode), nullptr),
                      nullptr);
    state.appendChild(ValueTree(STATE_SAMPLER_SOUND_PARAMETER)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE, "int", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "numSlices", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, gpi(IDs::numSlices), nullptr),
                      nullptr);
    state.appendChild(ValueTree(STATE_SAMPLER_SOUND_PARAMETER)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE, "float", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "playheadPosition", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, gpf(IDs::playheadPosition), nullptr),
                      nullptr);
    state.appendChild(ValueTree(STATE_SAMPLER_SOUND_PARAMETER)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE, "float", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "freezePlayheadSpeed", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, gpf(IDs::freezePlayheadSpeed), nullptr),
                      nullptr);
    state.appendChild(ValueTree(STATE_SAMPLER_SOUND_PARAMETER)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE, "float", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "filterCutoff", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, gpf(IDs::filterCutoff), nullptr),
                      nullptr);
    state.appendChild(ValueTree(STATE_SAMPLER_SOUND_PARAMETER)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE, "float", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "filterRessonance", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, gpf(IDs::filterRessonance), nullptr),
                      nullptr);
    state.appendChild(ValueTree(STATE_SAMPLER_SOUND_PARAMETER)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE, "float", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "filterKeyboardTracking", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, gpf(IDs::filterKeyboardTracking), nullptr),
                      nullptr);
    state.appendChild(ValueTree(STATE_SAMPLER_SOUND_PARAMETER)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE, "float", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "filterA", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, gpf(IDs::filterA), nullptr),
                      nullptr);
    state.appendChild(ValueTree(STATE_SAMPLER_SOUND_PARAMETER)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE, "float", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "filterD", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, gpf(IDs::filterD), nullptr),
                      nullptr);
    state.appendChild(ValueTree(STATE_SAMPLER_SOUND_PARAMETER)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE, "float", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "filterS", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, gpf(IDs::filterS), nullptr),
                      nullptr);
    state.appendChild(ValueTree(STATE_SAMPLER_SOUND_PARAMETER)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE, "float", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "filterR", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, gpf(IDs::filterR), nullptr),
                      nullptr);
    state.appendChild(ValueTree(STATE_SAMPLER_SOUND_PARAMETER)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE, "float", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "filterADSR2CutoffAmt", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, gpf(IDs::filterADSR2CutoffAmt), nullptr),
                      nullptr);
    state.appendChild(ValueTree(STATE_SAMPLER_SOUND_PARAMETER)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE, "float", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "gain", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, gpf(IDs::gain), nullptr),
                      nullptr);
    state.appendChild(ValueTree(STATE_SAMPLER_SOUND_PARAMETER)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE, "float", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "ampA", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, gpf(IDs::ampA), nullptr),
                      nullptr);
    state.appendChild(ValueTree(STATE_SAMPLER_SOUND_PARAMETER)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE, "float", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "ampD", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, gpf(IDs::ampD), nullptr),
                      nullptr);
    state.appendChild(ValueTree(STATE_SAMPLER_SOUND_PARAMETER)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE, "float", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "ampS", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, gpf(IDs::ampS), nullptr),
                      nullptr);
    state.appendChild(ValueTree(STATE_SAMPLER_SOUND_PARAMETER)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE, "float", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "ampR", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, gpf(IDs::ampR), nullptr),
                      nullptr);
    state.appendChild(ValueTree(STATE_SAMPLER_SOUND_PARAMETER)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE, "float", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "pan", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, gpf(IDs::pan), nullptr),
                      nullptr);
    state.appendChild(ValueTree(STATE_SAMPLER_SOUND_PARAMETER)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE, "float", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "pitch", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, gpf(IDs::pitch), nullptr),
                      nullptr);
    state.appendChild(ValueTree(STATE_SAMPLER_SOUND_PARAMETER)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE, "float", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "pitchBendRangeUp", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, gpf(IDs::pitchBendRangeUp), nullptr),
                      nullptr);
    state.appendChild(ValueTree(STATE_SAMPLER_SOUND_PARAMETER)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE, "float", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "pitchBendRangeDown", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, gpf(IDs::pitchBendRangeDown), nullptr),
                      nullptr);
    state.appendChild(ValueTree(STATE_SAMPLER_SOUND_PARAMETER)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE, "float", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "mod2CutoffAmt", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, gpf(IDs::mod2CutoffAmt), nullptr),
                      nullptr);
    state.appendChild(ValueTree(STATE_SAMPLER_SOUND_PARAMETER)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE, "float", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "mod2GainAmt", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, gpf(IDs::mod2GainAmt), nullptr),
                      nullptr);
    state.appendChild(ValueTree(STATE_SAMPLER_SOUND_PARAMETER)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE, "float", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "mod2PitchAmt", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, gpf(IDs::mod2PitchAmt), nullptr),
                      nullptr);
    state.appendChild(ValueTree(STATE_SAMPLER_SOUND_PARAMETER)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE, "float", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "mod2PlayheadPos", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, gpf(IDs::mod2PlayheadPos), nullptr),
                      nullptr);
    state.appendChild(ValueTree(STATE_SAMPLER_SOUND_PARAMETER)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE, "float", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "vel2CutoffAmt", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, gpf(IDs::vel2CutoffAmt), nullptr),
                      nullptr);
    state.appendChild(ValueTree(STATE_SAMPLER_SOUND_PARAMETER)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE, "float", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "vel2GainAmt", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, gpf(IDs::vel2GainAmt), nullptr),
                      nullptr);
    // --> End auto-generated code B
    
    return state;
}

void SourceSamplerSound::loadState(ValueTree soundState){
    
    if (soundState.hasProperty(STATE_SAMPLER_SOUND_MIDI_NOTES)){
        midiNotes.parseString(soundState.getProperty(STATE_SAMPLER_SOUND_MIDI_NOTES).toString(), 16);
    }
    
    // Iterate over sound parameters and midi mappings and load them
    if (soundState.isValid()){
        for (int i=0; i<soundState.getNumChildren(); i++){
            ValueTree parameterOrMapping = soundState.getChild(i);
            if (parameterOrMapping.hasType(STATE_SAMPLER_SOUND_PARAMETER)){
                // VT is a sound parameter
                String type = parameterOrMapping.getProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE).toString();
                String name = parameterOrMapping.getProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME).toString();
                if (type == "float"){
                    float value = (float)parameterOrMapping.getProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE);
                    setParameterByNameFloat(name, value);
                } else if (type == "int"){
                    int value = (int)parameterOrMapping.getProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE);
                    setParameterByNameInt(name, value);
                }
            } else if (parameterOrMapping.hasType(STATE_SAMPLER_SOUND_MIDI_CC_MAPPING)){
                // VT is a midi CC mapping
                addOrEditMidiMapping(-1,
                                     (int)parameterOrMapping.getProperty(STATE_SAMPLER_SOUND_MIDI_CC_MAPPING_NUMBER),
                                     parameterOrMapping.getProperty(STATE_SAMPLER_SOUND_MIDI_CC_MAPPING_NAME).toString(),
                                     (float)parameterOrMapping.getProperty(STATE_SAMPLER_SOUND_MIDI_CC_MAPPING_MIN),
                                     (float)parameterOrMapping.getProperty(STATE_SAMPLER_SOUND_MIDI_CC_MAPPING_MAX));
            }
        }
    }
}

int SourceSamplerSound::getLengthInSamples(){
    return length;
}

float SourceSamplerSound::getLengthInSeconds(){
    return (float)getLengthInSamples()/soundSampleRate;
}

void SourceSamplerSound::setIdx(int newIdx)
{
    idx = newIdx;
}

int SourceSamplerSound::getIdx(){
    return idx;
}

int SourceSamplerSound::getNumberOfMappedMidiNotes()
{
    return midiNotes.countNumberOfSetBits();
}

juce::BigInteger SourceSamplerSound::getMappedMidiNotes()
{
    return midiNotes;
}

void SourceSamplerSound::setMappedMidiNotes(juce::BigInteger newMappedMidiNotes)
{
    midiNotes = newMappedMidiNotes;
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

void SourceSamplerSound::addOrEditMidiMapping(int randomID, int ccNumber, String parameterName, float minRange, float maxRange){
    if (randomID < 0){
        midiMappings.push_back(MidiCCMapping(ccNumber, parameterName, jlimit(0.0f, 1.0f, minRange), jlimit(0.0f, 1.0f, maxRange)));
    } else {
        for (int i=0; i<midiMappings.size(); i++){
            if (midiMappings[i].randomID == randomID){
                midiMappings[i].ccNumber = ccNumber;
                midiMappings[i].parameterName = parameterName;
                midiMappings[i].minRange = minRange;
                midiMappings[i].maxRange = maxRange;
            }
        }
    }
}

std::vector<MidiCCMapping> SourceSamplerSound::getMidiMappingsForCcNumber(int ccNumber){
    std::vector<MidiCCMapping> toReturn = {};
    for (int i=0; i<midiMappings.size(); i++){
        if (midiMappings[i].ccNumber == ccNumber){
            toReturn.push_back(midiMappings[i]);
        }
    }
    return toReturn;
}

std::vector<MidiCCMapping> SourceSamplerSound::getMidiMappingsSorted(){
    std::vector<MidiCCMapping> midiMappingsSorted = midiMappings;
    std::sort(midiMappingsSorted.begin(), midiMappingsSorted.end());
    return midiMappingsSorted;
}

void SourceSamplerSound::removeMidiMapping(int randomID){
    std::vector<MidiCCMapping> newMidiMappings = {};
    for (int i=0; i<midiMappings.size(); i++){
        if (midiMappings[i].randomID != randomID){
            newMidiMappings.push_back(midiMappings[i]);
        }
    }
    midiMappings = newMidiMappings;
}
