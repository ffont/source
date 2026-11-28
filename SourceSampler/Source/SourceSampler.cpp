/*
  ==============================================================================

    SourceSampler.cpp
    Created: 13 Jan 2023 1:41:00pm
    Author:  Frederic Font Corbera

  ==============================================================================
*/

#include "SourceSampler.h"
#ifndef FREESOUND_API_KEY
#include "api_key.h"
#endif
#include <chrono>
#include <climits>  // for using INT_MAX
#include <random> // for using shuffle


//==============================================================================
SourceSampler::SourceSampler(juce::File baseDataLocation):
    queryMakerThread (*this),
    freesoundSearch (FREESOUND_API_KEY),
    serverInterface ([this]{return getGlobalContext();})
{
    weakThis = this;
    
    DBG("Creating needed directories");
    createDirectories(SOURCE_APP_DIRECTORY_NAME, baseDataLocation);
    fixFreesoundPreviewURLsInPresetFiles();
    
    std::cout << "Configuring app" << std::endl;
    if(audioFormatManager.getNumKnownFormats() == 0){ audioFormatManager.registerBasicFormats(); }
    
    startTime = juce::Time::getMillisecondCounterHiRes() * 0.001;
    
    // Action listeners
    serverInterface.addActionListener(this);
    
    // Start timer to run periodic tasks like deleting sounds that are waiting to be deleted
    startTimerHz(MAIN_TIMER_HZ);
    
    // Start timer that applies MIDI CC mappings and program changes received in the audio thread
    controlEventsTimer.startTimerHz(CONTROL_EVENTS_TIMER_HZ);
    
    // Load empty session to state
    DBG("Creating default empty state");
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
    
    DBG("SOURCE sampler is up and running!");
}

SourceSampler::~SourceSampler()
{
    stopTimer();
    controlEventsTimer.stopTimer();
    queryMakerThread.stopThread(5000);
    
    // Save current global persistent state (global settings)
    saveGlobalPersistentStateToFile();
    if (sessionNeedsSaving)
        saveSessionNow();
    
    // Remove listeners
    serverInterface.removeActionListener(this);
    
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
    state.setProperty(SourceIDs::pluginVersion, juce::String(ProjectInfo::versionString), nullptr);
    
    SourceHelpers::addPropertyWithDefaultValueIfNotExisting(state, SourceIDs::currentPresetIndex, SourceDefaults::currentPresetIndex);
    currentPresetIndex.referTo(state, SourceIDs::currentPresetIndex, nullptr, SourceDefaults::currentPresetIndex);
    SourceHelpers::addPropertyWithDefaultValueIfNotExisting(state, SourceIDs::globalMidiInChannel, SourceDefaults::globalMidiInChannel);
    globalMidiInChannel.referTo(state, SourceIDs::globalMidiInChannel, nullptr, SourceDefaults::globalMidiInChannel);
    SourceHelpers::addPropertyWithDefaultValueIfNotExisting(state, SourceIDs::midiOutForwardsMidiIn, SourceDefaults::midiOutForwardsMidiIn);
    midiOutForwardsMidiIn.referTo(state, SourceIDs::midiOutForwardsMidiIn, nullptr, SourceDefaults::midiOutForwardsMidiIn);
    SourceHelpers::addPropertyWithDefaultValueIfNotExisting(state, SourceIDs::useOriginalFilesPreference, SourceDefaults::useOriginalFilesPreference);
    useOriginalFilesPreference.referTo(state, SourceIDs::useOriginalFilesPreference, nullptr, SourceDefaults::useOriginalFilesPreference);
    SourceHelpers::addPropertyWithDefaultValueIfNotExisting(state, SourceIDs::freesoundOauthAccessToken, SourceDefaults::freesoundOauthAccessToken);
    freesoundOauthAccessToken.referTo(state, SourceIDs::freesoundOauthAccessToken, nullptr, SourceDefaults::freesoundOauthAccessToken);
    SourceHelpers::addPropertyWithDefaultValueIfNotExisting(state, SourceIDs::mpeEnabled, SourceDefaults::mpeEnabled);
    mpeEnabled.referTo(state, SourceIDs::mpeEnabled, nullptr, SourceDefaults::mpeEnabled);
    SourceHelpers::addPropertyWithDefaultValueIfNotExisting(state, SourceIDs::mpePitchBendRange, SourceDefaults::mpePitchBendRange);
    mpePitchBendRange.referTo(state, SourceIDs::mpePitchBendRange, nullptr, SourceDefaults::mpePitchBendRange);
    SourceHelpers::addPropertyWithDefaultValueIfNotExisting(state, SourceIDs::bpm, (float)SourceDefaults::bpm);
    bpm.referTo(state, SourceIDs::bpm, nullptr, SourceDefaults::bpm);
    
    // Load global settings stored in file, now before sounds are created as these might need the oauth token
    loadGlobalPersistentStateFromFile();
    sampler.setGlobalMidiInChannel(globalMidiInChannel.get());
    sampler.setMpeSettings(mpeEnabled.get(), mpePitchBendRange.get());
    globalBpm.store(bpm.get());
    setCurrentTempo(bpm.get(), TempoSource::internal);
    
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
    
    // Effects (presets without them get the defaults, with all effects off). Changes to these properties are passed
    // to the effects chain in valueTreePropertyChanged.
    for (size_t i = 0; i < SourceFx::getParamInfos().size(); ++i){
        const auto& info = SourceFx::getParamInfos()[i];
        SourceHelpers::addPropertyWithDefaultValueIfNotExisting(preset, info.getIdentifier(), info.defaultValue);
        sampler.setFxParameter((SourceFx::Param) i, (float) preset.getProperty(info.getIdentifier()));
    }
    
    // Swap pointer with oldSound so if there were objects in there still pending to be safely deleted, these will be
    // deleted when needed. Then create a new SourceSoundList with the new preset information
    soundsOld.swap(sounds);
    sounds.reset();
    sounds = std::make_unique<SourceSoundList>(state.getChildWithName(SourceIDs::PRESET), [this]{return getGlobalContext();});
}

void SourceSampler::createDirectories(const juce::String& appDirectoryName, juce::File baseLocation)
{
    #if ELK_BUILD
    sourceDataLocation = juce::File(ELK_SOURCE_DATA_BASE_LOCATION);
    soundsDownloadLocation = juce::File(ELK_SOURCE_SOUNDS_LOCATION);
    presetFilesLocation = juce::File(ELK_SOURCE_PRESETS_LOCATION);
    tmpFilesLocation = juce::File(ELK_SOURCE_TMP_LOCATION);
    juce::ignoreUnused(appDirectoryName, baseLocation);
    #else
    if (baseLocation == juce::File()){
        baseLocation = SourceHelpers::getDefaultBaseDataLocation();
    }
    sourceDataLocation = baseLocation.getChildFile(appDirectoryName);
    soundsDownloadLocation = baseLocation.getChildFile(appDirectoryName + "/sounds");
    presetFilesLocation = baseLocation.getChildFile(appDirectoryName + "/presets");
    tmpFilesLocation = baseLocation.getChildFile(appDirectoryName + "/tmp");
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
    for (auto& preview : hoverPreviews)
        preview.transport.prepareToPlay (blockSize, sampleRate);
    hoverPreviewBuffer.setSize (juce::jmax (2, getTotalNumOutputChannels()), juce::jmax (blockSize, 4096));
    outputGain.reset (sampleRate, 0.02);
    outputGain.setCurrentAndTargetValue (outputMuted.load() ? 0.0f : 1.0f);
    
    // Pre-allocate space for MIDI messages coming from the UI
    {
        const juce::SpinLock::ScopedLockType sl (midiFromEditorLock);
        midiFromEditor.ensureSize(4096);
    }
    
    // Configure level measurer
    // Exponential moving average of the block mean square with a ~200ms time constant
    levelSmoothingCoefficient = 1.0f - std::exp(-(float)blockSize / (0.2f * (float)sampleRate));
    outputMeanSquares.fill(0.0f);
    for (auto& level: outputRmsLevels)
        level.store(0.0f);
    
    // Loaded the last loaded preset (only in ELK platform)
    # if LOADED_LATEST_LOADED_PRESET_AT_STARTUP
    if (!loadedPresetAtElkStartup){
        std::cout << "Loading latest loaded preset " << (juce::String)latestLoadedPreset << std::endl;
        loadPresetFromIndex(latestLoadedPreset);
        loadedPresetAtElkStartup = true;
    }
    # endif
}

void SourceSampler::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
    transportSource.releaseResources();
    for (auto& preview : hoverPreviews)
        preview.transport.releaseResources();
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
    // it gets played here. Don't block if the message thread is adding messages: they'll be picked up next block.
    {
        const juce::SpinLock::ScopedTryLockType tl (midiFromEditorLock);
        if (tl.isLocked() && !midiFromEditor.isEmpty()){
            midiMessages.addEvents(midiFromEditor, 0, buffer.getNumSamples(), 0);
            midiFromEditor.clear();
        }
    }
    
    // Render preview player into buffer
    transportSource.getNextAudioBlock(juce::AudioSourceChannelInfo(buffer));
    
    // Add the polyphonic previews (the scratch buffer is only reallocated for unusually big blocks)
    for (auto& preview : hoverPreviews){
        if (!preview.transport.isPlaying())
            continue;
        if (hoverPreviewBuffer.getNumSamples() < buffer.getNumSamples() || hoverPreviewBuffer.getNumChannels() < buffer.getNumChannels())
            hoverPreviewBuffer.setSize(juce::jmax(buffer.getNumChannels(), hoverPreviewBuffer.getNumChannels()), buffer.getNumSamples(), false, false, true);
        juce::AudioSourceChannelInfo info (&hoverPreviewBuffer, 0, buffer.getNumSamples());
        info.clearActiveBufferRegion();
        preview.transport.getNextAudioBlock(info);
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            buffer.addFrom(ch, 0, hoverPreviewBuffer, ch, 0, buffer.getNumSamples());
    }
    
    // Render sampler voices into buffer
    sampler.renderNextBlock(buffer, midiMessages, 0, buffer.getNumSamples());
    
    // Mute (with a short fade)
    outputGain.setTargetValue(outputMuted.load(std::memory_order_relaxed) ? 0.0f : 1.0f);
    if (outputGain.isSmoothing()){
        const float startGain = outputGain.getCurrentValue();
        const float endGain = outputGain.skip(buffer.getNumSamples());
        buffer.applyGainRamp(0, buffer.getNumSamples(), startGain, endGain);
    } else if (outputGain.getTargetValue() == 0.0f){
        buffer.clear();
    }
    
    // Measure audio levels (smoothed RMS per channel, published atomically for the UIs)
    const int numMeteredChannels = juce::jmin(buffer.getNumChannels(), maxMeteredChannels);
    if (buffer.getNumSamples() > 0){
        // Scale the smoothing for blocks of a different size than the one given in prepareToPlay
        const float coefficient = juce::jlimit(0.0f, 1.0f, levelSmoothingCoefficient * (float)buffer.getNumSamples() / (float)juce::jmax(1, blockSize));
        for (int ch = 0; ch < numMeteredChannels; ++ch){
            const float rms = buffer.getRMSLevel(ch, 0, buffer.getNumSamples());
            auto& meanSquare = outputMeanSquares[(size_t)ch];
            meanSquare += coefficient * (rms * rms - meanSquare);
            outputRmsLevels[(size_t)ch].store(std::sqrt(meanSquare), std::memory_order_relaxed);
        }
    }
    
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
                    // Onsets from Freesound: <fsAnalysis><onsets><onset time="..."/> in this format (they become the
                    // ANALYSIS of the sound sample, and the manual slices if numSlices is 0, see SourceSound::bindState)
                    juce::StringArray oldOnsets;
                    for (const auto& onset: soundInfo.getChildWithName("fsAnalysis").getChildWithName("onsets"))
                        if (onset.hasProperty("time"))
                            oldOnsets.add(onset.getProperty("time").toString());
                    juce::ValueTree sound = SourceHelpers::createSourceSoundAndSourceSamplerSoundFromProperties((int)soundInfo.getProperty("soundId"), soundInfo.getProperty("soundName"), soundInfo.getProperty("soundUser"), soundInfo.getProperty("soundLicense"), soundInfo.getProperty("soundOGGURL"), "", "", -1, oldOnsets, midiNotes, (int)samplerSound.getChildWithProperty("parameter_name", "midiRootNote").getProperty("parameter_value"), 0, (int)samplerSound.getChildWithProperty("parameter_name", "midiChannel").getProperty("parameter_value", SourceDefaults::midiChannel));
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
                    sound.setProperty(juce::Identifier("mod2CutoffAmt"), (float)samplerSound.getChildWithProperty("parameter_name", "mod2CutoffAmt").getProperty("parameter_value"), nullptr);
                    sound.setProperty(juce::Identifier("mod2GainAmt"), (float)samplerSound.getChildWithProperty("parameter_name", "mod2GainAmt").getProperty("parameter_value"), nullptr);
                    sound.setProperty(juce::Identifier("mod2PitchAmt"), (float)samplerSound.getChildWithProperty("parameter_name", "mod2PitchAmt").getProperty("parameter_value"), nullptr);
                    sound.setProperty(juce::Identifier("mod2PlayheadPos"), (float)samplerSound.getChildWithProperty("parameter_name", "mod2PlayheadPos").getProperty("parameter_value"), nullptr);
                    sound.setProperty(juce::Identifier("vel2CutoffAmt"), (float)samplerSound.getChildWithProperty("parameter_name", "vel2CutoffAmt").getProperty("parameter_value"), nullptr);
                    sound.setProperty(juce::Identifier("vel2GainAmt"), (float)samplerSound.getChildWithProperty("parameter_name", "vel2GainAmt").getProperty("parameter_value"), nullptr);
                    
                    sound.removeProperty(SourceIDs::slices, nullptr);  // Converted from the onsets when the sound is created
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

void SourceSampler::enableSessionAutosave (const juce::File& file)
{
    sessionAutosaveFile = file;
    // Restore the last session asynchronously, so it takes precedence over any (possibly older) state the wrapper restores
    juce::WeakReference<SourceSampler> weak = weakThis;
    juce::MessageManager::callAsync([weak]{
        auto* sampler = weak.get();
        if (sampler == nullptr || !sampler->sessionAutosaveFile.existsAsFile())
            return;
        if (auto xml = juce::XmlDocument::parse(sampler->sessionAutosaveFile)){
            auto savedState = juce::ValueTree::fromXml(*xml);
            if (savedState.hasType(SourceIDs::SOURCE_STATE)){
                DBG("Restoring autosaved session from " << sampler->sessionAutosaveFile.getFullPathName());
                sampler->loadPresetFromStateInformation(savedState);
                sampler->sessionNeedsSaving = false;
            }
        }
    });
}

int SourceSampler::fixFreesoundPreviewURLsInPresetFiles()
{
    // Old presets store Freesound preview URLs which now redirect to the CDN (see SourceHelpers::fixFreesoundPreviewURL).
    // Only files with old URLs are rewritten, so this is quick once they have been fixed.
    int numFixed = 0;
    for (const auto& file : presetFilesLocation.findChildFiles(juce::File::findFiles, false, "*.xml"))
        if (SourceHelpers::fixFreesoundPreviewURLsInFile(file))
            numFixed += 1;
    if (numFixed > 0)
        DBG("Fixed old Freesound preview URLs in " << numFixed << " preset files");
    return numFixed;
}

void SourceSampler::enablePresetImportFolder (const juce::File& folder)
{
    if (folder == juce::File() || folder == presetFilesLocation)
        return;
    presetImportFolder = folder;
    if (!presetImportFolder.isDirectory())
        presetImportFolder.createDirectory();
    auto readme = presetImportFolder.getChildFile("README.txt");
    if (!readme.existsAsFile())
        readme.replaceWithText("Copy SOURCE preset files here (named <preset number>.xml, e.g. 4.xml) to import them. "
                               "They are moved to the presets folder used by the SOURCE app and plugin, replacing the "
                               "preset with the same number.\n");
    importPresetsFromImportFolder();
}

int SourceSampler::importPresetsFromImportFolder()
{
    lastPresetImportCheckTime = juce::Time::getMillisecondCounterHiRes();
    if (presetImportFolder == juce::File() || !presetImportFolder.isDirectory())
        return 0;
    
    int numImported = 0;
    for (const auto& file : presetImportFolder.findChildFiles(juce::File::findFiles, false, "*.xml")){
        // Preset files are named by their index
        const auto indexString = file.getFileNameWithoutExtension();
        if (!indexString.containsOnly("0123456789") || indexString.isEmpty()){
            DBG("Not importing " << file.getFileName() << ": preset files must be named <preset number>.xml");
            continue;
        }
        auto xml = juce::XmlDocument::parse(file);
        if (xml == nullptr || !(xml->hasTagName(SourceIDs::PRESET.toString()) || xml->hasTagName("SourcePresetState"))){
            DBG("Not importing " << file.getFileName() << ": not a SOURCE preset file");
            continue;
        }
        auto destination = getPresetFilePath(getPresetFilenameByIndex(indexString.getIntValue()));
        if (file.copyFileTo(destination)){
            file.deleteFile();
            SourceHelpers::fixFreesoundPreviewURLsInFile(destination);
            DBG("Imported preset " << file.getFileName() << " to " << destination.getFullPathName());
            ++numImported;
        }
    }
    return numImported;
}

void SourceSampler::saveSessionNow()
{
    sessionNeedsSaving = false;
    if (sessionAutosaveFile == juce::File())
        return;
    if (auto xml = state.createXml())
        xml->writeTo(sessionAutosaveFile);  // Writes to a temporary file first, then replaces the target
}

void SourceSampler::runOnMessageThread (std::function<void(SourceSampler&)> fn)
{
    if (juce::MessageManager::getInstance()->isThisTheMessageThread()){
        fn(*this);
    } else {
        juce::WeakReference<SourceSampler> weak = weakThis;
        juce::MessageManager::callAsync([weak, fn = std::move(fn)]{
            if (auto* sampler = weak.get())
                fn(*sampler);
        });
    }
}

void SourceSampler::addMidiFromEditor (const juce::MidiMessage& message)
{
    const juce::SpinLock::ScopedLockType sl (midiFromEditorLock);
    midiFromEditor.addEvent(message, 0);
}

void SourceSampler::processPendingControlEvents()
{
    sampler.processPendingControlEvents([this](const SourceSamplerSynthesiser::ControlEvent& event){
        if (event.type == SourceSamplerSynthesiser::ControlEvent::Type::controller){
            if (sounds != nullptr){
                for (auto* sound: sounds->objects){
                    sound->applyMidiCCModulations(event.channel, event.number, event.value);
                }
            }
        } else if (event.type == SourceSamplerSynthesiser::ControlEvent::Type::programChange){
            loadPresetFromIndex(event.number);
        }
    });
}

void SourceSampler::forEachVoicePlayingSound(const juce::String& soundUUID, const std::function<void(int, const SourceSamplerVoice&)>& callback)
{
    // Message thread only (voices are only added/removed in the message thread). Like getVoicesInfo, the published
    // SourceSound pointer is only compared with our own sounds.
    if (sounds == nullptr)
        return;
    const SourceSound* sound = sounds->getSoundWithUUID(soundUUID);
    if (sound == nullptr)
        return;
    for (int i=0; i<sampler.getNumVoices(); i++){
        if (auto* voice = static_cast<SourceSamplerVoice*> (sampler.getVoiceFromMessageThread(i))){
            const auto published = voice->getPublishedState();
            if (published.active && published.sourceSound == sound)
                callback(i, *voice);
        }
    }
}

std::vector<SourceSampler::VoiceInfo> SourceSampler::getVoicesInfo()
{
    // Message thread only. Reads only the state published (atomically) by the voices in the audio thread. The published SourceSound
    // pointer is only used as an identifier to look up the sound in our own list (it is never dereferenced).
    std::vector<VoiceInfo> info;
    for (int i=0; i<sampler.getNumVoices(); i++){
        VoiceInfo voiceInfo;
        if (auto* voice = static_cast<SourceSamplerVoice*> (sampler.getVoiceFromMessageThread(i))){
            const auto published = voice->getPublishedState();
            if (published.active && sounds != nullptr){
                for (auto* sound: sounds->objects){
                    if (sound == published.sourceSound){
                        voiceInfo.active = true;
                        voiceInfo.soundUUID = sound->getUUID();
                        voiceInfo.firstSampleUUID = sound->state.getChildWithName(SourceIDs::SOUND_SAMPLE).getProperty(SourceIDs::uuid).toString();
                        voiceInfo.playPosition = published.playPosition;
                        break;
                    }
                }
            }
        }
        info.push_back(voiceInfo);
    }
    return info;
}

std::vector<float> SourceSampler::getOutputLevels()
{
    std::vector<float> levels;
    for (int i=0; i<getTotalNumOutputChannels(); i++){
        levels.push_back(outputRmsLevels[(size_t)juce::jmin(i, maxMeteredChannels - 1)].load(std::memory_order_relaxed));
    }
    return levels;
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
    settings.setProperty(SourceIDs::mpeEnabled, mpeEnabled.get(), nullptr);
    settings.setProperty(SourceIDs::mpePitchBendRange, mpePitchBendRange.get(), nullptr);
    settings.setProperty(SourceIDs::bpm, bpm.get(), nullptr);
    settings.setProperty(SourceIDs::pluginVersion, juce::String(ProjectInfo::versionString), nullptr);
    
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
            if (settings.hasProperty(SourceIDs::mpeEnabled)){
                mpeEnabled = (bool)settings.getProperty(SourceIDs::mpeEnabled);
            }
            if (settings.hasProperty(SourceIDs::mpePitchBendRange)){
                mpePitchBendRange = (int)settings.getProperty(SourceIDs::mpePitchBendRange);
            }
            if (settings.hasProperty(SourceIDs::bpm)){
                bpm = juce::jlimit(SourceHelpers::minBpm, SourceHelpers::maxBpm, (double)settings.getProperty(SourceIDs::bpm));
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
    state.setProperty(SourceIDs::isQuerying, isQuerying.load(), nullptr);
    state.setProperty(SourceIDs::midiInLastStateReportBlock, midiMessagesPresentInLastStateReport.exchange(false), nullptr);
    state.setProperty(SourceIDs::lastMIDICCNumber, lastReceivedMIDIControllerNumber.load(), nullptr);
    state.setProperty(SourceIDs::lastMIDINoteNumber, lastReceivedMIDINoteNumber.load(), nullptr);
    
    juce::String voiceActivations = "";
    juce::String voiceSoundIdxs = "";
    juce::String voiceSoundPlayPositions = "";
    
    for (const auto& voice: getVoicesInfo()){
        if (voice.active){
            voiceActivations += "1,";
            voiceSoundIdxs += voice.firstSampleUUID + ",";
            voiceSoundPlayPositions += (juce::String)voice.playPosition + ",";
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
        audioLevels += (juce::String)outputRmsLevels[(size_t)juce::jmin(i, maxMeteredChannels - 1)].load(std::memory_order_relaxed) + ",";
    }
    state.setProperty(SourceIDs::audioLevels, audioLevels, nullptr);
    return state;
}

juce::String SourceSampler::collectVolatileStateInformationAsString(){
    
    juce::StringArray stateAsStringParts = {};
    
    stateAsStringParts.add(isQuerying ? "1": "0");
    stateAsStringParts.add(midiMessagesPresentInLastStateReport.exchange(false) ? "1" : "0");
    stateAsStringParts.add((juce::String)lastReceivedMIDIControllerNumber.load());
    stateAsStringParts.add((juce::String)lastReceivedMIDINoteNumber.load());
    
    juce::String voiceActivations = "";
    juce::String voiceSoundIdxs = "";
    juce::String voiceSoundPlayPositions = "";
    
    for (const auto& voice: getVoicesInfo()){
        if (voice.active){
            voiceActivations += "1,";
            voiceSoundIdxs += voice.firstSampleUUID + ",";
            voiceSoundPlayPositions += (juce::String)voice.playPosition + ",";
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
        audioLevels += (juce::String)outputRmsLevels[(size_t)juce::jmin(i, maxMeteredChannels - 1)].load(std::memory_order_relaxed) + ",";
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
            lastRequestedPreviewPath = soundPath;
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
        addMidiFromEditor(message);
    }
    else if (actionName == ACTION_NOTE_OFF){
        int note = parameters[0].getIntValue();
        int velocity = parameters[1].getIntValue();
        int channel = parameters[2].getIntValue();
        juce::MidiMessage message = juce::MidiMessage::noteOff(channel > 0 ? channel : (globalMidiInChannel > 0 ? globalMidiInChannel : 1), note, (juce::uint8)velocity);
        addMidiFromEditor(message);
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
    else if (actionName == ACTION_SET_FX_PARAMETER){
        // Parameters: parameter id (see SourceFx::getParamInfos), value
        const int fxParam = SourceFx::findParam(parameters[0]);
        if (fxParam >= 0){
            const auto& info = SourceFx::getParamInfos()[(size_t) fxParam];
            const float value = juce::jlimit(info.minValue, info.maxValue, parameters[1].getFloatValue());
            state.getChildWithName(SourceIDs::PRESET).setProperty(info.getIdentifier(), info.isToggle ? (value >= 0.5f ? 1.0f : 0.0f) : value, nullptr);
        }
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
        numVoices = juce::jlimit(1, (int)SourceSamplerSynthesiser::maxNumVoices, parameters[0].getIntValue());
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
        if (!queryMakerThread.isThreadRunning()){
            queryMakerThread.setQueryParameters(addReplaceOrReplaceSound, query, numSounds, minSoundLength, maxSoundLength);
            queryExtraFilter = parameters[5];  // Optional extra filter (e.g. "brightness:[0 TO 33.33]")
            isQuerying = true;
            state.setProperty(SourceIDs::numResultsLastQuery, -1, nullptr);
            queryMakerThread.startThread();
        } else {
            DBG("Skip query as already querying...");
        }
    }
    else if (actionName == ACTION_PLAY_HOVER_PREVIEW){
        if (parameters[0].isNotEmpty())
            playHoverPreview(parameters[0]);
    }
    else if (actionName == ACTION_REPLACE_EXISTING_SOUNDS_RANDOMLY){
        // Parameters: number of sounds, min length, max length, note layout type
        if (isQuerying){
            DBG("Skip query as already querying...");
        } else {
            const int numSounds = juce::jlimit(1, 128, parameters[0].getIntValue());
            const float minLength = parameters[1].getFloatValue();
            const float maxLength = parameters[2].getFloatValue();
            noteLayoutType = parameters[3].getIntValue();
            isQuerying = true;
            state.setProperty(SourceIDs::numResultsLastQuery, -1, nullptr);
            juce::String filter = "duration:[" + juce::String(minLength) + " TO " + (maxLength > 0.0f ? juce::String(maxLength) : juce::String("*")) + "]";
            if (parameters[4].isNotEmpty())
                filter += " " + parameters[4];  // Optional extra filter (e.g. acoustic properties)
            juce::WeakReference<SourceSampler> weak = weakThis;
            freesoundSearch.findRandomSounds(numSounds, filter, [weak](std::vector<FreesoundSearchResult> found, int numMatching, juce::String error){
                auto* sampler = weak.get();
                if (sampler == nullptr)
                    return;
                sampler->isQuerying = false;
                sampler->state.setProperty(SourceIDs::numResultsLastQuery, numMatching, nullptr);
                juce::ignoreUnused(error);
                if (found.empty()){
                    DBG("No random sounds found " << error);
                    return;
                }
                sampler->removeAllSounds();
                const int n = (int) found.size();
                for (int i = 0; i < n; i++){
                    const auto& r = found[(size_t) i];
                    const auto layout = SourceHelpers::computeNoteLayout(sampler->noteLayoutType, i, n);
                    sampler->addOrReplaceSoundFromBasicSoundProperties("", r.id, r.name, r.username, r.license, r.previewURL, "", r.format,
                                                                        (int) juce::jmin((juce::int64) INT_MAX, r.filesize), r.getOnsetTimesAsStrings(), layout.midiNotes,
                                                                        layout.midiRootNote, SourceDefaults::midiVelocityLayer, layout.midiChannel, false);
                }
            });
        }
    }
    else if (actionName == ACTION_REPLACE_SOUND_FROM_QUERY){
        juce::String addReplaceOrReplaceSound = parameters[0]; // Will be UUID of the sound to replace
        juce::String query = parameters[1];
        float minSoundLength = parameters[2].getFloatValue();
        float maxSoundLength = parameters[3].getFloatValue();
        int noteMappingType = parameters[4].getIntValue();
        noteLayoutType = noteMappingType; // Set noteLayoutType so when sounds are actually downloaded and loaded, the requested mode is used
        
        // Trigger query in a separate thread so that we don't block UI
        if (!queryMakerThread.isThreadRunning()){
            queryMakerThread.setQueryParameters(addReplaceOrReplaceSound, query, 1, minSoundLength, maxSoundLength);
            queryExtraFilter = {};
            isQuerying = true;
            state.setProperty(SourceIDs::numResultsLastQuery, -1, nullptr);
            queryMakerThread.startThread();
        } else {
            DBG("Skip query as already querying...");
        }
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
        int midiChannel = SourceDefaults::midiChannel;
        bool addToExistingSourceSampleSounds = false;

        addOrReplaceSoundFromBasicSoundProperties("", soundID, soundName, soundUser, soundLicense, oggDownloadURL, localFilePath, type, sizeBytes, slices, midiNotes, midiRootNote, midiVelocityLayer, midiChannel, addToExistingSourceSampleSounds);
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
        int midiChannel = -1;
        bool addToExistingSourceSampleSounds = false;

        addOrReplaceSoundFromBasicSoundProperties(soundUUID, soundID, soundName, soundUser, soundLicense, oggDownloadURL, localFilePath, type, sizeBytes, slices, midiNotes, midiRootNote, midiVelocityLayer, midiChannel, addToExistingSourceSampleSounds);
    }
    else if (actionName == ACTION_REMOVE_SOUND){
        juce::String soundUUID = parameters[0];
        removeSound(soundUUID);
    }
    else if (actionName == ACTION_REPLACE_SOUND_AUDIO){
        // Parameters: sound UUID, sound ID, name, user, license, preview URL, local file path, format, size, slices
        juce::StringArray slices;
        if (parameters[9] != "")
            slices.addTokens(parameters[9], ",", "");
        replaceSoundAudio(parameters[0], parameters[1].getIntValue(), parameters[2], parameters[3], parameters[4], parameters[5],
                          parameters[6], parameters[7], parameters[8].getIntValue(), slices);
    }
    else if (actionName == ACTION_REPLACE_SOUND_WITH_SIMILAR){
        replaceSoundWithSimilar(parameters[0]);
    }
    else if (actionName == ACTION_RETRY_SOUND_DOWNLOADS){
        if (auto* sound = getSoundWithUUID(parameters[0]))
            sound->retryFailedDownloads();
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
            sound->setManualSlices(sliceOnsetTimes);
        }
    }
    else if (actionName == ACTION_SET_SOUND_SLICES_FROM_ONSETS){
        if (auto* sound = sounds->getSoundWithUUID(parameters[0]))
            sound->setSlicesFromOnsets();
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
        int midiChannel = SourceDefaults::midiChannel;
        bool addToExistingSourceSampleSounds = true;

        addOrReplaceSoundFromBasicSoundProperties("", soundID, soundName, soundUser, soundLicense, oggDownloadURL, localFilePath, type, sizeBytes, slices, midiNotes, midiRootNote, midiVelocityLayer, midiChannel, addToExistingSourceSampleSounds);
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
    
    else if (actionName == ACTION_SET_SOUND_MUTED){
        if (auto* sound = sounds->getSoundWithUUID(parameters[0]))
            sound->setMuted(parameters[1].getIntValue() != 0);
    }
    else if (actionName == ACTION_SET_BPM){
        if (parameters[0].isNotEmpty())
            setBpm(parameters[0].getDoubleValue());
    }
    
    // MPE actions -----------------------------------------------------------------------------------
    else if (actionName == ACTION_SET_MPE_SETTINGS){
        bool enabled = parameters[0].getIntValue() != 0;
        int bendRange = parameters.size() > 1 && parameters[1].isNotEmpty() ? parameters[1].getIntValue() : mpePitchBendRange.get();
        setMpeSettings(enabled, bendRange);
    }
    else if (actionName == ACTION_ADD_OR_UPDATE_MPE_MAPPING){
        juce::String soundUUID = parameters[0];
        juce::String uuid = parameters[1];
        juce::String source = parameters[2];
        juce::String parameterName = parameters[3];
        float minRange = parameters[4].getFloatValue();
        float maxRange = parameters[5].getFloatValue();
        float curve = parameters[6].getFloatValue();
        if (auto* sound = sounds->getSoundWithUUID(soundUUID)){
            sound->addOrEditMpeMapping(uuid, source, parameterName, minRange, maxRange, curve);
        }
    }
    else if (actionName == ACTION_REMOVE_MPE_MAPPING){
        if (auto* sound = sounds->getSoundWithUUID(parameters[0])){
            sound->removeMpeMapping(parameters[1]);
        }
    }
    
    // Modulator actions -----------------------------------------------------------------------------------
    else if (actionName == ACTION_ADD_OR_UPDATE_MODULATOR){
        // Missing or empty values use the defaults
        auto floatParam = [&parameters](int index, float defaultValue){ return parameters.size() > index && parameters[index].isNotEmpty() ? parameters[index].getFloatValue() : defaultValue; };
        if (auto* sound = sounds->getSoundWithUUID(parameters[0])){
            sound->addOrEditModulator(parameters[1], parameters[2],
                                      parameters.size() > 3 && parameters[3].isNotEmpty() ? parameters[3] : SourceDefaults::modulatorParameterName,
                                      floatParam(4, SourceDefaults::modulatorMinRange), floatParam(5, SourceDefaults::modulatorMaxRange),
                                      (int)floatParam(6, (float)SourceDefaults::lfoWaveform), floatParam(7, SourceDefaults::lfoRate),
                                      floatParam(8, SourceDefaults::modAttack), floatParam(9, SourceDefaults::modDecay),
                                      floatParam(10, SourceDefaults::modSustain), floatParam(11, SourceDefaults::modRelease),
                                      floatParam(12, SourceDefaults::lfoAttack), floatParam(13, SourceDefaults::lfoSync ? 1.0f : 0.0f) != 0.0f,
                                      (int)floatParam(14, (float)SourceDefaults::lfoSyncDivision), (int)floatParam(15, (float)SourceDefaults::lfoAttackDivision),
                                      (int)floatParam(16, (float)SourceDefaults::numSteps), floatParam(17, SourceDefaults::stepsRate),
                                      (int)floatParam(18, (float)SourceDefaults::stepsSyncLength), parameters.size() > 19 ? parameters[19] : juce::String());
        }
    }
    else if (actionName == ACTION_REMOVE_MODULATOR){
        if (auto* sound = sounds->getSoundWithUUID(parameters[0])){
            sound->removeModulator(parameters[1]);
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
    sampler.setGlobalMidiInChannel(channel);
    saveGlobalPersistentStateToFile();
}

void SourceSampler::setBpm(double newBpm)
{
    bpm = juce::jlimit(SourceHelpers::minBpm, SourceHelpers::maxBpm, newBpm);
    globalBpm.store(bpm.get());
    if (getTempoSource() == TempoSource::internal)
        setCurrentTempo(bpm.get(), TempoSource::internal);  // Also when audio is not running
    saveGlobalPersistentStateToFile();
}

void SourceSampler::setCurrentTempo(double newBpm, TempoSource source) noexcept
{
    currentBpm.store(newBpm, std::memory_order_relaxed);
    tempoSource.store((int)source, std::memory_order_relaxed);
    sampler.setCurrentBpm(newBpm);
}

void SourceSampler::setMpeSettings(bool enabled, int perNotePitchBendRange)
{
    mpeEnabled = enabled;
    mpePitchBendRange = juce::jlimit(1, 96, perNotePitchBendRange);
    sampler.setMpeSettings(mpeEnabled.get(), mpePitchBendRange.get());
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
     hardware stack). In ELK stack, the communication with Freesound happens in the UI application (Python) and the UI tells the plugin which sounds to load (and when).
     
     This runs in QueryMakerThread. The (slow) HTTP request is done here, and the results are then passed to the message thread where the state is modified.
     */
    
    FreesoundClient client(FREESOUND_API_KEY);
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
        if (queryExtraFilter.isNotEmpty())
            filter += " " + queryExtraFilter;
        query = textQuery;
        pageSize = 100;
    }
    SoundList list = client.textSearch(query, filter, "score", 0, -1, pageSize, "id,name,username,license,type,filesize,previews,onset_times", "", 0);
    int numResults = list.getCount();
    juce::Array<FSSound> soundsFound = numResults > 0 ? list.toArrayOfSounds() : juce::Array<FSSound>();
    
    juce::WeakReference<SourceSampler> weak = weakThis;
    juce::MessageManager::callAsync([weak, addReplaceOrReplaceSound, numSounds, numResults, soundsFound]{
        if (auto* sampler = weak.get())
            sampler->loadQueryResults(addReplaceOrReplaceSound, numSounds, numResults, soundsFound);
    });
}

void SourceSampler::loadQueryResults(const juce::String& addReplaceOrReplaceSound, int numSounds, int numResults, juce::Array<FSSound> soundsFound)
{
    isQuerying = false;
    state.setProperty(SourceIDs::numResultsLastQuery, numResults, nullptr);
    
    juce::String soundUUIDToReplace = "";
    if ((addReplaceOrReplaceSound != "add") && (addReplaceOrReplaceSound != "replace")){
        // if action is not "add" or "replace", then it is "replace one" and the contents of this variable are the sound UUID to replace
        // in that case, make sure numSounds is limited to 1 as this is the number of sounds we need to replace
        numSounds = 1;
        soundUUIDToReplace = addReplaceOrReplaceSound;
    }
    
    if (numResults > 0 && soundsFound.size() > 0){
        // Randmomize results and prepare for iteration
        DBG("Query got " + (juce::String)numResults + " results, " + (juce::String)soundsFound.size() + " in the first page. Will load " + (juce::String)numSounds + " sounds.");
        unsigned seed = (unsigned) std::chrono::system_clock::now().time_since_epoch().count();
        std::shuffle(soundsFound.begin(), soundsFound.end(), std::default_random_engine(seed));
        soundsFound.resize(juce::jmin(numSounds, soundsFound.size()));
        int nSounds = soundsFound.size();
        
        if (addReplaceOrReplaceSound == "replace"){
            // If action is to replace all sounds, then we need to first clear all loaded sounds
            removeAllSounds();
        }

        for (int i=0; i<nSounds; i++){
            SourceHelpers::NoteLayout layout;
            if (soundUUIDToReplace == ""){
                // If no sound is being replaced, calculate the mapping depending to the requested mapping type
                layout = SourceHelpers::computeNoteLayout(noteLayoutType, i, nSounds);
            } else {
                // If a specific sound is being replaced, no need to calcualte any mapping here because it will be copied from the
                // sound being replaced in addOrReplaceSoundFromBasicSoundProperties. Also set midiChannel to -1 so it is copied from the sound being replaced
                layout.midiRootNote = -1;
                layout.midiChannel = -1;
            }
            
            // soundUUIDToReplace will be "" when we're adding or replacing all sounds, otherwise it will be the UUID of the sound to replace
            int midiVelocityLayer = SourceDefaults::midiVelocityLayer; // Always use default velocity layer as we're not layering sounds
            bool addToExistingSourceSampleSounds = false;
            addOrReplaceSoundFromBasicSoundProperties(soundUUIDToReplace, soundsFound[i], layout.midiNotes, layout.midiRootNote, midiVelocityLayer, layout.midiChannel, addToExistingSourceSampleSounds);
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
                                                              int midiChannel,
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
            if (midiChannel == -1){ // If replacing a sound, keep the assigned midi channel
                midiChannel = sound->getParameterInt(SourceIDs::midiChannel);
            }
        }
        
        juce::ValueTree sourceSound = SourceHelpers::createSourceSoundAndSourceSamplerSoundFromProperties(soundID, soundName, soundUser, soundLicense, previewURL, localFilePath, format, sizeBytes, slices, midiNotes, midiRootNote, midiVelocityLayer, midiChannel);
        
        juce::ValueTree presetState = state.getChildWithName(SourceIDs::PRESET);
        if (existingSoundIndex > -1){
            // If replacing an existing sound, copy as well the midi mappings
            juce::ValueTree soundToReplaceState = presetState.getChildWithProperty(SourceIDs::uuid, soundUUID);
            for (int i=0; i<soundToReplaceState.getNumChildren(); i++){
                auto child = soundToReplaceState.getChild(i);
                if (child.hasType(SourceIDs::MIDI_CC_MAPPING) || child.hasType(SourceIDs::MPE_MAPPING) || child.hasType(SourceIDs::MODULATOR)){
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
                                                              int midiChannel,
                                                              bool addToExistingSourceSampleSounds)
{
    // Prepare slices (onsets, stored in the analysis of the sound sample, used by "slices from waveform")
    juce::StringArray slices;
    if (sound.onsetTimes.isArray()){
        for (int j=0; j<sound.onsetTimes.size(); j++)
            slices.add(sound.onsetTimes[j].toString());
    } else if (sound.onsetTimes.isDouble() || sound.onsetTimes.isInt()){
        slices.add(sound.onsetTimes.toString());
    } else if (sound.analysis.hasProperty("rhythm")){
        if (sound.analysis["rhythm"].hasProperty("onset_times")){
            for (int j=0; j<sound.analysis["rhythm"]["onset_times"].size(); j++){
                slices.add(sound.analysis["rhythm"]["onset_times"][j].toString());
            }
        }
    }
    
    // Create sound
    addOrReplaceSoundFromBasicSoundProperties(soundUUID, sound.id.getIntValue(), sound.name, sound.user, sound.license, sound.getOGGPreviewURL().toString(false), "", sound.format, sound.filesize, slices, midiNotes, midiRootNote, midiVelocityLayer, midiChannel, addToExistingSourceSampleSounds);
}

bool SourceSampler::replaceSoundAudio(const juce::String& soundUUID, int soundID, const juce::String& soundName, const juce::String& soundUser,
                                      const juce::String& soundLicense, const juce::String& previewURL, const juce::String& localFilePath,
                                      const juce::String& format, int sizeBytes, juce::StringArray slices)
{
    juce::ValueTree presetState = state.getChildWithName(SourceIDs::PRESET);
    const int index = presetState.indexOf(presetState.getChildWithProperty(SourceIDs::uuid, soundUUID));
    auto* sound = sounds->getSoundWithUUID(soundUUID);
    if (index < 0 || sound == nullptr)
        return false;
    const juce::ValueTree oldSound = presetState.getChild(index);
    
    juce::BigInteger midiNotes = sound->getMappedMidiNotes();
    const int midiRootNote = sound->getMidiNoteFromFirstSourceSamplerSound();
    const int midiChannel = sound->getParameterInt(SourceIDs::midiChannel);
    juce::ValueTree newSound = SourceHelpers::createSourceSoundAndSourceSamplerSoundFromProperties(soundID, soundName, soundUser, soundLicense, previewURL, localFilePath, format, sizeBytes, slices, midiNotes, midiRootNote, SourceDefaults::midiVelocityLayer, midiChannel);
    
    // Keep all the settings of the old sound (but not its identity or loading state)
    for (int i = 0; i < oldSound.getNumProperties(); ++i){
        const auto property = oldSound.getPropertyName(i);
        // (Manual slices are times of the old audio: the new sound starts without them)
        if (property == SourceIDs::uuid || property == SourceIDs::allSoundsLoaded || property == SourceIDs::willBeDeleted || property == SourceIDs::findingSimilar
            || property == SourceIDs::slices)
            continue;
        newSound.setProperty(property, oldSound.getProperty(property), nullptr);
    }
    for (int i = 0; i < oldSound.getNumChildren(); ++i){
        auto child = oldSound.getChild(i);
        if (child.hasType(SourceIDs::MIDI_CC_MAPPING) || child.hasType(SourceIDs::MPE_MAPPING) || child.hasType(SourceIDs::MODULATOR))
            newSound.addChild(child.createCopy(), -1, nullptr);
    }
    presetState.removeChild(index, nullptr);
    presetState.addChild(newSound, index, nullptr);
    return true;
}

void SourceSampler::replaceSoundWithSimilar(const juce::String& soundUUID)
{
    juce::ValueTree soundState = state.getChildWithName(SourceIDs::PRESET).getChildWithProperty(SourceIDs::uuid, soundUUID);
    const auto sample = soundState.getChildWithName(SourceIDs::SOUND_SAMPLE);
    const int soundId = sample.getProperty(SourceIDs::soundId, -1);
    if (!soundState.isValid() || soundId <= 0 || !(bool) sample.getProperty(SourceIDs::soundFromFreesound, false))
        return;
    soundState.setProperty(SourceIDs::findingSimilar, true, nullptr);
    juce::WeakReference<SourceSampler> weak = weakThis;
    freesoundSearch.findSimilarSounds(soundId, 10, [weak, soundUUID, soundId](std::vector<FreesoundSearchResult> similar, juce::String error){
        auto* sampler = weak.get();
        if (sampler == nullptr)
            return;
        auto current = sampler->state.getChildWithName(SourceIDs::PRESET).getChildWithProperty(SourceIDs::uuid, soundUUID);
        if (!current.isValid())
            return;  // The sound was removed in the meantime
        current.setProperty(SourceIDs::findingSimilar, false, nullptr);
        similar.erase(std::remove_if(similar.begin(), similar.end(), [soundId](const auto& r){ return r.id == soundId || r.previewURL.isEmpty(); }), similar.end());
        juce::ignoreUnused(error);
        if (similar.empty()){
            DBG("No similar sounds found for " << soundId << " " << error);
            return;
        }
        const auto& chosen = similar[(size_t) juce::Random::getSystemRandom().nextInt((int) similar.size())];
        sampler->replaceSoundAudio(soundUUID, chosen.id, chosen.name, chosen.username, chosen.license, chosen.previewURL, "",
                                   chosen.format, (int) juce::jmin((juce::int64) INT_MAX, chosen.filesize), chosen.getOnsetTimesAsStrings());
    });
}

void SourceSampler::reapplyNoteLayout(int newNoteLayoutType)
{
    noteLayoutType = newNoteLayoutType;
    // Only consider sounds which are not going to be deleted
    std::vector<SourceSound*> activeSounds;
    for (auto* sound: sounds->objects){
        if (!sound->isScheduledForDeletion())
            activeSounds.push_back(sound);
    }
    const int nSounds = (int)activeSounds.size();
    for (int i=0; i<nSounds; i++){
        auto layout = SourceHelpers::computeNoteLayout(noteLayoutType, i, nSounds);
        activeSounds[(size_t)i]->setMappedMidiNotes(layout.midiNotes);
        activeSounds[(size_t)i]->setMidiRootNote(layout.midiRootNote);
        activeSounds[(size_t)i]->setParameterByNameInt(SourceIDs::midiChannel, layout.midiChannel);
    }
}

void SourceSampler::addToMidiBuffer(const juce::String& soundUUID, bool doNoteOff)
{
    auto* sound = sounds->getSoundWithUUID(soundUUID);
    if (sound != nullptr){
        int midiNoteForNormalPitch = sound->getMidiNoteFromFirstSourceSamplerSound();
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
            addMidiFromEditor(message);
        }
    }
}

double SourceSampler::getStartTime(){
    return startTime;
}

void SourceSampler::timerCallback()
{
    // Session autosave (debounced so we don't write on every slider movement or download progress update)
    if (sessionNeedsSaving && (juce::Time::getMillisecondCounterHiRes() - lastStateChangeTime) > SESSION_AUTOSAVE_DELAY_MS){
        saveSessionNow();
    }
    
    // Preset import folder (files can be added from the Files app while the app is running)
    if (presetImportFolder != juce::File() && (juce::Time::getMillisecondCounterHiRes() - lastPresetImportCheckTime) > PRESET_IMPORT_CHECK_INTERVAL_MS){
        importPresetsFromImportFolder();
    }
    
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
    #if USE_WS_SERVER
    return serverInterface.wsServer.assignedPort;
    #else
    return 0;
    #endif
}


void SourceSampler::previewFile(const juce::String& path)
{
    // Message thread. Plays a local file, or a URL which is first downloaded to the tmp folder (with the same
    // downloader used for sounds: long timeout for the slow Freesound CDN and retries).
    if (transportSource.isPlaying()){
        transportSource.stop();
    }
    previewFailed = false;
    previewLoading = false;
    if (currentlyLoadedPreviewFilePath == path && readerSource != nullptr){
        transportSource.setPosition(0.0);
        transportSource.start();
        return;
    }
    
    if (path.startsWith("http")){
        juce::StringArray tokens;
        tokens.addTokens (path, "/", "");
        juce::File location = tmpFilesLocation.getChildFile("preview-" + tokens[tokens.size() - 1]);
        if (location.existsAsFile()){  // Dont' re-download if file already exists
            previewFile(location.getFullPathName());
            return;
        }
        # if !ELK_BUILD  // On ELK, downloads should be triggered from the Python script before calling this function
        // Only the last requested preview is downloaded
        previewDownloadPool->cancel(previewDownloadCancelFlag);
        previewDownloadCancelFlag = DownloadPool::makeCancelFlag();
        previewLoading = true;
        juce::WeakReference<SourceSampler> weak = weakThis;
        previewDownloadPool->download({ juce::URL(path), {}, location }, previewDownloadCancelFlag, nullptr,
            [weak, path](const juce::File& target, bool ok){
                juce::MessageManager::callAsync([weak, path, target, ok]{
                    auto* sampler = weak.get();
                    if (sampler == nullptr || sampler->lastRequestedPreviewPath != path)
                        return;
                    sampler->previewLoading = false;
                    if (ok){
                        sampler->previewFile(target.getFullPathName());
                    } else {
                        DBG("Could not download preview " << path);
                        sampler->previewFailed = true;
                    }
                });
            });
        # endif
        return;
    }
    
    juce::File file = juce::File(path);
    std::unique_ptr<juce::AudioFormatReader> reader (file.existsAsFile() ? audioFormatManager.createReaderFor (file) : nullptr);
    if (reader == nullptr){
        DBG("Could not play preview " << path);
        previewFailed = true;
        if (file.isAChildOf(tmpFilesLocation))
            file.deleteFile();  // Don't keep broken downloads
        return;
    }
    const double sampleRateOfFile = reader->sampleRate;
    auto newSource = std::make_unique<juce::AudioFormatReaderSource> (reader.release(), true);
    transportSource.setSource (newSource.get(), 0, nullptr, sampleRateOfFile);
    readerSource.reset (newSource.release());
    currentlyLoadedPreviewFilePath = path;
    transportSource.setPosition(0.0);
    transportSource.start();
}

void SourceSampler::playHoverPreview(const juce::String& path)
{
    // Message thread
    if (!path.startsWith("http")){
        startHoverPreview(juce::File(path));
        return;
    }
    juce::StringArray tokens;
    tokens.addTokens (path, "/", "");
    juce::File location = tmpFilesLocation.getChildFile("preview-" + tokens[tokens.size() - 1]);
    if (location.existsAsFile()){
        startHoverPreview(location);
        return;
    }
    # if !ELK_BUILD
    // Download it (shared with the other previews' cache) and play it only if it arrives soon enough
    const double requestTime = juce::Time::getMillisecondCounterHiRes();
    juce::WeakReference<SourceSampler> weak = weakThis;
    previewDownloadPool->download({ juce::URL(path), {}, location }, hoverPreviewDownloadsCancelFlag, nullptr,
        [weak, requestTime](const juce::File& target, bool ok){
            juce::MessageManager::callAsync([weak, requestTime, target, ok]{
                if (auto* sampler = weak.get())
                    if (ok && juce::Time::getMillisecondCounterHiRes() - requestTime < maxHoverPreviewDelayMs)
                        sampler->startHoverPreview(target);
            });
        });
    # endif
}

void SourceSampler::startHoverPreview(const juce::File& file)
{
    std::unique_ptr<juce::AudioFormatReader> reader (file.existsAsFile() ? audioFormatManager.createReaderFor(file) : nullptr);
    if (reader == nullptr){
        if (file.isAChildOf(tmpFilesLocation))
            file.deleteFile();  // Don't keep broken downloads
        return;
    }
    // Use a free voice, or the oldest one
    HoverPreview* voice = nullptr;
    for (auto& preview : hoverPreviews)
        if (!preview.transport.isPlaying() && (voice == nullptr || preview.startTime < voice->startTime))
            voice = &preview;
    if (voice == nullptr){
        voice = &hoverPreviews[0];
        for (auto& preview : hoverPreviews)
            if (preview.startTime < voice->startTime)
                voice = &preview;
    }
    const double fileSampleRate = reader->sampleRate;
    auto newSource = std::make_unique<juce::AudioFormatReaderSource>(reader.release(), true);
    voice->transport.stop();
    voice->transport.setSource(newSource.get(), 0, nullptr, fileSampleRate);
    voice->reader = std::move(newSource);
    voice->transport.setGain(0.6f);  // Many previews can play at the same time
    voice->transport.setPosition(0.0);
    voice->transport.start();
    voice->startTime = juce::Time::getMillisecondCounterHiRes();
}

SourceSampler::PreviewState SourceSampler::getPreviewState() const
{
    if (transportSource.isPlaying()) return PreviewState::playing;
    if (previewLoading) return PreviewState::loading;
    if (previewFailed) return PreviewState::failed;
    return PreviewState::idle;
}

void SourceSampler::stopPreviewingFile(){
    for (auto& preview : hoverPreviews)
        preview.transport.stop();
    // Hover previews still downloading won't start either
    previewDownloadPool->cancel(hoverPreviewDownloadsCancelFlag);
    hoverPreviewDownloadsCancelFlag = DownloadPool::makeCancelFlag();
    lastRequestedPreviewPath = {};
    previewLoading = false;
    previewDownloadPool->cancel(previewDownloadCancelFlag);
    previewDownloadCancelFlag = DownloadPool::makeCancelFlag();
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
    #if USING_DIRECT_COMMUNICATION_METHOD
    // Send message to the browser-based UI loaed in the WebComponenet of the PluginEditor
    // NOTE: for some reason calling this crashes the app sometimes. need to be further investigated...
    sendActionMessage(serverInterface.serliaizeOSCMessage(message));
    #else
    // Send message to all connected WS clients (if WS server is up and running)
    if (serverInterface.wsServer.serverPtr != nullptr){
        serverInterface.sendMessageToWebSocketClients(message);
    }
    #endif
}


//==============================================================================

void SourceSampler::valueTreePropertyChanged (juce::ValueTree& treeWhosePropertyHasChanged, const juce::Identifier& property)
{
    if (sessionAutosaveFile != juce::File()){
        sessionNeedsSaving = true;
        lastStateChangeTime = juce::Time::getMillisecondCounterHiRes();
    }
    if (treeWhosePropertyHasChanged.hasType(SourceIDs::PRESET)){
        const int fxParam = SourceFx::findParam(property.toString());
        if (fxParam >= 0)
            sampler.setFxParameter((SourceFx::Param) fxParam, (float) treeWhosePropertyHasChanged.getProperty(property));
    }
    #if !SOURCE_SYNC_STATE_WITH_REMOTE_UI
    juce::ignoreUnused(treeWhosePropertyHasChanged, property);
    return;
    #endif
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
    if (sessionAutosaveFile != juce::File()){
        sessionNeedsSaving = true;
        lastStateChangeTime = juce::Time::getMillisecondCounterHiRes();
    }
    #if !SOURCE_SYNC_STATE_WITH_REMOTE_UI
    juce::ignoreUnused(parentTree, childWhichHasBeenAdded);
    return;
    #endif
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
    if (sessionAutosaveFile != juce::File()){
        sessionNeedsSaving = true;
        lastStateChangeTime = juce::Time::getMillisecondCounterHiRes();
    }
    #if !SOURCE_SYNC_STATE_WITH_REMOTE_UI
    juce::ignoreUnused(parentTree, childWhichHasBeenRemoved, indexFromWhichChildWasRemoved);
    return;
    #endif
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
