/*
  ==============================================================================

    This file was auto-generated!

    It contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "api_key.h"

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
    tmpDownloadLocation = File::getSpecialLocation(File::userDocumentsDirectory).getChildFile("FreesoundSimpleSampler");
    tmpDownloadLocation.deleteRecursively();
    tmpDownloadLocation.createDirectory();
    midicounter = 1;
    startTime = Time::getMillisecondCounterHiRes() * 0.001;
    
    // Start with a default random query
    std::vector<String> queries = {"wood", "metal", "glass", "percussion", "cat", "hit", "drums"};
    auto randomInt = juce::Random::getSystemRandom().nextInt(queries.size());
    makeQueryAndLoadSounds(queries[randomInt]);
}

SourceSamplerAudioProcessor::~SourceSamplerAudioProcessor()
{
    // Deletes the tmp directory so downloaded files do not stay there
    tmpDownloadLocation.deleteRecursively();
    for (int i = 0; i < downloadTasksToDelete.size(); i++) {
        downloadTasksToDelete.at(i).reset();
    }
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
    // Use this method as the place to do any pre-playback
    // initialisation that you need..
    sampler.setCurrentPlaybackSampleRate(sampleRate);
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
    midiMessages.addEvents(midiFromEditor, 0, INT_MAX, 0);
    midiFromEditor.clear();
    sampler.renderNextBlock(buffer, midiMessages, 0, buffer.getNumSamples());
    midiMessages.clear();
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

void SourceSamplerAudioProcessor::makeQueryAndLoadSounds(const String& query)
{
    std::cout << "Querying new sounds for: " << query << std::endl;
    FreesoundClient client(FREESOUND_API_KEY);
    SoundList list = client.textSearch(query, "duration:[0 TO 0.5]", "score", 1, -1, 150, "id,name,username,license,previews");
    Array<FSSound> sounds = list.toArrayOfSounds();
    std::random_shuffle(sounds.begin(), sounds.end());
    sounds.resize(16);
    std::vector<juce::StringArray> soundsInfo;
    for (int i=0; i<sounds.size(); i++){
        FSSound sound = sounds[i];
        StringArray soundData;
        soundData.add(sound.name);
        soundData.add(sound.user);
        soundData.add(sound.license);
        soundsInfo.push_back(soundData);
    }
    newSoundsReady(sounds, query, soundsInfo);
}

void SourceSamplerAudioProcessor::newSoundsReady (Array<FSSound> sounds, String textQuery, std::vector<juce::StringArray> soundsInfo)
{
    // This method is called by the FreesoundSearchComponent when a new query has
    // been made and new sounda have been selected for loading into the sampler.
    // This methods downloads the sounds, sotres in tmp directory and...
    
    // Download the sounds
    std::cout << "Downloading new sounds" << std::endl;
    FreesoundClient client(FREESOUND_API_KEY);
    for (int i=0; i<sounds.size(); i++){
        File location = tmpDownloadLocation.getChildFile(sounds[i].id).withFileExtension("mp3");
        //std::cout << location.getFullPathName() << std::endl;
        std::unique_ptr<URL::DownloadTask> downloadTask = client.downloadMP3SoundPreview(sounds[i], location);
        downloadTasksToDelete.push_back(std::move(downloadTask));
    }
    query = textQuery;
    soundsArray = soundsInfo;
    sendActionMessage("SHOULD_UPDATE_SOUNDS_TABLE");
    
    // Load the sounds in the sampler
    std::cout << "Loading downloaded sounds to sampler" << std::endl;
    setSources();
    
    std::cout << "All done and ready!" << std::endl << std::endl;
}

void SourceSamplerAudioProcessor::setSources()
{
    int poliphony = 16;
    int maxLength = 10;
    for (int i = 0; i < poliphony; i++) {
        sampler.addVoice(new SamplerVoice());
    }

    if(audioFormatManager.getNumKnownFormats() == 0){
        audioFormatManager.registerBasicFormats();
    }

    Array<File> files = tmpDownloadLocation.findChildFiles(2, false);
    for (int i = 0; i < files.size(); i++) {
        std::unique_ptr<AudioFormatReader> reader(audioFormatManager.createReaderFor(files[i]));
        BigInteger notes;
        notes.setRange(i * 8, i * 8 + 7, true);
        sampler.addSound(new SamplerSound(String(i), *reader, notes, i*8, 0, maxLength, maxLength));
        //reader.release();
    }
}

void SourceSamplerAudioProcessor::addToMidiBuffer(int notenumber)
{

    MidiMessage message = MidiMessage::noteOn(10, notenumber, (uint8)100);
    double timestamp = Time::getMillisecondCounterHiRes() * 0.001 - getStartTime();
    message.setTimeStamp(timestamp);

    auto sampleNumber = (int)(timestamp * getSampleRate());

    midiFromEditor.addEvent(message,sampleNumber);

    //auto messageOff = MidiMessage::noteOff(message.getChannel(), message.getNoteNumber());
    //messageOff.setTimeStamp(Time::getMillisecondCounterHiRes() * 0.001 - startTime);
    //midiFromEditor.addEvent(messageOff,sampleNumber+1);

}

double SourceSamplerAudioProcessor::getStartTime(){
    return startTime;
}

bool SourceSamplerAudioProcessor::isArrayNotEmpty()
{
    return soundsArray.size() != 0;
}

String SourceSamplerAudioProcessor::getQuery()
{
    return query;
}

std::vector<juce::StringArray> SourceSamplerAudioProcessor::getData()
{
    return soundsArray;
}

//==============================================================================
// This creates new instances of the plugin..
AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SourceSamplerAudioProcessor();
}
