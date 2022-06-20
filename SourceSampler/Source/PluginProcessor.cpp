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
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ),
    queryMakerThread (*this),
    serverInterface ([this]{return getGlobalContext();})
#endif
{
    std::cout << "Creating needed directories" << std::endl;
    createDirectories();
    
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
    state = Helpers::createDefaultEmptyState();
    
    // Add state change listener and bind cached properties to state properties (including loaded sounds)
    bindState();
    
    // Load global settings and do extra configuration
    std::cout << "Loading global settings" << std::endl;
    loadGlobalPersistentStateFromFile();
    
    // Notify that plugin is running
    #if SYNC_STATE_WITH_OSC
    sendOSCMessage(juce::OSCMessage("/plugin_started"));
    #endif
    sendWSMessage(juce::OSCMessage("/plugin_started"));
    
    std::cout << "SOURCE plugin is up and running!" << std::endl;
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
    state.setProperty(IDs::pluginVersion, juce::String(JucePlugin_VersionString), nullptr);
    
    Helpers::addPropertyWithDefaultValueIfNotExisting(state, IDs::currentPresetIndex, Defaults::currentPresetIndex);
    currentPresetIndex.referTo(state, IDs::currentPresetIndex, nullptr, Defaults::currentPresetIndex);
    Helpers::addPropertyWithDefaultValueIfNotExisting(state, IDs::globalMidiInChannel, Defaults::globalMidiInChannel);
    globalMidiInChannel.referTo(state, IDs::globalMidiInChannel, nullptr, Defaults::globalMidiInChannel);
    Helpers::addPropertyWithDefaultValueIfNotExisting(state, IDs::midiOutForwardsMidiIn, Defaults::midiOutForwardsMidiIn);
    midiOutForwardsMidiIn.referTo(state, IDs::midiOutForwardsMidiIn, nullptr, Defaults::midiOutForwardsMidiIn);
    Helpers::addPropertyWithDefaultValueIfNotExisting(state, IDs::useOriginalFilesPreference, Defaults::currentPresetIndex);
    useOriginalFilesPreference.referTo(state, IDs::useOriginalFilesPreference, nullptr, Defaults::useOriginalFilesPreference);
    
    juce::ValueTree preset = state.getChildWithName(IDs::PRESET);
    Helpers::addPropertyWithDefaultValueIfNotExisting(preset, IDs::numVoices, Defaults::numVoices);
    numVoices.referTo(preset, IDs::numVoices, nullptr, Defaults::numVoices);
    Helpers::addPropertyWithDefaultValueIfNotExisting(preset, IDs::name, Helpers::defaultPresetName());
    presetName.referTo(preset, IDs::name, nullptr, Helpers::defaultPresetName());
    Helpers::addPropertyWithDefaultValueIfNotExisting(preset, IDs::noteLayoutType, Defaults::noteLayoutType);
    noteLayoutType.referTo(preset, IDs::noteLayoutType, nullptr, Defaults::noteLayoutType);
    Helpers::addPropertyWithDefaultValueIfNotExisting(preset, IDs::reverbRoomSize, Defaults::reverbRoomSize);
    reverbRoomSize.referTo(preset, IDs::reverbRoomSize, nullptr, Defaults::reverbRoomSize);
    Helpers::addPropertyWithDefaultValueIfNotExisting(preset, IDs::reverbDamping, Defaults::reverbDamping);
    reverbDamping.referTo(preset, IDs::reverbDamping, nullptr, Defaults::reverbDamping);
    Helpers::addPropertyWithDefaultValueIfNotExisting(preset, IDs::reverbWetLevel, Defaults::reverbWetLevel);
    reverbWetLevel.referTo(preset, IDs::reverbWetLevel, nullptr, Defaults::reverbWetLevel);
    Helpers::addPropertyWithDefaultValueIfNotExisting(preset, IDs::reverbDryLevel, Defaults::reverbDryLevel);
    reverbDryLevel.referTo(preset, IDs::reverbDryLevel, nullptr, Defaults::reverbDryLevel);
    Helpers::addPropertyWithDefaultValueIfNotExisting(preset, IDs::reverbWidth, Defaults::reverbWidth);
    reverbWidth.referTo(preset, IDs::reverbWidth, nullptr, Defaults::reverbWidth);
    Helpers::addPropertyWithDefaultValueIfNotExisting(preset, IDs::reverbFreezeMode, Defaults::reverbFreezeMode);
    reverbFreezeMode.referTo(preset, IDs::reverbFreezeMode, nullptr, Defaults::reverbFreezeMode);
    
    // Swap pointer with oldSound so if there were objects in there still pending to be safely deleted, these will be
    // deleted when needed. Then create a new SourceSoundList with the new preset information
    soundsOld.swap(sounds);
    sounds.reset();
    sounds = std::make_unique<SourceSoundList>(state.getChildWithName(IDs::PRESET), [this]{return getGlobalContext();});
}

void SourceSamplerAudioProcessor::createDirectories()
{
    #if ELK_BUILD
    sourceDataLocation = juce::File(ELK_SOURCE_DATA_BASE_LOCATION);
    soundsDownloadLocation = juce::File(ELK_SOURCE_SOUNDS_LOCATION);
    presetFilesLocation = juce::File(ELK_SOURCE_PRESETS_LOCATION);
    tmpFilesLocation = juce::File(ELK_SOURCE_TMP_LOCATION);
    #else
    sourceDataLocation = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("SourceSampler/");
    soundsDownloadLocation = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("SourceSampler/sounds");
    presetFilesLocation = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("SourceSampler/presets");
    tmpFilesLocation = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("SourceSampler/tmp");
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
    return currentPresetIndex;
}

void SourceSamplerAudioProcessor::setCurrentProgram (int index)
{
    // Load the preset from the file, this will also clear any existing sounds
    bool loaded = loadPresetFromFile(getPresetFilenameByIndex(index));
    if (loaded){
        // If file was loaded, update the current present index to the new one
        currentPresetIndex = index;
    } else {
        // If no file was loaded (no file found or errors ocurred), create a new empty preset
        DBG("Creating new empty preset");
        juce::ValueTree newState = Helpers::createNewStateFromCurrentSatate(state);
        juce::ValueTree newPresetState = Helpers::createEmptyPresetState();
        newState.addChild(newPresetState, -1, nullptr);
        loadPresetFromStateInformation(newState);
        currentPresetIndex = index;
    }
    saveGlobalPersistentStateToFile(); // Save global settings to file (which inlucdes the latest loaded preset index)
    
    // Trigger action to re-send full state to UI clients
    actionListenerCallback(juce::String(ACTION_GET_STATE) + juce::String(":full"));
}

const juce::String SourceSamplerAudioProcessor::getProgramName (int index)
{
    juce::File location = getPresetFilePath(getPresetFilenameByIndex(index));
    if (location.existsAsFile()){
        juce::XmlDocument xmlDocument (location);
        std::unique_ptr<juce::XmlElement> xmlState = xmlDocument.getDocumentElement();
        if (xmlState.get() != nullptr){
            juce::ValueTree state = juce::ValueTree::fromXml(*xmlState.get());
            if (state.hasProperty(IDs::name)){
                return state.getProperty(IDs::name).toString();
            }
        }
    }
    return {};
}

void SourceSamplerAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::File location = getPresetFilePath(getPresetFilenameByIndex(index));
    if (location.existsAsFile()){
        juce::XmlDocument xmlDocument (location);
        std::unique_ptr<juce::XmlElement> xmlState = xmlDocument.getDocumentElement();
        if (xmlState.get() != nullptr){
            juce::ValueTree state = juce::ValueTree::fromXml(*xmlState.get());
            state.setProperty(IDs::name, newName, nullptr);
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

juce::String SourceSamplerAudioProcessor::getPresetFilenameByIndex(int index)
{
    return (juce::String)index + ".xml";
}

//==============================================================================
void SourceSamplerAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    DBG("Called prepareToPlay with sampleRate " + (juce::String)sampleRate + " and block size " + (juce::String)samplesPerBlock);
    
    // Prepare sampler
    sampler.prepare ({ sampleRate, (juce::uint32) samplesPerBlock, 2 });
    
    // Prepare preview player
    transportSource.prepareToPlay (samplesPerBlock, sampleRate);
    
    // Configure level measurer
    lms.resize(getTotalNumOutputChannels(), 200 * 0.001f * sampleRate / samplesPerBlock); // 200ms average window
    
    // Loaded the last loaded preset (only in ELK platform)
    # if ELK_BUILD
    if (!loadedPresetAtElkStartup){
        std::cout << "Loading latest loaded preset " << (juce::String)latestLoadedPreset << std::endl;
            setCurrentProgram(latestLoadedPreset);
        loadedPresetAtElkStartup = true;
    }
    # endif
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
bool SourceSamplerAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* SourceSamplerAudioProcessor::createEditor()
{
    return new SourceSamplerAudioProcessorEditor (*this);
}

//==============================================================================

void SourceSamplerAudioProcessor::saveCurrentPresetToFile (const juce::String& _presetName, int index)
{
    juce::ValueTree presetState = state.getChildWithName(IDs::PRESET);
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

bool SourceSamplerAudioProcessor::loadPresetFromFile (const juce::String& fileName)
{
    juce::File location = getPresetFilePath(fileName);
    if (location.existsAsFile()){
        juce::XmlDocument xmlDocument (location);
        std::unique_ptr<juce::XmlElement> xmlState = xmlDocument.getDocumentElement();
        if (xmlState.get() != nullptr){
            juce::ValueTree newState = Helpers::createNewStateFromCurrentSatate(state);
            juce::ValueTree presetState = juce::ValueTree::fromXml(*xmlState.get());
            // Check if preset has old format, and if so transform it to the new format
            // In old format, root preset node type is "SourcePresetState", in the new one it is "PRESET"
            if (presetState.getType().toString() == "SourcePresetState"){
                DBG("Old preset file found, transforming to new format");
                juce::ValueTree modifiedPresetState = Helpers::createEmptyPresetState();
                // Preset is of new format, we can add it to the new state without modification
                modifiedPresetState.setProperty(IDs::name, presetState.getProperty("presetName", "old preset no name"), nullptr);
                modifiedPresetState.setProperty(IDs::noteLayoutType, presetState.getProperty("noteLayoutType", Defaults::noteLayoutType), nullptr);
                juce::ValueTree samplerState = presetState.getChildWithName("Sampler");
                if (samplerState.isValid()){
                    modifiedPresetState.setProperty(IDs::numVoices, samplerState.getProperty("NumVoices", Defaults::numVoices), nullptr);
                }
                juce::ValueTree reverbParamsState = samplerState.getChildWithName("ReverbParameters");
                if (reverbParamsState.isValid()){
                    modifiedPresetState.setProperty(IDs::reverbDamping, reverbParamsState.getProperty("reverb_damping", Defaults::reverbDamping), nullptr);
                    modifiedPresetState.setProperty(IDs::reverbWetLevel, reverbParamsState.getProperty("reverb_wetLevel", Defaults::reverbWetLevel), nullptr);
                    modifiedPresetState.setProperty(IDs::reverbDryLevel, reverbParamsState.getProperty("reverb_dryLevel", Defaults::reverbDryLevel), nullptr);
                    modifiedPresetState.setProperty(IDs::reverbWidth, reverbParamsState.getProperty("reverb_width", Defaults::reverbWidth), nullptr);
                    modifiedPresetState.setProperty(IDs::reverbFreezeMode, reverbParamsState.getProperty("reverb_freezeMode", Defaults::reverbFreezeMode), nullptr);
                    modifiedPresetState.setProperty(IDs::reverbRoomSize, reverbParamsState.getProperty("reverb_roomSize", Defaults::reverbRoomSize), nullptr);
                }
                juce::ValueTree soundsInfoState = presetState.getChildWithName("soundsInfo");
                for (int i=0; i<soundsInfoState.getNumChildren(); i++){
                    auto soundInfo = soundsInfoState.getChild(i);
                    auto samplerSound = soundInfo.getChildWithName("SamplerSound");
                    juce::BigInteger midiNotes = 0;
                    midiNotes.parseString(samplerSound.getProperty("midiNotes", Defaults::midiNotes).toString(), 16);
                    juce::ValueTree sound = Helpers::createSourceSoundAndSourceSamplerSoundFromProperties((int)soundInfo.getProperty("soundId"), soundInfo.getProperty("soundName"), soundInfo.getProperty("soundUser"), soundInfo.getProperty("soundLicense"), soundInfo.getProperty("soundOGGURL"), "", "", -1, {}, midiNotes, (int)samplerSound.getChildWithProperty("parameter_name", "midiRootNote").getProperty("parameter_value"), 0);
                    sound.getChildWithName(IDs::SOUND_SAMPLE).setProperty(IDs::usesPreview, true, nullptr);
                    
                    sound.setProperty(IDs::launchMode, (int)samplerSound.getChildWithProperty("parameter_name", "launchMode").getProperty("parameter_value"), nullptr);
                    sound.setProperty(IDs::startPosition, (float)samplerSound.getChildWithProperty("parameter_name", "startPosition").getProperty("parameter_value"), nullptr);
                    sound.setProperty(IDs::endPosition, (float)samplerSound.getChildWithProperty("parameter_name", "endPosition").getProperty("parameter_value"), nullptr);
                    sound.setProperty(IDs::loopStartPosition, (float)samplerSound.getChildWithProperty("parameter_name", "loopStartPosition").getProperty("parameter_value"), nullptr);
                    sound.setProperty(IDs::loopEndPosition, (float)samplerSound.getChildWithProperty("parameter_name", "loopEndPosition").getProperty("parameter_value"), nullptr);
                    sound.setProperty(IDs::loopXFadeNSamples, (int)samplerSound.getChildWithProperty("parameter_name", "loopXFadeNSamples").getProperty("parameter_value"), nullptr);
                    sound.setProperty(IDs::reverse, (int)samplerSound.getChildWithProperty("parameter_name", "reverse").getProperty("parameter_value"), nullptr);
                    sound.setProperty(IDs::noteMappingMode, (int)samplerSound.getChildWithProperty("parameter_name", "noteMappingMode").getProperty("parameter_value"), nullptr);
                    sound.setProperty(IDs::numSlices, (int)samplerSound.getChildWithProperty("parameter_name", "numSlices").getProperty("parameter_value"), nullptr);
                    sound.setProperty(IDs::playheadPosition, (float)samplerSound.getChildWithProperty("parameter_name", "playheadPosition").getProperty("parameter_value"), nullptr);
                    sound.setProperty(IDs::freezePlayheadSpeed, (float)samplerSound.getChildWithProperty("parameter_name", "freezePlayheadSpeed").getProperty("parameter_value"), nullptr);
                    sound.setProperty(IDs::filterCutoff, (float)samplerSound.getChildWithProperty("parameter_name", "filterCutoff").getProperty("parameter_value"), nullptr);
                    sound.setProperty(IDs::filterRessonance, (float)samplerSound.getChildWithProperty("parameter_name", "filterRessonance").getProperty("parameter_value"), nullptr);
                    sound.setProperty(IDs::filterKeyboardTracking, (float)samplerSound.getChildWithProperty("parameter_name", "filterKeyboardTracking").getProperty("parameter_value"), nullptr);
                    sound.setProperty(IDs::filterAttack, (float)samplerSound.getChildWithProperty("parameter_name", "filterADSR.attack").getProperty("parameter_value"), nullptr);
                    sound.setProperty(IDs::filterDecay, (float)samplerSound.getChildWithProperty("parameter_name", "filterADSR.decay").getProperty("parameter_value"), nullptr);
                    sound.setProperty(IDs::filterSustain, (float)samplerSound.getChildWithProperty("parameter_name", "filterADSR.sustain").getProperty("parameter_value"), nullptr);
                    sound.setProperty(IDs::filterRelease, (float)samplerSound.getChildWithProperty("parameter_name", "filterADSR.release").getProperty("parameter_value"), nullptr);
                    sound.setProperty(IDs::filterADSR2CutoffAmt, (float)samplerSound.getChildWithProperty("parameter_name", "filterADSR2CutoffAmt").getProperty("parameter_value"), nullptr);
                    sound.setProperty(IDs::gain, (float)samplerSound.getChildWithProperty("parameter_name", "gain").getProperty("parameter_value"), nullptr);
                    sound.setProperty(IDs::attack, (float)samplerSound.getChildWithProperty("parameter_name", "ampADSR.attack").getProperty("parameter_value"), nullptr);
                    sound.setProperty(IDs::decay, (float)samplerSound.getChildWithProperty("parameter_name", "ampADSR.decay").getProperty("parameter_value"), nullptr);
                    sound.setProperty(IDs::sustain, (float)samplerSound.getChildWithProperty("parameter_name", "ampADSR.sustain").getProperty("parameter_value"), nullptr);
                    sound.setProperty(IDs::release, (float)samplerSound.getChildWithProperty("parameter_name", "ampADSR.release").getProperty("parameter_value"), nullptr);
                    sound.setProperty(IDs::pan, (float)samplerSound.getChildWithProperty("parameter_name", "pan").getProperty("parameter_value"), nullptr);
                    sound.setProperty(IDs::pitch, (float)samplerSound.getChildWithProperty("parameter_name", "pitch").getProperty("parameter_value"), nullptr);
                    sound.setProperty(IDs::pitchBendRangeUp, (float)samplerSound.getChildWithProperty("parameter_name", "pitchBendRangeUp").getProperty("parameter_value"), nullptr);
                    sound.setProperty(IDs::pitchBendRangeDown, (float)samplerSound.getChildWithProperty("parameter_name", "pitchBendRangeDown").getProperty("parameter_value"), nullptr);
                    sound.setProperty(IDs::mod2CutoffAmt, (float)samplerSound.getChildWithProperty("parameter_name", "mod2CutoffAmt").getProperty("parameter_value"), nullptr);
                    sound.setProperty(IDs::mod2GainAmt, (float)samplerSound.getChildWithProperty("parameter_name", "mod2GainAmt").getProperty("parameter_value"), nullptr);
                    sound.setProperty(IDs::mod2PitchAmt, (float)samplerSound.getChildWithProperty("parameter_name", "mod2PitchAmt").getProperty("parameter_value"), nullptr);
                    sound.setProperty(IDs::mod2PlayheadPos, (float)samplerSound.getChildWithProperty("parameter_name", "mod2PlayheadPos").getProperty("parameter_value"), nullptr);
                    sound.setProperty(IDs::vel2CutoffAmt, (float)samplerSound.getChildWithProperty("parameter_name", "vel2CutoffAmt").getProperty("parameter_value"), nullptr);
                    sound.setProperty(IDs::vel2GainAmt, (float)samplerSound.getChildWithProperty("parameter_name", "vel2GainAmt").getProperty("parameter_value"), nullptr);
                    
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

void SourceSamplerAudioProcessor::loadPresetFromStateInformation (juce::ValueTree _state)
{
    // If sounds are currently loaded in the state, remove them all
    // This will trigger the deletion of SampleSound and SourceSamplerSound objects
    juce::ValueTree presetState = state.getChildWithName(IDs::PRESET);
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

void SourceSamplerAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // Save current state information to memory block
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    DBG("> Running getStateInformation");
    DBG(xml->toString()); // Print state for debugging purposes (print it nicely indented)
    copyXmlToBinary (*xml, destData);
}

void SourceSamplerAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // Called by the plugin host to load state stored in host into plugin
    DBG("> Running setStateInformation");
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr){
        loadPresetFromStateInformation(juce::ValueTree::fromXml(*xmlState.get()));
    }
}

void SourceSamplerAudioProcessor::saveGlobalPersistentStateToFile()
{
    // This is to save settings that need to persist between sampler runs and that do not
    // change per preset
    
    juce::ValueTree settings = juce::ValueTree(IDs::GLOBAL_SETTINGS);
    settings.setProperty(IDs::globalMidiInChannel, globalMidiInChannel.get(), nullptr);
    settings.setProperty(IDs::midiOutForwardsMidiIn, midiOutForwardsMidiIn.get(), nullptr);
    settings.setProperty(IDs::latestLoadedPresetIndex, currentPresetIndex.get(), nullptr);
    settings.setProperty(IDs::useOriginalFilesPreference, useOriginalFilesPreference.get(), nullptr);
    settings.setProperty(IDs::pluginVersion, juce::String(JucePlugin_VersionString), nullptr);
    
    std::unique_ptr<juce::XmlElement> xml (settings.createXml());
    juce::File location = getGlobalSettingsFilePathFromName();
    if (location.existsAsFile()){
        // If already exists, delete it
        location.deleteFile();
    }
    xml->writeTo(location);
}

void SourceSamplerAudioProcessor::loadGlobalPersistentStateFromFile()
{
    juce::File location = getGlobalSettingsFilePathFromName();
    if (location.existsAsFile()){
        juce::XmlDocument xmlDocument (location);
        std::unique_ptr<juce::XmlElement> xmlState = xmlDocument.getDocumentElement();
        if (xmlState.get() != nullptr){
            juce::ValueTree settings = juce::ValueTree::fromXml(*xmlState.get());
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


juce::File SourceSamplerAudioProcessor::getPresetFilePath(const juce::String& presetFilename)
{
    return presetFilesLocation.getChildFile(presetFilename).withFileExtension("xml");
}


juce::String SourceSamplerAudioProcessor::getPresetFilenameFromNameAndIndex(const juce::String& presetName, int index)
{
    return (juce::String)index; // Only use index as filename
}


juce::File SourceSamplerAudioProcessor::getGlobalSettingsFilePathFromName()
{
    return sourceDataLocation.getChildFile("settings").withFileExtension("xml");
}

juce::ValueTree SourceSamplerAudioProcessor::collectVolatileStateInformation (){
    juce::ValueTree state = juce::ValueTree(IDs::VOLATILE_STATE);
    state.setProperty(IDs::isQuerying, isQuerying, nullptr);
    state.setProperty(IDs::midiInLastStateReportBlock, midiMessagesPresentInLastStateReport, nullptr);
    midiMessagesPresentInLastStateReport = false;
    state.setProperty(IDs::lastMIDICCNumber, lastReceivedMIDIControllerNumber, nullptr);
    state.setProperty(IDs::lastMIDINoteNumber, lastReceivedMIDINoteNumber, nullptr);
    
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
    
    state.setProperty(IDs::voiceActivations, voiceActivations, nullptr);
    state.setProperty(IDs::voiceSoundIdxs, voiceSoundIdxs, nullptr);
    state.setProperty(IDs::voiceSoundPlayPosition, voiceSoundPlayPositions, nullptr);
    
    juce::String audioLevels = "";
    for (int i=0; i<getTotalNumOutputChannels(); i++){
        audioLevels += (juce::String)lms.getRMSLevel(i) + ",";
    }
    state.setProperty(IDs::audioLevels, audioLevels, nullptr);
    return state;
}

juce::String SourceSamplerAudioProcessor::collectVolatileStateInformationAsString(){
    
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


void SourceSamplerAudioProcessor::updateReverbParameters()
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

void SourceSamplerAudioProcessor::actionListenerCallback (const juce::String &message)
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
        setCurrentProgram(index);
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
        int midiVelocityLayer = Defaults::midiVelocityLayer;
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
        int midiVelocityLayer = Defaults::midiVelocityLayer;
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

void SourceSamplerAudioProcessor::setGlobalMidiInChannel(int channel)
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

void SourceSamplerAudioProcessor::makeQueryAndLoadSounds(const juce::String& addReplaceOrReplaceSound, const juce::String& textQuery, int numSounds, float minSoundLength, float maxSoundLength)
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
    state.setProperty(IDs::numResultsLastQuery, -1, nullptr);
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
    state.setProperty(IDs::numResultsLastQuery, numResults, nullptr);
    isQuerying = false;
    if (numResults > 0){
        
        // Randmomize results and prepare for iteration
        juce::Array<FSSound> soundsFound = list.toArrayOfSounds();
        DBG("Query got " + (juce::String)list.getCount() + " results, " + (juce::String)soundsFound.size() + " in the first page. Will load " + (juce::String)numSounds + " sounds.");
        //unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
        //std::shuffle(soundsFound.begin(), soundsFound.end(), std::default_random_engine(seed));
        soundsFound.resize(juce::jmin(numSounds, list.getCount()));
        int nSounds = soundsFound.size();
        
        // Move this elsewhere in a helper function (?)
        juce::ValueTree presetState = state.getChildWithName(IDs::PRESET);
        
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
            } else {
                // If a specific sound is being replaced, no need to calcualte any mapping here because it will be copied from the
                // sound being replaced in addOrReplaceSoundFromBasicSoundProperties
            }
            
            // Create sound
            // soundUUIDToReplace will be "" when we're adding or replacing all sounds, otherwise it will be the UUID of the sound to replace
            int midiVelocityLayer = Defaults::midiVelocityLayer; // Always use default velocity layer as we're not layering sounds
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


void SourceSamplerAudioProcessor::removeSound(const juce::String& soundUUID)
{
    // Trigger the deletion of the sound by disabling it
    // Once disabled, all playing notes will be stopped and the sound removed a while after that
    const juce::ScopedLock sl (soundDeleteLock);
    auto* sound = sounds->getSoundWithUUID(soundUUID);
    if (sound != nullptr){
        sound->scheduleSoundDeletion();
    }
}

void SourceSamplerAudioProcessor::removeSamplerSound(const juce::String& soundUUID, const juce::String& samplerSoundUUID)
{
    // Trigger the deletion of the sound by disabling it
    // Once disabled, all playing notes will be stopped and the sound removed a while after that
    const juce::ScopedLock sl (soundDeleteLock);
    auto* sound = sounds->getSoundWithUUID(soundUUID);
    if (sound != nullptr){
        sound->scheduleDeletionOfSourceSamplerSound(samplerSoundUUID);
    }
}


void SourceSamplerAudioProcessor::removeAllSounds()
{
    // Trigger the deletion of the sounds by disabling them
    // Once disabled, all playing notes will be stopped and the sounds removed a while after that
    const juce::ScopedLock sl (soundDeleteLock);
    for (auto* sound: sounds->objects){
        sound->scheduleSoundDeletion();
    }
}

void SourceSamplerAudioProcessor::addOrReplaceSoundFromBasicSoundProperties(const juce::String& soundUUID,
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
        
        juce::ValueTree sourceSound = Helpers::createSourceSoundAndSourceSamplerSoundFromProperties(soundID, soundName, soundUser, soundLicense, previewURL, localFilePath, format, sizeBytes, slices, midiNotes, midiRootNote, midiVelocityLayer);
        
        juce::ValueTree presetState = state.getChildWithName(IDs::PRESET);
        if (existingSoundIndex > -1){
            // If replacing an existing sound, copy as well the midi mappings
            juce::ValueTree soundToReplaceState = presetState.getChildWithProperty(IDs::uuid, soundUUID);
            for (int i=0; i<soundToReplaceState.getNumChildren(); i++){
                auto child = soundToReplaceState.getChild(i);
                if (child.hasType(IDs::MIDI_CC_MAPPING)){
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
            juce::ValueTree sourceSamplerSound = Helpers::createSourceSampleSoundState(soundID, soundName, soundUser, soundLicense, previewURL, localFilePath, format, sizeBytes, slices, midiRootNote, midiVelocityLayer);
            sound->addNewSourceSamplerSoundFromValueTree(sourceSamplerSound);
        }
    }
}

void SourceSamplerAudioProcessor::addOrReplaceSoundFromBasicSoundProperties(const juce::String& soundUUID,
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
        midiNoteForNormalPitch = sound->getFirstLinkedSourceSamplerSound()->getMidiRootNote();
        juce::BigInteger assignedMidiNotes = sound->getMappedMidiNotes();
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
            juce::MidiMessage message = juce::MidiMessage::noteOn(midiChannel, midiNoteForNormalPitch, (juce::uint8)127);
            if (doNoteOff){
                message = juce::MidiMessage::noteOff(midiChannel, midiNoteForNormalPitch, (juce::uint8)127);
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


int SourceSamplerAudioProcessor::getServerInterfaceHttpPort()
{
    #if USE_HTTP_SERVER
    return serverInterface.httpServer.assignedPort;
    #else
    return 0;
    #endif
}

int SourceSamplerAudioProcessor::getServerInterfaceWSPort()
{
    return serverInterface.wsServer.assignedPort;
}


void SourceSamplerAudioProcessor::previewFile(const juce::String& path)
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

void SourceSamplerAudioProcessor::stopPreviewingFile(){
    if (transportSource.isPlaying()){
        transportSource.stop();
    }
}


//==============================================================================

void SourceSamplerAudioProcessor::sendOSCMessage(const juce::OSCMessage& message)
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

void SourceSamplerAudioProcessor::sendWSMessage(const juce::OSCMessage& message)
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

void SourceSamplerAudioProcessor::valueTreePropertyChanged (juce::ValueTree& treeWhosePropertyHasChanged, const juce::Identifier& property)
{
    // We should never call this function from the realtime thread because editing VT might not be RT safe...
    // TODO: proper check that this is not audio thread
    //jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
    DBG("Changed " << treeWhosePropertyHasChanged[IDs::name].toString() << " " << property.toString() << ": " << treeWhosePropertyHasChanged[property].toString());
    juce::OSCMessage message = juce::OSCMessage("/state_update");
    message.addString("propertyChanged");
    message.addInt32(stateUpdateID);
    message.addString(treeWhosePropertyHasChanged[IDs::uuid].toString());
    message.addString(treeWhosePropertyHasChanged.getType().toString());
    message.addString(property.toString());
    message.addString(treeWhosePropertyHasChanged[property].toString());
    #if SYNC_STATE_WITH_OSC
    sendOSCMessage(message);
    #endif
    sendWSMessage(message);
    stateUpdateID += 1;
}

void SourceSamplerAudioProcessor::valueTreeChildAdded (juce::ValueTree& parentTree, juce::ValueTree& childWhichHasBeenAdded)
{
    // We should never call this function from the realtime thread because editing VT might not be RT safe...
    // TODO: proper check that this is not audio thread
    //jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
    DBG("Added VT child " << childWhichHasBeenAdded.getType());
    juce::OSCMessage message = juce::OSCMessage("/state_update");
    message.addString("addedChild");
    message.addInt32(stateUpdateID);
    message.addString(parentTree[IDs::uuid].toString());
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

void SourceSamplerAudioProcessor::valueTreeChildRemoved (juce::ValueTree& parentTree, juce::ValueTree& childWhichHasBeenRemoved, int indexFromWhichChildWasRemoved)
{
    // We should never call this function from the realtime thread because editing VT might not be RT safe...
    // TODO: proper check that this is not audio thread
    //jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
    DBG("Removed VT child " << childWhichHasBeenRemoved.getType());
    juce::OSCMessage message = juce::OSCMessage("/state_update");
    message.addString("removedChild");
    message.addInt32(stateUpdateID);
    message.addString(childWhichHasBeenRemoved[IDs::uuid].toString());
    message.addString(childWhichHasBeenRemoved.getType().toString());
    #if SYNC_STATE_WITH_OSC
    sendOSCMessage(message);
    #endif
    sendWSMessage(message);
    stateUpdateID += 1;
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
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SourceSamplerAudioProcessor();
}
