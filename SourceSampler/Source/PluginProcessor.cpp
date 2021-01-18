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
                       ),
    sampleLoaderThread (*this),
    queryMakerThread (*this)
#endif
{
    
    #if ELK_BUILD
    sourceDataLocation = File(ELK_SOURCE_DATA_BASE_LOCATION);
    soundsDownloadLocation = File(ELK_SOURCE_SOUNDS_LOCATION);
    presetFilesLocation = File(ELK_SOURCE_PRESETS_LOCATION);
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
    
    if(audioFormatManager.getNumKnownFormats() == 0){ audioFormatManager.registerBasicFormats(); }
    
    midicounter = 1;
    startTime = Time::getMillisecondCounterHiRes() * 0.001;
    
    // Action listeners
    serverInterface.addActionListener(this);
    sampler.addActionListener(this);
    downloader.addActionListener(this);
    
    // Load global settings and do extra configuration
    loadGlobalPersistentStateFromFile();
    
    // Start timer to collect state and pass it to the UI
    startTimerHz(STATE_UPDATE_HZ);
    
    // If on ELK build, start loading preset 0
    #if ELK_BUILD
    setCurrentProgram(latestLoadedPreset);
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
        loadedSoundsInfo = ValueTree(STATE_SOUNDS_INFO);
        sampler.clearSounds();
        sampler.setSamplerVoices(16);
    }
    saveGlobalPersistentStateToFile(); // Save global settings to file (which inlucdes the latest loaded preset index)
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
    // Check if there are MIDI CC message in the buffer which are directed to the channel we're listening to
    // and store if we're receiving any message and the last MIDI CC controller number (if there's any)
    // Also get timestamp of the last received message
    for (const MidiMessageMetadata metadata : midiMessages){
        MidiMessage message = metadata.getMessage();
        if ((sampler.midiInChannel == 0) || (message.getChannel() == sampler.midiInChannel)){
            midiMessagesPresentInLastStateReport = true;
            if (message.isController()){
                lastReceivedMIDIControllerNumber = message.getControllerNumber();
            }
        }
    }
    
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
    state.setProperty(STATE_PRESET_NOTE_LAYOUT_TYPE, noteLayoutType, nullptr);
    
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
    logToState("Saving preset to: " + location.getFullPathName());
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
    if (state.hasProperty(STATE_PRESET_NAME)){
        presetName = state.getProperty(STATE_PRESET_NAME).toString();
    }
    if (state.hasProperty(STATE_PRESET_NUMBER)){
        currentPresetIndex = (int)state.getProperty(STATE_PRESET_NUMBER);
    }
    
    // Load layout type
    if (state.hasProperty(STATE_PRESET_NOTE_LAYOUT_TYPE)){
        noteLayoutType = (int)state.getProperty(STATE_PRESET_NOTE_LAYOUT_TYPE);
    }
    
    // Now load sampler state (does not include sound properties)
    ValueTree samplerState = state.getChildWithName(STATE_SAMPLER);
    if (samplerState.isValid()){
        sampler.loadState(samplerState);
    }

    // Now load sounds into the sampler: download if needed, create SamplerSound objects and adjust parameters following state info
    ValueTree soundsInfo = state.getChildWithName(STATE_SOUNDS_INFO);
    if (soundsInfo.isValid()){
        loadedSoundsInfo = soundsInfo;
        downloadSounds(false, -1);
        // the loading of the sounds will be triggered automaticaly when download finishes
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
    settings.setProperty(GLOBAL_PERSISTENT_STATE_LATEST_LOADED_PRESET, currentPresetIndex, nullptr);
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
            
            if (settings.hasProperty(GLOBAL_PERSISTENT_STATE_LATEST_LOADED_PRESET)){
                latestLoadedPreset = (int)settings.getProperty(GLOBAL_PERSISTENT_STATE_LATEST_LOADED_PRESET);
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
    
    state.setProperty(STATE_VOLATILE_SOURCE_DATA_LOCATION, sourceDataLocation.getFullPathName(), nullptr);
    state.setProperty(STATE_VOLATILE_SOUNDS_DATA_LOCATION, soundsDownloadLocation.getFullPathName(), nullptr);
    state.setProperty(STATE_VOLATILE_PRESETS_DATA_LOCATION, presetFilesLocation.getFullPathName(), nullptr);
    
    state.setProperty(STATE_VOLATILE_IS_QUERYING_AND_DOWNLOADING_SOUNDS, isQueryDownloadingAndLoadingSounds, nullptr);
    
    state.setProperty(STATE_VOLATILE_MIDI_IN_LAST_STATE_REPORT, midiMessagesPresentInLastStateReport, nullptr);
    midiMessagesPresentInLastStateReport = false;
    state.setProperty(STATE_VOLATILE_LAST_MIDI_CC, lastReceivedMIDIControllerNumber, nullptr);
    
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
    DBG("Action message: " << message);
    
    if (message.startsWith(String(ACTION_FINISHED_DOWNLOADING_SOUND))){
        // A sound has finished downloading, trigger loading into sampler
        String soundTargetLocation = message.substring(String(ACTION_FINISHED_DOWNLOADING_SOUND).length() + 1);
        for (int i=0; i<loadedSoundsInfo.getNumChildren(); i++){
            ValueTree soundInfo = loadedSoundsInfo.getChild(i);
            String soundID = soundInfo.getProperty(STATE_SOUND_INFO_ID).toString();
            File locationToCompare = soundsDownloadLocation.getChildFile(soundID).withFileExtension("ogg");
            if (soundTargetLocation == locationToCompare.getFullPathName()){
                soundInfo.setProperty(STATE_SOUND_INFO_DOWNLOAD_PROGRESS, 100, nullptr); // Set download progress to 100 to indicate download has finished
                
                #if LOAD_SAMPLES_IN_THREAD
                    // Trigger loading of audio sample into the sampler in thread
                    sampleLoaderThread.setSoundToLoad(i);
                    sampleLoaderThread.run();
                #else
                    // Trigger loading of audio sample into the sampler
                    setSingleSourceSamplerSoundObject(i);
                #endif
            }
        }
        
        if (allSoundsFinishedDownloading()){
            isQueryDownloadingAndLoadingSounds = false;  // Set flag to false because we finished downloading and loading sounds
        }
        
    } else if (message.startsWith(String(ACTION_UPDATE_DOWNLOADING_SOUND_PROGRESS))){
        // A sound has finished downloading, trigger loading into sampler
        String serializedParameters = message.substring(String(ACTION_UPDATE_DOWNLOADING_SOUND_PROGRESS).length() + 1);
        StringArray tokens;
        tokens.addTokens (serializedParameters, (String)SERIALIZATION_SEPARATOR, "");
        String soundTargetLocation = tokens[0];
        int percentageDone = tokens[1].getIntValue();
        for (int i=0; i<loadedSoundsInfo.getNumChildren(); i++){
            ValueTree soundInfo = loadedSoundsInfo.getChild(i);
            String soundID = soundInfo.getProperty(STATE_SOUND_INFO_ID).toString();
            File locationToCompare = soundsDownloadLocation.getChildFile(soundID).withFileExtension("ogg");
            if (soundTargetLocation == locationToCompare.getFullPathName()){
                soundInfo.setProperty(STATE_SOUND_INFO_DOWNLOAD_PROGRESS, percentageDone, nullptr); // Set download progress to 100 to indicate download has finished
            }
        }
        
    } else if (message.startsWith(String(ACTION_NEW_QUERY_TRIGGERED_FROM_SERVER))){
        String serializedParameters = message.substring(String(ACTION_NEW_QUERY_TRIGGERED_FROM_SERVER).length() + 1);
        StringArray tokens;
        tokens.addTokens (serializedParameters, (String)SERIALIZATION_SEPARATOR, "");
        String query = tokens[0];
        int numSounds = tokens[1].getIntValue();
        float minSoundLength = tokens[2].getFloatValue();
        float maxSoundLength = tokens[3].getFloatValue();
        int noteMappingType = tokens[4].getFloatValue();
        noteLayoutType = noteMappingType; // Set noteLayoutType so when sounds are actually downloaded and loaded, the requested mode is used
        #if MAKE_QUERY_IN_THREAD
            queryMakerThread.setQueryParameters(query, numSounds, minSoundLength, maxSoundLength);
            queryMakerThread.run();
        #else
            makeQueryAndLoadSounds(query, numSounds, minSoundLength, maxSoundLength);
        #endif
        
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
        saveCurrentPresetToFile(presetName, index);  // Save to file...
        currentPresetIndex = index; // ...and update current preset index and name in case it was changed
        
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

    } else if (message.startsWith(String(ACTION_ADD_OR_UPDATE_CC_MAPPING))){
        String serializedParameters = message.substring(String(ACTION_ADD_OR_UPDATE_CC_MAPPING).length() + 1);
        StringArray tokens;
        tokens.addTokens (serializedParameters, (String)SERIALIZATION_SEPARATOR, "");
        int soundIndex = tokens[0].getIntValue();
        int randomID = tokens[1].getIntValue();
        int ccNumber = tokens[2].getIntValue();
        String parameterName = tokens[3];
        float minRange = tokens[4].getFloatValue();
        float maxRange = tokens[5].getFloatValue();
        auto* sound = sampler.getSourceSamplerSoundByIdx(soundIndex);  // This index is provided by the UI and corresponds to the position in loadedSoundsInfo, which matches idx property of SourceSamplerSound
        if (sound != nullptr){
            sound->addOrEditMidiMapping(randomID, ccNumber, parameterName, minRange, maxRange);
        }

    } else if (message.startsWith(String(ACTION_REMOVE_CC_MAPPING))){
        String serializedParameters = message.substring(String(ACTION_REMOVE_CC_MAPPING).length() + 1);
        StringArray tokens;
        tokens.addTokens (serializedParameters, (String)SERIALIZATION_SEPARATOR, "");
        int soundIndex = tokens[0].getIntValue();
        int randomID = tokens[1].getIntValue();
        auto* sound = sampler.getSourceSamplerSoundByIdx(soundIndex);  // This index is provided by the UI and corresponds to the position in loadedSoundsInfo, which matches idx property of SourceSamplerSound
        if (sound != nullptr){
            sound->removeMidiMapping(randomID);
        }

    } else if (message.startsWith(String(ACTION_SET_STATE_TIMER_HZ))){
        int newHz = message.substring(String(ACTION_SET_STATE_TIMER_HZ).length() + 1).getIntValue();
        stopTimer();
        startTimerHz(newHz);
        
    } else if (message.startsWith(String(ACTION_REMOVE_SOUND))){
        int soundIndex = message.substring(String(ACTION_REMOVE_SOUND).length() + 1).getIntValue();
        removeSound(soundIndex);
        
    } else if (message.startsWith(String(ACTION_ADD_OR_REPLACE_SOUND))){
        String serializedParameters = message.substring(String(ACTION_ADD_OR_REPLACE_SOUND).length() + 1);
        StringArray tokens;
        tokens.addTokens (serializedParameters, (String)SERIALIZATION_SEPARATOR, "");
        int soundIdx = tokens[0].getIntValue();
        int soundID = tokens[1].getIntValue();
        String soundName = tokens[2];
        String soundUser = tokens[3];
        String soundLicense = tokens[4];
        String oggDownloadURL = tokens[5];
        String serializedSlices = tokens[6];
        StringArray slices;
        if (serializedSlices != ""){
            slices.addTokens(serializedSlices, ",", "");
        }
        String assignedNotesBigInteger = tokens[7];
        BigInteger midiNotes;
        if (assignedNotesBigInteger != ""){
            midiNotes.parseString(assignedNotesBigInteger, 16);
        }
        String triggerDownloadSoundAction = tokens[8];
        
        addOrReplaceSoundFromBasicSoundProperties(soundIdx, soundID, soundName, soundUser, soundLicense, oggDownloadURL, slices, midiNotes, triggerDownloadSoundAction);
        
    } else if (message.startsWith(String(ACTION_REAPPLY_LAYOUT))){
        int newNoteLayout = message.substring(String(ACTION_REAPPLY_LAYOUT).length() + 1).getIntValue();
        reapplyNoteLayout(newNoteLayout);
        
    } else if (message.startsWith(String(ACTION_SET_SOUND_SLICES))){
        String serializedParameters = message.substring(String(ACTION_SET_SOUND_SLICES).length() + 1);
        StringArray tokens;
        tokens.addTokens (serializedParameters, (String)SERIALIZATION_SEPARATOR, "");
        int soundIndex = tokens[0].getIntValue();
        String serializedSlices = tokens[1];
        StringArray slices;
        slices.addTokens(serializedSlices, ",", "");
        
        // Add slices to sound info
        ValueTree soundInfo = loadedSoundsInfo.getChild(soundIndex);
        ValueTree onsetTimes = ValueTree(STATE_SOUND_FS_SOUND_ANALYSIS_ONSETS);  // TODO: this should be renamed or something to store slices with a proper name instead of fs analysis onsets
        for (int i=0; i<slices.size(); i++){
            ValueTree onset = ValueTree(STATE_SOUND_FS_SOUND_ANALYSIS_ONSET);
            onset.setProperty(STATE_SOUND_FS_SOUND_ANALYSIS_ONSET_TIME, slices[i].getFloatValue(), nullptr);
            onsetTimes.appendChild(onset, nullptr);
        }
        ValueTree existingFsAnalysis = soundInfo.getChildWithName(STATE_SOUND_FS_SOUND_ANALYSIS);
        if (!existingFsAnalysis.isValid()){ existingFsAnalysis = ValueTree(STATE_SOUND_FS_SOUND_ANALYSIS); }
        
        if (existingFsAnalysis.getChildWithName(STATE_SOUND_FS_SOUND_ANALYSIS_ONSETS).isValid()){
            // An onset analysis is preset, delete it first so then we can update with the new one
            existingFsAnalysis.removeChild(existingFsAnalysis.getChildWithName(STATE_SOUND_FS_SOUND_ANALYSIS_ONSETS), nullptr);
        }
        existingFsAnalysis.appendChild(onsetTimes, nullptr);
        
        // Replace the objects in the value trees
        if (soundInfo.getChildWithName(STATE_SOUND_FS_SOUND_ANALYSIS).isValid()){
            soundInfo.getChildWithName(STATE_SOUND_FS_SOUND_ANALYSIS) = existingFsAnalysis;
        } else {
            soundInfo.appendChild(existingFsAnalysis, nullptr);
        }
        loadedSoundsInfo.getChild(soundIndex) = soundInfo;
        
        // Load them into sound object
        auto* sound = sampler.getSourceSamplerSoundByIdx(soundIndex);  // This index is provided by the UI and corresponds to the position in loadedSoundsInfo, which matches idx property of SourceSamplerSound
        if (sound != nullptr){
            std::vector<float> onsets = {};
            for (int i=0; i<onsetTimes.getNumChildren(); i++){
                ValueTree onsetVT = onsetTimes.getChild(i);
                float onset = (float)(onsetVT.getProperty(STATE_SOUND_FS_SOUND_ANALYSIS_ONSET_TIME));
                onsets.push_back(onset);
            }
            sound->setOnsetTimesSamples(onsets);
        }
    } else if (message.startsWith(String(ACTION_CLEAR_ALL_SOUNDS))){
        loadedSoundsInfo = ValueTree(STATE_SOUNDS_INFO);
        sampler.clearSounds();
        
    }
}

void SourceSamplerAudioProcessor::sendStateToExternalServer(ValueTree state)
{
    // This is only used in ELK builds in which the HTTP server is running outside the plugin
    URL url = URL("http://localhost:8123/state_from_plugin");
    String header = "Content-Type: text/xml";
    int statusCode = -1;
    StringPairArray responseHeaders;
    String data = state.toXmlString();
    if (data.isNotEmpty()) { url = url.withPOSTData(data); }
    bool postLikeRequest = true;
    if (auto stream = std::unique_ptr<InputStream>(url.createInputStream(postLikeRequest, nullptr, nullptr, header,
        MAX_DOWNLOAD_WAITING_TIME_MS, // timeout in millisecs
        &responseHeaders, &statusCode)))
    {
        // No need to read response really
        //String resp = stream->readEntireStreamAsString();
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

void SourceSamplerAudioProcessor::makeQueryAndLoadSounds(const String& textQuery, int numSounds, float minSoundLength, float maxSoundLength)
{
    if ((isQueryDownloadingAndLoadingSounds) && (Time::getMillisecondCounter() - startedQueryDownloadingAndLoadingSoundsTime < MAX_QUERY_AND_DOWNLOADING_BUSY_TIME)){
        logToState("Source is already busy querying and downloading sounds");
    }
    
    isQueryDownloadingAndLoadingSounds = true;
    startedQueryDownloadingAndLoadingSoundsTime = Time::getMillisecondCounter();
    
    FreesoundClient client(FREESOUND_API_KEY);
    logToState("Querying new sounds for: " + textQuery);
    auto filter = "duration:[" + (String)minSoundLength + " TO " + (String)maxSoundLength + "]";
    SoundList list = client.textSearch(textQuery, filter, "score", 0, -1, 150, "id,name,username,license,previews,analysis", "rhythm.onset_times", 0);
    if (list.getCount() > 0){
        logToState("Query got " + (String)list.getCount() + " results");
        
        // Generate ValueTree for loadedSoundsInfo
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
        
        // Clear existing sounds in sampler
        sampler.clearSounds();
        
        // Save the generated ValueTree to loadedSoundsInfo and trigger download of sounds
        // The loading of sounds will be triggered automatically when sounds finish downloading
        loadedSoundsInfo = soundsInfo; 
        downloadSounds(false, -1);
        
    } else {
        logToState("Query got no results...");
    }
}

void SourceSamplerAudioProcessor::downloadSounds (bool blocking, int soundIndexFilter)
{
    // NOTE: blocking only has effect in non-elk situation
    
    #if !USE_EXTERNAL_HTTP_SERVER_FOR_DOWNLOADS
    
    // Download the sounds within the plugin
    
    std::vector<std::pair<File, String>> soundTargetLocationsAndUrlsToDownload = {};
    for (int i=0; i<loadedSoundsInfo.getNumChildren(); i++){
        if ((soundIndexFilter == -1) || (soundIndexFilter == i)){
            ValueTree soundInfo = loadedSoundsInfo.getChild(i);
            String soundID = soundInfo.getProperty(STATE_SOUND_INFO_ID).toString();
            File location = soundsDownloadLocation.getChildFile(soundID).withFileExtension("ogg");
            String url = soundInfo.getProperty(STATE_SOUND_INFO_OGG_DOWNLOAD_URL);
            soundTargetLocationsAndUrlsToDownload.emplace_back(location, url);
        }
    }
    
    // Download the sounds (if not already downloaded)
    logToState("Downloading new sounds...");
    downloader.setSoundsToDownload(soundTargetLocationsAndUrlsToDownload);
    if (!blocking){
        downloader.startThread(0);
    } else {
        downloader.downloadAllSounds();
    }
        
    #else
    
    URL url;
    url = URL("http://localhost:" + String(HTTP_DOWNLOAD_SERVER_PORT) + "/download_sounds");
    String urlsParam = "";
    int nAlreadyDownloaded = 0;
    int nSentToDownload = 0;
    for (int i=0; i<loadedSoundsInfo.getNumChildren(); i++){
        if ((soundIndexFilter == -1) || (soundIndexFilter == i)){
            ValueTree soundInfo = loadedSoundsInfo.getChild(i);
            // Check if file exists, if it already exists trigger action to load it, otherwise
            // add URL to list of sounds to download
            String soundID = soundInfo.getProperty(STATE_SOUND_INFO_ID).toString();
            File audioSample = soundsDownloadLocation.getChildFile(soundID).withFileExtension("ogg");
            if (audioSample.exists() && audioSample.getSize() > 0){
                String actionMessage = String(ACTION_FINISHED_DOWNLOADING_SOUND) + ":" + audioSample.getFullPathName();
                actionListenerCallback(actionMessage);
                nAlreadyDownloaded += 1;
            } else {
                urlsParam = urlsParam + soundInfo.getProperty(STATE_SOUND_INFO_OGG_DOWNLOAD_URL).toString() + ",";
                nSentToDownload += 1;
            }
        }
    }
    url = url.withParameter("urls", urlsParam);
    url = url.withParameter("location", soundsDownloadLocation.getFullPathName());
    
    logToState("Downloading sounds...");
    logToState("- " + (String)nAlreadyDownloaded + " sounds already existing");
    logToState("- " + (String)nSentToDownload + " sounds sent to python server for downloading");
    
    if (nSentToDownload > 0){
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
            logToState("Python server download response with " + (String)statusCode + ": " + resp);
        } else {
            logToState("Python server error downloading!");
        }
    }
    
    #endif
}


bool SourceSamplerAudioProcessor::allSoundsFinishedDownloading(){
    
    for (int i=0; i<loadedSoundsInfo.getNumChildren(); i++){
        ValueTree soundInfo = loadedSoundsInfo.getChild(i);
        if ((int)soundInfo.getProperty(STATE_SOUND_INFO_DOWNLOAD_PROGRESS) != 100){
            return false;
        }
    }
    return true;
}

void SourceSamplerAudioProcessor::setSingleSourceSamplerSoundObject(int soundIdx)
{
    // Clear existing sound for this soundIdx in sampler (if any)
    sampler.clearSourceSamplerSoundByIdx(soundIdx);
    
    int nSounds = loadedSoundsInfo.getNumChildren();
    int nNotesPerSound = 128 / nSounds;  // Only used if we need to generate midiRootNote and midiNotes
    
    ValueTree soundInfo = loadedSoundsInfo.getChild(soundIdx);
    String soundID = soundInfo.getProperty(STATE_SOUND_INFO_ID).toString();
    File audioSample = soundsDownloadLocation.getChildFile(soundID).withFileExtension("ogg");
    if (audioSample.exists() && audioSample.getSize() > 0){  // Check that file exists and is not empty
        logToState("- Adding sound " + audioSample.getFullPathName());
        
        // 1) Create SourceSamplerSound object and add to sampler
        std::unique_ptr<AudioFormatReader> reader(audioFormatManager.createReaderFor(audioSample));
        SourceSamplerSound* justAddedSound = static_cast<SourceSamplerSound*>(sampler.addSound(new SourceSamplerSound(soundIdx, String(soundIdx), *reader, MAX_SAMPLE_LENGTH, getSampleRate(), getBlockSize()))); // Create sound (this sets idx property in the sound)
        
        // Pass onset times to the just added sound (if info is preset in Freesound analysis)
        ValueTree soundAnalysis = soundInfo.getChildWithName(STATE_SOUND_FS_SOUND_ANALYSIS);
        if (soundAnalysis.isValid()){
            ValueTree onsetTimes = soundAnalysis.getChildWithName(STATE_SOUND_FS_SOUND_ANALYSIS_ONSETS);
            if (onsetTimes.isValid()){
                std::vector<float> onsets = {};
                for (int i=0; i<onsetTimes.getNumChildren(); i++){
                    ValueTree onsetVT = onsetTimes.getChild(i);
                    float onset = (float)(onsetVT.getProperty(STATE_SOUND_FS_SOUND_ANALYSIS_ONSET_TIME));
                    onsets.push_back(onset);
                }
                justAddedSound->setOnsetTimesSamples(onsets);
                logToState("    ...with analysis info of " + (String)onsets.size() + " onsets");
            }
        }
        
        // Add duration information to loadedSoundsInfo ValueTree (we do it here because this is a reliable value which also takes into account MAX_SAMPLE_LENGTH)
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
            // root note was not found on sound propperties, generate considering the note mapping type
            if (noteLayoutType == NOTE_MAPPING_TYPE_CONTIGUOUS){
                // Set midi root note to the center of the assigned range
                samplerSoundParameters.appendChild(ValueTree(STATE_SAMPLER_SOUND_PARAMETER)
                    .setProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE, "int", nullptr)
                    .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "midiRootNote", nullptr)
                    .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, soundIdx * nNotesPerSound + nNotesPerSound / 2, nullptr),
                    nullptr);
            } else if (noteLayoutType == NOTE_MAPPING_TYPE_INTERLEAVED){
                // Set midi root note starting at C2 (midi note=36)
                samplerSoundParameters.appendChild(ValueTree(STATE_SAMPLER_SOUND_PARAMETER)
                    .setProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE, "int", nullptr)
                    .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "midiRootNote", nullptr)
                    .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, NOTE_MAPPING_INTERLEAVED_ROOT_NOTE + soundIdx, nullptr),
                    nullptr);
            }
        }
        
        // Check if midiNotes is in the provided sound sampelr state, if not, add it with an appropriate value
        if (!samplerSoundParameters.hasProperty(STATE_SAMPLER_SOUND_MIDI_NOTES)){
            BigInteger midiNotes;
            if (noteLayoutType == NOTE_MAPPING_TYPE_CONTIGUOUS){
                // In this case, all the notes mapped to this sound are contiguous in a range which depends on the total number of sounds to load
                midiNotes.setRange(soundIdx * nNotesPerSound, nNotesPerSound, true);
            } else if (noteLayoutType == NOTE_MAPPING_TYPE_INTERLEAVED){
                // Notes are mapped to sounds in interleaved fashion so each contiguous note corresponds to a different sound.
                int rootNoteForSound = NOTE_MAPPING_INTERLEAVED_ROOT_NOTE + soundIdx;
                for (int i=rootNoteForSound; i<128; i=i+nSounds){
                    midiNotes.setBit(i);  // Map notes in upwards direction
                }
                for (int i=rootNoteForSound; i>=0; i=i-nSounds){
                    midiNotes.setBit(i);  // Map notes in downwards direction
                }
            }
            samplerSoundParameters.setProperty(STATE_SAMPLER_SOUND_MIDI_NOTES, midiNotes.toString(16), nullptr);
        }
        
        // Now to load all parameters to the sound
        justAddedSound->loadState(samplerSoundParameters);
    } else {
        logToState("- Skipping sound " + (String)soundID + " (no file found or file is empty)");
    }
}


void SourceSamplerAudioProcessor::removeSound(int soundIndex)
{
    // Clear existing sound for this soundIdx in sampler (if any)
    sampler.clearSourceSamplerSoundByIdx(soundIndex);
    
    // Update "loadedSoundsInfo" to remove this element and, at the same time, update the "idx" in SamplerSound objects to be in sync with new "loadedSoundsInfo"
    ValueTree newLoadedSoundsInfo = ValueTree(STATE_SOUNDS_INFO);
    for (int i=0; i<loadedSoundsInfo.getNumChildren(); i++){
        if (i != soundIndex){
            newLoadedSoundsInfo.appendChild(loadedSoundsInfo.getChild(i).createCopy(), nullptr);
            if (i >= soundIndex){
                // In this case the sound will have moved one position down in the new list of "loadedSoundsInfo", so idx in the sampler should be updated
                auto* sound = sampler.getSourceSamplerSoundByIdx(i);
                if (sound != nullptr){
                    sound->setIdx(sound->getIdx() - 1);
                }
            }
        } else {
            // Do nothing here, don't add the sound we are removing to new "loadedSoundsInfo" and also don't update the index of the sound in the sampler because it was already removed
        }
    }
    loadedSoundsInfo = newLoadedSoundsInfo;
    
}

int SourceSamplerAudioProcessor::addOrReplaceSoundFromSoundInfoValueTree(int soundIndex, ValueTree newSoundInfo)
{
    bool replacing = soundIndex > -1;  // If sound index > -1, we are replacing a sound, otherwise we're adding a new one
    
    if (replacing){
        // If replacing an existing sound, first we need to remove it from the sampelr, then update "loadedSoundsInfo" with the info from the new sound (and copying note assignment information from the old sound if the new is not carrying it), and the trigger download of the new sound
        
        // Clear existing sound for this soundIdx in sampler (if any)
        sampler.clearSourceSamplerSoundByIdx(soundIndex);
        
        // Replace the corresponding position of the loadedSoundsInfo ValueTree with the new sound
        ValueTree newLoadedSoundsInfo = ValueTree(STATE_SOUNDS_INFO);
        for (int i=0; i<loadedSoundsInfo.getNumChildren(); i++){
            if (i == soundIndex){
                // Check if newSoundInfo has sotred sound sampler parameters, if not, create an empty value tree for them
                ValueTree samplerSoundParameters = newSoundInfo.getChildWithName(STATE_SAMPLER_SOUND);
                if (!samplerSoundParameters.isValid()){ samplerSoundParameters = ValueTree(STATE_SAMPLER_SOUND); }
                if (!samplerSoundParameters.hasProperty(STATE_SAMPLER_SOUND_MIDI_NOTES)){
                    // If sampler sound parameters have no MIDI notes assigned, copy them form the sound we are replacing
                    ValueTree existingSoundInfo = loadedSoundsInfo.getChild(i);
                    samplerSoundParameters.setProperty(STATE_SAMPLER_SOUND_MIDI_NOTES,
                                                       existingSoundInfo.getChildWithName(STATE_SAMPLER_SOUND).getProperty(STATE_SAMPLER_SOUND_MIDI_NOTES)
                                                       , nullptr);
                }
                
                // Finally add new modified newSoundInfo to the new loadedSoundsInfo list
                newLoadedSoundsInfo.appendChild(newSoundInfo, nullptr);
            } else {
                newLoadedSoundsInfo.appendChild(loadedSoundsInfo.getChild(i).createCopy(), nullptr);
            }
        }
        loadedSoundsInfo = newLoadedSoundsInfo;
        return soundIndex;
        
    } else {
        // If creating a new sound, just add the ValueTree to loadedSoundsInfo, get the new ID and return it
        loadedSoundsInfo.appendChild(newSoundInfo, nullptr);
        int newSoundIndex = loadedSoundsInfo.getNumChildren() - 1;
        return newSoundIndex;
    }
}

void SourceSamplerAudioProcessor::addOrReplaceSoundFromBasicSoundProperties(int soundIdx,
                                                                            int soundID,
                                                                            const String& soundName,
                                                                            const String& soundUser,
                                                                            const String& soundLicense,
                                                                            const String& oggDownloadURL,
                                                                            StringArray slices,
                                                                            BigInteger midiNotes,
                                                                            const String& triggerDownloadSoundAction)
{
    ValueTree soundInfo = ValueTree(STATE_SOUND_INFO);
    soundInfo.setProperty(STATE_SOUND_INFO_ID, soundID, nullptr);
    soundInfo.setProperty(STATE_SOUND_INFO_NAME, soundName, nullptr);
    soundInfo.setProperty(STATE_SOUND_INFO_USER, soundUser, nullptr);
    soundInfo.setProperty(STATE_SOUND_INFO_LICENSE, soundLicense, nullptr);
    soundInfo.setProperty(STATE_SOUND_INFO_OGG_DOWNLOAD_URL, oggDownloadURL, nullptr);
    soundInfo.setProperty(STATE_SOUND_INFO_OGG_DOWNLOAD_URL, oggDownloadURL, nullptr);
    
    if (!midiNotes.isZero()){
        ValueTree samplerSoundParameters = ValueTree(STATE_SAMPLER_SOUND);
        samplerSoundParameters.setProperty(STATE_SAMPLER_SOUND_MIDI_NOTES, midiNotes.toString(16), nullptr);
        soundInfo.appendChild(samplerSoundParameters, nullptr);
    }
    
    if (slices.size() > 0){
        ValueTree soundAnalysis = ValueTree(STATE_SOUND_FS_SOUND_ANALYSIS);
        ValueTree soundAnalysisOnsetTimes = ValueTree(STATE_SOUND_FS_SOUND_ANALYSIS_ONSETS);
        for (int i=0; i<slices.size(); i++){
            ValueTree onset = ValueTree(STATE_SOUND_FS_SOUND_ANALYSIS_ONSET);
            onset.setProperty(STATE_SOUND_FS_SOUND_ANALYSIS_ONSET_TIME, slices[i].getFloatValue(), nullptr);
            soundAnalysisOnsetTimes.appendChild(onset, nullptr);
        }
        soundAnalysis.appendChild(soundAnalysisOnsetTimes, nullptr);
        soundInfo.appendChild(soundAnalysis, nullptr);
    }
    
    int newSoundIdx = addOrReplaceSoundFromSoundInfoValueTree(soundIdx, soundInfo);
    
    // If requested, trigger download (and further loading) of the new sound (or of all sounds if specificed like this)
    if (triggerDownloadSoundAction == "none"){
        // Trigger no download task
    } else if (triggerDownloadSoundAction == "all"){
        // Trigger download of all sounds
        downloadSounds(false, -1);
    } else {
        // Trigger download of only the added/replaced sound
        downloadSounds(false, newSoundIdx);
    }
}

void SourceSamplerAudioProcessor::reapplyNoteLayout(int newNoteLayoutType)
{
    // TODO: this function uses code repeaded from "setSingleSourceSamplerSoundObject" method, we should abstract that
    noteLayoutType = newNoteLayoutType;
    int nSounds = loadedSoundsInfo.getNumChildren();
    if (nSounds > 0){
        int nNotesPerSound = 128 / nSounds;  // Only used if we need to generate midiRootNote and midiNotes
        
        for (int soundIdx=0; soundIdx<loadedSoundsInfo.getNumChildren(); soundIdx++){
            auto* sound = sampler.getSourceSamplerSoundByIdx(soundIdx);
            if (sound != nullptr){
            
                ValueTree samplerSoundParametersToUpdate = ValueTree(STATE_SAMPLER_SOUND);
                
                if (noteLayoutType == NOTE_MAPPING_TYPE_CONTIGUOUS){
                    // Set midi root note to the center of the assigned range
                    samplerSoundParametersToUpdate.appendChild(ValueTree(STATE_SAMPLER_SOUND_PARAMETER)
                        .setProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE, "int", nullptr)
                        .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "midiRootNote", nullptr)
                        .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, soundIdx * nNotesPerSound + nNotesPerSound / 2, nullptr),
                        nullptr);
                } else if (noteLayoutType == NOTE_MAPPING_TYPE_INTERLEAVED){
                    // Set midi root note starting at C2 (midi note=36)
                    samplerSoundParametersToUpdate.appendChild(ValueTree(STATE_SAMPLER_SOUND_PARAMETER)
                        .setProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE, "int", nullptr)
                        .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "midiRootNote", nullptr)
                        .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, NOTE_MAPPING_INTERLEAVED_ROOT_NOTE + soundIdx, nullptr),
                        nullptr);
                }
                
                BigInteger midiNotes;
                if (noteLayoutType == NOTE_MAPPING_TYPE_CONTIGUOUS){
                    // In this case, all the notes mapped to this sound are contiguous in a range which depends on the total number of sounds to load
                    midiNotes.setRange(soundIdx * nNotesPerSound, nNotesPerSound, true);
                } else if (noteLayoutType == NOTE_MAPPING_TYPE_INTERLEAVED){
                    // Notes are mapped to sounds in interleaved fashion so each contiguous note corresponds to a different sound.
                    int rootNoteForSound = NOTE_MAPPING_INTERLEAVED_ROOT_NOTE + soundIdx;
                    for (int i=rootNoteForSound; i<128; i=i+nSounds){
                        midiNotes.setBit(i);  // Map notes in upwards direction
                    }
                    for (int i=rootNoteForSound; i>=0; i=i-nSounds){
                        midiNotes.setBit(i);  // Map notes in downwards direction
                    }
                }
                samplerSoundParametersToUpdate.setProperty(STATE_SAMPLER_SOUND_MIDI_NOTES, midiNotes.toString(16), nullptr);
                
                // Now to load all parameters that set the layout to the sound
                sound->loadState(samplerSoundParametersToUpdate);
            }
        }
    }
}

void SourceSamplerAudioProcessor::addToMidiBuffer(int soundIndex, bool doNoteOff)
{
    
    int nSounds = loadedSoundsInfo.getNumChildren();
    if (nSounds > 0){
        auto* sound = sampler.getSourceSamplerSoundByIdx(soundIndex);
        int midiNoteForNormalPitch = 36;
        if (sound != nullptr){
            midiNoteForNormalPitch = sound->getMidiRootNote();
            BigInteger assignedMidiNotes = sound->getMappedMidiNotes();
            if (assignedMidiNotes[midiNoteForNormalPitch] == false){
                // If the root note is not mapped to the sound, find the closest mapped one
                midiNoteForNormalPitch = assignedMidiNotes.findNextSetBit(midiNoteForNormalPitch);
            }
        }
        if (midiNoteForNormalPitch < 0){
            // If for some reason no note was found, don't play anything
        } else {
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
}

double SourceSamplerAudioProcessor::getStartTime(){
    return startTime;
}

void SourceSamplerAudioProcessor::timerCallback()
{
    //std::cout << "Calling timer callback" << std::endl;
    
    // Collect the state and update the serverInterface object with that state information so it can be used by the http server
    ValueTree presetState = collectPresetStateInformation();
    ValueTree globalSettings = collectGlobalSettingsStateInformation();
    ValueTree volatileState = collectVolatileStateInformation();
    ValueTree fullState = ValueTree(STATE_FULL_STATE);
    fullState.appendChild(presetState, nullptr);
    fullState.appendChild(globalSettings, nullptr);
    fullState.appendChild(volatileState, nullptr);
    fullState.setProperty(STATE_LOG_MESSAGES, recentLogMessagesSerialized, nullptr);
    
    #if ENABLE_EMBEDDED_HTTP_SERVER
    fullState.setProperty(STATE_CURRENT_PORT, getServerInterfaceHttpPort(), nullptr);
    serverInterface.serializedAppState = fullState.toXmlString();
    #endif
    
    #if USE_EXTERNAL_HTTP_SERVER
    sendStateToExternalServer(fullState);
    #endif
   
}


int SourceSamplerAudioProcessor::getServerInterfaceHttpPort()
{
    #if ENABLE_EMBEDDED_HTTP_SERVER
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
