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
#include <algorithm> // for using shuffle


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
    queryMakerThread (*this)
#endif
{
    
    #if ELK_BUILD
    sourceDataLocation = File(ELK_SOURCE_DATA_BASE_LOCATION);
    soundsDownloadLocation = File(ELK_SOURCE_SOUNDS_LOCATION);
    presetFilesLocation = File(ELK_SOURCE_PRESETS_LOCATION);
    tmpFilesLocation = File(ELK_SOURCE_TMP_LOCATION);
    #else
    sourceDataLocation = File::getSpecialLocation(File::userDocumentsDirectory).getChildFile("SourceSampler/");
    soundsDownloadLocation = File::getSpecialLocation(File::userDocumentsDirectory).getChildFile("SourceSampler/sounds");
    presetFilesLocation = File::getSpecialLocation(File::userDocumentsDirectory).getChildFile("SourceSampler/presets");
    tmpFilesLocation = File::getSpecialLocation(File::userDocumentsDirectory).getChildFile("SourceSampler/tmp");
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
    
    if(audioFormatManager.getNumKnownFormats() == 0){ audioFormatManager.registerBasicFormats(); }
    
    startTime = Time::getMillisecondCounterHiRes() * 0.001;
    
    // Action listeners
    serverInterface.addActionListener(this);
    sampler.addActionListener(this);
    
    // Start timer to pass state to the UI and run other periodic tasks like deleting sounds that are waiting to be deleted
    startTimerHz(MAIN_TIMER_HZ);
    
    // Load empty session to state
    state = Helpers::createDefaultEmptyState();
    
    // Add state change listener and bind cached properties to state properties (including loaded sounds)
    bindState();
    
    // Load global settings and do extra configuration
    loadGlobalPersistentStateFromFile();
    
    // If on ELK build, start loading preset 0
    #if ELK_BUILD
    setCurrentProgram(latestLoadedPreset);
    #endif
    
    // Notify that plugin is running
    #if SYNC_STATE_WITH_OSC
    sendOSCMessage(juce::OSCMessage("/plugin_started"));
    #endif
}

SourceSamplerAudioProcessor::~SourceSamplerAudioProcessor()
{
    // Save current global persistent state (global settings)
    saveGlobalPersistentStateToFile();
    
    // Remove listeners
    serverInterface.removeActionListener(this);
    sampler.removeActionListener(this);
    
    // Clean data in tmp directory
    tmpFilesLocation.deleteRecursively();
}

void SourceSamplerAudioProcessor::bindState()
{
    state.addListener(this);
    
    state.setProperty(IDs::sourceDataLocation, sourceDataLocation.getFullPathName(), nullptr);
    state.setProperty(IDs::soundsDownloadLocation, soundsDownloadLocation.getFullPathName(), nullptr);
    state.setProperty(IDs::presetFilesLocation, presetFilesLocation.getFullPathName(), nullptr);
    state.setProperty(IDs::tmpFilesLocation, tmpFilesLocation.getFullPathName(), nullptr);
    state.setProperty(IDs::pluginVersion, String(JucePlugin_VersionString), nullptr);    
    
    currentPresetIndex.referTo(state, IDs::currentPresetIndex, nullptr, Defaults::currentPresetIndex);
    globalMidiInChannel.referTo(state, IDs::globalMidiInChannel, nullptr, Defaults::globalMidiInChannel);
    midiOutForwardsMidiIn.referTo(state, IDs::midiOutForwardsMidiIn, nullptr, Defaults::midiOutForwardsMidiIn);
    useOriginalFilesPreference.referTo(state, IDs::useOriginalFilesPreference, nullptr, Defaults::useOriginalFilesPreference);
    
    ValueTree preset = state.getChildWithName(IDs::PRESET);
    numVoices.referTo(preset, IDs::numVoices, nullptr, Defaults::numVoices);
    presetName.referTo(preset, IDs::name, nullptr);
    noteLayoutType.referTo(preset, IDs::noteLayoutType, nullptr, Defaults::noteLayoutType);
    reverbRoomSize.referTo(preset, IDs::reverbRoomSize, nullptr, Defaults::reverbRoomSize);
    reverbDamping.referTo(preset, IDs::reverbDamping, nullptr, Defaults::reverbDamping);
    reverbWetLevel.referTo(preset, IDs::reverbWetLevel, nullptr, Defaults::reverbWetLevel);
    reverbDryLevel.referTo(preset, IDs::reverbDryLevel, nullptr, Defaults::reverbDryLevel);
    reverbWidth.referTo(preset, IDs::reverbWidth, nullptr, Defaults::reverbWidth);
    reverbFreezeMode.referTo(preset, IDs::reverbFreezeMode, nullptr, Defaults::reverbFreezeMode);
    
    sounds = std::make_unique<SourceSoundList>(state.getChildWithName(IDs::PRESET), [this]{return getGlobalContext();});
}

GlobalContextStruct SourceSamplerAudioProcessor::getGlobalContext()
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
    context.freesoundOauthAccessToken = ""; // TODO: implement support for obtaining an access token and enable downloading original quality files
    context.midiInChannel = globalMidiInChannel.get();
    return context;
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
   return 128;
}

int SourceSamplerAudioProcessor::getCurrentProgram()
{
    return currentPresetIndex;
}

void SourceSamplerAudioProcessor::setCurrentProgram (int index)
{
    // Load the preset from the file, this will also clear any existing sounds
    bool loaded = loadPresetFromFile(getPresetFilenameByIndex(index));
    if (loaded){
        currentPresetIndex = index;
    } else {
        // No preset exists at that number, create a new empty one
        currentPresetIndex = index;
        juce::ValueTree newState = state.createCopy();
        juce::ValueTree presetState = newState.getChildWithName(IDs::PRESET);
        if (presetState.isValid()){
            newState.removeChild(presetState, nullptr);
        }
        juce::ValueTree newPresetState = Helpers::createEmptyPresetState();
        newState.addChild(newPresetState, -1, nullptr);
        loadPresetFromStateInformation(newState);
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
            if (state.hasProperty(IDs::name)){
                return state.getProperty(IDs::name).toString();
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
            state.setProperty(IDs::name, newName, nullptr);
            std::unique_ptr<XmlElement> updatedXmlState (state.createXml());
            String filename = getPresetFilenameFromNameAndIndex(newName, index);
            File location = getPresetFilePath(filename);
            if (location.existsAsFile()){
                // If already exists, delete it
                location.deleteFile();
            }
            updatedXmlState->writeTo(location);
        }
    }
}

String SourceSamplerAudioProcessor::getPresetFilenameByIndex(int index)
{
    return (String)index + ".xml";
}

//==============================================================================
void SourceSamplerAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    logToState("Called prepareToPlay with sampleRate " + (String)sampleRate + " and block size " + (String)samplesPerBlock);
    
    // Prepare sampler
    sampler.prepare ({ sampleRate, (juce::uint32) samplesPerBlock, 2 });
    
    // Prepare preview player
    transportSource.prepareToPlay (samplesPerBlock, sampleRate);
    
    // Configure level measurer
    lms.resize(getTotalNumOutputChannels(), 200 * 0.001f * sampleRate / samplesPerBlock); // 200ms average window
}

void SourceSamplerAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
    transportSource.releaseResources();
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
    transportSource.getNextAudioBlock(AudioSourceChannelInfo(buffer));
    
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
bool SourceSamplerAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

AudioProcessorEditor* SourceSamplerAudioProcessor::createEditor()
{
    return new SourceSamplerAudioProcessorEditor (*this);
}

//==============================================================================

void SourceSamplerAudioProcessor::saveCurrentPresetToFile (const String& _presetName, int index)
{
    juce::ValueTree presetState = state.getChildWithName(IDs::PRESET);
    if (presetState.isValid()){
        
        if (_presetName == ""){
            // No name provided, generate unique name
            presetName = "unnamed";
            for (int i=0; i<8; i++){
                presetName = presetName + (String)juce::Random::getSystemRandom().nextInt (9);
            }
        } else {
            presetName = _presetName;
        }
        
        std::unique_ptr<XmlElement> xml (presetState.createXml());
        String filename = getPresetFilenameFromNameAndIndex(presetName, index);
        File location = getPresetFilePath(filename);
        if (location.existsAsFile()){
            // If already exists, delete it
            location.deleteFile();
        }
        logToState("Saving preset to: " + location.getFullPathName());
        xml->writeTo(location);
    }
}

bool SourceSamplerAudioProcessor::loadPresetFromFile (const juce::String& fileName)
{
    juce::File location = getPresetFilePath(fileName);
    if (location.existsAsFile()){
        juce::XmlDocument xmlDocument (location);
        std::unique_ptr<juce::XmlElement> xmlState = xmlDocument.getDocumentElement();
        if (xmlState.get() != nullptr){
            juce::ValueTree newState = Helpers::createEmptyState();
            juce::ValueTree presetState = juce::ValueTree::fromXml(*xmlState.get());
            newState.addChild (presetState, -1, nullptr);
            loadPresetFromStateInformation(newState);
            return true;
        }
    }
    return false; // No file found
}

void SourceSamplerAudioProcessor::loadPresetFromStateInformation (ValueTree _state)
{
    // If sounds are currently loaded in the state, remove them all
    // This will trigger the deletion of SampleSound and SourceSamplerSound objects
    juce::ValueTree presetState = state.getChildWithName(IDs::PRESET);
    if (presetState.isValid()){
        removeAllSounds();
    }
    
    // Load new state informaiton to the state
    logToState("Loading state...");
    state.copyPropertiesAndChildrenFrom(_state, nullptr);
    
    // Trigger bind state again to re-create sounds and the rest
    bindState();
    
    // Run some more actions needed to sync some parameters which are not automatically loaded from state
    updateReverbParameters();
    sampler.setSamplerVoices(numVoices);
    setMidiInChannelFilter(globalMidiInChannel);
}

void SourceSamplerAudioProcessor::getStateInformation (MemoryBlock& destData)
{
    // Save current state information to memory block
    std::unique_ptr<XmlElement> xml (state.createXml());
    DBG("> Running getStateInformation");
    DBG(xml->toString()); // Print state for debugging purposes
    copyXmlToBinary (*xml, destData);
}

void SourceSamplerAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    DBG("> Running setStateInformation");
    std::unique_ptr<XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr){
        loadPresetFromStateInformation(ValueTree::fromXml(*xmlState.get()));
    }
    
}

void SourceSamplerAudioProcessor::saveGlobalPersistentStateToFile()
{
    // This is to save settings that need to persist between sampler runs and that do not
    // change per preset
    
    ValueTree settings = ValueTree(IDs::GLOBAL_SETTINGS);
    settings.setProperty(IDs::globalMidiInChannel, globalMidiInChannel.get(), nullptr);
    settings.setProperty(IDs::midiOutForwardsMidiIn, midiOutForwardsMidiIn.get(), nullptr);
    settings.setProperty(IDs::latestLoadedPresetIndex, currentPresetIndex.get(), nullptr);
    settings.setProperty(IDs::useOriginalFilesPreference, useOriginalFilesPreference.get(), nullptr);
    settings.setProperty(IDs::pluginVersion, String(JucePlugin_VersionString), nullptr);
    
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
            if (settings.hasProperty(IDs::globalMidiInChannel)){
                globalMidiInChannel = (int)settings.getProperty(IDs::globalMidiInChannel);
            }
            if (settings.hasProperty(IDs::midiOutForwardsMidiIn)){
                midiOutForwardsMidiIn = (bool)settings.getProperty(IDs::midiOutForwardsMidiIn);
            }
            if (settings.hasProperty(IDs::latestLoadedPresetIndex)){
                latestLoadedPreset = (int)settings.getProperty(IDs::latestLoadedPresetIndex);
            }
            if (settings.hasProperty(IDs::useOriginalFilesPreference)){
                useOriginalFilesPreference = settings.getProperty(IDs::useOriginalFilesPreference).toString();
            }
        }
    }
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
    ValueTree state = ValueTree(IDs::VOLATILE_STATE);
    state.setProperty(IDs::isQuerying, isQuerying, nullptr);
    state.setProperty(IDs::midiInLastStateReportBlock, midiMessagesPresentInLastStateReport, nullptr);
    midiMessagesPresentInLastStateReport = false;
    state.setProperty(IDs::lastMIDICCNumber, lastReceivedMIDIControllerNumber, nullptr);
    state.setProperty(IDs::lastMIDINoteNumber, lastReceivedMIDINoteNumber, nullptr);
    
    String voiceActivations = "";
    String voiceSoundIdxs = "";
    String voiceSoundPlayPositions = "";
    
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
            voiceSoundPlayPositions += (String)voice->getPlayingPositionPercentage() + ",";
        } else {
            voiceActivations += "0,";
            voiceSoundIdxs += "-1,";
            voiceSoundPlayPositions += "-1,";
        }
    }
    
    state.setProperty(IDs::voiceActivations, voiceActivations, nullptr);
    state.setProperty(IDs::voiceSoundIdxs, voiceSoundIdxs, nullptr);
    state.setProperty(IDs::voiceSoundPlayPosition, voiceSoundPlayPositions, nullptr);
    
    String audioLevels = "";
    for (int i=0; i<getTotalNumOutputChannels(); i++){
        audioLevels += (String)lms.getRMSLevel(i) + ",";
    }
    state.setProperty(IDs::audioLevels, audioLevels, nullptr);
    return state;
}

String SourceSamplerAudioProcessor::collectVolatileStateInformationAsString(){
    
    StringArray stateAsStringParts = {};
    
    stateAsStringParts.add(isQuerying ? "1": "0");
    stateAsStringParts.add(midiMessagesPresentInLastStateReport ? "1" : "0");
    midiMessagesPresentInLastStateReport = false;
    stateAsStringParts.add((String)lastReceivedMIDIControllerNumber);
    stateAsStringParts.add((String)lastReceivedMIDINoteNumber);
    
    String voiceActivations = "";
    String voiceSoundIdxs = "";
    String voiceSoundPlayPositions = "";
    
    for (int i=0; i<sampler.getNumVoices(); i++){
        SourceSamplerVoice* voice = static_cast<SourceSamplerVoice*> (sampler.getVoice(i));
        if (voice->isVoiceActive()){
            voiceActivations += "1,";
            if (auto* playingSound = voice->getCurrentlyPlayingSourceSamplerSound())
            {
                voiceSoundIdxs += (String)playingSound->getSourceSound()->getFirstLinkedSourceSamplerSound()->getUUID() + ",";
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
    
    stateAsStringParts.add(voiceActivations);
    stateAsStringParts.add(voiceSoundIdxs);
    stateAsStringParts.add(voiceSoundPlayPositions);
    
    String audioLevels = "";
    for (int i=0; i<getTotalNumOutputChannels(); i++){
        audioLevels += (String)lms.getRMSLevel(i) + ",";
    }
    
    stateAsStringParts.add(audioLevels);
    
    return stateAsStringParts.joinIntoString(";");
}

void SourceSamplerAudioProcessor::sendStateToExternalServer(ValueTree state, String stringData)
{
    // This is only used in ELK builds in which the HTTP server is running outside the plugin
    URL url = URL("http://localhost:8123/state_from_plugin");
    String header = "Content-Type: text/xml";
    int statusCode = -1;
    StringPairArray responseHeaders;
    String data = stringData;
    if (state.isValid()){
        data = state.toXmlString();
    }
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

void SourceSamplerAudioProcessor::updateReverbParameters()
{
    Reverb::Parameters reverbParameters;
    reverbParameters.roomSize = reverbRoomSize;
    reverbParameters.damping = reverbDamping;
    reverbParameters.wetLevel = reverbWetLevel;
    reverbParameters.dryLevel = reverbDryLevel;
    reverbParameters.width = reverbWidth;
    reverbParameters.freezeMode = reverbFreezeMode;
    sampler.setReverbParameters(reverbParameters);
}


//==============================================================================

void SourceSamplerAudioProcessor::actionListenerCallback (const String &message)
{
    String actionName = message.substring(0, message.indexOf(":"));
    String serializedParameters = message.substring(message.indexOf(":") + 1);
    StringArray parameters;
    parameters.addTokens (serializedParameters, (String)SERIALIZATION_SEPARATOR, "");
    
    if (actionName != ACTION_GET_STATE){
        // Don't log get state actions as it creates too much logs
        DBG("Action message: " << message);
    }
    
    if (actionName == ACTION_FINISHED_DOWNLOADING_SOUND){
        // If using external server to download sounds, notify download fininshed through this action
        juce::String soundUUID = parameters[0];
        juce::File targetFileLocation = juce::File(parameters[1]);
        bool taskSucceeded = parameters[2].getIntValue() == 1;
        SourceSound* sound = sounds->getSoundWithUUID(soundUUID);
        if (sound != nullptr){
            sound->downloadFinished(targetFileLocation, taskSucceeded);
        }
        
    } else if (actionName == ACTION_DOWNLOADING_SOUND_PROGRESS){
        // If using external server to download sounds, send progress updates through this action
        juce::String soundUUID = parameters[0];
        juce::File targetFileLocation = juce::File(parameters[1]);
        float downloadedPercentage = parameters[2].getFloatValue();
        SourceSound* sound = sounds->getSoundWithUUID(soundUUID);
        if (sound != nullptr){
            sound->downloadProgressUpdate(targetFileLocation, downloadedPercentage);
        }
        
    } else if (actionName == ACTION_NEW_QUERY){
        String query = parameters[0];
        int numSounds = parameters[1].getIntValue();
        float minSoundLength = parameters[2].getFloatValue();
        float maxSoundLength = parameters[3].getFloatValue();
        int noteMappingType = parameters[4].getFloatValue();
        noteLayoutType = noteMappingType; // Set noteLayoutType so when sounds are actually downloaded and loaded, the requested mode is used
        
        // Trigger query in a separate thread so that we don't block UI
        queryMakerThread.setQueryParameters(query, numSounds, minSoundLength, maxSoundLength);
        queryMakerThread.startThread(0);  // Lowest thread priority
        
    } else if (actionName == ACTION_SET_SOUND_PARAMETER_FLOAT){
        juce::String soundUUID = parameters[0];  // "" means all sounds
        juce::String parameterName = parameters[1];
        float parameterValue = parameters[2].getFloatValue();
        DBG("Setting FLOAT parameter " << parameterName << " of sound " << soundUUID << " to value " << parameterValue);
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
    } else if (actionName == ACTION_SET_SOUND_PARAMETER_INT){
        juce::String soundUUID = parameters[0];  // "" means all sounds
        juce::String parameterName = parameters[1];
        int parameterValue = parameters[2].getIntValue();
        DBG("Setting INT parameter " << parameterName << " of sound " << soundUUID << " to value " << parameterValue);
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
    } else if (actionName == ACTION_SET_REVERB_PARAMETERS){
        reverbRoomSize = parameters[0].getFloatValue();
        reverbDamping = parameters[1].getFloatValue();
        reverbWetLevel = parameters[2].getFloatValue();
        reverbDryLevel = parameters[3].getFloatValue();
        reverbWidth = parameters[4].getFloatValue();
        reverbFreezeMode = parameters[5].getFloatValue();
        updateReverbParameters();
        
    } else if (actionName == ACTION_SAVE_PRESET){
        String presetName = parameters[0];
        int index = parameters[1].getIntValue();
        saveCurrentPresetToFile(presetName, index);  // Save to file...
        currentPresetIndex = index; // ...and update current preset index and name in case it was changed
        saveGlobalPersistentStateToFile();  // Save global state to reflect last loaded preset has the right index
        
    } else if (actionName == ACTION_LOAD_PRESET){
        int index = parameters[0].getIntValue();
        setCurrentProgram(index);
        
    } else if (actionName == ACTION_SET_MIDI_IN_CHANNEL){
        int channel = parameters[0].getIntValue();
        setMidiInChannelFilter(channel);
        
    } else if (actionName == ACTION_SET_MIDI_THRU){
        bool midiThru = parameters[0].getIntValue() == 1;
        setMidiThru(midiThru);
        
    } else if (actionName == ACTION_PLAY_SOUND){
        juce::String soundUUID = parameters[0];
        addToMidiBuffer(soundUUID, false);
        
    } else if (actionName == ACTION_STOP_SOUND){
        juce::String soundUUID = parameters[0];
        addToMidiBuffer(soundUUID, true);
        
    } else if (actionName == ACTION_SET_POLYPHONY){
        numVoices = parameters[0].getIntValue();
        sampler.setSamplerVoices(numVoices);

    } else if (actionName == ACTION_ADD_OR_UPDATE_CC_MAPPING){
        juce::String soundUUID = parameters[0];
        juce::String uuid = parameters[1];
        int ccNumber = parameters[2].getIntValue();
        juce::String parameterName = parameters[3];
        float minRange = parameters[4].getFloatValue();
        float maxRange = parameters[5].getFloatValue();
        DBG(soundUUID);
        auto* sound = sounds->getSoundWithUUID(soundUUID);
        if (sound != nullptr){
            sound->addOrEditMidiMapping(uuid, ccNumber, parameterName, minRange, maxRange);
        }

    } else if (actionName == ACTION_REMOVE_CC_MAPPING){
        juce::String soundUUID = parameters[0];
        String uuid = parameters[1];
        auto* sound = sounds->getSoundWithUUID(soundUUID);
        if (sound != nullptr){
            sound->removeMidiMapping(uuid);
        }

    } else if (actionName == ACTION_SET_STATE_TIMER_HZ){
        int newHz = parameters[0].getIntValue();
        stopTimer();
        startTimerHz(newHz);
        
    } else if (actionName == ACTION_REMOVE_SOUND){
        juce::String soundUUID = parameters[0];
        removeSound(soundUUID);
        
    } else if (actionName == ACTION_ADD_OR_REPLACE_SOUND){
        juce::String soundUUID = parameters[0];
        int soundID = parameters[1].getIntValue();
        String soundName = parameters[2];
        String soundUser = parameters[3];
        String soundLicense = parameters[4];
        String oggDownloadURL = parameters[5];
        String localFilePath = parameters[6];
        String type = parameters[7];
        int sizeBytes = parameters[8].getIntValue();
        String serializedSlices = parameters[9];
        StringArray slices;
        if (serializedSlices != ""){
            slices.addTokens(serializedSlices, ",", "");
        }
        String assignedNotesBigInteger = parameters[10];
        BigInteger midiNotes;
        if (assignedNotesBigInteger != ""){
            midiNotes.parseString(assignedNotesBigInteger, 16);
        }
        int midiRootNote = parameters[11].getIntValue();
        
        addOrReplaceSoundFromBasicSoundProperties(soundUUID, soundID, soundName, soundUser, soundLicense, oggDownloadURL, localFilePath, type, sizeBytes, slices, midiNotes, midiRootNote);
        
    } else if (actionName == ACTION_REAPPLY_LAYOUT){
        int newNoteLayout = parameters[0].getIntValue();
        reapplyNoteLayout(newNoteLayout);
        
    } else if (actionName == ACTION_SET_SOUND_SLICES){
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

    } else if (actionName == ACTION_SET_SOUND_ASSIGNED_NOTES){
        juce::String soundUUID = parameters[0];
        String assignedNotesBigInteger = parameters[1];
        int rootNote = parameters[2].getIntValue();
        
        BigInteger midiNotes;
        if (assignedNotesBigInteger != ""){
            midiNotes.parseString(assignedNotesBigInteger, 16);
        }
        
        auto* sound = sounds->getSoundWithUUID(soundUUID);
        if (sound != nullptr){
            sound->setMappedMidiNotes(midiNotes);
            sound->setMidiRootNote(rootNote);
        }
        
    } else if (actionName == ACTION_CLEAR_ALL_SOUNDS){
        removeAllSounds();
        
    } else if (actionName == ACTION_GET_STATE){
        String stateType = parameters[0];
        if (stateType == "volatile"){
            sendStateToExternalServer(collectVolatileStateInformation(), "");
        } else if (stateType == "volatileString"){
            #if SEND_VOLATILE_STATE_OVER_OSC
            juce::OSCMessage message = juce::OSCMessage("/volatile_state_osc");
            message.addString(collectVolatileStateInformationAsString());
            sendOSCMessage(message);
            #else
            sendStateToExternalServer(ValueTree(), collectVolatileStateInformationAsString());
            #endif
            
        } else if (stateType == "full"){
            sendStateToExternalServer(state, "");
            
        } else if (stateType == "oscFull"){
            juce::OSCMessage message = juce::OSCMessage("/full_state");
            message.addInt32(stateUpdateID);
            message.addString(state.toXmlString());
            sendOSCMessage(message);
            
        } else if (stateType == "oscVolatileString"){
            juce::OSCMessage message = juce::OSCMessage("/volatile_state_string");
            message.addString(collectVolatileStateInformationAsString());
            sendOSCMessage(message);
        }
    } else if (actionName == ACTION_PLAY_SOUND_FROM_PATH){
        String soundPath = parameters[0];
        if (soundPath == ""){
            stopPreviewingFile();  // Send empty string to stop currently previewing sound
        } else {
            previewFile(soundPath);
        }
    } else if (actionName == ACTION_SET_USE_ORIGINAL_FILES_PREFERENCE){
        String preference = parameters[0];
        useOriginalFilesPreference = preference;
        saveGlobalPersistentStateToFile();
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
    globalMidiInChannel = channel;
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
    if (isQuerying){
        // If already querying, don't run another query
        logToState("Skip query as already querying...");
        return;
    }
    
    FreesoundClient client(FREESOUND_API_KEY);
    isQuerying = true;
    logToState("Querying new sounds for: " + textQuery);
    auto filter = "duration:[" + (String)minSoundLength + " TO " + (String)maxSoundLength + "]";
    SoundList list = client.textSearch(textQuery, filter, "score", 0, -1, 80, "id,name,username,license,type,filesize,previews,analysis", "rhythm.onset_times", 0);
    isQuerying = false;
    if (list.getCount() > 0){
        
        // Randmomize results and prepare for iteration
        Array<FSSound> soundsFound = list.toArrayOfSounds();
        logToState("Query got " + (String)list.getCount() + " results, " + (String)soundsFound.size() + " in the first page. Will load " + (String)numSounds + " sounds.");
        unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
        std::shuffle(soundsFound.begin(), soundsFound.end(), std::default_random_engine(seed));
        soundsFound.resize(jmin(numSounds, list.getCount()));
        int nSounds = soundsFound.size();
        
        // Move this elsewhere in a helper function (?)
        juce::ValueTree presetState = state.getChildWithName(IDs::PRESET);
        
        // Clear all loaded sounds
        removeAllSounds();

        // Load the first nSounds from the found ones
        int nNotesPerSound = 128 / nSounds;
        for (int i=0; i<nSounds; i++){
            
            // Calculate assigned notes
            int midiRootNote;
            BigInteger midiNotes;
            if (noteLayoutType == NOTE_MAPPING_TYPE_CONTIGUOUS){
                // In this case, all the notes mapped to this sound are contiguous in a range which depends on the total number of sounds to load
                midiRootNote = i * nNotesPerSound + nNotesPerSound / 2;
                midiNotes.setRange(i * nNotesPerSound, nNotesPerSound, true);
            } else {
                // Notes are mapped to sounds in interleaved fashion so each contiguous note corresponds to a different sound
                midiRootNote = NOTE_MAPPING_INTERLEAVED_ROOT_NOTE + i;
                for (int j=midiRootNote; j<128; j=j+nSounds){
                    midiNotes.setBit(j);  // Map notes in upwards direction
                }
                for (int j=midiRootNote; j>=0; j=j-nSounds){
                    midiNotes.setBit(j);  // Map notes in downwards direction
                }
            }
            
            // Create sound
            addOrReplaceSoundFromBasicSoundProperties("", soundsFound[i], midiNotes, midiRootNote);
        }
    } else {
        logToState("Query got no results...");
    }
}


void SourceSamplerAudioProcessor::removeSound(const juce::String& soundUUID)
{
    // Trigger the deletion of the sound by disabling it
    // Once disabled, all playing notes will be stopped and the sound removed a while after that
    const ScopedLock sl (soundDeleteLock);
    sounds->getSoundWithUUID(soundUUID)->disableSound();
}

void SourceSamplerAudioProcessor::removeAllSounds()
{
    // Trigger the deletion of the sounds by disabling them
    // Once disabled, all playing notes will be stopped and the sounds removed a while after that
    const ScopedLock sl (soundDeleteLock);
    for (auto* sound: sounds->objects){
        sound->disableSound();
    }
}

void SourceSamplerAudioProcessor::addOrReplaceSoundFromBasicSoundProperties(const String& soundUUID,
                                                                            int soundID,
                                                                            const String& soundName,
                                                                            const String& soundUser,
                                                                            const String& soundLicense,
                                                                            const String& previewURL,
                                                                            const String& localFilePath,
                                                                            const String& format,
                                                                            int sizeBytes,
                                                                            StringArray slices,
                                                                            BigInteger midiNotes,
                                                                            int midiRootNote)
{
    int existingSoundIndex = sounds->getIndexOfSoundWithUUID(soundUUID);
    if ((existingSoundIndex > -1) && (midiRootNote ==  -1) && (midiNotes.isZero())){
        // If sound exists for that UUID and midiNotes/midiRootNote are not passed as parameters, use the same parameters from the original sound
        auto* sound = sounds->getSoundWithUUID(soundUUID);
        midiNotes = sound->getMappedMidiNotes();
        midiRootNote = sound->getMidiRootNote();
    }
    
    juce::ValueTree sourceSound = Helpers::createSourceSoundAndSourceSamplerSoundFromProperties(soundUUID, soundID, soundName, soundUser, soundLicense, previewURL, localFilePath, format, sizeBytes, slices, midiNotes, midiRootNote);
    juce::ValueTree presetState = state.getChildWithName(IDs::PRESET);
    
    if (existingSoundIndex > -1){
        // If a sound exists for that UUID, remove the sound with that UUID and replace it by the new sound
        presetState.removeChild(existingSoundIndex, nullptr);
        presetState.addChild(sourceSound, existingSoundIndex, nullptr);
        
    } else {
        // If no sound exists with the given UUID, add the sound as a NEW one at the end of the list
        presetState.addChild(sourceSound, -1, nullptr);
    }
}

void SourceSamplerAudioProcessor::addOrReplaceSoundFromBasicSoundProperties(const String& soundUUID,
                                                                            FSSound sound,
                                                                            BigInteger midiNotes,
                                                                            int midiRootNote)
{
    // Prepare slices
    StringArray slices;
    if (sound.analysis.hasProperty("rhythm")){
        if (sound.analysis["rhythm"].hasProperty("onset_times")){
            for (int j=0; j<sound.analysis["rhythm"]["onset_times"].size(); j++){
                slices.add(sound.analysis["rhythm"]["onset_times"][j].toString());
            }
        }
    }
    
    // Create sound
    addOrReplaceSoundFromBasicSoundProperties(soundUUID, sound.id.getIntValue(), sound.name, sound.user, sound.license, sound.getOGGPreviewURL().toString(false), "", sound.format, sound.filesize, slices, midiNotes, midiRootNote);
}

void SourceSamplerAudioProcessor::reapplyNoteLayout(int newNoteLayoutType)
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
                    BigInteger midiNotes;
                    midiNotes.setRange(i * nNotesPerSound, nNotesPerSound, true);
                    sound->setMappedMidiNotes(midiNotes);
                    sound->setMidiRootNote(rootNote);
                } else if (noteLayoutType == NOTE_MAPPING_TYPE_INTERLEAVED){
                    int rootNote = NOTE_MAPPING_INTERLEAVED_ROOT_NOTE + i;
                    BigInteger midiNotes;
                    for (int j=rootNote; j<128; j=j+nSounds){
                        midiNotes.setBit(j);  // Map notes in upwards direction
                    }
                    for (int j=rootNote; j>=0; j=j-nSounds){
                        midiNotes.setBit(j);  // Map notes in downwards direction
                    }
                    sound->setMappedMidiNotes(midiNotes);
                    sound->setMidiRootNote(rootNote);
                }
            }
        }
    }
}

void SourceSamplerAudioProcessor::addToMidiBuffer(const juce::String& soundUUID, bool doNoteOff)
{
    auto* sound = sounds->getSoundWithUUID(soundUUID);
    if (sound != nullptr){
        int midiNoteForNormalPitch = 36;
        midiNoteForNormalPitch = sound->getMidiRootNote();
        BigInteger assignedMidiNotes = sound->getMappedMidiNotes();
        if (assignedMidiNotes[midiNoteForNormalPitch] == false){
            // If the root note is not mapped to the sound, find the closest mapped one
            midiNoteForNormalPitch = assignedMidiNotes.findNextSetBit(midiNoteForNormalPitch);
        }
        if (midiNoteForNormalPitch < 0){
            // If for some reason no note was found, don't play anything
        } else {
            int midiChannel = sound->getParameterInt(IDs::midiChannel);
            if (midiChannel == 0){
                if (globalMidiInChannel > 0){
                    midiChannel = globalMidiInChannel;
                } else {
                    midiChannel = 1;
                }
            }
            MidiMessage message = MidiMessage::noteOn(midiChannel, midiNoteForNormalPitch, (uint8)127);
            if (doNoteOff){
                message = MidiMessage::noteOff(midiChannel, midiNoteForNormalPitch, (uint8)127);
            }
            midiFromEditor.addEvent(message, 0);
        }
    }
}

double SourceSamplerAudioProcessor::getStartTime(){
    return startTime;
}

void SourceSamplerAudioProcessor::timerCallback()
{
    // Delete sounds that should be deleted
    //const ScopedLock sl (soundDeleteLock);
    for (int i=sounds->objects.size() - 1; i>=0 ; i--){
        auto* sound = sounds->objects[i];
        if (sound->shouldBeDeleted()){
            sounds->removeSoundWithUUID(sound->getUUID());
        }
    }
    
    // Collect the state and update the serverInterface object with that state information so it can be used by the embedded http server    
    #if ENABLE_EMBEDDED_HTTP_SERVER
    //fullState.setProperty(STATE_CURRENT_PORT, getServerInterfaceHttpPort(), nullptr);
    serverInterface.serializedAppState = state.toXmlString();
    juce::ValueTree volatileState = collectVolatileStateInformation();
    volatileState.setProperty(IDs::logMessages, recentLogMessagesSerialized, nullptr);
    serverInterface.serializedAppStateVolatile = volatileState.toXmlString();
    #endif
    
    // NOTE: if using externall HTTP server (i.e. in ELK), we don't send state updates from a timer but only send
    // them in response to requests from the external server app
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

void SourceSamplerAudioProcessor::previewFile(const String& path)
{
    if (transportSource.isPlaying()){
        transportSource.stop();
    }
    if (currentlyLoadedPreviewFilePath != path){
        // Load new file
        String pathToLoad;
        if (path.startsWith("http")){
            // If path is an URL, download the file first
            StringArray tokens;
            tokens.addTokens (path, "/", "");
            String filename = tokens[tokens.size() - 1];
            
            File location = tmpFilesLocation.getChildFile(filename);
            if (!location.exists()){  // Dont' re-download if file already exists
                # if ELK_BUILD
                // If on ELK build don't download here because it seems to hang forever
                // On ELK, we should trigger the downloads from the Python script before calling this function
                # else
                std::unique_ptr<URL::DownloadTask> downloadTask = URL(path).downloadToFile(location, "");
                while (!downloadTask->isFinished()){
                    // Wait until it finished downloading
                }
                # endif
            }
            pathToLoad = location.getFullPathName();  // Update path variable to the download location
        } else {
            pathToLoad = path;
        }
        File file = File(pathToLoad);
        if (file.existsAsFile()){
            
            auto* reader = audioFormatManager.createReaderFor (file);
            if (reader != nullptr)
            {
                std::unique_ptr<AudioFormatReaderSource> newSource (new AudioFormatReaderSource (reader, true));
                transportSource.setSource (newSource.get(), 0, nullptr, reader->sampleRate);
                readerSource.reset (newSource.release());
            }
        }
        
    }
    transportSource.start();
}

void SourceSamplerAudioProcessor::stopPreviewingFile(){
    if (transportSource.isPlaying()){
        transportSource.stop();
    }
}


//==============================================================================

void SourceSamplerAudioProcessor::sendOSCMessage(const OSCMessage& message)
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


//==============================================================================

void SourceSamplerAudioProcessor::valueTreePropertyChanged (juce::ValueTree& treeWhosePropertyHasChanged, const juce::Identifier& property)
{
    // We should never call this function from the realtime thread because editing VT might not be RT safe...
    // TODO: proper check that this is not audio thread
    //jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
    DBG("Changed " << treeWhosePropertyHasChanged[IDs::name].toString() << " " << property.toString() << ": " << treeWhosePropertyHasChanged[property].toString());
    #if SYNC_STATE_WITH_OSC
    juce::OSCMessage message = juce::OSCMessage("/state_update");
    message.addString("propertyChanged");
    message.addInt32(stateUpdateID);
    message.addString(treeWhosePropertyHasChanged[IDs::uuid].toString());
    message.addString(treeWhosePropertyHasChanged.getType().toString());
    message.addString(property.toString());
    message.addString(treeWhosePropertyHasChanged[property].toString());
    sendOSCMessage(message);
    stateUpdateID += 1;
    #endif
}

void SourceSamplerAudioProcessor::valueTreeChildAdded (juce::ValueTree& parentTree, juce::ValueTree& childWhichHasBeenAdded)
{
    // We should never call this function from the realtime thread because editing VT might not be RT safe...
    // TODO: proper check that this is not audio thread
    //jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
    DBG("Added VT child " << childWhichHasBeenAdded.getType());
    #if SYNC_STATE_WITH_OSC
    juce::OSCMessage message = juce::OSCMessage("/state_update");
    message.addString("addedChild");
    message.addInt32(stateUpdateID);
    message.addString(parentTree[IDs::uuid].toString());
    message.addString(parentTree.getType().toString());
    message.addString(childWhichHasBeenAdded.toXmlString());
    message.addInt32(parentTree.indexOf(childWhichHasBeenAdded));
    sendOSCMessage(message);
    stateUpdateID += 1;
    #endif
}

void SourceSamplerAudioProcessor::valueTreeChildRemoved (juce::ValueTree& parentTree, juce::ValueTree& childWhichHasBeenRemoved, int indexFromWhichChildWasRemoved)
{
    // We should never call this function from the realtime thread because editing VT might not be RT safe...
    // TODO: proper check that this is not audio thread
    //jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
    DBG("Removed VT child " << childWhichHasBeenRemoved.getType());
    #if SYNC_STATE_WITH_OSC
    juce::OSCMessage message = juce::OSCMessage("/state_update");
    message.addString("removedChild");
    message.addInt32(stateUpdateID);
    message.addString(childWhichHasBeenRemoved[IDs::uuid].toString());
    message.addString(childWhichHasBeenRemoved.getType().toString());
    sendOSCMessage(message);
    stateUpdateID += 1;
    #endif
}

void SourceSamplerAudioProcessor::valueTreeChildOrderChanged (juce::ValueTree& parentTree, int oldIndex, int newIndex)
{
    // We should never call this function from the realtime thread because editing VT might not be RT safe...
    // jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
}

void SourceSamplerAudioProcessor::valueTreeParentChanged (juce::ValueTree& treeWhoseParentHasChanged)
{
    // We should never call this function from the realtime thread because editing VT might not be RT safe...
    // jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
}


//==============================================================================
// This creates new instances of the plugin..
AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SourceSamplerAudioProcessor();
}
