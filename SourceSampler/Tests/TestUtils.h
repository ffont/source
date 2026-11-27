/*
  ==============================================================================

    TestUtils.h - helpers shared by the unit tests

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>


namespace TestUtils
{
    /** A temporary folder deleted when the object goes out of scope. */
    struct TemporaryFolder
    {
        TemporaryFolder()
            : folder (juce::File::getSpecialLocation (juce::File::tempDirectory)
                          .getChildFile ("SourceSamplerTests-" + juce::Uuid().toString()))
        {
            folder.createDirectory();
        }
        ~TemporaryFolder() { folder.deleteRecursively(); }
        juce::File folder;
    };

    /** Writes a mono sine wave WAV file and returns it. */
    inline juce::File writeSineWav (const juce::File& file, double frequency, double seconds, double sampleRate = 44100.0, float amplitude = 0.5f)
    {
        const int numSamples = (int) (seconds * sampleRate);
        juce::AudioBuffer<float> buffer (1, numSamples);
        for (int i = 0; i < numSamples; ++i)
            buffer.setSample (0, i, amplitude * (float) std::sin (2.0 * juce::MathConstants<double>::pi * frequency * i / sampleRate));

        file.deleteFile();
        juce::WavAudioFormat format;
        std::unique_ptr<juce::OutputStream> stream = std::make_unique<juce::FileOutputStream> (file);
        auto options = juce::AudioFormatWriterOptions{}.withSampleRate (sampleRate).withNumChannels (1).withBitsPerSample (24);
        if (auto writer = format.createWriterFor (stream, options))
            writer->writeFromAudioSampleBuffer (buffer, 0, numSamples);
        return file;
    }

    /** Runs the message loop until the condition is true or the timeout expires. Returns the condition value. */
    inline bool runMessageLoopUntil (std::function<bool()> condition, int timeoutMs = 5000)
    {
        const auto end = juce::Time::getMillisecondCounter() + (juce::uint32) timeoutMs;
        while (! condition())
        {
            if (juce::Time::getMillisecondCounter() > end)
                return false;
            juce::MessageManager::getInstance()->runDispatchLoopUntil (10);
        }
        return true;
    }

    inline void runMessageLoopFor (int ms)
    {
        juce::MessageManager::getInstance()->runDispatchLoopUntil (ms);
    }

    inline float rms (const juce::AudioBuffer<float>& buffer)
    {
        float total = 0.0f;
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            total += buffer.getRMSLevel (ch, 0, buffer.getNumSamples());
        return total / (float) juce::jmax (1, buffer.getNumChannels());
    }

    inline int countPositiveZeroCrossings (const juce::AudioBuffer<float>& buffer, int channel = 0)
    {
        int count = 0;
        const float* data = buffer.getReadPointer (channel);
        for (int i = 1; i < buffer.getNumSamples(); ++i)
            if (data[i - 1] < 0.0f && data[i] >= 0.0f)
                ++count;
        return count;
    }
}
