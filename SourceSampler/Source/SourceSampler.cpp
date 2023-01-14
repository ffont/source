/*
  ==============================================================================

    SourceSampler.cpp
    Created: 13 Jan 2023 1:41:00pm
    Author:  Frederic Font Corbera

  ==============================================================================
*/

#include "SourceSampler.h"
#include "api_key.h"
#include <climits>  // for using INT_MAX
#include <random> // for using shuffle


//==============================================================================
SourceSampler::SourceSampler():
    queryMakerThread (*this),
    serverInterface ([this]{return getGlobalContext();})
{
    std::cout << "Creating needed directories" << std::endl;
    createDirectories(APP_DIRECTORY_NAME);
    
    std::cout << "Configuring app" << std::endl;
    if(audioFormatManager.getNumKnownFormats() == 0){ audioFormatManager.registerBasicFormats(); }
    
    startTime = juce::Time::getMillisecondCounterHiRes() * 0.001;
    
    // Action listeners
    serverInterface.addActionListener(this);
    sampler.addActionListener(this);
    
    // Start timer to pass state to the UI and run other periodic tasks like deleting sounds that are waiting to be deleted
    std::cout << "Starting timer" << std::endl;
    startTimerHz(MAIN_TIMER_HZ);
    
    // Load empty session to state
    std::cout << "Creating default empty state" << std::endl;
    state = SourceHelpers::createDefaultEmptyState();
    
    // Add state change listener and bind cached properties to state properties (including loaded sounds)
    bindState();
    
    // Load global settings and do extra configuration
    //std::cout << "Loading global settings" << std::endl;
    //loadGlobalPersistentStateFromFile();
    // NOTE: loading global persistent state is now part of the bindState method
    
    // Notify that plugin is running
    #if SYNC_STATE_WITH_OSC
    sendOSCMessage(juce::OSCMessage("/plugin_started"));
    #endif
    sendWSMessage(juce::OSCMessage("/plugin_started"));
    
    std::cout << "SOURCE plugin is up and running!" << std::endl;
}

SourceSampler::~SourceSampler()
{
    // Save current global persistent state (global settings)
    saveGlobalPersistentStateToFile();
    
    // Remove listeners
    serverInterface.removeActionListener(this);
    sampler.removeActionListener(this);
    
    // Clean data in tmp directory
    tmpFilesLocation.deleteRecursively();
}

void SourceSampler::bindState()
{
    state.addListener(this);
    
    state.setProperty(SourceIDs::sourceDataLocation, sourceDataLocation.getFullPathName(), nullptr);
    state.setProperty(SourceIDs::soundsDownloadLocation, soundsDownloadLocation.getFullPathName(), nullptr);
    state.setProperty(SourceIDs::presetFilesLocation, presetFilesLocation.getFullPathName(), nullptr);
    state.setProperty(SourceIDs::tmpFilesLocation, tmpFilesLocation.getFullPathName(), nullptr);
    state.setProperty(SourceIDs::pluginVersion, juce::String(JucePlugin_VersionString), nullptr);
    
    SourceHelpers::addPropertyWithDefaultValueIfNotExisting(state, SourceIDs::currentPresetIndex, SourceDefaults::currentPresetIndex);
    currentPresetIndex.referTo(state, SourceIDs::currentPresetIndex, nullptr, SourceDefaults::currentPresetIndex);
    SourceHelpers::addPropertyWithDefaultValueIfNotExisting(state, SourceIDs::globalMidiInChannel, SourceDefaults::globalMidiInChannel);
    globalMidiInChannel.referTo(state, SourceIDs::globalMidiInChannel, nullptr, SourceDefaults::globalMidiInChannel);
    SourceHelpers::addPropertyWithDefaultValueIfNotExisting(state, SourceIDs::midiOutForwardsMidiIn, SourceDefaults::midiOutForwardsMidiIn);
    midiOutForwardsMidiIn.referTo(state, SourceIDs::midiOutForwardsMidiIn, nullptr, SourceDefaults::midiOutForwardsMidiIn);
    SourceHelpers::addPropertyWithDefaultValueIfNotExisting(state, SourceIDs::useOriginalFilesPreference, SourceDefaults::currentPresetIndex);
    useOriginalFilesPreference.referTo(state, SourceIDs::useOriginalFilesPreference, nullptr, SourceDefaults::useOriginalFilesPreference);
    SourceHelpers::addPropertyWithDefaultValueIfNotExisting(state, SourceIDs::freesoundOauthAccessToken, SourceDefaults::freesoundOauthAccessToken);
    freesoundOauthAccessToken.referTo(state, SourceIDs::freesoundOauthAccessToken, nullptr, SourceDefaults::freesoundOauthAccessToken);
    
    // Load global settings stored in file, now before sounds are created as these might need the oauth token
    loadGlobalPersistentStateFromFile();
    
    juce::ValueTree preset = state.getChildWithName(SourceIDs::PRESET);
    SourceHelpers::addPropertyWithDefaultValueIfNotExisting(preset, SourceIDs::numVoices, SourceDefaults::numVoices);
    numVoices.referTo(preset, SourceIDs::numVoices, nullptr, SourceDefaults::numVoices);
    SourceHelpers::addPropertyWithDefaultValueIfNotExisting(preset, SourceIDs::name, SourceHelpers::defaultPresetName());
    presetName.referTo(preset, SourceIDs::name, nullptr, SourceHelpers::defaultPresetName());
    SourceHelpers::addPropertyWithDefaultValueIfNotExisting(preset, SourceIDs::noteLayoutType, SourceDefaults::noteLayoutType);
    noteLayoutType.referTo(preset, SourceIDs::noteLayoutType, nullptr, SourceDefaults::noteLayoutType);
    SourceHelpers::addPropertyWithDefaultValueIfNotExisting(preset, SourceIDs::reverbRoomSize, SourceDefaults::reverbRoomSize);
    reverbRoomSize.referTo(preset, SourceIDs::reverbRoomSize, nullptr, SourceDefaults::reverbRoomSize);
    SourceHelpers::addPropertyWithDefaultValueIfNotExisting(preset, SourceIDs::reverbDamping, SourceDefaults::reverbDamping);
    reverbDamping.referTo(preset, SourceIDs::reverbDamping, nullptr, SourceDefaults::reverbDamping);
    SourceHelpers::addPropertyWithDefaultValueIfNotExisting(preset, SourceIDs::reverbWetLevel, SourceDefaults::reverbWetLevel);
    reverbWetLevel.referTo(preset, SourceIDs::reverbWetLevel, nullptr, SourceDefaults::reverbWetLevel);
    SourceHelpers::addPropertyWithDefaultValueIfNotExisting(preset, SourceIDs::reverbDryLevel, SourceDefaults::reverbDryLevel);
    reverbDryLevel.referTo(preset, SourceIDs::reverbDryLevel, nullptr, SourceDefaults::reverbDryLevel);
    SourceHelpers::addPropertyWithDefaultValueIfNotExisting(preset, SourceIDs::reverbWidth, SourceDefaults::reverbWidth);
    reverbWidth.referTo(preset, SourceIDs::reverbWidth, nullptr, SourceDefaults::reverbWidth);
    SourceHelpers::addPropertyWithDefaultValueIfNotExisting(preset, SourceIDs::reverbFreezeMode, SourceDefaults::reverbFreezeMode);
    reverbFreezeMode.referTo(preset, SourceIDs::reverbFreezeMode, nullptr, SourceDefaults::reverbFreezeMode);
    
    // Swap pointer with oldSound so if there were objects in there still pending to be safely deleted, these will be
    // deleted when needed. Then create a new SourceSoundList with the new preset information
    soundsOld.swap(sounds);
    sounds.reset();
    sounds = std::make_unique<SourceSoundList>(state.getChildWithName(SourceIDs::PRESET), [this]{return getGlobalContext();});
}

void SourceSampler::createDirectories(const juce::String& appDirectoryName)
{
    #if ELK_BUILD
    sourceDataLocation = juce::File(ELK_SOURCE_DATA_BASE_LOCATION);
    soundsDownloadLocation = juce::File(ELK_SOURCE_SOUNDS_LOCATION);
    presetFilesLocation = juce::File(ELK_SOURCE_PRESETS_LOCATION);
    tmpFilesLocation = juce::File(ELK_SOURCE_TMP_LOCATION);
    #else
    sourceDataLocation = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile(appDirectoryName);
    soundsDownloadLocation = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile(appDirectoryName + "/sounds");
    presetFilesLocation = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile(appDirectoryName + "/presets");
    tmpFilesLocation = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile(appDirectoryName + "/tmp");
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
    if (!tmpFilesLocation.exists()){
        tmpFilesLocation.createDirectory();
    }
}

GlobalContextStruct SourceSampler::getGlobalContext()
{
    GlobalContextStruct context;
    context.sampleRate = getSampleRate();
    context.samplesPerBlock = getBlockSize();
    context.sampler = &sampler;
    context.useOriginalFilesPreference = useOriginalFilesPreference.get();
    context.soundsDownloadLocation = soundsDownloadLocation;
    context.sourceDataLocation = sourceDataLocation;
    context.presetFilesLocation = presetFilesLocation;
    context.tmpFilesLocation = tmpFilesLocation;
    context.freesoundOauthAccessToken = freesoundOauthAccessToken.get();
    context.midiInChannel = globalMidiInChannel.get();
    return context;
}

//==============================================================================

double SourceSampler::getSampleRate(){
    return sampleRate;
}

int SourceSampler::getBlockSize(){
    return blockSize;
}

int SourceSampler::getTotalNumOutputChannels(){
    return numberOfChannels;
}

void SourceSampler::setTotalNumOutputChannels(int numChannels) {
    numberOfChannels = numChannels;
}

void SourceSampler::prepareToPlay (double _sampleRate, int _samplesPerBlock)
{
    // Save sample rate and block size to global properties because these might be needed in other parts of the processing chain
    sampleRate = _sampleRate;
    blockSize = _samplesPerBlock;
    
    DBG("Called prepareToPlay with sampleRate " + (juce::String)sampleRate + " and block size " + (juce::String)blockSize);
    
    // Prepare sampler
    sampler.prepare ({ sampleRate, (juce::uint32) blockSize, 2 });
    
    // Prepare preview player
    transportSource.prepareToPlay (blockSize, sampleRate);
    
    // Configure level measurer
    lms.resize(getTotalNumOutputChannels(), 200 * 0.001f * sampleRate / blockSize); // 200ms average window
    
    // Loaded the last loaded preset (only in ELK platform)
    # if ELK_BUILD
    if (!loadedPresetAtElkStartup){
        std::cout << "Loading latest loaded preset " << (juce::String)latestLoadedPreset << std::endl;
            setCurrentProgram(latestLoadedPreset);
        loadedPresetAtElkStartup = true;
    }
    # endif
}

void SourceSampler::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
    transportSource.releaseResources();
}

void SourceSampler::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    // Check if there are MIDI CC message in the buffer which are directed to the channel we're listening to
    // and store if we're receiving any message and the last MIDI CC controller number (if there's any)
    // Also get timestamp of the last received message
    for (const juce::MidiMessageMetadata metadata : midiMessages){
        juce::MidiMessage message = metadata.getMessage();
        if ((globalMidiInChannel == 0) || (message.getChannel() == globalMidiInChannel)){
            midiMessagesPresentInLastStateReport = true;
            if (message.isController()){
                lastReceivedMIDIControllerNumber = message.getControllerNumber();
            } else if (message.isNoteOn()){
                lastReceivedMIDINoteNumber = message.getNoteNumber();
            }
        }
    }
    
    // Add MIDI messages from editor to the midiMessages buffer so when we click in the sound from the editor
    // it gets played here
    midiMessages.addEvents(midiFromEditor, 0, buffer.getNumSamples(), 0);
    midiFromEditor.clear();
    
    // Render preview player into buffer
    transportSource.getNextAudioBlock(juce::AudioSourceChannelInfo(buffer));
    
    // Render sampler voices into buffer
    sampler.renderNextBlock(buffer, midiMessages, 0, buffer.getNumSamples());
    
    // Measure audio levels (will be store in lms object itself)
    lms.measureBlock (buffer);
    
    // Remove midi messages from buffer if these should not be forwarded
    if (!midiOutForwardsMidiIn.get()){
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

juce::String SourceSampler::getPresetFilenameByIndex(int index)
{
    return (juce::String)index + ".xml";
}

juce::String SourceSampler::getPresetNameByIndex(int index)
{
    juce::File location = getPresetFilePath(getPresetFilenameByIndex(index));
    if (location.existsAsFile()){
        juce::XmlDocument xmlDocument (location);
        std::unique_ptr<juce::XmlElement> xmlState = xmlDocument.getDocumentElement();
        if (xmlState.get() != nullptr){
            juce::ValueTree state = juce::ValueTree::fromXml(*xmlState.get());
            if (state.hasProperty(SourceIDs::name)){
                return state.getProperty(SourceIDs::name).toString();
            }
        }
    }
    return {};
}

void SourceSampler::renamePreset(int index, const juce::String& newName)
{
    juce::File location = getPresetFilePath(getPresetFilenameByIndex(index));
    if (location.existsAsFile()){
        juce::XmlDocument xmlDocument (location);
        std::unique_ptr<juce::XmlElement> xmlState = xmlDocument.getDocumentElement();
        if (xmlState.get() != nullptr){
            juce::ValueTree state = juce::ValueTree::fromXml(*xmlState.get());
            state.setProperty(SourceIDs::name, newName, nullptr);
            std::unique_ptr<juce::XmlElement> updatedXmlState (state.createXml());
            juce::String filename = getPresetFilenameFromNameAndIndex(newName, index);
            juce::File location = getPresetFilePath(filename);
            if (location.existsAsFile()){
                // If already exists, delete it
                location.deleteFile();
            }
            updatedXmlState->writeTo(location);
        }
    }
}

void SourceSampler::saveCurrentPresetToFile (const juce::String& _presetName, int index)
{
    juce::ValueTree presetState = state.getChildWithName(SourceIDs::PRESET);
    if (presetState.isValid()){
        
        if (_presetName == ""){
            // No name provided, generate unique name
            presetName = "unnamed";
            for (int i=0; i<8; i++){
                presetName = presetName + (juce::String)juce::Random::getSystemRandom().nextInt (9);
            }
        } else {
            presetName = _presetName;
        }
        
        std::unique_ptr<juce::XmlElement> xml (presetState.createXml());
        juce::String filename = getPresetFilenameFromNameAndIndex(presetName, index);
        juce::File location = getPresetFilePath(filename);
        if (location.existsAsFile()){
            // If already exists, delete it
            location.deleteFile();
        }
        DBG("Saving preset to: " + location.getFullPathName());
        xml->writeTo(location);
    }
}

bool SourceSampler::loadPresetFromFile (const juce::String& fileName)
{
    juce::File location = getPresetFilePath(fileName);
    if (location.existsAsFile()){
        juce::XmlDocument xmlDocument (location);
        std::unique_ptr<juce::XmlElement> xmlState = xmlDocument.getDocumentElement();
        if (xmlState.get() != nullptr){
            juce::ValueTree newState = SourceHelpers::createNewStateFromCurrentSatate(state);
            juce::ValueTree presetState = juce::ValueTree::fromXml(*xmlState.get());
            // Check if preset has old format, and if so transform it to the new format
            // In old format, root preset node type is "SourcePresetState", in the new one it is "PRESET"
            if (presetState.getType().toString() == "SourcePresetState"){
                DBG("Old preset file found, transforming to new format");
                juce::ValueTree modifiedPresetState = SourceHelpers::createEmptyPresetState();
                // Preset is of new format, we can add it to the new state without modification
                modifiedPresetState.setProperty(SourceIDs::name, presetState.getProperty("presetName", "old preset no name"), nullptr);
                modifiedPresetState.setProperty(SourceIDs::noteLayoutType, presetState.getProperty("noteLayoutType", SourceDefaults::noteLayoutType), nullptr);
                juce::ValueTree samplerState = presetState.getChildWithName("Sampler");
                if (samplerState.isValid()){
                    modifiedPresetState.setProperty(SourceIDs::numVoices, samplerState.getProperty("NumVoices", SourceDefaults::numVoices), nullptr);
                }
                juce::ValueTree reverbParamsState = samplerState.getChildWithName("ReverbParameters");
                if (reverbParamsState.isValid()){
                    modifiedPresetState.setProperty(SourceIDs::reverbDamping, reverbParamsState.getProperty("reverb_damping", SourceDefaults::reverbDamping), nullptr);
                    modifiedPresetState.setProperty(SourceIDs::reverbWetLevel, reverbParamsState.getProperty("reverb_wetLevel", SourceDefaults::reverbWetLevel), nullptr);
                    modifiedPresetState.setProperty(SourceIDs::reverbDryLevel, reverbParamsState.getProperty("reverb_dryLevel", SourceDefaults::reverbDryLevel), nullptr);
                    modifiedPresetState.setProperty(SourceIDs::reverbWidth, reverbParamsState.getProperty("reverb_width", SourceDefaults::reverbWidth), nullptr);
                    modifiedPresetState.setProperty(SourceIDs::reverbFreezeMode, reverbParamsState.getProperty("reverb_freezeMode", SourceDefaults::reverbFreezeMode), nullptr);
                    modifiedPresetState.setProperty(SourceIDs::reverbRoomSize, reverbParamsState.getProperty("reverb_roomSize", SourceDefaults::reverbRoomSize), nullptr);
                }
                juce::ValueTree soundsInfoState = presetState.getChildWithName("soundsInfo");
                for (int i=0; i<soundsInfoState.getNumChildren(); i++){
                    auto soundInfo = soundsInfoState.getChild(i);
                    auto samplerSound = soundInfo.getChildWithName("SamplerSound");
                    juce::BigInteger midiNotes = 0;
                    midiNotes.parseString(samplerSound.getProperty("midiNotes", SourceDefaults::midiNotes).toString(), 16);
                    juce::ValueTree sound = SourceHelpers::createSourceSoundAndSourceSamplerSoundFromProperties((int)soundInfo.getProperty("soundId"), soundInfo.getProperty("soundName"), soundInfo.getProperty("soundUser"), soundInfo.getProperty("soundLicense"), soundInfo.getProperty("soundOGGURL"), "", "", -1, {}, midiNotes, (int)samplerSound.getChildWithProperty("parameter_name", "midiRootNote").getProperty("parameter_value"), 0);
                    sound.getChildWithName(SourceIDs::SOUND_SAMPLE).setProperty(SourceIDs::usesPreview, true, nullptr);
                    
                    sound.setProperty(SourceIDs::launchMode, (int)samplerSound.getChildWithProperty("parameter_name", "launchMode").getProperty("parameter_value"), nullptr);
                    sound.setProperty(SourceIDs::startPosition, (float)samplerSound.getChildWithProperty("parameter_name", "startPosition").getProperty("parameter_value"), nullptr);
                    sound.setProperty(SourceIDs::endPosition, (float)samplerSound.getChildWithProperty("parameter_name", "endPosition").getProperty("parameter_value"), nullptr);
                    sound.setProperty(SourceIDs::loopStartPosition, (float)samplerSound.getChildWithProperty("parameter_name", "loopStartPosition").getProperty("parameter_value"), nullptr);
                    sound.setProperty(SourceIDs::loopEndPosition, (float)samplerSound.getChildWithProperty("parameter_name", "loopEndPosition").getProperty("parameter_value"), nullptr);
                    sound.setProperty(SourceIDs::loopXFadeNSamples, (int)samplerSound.getChildWithProperty("parameter_name", "loopXFadeNSamples").getProperty("parameter_value"), nullptr);
                    sound.setProperty(SourceIDs::reverse, (int)samplerSound.getChildWithProperty("parameter_name", "reverse").getProperty("parameter_value"), nullptr);
                    sound.setProperty(SourceIDs::noteMappingMode, (int)samplerSound.getChildWithProperty("parameter_name", "noteMappingMode").getProperty("parameter_value"), nullptr);
                    sound.setProperty(SourceIDs::numSlices, (int)samplerSound.getChildWithProperty("parameter_name", "numSlices").getProperty("parameter_value"), nullptr);
                    sound.setProperty(SourceIDs::playheadPosition, (float)samplerSound.getChildWithProperty("parameter_name", "playheadPosition").getProperty("parameter_value"), nullptr);
                    sound.setProperty(SourceIDs::freezePlayheadSpeed, (float)samplerSound.getChildWithProperty("parameter_name", "freezePlayheadSpeed").getProperty("parameter_value"), nullptr);
                    sound.setProperty(SourceIDs::filterCutoff, (float)samplerSound.getChildWithProperty("parameter_name", "filterCutoff").getProperty("parameter_value"), nullptr);
                    sound.setProperty(SourceIDs::filterRessonance, (float)samplerSound.getChildWithProperty("parameter_name", "filterRessonance").getProperty("parameter_value"), nullptr);
                    sound.setProperty(SourceIDs::filterKeyboardTracking, (float)samplerSound.getChildWithProperty("parameter_name", "filterKeyboardTracking").getProperty("parameter_value"), nullptr);
                    sound.setProperty(SourceIDs::filterAttack, (float)samplerSound.getChildWithProperty("parameter_name", "filterADSR.attack").getProperty("parameter_value"), nullptr);
                    sound.setProperty(SourceIDs::filterDecay, (float)samplerSound.getChildWithProperty("parameter_name", "filterADSR.decay").getProperty("parameter_value"), nullptr);
                    sound.setProperty(SourceIDs::filterSustain, (float)samplerSound.getChildWithProperty("parameter_name", "filterADSR.sustain").getProperty("parameter_value"), nullptr);
                    sound.setProperty(SourceIDs::filterRelease, (float)samplerSound.getChildWithProperty("parameter_name", "filterADSR.release").getProperty("parameter_value"), nullptr);
                    sound.setProperty(SourceIDs::filterADSR2CutoffAmt, (float)samplerSound.getChildWithProperty("parameter_name", "filterADSR2CutoffAmt").getProperty("parameter_value"), nullptr);
                    sound.setProperty(SourceIDs::gain, (float)samplerSound.getChildWithProperty("parameter_name", "gain").getProperty("parameter_value"), nullptr);
                    sound.setProperty(SourceIDs::attack, (float)samplerSound.getChildWithProperty("parameter_name", "ampADSR.attack").getProperty("parameter_value"), nullptr);
                    sound.setProperty(SourceIDs::decay, (float)samplerSound.getChildWithProperty("parameter_name", "ampADSR.decay").getProperty("parameter_value"), nullptr);
                    sound.setProperty(SourceIDs::sustain, (float)samplerSound.getChildWithProperty("parameter_name", "ampADSR.sustain").getProperty("parameter_value"), nullptr);
                    sound.setProperty(SourceIDs::release, (float)samplerSound.getChildWithProperty("parameter_name", "ampADSR.release").getProperty("parameter_value"), nullptr);
                    sound.setProperty(SourceIDs::pan, (float)samplerSound.getChildWithProperty("parameter_name", "pan").getProperty("parameter_value"), nullptr);
                    sound.setProperty(SourceIDs::pitch, (float)samplerSound.getChildWithProperty("parameter_name", "pitch").getProperty("parameter_value"), nullptr);
                    sound.setProperty(SourceIDs::pitchBendRangeUp, (float)samplerSound.getChildWithProperty("parameter_name", "pitchBendRangeUp").getProperty("parameter_value"), nullptr);
                    sound.setProperty(SourceIDs::pitchBendRangeDown, (float)samplerSound.getChildWithProperty("parameter_name", "pitchBendRangeDown").getProperty("parameter_value"), nullptr);
                    sound.setProperty(SourceIDs::mod2CutoffAmt, (float)samplerSound.getChildWithProperty("parameter_name", "mod2CutoffAmt").getProperty("parameter_value"), nullptr);
                    sound.setProperty(SourceIDs::mod2GainAmt, (float)samplerSound.getChildWithProperty("parameter_name", "mod2GainAmt").getProperty("parameter_value"), nullptr);
                    sound.setProperty(SourceIDs::mod2PitchAmt, (float)samplerSound.getChildWithProperty("parameter_name", "mod2PitchAmt").getProperty("parameter_value"), nullptr);
                    sound.setProperty(SourceIDs::mod2PlayheadPos, (float)samplerSound.getChildWithProperty("parameter_name", "mod2PlayheadPos").getProperty("parameter_value"), nullptr);
                    sound.setProperty(SourceIDs::vel2CutoffAmt, (float)samplerSound.getChildWithProperty("parameter_name", "vel2CutoffAmt").getProperty("parameter_value"), nullptr);
                    sound.setProperty(SourceIDs::vel2GainAmt, (float)samplerSound.getChildWithProperty("parameter_name", "vel2GainAmt").getProperty("parameter_value"), nullptr);
                    
                    modifiedPresetState.addChild (sound, -1, nullptr);
                }
                presetState = modifiedPresetState;
            }
            newState.addChild (presetState, -1, nullptr);
            loadPresetFromStateInformation(newState);
            return true;
        }
    }
    return false; // No file found
}

void SourceSampler::loadPresetFromIndex(int index)
{
    // Load the preset from the file, this will also clear any existing sounds
    bool loaded = loadPresetFromFile(getPresetFilenameByIndex(index));
    if (loaded){
        // If file was loaded, update the current present index to the new one
        currentPresetIndex = index;
    } else {
        // If no file was loaded (no file found or errors ocurred), create a new empty preset
        DBG("Creating new empty preset");
        juce::ValueTree newState = SourceHelpers::createNewStateFromCurrentSatate(state);
        juce::ValueTree newPresetState = SourceHelpers::createEmptyPresetState();
        newState.addChild(newPresetState, -1, nullptr);
        loadPresetFromStateInformation(newState);
        currentPresetIndex = index;
    }
    saveGlobalPersistentStateToFile(); // Save global settings to file (which inlucdes the latest loaded preset index)
    
    // Trigger action to re-send full state to UI clients
    actionListenerCallback(juce::String(ACTION_GET_STATE) + juce::String(":full"));
}

void SourceSampler::loadPresetFromStateInformation (juce::ValueTree _state)
{
    // If sounds are currently loaded in the state, remove them all
    // This will trigger the deletion of SampleSound and SourceSamplerSound objects
    juce::ValueTree presetState = state.getChildWithName(SourceIDs::PRESET);
    if (presetState.isValid()){
        removeAllSounds();
    }
    
    // Load new state informaiton to the state
    DBG("Loading state...");
    state.copyPropertiesAndChildrenFrom(_state, nullptr);
    
    // Trigger bind state again to re-create sounds and the rest
    bindState();
    
    // Run some more actions needed to sync some parameters which are not automatically loaded from state
    updateReverbParameters();
    sampler.setSamplerVoices(numVoices);
    setGlobalMidiInChannel(globalMidiInChannel);
}

void SourceSampler::saveGlobalPersistentStateToFile()
{
    // This is to save settings that need to persist between sampler runs and that do not
    // change per preset
    
    juce::ValueTree settings = juce::ValueTree(SourceIDs::GLOBAL_SETTINGS);
    settings.setProperty(SourceIDs::globalMidiInChannel, globalMidiInChannel.get(), nullptr);
    settings.setProperty(SourceIDs::midiOutForwardsMidiIn, midiOutForwardsMidiIn.get(), nullptr);
    settings.setProperty(SourceIDs::latestLoadedPresetIndex, currentPresetIndex.get(), nullptr);
    settings.setProperty(SourceIDs::useOriginalFilesPreference, useOriginalFilesPreference.get(), nullptr);
    settings.setProperty(SourceIDs::freesoundOauthAccessToken, freesoundOauthAccessToken.get(), nullptr);
    settings.setProperty(SourceIDs::pluginVersion, juce::String(JucePlugin_VersionString), nullptr);
    
    std::unique_ptr<juce::XmlElement> xml (settings.createXml());
    juce::File location = getGlobalSettingsFilePathFromName();
    if (location.existsAsFile()){
        // If already exists, delete it
        location.deleteFile();
    }
    xml->writeTo(location);
}

void SourceSampler::loadGlobalPersistentStateFromFile()
{
    juce::File location = getGlobalSettingsFilePathFromName();
    if (location.existsAsFile()){
        juce::XmlDocument xmlDocument (location);
        std::unique_ptr<juce::XmlElement> xmlState = xmlDocument.getDocumentElement();
        if (xmlState.get() != nullptr){
            juce::ValueTree settings = juce::ValueTree::fromXml(*xmlState.get());
            if (settings.hasProperty(SourceIDs::globalMidiInChannel)){
                globalMidiInChannel = (int)settings.getProperty(SourceIDs::globalMidiInChannel);
            }
            if (settings.hasProperty(SourceIDs::midiOutForwardsMidiIn)){
                midiOutForwardsMidiIn = (bool)settings.getProperty(SourceIDs::midiOutForwardsMidiIn);
            }
            if (settings.hasProperty(SourceIDs::latestLoadedPresetIndex)){
                latestLoadedPreset = (int)settings.getProperty(SourceIDs::latestLoadedPresetIndex);
            }
            if (settings.hasProperty(SourceIDs::useOriginalFilesPreference)){
                useOriginalFilesPreference = settings.getProperty(SourceIDs::useOriginalFilesPreference).toString();
            }
            if (settings.hasProperty(SourceIDs::freesoundOauthAccessToken)){
                freesoundOauthAccessToken = settings.getProperty(SourceIDs::freesoundOauthAccessToken).toString();
            }
        }
    }
}


juce::File SourceSampler::getPresetFilePath(const juce::String& presetFilename)
{
    return presetFilesLocation.getChildFile(presetFilename).withFileExtension("xml");
}


juce::String SourceSampler::getPresetFilenameFromNameAndIndex(const juce::String& presetName, int index)
{
    return (juce::String)index; // Only use index as filename
}


juce::File SourceSampler::getGlobalSettingsFilePathFromName()
{
    return sourceDataLocation.getChildFile("settings").withFileExtension("xml");
}

juce::ValueTree SourceSampler::collectVolatileStateInformation (){
    juce::ValueTree state = juce::ValueTree(SourceIDs::VOLATILE_STATE);
    state.setProperty(SourceIDs::isQuerying, isQuerying, nullptr);
    state.setProperty(SourceIDs::midiInLastStateReportBlock, midiMessagesPresentInLastStateReport, nullptr);
    midiMessagesPresentInLastStateReport = false;
    state.setProperty(SourceIDs::lastMIDICCNumber, lastReceivedMIDIControllerNumber, nullptr);
    state.setProperty(SourceIDs::lastMIDINoteNumber, lastReceivedMIDINoteNumber, nullptr);
    
    juce::String voiceActivations = "";
    juce::String voiceSoundIdxs = "";
    juce::String voiceSoundPlayPositions = "";
    
    for (int i=0; i<sampler.getNumVoices(); i++){
        SourceSamplerVoice* voice = static_cast<SourceSamplerVoice*> (sampler.getVoice(i));
        if (voice->isVoiceActive()){
            voiceActivations += "1,";
            if (auto* playingSound = voice->getCurrentlyPlayingSourceSamplerSound())
            {
                voiceSoundIdxs += playingSound->getSourceSound()->getFirstLinkedSourceSamplerSound()->getUUID() + ",";
            } else {
                voiceSoundIdxs += "-1,";
            }
            voiceSoundPlayPositions += (juce::String)voice->getPlayingPositionPercentage() + ",";
        } else {
            voiceActivations += "0,";
            voiceSoundIdxs += "-1,";
            voiceSoundPlayPositions += "-1,";
        }
    }
    
    state.setProperty(SourceIDs::voiceActivations, voiceActivations, nullptr);
    state.setProperty(SourceIDs::voiceSoundIdxs, voiceSoundIdxs, nullptr);
    state.setProperty(SourceIDs::voiceSoundPlayPosition, voiceSoundPlayPositions, nullptr);
    
    juce::String audioLevels = "";
    for (int i=0; i<getTotalNumOutputChannels(); i++){
        audioLevels += (juce::String)lms.getRMSLevel(i) + ",";
    }
    state.setProperty(SourceIDs::audioLevels, audioLevels, nullptr);
    return state;
}

juce::String SourceSampler::collectVolatileStateInformationAsString(){
    
    juce::StringArray stateAsStringParts = {};
    
    stateAsStringParts.add(isQuerying ? "1": "0");
    stateAsStringParts.add(midiMessagesPresentInLastStateReport ? "1" : "0");
    midiMessagesPresentInLastStateReport = false;
    stateAsStringParts.add((juce::String)lastReceivedMIDIControllerNumber);
    stateAsStringParts.add((juce::String)lastReceivedMIDINoteNumber);
    
    juce::String voiceActivations = "";
    juce::String voiceSoundIdxs = "";
    juce::String voiceSoundPlayPositions = "";
    
    for (int i=0; i<sampler.getNumVoices(); i++){
        SourceSamplerVoice* voice = static_cast<SourceSamplerVoice*> (sampler.getVoice(i));
        if (voice->isVoiceActive()){
            voiceActivations += "1,";
            if (auto* playingSound = voice->getCurrentlyPlayingSourceSamplerSound())
            {
                voiceSoundIdxs += (juce::String)playingSound->getSourceSound()->getFirstLinkedSourceSamplerSound()->getUUID() + ",";
            } else {
                voiceSoundIdxs += "-1,";
            }
            voiceSoundPlayPositions += (juce::String)voice->getPlayingPositionPercentage() + ",";
        } else {
            voiceActivations += "0,";
            voiceSoundIdxs += "-1,";
            voiceSoundPlayPositions += "-1,";
        }
    }
    
    stateAsStringParts.add(voiceActivations);
    stateAsStringParts.add(voiceSoundIdxs);
    stateAsStringParts.add(voiceSoundPlayPositions);
    
    juce::String audioLevels = "";
    for (int i=0; i<getTotalNumOutputChannels(); i++){
        audioLevels += (juce::String)lms.getRMSLevel(i) + ",";
    }
    
    stateAsStringParts.add(audioLevels);
    
    return stateAsStringParts.joinIntoString(";");
}


void SourceSampler::updateReverbParameters()
{
    juce::Reverb::Parameters reverbParameters;
    reverbParameters.roomSize = reverbRoomSize;
    reverbParameters.damping = reverbDamping;
    reverbParameters.wetLevel = reverbWetLevel;
    reverbParameters.dryLevel = reverbDryLevel;
    reverbParameters.width = reverbWidth;
    reverbParameters.freezeMode = reverbFreezeMode;
    sampler.setReverbParameters(reverbParameters);
}


//==============================================================================

void SourceSampler::actionListenerCallback (const juce::String &message)
{
    juce::String actionName = message.substring(0, message.indexOf(":"));
    juce::String serializedParameters = message.substring(message.indexOf(":") + 1);
    juce::StringArray parameters;
    parameters.addTokens (serializedParameters, (juce::String)SERIALIZATION_SEPARATOR, "");
    
    // Log all actions except for "get state" actions as there are too many :)
    if (actionName != ACTION_GET_STATE){
        DBG("Action message: " << message);
    }
    
    // Global actions -----------------------------------------------------------------------------------
    if (actionName == ACTION_GET_STATE) {
        juce::String stateType = parameters[0];
        if (stateType == "full"){
            juce::OSCMessage message = juce::OSCMessage("/full_state");
            message.addInt32(stateUpdateID);
            message.addString(state.toXmlString(juce::XmlElement::TextFormat().singleLine()));
            #if SYNC_STATE_WITH_OSC
            sendOSCMessage(message);
            #endif
            sendWSMessage(message);
        } else if (stateType == "volatileString"){
            juce::OSCMessage message = juce::OSCMessage("/volatile_state_string");
            message.addString(collectVolatileStateInformationAsString());
            #if SYNC_STATE_WITH_OSC
            sendOSCMessage(message);
            #endif
            sendWSMessage(message);
        } else if (stateType == "volatile"){
            juce::OSCMessage message = juce::OSCMessage("/volatile_state");
            message.addString(collectVolatileStateInformation().toXmlString(juce::XmlElement::TextFormat().singleLine()));
            #if SYNC_STATE_WITH_OSC
            sendOSCMessage(message);
            #endif
            sendWSMessage(message);
        }
    }
    else if (actionName == ACTION_PLAY_SOUND_FILE_FROM_PATH){
        juce::String soundPath = parameters[0];
        if (soundPath == ""){
            stopPreviewingFile();  // Send empty string to stop currently previewing sound
        } else {
            previewFile(soundPath);
        }
    }
    else if (actionName == ACTION_SET_USE_ORIGINAL_FILES_PREFERENCE){
        juce::String preference = parameters[0];
        useOriginalFilesPreference = preference;
        saveGlobalPersistentStateToFile();
    }
    else if (actionName == ACTION_SET_FREESOUND_OAUTH_TOKEN){
        juce::String oauthToken = parameters[0];
        freesoundOauthAccessToken = oauthToken;
        saveGlobalPersistentStateToFile();
    }
    else if (actionName == ACTION_SET_GLOBAL_MIDI_IN_CHANNEL){
        int channel = parameters[0].getIntValue();
        setGlobalMidiInChannel(channel);
    }
    else if (actionName == ACTION_NOTE_ON){
        int note = parameters[0].getIntValue();
        int velocity = parameters[1].getIntValue();
        int channel = parameters[2].getIntValue();
        juce::MidiMessage message = juce::MidiMessage::noteOn(channel > 0 ? channel : (globalMidiInChannel > 0 ? globalMidiInChannel : 1), note, (juce::uint8)velocity);
        midiFromEditor.addEvent(message, 0);
    }
    else if (actionName == ACTION_NOTE_OFF){
        int note = parameters[0].getIntValue();
        int velocity = parameters[1].getIntValue();
        int channel = parameters[2].getIntValue();
        juce::MidiMessage message = juce::MidiMessage::noteOff(channel > 0 ? channel : (globalMidiInChannel > 0 ? globalMidiInChannel : 1), note, (juce::uint8)velocity);
        midiFromEditor.addEvent(message, 0);
    }
    
    // Preset actions -----------------------------------------------------------------------------------
    else if (actionName == ACTION_LOAD_PRESET){
        int index = parameters[0].getIntValue();
        loadPresetFromIndex(index);
    }
    else if (actionName == ACTION_SAVE_PRESET){
        juce::String presetName = parameters[0];
        int index = parameters[1].getIntValue();
        saveCurrentPresetToFile(presetName, index);  // Save to file...
        currentPresetIndex = index; // ...and update current preset index and name in case it was changed
        saveGlobalPersistentStateToFile();  // Save global state to reflect last loaded preset has the right index
    }
    else if (actionName == ACTION_SET_REVERB_PARAMETERS){
        reverbRoomSize = parameters[0].getFloatValue();
        reverbDamping = parameters[1].getFloatValue();
        reverbWetLevel = parameters[2].getFloatValue();
        reverbDryLevel = parameters[3].getFloatValue();
        reverbWidth = parameters[4].getFloatValue();
        reverbFreezeMode = parameters[5].getFloatValue();
        updateReverbParameters();
    }
    else if (actionName == ACTION_SET_POLYPHONY){
        numVoices = parameters[0].getIntValue();
        sampler.setSamplerVoices(numVoices);
    }
    else if (actionName == ACTION_REAPPLY_LAYOUT){
        int newNoteLayout = parameters[0].getIntValue();
        reapplyNoteLayout(newNoteLayout);
    }
    else if (actionName == ACTION_CLEAR_ALL_SOUNDS){
        removeAllSounds();
    }
    else if ((actionName == ACTION_ADD_NEW_SOUNDS_FROM_QUERY) || (actionName == ACTION_REPLACE_EXISTING_SOUNDS_FROM_QUERY)){
        juce::String addReplaceOrReplaceSound = actionName == ACTION_ADD_NEW_SOUNDS_FROM_QUERY ? "add": "replace";
        juce::String query = parameters[0];
        int numSounds = parameters[1].getIntValue();
        float minSoundLength = parameters[2].getFloatValue();
        float maxSoundLength = parameters[3].getFloatValue();
        int noteMappingType = parameters[4].getIntValue();
        noteLayoutType = noteMappingType; // Set noteLayoutType so when sounds are actually downloaded and loaded, the requested mode is used
        
        // Trigger query in a separate thread so that we don't block UI
        queryMakerThread.setQueryParameters(addReplaceOrReplaceSound, query, numSounds, minSoundLength, maxSoundLength);
        queryMakerThread.startThread(0);  // Lowest thread priority
    }
    else if (actionName == ACTION_REPLACE_SOUND_FROM_QUERY){
        juce::String addReplaceOrReplaceSound = parameters[0]; // Will be UUID of the sound to replace
        juce::String query = parameters[1];
        float minSoundLength = parameters[2].getFloatValue();
        float maxSoundLength = parameters[3].getFloatValue();
        int noteMappingType = parameters[4].getIntValue();
        noteLayoutType = noteMappingType; // Set noteLayoutType so when sounds are actually downloaded and loaded, the requested mode is used
        
        // Trigger query in a separate thread so that we don't block UI
        queryMakerThread.setQueryParameters(addReplaceOrReplaceSound, query, 1, minSoundLength, maxSoundLength);
        queryMakerThread.startThread(0);  // Lowest thread priority
    }

    // Sound actions -----------------------------------------------------------------------------------
    else if (actionName == ACTION_ADD_SOUND){
        int soundID = parameters[0].getIntValue();
        juce::String soundName = parameters[1];
        juce::String soundUser = parameters[2];
        juce::String soundLicense = parameters[3];
        juce::String oggDownloadURL = parameters[4];
        juce::String localFilePath = parameters[5];
        juce::String type = parameters[6];
        int sizeBytes = parameters[7].getIntValue();
        juce::String serializedSlices = parameters[8];
        juce::StringArray slices;
        if (serializedSlices != ""){
            slices.addTokens(serializedSlices, ",", "");
        }
        juce::String assignedNotesBigInteger = parameters[9];
        juce::BigInteger midiNotes;
        if (assignedNotesBigInteger != ""){
            midiNotes.parseString(assignedNotesBigInteger, 16);
        }
        int midiRootNote = parameters[10].getIntValue();
        int midiVelocityLayer = SourceDefaults::midiVelocityLayer;
        bool addToExistingSourceSampleSounds = false;

        addOrReplaceSoundFromBasicSoundProperties("", soundID, soundName, soundUser, soundLicense, oggDownloadURL, localFilePath, type, sizeBytes, slices, midiNotes, midiRootNote, midiVelocityLayer, addToExistingSourceSampleSounds);
    }
    else if (actionName == ACTION_REPLACE_SOUND){
        juce::String soundUUID = parameters[0];
        int soundID = parameters[1].getIntValue();
        juce::String soundName = parameters[2];
        juce::String soundUser = parameters[3];
        juce::String soundLicense = parameters[4];
        juce::String oggDownloadURL = parameters[5];
        juce::String localFilePath = parameters[6];
        juce::String type = parameters[7];
        int sizeBytes = parameters[8].getIntValue();
        juce::String serializedSlices = parameters[9];
        juce::StringArray slices;
        if (serializedSlices != ""){
            slices.addTokens(serializedSlices, ",", "");
        }
        juce::String assignedNotesBigInteger = parameters[10];
        juce::BigInteger midiNotes;
        if (assignedNotesBigInteger != ""){
            midiNotes.parseString(assignedNotesBigInteger, 16);
        }
        int midiRootNote = parameters[11].getIntValue();
        int midiVelocityLayer = SourceDefaults::midiVelocityLayer;
        bool addToExistingSourceSampleSounds = false;

        addOrReplaceSoundFromBasicSoundProperties(soundUUID, soundID, soundName, soundUser, soundLicense, oggDownloadURL, localFilePath, type, sizeBytes, slices, midiNotes, midiRootNote, midiVelocityLayer, addToExistingSourceSampleSounds);
    }
    else if (actionName == ACTION_REMOVE_SOUND){
        juce::String soundUUID = parameters[0];
        removeSound(soundUUID);
    }
    else if (actionName == ACTION_SET_SOUND_PARAMETER_FLOAT){
        juce::String soundUUID = parameters[0];  // "" means all sounds
        juce::String parameterName = parameters[1];
        float parameterValue = parameters[2].getFloatValue();
        if (soundUUID != "") {
            // Change one sound
            SourceSound* sound = sounds->getSoundWithUUID(soundUUID);
            if (sound != nullptr){
                sound->setParameterByNameFloat(parameterName, parameterValue, false);
            }
        } else {
            // Change all sounds
            for (auto* sound: sounds->objects){
                sound->setParameterByNameFloat(parameterName, parameterValue, false);
            }
        }
    }
    else if (actionName == ACTION_SET_SOUND_PARAMETER_INT){
        juce::String soundUUID = parameters[0];  // "" means all sounds
        juce::String parameterName = parameters[1];
        int parameterValue = parameters[2].getIntValue();
        if (soundUUID != "") {
            // Change one sound
            SourceSound* sound = sounds->getSoundWithUUID(soundUUID);
            if (sound != nullptr){
                sound->setParameterByNameInt(parameterName, parameterValue);
            }
        } else {
            // Change all sounds
            for (auto* sound: sounds->objects){
                sound->setParameterByNameInt(parameterName, parameterValue);
            }
        }
    }
    else if (actionName == ACTION_SET_SOUND_SLICES){
        juce::String soundUUID = parameters[0];
        juce::String serializedSlices = parameters[1];
        juce::StringArray slices;
        slices.addTokens(serializedSlices, ",", "");
        
        auto* sound = sounds->getSoundWithUUID(soundUUID);
        if (sound != nullptr){
            std::vector<float> sliceOnsetTimes = {};
            for (juce::String sliceOnsetTimeStr: slices){
                sliceOnsetTimes.push_back(sliceOnsetTimeStr.getFloatValue());
            }
            sound->setOnsetTimesSamples(sliceOnsetTimes);
        }
    }
    else if (actionName == ACTION_SET_SOUND_ASSIGNED_NOTES){
        juce::String soundUUID = parameters[0];
        juce::String assignedNotesBigInteger = parameters[1];
        int rootNote = parameters[2].getIntValue();
        auto* sound = sounds->getSoundWithUUID(soundUUID);
        if (sound != nullptr){
            juce::BigInteger midiNotes;
            if (assignedNotesBigInteger != ""){
                midiNotes.parseString(assignedNotesBigInteger, 16);
                sound->setMappedMidiNotes(midiNotes);
            }
            if (rootNote > -1){
                sound->setMidiRootNote(rootNote);  // Note that this sets the midi root note only for the first SamplerSound associated with the sound
            }
        }
    }
    else if (actionName == ACTION_PLAY_SOUND){
        juce::String soundUUID = parameters[0];
        addToMidiBuffer(soundUUID, false);
    }
    else if (actionName == ACTION_STOP_SOUND){
        juce::String soundUUID = parameters[0];
        addToMidiBuffer(soundUUID, true);
    }
    
    // SamplerSound actions -----------------------------------------------------------------------------------
    else if (actionName == ACTION_ADD_SAMPLER_SOUND){
        int soundID = parameters[0].getIntValue();
        juce::String soundName = parameters[1];
        juce::String soundUser = parameters[2];
        juce::String soundLicense = parameters[3];
        juce::String oggDownloadURL = parameters[4];
        juce::String localFilePath = parameters[5];
        juce::String type = parameters[6];
        int sizeBytes = parameters[7].getIntValue();
        juce::String serializedSlices = parameters[8];
        juce::StringArray slices;
        if (serializedSlices != ""){
            slices.addTokens(serializedSlices, ",", "");
        }
        juce::String assignedNotesBigInteger = parameters[9];
        juce::BigInteger midiNotes;
        if (assignedNotesBigInteger != ""){
            midiNotes.parseString(assignedNotesBigInteger, 16);
        }
        int midiRootNote = parameters[10].getIntValue();
        int midiVelocityLayer = parameters[11].getIntValue();
        bool addToExistingSourceSampleSounds = true;

        addOrReplaceSoundFromBasicSoundProperties("", soundID, soundName, soundUser, soundLicense, oggDownloadURL, localFilePath, type, sizeBytes, slices, midiNotes, midiRootNote, midiVelocityLayer, addToExistingSourceSampleSounds);
    }
    else if (actionName == ACTION_REMOVE_SAMPLER_SOUND){
        juce::String soundUUID = parameters[0];
        juce::String samplerSoundUUID = parameters[1];
        removeSamplerSound(soundUUID, samplerSoundUUID);
    }
    else if (actionName == ACTION_SET_SAMPLER_SOUND_PARAMETERS){
        juce::String soundUUID = parameters[0];
        juce::String samplerSoundUUID = parameters[1];
        float startPosition = parameters[2].getFloatValue();
        float endPosition = parameters[3].getFloatValue();
        float loopStartPosition = parameters[4].getFloatValue();
        float loopEndPosition = parameters[5].getFloatValue();
        auto* sound = sounds->getSoundWithUUID(soundUUID);
        if (sound != nullptr){
            auto* sourceSamplerSound = sound->getLinkedSourceSamplerSoundWithUUID(samplerSoundUUID);
            if (sourceSamplerSound != nullptr){
                sourceSamplerSound->setSampleStartEndAndLoopPositions(startPosition, endPosition, loopStartPosition, loopEndPosition);
            }
        }
    }
    else if (actionName == ACTION_SET_SAMPLER_SOUND_MIDI_ROOT_NOTE){
        juce::String soundUUID = parameters[0];
        juce::String samplerSoundUUID = parameters[1];
        int rootNote = parameters[2].getIntValue();
        auto* sound = sounds->getSoundWithUUID(soundUUID);
        if (sound != nullptr){
            auto* sourceSamplerSound = sound->getLinkedSourceSamplerSoundWithUUID(samplerSoundUUID);
            if (sourceSamplerSound != nullptr){
                sourceSamplerSound->setMidiRootNote(rootNote);
            }
        }
    }
    
    // MIDI mapping actions -----------------------------------------------------------------------------------
    else if (actionName == ACTION_ADD_OR_UPDATE_CC_MAPPING){
        juce::String soundUUID = parameters[0];
        juce::String uuid = parameters[1];
        int ccNumber = parameters[2].getIntValue();
        juce::String parameterName = parameters[3];
        float minRange = parameters[4].getFloatValue();
        float maxRange = parameters[5].getFloatValue();
        auto* sound = sounds->getSoundWithUUID(soundUUID);
        if (sound != nullptr){
            sound->addOrEditMidiMapping(uuid, ccNumber, parameterName, minRange, maxRange);
        }
    }
    else if (actionName == ACTION_REMOVE_CC_MAPPING){
        juce::String soundUUID = parameters[0];
        juce::String uuid = parameters[1];
        auto* sound = sounds->getSoundWithUUID(soundUUID);
        if (sound != nullptr){
            sound->removeMidiMapping(uuid);
        }
    }
    
    // Sound external downloads actions -----------------------------------------------------------------------------------
    else if (actionName == ACTION_FINISHED_DOWNLOADING_SOUND){
        // If using external server to download sounds, notify download fininshed through this action
        juce::String soundUUID = parameters[0];
        juce::File targetFileLocation = juce::File(parameters[1]);
        bool taskSucceeded = parameters[2].getIntValue() == 1;
        SourceSound* sound = sounds->getSoundWithUUID(soundUUID);
        if (sound != nullptr){
            sound->downloadFinished(targetFileLocation, taskSucceeded);
        }
    }
    else if (actionName == ACTION_DOWNLOADING_SOUND_PROGRESS){
        // If using external server to download sounds, send progress updates through this action
        juce::String soundUUID = parameters[0];
        juce::File targetFileLocation = juce::File(parameters[1]);
        float downloadedPercentage = parameters[2].getFloatValue();
        SourceSound* sound = sounds->getSoundWithUUID(soundUUID);
        if (sound != nullptr){
            sound->downloadProgressUpdate(targetFileLocation, downloadedPercentage);
        }
    }
}

//==============================================================================

void SourceSampler::setGlobalMidiInChannel(int channel)
{
    if (channel < 0){
        channel = 0;  // Al channels
    } else if (channel > 16){
        channel = 16;
    }
    globalMidiInChannel = channel;
    saveGlobalPersistentStateToFile();
}

void SourceSampler::setMidiThru(bool doMidiTrhu)
{
    midiOutForwardsMidiIn = doMidiTrhu;
    saveGlobalPersistentStateToFile();
}


//==============================================================================

void SourceSampler::makeQueryAndLoadSounds(const juce::String& addReplaceOrReplaceSound, const juce::String& textQuery, int numSounds, float minSoundLength, float maxSoundLength)
{
    /* This function makes a query to Freesound and loads the results in the sammpler. This is only used when running SOURCE as a plugin in Desktop environment (not deployed in ELK
     hardware stack). In ELK stack, the communication with Freesound happens in the UI application (Python) and the UI tells the plugin which sounds to load (and when). In the future
     it might be a good idea to move all communication with Freesound to the plugin itself so that it will be easier that the Desktop plugin and the ELK version offer similar
     functionality and there is less need to duplicate code.
     */
    
    if (isQuerying){
        // If already querying, don't run another query
        DBG("Skip query as already querying...");
        return;
    }
    
    juce::String soundUUIDToReplace = "";
    if ((addReplaceOrReplaceSound != "add") && (addReplaceOrReplaceSound != "replace")){
        // if action is not "add" or "replace", then it is "replace one" and the contents of this variable are the sound UUID to replace
        // in that case, make sure numSounds is limited to 1 as this is the number of sounds we need to replace
        numSounds = 1;
        soundUUIDToReplace = addReplaceOrReplaceSound;
    }
    
    FreesoundClient client(FREESOUND_API_KEY);
    isQuerying = true;
    state.setProperty(SourceIDs::numResultsLastQuery, -1, nullptr);
    juce::String query = "";
    juce::String filter = "";
    int pageSize = 0;
    if (textQuery.contains("id:")){
        // Assume user is looking for a specific sound with id
        filter = textQuery;
        DBG("Querying for sound with " + filter);
        query = "";
        pageSize = 1;
    } else {
        DBG("Querying new sounds for: " + textQuery);
        filter = "duration:[" + (juce::String)minSoundLength + " TO " + (juce::String)maxSoundLength + "]";
        query = textQuery;
        pageSize = 100;
    }
    SoundList list = client.textSearch(query, filter, "score", 0, -1, pageSize, "id,name,username,license,type,filesize,previews,analysis", "rhythm.onset_times", 0);
    int numResults = list.getCount();
    state.setProperty(SourceIDs::numResultsLastQuery, numResults, nullptr);
    isQuerying = false;
    if (numResults > 0){
        
        // Randmomize results and prepare for iteration
        juce::Array<FSSound> soundsFound = list.toArrayOfSounds();
        DBG("Query got " + (juce::String)list.getCount() + " results, " + (juce::String)soundsFound.size() + " in the first page. Will load " + (juce::String)numSounds + " sounds.");
        unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
        std::shuffle(soundsFound.begin(), soundsFound.end(), std::default_random_engine(seed));
        soundsFound.resize(juce::jmin(numSounds, list.getCount()));
        int nSounds = soundsFound.size();
        
        // Move this elsewhere in a helper function (?)
        juce::ValueTree presetState = state.getChildWithName(SourceIDs::PRESET);
        
        if (addReplaceOrReplaceSound == "replace"){
            // If action is to replace all sounds, then we need to first clear all loaded sounds
            removeAllSounds();
        }

        // Load the first nSounds from the found ones
        int nNotesPerSound = 128 / nSounds;
        for (int i=0; i<nSounds; i++){
            
            // Calculate assigned notes
            int midiRootNote = -1;
            juce::BigInteger midiNotes = 0;
            
            if (soundUUIDToReplace == ""){
                // If no sound is being replaced, calculate the mapping depending to the requested mapping type
                if (noteLayoutType == NOTE_MAPPING_TYPE_CONTIGUOUS){
                    // In this case, all the notes mapped to this sound are contiguous in a range which depends on the total number of sounds to load
                    midiRootNote = i * nNotesPerSound + nNotesPerSound / 2;
                    midiNotes.setRange(i * nNotesPerSound, nNotesPerSound, true);
                } else if (noteLayoutType == NOTE_MAPPING_TYPE_INTERLEAVED) {
                    // Notes are mapped to sounds in interleaved fashion so each contiguous note corresponds to a different sound
                    midiRootNote = NOTE_MAPPING_INTERLEAVED_ROOT_NOTE + i;
                    for (int j=midiRootNote; j<128; j=j+nSounds){
                        midiNotes.setBit(j);  // Map notes in upwards direction
                    }
                    for (int j=midiRootNote; j>=0; j=j-nSounds){
                        midiNotes.setBit(j);  // Map notes in downwards direction
                    }
                } else if (noteLayoutType == NOTE_MAPPING_TYPE_ALL) {
                    // Map all notes to the sound
                    midiNotes.setRange(0, 127, true);
                }
            } else {
                // If a specific sound is being replaced, no need to calcualte any mapping here because it will be copied from the
                // sound being replaced in addOrReplaceSoundFromBasicSoundProperties
            }
            
            // Create sound
            // soundUUIDToReplace will be "" when we're adding or replacing all sounds, otherwise it will be the UUID of the sound to replace
            int midiVelocityLayer = SourceDefaults::midiVelocityLayer; // Always use default velocity layer as we're not layering sounds
            bool addToExistingSourceSampleSounds = false;
            addOrReplaceSoundFromBasicSoundProperties(soundUUIDToReplace, soundsFound[i], midiNotes, midiRootNote, midiVelocityLayer, addToExistingSourceSampleSounds);
        }
        
        if (addReplaceOrReplaceSound == "add"){
            // If the operation was adding sounds, recompute mapping layout as otherwise new sounds might overlap with previous
            reapplyNoteLayout(noteLayoutType);
        }
    } else {
        DBG("Query got no results...");
    }
}


void SourceSampler::removeSound(const juce::String& soundUUID)
{
    // Trigger the deletion of the sound by disabling it
    // Once disabled, all playing notes will be stopped and the sound removed a while after that
    const juce::ScopedLock sl (soundDeleteLock);
    auto* sound = sounds->getSoundWithUUID(soundUUID);
    if (sound != nullptr){
        sound->scheduleSoundDeletion();
    }
}

void SourceSampler::removeSamplerSound(const juce::String& soundUUID, const juce::String& samplerSoundUUID)
{
    // Trigger the deletion of the sound by disabling it
    // Once disabled, all playing notes will be stopped and the sound removed a while after that
    const juce::ScopedLock sl (soundDeleteLock);
    auto* sound = sounds->getSoundWithUUID(soundUUID);
    if (sound != nullptr){
        sound->scheduleDeletionOfSourceSamplerSound(samplerSoundUUID);
    }
}


void SourceSampler::removeAllSounds()
{
    // Trigger the deletion of the sounds by disabling them
    // Once disabled, all playing notes will be stopped and the sounds removed a while after that
    const juce::ScopedLock sl (soundDeleteLock);
    for (auto* sound: sounds->objects){
        sound->scheduleSoundDeletion();
    }
}

void SourceSampler::addOrReplaceSoundFromBasicSoundProperties(const juce::String& soundUUID,
                                                                            int soundID,
                                                                            const juce::String& soundName,
                                                                            const juce::String& soundUser,
                                                                            const juce::String& soundLicense,
                                                                            const juce::String& previewURL,
                                                                            const juce::String& localFilePath,
                                                                            const juce::String& format,
                                                                            int sizeBytes,
                                                                            juce::StringArray slices,
                                                                            juce::BigInteger midiNotes,
                                                                            int midiRootNote,
                                                                            int midiVelocityLayer,
                                                                            bool addToExistingSourceSampleSounds)
{
    if (!addToExistingSourceSampleSounds){
        // If not adding this sound as a new sample sound to an existing sound, follow the "normal" process for either adding a new sound or replacing an existing one
        
        int existingSoundIndex = sounds->getIndexOfSoundWithUUID(soundUUID);
        if ((existingSoundIndex > -1) && (midiRootNote ==  -1) && (midiNotes.isZero())){
            // If sound exists for that UUID and midiNotes/midiRootNote are not passed as parameters, use the same parameters from the original sound
            // so assigned notes and root note are kept. Note that if the sound is a multisample sound, we take the first of the root notes
            auto* sound = sounds->getSoundWithUUID(soundUUID);
            midiNotes = sound->getMappedMidiNotes();
            midiRootNote = sound->getMidiNoteFromFirstSourceSamplerSound();
        }
        
        juce::ValueTree sourceSound = SourceHelpers::createSourceSoundAndSourceSamplerSoundFromProperties(soundID, soundName, soundUser, soundLicense, previewURL, localFilePath, format, sizeBytes, slices, midiNotes, midiRootNote, midiVelocityLayer);
        
        juce::ValueTree presetState = state.getChildWithName(SourceIDs::PRESET);
        if (existingSoundIndex > -1){
            // If replacing an existing sound, copy as well the midi mappings
            juce::ValueTree soundToReplaceState = presetState.getChildWithProperty(SourceIDs::uuid, soundUUID);
            for (int i=0; i<soundToReplaceState.getNumChildren(); i++){
                auto child = soundToReplaceState.getChild(i);
                if (child.hasType(SourceIDs::MIDI_CC_MAPPING)){
                    juce::ValueTree copyChild = child.createCopy();
                    sourceSound.addChild(copyChild, -1, nullptr);
                }
            }
            // Now remove the sound with that UUID and replace it by the new sound
            presetState.removeChild(existingSoundIndex, nullptr);
            presetState.addChild(sourceSound, existingSoundIndex, nullptr);
        } else {
            // If no sound exists with the given UUID, add the sound as a NEW one at the end of the list
            presetState.addChild(sourceSound, -1, nullptr);
        }
    } else {
        // If adding the sound as a new sound sample of an existing sound, get the existing sound, add a new sound sample to it's state and re-trigger loading of
        // the sound samples
        // Note that when adding new sample sound to an existing sound,the midiNotes argument is ignored as this affects the main sound only
        auto* sound = sounds->getSoundWithUUID(soundUUID);
        if (sound != nullptr){
            juce::ValueTree sourceSamplerSound = SourceHelpers::createSourceSampleSoundState(soundID, soundName, soundUser, soundLicense, previewURL, localFilePath, format, sizeBytes, slices, midiRootNote, midiVelocityLayer);
            sound->addNewSourceSamplerSoundFromValueTree(sourceSamplerSound);
        }
    }
}

void SourceSampler::addOrReplaceSoundFromBasicSoundProperties(const juce::String& soundUUID,
                                                                            FSSound sound,
                                                                            juce::BigInteger midiNotes,
                                                                            int midiRootNote,
                                                                            int midiVelocityLayer,
                                                                            bool addToExistingSourceSampleSounds)
{
    // Prepare slices
    juce::StringArray slices;
    if (sound.analysis.hasProperty("rhythm")){
        if (sound.analysis["rhythm"].hasProperty("onset_times")){
            for (int j=0; j<sound.analysis["rhythm"]["onset_times"].size(); j++){
                slices.add(sound.analysis["rhythm"]["onset_times"][j].toString());
            }
        }
    }
    
    // Create sound
    addOrReplaceSoundFromBasicSoundProperties(soundUUID, sound.id.getIntValue(), sound.name, sound.user, sound.license, sound.getOGGPreviewURL().toString(false), "", sound.format, sound.filesize, slices, midiNotes, midiRootNote, midiVelocityLayer, addToExistingSourceSampleSounds);
}

void SourceSampler::reapplyNoteLayout(int newNoteLayoutType)
{
    noteLayoutType = newNoteLayoutType;
    int nSounds =  sounds->objects.size();
    if (nSounds > 0){
        int nNotesPerSound = 128 / nSounds;  // Only used if we need to generate midiRootNote and midiNotes
        for (int i=0; i<sounds->objects.size(); i++){
            auto* sound = sounds->getSoundAt(i);
            if (sound != nullptr){
                if (noteLayoutType == NOTE_MAPPING_TYPE_CONTIGUOUS){
                    // Set midi root note to the center of the assigned range
                    int rootNote = i * nNotesPerSound + nNotesPerSound / 2;
                    juce::BigInteger midiNotes;
                    midiNotes.setRange(i * nNotesPerSound, nNotesPerSound, true);
                    sound->setMappedMidiNotes(midiNotes);
                    sound->setMidiRootNote(rootNote);
                } else if (noteLayoutType == NOTE_MAPPING_TYPE_INTERLEAVED){
                    int rootNote = NOTE_MAPPING_INTERLEAVED_ROOT_NOTE + i;
                    juce::BigInteger midiNotes;
                    for (int j=rootNote; j<128; j=j+nSounds){
                        midiNotes.setBit(j);  // Map notes in upwards direction
                    }
                    for (int j=rootNote; j>=0; j=j-nSounds){
                        midiNotes.setBit(j);  // Map notes in downwards direction
                    }
                    sound->setMappedMidiNotes(midiNotes);
                    sound->setMidiRootNote(rootNote);
                } else if (noteLayoutType == NOTE_MAPPING_TYPE_ALL) {
                    juce::BigInteger midiNotes;
                    midiNotes.setRange(0, 127, true);
                    sound->setMappedMidiNotes(midiNotes);
                    sound->setMidiRootNote(64);
                }
            }
        }
    }
}

void SourceSampler::addToMidiBuffer(const juce::String& soundUUID, bool doNoteOff)
{
    auto* sound = sounds->getSoundWithUUID(soundUUID);
    if (sound != nullptr){
        int midiNoteForNormalPitch = 36;
        midiNoteForNormalPitch = sound->getFirstLinkedSourceSamplerSound()->getMidiRootNote();
        juce::BigInteger assignedMidiNotes = sound->getMappedMidiNotes();
        if (assignedMidiNotes[midiNoteForNormalPitch] == false){
            // If the root note is not mapped to the sound, find the closest mapped one
            midiNoteForNormalPitch = assignedMidiNotes.findNextSetBit(midiNoteForNormalPitch);
        }
        if (midiNoteForNormalPitch < 0){
            // If for some reason no note was found, don't play anything
        } else {
            int midiChannel = sound->getParameterInt(SourceIDs::midiChannel);
            if (midiChannel == 0){
                if (globalMidiInChannel > 0){
                    midiChannel = globalMidiInChannel;
                } else {
                    midiChannel = 1;
                }
            }
            juce::MidiMessage message = juce::MidiMessage::noteOn(midiChannel, midiNoteForNormalPitch, (juce::uint8)127);
            if (doNoteOff){
                message = juce::MidiMessage::noteOff(midiChannel, midiNoteForNormalPitch, (juce::uint8)127);
            }
            midiFromEditor.addEvent(message, 0);
        }
    }
}

double SourceSampler::getStartTime(){
    return startTime;
}

void SourceSampler::timerCallback()
{
    const juce::ScopedLock sl (soundDeleteLock);
    
    // Delete sounds that should be deleted (delete them both from current sounds list and from
    // soundsOld copy as ther might still be sounds scheduled for deletion there)
    if (sounds != nullptr){
        for (int i=sounds->objects.size() - 1; i>=0 ; i--){
            auto* sound = sounds->objects[i];
            if (sound->shouldBeDeleted()){
                sounds->removeSoundWithUUID(sound->getUUID());
            }
        }
    }
    if (soundsOld != nullptr){
        for (int i=soundsOld->objects.size() - 1; i>=0 ; i--){
            auto* sound = soundsOld->objects[i];
            if (sound->shouldBeDeleted()){
                soundsOld->removeSoundWithUUID(sound->getUUID());
            }
        }
    }
    
    // Also, delete SourceSampleSounds that are schedule for deletion (and have not been deleted yet)
    for (int i=sampler.getNumSounds() - 1; i>=0; i--){
        auto* sourceSamplerSound = static_cast<SourceSamplerSound*>(sampler.getSound(i).get());
        if (sourceSamplerSound->shouldBeDeleted()){
            sourceSamplerSound->getSourceSound()->removeSourceSamplerSound(sourceSamplerSound->getUUID(), i);
        }
    }
    
    #if SYNC_STATE_WITH_OSC
    // If syncing the state wia OSC, we send "/plugin_alive" messages as these are used to determine
    // if the plugin is up and running
    if ((juce::Time::getMillisecondCounterHiRes() - lastTimeIsAliveWasSent) > 1000.0){
        // Every second send "alive" message
        juce::OSCMessage message = juce::OSCMessage("/plugin_alive");
        sendOSCMessage(message);
        lastTimeIsAliveWasSent = juce::Time::getMillisecondCounterHiRes();
    }
    #endif
}


int SourceSampler::getServerInterfaceHttpPort()
{
    #if USE_HTTP_SERVER
    return serverInterface.httpServer.assignedPort;
    #else
    return 0;
    #endif
}

int SourceSampler::getServerInterfaceWSPort()
{
    return serverInterface.wsServer.assignedPort;
}


void SourceSampler::previewFile(const juce::String& path)
{
    if (transportSource.isPlaying()){
        transportSource.stop();
    }
    if (currentlyLoadedPreviewFilePath != path){
        // Load new file
        juce::String pathToLoad;
        if (path.startsWith("http")){
            // If path is an URL, download the file first
            juce::StringArray tokens;
            tokens.addTokens (path, "/", "");
            juce::String filename = tokens[tokens.size() - 1];
            
            juce::File location = tmpFilesLocation.getChildFile(filename);
            if (!location.exists()){  // Dont' re-download if file already exists
                # if ELK_BUILD
                // If on ELK build don't download here because it seems to hang forever
                // On ELK, we should trigger the downloads from the Python script before calling this function
                # else
                std::unique_ptr<juce::URL::DownloadTask> downloadTask = juce::URL(path).downloadToFile(location, "");
                while (!downloadTask->isFinished()){
                    // Wait until it finished downloading
                }
                # endif
            }
            pathToLoad = location.getFullPathName();  // Update path variable to the download location
        } else {
            pathToLoad = path;
        }
        juce::File file = juce::File(pathToLoad);
        if (file.existsAsFile()){
            
            auto* reader = audioFormatManager.createReaderFor (file);
            if (reader != nullptr)
            {
                std::unique_ptr<juce::AudioFormatReaderSource> newSource (new juce::AudioFormatReaderSource (reader, true));
                transportSource.setSource (newSource.get(), 0, nullptr, reader->sampleRate);
                readerSource.reset (newSource.release());
            }
        }
        
    }
    transportSource.start();
}

void SourceSampler::stopPreviewingFile(){
    if (transportSource.isPlaying()){
        transportSource.stop();
    }
}


//==============================================================================

void SourceSampler::sendOSCMessage(const juce::OSCMessage& message)
{
    if (!oscSenderIsConnected){
        if (oscSender.connect ("127.0.0.1", OSC_TO_SEND_PORT)){
            oscSenderIsConnected = true;
        }
    }
    if (oscSenderIsConnected){
        oscSender.send (message);
    }
}

void SourceSampler::sendWSMessage(const juce::OSCMessage& message)
{
    // Send message to all connected WS clients (if WS server is up and running)
    if (serverInterface.wsServer.serverPtr != nullptr){
        serverInterface.sendMessageToWebSocketClients(message);
    }
    
    #if USING_DIRECT_COMMUNICATION_METHOD
    // Send message to the browser-based UI loaed in the WebComponenet of the PluginEditor
    // NOTE: for some reason calling this crashes the app sometimes. need to be further investigated...
    sendActionMessage(serverInterface.serliaizeOSCMessage(message));
    #endif
}


//==============================================================================

void SourceSampler::valueTreePropertyChanged (juce::ValueTree& treeWhosePropertyHasChanged, const juce::Identifier& property)
{
    // We should never call this function from the realtime thread because editing VT might not be RT safe...
    // TODO: proper check that this is not audio thread
    //jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
    DBG("Changed " << treeWhosePropertyHasChanged[SourceIDs::name].toString() << " " << property.toString() << ": " << treeWhosePropertyHasChanged[property].toString());
    juce::OSCMessage message = juce::OSCMessage("/state_update");
    message.addString("propertyChanged");
    message.addInt32(stateUpdateID);
    message.addString(treeWhosePropertyHasChanged[SourceIDs::uuid].toString());
    message.addString(treeWhosePropertyHasChanged.getType().toString());
    message.addString(property.toString());
    message.addString(treeWhosePropertyHasChanged[property].toString());
    #if SYNC_STATE_WITH_OSC
    sendOSCMessage(message);
    #endif
    sendWSMessage(message);
    stateUpdateID += 1;
}

void SourceSampler::valueTreeChildAdded (juce::ValueTree& parentTree, juce::ValueTree& childWhichHasBeenAdded)
{
    // We should never call this function from the realtime thread because editing VT might not be RT safe...
    // TODO: proper check that this is not audio thread
    //jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
    DBG("Added VT child " << childWhichHasBeenAdded.getType());
    juce::OSCMessage message = juce::OSCMessage("/state_update");
    message.addString("addedChild");
    message.addInt32(stateUpdateID);
    message.addString(parentTree[SourceIDs::uuid].toString());
    message.addString(parentTree.getType().toString());
    message.addInt32(parentTree.indexOf(childWhichHasBeenAdded));
    // NOTE: for the "direct communication method" to work, we need to make a copy of childWhichHasBeenAdded otherwise the app can crash (not sure why...)
    message.addString(childWhichHasBeenAdded.createCopy().toXmlString(juce::XmlElement::TextFormat().singleLine()));
    #if SYNC_STATE_WITH_OSC
    sendOSCMessage(message);
    #endif
    sendWSMessage(message);
    stateUpdateID += 1;
}

void SourceSampler::valueTreeChildRemoved (juce::ValueTree& parentTree, juce::ValueTree& childWhichHasBeenRemoved, int indexFromWhichChildWasRemoved)
{
    // We should never call this function from the realtime thread because editing VT might not be RT safe...
    // TODO: proper check that this is not audio thread
    //jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
    DBG("Removed VT child " << childWhichHasBeenRemoved.getType());
    juce::OSCMessage message = juce::OSCMessage("/state_update");
    message.addString("removedChild");
    message.addInt32(stateUpdateID);
    message.addString(childWhichHasBeenRemoved[SourceIDs::uuid].toString());
    message.addString(childWhichHasBeenRemoved.getType().toString());
    #if SYNC_STATE_WITH_OSC
    sendOSCMessage(message);
    #endif
    sendWSMessage(message);
    stateUpdateID += 1;
}

void SourceSampler::valueTreeChildOrderChanged (juce::ValueTree& parentTree, int oldIndex, int newIndex)
{
    // We should never call this function from the realtime thread because editing VT might not be RT safe...
    // jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
}

void SourceSampler::valueTreeParentChanged (juce::ValueTree& treeWhoseParentHasChanged)
{
    // We should never call this function from the realtime thread because editing VT might not be RT safe...
    // jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
}
