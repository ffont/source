/*
  ==============================================================================

    This file was auto-generated!

    It contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "api_key.h"
#include <climits>  // for using INT_MAX

//==============================================================================
SourceSamplerAudioProcessor::SourceSamplerAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", AudioChannelSet::stereo(), true)
                     #endif
                       )
#endif
{
    
    #if ELK_BUILD
    sourceDataLocation = File("/udata/source/");
    soundsDownloadLocation = File("/udata/source/sounds/");
    presetFilesLocation = File("/udata/source/presets/");
    #else
    sourceDataLocation = File::getSpecialLocation(File::userDocumentsDirectory).getChildFile("SourceSampler/");
    soundsDownloadLocation = File::getSpecialLocation(File::userDocumentsDirectory).getChildFile("SourceSampler/sounds");
    presetFilesLocation = File::getSpecialLocation(File::userDocumentsDirectory).getChildFile("SourceSampler/presets");
    #endif
    
    if (!sourceDataLocation.exists()){
        sourceDataLocation.createDirectory();
    }
    if (!soundsDownloadLocation.exists()){
        soundsDownloadLocation.createDirectory();
    }
    if (!presetFilesLocation.exists()){
        presetFilesLocation.createDirectory();
    }
    
    midicounter = 1;
    startTime = Time::getMillisecondCounterHiRes() * 0.001;
    
    // Action listeners
    serverInterface.addActionListener(this);
    sampler.addActionListener(this);
    downloader.addActionListener(this);
    
    // Load global settings and do extra configuration
    loadGlobalPersistentStateFromFile();
    downloader.setBaseDownloadLocation(soundsDownloadLocation);
    
    // Start timer to collect state and pass it to the UI
    #if ENABLE_HTTP_SERVER
    startTimerHz(STATE_UPDATE_HZ);
    #endif
    
    // If on ELK build, start loading preset 0
    #if ELK_BUILD
    setCurrentProgram(0);
    #endif

}

SourceSamplerAudioProcessor::~SourceSamplerAudioProcessor()
{
    // Save current global persistent state (global settings)
    saveGlobalPersistentStateToFile();
    
    // Remove listeners
    serverInterface.removeActionListener(this);
    sampler.removeActionListener(this);
    downloader.removeActionListener(this);
}

//==============================================================================
std::string SourceSamplerAudioProcessor::exec(const char* cmd) {
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

//==============================================================================
const String SourceSamplerAudioProcessor::getName() const
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
    int numPresets = presetNumberMapping.getNumChildren(); //TODO: should this be the biggest integer?
    if (numPresets > 0)
        return numPresets;
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int SourceSamplerAudioProcessor::getCurrentProgram()
{
    return currentPresetIndex;
}

void SourceSamplerAudioProcessor::setCurrentProgram (int index)
{
    bool loaded = loadPresetFromFile(getPresetFilenameByIndex(index));
    if (loaded){
        currentPresetIndex = index;
    } else {
        // No preset exists at that number, create a new empty one
        currentPresetIndex = index;
        presetName = "empty";
        query = "";
        loadedSoundsInfo = {};
        sampler.clearSounds();
        sampler.setSamplerVoices(16);
    }
}

const String SourceSamplerAudioProcessor::getProgramName (int index)
{
    File location = getPresetFilePath(getPresetFilenameByIndex(index));
    if (location.existsAsFile()){
        XmlDocument xmlDocument (location);
        std::unique_ptr<XmlElement> xmlState = xmlDocument.getDocumentElement();
        if (xmlState.get() != nullptr){
            ValueTree state = ValueTree::fromXml(*xmlState.get());
            if (state.hasProperty(STATE_PRESET_NAME)){
                return state.getProperty(STATE_PRESET_NAME).toString();
            }
        }
    }
    return {};
}

void SourceSamplerAudioProcessor::changeProgramName (int index, const String& newName)
{
    File location = getPresetFilePath(getPresetFilenameByIndex(index));
    if (location.existsAsFile()){
        XmlDocument xmlDocument (location);
        std::unique_ptr<XmlElement> xmlState = xmlDocument.getDocumentElement();
        if (xmlState.get() != nullptr){
            ValueTree state = ValueTree::fromXml(*xmlState.get());
            state.setProperty(STATE_PRESET_NAME, newName, nullptr);
            std::unique_ptr<XmlElement> updatedXmlState (state.createXml());
            String filename = getPresetFilenameFromNameAndIndex(newName, index);
            File location = getPresetFilePath(filename);
            if (location.existsAsFile()){
                // If already exists, delete it
                location.deleteFile();
            }
            updatedXmlState->writeTo(location);
            updatePresetNumberMapping(filename, index);
        }
    }
}

String SourceSamplerAudioProcessor::getPresetFilenameByIndex(int index)
{
    for (int i=0; i<presetNumberMapping.getNumChildren(); i++){
        ValueTree presetMapping = presetNumberMapping.getChild(i);
        int mappingNumber = (int)presetMapping.getProperty(GLOBAL_PERSISTENT_STATE_PRESET_NUMBER_MAPPING_NUMBER);
        if (mappingNumber == index){
            String filename = presetMapping.getProperty(GLOBAL_PERSISTENT_STATE_PRESET_NUMBER_MAPPING_FILENAME).toString();
            return filename;
        }
    }
    return "";
}

//==============================================================================
void SourceSamplerAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    logToState("Called prepareToPlay with sampleRate " + (String)sampleRate + " and block size " + (String)samplesPerBlock);
    sampler.prepare ({ sampleRate, (juce::uint32) samplesPerBlock, 2 });
    
    // Configure level measurer
    lms.resize(getTotalNumOutputChannels(), 200 * 0.001f * sampleRate / samplesPerBlock); // 200ms average window
}

void SourceSamplerAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
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
    if (layouts.getMainOutputChannelSet() != AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != AudioChannelSet::stereo())
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

void SourceSamplerAudioProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midiMessages)
{
    #if ELK_BUILD
    // It is very ugly to do this here... should find an alternative!
    if (!aconnectWasRun){
        String aconnectCommandLine =  "aconnect " + String(ACONNECT_MIDI_INTERFACE_ID) + " " + String(ACONNECT_SUSHI_ID);
        logToState("Calling aconnect to setup MIDI in to sushi connection");
        exec(static_cast<const char*> (aconnectCommandLine.toUTF8()));
        aconnectWasRun = true;
    }
    #endif
    
    // Add MIDI messages from editor to the midiMessages buffer so when we click in the sound from the editor
    // it gets played here
    midiMessages.addEvents(midiFromEditor, 0, INT_MAX, 0);
    midiFromEditor.clear();
    
    // Render sampler voices into buffer
    sampler.renderNextBlock(buffer, midiMessages, 0, buffer.getNumSamples());
    
    // Measure audio levels (will be store in lms object itself)
    lms.measureBlock (buffer);
    
    // Remove midi messages from buffer if these should not be forwarded
    if (!midiOutForwardsMidiIn){
        midiMessages.clear();
    }
    
    /*
     // TESTING code for MIDI out messages
    std::cout << "midi out" << std::endl;
    MidiMessage message = MidiMessage::noteOn(1, 65, (uint8)127);
    double timestamp = Time::getMillisecondCounterHiRes() * 0.001 - getStartTime();
    message.setTimeStamp(timestamp);
    auto sampleNumber =  (int) (timestamp * getSampleRate());
    midiMessages.addEvent (message, sampleNumber);
     */
    
}

//==============================================================================
bool SourceSamplerAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

AudioProcessorEditor* SourceSamplerAudioProcessor::createEditor()
{
    return new SourceSamplerAudioProcessorEditor (*this);
}

//==============================================================================
ValueTree SourceSamplerAudioProcessor::collectPresetStateInformation ()
{
    ValueTree state = ValueTree(STATE_PRESET_IDENTIFIER);
    
    // Add general stuff
    state.setProperty(STATE_PRESET_NAME, presetName, nullptr);
    state.setProperty(STATE_PRESET_NUMBER, currentPresetIndex, nullptr);
    state.setProperty(STATE_QUERY, query, nullptr);
    
    // Add sampler main settings (not including individual sound settings because it will be in soundsData value tree)
    state.appendChild(sampler.getState(), nullptr);
    
    // Add sounds info (including sampler sound settings)
    ValueTree soundsData = ValueTree(STATE_SOUNDS_INFO);
    for (int i=0; i<loadedSoundsInfo.getNumChildren(); i++){
        ValueTree soundInfo = loadedSoundsInfo.getChild(i).createCopy();  // Retrieve basic sound info
        soundInfo.removeChild(soundInfo.getChildWithName(STATE_SAMPLER_SOUND), nullptr); // Remove existing sampler sound child so we replace it with updated version from sampler
        ValueTree samplerSoundInfo = sampler.getStateForSound(i);
        soundInfo.appendChild(samplerSoundInfo, nullptr);
        soundsData.appendChild(soundInfo, nullptr);
    }
    state.appendChild(soundsData, nullptr);
    
    return state;
}

void SourceSamplerAudioProcessor::getStateInformation (MemoryBlock& destData)
{
    // Save current state information to memory block
    ValueTree state = collectPresetStateInformation();
    std::unique_ptr<XmlElement> xml (state.createXml());
    //std::cout << xml->createDocument("") <<std::endl; // Print state for debugging purposes
    copyXmlToBinary (*xml, destData);
}

void SourceSamplerAudioProcessor::saveCurrentPresetToFile (const String& _presetName, int index)
{
    if (_presetName == ""){
        // No name provided, generate unique name
        presetName = "unnamed";
        for (int i=0; i<8; i++){
            presetName = presetName + (String)juce::Random::getSystemRandom().nextInt (9);
        }
    } else {
        presetName = _presetName;
    }

    ValueTree state = collectPresetStateInformation();
    std::unique_ptr<XmlElement> xml (state.createXml());
    String filename = getPresetFilenameFromNameAndIndex(presetName, index);
    File location = getPresetFilePath(filename);
    if (location.existsAsFile()){
        // If already exists, delete it
        location.deleteFile();
    }
    xml->writeTo(location);
    if (index > -1){
        updatePresetNumberMapping(filename, index);
    }
}

bool SourceSamplerAudioProcessor::loadPresetFromFile (const String& fileName)
{
    File location = getPresetFilePath(fileName);
    if (location.existsAsFile()){
        XmlDocument xmlDocument (location);
        std::unique_ptr<XmlElement> xmlState = xmlDocument.getDocumentElement();
        if (xmlState.get() != nullptr){
            ValueTree presetState = ValueTree::fromXml(*xmlState.get());
            loadPresetFromStateInformation(presetState);
            return true;
        }
    }
    return false; // No file found
}

void SourceSamplerAudioProcessor::loadPresetFromStateInformation (ValueTree state)
{
    // Load state informaiton form XML state
    logToState("Loading state...");
    
    // Set main stuff
    if (state.hasProperty(STATE_QUERY)){
        query = state.getProperty(STATE_QUERY).toString();
    }
    if (state.hasProperty(STATE_PRESET_NAME)){
        presetName = state.getProperty(STATE_PRESET_NAME).toString();
    }
    if (state.hasProperty(STATE_PRESET_NUMBER)){
        currentPresetIndex = (int)state.getProperty(STATE_PRESET_NUMBER);
    }
    
    // Now load sampler state (does not include sound properties)
    ValueTree samplerState = state.getChildWithName(STATE_SAMPLER);
    if (samplerState.isValid()){
        sampler.loadState(samplerState);
    }

    // Now load sounds into the sampler: download if needed, create SamplerSound objects and adjust parameters following state info
    ValueTree soundsInfo = state.getChildWithName(STATE_SOUNDS_INFO);
    if (soundsInfo.isValid()){
        soundsToLoadInfo = soundsInfo;
        downloadSounds(true);  // Download sounds sync (sounds will most probably already be in disk)
        loadDownloadedSoundsIntoSampler();
    }
}

void SourceSamplerAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // Load state information form memory block, convert to XML and call function to
    // load preset from state xml
    std::unique_ptr<XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr){
        loadPresetFromStateInformation(ValueTree::fromXml(*xmlState.get()));
    }
}

ValueTree SourceSamplerAudioProcessor::collectGlobalSettingsStateInformation ()
{
    ValueTree settings = ValueTree(GLOBAL_PERSISTENT_STATE);
    settings.setProperty(GLOBAL_PERSISTENT_STATE_MIDI_IN_CHANNEL, sampler.midiInChannel, nullptr);
    settings.setProperty(GLOBAL_PERSISTENT_STATE_MIDI_THRU, midiOutForwardsMidiIn, nullptr);
    settings.appendChild(presetNumberMapping.createCopy(), nullptr);
    return settings;
}

void SourceSamplerAudioProcessor::saveGlobalPersistentStateToFile()
{
    // This is to save settings that need to persist between sampler runs and that do not
    // change per preset
    ValueTree settings = collectGlobalSettingsStateInformation();
    std::unique_ptr<XmlElement> xml (settings.createXml());
    File location = getGlobalSettingsFilePathFromName();
    if (location.existsAsFile()){
        // If already exists, delete it
        location.deleteFile();
    }
    xml->writeTo(location);
}

void SourceSamplerAudioProcessor::loadGlobalPersistentStateFromFile()
{
    File location = getGlobalSettingsFilePathFromName();
    if (location.existsAsFile()){
        XmlDocument xmlDocument (location);
        std::unique_ptr<XmlElement> xmlState = xmlDocument.getDocumentElement();
        if (xmlState.get() != nullptr){
            ValueTree settings = ValueTree::fromXml(*xmlState.get());
            
            if (settings.hasProperty(GLOBAL_PERSISTENT_STATE_MIDI_IN_CHANNEL)){
                sampler.midiInChannel = (int)settings.getProperty(GLOBAL_PERSISTENT_STATE_MIDI_IN_CHANNEL);
            }
            
            if (settings.hasProperty(GLOBAL_PERSISTENT_STATE_MIDI_THRU)){
                midiOutForwardsMidiIn = (bool)settings.getProperty(GLOBAL_PERSISTENT_STATE_MIDI_THRU);
            }
            
            ValueTree _presetNumberMapping = settings.getChildWithName(GLOBAL_PERSISTENT_STATE_PRESETS_MAPPING);
            if (_presetNumberMapping.isValid()){
                presetNumberMapping = _presetNumberMapping;
            }
        }
    }
}

void SourceSamplerAudioProcessor::updatePresetNumberMapping(const String& presetName, int index)
{
    // If preset already exists at this index location, remove it from the value tree so later we re-add it (updated)
    if (getPresetFilenameByIndex(index) != ""){
        ValueTree newPresetNumberMapping = ValueTree(GLOBAL_PERSISTENT_STATE_PRESETS_MAPPING);
        for (int i=0; i<presetNumberMapping.getNumChildren(); i++){
            ValueTree preset = presetNumberMapping.getChild(i);
            if (index != (int)preset.getProperty(GLOBAL_PERSISTENT_STATE_PRESET_NUMBER_MAPPING_NUMBER)){
                newPresetNumberMapping.appendChild(preset.createCopy(), nullptr);
            }
        }
        presetNumberMapping = newPresetNumberMapping;
    }
    
    // Add entry to the preset mapping list
    ValueTree mapping = ValueTree(GLOBAL_PERSISTENT_STATE_PRESET_NUMBER_MAPPING);
    mapping.setProperty(GLOBAL_PERSISTENT_STATE_PRESET_NUMBER_MAPPING_NUMBER, index, nullptr);
    mapping.setProperty(GLOBAL_PERSISTENT_STATE_PRESET_NUMBER_MAPPING_FILENAME, presetName, nullptr);
    presetNumberMapping.appendChild(mapping, nullptr);
    saveGlobalPersistentStateToFile();
}

File SourceSamplerAudioProcessor::getPresetFilePath(const String& presetFilename)
{
    return presetFilesLocation.getChildFile(presetFilename).withFileExtension("xml");
}

String SourceSamplerAudioProcessor::getPresetFilenameFromNameAndIndex(const String& presetName, int index)
{
    return (String)index; // Only use index as filename
}


File SourceSamplerAudioProcessor::getGlobalSettingsFilePathFromName()
{
    return sourceDataLocation.getChildFile("settings").withFileExtension("xml");
}

ValueTree SourceSamplerAudioProcessor::collectVolatileStateInformation (){
    ValueTree state = ValueTree(STATE_VOLATILE_IDENTIFIER);
    
    String voiceActivations = "";
    String voiceSoundIdxs = "";
    String voiceSoundPlayPositions = "";
    
    for (int i=0; i<sampler.getNumVoices(); i++){
        SourceSamplerVoice* voice = static_cast<SourceSamplerVoice*> (sampler.getVoice(i));
        if (voice->isVoiceActive()){
            voiceActivations += "1,";
            if (auto* playingSound = voice->getCurrentlyPlayingSourceSamplerSound())
            {
                voiceSoundIdxs += (String)playingSound->getIdx() + ",";
            } else {
                voiceSoundIdxs += "-1,";
            }
            voiceSoundPlayPositions += (String)voice->getPlayingPositionPercentage() + ",";
        } else {
            voiceActivations += "0,";
            voiceSoundIdxs += "-1,";
            voiceSoundPlayPositions += "-1,";
        }
    }
    
    state.setProperty(STATE_VOLATILE_VOICE_ACTIVATIONS, voiceActivations, nullptr);
    state.setProperty(STATE_VOLATILE_VOICE_SOUND_IDXS, voiceSoundIdxs, nullptr);
    state.setProperty(STATE_VOLATILE_VOICE_SOUND_PLAY_POSITION, voiceSoundPlayPositions, nullptr);
    
    String audioLevels = "";
    for (int i=0; i<getTotalNumOutputChannels(); i++){
        audioLevels += (String)lms.getRMSLevel(i) + ",";
    }
    state.setProperty(STATE_VOLATILE_AUDIO_LEVELS, audioLevels, nullptr);
    
    return state;
}

//==============================================================================

void SourceSamplerAudioProcessor::actionListenerCallback (const String &message)
{
    //DBG("Action message: " << message);
    
    if (message.startsWith(String(ACTION_FINISHED_DOWNLOADING_SOUNDS))){
        // Sounds have finished downloading, trigger loading into sampler
        loadDownloadedSoundsIntoSampler();
    } else if (message.startsWith(String(ACTION_NEW_QUERY_TRIGGERED_FROM_SERVER))){
        String serializedParameters = message.substring(String(ACTION_NEW_QUERY_TRIGGERED_FROM_SERVER).length() + 1);
        StringArray tokens;
        tokens.addTokens (serializedParameters, (String)SERIALIZATION_SEPARATOR, "");
        String query = tokens[0];
        int numSounds = tokens[1].getIntValue();
        float maxSoundLength = tokens[2].getFloatValue();
        makeQueryAndLoadSounds(query, numSounds, maxSoundLength);
        
    } else if (message.startsWith(String(ACTION_SET_SOUND_PARAMETER_FLOAT))){
        String serializedParameters = message.substring(String(ACTION_SET_SOUND_PARAMETER_FLOAT).length() + 1);
        StringArray tokens;
        tokens.addTokens (serializedParameters, (String)SERIALIZATION_SEPARATOR, "");
        int soundIndex = tokens[0].getIntValue();  // -1 means all sounds
        String parameterName = tokens[1];
        float parameterValue = tokens[2].getFloatValue();
        DBG("Setting FLOAT parameter " << parameterName << " of sound " << soundIndex << " to value " << parameterValue);
        if ((soundIndex >= 0) && (soundIndex < sampler.getNumSounds())){
            if (soundIndex < sampler.getNumSounds()){
                auto* sound = sampler.getSourceSamplerSoundByIdx(soundIndex);  // This index is provided by the UI and corresponds to the position in loadedSoundsInfo, which matches idx property of SourceSamplerSound
                if (sound != nullptr){
                    sound->setParameterByNameFloat(parameterName, parameterValue);
                }
            }
        } else {
            for (int i=0; i<sampler.getNumSounds(); i++){
                auto* sound = sampler.getSourceSamplerSoundInPosition(i);  // Here we do want to get sound by sampler position (not loadedSoundsInfo idx) because we're iterating over them all
                if (sound != nullptr){
                    sound->setParameterByNameFloat(parameterName, parameterValue);
                }
            }
        }
        
    } else if (message.startsWith(String(ACTION_SET_SOUND_PARAMETER_INT))){
        String serializedParameters = message.substring(String(ACTION_SET_SOUND_PARAMETER_INT).length() + 1);
        StringArray tokens;
        tokens.addTokens (serializedParameters, (String)SERIALIZATION_SEPARATOR, "");
        int soundIndex = tokens[0].getIntValue();  // -1 means all sounds
        String parameterName = tokens[1];
        int parameterValue = tokens[2].getIntValue();
        DBG("Setting INT parameter " << parameterName << " of sound " << soundIndex << " to value " << parameterValue);
        if ((soundIndex >= 0) && (soundIndex < sampler.getNumSounds())){
            if (soundIndex < sampler.getNumSounds()){
                auto* sound = sampler.getSourceSamplerSoundByIdx(soundIndex);  // This index is provided by the UI and corresponds to the position in loadedSoundsInfo, which matches idx property of SourceSamplerSound
                if (sound != nullptr){
                    sound->setParameterByNameInt(parameterName, parameterValue);
                }
            }
        } else {
            for (int i=0; i<sampler.getNumSounds(); i++){
                auto* sound = sampler.getSourceSamplerSoundInPosition(i);  // Here we do want to get sound by sampler position (not loadedSoundsInfo idx) because we're iterating over them all
                if (sound != nullptr){
                    sound->setParameterByNameInt(parameterName, parameterValue);
                }
            }
        }
        
    } else if (message.startsWith(String(ACTION_SET_REVERB_PARAMETERS))){
        String serializedParameters = message.substring(String(ACTION_SET_REVERB_PARAMETERS).length() + 1);
        StringArray tokens;
        tokens.addTokens (serializedParameters, (String)SERIALIZATION_SEPARATOR, "");
        Reverb::Parameters reverbParameters;
        reverbParameters.roomSize = tokens[0].getFloatValue();
        reverbParameters.damping = tokens[1].getFloatValue();
        reverbParameters.wetLevel = tokens[2].getFloatValue();
        reverbParameters.dryLevel = tokens[3].getFloatValue();
        reverbParameters.width = tokens[4].getFloatValue();
        reverbParameters.freezeMode = tokens[5].getFloatValue();
        sampler.setReverbParameters(reverbParameters);
        
    } else if (message.startsWith(String(ACTION_SAVE_CURRENT_PRESET))){
        String serializedParameters = message.substring(String(ACTION_SAVE_CURRENT_PRESET).length() + 1);
        StringArray tokens;
        tokens.addTokens (serializedParameters, (String)SERIALIZATION_SEPARATOR, "");
        String presetName = tokens[0];
        int index = tokens[1].getIntValue();
        saveCurrentPresetToFile(presetName, index);
        
    } else if (message.startsWith(String(ACTION_LOAD_PRESET))){
        int index = message.substring(String(ACTION_LOAD_PRESET).length() + 1).getIntValue();
        setCurrentProgram(index);
        
    } else if (message.startsWith(String(ACTION_SET_MIDI_IN_CHANNEL))){
        int channel = message.substring(String(ACTION_SET_MIDI_IN_CHANNEL).length() + 1).getIntValue();
        setMidiInChannelFilter(channel);
        
    } else if (message.startsWith(String(ACTION_SET_MIDI_THRU))){
        bool midiThru = message.substring(String(ACTION_SET_MIDI_THRU).length() + 1).getIntValue() == 1;
        setMidiThru(midiThru);
        
    } else if (message.startsWith(String(ACTION_PLAY_SOUND))){
        int soundIndex = message.substring(String(ACTION_PLAY_SOUND).length() + 1).getIntValue();
        addToMidiBuffer(soundIndex, false);
        
    } else if (message.startsWith(String(ACTION_STOP_SOUND))){
        int soundIndex = message.substring(String(ACTION_STOP_SOUND).length() + 1).getIntValue();
        addToMidiBuffer(soundIndex, true);
        
    } else if (message.startsWith(String(ACTION_SET_POLYPHONY))){
        int numVoices = message.substring(String(ACTION_SET_POLYPHONY).length() + 1).getIntValue();
        sampler.setSamplerVoices(numVoices);

    }
}


//==============================================================================

void SourceSamplerAudioProcessor::setMidiInChannelFilter(int channel)
{
    if (channel < 0){
        channel = 0;  // Al channels
    } else if (channel > 16){
        channel = 16;
    }
    sampler.midiInChannel = channel;
    saveGlobalPersistentStateToFile();
}

void SourceSamplerAudioProcessor::setMidiThru(bool doMidiTrhu)
{
    midiOutForwardsMidiIn = doMidiTrhu;
    saveGlobalPersistentStateToFile();
}


//==============================================================================

void SourceSamplerAudioProcessor::makeQueryAndLoadSounds(const String& textQuery, int numSounds, float maxSoundLength)
{
    if (isQueryDownloadingAndLoadingSounds){
        logToState("Source is already busy querying and downloading sounds");
    }
    
    query = textQuery;
    isQueryDownloadingAndLoadingSounds = true;
    
    FreesoundClient client(FREESOUND_API_KEY);
    logToState("Querying new sounds for: " + query);
    auto filter = "duration:[0 TO " + (String)maxSoundLength + "]";
    SoundList list = client.textSearch(query, filter, "score", 0, -1, 150, "id,name,username,license,previews,analysis", "rhytwhm.onset_times", 0);
    if (list.getCount() > 0){
        logToState("Query got " + (String)list.getCount() + " results");
        Array<FSSound> sounds = list.toArrayOfSounds();
        std::random_shuffle(sounds.begin(), sounds.end());
        sounds.resize(jmin(numSounds, list.getCount()));
        
        ValueTree soundsInfo = ValueTree(STATE_SOUNDS_INFO);
        for (int i=0; i<sounds.size(); i++){
            ValueTree soundInfo = ValueTree(STATE_SOUND_INFO);
            FSSound sound = sounds[i];
            soundInfo.setProperty(STATE_SOUND_INFO_ID, sound.id, nullptr);
            soundInfo.setProperty(STATE_SOUND_INFO_NAME, sound.name, nullptr);
            soundInfo.setProperty(STATE_SOUND_INFO_USER, sound.user, nullptr);
            soundInfo.setProperty(STATE_SOUND_INFO_LICENSE, sound.license, nullptr);
            soundInfo.setProperty(STATE_SOUND_INFO_OGG_DOWNLOAD_URL,sound.getOGGPreviewURL().toString(false), nullptr);
            ValueTree soundAnalysis = ValueTree(STATE_SOUND_FS_SOUND_ANALYSIS);
            if (sound.analysis.hasProperty("rhythm")){
                if (sound.analysis["rhythm"].hasProperty("onset_times")){
                    ValueTree soundAnalysisOnsetTimes = ValueTree(STATE_SOUND_FS_SOUND_ANALYSIS_ONSETS);
                    for (int j=0; j<sound.analysis["rhythm"]["onset_times"].size(); j++){
                        ValueTree onset = ValueTree(STATE_SOUND_FS_SOUND_ANALYSIS_ONSET);
                        onset.setProperty(STATE_SOUND_FS_SOUND_ANALYSIS_ONSET_TIME, sound.analysis["rhythm"]["onset_times"][j], nullptr);
                        soundAnalysisOnsetTimes.appendChild(onset, nullptr);
                    }
                    soundAnalysis.appendChild(soundAnalysisOnsetTimes, nullptr);
                }
            }
            soundInfo.appendChild(soundAnalysis, nullptr);
            soundsInfo.appendChild(soundInfo, nullptr);
        }
        soundsToLoadInfo = soundsInfo;
        downloadSounds(true);  // Download sounds sync
        loadDownloadedSoundsIntoSampler(); // Need to call this if doing download sounds sync
    } else {
        logToState("Query got no results...");
    }
}

void SourceSamplerAudioProcessor::downloadSounds (bool blocking)
{
    #if !ELK_BUILD
    
    // Download the sounds within the plugin
    
    std::vector<std::pair<String, String>> soundIDsUrlsToDownload = {};
    for (int i=0; i<soundsToLoadInfo.getNumChildren(); i++){
        ValueTree soundInfo = soundsToLoadInfo.getChild(i);
        String soundID = soundInfo.getProperty(STATE_SOUND_INFO_ID).toString();
        String url = soundInfo.getProperty(STATE_SOUND_INFO_OGG_DOWNLOAD_URL);
        soundIDsUrlsToDownload.emplace_back(soundID, url);
    }
    
    // Download the sounds (if not already downloaded)
    logToState("Downloading new sounds...");
    downloader.setSoundsToDownload(soundIDsUrlsToDownload);
    if (!blocking){
        downloader.startThread(0);
    } else {
        downloader.downloadAllSounds();
    }
        
    #else

    // If inside ELK build, download the sounds with the python server as it seems to be much much faster
    logToState("Sending download task to python server...");
    URL url = URL("http://localhost:" + String(HTTP_DOWNLOAD_SERVER_PORT) + "/download_sounds");
    
    String urlsParam = "";
    for (int i=0; i<soundsToLoadInfo.getNumChildren(); i++){
        ValueTree soundInfo = soundsToLoadInfo.getChild(i);
        urlsParam = urlsParam + soundInfo.getProperty(STATE_SOUND_INFO_OGG_DOWNLOAD_URL).toString() + ",";
    }
    url = url.withParameter("urls", urlsParam);
    
    String header;
    int statusCode = -1;
    StringPairArray responseHeaders;
    String data = "";
    if (data.isNotEmpty()) { url = url.withPOSTData(data); }
    bool postLikeRequest = false;
    
    if (auto stream = std::unique_ptr<InputStream>(url.createInputStream(postLikeRequest, nullptr, nullptr, header,
        MAX_DOWNLOAD_WAITING_TIME_MS, // timeout in millisecs
        &responseHeaders, &statusCode)))
    {
        //Stream created successfully, store the response, log it and return the response in a pair containing (statusCode, response)
        String resp = stream->readEntireStreamAsString();
        logToState("Response with " + (String)statusCode + ": " + resp);
    } else {
        logToState("Downloading in server failed!");
    }
    
    loadDownloadedSoundsIntoSampler();
    
    #endif
}

void SourceSamplerAudioProcessor::loadDownloadedSoundsIntoSampler(){
    // Make sure that files were downloaded correctly and remove those sounds for which files were not downloaded
    ValueTree soundsInfoDownloadedOk = ValueTree(STATE_SOUNDS_INFO);
    for (int i=0; i<soundsToLoadInfo.getNumChildren(); i++){
        ValueTree soundInfo = soundsToLoadInfo.getChild(i);
        if ((soundsDownloadLocation.getChildFile(soundInfo.getProperty(STATE_SOUND_INFO_ID).toString()).withFileExtension("ogg").exists()) && (soundsDownloadLocation.getChildFile(soundInfo.getProperty(STATE_SOUND_INFO_ID).toString()).withFileExtension("ogg").getSize() > 0)){
            soundsInfoDownloadedOk.appendChild(soundInfo.createCopy(), nullptr);
        }
    }
    soundsToLoadInfo = soundsInfoDownloadedOk;  // Update stored sounds info to only include those that downloaded ok
    
    // Load sounds
    setSourceSamplerSoundObjects();  // Load the sounds in the sampler, includes setting sound parameters from state (e.g. frequency cutoff, etc...)
    loadedSoundsInfo = soundsInfoDownloadedOk; // Store loaded sounds info and...
    soundsToLoadInfo = {};  // ...clear "sounds to load" because sounds have already been loaded
    
    isQueryDownloadingAndLoadingSounds = false;  // Set flag to false because we finished downloading and loading sounds
}

void SourceSamplerAudioProcessor::setSingleSourceSamplerSoundObject(int soundIdx)
{
    int nSounds = soundsToLoadInfo.getNumChildren();
    int nNotesPerSound = 128 / nSounds;  // Only used if we need to generate midiRootNote and midiNotes
    
    ValueTree soundInfo = soundsToLoadInfo.getChild(soundIdx);
    String soundID = soundInfo.getProperty(STATE_SOUND_INFO_ID).toString();
    File audioSample = soundsDownloadLocation.getChildFile(soundID).withFileExtension("ogg");
    if (audioSample.exists() && audioSample.getSize() > 0){  // Check that file exists and is not empty
        logToState("- Adding sound " + audioSample.getFullPathName());
        
        // 1) Create SourceSamplerSound object and add to sampler
        std::unique_ptr<AudioFormatReader> reader(audioFormatManager.createReaderFor(audioSample));
        SourceSamplerSound* justAddedSound = static_cast<SourceSamplerSound*>(sampler.addSound(new SourceSamplerSound(soundIdx, String(soundIdx), *reader, MAX_SAMPLE_LENGTH, getSampleRate(), getBlockSize()))); // Create sound (this sets idx property in the sound)
        
        // Add duration information to soundsToLoadInfo ValueTree (we do it here because this is a reliable value which also takes into account MAX_SAMPLE_LENGTH)
        soundInfo.setProperty(STATE_SOUND_INFO_DURATION, justAddedSound->getLengthInSeconds(), nullptr);
        
        // 2) Load sound parameters to sampler
        
        // Get ValueTree with note parameters. If none in the preset data, create an empty one.
        ValueTree samplerSoundParameters = soundInfo.getChildWithName(STATE_SAMPLER_SOUND);
        if (!samplerSoundParameters.isValid()){ samplerSoundParameters = ValueTree(STATE_SAMPLER_SOUND); }
        
        // Check if midiRootNote is in the provided sound parameters, if not, add it with an appropriate value
        int midiRootNote = -1;
        for (int j=0; j<samplerSoundParameters.getNumChildren();j++){
            if (samplerSoundParameters.getChild(j).getProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME).toString() == "midiRootNote"){
                midiRootNote = (int)samplerSoundParameters.getChild(j).getProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE);
            }
        }
        if (midiRootNote == -1){
            // root note was not found on sound propperties, generate one assuming some nice sounds layout
            samplerSoundParameters.appendChild(ValueTree(STATE_SAMPLER_SOUND_PARAMETER)
                                            .setProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE, "int", nullptr)
                                            .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "midiRootNote", nullptr)
                                            .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, soundIdx * nNotesPerSound + nNotesPerSound / 2, nullptr),
                                            nullptr);
        }
        
        // Check if midiNotes is in the provided sound sampelr state, if not, add it with an appropriate value
        if (!samplerSoundParameters.hasProperty(STATE_SAMPLER_SOUND_MIDI_NOTES)){
            BigInteger midiNotes;
            midiNotes.setRange(soundIdx * nNotesPerSound, nNotesPerSound, true);
            samplerSoundParameters.setProperty(STATE_SAMPLER_SOUND_MIDI_NOTES, midiNotes.toString(16), nullptr);
        }
        
        // Now to load all parameters to the sound
        justAddedSound->loadState(samplerSoundParameters);
    } else {
        logToState("- Skipping sound " + (String)soundID + " (no file found or file is empty)");
    }
}

void SourceSamplerAudioProcessor::setSourceSamplerSoundObjects()
{
    sampler.clearSounds();
    if(audioFormatManager.getNumKnownFormats() == 0){ audioFormatManager.registerBasicFormats(); }
    int nSounds = soundsToLoadInfo.getNumChildren();
    logToState("Loading " + (String)nSounds + " sounds to sampler");
    if (nSounds > 0){
        for (int i = 0; i < nSounds; i++) {
            setSingleSourceSamplerSoundObject(i);
        }
    }
    logToState("Sampler sources configured with " + (String)sampler.getNumSounds() + " sounds and " + (String)sampler.getNumVoices() + " voices");
}

void SourceSamplerAudioProcessor::addToMidiBuffer(int soundNumber, bool doNoteOff)
{
    
    int nSounds = loadedSoundsInfo.getNumChildren();
    if (nSounds > 0){
        int nNotesPerSound = 128 / nSounds;
        int midiNoteForNormalPitch = soundNumber * nNotesPerSound + nNotesPerSound / 2;
        
        int midiChannel = sampler.midiInChannel;
        if (midiChannel == 0){
            midiChannel = 1; // If midi in is expected in all channels, set it to channel 1
        }
        
        MidiMessage message = MidiMessage::noteOn(midiChannel, midiNoteForNormalPitch, (uint8)127);
        if (doNoteOff){
            message = MidiMessage::noteOff(midiChannel, midiNoteForNormalPitch, (uint8)127);
        }
        
        double timestamp = Time::getMillisecondCounterHiRes() * 0.001 - getStartTime();
        message.setTimeStamp(timestamp);

        auto sampleNumber = (int)(timestamp * getSampleRate());

        midiFromEditor.addEvent(message,sampleNumber);
    }
}

double SourceSamplerAudioProcessor::getStartTime(){
    return startTime;
}

String SourceSamplerAudioProcessor::getQuery()
{
    return query;
}

void SourceSamplerAudioProcessor::timerCallback()
{
    // Collect the state and update the serverInterface object with that state information so it can be used by the http server
    ValueTree presetState = collectPresetStateInformation();
    ValueTree globalSettings = collectGlobalSettingsStateInformation();
    ValueTree volatileState = collectVolatileStateInformation();
    ValueTree fullState = ValueTree("SourceFullState");
    fullState.appendChild(presetState, nullptr);
    fullState.appendChild(globalSettings, nullptr);
    fullState.appendChild(volatileState, nullptr);
    fullState.setProperty(STATE_CURRENT_PORT, getServerInterfaceHttpPort(), nullptr);
    fullState.setProperty(STATE_LOG_MESSAGES, recentLogMessagesSerialized, nullptr);
    serverInterface.serializedAppState = fullState.toXmlString();
}


int SourceSamplerAudioProcessor::getServerInterfaceHttpPort()
{
    #if ENABLE_HTTP_SERVER
    return serverInterface.httpServer.port;
    #else
    return 0;
    #endif
}

void SourceSamplerAudioProcessor::logToState(const String& message)
{
    DBG(message);
    
    recentLogMessages.push_back(message);  // Add a new element at the end
    if (recentLogMessages.size() > 50){
        // Keep only the last 20
        std::vector<String>::const_iterator first = recentLogMessages.end() - 20;
        std::vector<String>::const_iterator last = recentLogMessages.end();
        std::vector<String> newVec(first, last);
        recentLogMessages = newVec;
    }
    recentLogMessagesSerialized = "";
    for (int i=0; i<recentLogMessages.size();i++){
        recentLogMessagesSerialized += recentLogMessages[i] +  ";";
    }
}


//==============================================================================
// This creates new instances of the plugin..
AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SourceSamplerAudioProcessor();
}
