/*
  ==============================================================================

    MetronomeSynth.cpp
    Created: 22 Mar 2021 3:27:51pm
    Author:  Frederic Font Corbera

  ==============================================================================
*/

#include "MetronomeSynth.h"

#define MAX_VOICES 2

void MetronomeSynth::setup() {
    // add voices to our sampler
    for (int i = 0; i < MAX_VOICES; i++) {
        addVoice(new SamplerVoice());
    }

    audioFormatManager.registerBasicFormats();

    std::unique_ptr<AudioFormatReader> reader(audioFormatManager.createReaderFor(
                                                                                 std::make_unique<MemoryInputStream>(BinaryData::metro1_down_wav, BinaryData::metro1_down_wavSize, false)
    ));
            
    BigInteger allNotes;
    allNotes.setRange(0, 128, true);

    addSound(new SamplerSound("default", *reader, allNotes, 60, 0, 10, 10.0));
}
