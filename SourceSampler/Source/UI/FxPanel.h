/*
  ==============================================================================

    FxPanel.h

    "FX" tab of the native UI: one card per effect (built from the effects
    parameters table, see SourceFxChain.h) plus the reverb card.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "SourceLookAndFeel.h"
#include "SourceFxChain.h"


class FxPanel : public juce::Component
{
public:
    explicit FxPanel (SourceActionSender actionSender);

    /** Updates the controls from the preset state (message thread). */
    void updateFromState (const juce::ValueTree& presetState);
    void resized() override;

private:
    struct Control
    {
        int paramIndex = -1;
        juce::Label label;
        juce::Slider slider;
        juce::ToggleButton toggle;
    };
    struct EffectCard
    {
        juce::String name;
        std::unique_ptr<CardComponent> card;
        juce::ToggleButton enabledToggle;
        int enabledParamIndex = -1;
        juce::OwnedArray<Control> controls;
    };
    struct ReverbControl
    {
        juce::Identifier id;
        juce::Label label;
        juce::Slider slider;
    };

    void sendParameter (int paramIndex, float value);
    void sendReverbParameters();
    void layoutCard (EffectCard& effect);

    SourceActionSender actions;
    juce::OwnedArray<EffectCard> effects;
    CardComponent reverbCard { "Reverb" };
    std::array<ReverbControl, 6> reverbControls;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FxPanel)
};
