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
    sampleLoaderThread (*this),
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
    downloader.addActionListener(this);
    
    // Start timer to collect state and pass it to the UI
    startTimerHz(STATE_UPDATE_HZ);
    
    // NOTE: code below is for the VT refactor, things above willl most likely need to change as well...
    
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
    context.soundsDownloadLocation = soundsDownloadLocation;
    context.sourceDataLocation = sourceDataLocation;
    context.presetFilesLocation = presetFilesLocation;
    context.tmpFilesLocation = tmpFilesLocation;
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
        if ((sampler.getMidiInChannel() == 0) || (message.getChannel() == sampler.getMidiInChannel())){
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
        presetState.removeAllChildren(nullptr);
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
                sampler.setMidiInChannel((int)settings.getProperty(IDs::globalMidiInChannel));
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
    state.setProperty(IDs::isQueryingAndDownloadingSounds, isQueryDownloadingAndLoadingSounds, nullptr);
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
                voiceSoundIdxs += (String)playingSound->getSourceSound()->getFirstLinkedSourceSamplerSound()->getSoundId() + ",";
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
    
    stateAsStringParts.add(isQueryDownloadingAndLoadingSounds ? "1" : "0");
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
                voiceSoundIdxs += (String)playingSound->getSourceSound()->getFirstLinkedSourceSamplerSound()->getSoundId() + ",";
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
        // TODO: VT refactor
        // A sound has finished downloading, trigger loading into sampler
        String soundID = parameters[0];
        for (int i=0; i<loadedSoundsInfo.getNumChildren(); i++){
            ValueTree soundInfo = loadedSoundsInfo.getChild(i);
            if (soundID == soundInfo.getProperty(IDs::soundId).toString()){
                soundInfo.setProperty(IDs::downloadProgress, 100, nullptr); // Set download progress to 100 to indicate download has finished
                
                #if LOAD_SAMPLES_IN_THREAD
                    // Trigger loading of audio sample into the sampler in thread
                    sampleLoaderThread.setSoundToLoad(i);
                    sampleLoaderThread.run();
                #else
                    // Trigger loading of audio sample into the sampler
                    // NOTE: commented for testing purposes
                    //setSingleSourceSamplerSoundObject(i);
                #endif
            }
        }
        
        if (allSoundsFinishedDownloading()){
            isQueryDownloadingAndLoadingSounds = false;  // Set flag to false because we finished downloading and loading sounds
        }
        
    } else if (actionName == ACTION_SOUND_READY_TO_LOAD){
        // TODO: VT refactor
        // A sound can be loaded because the local file already exists
        // NOTE: for sounds that come from Freesound, we call ACTION_FINISHED_DOWNLOADING_SOUND even if
        // the sound is already downloaded from previous uses. ACTION_FINISHED_DOWNLOADING_SOUND will also
        // load the sound in the sampler just like ACTION_SOUND_READY_TO_LOAD
        
        int soundIndex = parameters[0].getIntValue();
        #if LOAD_SAMPLES_IN_THREAD
            // Trigger loading of audio sample into the sampler in thread
            sampleLoaderThread.setSoundToLoad(soundIndex);
            sampleLoaderThread.run();
        #else
            // Trigger loading of audio sample into the sampler
            // NOTE: commented for testing purposes
            // setSingleSourceSamplerSoundObject(soundIndex);
        #endif
        
        if (allSoundsFinishedDownloading()){
            isQueryDownloadingAndLoadingSounds = false;  // Set flag to false because we finished downloading and loading sounds
        }
        
    } else if (actionName == ACTION_DOWNLOADING_SOUND_PROGRESS){
        // A sound has finished downloading, trigger loading into sampler
        // TODO: VT refactor
        String soundID = parameters[0];
        int percentageDone = parameters[1].getIntValue();
        for (int i=0; i<loadedSoundsInfo.getNumChildren(); i++){
            ValueTree soundInfo = loadedSoundsInfo.getChild(i);
            if (soundID == soundInfo.getProperty(IDs::soundId).toString()){
                soundInfo.setProperty(IDs::downloadProgress, percentageDone, nullptr); // Set download progress to 100 to indicate download has finished
            }
        }
        
    } else if (actionName == ACTION_NEW_QUERY){
        String query = parameters[0];
        int numSounds = parameters[1].getIntValue();
        float minSoundLength = parameters[2].getFloatValue();
        float maxSoundLength = parameters[3].getFloatValue();
        int noteMappingType = parameters[4].getFloatValue();
        noteLayoutType = noteMappingType; // Set noteLayoutType so when sounds are actually downloaded and loaded, the requested mode is used
        #if MAKE_QUERY_IN_THREAD
            queryMakerThread.setQueryParameters(query, numSounds, minSoundLength, maxSoundLength);
            queryMakerThread.run();
        #else
            makeQueryAndLoadSounds(query, numSounds, minSoundLength, maxSoundLength);
        #endif
        
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
        auto* sound = sounds->getSoundWithUUID(soundUUID);
        if (sound != nullptr){
            sound->addOrEditMidiMapping(uuid, ccNumber, parameterName, minRange, maxRange);
        }

    } else if (actionName == ACTION_REMOVE_CC_MAPPING){
        juce::String soundUUID = parameters[0];
        String uuid = parameters[1];
        auto* sound = sounds->getSoundWithUUID(soundUUID);  // This index is provided by the UI and corresponds to the position in loadedSoundsInfo, which matches idx property of SourceSamplerSound
        if (sound != nullptr){
            sound->removeMidiMapping(uuid);
        }

    } else if (actionName == ACTION_SET_STATE_TIMER_HZ){
        int newHz = parameters[0].getIntValue();
        stopTimer();
        startTimerHz(newHz);
        
    } else if (actionName == ACTION_REMOVE_SOUND){
        // TODO: VT refactor
        int soundIndex = parameters[0].getIntValue();
        removeSound(soundIndex);
        
    } else if (actionName == ACTION_ADD_OR_REPLACE_SOUND){
        // TODO: VT refactor
        int soundIndex = parameters[0].getIntValue();
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
        String triggerDownloadSoundAction = parameters[12];
        
        addOrReplaceSoundFromBasicSoundProperties(soundIndex, soundID, soundName, soundUser, soundLicense, oggDownloadURL, localFilePath, type, sizeBytes, slices, midiNotes, midiRootNote, triggerDownloadSoundAction);
        
    } else if (actionName == ACTION_REAPPLY_LAYOUT){
        // TODO: VT refactor
        int newNoteLayout = parameters[0].getIntValue();
        reapplyNoteLayout(newNoteLayout);
        
    } else if (actionName == ACTION_SET_SOUND_SLICES){
        // TODO: VT refactor (?)
        juce::String soundUUID = parameters[0];
        juce::String serializedSlices = parameters[1];
        juce::StringArray slices;
        slices.addTokens(serializedSlices, ",", "");
        
        /*
        
        // Add slices to sound info
        juce::ValueTree onsetTimes = juce::ValueTree(STATE_SOUND_FS_SOUND_ANALYSIS_ONSETS);  // TODO: this should be renamed or something to store slices with a proper name instead of fs analysis onsets
        for (int i=0; i<slices.size(); i++){
            ValueTree onset = ValueTree(STATE_SOUND_FS_SOUND_ANALYSIS_ONSET);
            onset.setProperty(STATE_SOUND_FS_SOUND_ANALYSIS_ONSET_TIME, slices[i].getFloatValue(), nullptr);
            onsetTimes.appendChild(onset, nullptr);
        }
        juce::ValueTree existingFsAnalysis = soundInfo.getChildWithName(STATE_SOUND_FS_SOUND_ANALYSIS);
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
        auto* sound = sounds->getSoundWithUUID(soundUUID)->getFirstLinkedSourceSamplerSound();  // This index is provided by the UI and corresponds to the position in loadedSoundsInfo, which matches idx property of SourceSamplerSound
        if (sound != nullptr){
            std::vector<float> onsets = {};
            for (int i=0; i<onsetTimes.getNumChildren(); i++){
                ValueTree onsetVT = onsetTimes.getChild(i);
                float onset = (float)(onsetVT.getProperty(STATE_SOUND_FS_SOUND_ANALYSIS_ONSET_TIME));
                onsets.push_back(onset);
            }
            sound->setOnsetTimesSamples(onsets);
        }*/
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
        juce::ValueTree presetState = state.getChildWithName(IDs::PRESET);
        if (presetState.isValid()){
            presetState.removeAllChildren(nullptr);
        }
        
    } else if (actionName == ACTION_GET_STATE){
        String stateType = parameters[0];
        if (stateType == "volatile"){
            sendStateToExternalServer(collectVolatileStateInformation(), "");
        } else if (stateType == "volatileString"){
            #if SEND_VOLATILE_STATE_OVER_OSC
            if (!oscSenderIsConnected){
                if (oscSender.connect ("127.0.0.1", 9002)){
                    oscSenderIsConnected = true;
                }
            }
            if (oscSenderIsConnected){
                oscSender.send ("/volatile_state_osc", collectVolatileStateInformationAsString());
            }
            #else
            sendStateToExternalServer(ValueTree(), collectVolatileStateInformationAsString());
            #endif
            
        } else if (stateType == "full"){
            sendStateToExternalServer(state, "");
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
    sampler.setMidiInChannel(globalMidiInChannel);
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
    SoundList list = client.textSearch(textQuery, filter, "score", 0, -1, 80, "id,name,username,license,type,filesize,previews,analysis", "rhythm.onset_times", 0);
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
        presetState.removeAllChildren(nullptr);
        
        // Prepare some variables for note layout
        int nNotesPerSound = 128 / nSounds;
        
        for (int i=0; i<nSounds; i++){
            FSSound sound = soundsFound[i];
            juce::ValueTree sourceSound = Helpers::createEmptySourceSoundState(sound.name);
            sourceSound.setProperty(IDs::soundId, sound.id, nullptr);
            sourceSound.setProperty (IDs::username, sound.user, nullptr);
            sourceSound.setProperty(IDs::license, sound.license, nullptr);
            sourceSound.setProperty(IDs::previewURL, sound.getOGGPreviewURL().toString(false), nullptr);
            sourceSound.setProperty(IDs::format, sound.format, nullptr);
            sourceSound.setProperty(IDs::filesize, sound.filesize, nullptr);
            
            juce::ValueTree sourceSamplerSound = Helpers::createSourceSampleSoundState(sourceSound.getProperty(IDs::soundId).toString() + " - 1", (int)sourceSound.getProperty(IDs::soundId), sourceSound.getProperty(IDs::previewURL).toString());
            juce::ValueTree soundAnalysis = Helpers::createAnalysisFromFreesoundAnalysis(sound.analysis);
            sourceSamplerSound.addChild(soundAnalysis, -1, nullptr);
            
            // Calcualte note layout stuff
            int midiRootNote;
            BigInteger midiNotes;
            if (noteLayoutType == NOTE_MAPPING_TYPE_CONTIGUOUS){
                // In this case, all the notes mapped to this sound are contiguous in a range which depends on the total number of sounds to load
                midiRootNote = i * nNotesPerSound + nNotesPerSound / 2;
                midiNotes.setRange(i * nNotesPerSound, nNotesPerSound, true);
            } else {
                // Notes are mapped to sounds in interleaved fashion so each contiguous note corresponds to a different sound
                midiRootNote = NOTE_MAPPING_INTERLEAVED_ROOT_NOTE + i;
                for (int i=midiRootNote; i<128; i=i+nSounds){
                    midiNotes.setBit(i);  // Map notes in upwards direction
                }
                for (int i=midiRootNote; i>=0; i=i-nSounds){
                    midiNotes.setBit(i);  // Map notes in downwards direction
                }
            }
            sourceSamplerSound.setProperty(IDs::midiRootNote, midiRootNote, nullptr);
            sourceSamplerSound.setProperty(IDs::midiNotes, midiNotes.toString(16), nullptr);
            

            // Add the source sampler sound to source sound, and source sound to the state
            sourceSound.addChild(sourceSamplerSound, -1, nullptr);
            presetState.addChild(sourceSound, -1, nullptr);
        }
         
    } else {
        logToState("Query got no results...");
    }
}

bool SourceSamplerAudioProcessor::isSupportedAudioFileFormat(const String& extension)
{
    return StringArray({"ogg", "wav", "aiff", "mp3", "flac"}).contains(extension.toLowerCase().replace(".", ""));
}


bool SourceSamplerAudioProcessor::fileLocationIsSupportedAudioFileFormat(File location)
{
    return isSupportedAudioFileFormat(location.getFileExtension());
}

File SourceSamplerAudioProcessor::getSoundPreviewLocation(ValueTree soundInfo)
{
    // TODO: vt refactor
    if (soundInfo.hasProperty(IDs::soundId)){
        return soundsDownloadLocation.getChildFile(soundInfo.getProperty(IDs::soundId).toString()).withFileExtension("ogg");
    }
    return File();  // Return empty path
}

File SourceSamplerAudioProcessor::getSoundOriginalFileLocation(ValueTree soundInfo)
{
    // TODO: vt refactor
    if (soundInfo.hasProperty(IDs::format)){
        return soundsDownloadLocation.getChildFile(soundInfo.getProperty(IDs::soundId).toString() + ".original." + soundInfo.getProperty(IDs::format).toString());
    }
    return File();  // Return empty path
}

File SourceSamplerAudioProcessor::getSoundLocalPathLocation(ValueTree soundInfo)
{
    // TODO: vt refactor
    if (soundInfo.hasProperty(IDs::pathInDisk)){
        // This is for sounds loaded directly from SD card
        return File(soundInfo.getProperty(IDs::pathInDisk).toString());
    }
    return File();  // Return empty path
}

File SourceSamplerAudioProcessor::getSoundFileLocationToLoad(ValueTree soundInfo)
{
    File localPath = getSoundLocalPathLocation(soundInfo);
    if (localPath.getFullPathName() != ""){
        // If local file path exists, then the sound is not from Freesound and we should load directly
        // the file referenced in local file property
        return localPath;
    }
    
    if (useOriginalFilesPreference.get() == USE_ORIGINAL_FILES_ALWAYS){
        // Return path to original sound if sound has "type" property and the file at sonund_id.original.type exists
        // Otherwise the function will default to the preview file path
        File originalFilePath = getSoundOriginalFileLocation(soundInfo);
        if (originalFilePath.getFullPathName() != ""){
            if (originalFilePath.exists() && originalFilePath.getSize() > 0){
                return originalFilePath;
            }
        }
    } else if (useOriginalFilesPreference.get() == USE_ORIGINAL_FILES_ONLY_SHORT){
        
        if (soundInfo.hasProperty(IDs::filesize)){
            // If sound has property "size", check if it is below the maximum allowed for original files and then
            // check if original file exists at sonund_id.original.type (like in the previous case)
            // Otherwise the function will default to the preview file path
            if ((int)soundInfo.getProperty(IDs::filesize) <= MAX_SIZE_FOR_ORIGINAL_FILE_DOWNLOAD){
                File originalFilePath = getSoundOriginalFileLocation(soundInfo);
                if (originalFilePath.getFullPathName() != ""){
                    if (originalFilePath.exists() && originalFilePath.getSize() > 0){
                        return originalFilePath;
                    }
                }
            }
        }
    }
    
    return getSoundPreviewLocation(soundInfo);
}

void SourceSamplerAudioProcessor::downloadSounds (bool blocking, int soundIndexFilter)
{
    // NOTE: blocking only has effect in non-elk situation
    
    #if !USE_EXTERNAL_HTTP_SERVER_FOR_DOWNLOADS
    
    // Download the sounds within the plugin
    // NOTE that when downloading in the plugin, only using previews is supported so
    // we make no check about useOriginalFilesPreference
    
    std::vector<std::tuple<File, String, String>> soundTargetLocationsAndUrlsToDownload = {};
    for (int i=0; i<loadedSoundsInfo.getNumChildren(); i++){
        if ((soundIndexFilter == -1) || (soundIndexFilter == i)){
            ValueTree soundInfo = loadedSoundsInfo.getChild(i);
            if (soundInfo.hasProperty(IDs::previewURL)){
                String soundID = soundInfo.getProperty(IDs::soundId).toString();
                String preview_url = soundInfo.getProperty(IDs::previewURL);
                soundTargetLocationsAndUrlsToDownload.push_back({soundsDownloadLocation, soundID, preview_url});
            } else {
                // If sound does not have property STATE_SOUND_INFO_OGG_DOWNLOAD_URL, it means the
                // sound does not come from Freesound and therefore should not be downloaded
                // Trigger directly an action to indicate that the sound is ready to be loaded
                String actionMessage = String(ACTION_SOUND_READY_TO_LOAD) + ":" + (String)i;
                actionListenerCallback(actionMessage);
            }
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
    String oggUrlsParam = "";
    String typesParam = "";
    String idsParam = "";
    String sizesParam = "";
    int nSentToDownload = 0;
    int nAlreadyDownloaded = 0;
    int nLocalSounds = 0;
    for (int i=0; i<loadedSoundsInfo.getNumChildren(); i++){
        if ((soundIndexFilter == -1) || (soundIndexFilter == i)){
            ValueTree soundInfo = loadedSoundsInfo.getChild(i);
            if (soundInfo.hasProperty(IDs::previewURL)){
                // If sound has property STATE_SOUND_INFO_OGG_DOWNLOAD_URL it means it is a Freesound sound
                // First check if the sound has already been downloaded in the original or preview locations
                // (which one to check for will depend on usage of original files preference).
                // If sounds have already been downloaded, trigger ACTION_FINISHED_DOWNLOADING_SOUND directly, otherwise
                // send request to server to download sounds
                
                bool soundAlreadyDownloaded = false;
                File previewFilePath = getSoundPreviewLocation(soundInfo);
                File originalFilePath = getSoundOriginalFileLocation(soundInfo);

                if (useOriginalFilesPreference.get() == USE_ORIGINAL_FILES_NEVER){
                    if (previewFilePath.exists() && previewFilePath.getSize() > 0) {
                        soundAlreadyDownloaded = true;
                    }
                    
                } else if (useOriginalFilesPreference.get() == USE_ORIGINAL_FILES_ALWAYS){
                    if (fileLocationIsSupportedAudioFileFormat(originalFilePath)){
                        if (originalFilePath.exists() && originalFilePath.getSize() > 0) {
                            soundAlreadyDownloaded = true;
                        }
                    } else {
                        // If we want original file but original file format is not supported, check for preview
                        if (previewFilePath.exists() && previewFilePath.getSize() > 0) {
                            soundAlreadyDownloaded = true;
                        }
                    }
                    
                } else if (useOriginalFilesPreference.get() == USE_ORIGINAL_FILES_ONLY_SHORT){
                    
                    if (soundInfo.hasProperty(IDs::format)){
                        if (((int)soundInfo.getProperty(IDs::filesize) <= MAX_SIZE_FOR_ORIGINAL_FILE_DOWNLOAD) && (fileLocationIsSupportedAudioFileFormat(originalFilePath))){
                            if (originalFilePath.exists() && originalFilePath.getSize() > 0) {
                                soundAlreadyDownloaded = true;
                            }
                        } else {
                            // If original file is too big or file format is not supported, check for preview
                            if (previewFilePath.exists() && previewFilePath.getSize() > 0) {
                                soundAlreadyDownloaded = true;
                            }
                        }
                    }
                }
                
                if (soundAlreadyDownloaded) {
                    String actionMessage = String(ACTION_FINISHED_DOWNLOADING_SOUND) + ":" + soundInfo.getProperty(IDs::soundId).toString();
                    actionListenerCallback(actionMessage);
                    nAlreadyDownloaded += 1;
                    
                } else {
                    if (soundInfo.hasProperty(IDs::format)){
                        typesParam = typesParam + soundInfo.getProperty(IDs::format).toString() + ",";
                    } else {
                        typesParam = typesParam + "not_available,";
                    }
                    if (soundInfo.hasProperty(IDs::filesize)){
                        sizesParam = sizesParam + soundInfo.getProperty(IDs::filesize).toString() + ",";
                    } else {
                        sizesParam = sizesParam + "0,";
                    }
                    oggUrlsParam = oggUrlsParam + soundInfo.getProperty(IDs::previewURL).toString() + ",";
                    idsParam = idsParam + soundInfo.getProperty(IDs::soundId).toString() + ",";
                    nSentToDownload += 1;
                }
                
                
            } else {
                
                // If sound does not have property STATE_SOUND_INFO_OGG_DOWNLOAD_URL, it means the
                // sound does not come from Freesound and therefore should not be downloaded
                // Trigger directly an action to indicate that the sound is ready to be loaded
                String actionMessage = String(ACTION_SOUND_READY_TO_LOAD) + ":" + (String)i;
                actionListenerCallback(actionMessage);
                nLocalSounds += 1;
            }
        }
    }
    url = url.withParameter("oggUrls", oggUrlsParam);
    url = url.withParameter("ids", idsParam);
    url = url.withParameter("types", typesParam);
    url = url.withParameter("sizes", sizesParam);
    url = url.withParameter("location", soundsDownloadLocation.getFullPathName());
    
    logToState("Downloading sounds...");
    logToState("- " + (String)nSentToDownload + " sounds sent to python server for downloading");
    logToState("- " + (String)nAlreadyDownloaded + " already downloaded and should not be downloaded");
    logToState("- " + (String)nLocalSounds + " sounds are local and should not be downloaded");
    
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
        if ((int)soundInfo.getProperty(IDs::downloadProgress) != 100){
            return false;
        }
    }
    return true;
}

void SourceSamplerAudioProcessor::setSingleSourceSamplerSoundObject(int soundIdx)
{
    // TODO: reimplement with new VT structure (probably does not need to be reimplemented or only partially)
    
    /*
    // Clear existing sound for this soundIdx in sampler (if any)
    sampler.clearSourceSamplerSoundByIdx(soundIdx);
    
    int nSounds = loadedSoundsInfo.getNumChildren();
    int nNotesPerSound = 128 / nSounds;  // Only used if we need to generate midiRootNote and midiNotes
    
    ValueTree soundInfo = loadedSoundsInfo.getChild(soundIdx);
    File audioSample = getSoundFileLocationToLoad(soundInfo);  // Get location of file to download. If original freesound is preset, take preference
    if (audioSample.exists() && audioSample.getSize() > 0){  // Check that file exists and is not empty
        logToState("- Adding sound " + audioSample.getFullPathName());
        
        // 1) Create SourceSamplerSound object and add to sampler
        std::unique_ptr<AudioFormatReader> reader(audioFormatManager.createReaderFor(audioSample));
        bool loadingPreviewVersion = audioSample.getFullPathName().indexOf("original") == -1;
        SourceSamplerSound* justAddedSound = static_cast<SourceSamplerSound*>(sampler.addSound(new SourceSamplerSound(ValueTree(), nullptr, *reader, loadingPreviewVersion, MAX_SAMPLE_LENGTH, getSampleRate(), getBlockSize()))); // Create sound (this sets idx property in the sound)
        
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
        logToState("- Skipping sound in index " + (String)soundIdx + " (no file found or file is empty)");
    }
     */
}


void SourceSamplerAudioProcessor::removeSound(int soundIndex)
{
    // TODO: reimplement with new VT structure
    
    /*
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
                    //sound->setIdx(sound->getSourceSound()->getIdx() - 1);
                }
            }
        } else {
            // Do nothing here, don't add the sound we are removing to new "loadedSoundsInfo" and also don't update the index of the sound in the sampler because it was already removed
        }
    }
    loadedSoundsInfo = newLoadedSoundsInfo;
    */
}

int SourceSamplerAudioProcessor::addOrReplaceSoundFromSoundInfoValueTree(int soundIndex, ValueTree newSoundInfo)
{
    // TODO: reimplement with new VT structure
    
    /*
    bool replacing = soundIndex > -1;  // If sound index > -1, we are replacing a sound, otherwise we're adding a new one
    
    if (replacing){
        // If replacing an existing sound, first we need to remove it from the sampelr, then update "loadedSoundsInfo" with the info from the new sound (and copying note assignment and midi root note information from the old sound if the new is not carrying it), and the trigger download of the new sound
        
        // Clear existing sound for this soundIdx in sampler (if any)
        sampler.clearSourceSamplerSoundByIdx(soundIndex);
        
        // Replace the corresponding position of the loadedSoundsInfo ValueTree with the new sound
        ValueTree newLoadedSoundsInfo = ValueTree(STATE_SOUNDS_INFO);
        for (int i=0; i<loadedSoundsInfo.getNumChildren(); i++){
            if (i == soundIndex){
                // Check if newSoundInfo has sotred sound sampler parameters, if not, create an empty value tree for them
                ValueTree samplerSoundParameters = newSoundInfo.getChildWithName(STATE_SAMPLER_SOUND);
                if (!samplerSoundParameters.isValid()){ samplerSoundParameters = ValueTree(STATE_SAMPLER_SOUND); }
                
                // If sampler sound parameters have no MIDI notes assigned, copy them form the sound we are replacing
                if (!samplerSoundParameters.hasProperty(STATE_SAMPLER_SOUND_MIDI_NOTES)){
                    ValueTree existingSoundInfo = loadedSoundsInfo.getChild(i);
                    samplerSoundParameters.setProperty(STATE_SAMPLER_SOUND_MIDI_NOTES,
                                                       existingSoundInfo.getChildWithName(STATE_SAMPLER_SOUND).getProperty(STATE_SAMPLER_SOUND_MIDI_NOTES)
                                                       , nullptr);
                }
                
                // If samplerSoundParameters have no midiRootNote sound parameter set, copy it from the existing sound
                ValueTree midiRootNoteParameterVT = samplerSoundParameters.getChildWithProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "midiRootNote");
                if (midiRootNoteParameterVT.isValid()){
                    int midiRootNote = (int)midiRootNoteParameterVT.getProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE);
                    samplerSoundParameters.appendChild(ValueTree(STATE_SAMPLER_SOUND_PARAMETER)
                        .setProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE, "int", nullptr)
                        .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "midiRootNote", nullptr)
                        .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, midiRootNote, nullptr),
                        nullptr);
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
    }*/
}

void SourceSamplerAudioProcessor::addOrReplaceSoundFromBasicSoundProperties(int soundIdx,
                                                                            int soundID,
                                                                            const String& soundName,
                                                                            const String& soundUser,
                                                                            const String& soundLicense,
                                                                            const String& oggDownloadURL,
                                                                            const String& localFilePath,
                                                                            const String& type,
                                                                            int sizeBytes,
                                                                            StringArray slices,
                                                                            BigInteger midiNotes,
                                                                            int midiRootNote,
                                                                            const String& triggerDownloadSoundAction)
{
    // TODO: VT refactor
    
    /*
    ValueTree soundInfo = ValueTree(STATE_SOUND_INFO);
    soundInfo.setProperty(STATE_SOUND_INFO_ID, soundID, nullptr);
    soundInfo.setProperty(STATE_SOUND_INFO_NAME, soundName, nullptr);
    soundInfo.setProperty(STATE_SOUND_INFO_USER, soundUser, nullptr);
    soundInfo.setProperty(STATE_SOUND_INFO_LICENSE, soundLicense, nullptr);
    soundInfo.setProperty(STATE_SOUND_INFO_SIZE, sizeBytes, nullptr);
    soundInfo.setProperty(STATE_SOUND_INFO_TYPE, type, nullptr);
    if (oggDownloadURL != ""){
        soundInfo.setProperty(STATE_SOUND_INFO_OGG_DOWNLOAD_URL, oggDownloadURL, nullptr);
    }
    if (localFilePath != ""){
        soundInfo.setProperty(STATE_SOUND_INFO_LOCAL_FILE_PATH, localFilePath, nullptr);
    }
    
    ValueTree samplerSoundParameters = ValueTree(STATE_SAMPLER_SOUND);
    
    if (!midiNotes.isZero()){
        samplerSoundParameters.setProperty(STATE_SAMPLER_SOUND_MIDI_NOTES, midiNotes.toString(16), nullptr);
    }
    
    if (midiRootNote >  -1){
        samplerSoundParameters.appendChild(ValueTree(STATE_SAMPLER_SOUND_PARAMETER)
            .setProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE, "int", nullptr)
            .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "midiRootNote", nullptr)
            .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, midiRootNote, nullptr),
            nullptr);
    }
    
    if (samplerSoundParameters.isValid()){
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
    }*/
}

void SourceSamplerAudioProcessor::reapplyNoteLayout(int newNoteLayoutType)
{
    // TODO: VT refactor
    
    /*
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
    }*/
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
            int midiChannel = sampler.getMidiInChannel();
            if (midiChannel == 0){
                midiChannel = 1; // If midi in is expected in all channels, set it to channel 1
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

void SourceSamplerAudioProcessor::valueTreePropertyChanged (juce::ValueTree& treeWhosePropertyHasChanged, const juce::Identifier& property)
{
    // We should never call this function from the realtime thread because editing VT might not be RT safe...
    // TODO: proper check that this is not audio thread
    //jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
    DBG("Changed " << treeWhosePropertyHasChanged[IDs::name].toString() << " " << property.toString() << ": " << treeWhosePropertyHasChanged[property].toString());
}

void SourceSamplerAudioProcessor::valueTreeChildAdded (juce::ValueTree& parentTree, juce::ValueTree& childWhichHasBeenAdded)
{
    // We should never call this function from the realtime thread because editing VT might not be RT safe...
    // TODO: proper check that this is not audio thread
    //jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
    DBG("Added VT child " << childWhichHasBeenAdded.getType());
}

void SourceSamplerAudioProcessor::valueTreeChildRemoved (juce::ValueTree& parentTree, juce::ValueTree& childWhichHasBeenRemoved, int indexFromWhichChildWasRemoved)
{
    // We should never call this function from the realtime thread because editing VT might not be RT safe...
    // TODO: proper check that this is not audio thread
    //jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
    DBG("Removed VT child " << childWhichHasBeenRemoved.getType());
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
