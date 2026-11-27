/*
  ==============================================================================

    SourceLink.cpp

  ==============================================================================
*/

#include "SourceLink.h"
#include <ableton/LinkAudio.hpp>


struct SourceLink::Impl
{
    explicit Impl (const std::string& name)
        : link (120.0, name),
          sink (link, "SOURCE", (size_t) (2 * maxBlockSize)) {}

    ableton::LinkAudio link;
    ableton::LinkAudioSink sink;  // Only used from the audio thread once created
};


SourceLink::SourceLink (juce::File file) : settingsFile (std::move (file))
{
    auto settings = juce::XmlDocument::parse (settingsFile);
    const juce::String defaultName = "SOURCE (" + juce::SystemStats::getComputerName() + ")";
    const juce::String name = settings != nullptr ? settings->getStringAttribute ("peerName", defaultName) : defaultName;
    impl = std::make_unique<Impl> (name.toStdString());
    if (settings != nullptr)
    {
        impl->link.enableLinkAudio (settings->getBoolAttribute ("audioEnabled", true));
        impl->link.enable (settings->getBoolAttribute ("enabled", false));
    }
    else
    {
        impl->link.enableLinkAudio (true);  // Link itself is off until enabled by the user
    }
}

SourceLink::~SourceLink()
{
    impl->link.enable (false);
}

void SourceLink::setEnabled (bool shouldBeEnabled)
{
    impl->link.enable (shouldBeEnabled);
    saveSettings();
}

bool SourceLink::isEnabled() const { return impl->link.isEnabled(); }

void SourceLink::setAudioEnabled (bool shouldBeEnabled)
{
    impl->link.enableLinkAudio (shouldBeEnabled);
    saveSettings();
}

bool SourceLink::isAudioEnabled() const { return impl->link.isLinkAudioEnabled(); }

void SourceLink::setPeerName (const juce::String& name)
{
    if (name.trim().isEmpty())
        return;
    impl->link.setPeerName (name.trim().toStdString());
    saveSettings();
}

juce::String SourceLink::getPeerName() const { return juce::String (impl->link.peerName()); }

int SourceLink::getNumPeers() const { return (int) impl->link.numPeers(); }

double SourceLink::getTempo() const { return impl->link.captureAppSessionState().tempo(); }

void SourceLink::process (const juce::AudioBuffer<float>& buffer, int numSamples, double sampleRate) noexcept
{
    auto& link = impl->link;
    if (numSamples <= 0 || sampleRate <= 0.0 || buffer.getNumChannels() == 0 || ! link.isEnabled() || ! link.isLinkAudioEnabled())
        return;

    auto& sink = impl->sink;
    const size_t numInterleavedSamples = (size_t) numSamples * 2;
    if (numInterleavedSamples > sink.maxNumSamples())
    {
        sink.requestMaxNumSamples (numInterleavedSamples);  // Bigger buffers will be available from next blocks
        return;
    }

    // A buffer is only available if some peer receives the channel
    ableton::LinkAudioSink::BufferHandle handle (sink);
    if (! handle)
        return;

    // Interleaved 16-bit stereo (mono outputs are duplicated)
    const float* left = buffer.getReadPointer (0);
    const float* right = buffer.getReadPointer (buffer.getNumChannels() > 1 ? 1 : 0);
    for (int i = 0; i < numSamples; ++i)
    {
        handle.samples[2 * i] = (int16_t) juce::jlimit (-32768.0f, 32767.0f, left[i] * 32767.0f);
        handle.samples[2 * i + 1] = (int16_t) juce::jlimit (-32768.0f, 32767.0f, right[i] * 32767.0f);
    }

    // Timing information (SOURCE doesn't follow the Link tempo yet, so the buffer is just placed at the current time)
    const auto sessionState = link.captureAudioSessionState();
    constexpr double quantum = 4.0;
    const double beatsAtBufferBegin = sessionState.beatAtTime (link.clock().micros(), quantum);
    handle.commit (sessionState, beatsAtBufferBegin, quantum, (size_t) numSamples, 2, (uint32_t) sampleRate);
}

void SourceLink::saveSettings() const
{
    juce::XmlElement settings ("LINK_SETTINGS");
    settings.setAttribute ("enabled", isEnabled());
    settings.setAttribute ("audioEnabled", isAudioEnabled());
    settings.setAttribute ("peerName", getPeerName());
    settings.writeTo (settingsFile);
}
