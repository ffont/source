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
    soundsDownloadLocation = File("/udata/source/");
    #else
    soundsDownloadLocation = File::getSpecialLocation(File::userDocumentsDirectory).getChildFile("FreesoundSimpleSampler");
    #endif
    
    midicounter = 1;
    startTime = Time::getMillisecondCounterHiRes() * 0.001;
    
    // Start with a default random query
    std::vector<String> queries = {"wood", "metal", "glass", "percussion", "cat", "hit", "drums"};
    auto randomInt = juce::Random::getSystemRandom().nextInt(queries.size());
    #if ELK_BUILD
    int numSounds = 8;  // Use less sounds by default in ELK so it starts faster
    #else
    int numSounds = 16;
    #endif
    float maxSoundLength = 0.5;
    makeQueryAndLoadSounds(queries[randomInt], numSounds, maxSoundLength);
    
    // Configure processor to listen messages from server interface
    serverInterface.addActionListener(this);
}

SourceSamplerAudioProcessor::~SourceSamplerAudioProcessor()
{
    // Deletes the tmp directory so downloaded files do not stay there
    //if (soundsDownloadLocation.exists()){
    //    soundsDownloadLocation.deleteRecursively();
    //}
    
    // Delete download task objects
    for (int i = 0; i < downloadTasks.size(); i++) {
        downloadTasks.at(i).reset();
    }
    
    // Remove this as a listener from serverInterface
    serverInterface.removeActionListener(this);
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
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int SourceSamplerAudioProcessor::getCurrentProgram()
{
    return 0;
}

void SourceSamplerAudioProcessor::setCurrentProgram (int index)
{
}

const String SourceSamplerAudioProcessor::getProgramName (int index)
{
    return {};
}

void SourceSamplerAudioProcessor::changeProgramName (int index, const String& newName)
{
}

//==============================================================================
void SourceSamplerAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    std::cout << "Called prepareToPlay with sampleRate " << sampleRate << " and block size " << samplesPerBlock << std::endl;
    
    sampler.prepare ({ sampleRate, (juce::uint32) samplesPerBlock, 2 });
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
        std::cout << "Calling aconnect to setup MIDI in to sushi connection " << std::endl;
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
    
    midiMessages.clear(); // Why clearing here?
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
void SourceSamplerAudioProcessor::getStateInformation (MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void SourceSamplerAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
}

//==============================================================================

void SourceSamplerAudioProcessor::actionListenerCallback (const String &message)
{
    std::cout << "Action message: " << message << std::endl;
    
    if (message.startsWith(String(ACTION_NEW_QUERY_TRIGGERED_FROM_SERVER))){
        String serializedParameters = message.substring(String(ACTION_NEW_QUERY_TRIGGERED_FROM_SERVER).length() + 1);
        StringArray tokens;
        tokens.addTokens (serializedParameters, (String)SERIALIZATION_SEPARATOR, "");
        String query = tokens[0];
        int numSounds = tokens[1].getIntValue();
        float maxSoundLength = tokens[2].getFloatValue();
        std::cout << "New query triggered from server: " << query << " " << numSounds << " " << maxSoundLength << std::endl;
        makeQueryAndLoadSounds(query, numSounds, maxSoundLength);
        
    } else if (message.startsWith(String(ACTION_SET_MIDI_ROOT_NOTE_OFFSET))){
        int newOffset = message.substring(String(ACTION_SET_MIDI_ROOT_NOTE_OFFSET).length() + 1).getIntValue();
        std::cout << "Offsetting midi root note by: " << newOffset << std::endl;
        setSources(newOffset);
        
    } else if (message.startsWith(String(ACTION_SET_SOUND_PARAMETER_FLOAT))){
        String serializedParameters = message.substring(String(ACTION_SET_SOUND_PARAMETER_FLOAT).length() + 1);
        StringArray tokens;
        tokens.addTokens (serializedParameters, (String)SERIALIZATION_SEPARATOR, "");
        int soundIndex = tokens[0].getIntValue();  // -1 means all sounds
        String parameterName = tokens[1];
        float parameterValue = tokens[2].getFloatValue();
        std::cout << "Setting parameter " << parameterName << " of sound " << soundIndex << " to value " << parameterValue << std::endl;
        if ((soundIndex >= 0) && (soundIndex < sampler.getNumSounds() - 1)){
            if (soundIndex < sampler.getNumSounds() - 1){
                auto* sound = static_cast<SourceSamplerSound*> (sampler.getSound(soundIndex).get());
                sound->setParameterByNameFloat(parameterName, parameterValue);
            }
        } else {
            for (int i=0; i<sampler.getNumSounds(); i++){
                auto* sound = static_cast<SourceSamplerSound*> (sampler.getSound(i).get());
                sound->setParameterByNameFloat(parameterName, parameterValue);
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
        std::cout << "Setting new reverb parameters " << std::endl;
        sampler.setReverbParameters(reverbParameters);
    }    
}

//==============================================================================

void SourceSamplerAudioProcessor::makeQueryAndLoadSounds(const String& query, int numSounds, float maxSoundLength)
{
    if (isQueryinAndDownloadingSounds){
        std::cout << "Source is already busy querying and downloading sounds" << std::endl;
    }
    
    std::cout << "Querying new sounds for: " << query << std::endl;
    isQueryinAndDownloadingSounds = true;
    FreesoundClient client(FREESOUND_API_KEY);
    auto filter = "duration:[0 TO " + (String)maxSoundLength + "]";
    SoundList list = client.textSearch(query, filter, "score", 0, -1, 150, "id,name,username,license,previews");
    if (list.getCount() > 0){
        std::cout << "Query got " << list.getCount() << " results" << std::endl;
        Array<FSSound> sounds = list.toArrayOfSounds();
        std::random_shuffle(sounds.begin(), sounds.end());
        sounds.resize(jmin(numSounds, list.getCount()));
        std::vector<juce::StringArray> soundsInfo;
        for (int i=0; i<sounds.size(); i++){
            FSSound sound = sounds[i];
            StringArray soundData;
            soundData.add(sound.id);
            soundData.add(sound.name);
            soundData.add(sound.user);
            soundData.add(sound.license);
            soundsInfo.push_back(soundData);
        }
        newSoundsReadyToDownload(sounds, query, soundsInfo);
    } else {
        std::cout << "Query got no results..." << std::endl;
    }
}

void SourceSamplerAudioProcessor::newSoundsReadyToDownload (Array<FSSound> sounds, String textQuery, std::vector<juce::StringArray> soundsInfo)
{
    // This method is called by the FreesoundSearchComponent when a new query has
    // been made and new sounda have been selected for loading into the sampler.
    // This methods downloads the sounds, sotres in tmp directory and...
    
    //
    if (!soundsDownloadLocation.exists()){
        soundsDownloadLocation.createDirectory();
    }
    
    std::vector<juce::StringArray> soundsInfoDownloadedOk;  // Here we will add info of soudns that downloaded ok
    
    #if !ELK_BUILD
    
    // Download the sounds within the plugin
    
    // Download the sounds
    std::cout << "Downloading new sounds..." << std::endl;
    FreesoundClient client(FREESOUND_API_KEY);
    for (int i=0; i<sounds.size(); i++){
        File location = soundsDownloadLocation.getChildFile(sounds[i].id).withFileExtension("ogg");
        if (!location.exists()){  // Dont' re-download if file already exists
            std::unique_ptr<URL::DownloadTask> downloadTask = client.downloadOGGSoundPreview(sounds[i], location);
            downloadTasks.push_back(std::move(downloadTask));
        }
    }
    
    std::cout << "Waiting for all download tasks to finish..." << std::endl;
    int64 startedWaitingTime = Time::getCurrentTime().toMilliseconds();
    bool allFinished = false;
    while (!allFinished){
        allFinished = true;
        for (int i=0; i<downloadTasks.size(); i++){
            if (!downloadTasks[i]->isFinished()){
                allFinished = false;
            }
        }
        if (Time::getCurrentTime().toMilliseconds() - startedWaitingTime > MAX_DOWNLOAD_WAITING_TIME_MS){
            // If more than 10 seconds, mark all as finished
            allFinished = true;
        }
    }
    
    // Make sure that files were downloaded correctly and remove those sounds for which files were not downloaded
    for (int i=0; i<downloadTasks.size(); i++){
        if ((downloadTasks[i]->isFinished()) && (!downloadTasks[i]->hadError()) && (soundsDownloadLocation.getChildFile(sounds[i].id).withFileExtension("ogg").exists()) && (soundsDownloadLocation.getChildFile(sounds[i].id).withFileExtension("ogg").getSize() > 0)){
            soundsInfoDownloadedOk.push_back(soundsInfo[i]);
        }
    }
    
    #else
    // If inside ELK build, download the sounds with the python server as it seems to be much much faster
    std::cout << "Sending download task to python server..." << std::endl;
    URL url = URL("http://localhost:8123/download_sounds");
    
    String urlsParam = "";
    for (int i=0; i<sounds.size(); i++){
        urlsParam = urlsParam + sounds[i].getOGGPreviewURL().toString(false) + ",";
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
        std::cout << "Response with " << statusCode << ": " << resp << std::endl;
        
        // Make sure that files were downloaded correctly and remove those sounds for which files were not downloaded
        for (int i=0; i<sounds.size(); i++){
            if ((soundsDownloadLocation.getChildFile(sounds[i].id).withFileExtension("ogg").exists()) && (soundsDownloadLocation.getChildFile(sounds[i].id).withFileExtension("ogg").getSize() > 0)){
                soundsInfoDownloadedOk.push_back(soundsInfo[i]);
            }
        }
    } else {
        std::cout << "Downloading in server failed!" << std::endl;
    }
    
    
    #endif
    
    // Store info about the query and tell UI component(s) to update
    query = textQuery;
    loadedSoundsInfo = soundsInfoDownloadedOk;
    sendActionMessage(String(ACTION_SHOULD_UPDATE_SOUNDS_TABLE));
    
    // Load the sounds in the sampler
    setSources(0);
    
    std::cout << "All done and ready!" << std::endl << std::endl;
    isQueryinAndDownloadingSounds = false;
}

void SourceSamplerAudioProcessor::setSources(int midiNoteRootOffset)
{
    sampler.clearSounds();
    
    if(audioFormatManager.getNumKnownFormats() == 0){
        audioFormatManager.registerBasicFormats();
    }

    int attackTime = 0;
    int releaseTime = 1;
    int maxSampleLength = 20;  // This is unrelated to the maxSoundLength of the makeQueryAndLoadSounds method
    int nSounds = (int)loadedSoundsInfo.size();
    
    std::cout << "Loading " << nSounds << " sounds to sampler" << std::endl;
    if (nSounds > 0){
        int nNotesPerSound = 128 / nSounds;
        for (int i = 0; i < nSounds; i++) {
            String soundID = loadedSoundsInfo[i][0];
            File audioSample = soundsDownloadLocation.getChildFile(soundID).withFileExtension("ogg");
            if (audioSample.exists() && audioSample.getSize() > 0){  // Check that file exists and is not empty
                std::unique_ptr<AudioFormatReader> reader(audioFormatManager.createReaderFor(audioSample));
                int midiNoteForNormalPitch = i * nNotesPerSound + nNotesPerSound / 2 + midiNoteRootOffset;
                BigInteger midiNotes;
                midiNotes.setRange(i * nNotesPerSound, nNotesPerSound, true);
                std::cout << "- Adding sound " << audioSample.getFullPathName() << " with midi root note " << midiNoteForNormalPitch << std::endl;
                sampler.addSound(new SourceSamplerSound(String(i), *reader, midiNotes, midiNoteForNormalPitch, attackTime, releaseTime, maxSampleLength));
            } else {
                std::cout << "- Skipping sound " << soundID << " (no file found or file is empty)" << std::endl;
            }
        }
    }
    std::cout << "Sampler sources configured with " << sampler.getNumSounds() << " sounds and " << sampler.getNumVoices() << " voices" << std::endl;
}

void SourceSamplerAudioProcessor::addToMidiBuffer(int soundNumber)
{
    
    int nSounds = (int)loadedSoundsInfo.size();
    if (nSounds > 0){
        int nNotesPerSound = 128 / nSounds;
        int midiNoteForNormalPitch = soundNumber * nNotesPerSound + nNotesPerSound / 2;
        
        MidiMessage message = MidiMessage::noteOn(1, midiNoteForNormalPitch, (uint8)127);
        double timestamp = Time::getMillisecondCounterHiRes() * 0.001 - getStartTime();
        message.setTimeStamp(timestamp);

        auto sampleNumber = (int)(timestamp * getSampleRate());

        midiFromEditor.addEvent(message,sampleNumber);
    }
}

double SourceSamplerAudioProcessor::getStartTime(){
    return startTime;
}

bool SourceSamplerAudioProcessor::isArrayNotEmpty()
{
    return loadedSoundsInfo.size() != 0;
}

String SourceSamplerAudioProcessor::getQuery()
{
    return query;
}

std::vector<juce::StringArray> SourceSamplerAudioProcessor::getData()
{
    return loadedSoundsInfo;
}

//==============================================================================
// This creates new instances of the plugin..
AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SourceSamplerAudioProcessor();
}
