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
      loadedPreviewVersion (_loadingPreviewVersion),
      soundSampleRate (source.sampleRate),
      pluginSampleRate (_pluginSampleRate),
      pluginBlockSize (_pluginBlockSize),
      name(_soundName)
      
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

void SourceSamplerSound::setParameterByNameFloat(const String& name, float value){
    // --> Start auto-generated code A
    if (name == "startPosition") { startPosition = jlimit(0.0f, 1.0f, value); }
    else if (name == "endPosition") { endPosition = jlimit(0.0f, 1.0f, value); }
    else if (name == "loopStartPosition") { loopStartPosition = jlimit(0.0f, 1.0f, value); }
    else if (name == "loopEndPosition") { loopEndPosition = jlimit(0.0f, 1.0f, value); }
    else if (name == "playheadPosition") { playheadPosition = jlimit(0.0f, 1.0f, value); }
    else if (name == "freezePlayheadSpeed") { freezePlayheadSpeed = jlimit(1.0f, 5000.0f, value); }
    else if (name == "filterCutoff") { filterCutoff = jlimit(10.0f, 20000.0f, value); }
    else if (name == "filterRessonance") { filterRessonance = jlimit(0.0f, 1.0f, value); }
    else if (name == "filterKeyboardTracking") { filterKeyboardTracking = jlimit(0.0f, 1.0f, value); }
    else if (name == "filterADSR.attack") { filterADSR.attack = value; }
    else if (name == "filterADSR.decay") { filterADSR.decay = value; }
    else if (name == "filterADSR.sustain") { filterADSR.sustain = value; }
    else if (name == "filterADSR.release") { filterADSR.release = value; }
    else if (name == "filterADSR2CutoffAmt") { filterADSR2CutoffAmt = jlimit(0.0f, 100.0f, value); }
    else if (name == "gain") { gain = jlimit(-80.0f, 12.0f, value); }
    else if (name == "ampADSR.attack") { ampADSR.attack = value; }
    else if (name == "ampADSR.decay") { ampADSR.decay = value; }
    else if (name == "ampADSR.sustain") { ampADSR.sustain = value; }
    else if (name == "ampADSR.release") { ampADSR.release = value; }
    else if (name == "pan") { pan = jlimit(-1.0f, 1.0f, value); }
    else if (name == "pitch") { pitch = jlimit(-36.0f, 36.0f, value); }
    else if (name == "pitchBendRangeUp") { pitchBendRangeUp = jlimit(0.0f, 36.0f, value); }
    else if (name == "pitchBendRangeDown") { pitchBendRangeDown = jlimit(0.0f, 36.0f, value); }
    else if (name == "mod2CutoffAmt") { mod2CutoffAmt = jlimit(0.0f, 100.0f, value); }
    else if (name == "mod2GainAmt") { mod2GainAmt = jlimit(-12.0f, 12.0f, value); }
    else if (name == "mod2PitchAmt") { mod2PitchAmt = jlimit(-12.0f, 12.0f, value); }
    else if (name == "mod2PlayheadPos") { mod2PlayheadPos = jlimit(0.0f, 1.0f, value); }
    else if (name == "vel2CutoffAmt") { vel2CutoffAmt = jlimit(0.0f, 100.0f, value); }
    else if (name == "vel2GainAmt") { vel2GainAmt = jlimit(0.0f, 1.0f, value); }
    // --> End auto-generated code A
    
    // Do some checking of start/end loop start/end positions to make sure we don't do anything wrong
    if (endPosition < startPosition){
        endPosition = startPosition;
    }
    if (loopStartPosition < startPosition){
        loopStartPosition = startPosition;
    }
    if (loopEndPosition > endPosition){
        loopEndPosition = endPosition;
    }
    if (loopStartPosition > loopEndPosition){
        loopStartPosition = loopEndPosition;
    }
}

void SourceSamplerSound::setParameterByNameFloatNorm(const String& name, float value0to1){
    // --> Start auto-generated code D
    if (name == "startPosition") { setParameterByNameFloat("startPosition", jmap(value0to1, 0.0f, 1.0f)); }
    else if (name == "endPosition") { setParameterByNameFloat("endPosition", jmap(value0to1, 0.0f, 1.0f)); }
    else if (name == "loopStartPosition") { setParameterByNameFloat("loopStartPosition", jmap(value0to1, 0.0f, 1.0f)); }
    else if (name == "loopEndPosition") { setParameterByNameFloat("loopEndPosition", jmap(value0to1, 0.0f, 1.0f)); }
    else if (name == "playheadPosition") { setParameterByNameFloat("playheadPosition", jmap(value0to1, 0.0f, 1.0f)); }
    else if (name == "freezePlayheadSpeed") { setParameterByNameFloat("freezePlayheadSpeed", jmap(value0to1, 1.0f, 5000.0f)); }
    else if (name == "filterCutoff") { setParameterByNameFloat("filterCutoff", jmap(value0to1, 10.0f, 20000.0f)); }
    else if (name == "filterRessonance") { setParameterByNameFloat("filterRessonance", jmap(value0to1, 0.0f, 1.0f)); }
    else if (name == "filterKeyboardTracking") { setParameterByNameFloat("filterKeyboardTracking", jmap(value0to1, 0.0f, 1.0f)); }
    else if (name == "filterADSR.attack") { setParameterByNameFloat("filterADSR.attack", value0to1); }
    else if (name == "filterADSR.decay") { setParameterByNameFloat("filterADSR.decay", value0to1); }
    else if (name == "filterADSR.sustain") { setParameterByNameFloat("filterADSR.sustain", value0to1); }
    else if (name == "filterADSR.release") { setParameterByNameFloat("filterADSR.release", value0to1); }
    else if (name == "filterADSR2CutoffAmt") { setParameterByNameFloat("filterADSR2CutoffAmt", jmap(value0to1, 0.0f, 100.0f)); }
    else if (name == "gain") { setParameterByNameFloat("gain", jmap(value0to1, -80.0f, 12.0f)); }
    else if (name == "ampADSR.attack") { setParameterByNameFloat("ampADSR.attack", value0to1); }
    else if (name == "ampADSR.decay") { setParameterByNameFloat("ampADSR.decay", value0to1); }
    else if (name == "ampADSR.sustain") { setParameterByNameFloat("ampADSR.sustain", value0to1); }
    else if (name == "ampADSR.release") { setParameterByNameFloat("ampADSR.release", value0to1); }
    else if (name == "pan") { setParameterByNameFloat("pan", jmap(value0to1, -1.0f, 1.0f)); }
    else if (name == "pitch") { setParameterByNameFloat("pitch", jmap(value0to1, -36.0f, 36.0f)); }
    else if (name == "pitchBendRangeUp") { setParameterByNameFloat("pitchBendRangeUp", jmap(value0to1, 0.0f, 36.0f)); }
    else if (name == "pitchBendRangeDown") { setParameterByNameFloat("pitchBendRangeDown", jmap(value0to1, 0.0f, 36.0f)); }
    else if (name == "mod2CutoffAmt") { setParameterByNameFloat("mod2CutoffAmt", jmap(value0to1, 0.0f, 100.0f)); }
    else if (name == "mod2GainAmt") { setParameterByNameFloat("mod2GainAmt", jmap(value0to1, -12.0f, 12.0f)); }
    else if (name == "mod2PitchAmt") { setParameterByNameFloat("mod2PitchAmt", jmap(value0to1, -12.0f, 12.0f)); }
    else if (name == "mod2PlayheadPos") { setParameterByNameFloat("mod2PlayheadPos", jmap(value0to1, 0.0f, 1.0f)); }
    else if (name == "vel2CutoffAmt") { setParameterByNameFloat("vel2CutoffAmt", jmap(value0to1, 0.0f, 100.0f)); }
    else if (name == "vel2GainAmt") { setParameterByNameFloat("vel2GainAmt", jmap(value0to1, 0.0f, 1.0f)); }
    // --> End auto-generated code D
}

void SourceSamplerSound::setParameterByNameInt(const String& name, int value){
    // --> Start auto-generated code C
    if (name == "launchMode") { launchMode = jlimit(0, 4, value); }
    else if (name == "loopXFadeNSamples") { loopXFadeNSamples = jlimit(10, 100000, value); }
    else if (name == "reverse") { reverse = jlimit(0, 1, value); }
    else if (name == "noteMappingMode") { noteMappingMode = jlimit(0, 3, value); }
    else if (name == "numSlices") { numSlices = jlimit(0, 100, value); }
    else if (name == "midiRootNote") { midiRootNote = jlimit(0, 127, value); }
    // --> End auto-generated code C
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
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "launchMode", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, launchMode, nullptr),
                      nullptr);
    state.appendChild(ValueTree(STATE_SAMPLER_SOUND_PARAMETER)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE, "float", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "startPosition", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, startPosition, nullptr),
                      nullptr);
    state.appendChild(ValueTree(STATE_SAMPLER_SOUND_PARAMETER)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE, "float", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "endPosition", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, endPosition, nullptr),
                      nullptr);
    state.appendChild(ValueTree(STATE_SAMPLER_SOUND_PARAMETER)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE, "float", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "loopStartPosition", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, loopStartPosition, nullptr),
                      nullptr);
    state.appendChild(ValueTree(STATE_SAMPLER_SOUND_PARAMETER)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE, "float", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "loopEndPosition", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, loopEndPosition, nullptr),
                      nullptr);
    state.appendChild(ValueTree(STATE_SAMPLER_SOUND_PARAMETER)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE, "int", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "loopXFadeNSamples", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, loopXFadeNSamples, nullptr),
                      nullptr);
    state.appendChild(ValueTree(STATE_SAMPLER_SOUND_PARAMETER)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE, "int", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "reverse", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, reverse, nullptr),
                      nullptr);
    state.appendChild(ValueTree(STATE_SAMPLER_SOUND_PARAMETER)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE, "int", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "noteMappingMode", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, noteMappingMode, nullptr),
                      nullptr);
    state.appendChild(ValueTree(STATE_SAMPLER_SOUND_PARAMETER)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE, "int", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "numSlices", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, numSlices, nullptr),
                      nullptr);
    state.appendChild(ValueTree(STATE_SAMPLER_SOUND_PARAMETER)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE, "float", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "playheadPosition", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, playheadPosition, nullptr),
                      nullptr);
    state.appendChild(ValueTree(STATE_SAMPLER_SOUND_PARAMETER)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE, "float", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "freezePlayheadSpeed", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, freezePlayheadSpeed, nullptr),
                      nullptr);
    state.appendChild(ValueTree(STATE_SAMPLER_SOUND_PARAMETER)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE, "float", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "filterCutoff", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, filterCutoff, nullptr),
                      nullptr);
    state.appendChild(ValueTree(STATE_SAMPLER_SOUND_PARAMETER)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE, "float", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "filterRessonance", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, filterRessonance, nullptr),
                      nullptr);
    state.appendChild(ValueTree(STATE_SAMPLER_SOUND_PARAMETER)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE, "float", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "filterKeyboardTracking", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, filterKeyboardTracking, nullptr),
                      nullptr);
    state.appendChild(ValueTree(STATE_SAMPLER_SOUND_PARAMETER)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE, "float", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "filterADSR.attack", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, filterADSR.attack, nullptr),
                      nullptr);
    state.appendChild(ValueTree(STATE_SAMPLER_SOUND_PARAMETER)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE, "float", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "filterADSR.decay", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, filterADSR.decay, nullptr),
                      nullptr);
    state.appendChild(ValueTree(STATE_SAMPLER_SOUND_PARAMETER)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE, "float", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "filterADSR.sustain", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, filterADSR.sustain, nullptr),
                      nullptr);
    state.appendChild(ValueTree(STATE_SAMPLER_SOUND_PARAMETER)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE, "float", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "filterADSR.release", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, filterADSR.release, nullptr),
                      nullptr);
    state.appendChild(ValueTree(STATE_SAMPLER_SOUND_PARAMETER)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE, "float", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "filterADSR2CutoffAmt", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, filterADSR2CutoffAmt, nullptr),
                      nullptr);
    state.appendChild(ValueTree(STATE_SAMPLER_SOUND_PARAMETER)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE, "float", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "gain", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, gain, nullptr),
                      nullptr);
    state.appendChild(ValueTree(STATE_SAMPLER_SOUND_PARAMETER)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE, "float", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "ampADSR.attack", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, ampADSR.attack, nullptr),
                      nullptr);
    state.appendChild(ValueTree(STATE_SAMPLER_SOUND_PARAMETER)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE, "float", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "ampADSR.decay", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, ampADSR.decay, nullptr),
                      nullptr);
    state.appendChild(ValueTree(STATE_SAMPLER_SOUND_PARAMETER)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE, "float", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "ampADSR.sustain", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, ampADSR.sustain, nullptr),
                      nullptr);
    state.appendChild(ValueTree(STATE_SAMPLER_SOUND_PARAMETER)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE, "float", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "ampADSR.release", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, ampADSR.release, nullptr),
                      nullptr);
    state.appendChild(ValueTree(STATE_SAMPLER_SOUND_PARAMETER)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE, "float", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "pan", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, pan, nullptr),
                      nullptr);
    state.appendChild(ValueTree(STATE_SAMPLER_SOUND_PARAMETER)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE, "int", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "midiRootNote", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, midiRootNote, nullptr),
                      nullptr);
    state.appendChild(ValueTree(STATE_SAMPLER_SOUND_PARAMETER)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE, "float", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "pitch", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, pitch, nullptr),
                      nullptr);
    state.appendChild(ValueTree(STATE_SAMPLER_SOUND_PARAMETER)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE, "float", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "pitchBendRangeUp", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, pitchBendRangeUp, nullptr),
                      nullptr);
    state.appendChild(ValueTree(STATE_SAMPLER_SOUND_PARAMETER)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE, "float", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "pitchBendRangeDown", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, pitchBendRangeDown, nullptr),
                      nullptr);
    state.appendChild(ValueTree(STATE_SAMPLER_SOUND_PARAMETER)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE, "float", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "mod2CutoffAmt", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, mod2CutoffAmt, nullptr),
                      nullptr);
    state.appendChild(ValueTree(STATE_SAMPLER_SOUND_PARAMETER)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE, "float", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "mod2GainAmt", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, mod2GainAmt, nullptr),
                      nullptr);
    state.appendChild(ValueTree(STATE_SAMPLER_SOUND_PARAMETER)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE, "float", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "mod2PitchAmt", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, mod2PitchAmt, nullptr),
                      nullptr);
    state.appendChild(ValueTree(STATE_SAMPLER_SOUND_PARAMETER)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE, "float", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "mod2PlayheadPos", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, mod2PlayheadPos, nullptr),
                      nullptr);
    state.appendChild(ValueTree(STATE_SAMPLER_SOUND_PARAMETER)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE, "float", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "vel2CutoffAmt", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, vel2CutoffAmt, nullptr),
                      nullptr);
    state.appendChild(ValueTree(STATE_SAMPLER_SOUND_PARAMETER)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE, "float", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "vel2GainAmt", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, vel2GainAmt, nullptr),
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

BigInteger SourceSamplerSound::getMappedMidiNotes()
{
    return midiNotes;
}

void SourceSamplerSound::setMappedMidiNotes(BigInteger newMappedMidiNotes)
{
    midiNotes = newMappedMidiNotes;
}

int SourceSamplerSound::getMidiRootNote()
{
    return midiRootNote;
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
