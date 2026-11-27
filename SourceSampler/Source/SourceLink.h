/*
  ==============================================================================

    SourceLink.h

    Ableton Link and Link Audio (iPad standalone app only, see SOURCE_LINK in
    CMakeLists.txt). Link connects SOURCE to a Link session on the local
    network (shared tempo, not used by SOURCE yet). Link Audio announces an
    audio channel with SOURCE's output to the session, so other peers (e.g.
    Ableton Live) can receive it through the network.

    Settings (enabled, audio enabled, name) are stored in a file and restored
    when the app starts.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <memory>


class SourceLink
{
public:
    explicit SourceLink (juce::File settingsFile);
    ~SourceLink();

    // Message thread
    void setEnabled (bool shouldBeEnabled);
    bool isEnabled() const;
    void setAudioEnabled (bool shouldBeEnabled);
    bool isAudioEnabled() const;
    /** Name of this device in the Link session (as shown in Live). */
    void setPeerName (const juce::String& name);
    juce::String getPeerName() const;
    int getNumPeers() const;
    double getTempo() const;

    /** Audio thread: sends the block (first two channels) to the Link Audio channel, if Link Audio is enabled and
        some peer is receiving it. */
    void process (const juce::AudioBuffer<float>& buffer, int numSamples, double sampleRate) noexcept;

    static constexpr int maxBlockSize = 4096;

private:
    void saveSettings() const;

    struct Impl;
    std::unique_ptr<Impl> impl;
    juce::File settingsFile;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SourceLink)
};
