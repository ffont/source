/*
  ==============================================================================

    FxPanel.cpp

  ==============================================================================
*/

#include "FxPanel.h"
#include "helpers_source.h"

namespace
{
    constexpr int rowHeight = 22;
    constexpr int rowGap = 5;
    constexpr int labelWidth = 86;

    int numDecimals (float interval)
    {
        if (interval >= 1.0f) return 0;
        if (interval >= 0.1f) return 1;
        return 2;
    }
}


FxPanel::FxPanel (SourceActionSender actionSender) : actions (std::move (actionSender))
{
    const auto& infos = SourceFx::getParamInfos();
    for (const auto& effectName : SourceFx::getEffectNames())
    {
        auto* effect = effects.add (new EffectCard());
        effect->name = effectName;
        effect->card = std::make_unique<CardComponent> (effectName);
        addAndMakeVisible (*effect->card);

        for (int i = 0; i < (int) infos.size(); ++i)
        {
            const auto& info = infos[(size_t) i];
            if (effectName != info.effect)
                continue;
            const juce::String id (info.id);
            if (id.endsWith ("Sync") || id.endsWith ("Division"))
                continue;  // Part of the control of the synced parameter (see below)
            if (id.endsWith ("Enabled"))
            {
                // The on/off switch goes in the title bar of the card
                effect->enabledParamIndex = i;
                effect->enabledToggle.setButtonText ("On");
                effect->enabledToggle.setTooltip ("Switch " + effectName.toLowerCase() + " on/off");
                effect->enabledToggle.onClick = [this, effect, i] { sendParameter (i, effect->enabledToggle.getToggleState() ? 1.0f : 0.0f); };
                effect->card->addAndMakeVisible (effect->enabledToggle);
                continue;
            }
            auto* control = effect->controls.add (new Control());
            control->paramIndex = i;
            if (info.isToggle)
            {
                control->toggle.setButtonText (info.label);
                control->toggle.onClick = [this, control, i] { sendParameter (i, control->toggle.getToggleState() ? 1.0f : 0.0f); };
                effect->card->addAndMakeVisible (control->toggle);
            }
            else
            {
                control->label.setText (info.label, juce::dontSendNotification);
                control->label.setColour (juce::Label::textColourId, SourceColours::dimText);
                control->label.setFont (juce::Font (juce::FontOptions (12.0f)));
                effect->card->addAndMakeVisible (control->label);
                auto& slider = control->slider;
                slider.setSliderStyle (juce::Slider::LinearHorizontal);
                slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 18);
                slider.setRange (info.minValue, info.maxValue, info.interval);
                if (info.skewMidPoint > 0.0f)
                    slider.setSkewFactorFromMidPoint (info.skewMidPoint);
                slider.setNumDecimalPlacesToDisplay (numDecimals (info.interval));
                if (juce::String (info.unit).isNotEmpty())
                    slider.setTextValueSuffix ((juce::String (info.unit) == ":1" ? "" : " ") + juce::String (info.unit));
                slider.setDoubleClickReturnValue (true, info.defaultValue);
                const int decimals = numDecimals (info.interval);
                slider.textFromValueFunction = [decimals] (double v)
                {
                    if (std::abs (v) < 0.5 * std::pow (10.0, -decimals))
                        v = 0.0;  // No "-0.00" (the slider adds the units suffix)
                    return juce::String (v, decimals);
                };
                slider.setValue (info.defaultValue, juce::dontSendNotification);
                slider.updateText();
                slider.onValueChange = [this, control, i] { sendParameter (i, (float) control->slider.getValue()); };
                effect->card->addAndMakeVisible (slider);

                // Tempo sync ("<id>Sync" and "<id>Division" parameters)
                control->syncParamIndex = SourceFx::findParam (id + "Sync");
                control->divisionParamIndex = SourceFx::findParam (id + "Division");
                if (control->syncParamIndex >= 0 && control->divisionParamIndex >= 0)
                {
                    const int syncIndex = control->syncParamIndex, divisionIndex = control->divisionParamIndex;
                    control->syncButton.setClickingTogglesState (true);
                    control->syncButton.setTooltip ("Sync to the tempo (Ableton Link, the host or the BPM next to the voices)");
                    control->syncButton.onClick = [this, control, syncIndex]
                    {
                        control->updateSyncVisibility();
                        sendParameter (syncIndex, control->syncButton.getToggleState() ? 1.0f : 0.0f);
                    };
                    effect->card->addAndMakeVisible (control->syncButton);
                    for (const auto& division : SourceHelpers::getSyncDivisions())
                        control->divisionCombo.addItem (division.name, control->divisionCombo.getNumItems() + 1);
                    control->divisionCombo.setTooltip (id.endsWith ("Time") ? "Delay time as a note division (D = dotted, T = triplet)"
                                                                            : "One cycle per note division (D = dotted, T = triplet)");
                    control->divisionCombo.onChange = [this, control, divisionIndex]
                    {
                        sendParameter (divisionIndex, (float) juce::jmax (0, control->divisionCombo.getSelectedItemIndex()));
                    };
                    effect->card->addChildComponent (control->divisionCombo);
                }
            }
        }
    }

    // Reverb (its on/off switch is in the effects parameters table, the other parameters are set as a whole)
    addAndMakeVisible (reverbCard);
    reverbEnabledToggle.setTooltip ("Switch reverb on/off");
    reverbEnabledToggle.onClick = [this]
    {
        actions.send (ACTION_SET_FX_PARAMETER, { SourceFx::getParamInfo (SourceFx::Param::reverbEnabled).id, reverbEnabledToggle.getToggleState() ? "1" : "0" });
    };
    reverbCard.addAndMakeVisible (reverbEnabledToggle);
    const std::array<std::pair<juce::Identifier, const char*>, 6> reverbParams {{
        { SourceIDs::reverbRoomSize, "Room size" }, { SourceIDs::reverbDamping, "Damping" }, { SourceIDs::reverbWidth, "Width" },
        { SourceIDs::reverbFreezeMode, "Freeze" }, { SourceIDs::reverbWetLevel, "Wet level" }, { SourceIDs::reverbDryLevel, "Dry level" } }};
    for (size_t i = 0; i < reverbControls.size(); ++i)
    {
        auto& control = reverbControls[i];
        control.id = reverbParams[i].first;
        control.label.setText (reverbParams[i].second, juce::dontSendNotification);
        control.label.setColour (juce::Label::textColourId, SourceColours::dimText);
        control.label.setFont (juce::Font (juce::FontOptions (12.0f)));
        reverbCard.addAndMakeVisible (control.label);
        control.slider.setSliderStyle (juce::Slider::LinearHorizontal);
        control.slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 18);
        control.slider.setRange (0.0, 1.0, 0.01);
        control.slider.onValueChange = [this] { sendReverbParameters(); };
        reverbCard.addAndMakeVisible (control.slider);
    }
}

void FxPanel::Control::updateSyncVisibility()
{
    if (syncParamIndex < 0)
        return;
    const bool sync = syncButton.getToggleState();
    slider.setVisible (! sync);
    divisionCombo.setVisible (sync);
}

void FxPanel::sendParameter (int paramIndex, float value)
{
    actions.send (ACTION_SET_FX_PARAMETER, { SourceFx::getParamInfos()[(size_t) paramIndex].id, juce::String (value) });
}

void FxPanel::sendReverbParameters()
{
    // Order defined by ACTION_SET_REVERB_PARAMETERS: roomSize, damping, wetLevel, dryLevel, width, freezeMode
    auto valueOf = [this] (const juce::Identifier& id)
    {
        for (auto& control : reverbControls)
            if (control.id == id)
                return juce::String (control.slider.getValue());
        return juce::String();
    };
    actions.send (ACTION_SET_REVERB_PARAMETERS, { valueOf (SourceIDs::reverbRoomSize), valueOf (SourceIDs::reverbDamping),
                                                  valueOf (SourceIDs::reverbWetLevel), valueOf (SourceIDs::reverbDryLevel),
                                                  valueOf (SourceIDs::reverbWidth), valueOf (SourceIDs::reverbFreezeMode) });
}

void FxPanel::updateFromState (const juce::ValueTree& presetState)
{
    const auto& infos = SourceFx::getParamInfos();
    for (auto* effect : effects)
    {
        const bool enabled = effect->enabledParamIndex >= 0
                             && (float) presetState.getProperty (infos[(size_t) effect->enabledParamIndex].getIdentifier(), 0.0f) >= 0.5f;
        effect->enabledToggle.setToggleState (enabled, juce::dontSendNotification);
        for (auto* control : effect->controls)
        {
            const auto& info = infos[(size_t) control->paramIndex];
            const float value = presetState.getProperty (info.getIdentifier(), info.defaultValue);
            if (info.isToggle)
                control->toggle.setToggleState (value >= 0.5f, juce::dontSendNotification);
            else if (! control->slider.isMouseButtonDown())
                control->slider.setValue (value, juce::dontSendNotification);
            if (control->syncParamIndex >= 0)
            {
                const auto& syncInfo = infos[(size_t) control->syncParamIndex];
                const auto& divisionInfo = infos[(size_t) control->divisionParamIndex];
                const bool sync = (float) presetState.getProperty (syncInfo.getIdentifier(), syncInfo.defaultValue) >= 0.5f;
                if (sync != control->syncButton.getToggleState())
                {
                    control->syncButton.setToggleState (sync, juce::dontSendNotification);
                    control->updateSyncVisibility();
                }
                if (! control->divisionCombo.isPopupActive())
                    control->divisionCombo.setSelectedItemIndex ((int) (float) presetState.getProperty (divisionInfo.getIdentifier(), divisionInfo.defaultValue),
                                                                 juce::dontSendNotification);
                control->syncButton.setAlpha (enabled ? 1.0f : 0.45f);
                control->divisionCombo.setAlpha (enabled ? 1.0f : 0.45f);
            }
            // Controls of effects that are off are dimmed (but can still be edited)
            const float alpha = enabled ? 1.0f : 0.45f;
            control->slider.setAlpha (alpha);
            control->label.setAlpha (alpha);
            control->toggle.setAlpha (alpha);
        }
    }
    const bool reverbEnabled = (float) presetState.getProperty (SourceFx::getParamInfo (SourceFx::Param::reverbEnabled).getIdentifier(), 1.0f) >= 0.5f;
    reverbEnabledToggle.setToggleState (reverbEnabled, juce::dontSendNotification);
    for (auto& control : reverbControls)
    {
        if (! control.slider.isMouseButtonDown())
            control.slider.setValue ((float) presetState.getProperty (control.id, 0.0f), juce::dontSendNotification);
        control.slider.setAlpha (reverbEnabled ? 1.0f : 0.45f);
        control.label.setAlpha (reverbEnabled ? 1.0f : 0.45f);
    }
}

void FxPanel::layoutCard (EffectCard& effect)
{
    auto& card = *effect.card;
    effect.enabledToggle.setBounds (card.getLocalBounds().reduced (8, 6).removeFromTop (22).removeFromRight (52));
    auto c = card.getContentBounds();
    for (auto* control : effect.controls)
    {
        auto row = c.removeFromTop (rowHeight);
        c.removeFromTop (rowGap);
        if (SourceFx::getParamInfos()[(size_t) control->paramIndex].isToggle)
        {
            control->toggle.setBounds (row.removeFromLeft (labelWidth + 60));
        }
        else
        {
            control->label.setBounds (row.removeFromLeft (labelWidth));
            if (control->syncParamIndex >= 0)
            {
                control->syncButton.setBounds (row.removeFromRight (48));
                row.removeFromRight (6);
                control->divisionCombo.setBounds (row.withWidth (juce::jmin (row.getWidth(), 120)));  // Same place as the slider
            }
            control->slider.setBounds (row);
        }
    }
}

void FxPanel::resized()
{
    // Cards in processing order, in a grid (3 columns, or 2 in narrow layouts)
    std::vector<juce::Component*> cards;
    for (int i = 0; i < effects.size(); ++i)
    {
        if (effects[i]->name == "Compressor")
            cards.push_back (&reverbCard);  // Reverb goes between delay and compressor
        cards.push_back (effects[i]->card.get());
    }
    const int gap = 8;
    const int columns = getWidth() >= 1000 ? 3 : 2;
    const int rows = ((int) cards.size() + columns - 1) / columns;
    int maxControls = (int) reverbControls.size();
    for (auto* effect : effects)
        maxControls = juce::jmax (maxControls, effect->controls.size());
    const int cardHeight = juce::jmin ((getHeight() - (rows - 1) * gap) / rows, 26 + 16 + maxControls * (rowHeight + rowGap) + 6);
    const int cardWidth = (getWidth() - (columns - 1) * gap) / columns;
    for (size_t i = 0; i < cards.size(); ++i)
    {
        const int column = (int) i % columns, row = (int) i / columns;
        cards[i]->setBounds (column * (cardWidth + gap), row * (cardHeight + gap), cardWidth, cardHeight);
    }
    for (auto* effect : effects)
        layoutCard (*effect);

    reverbEnabledToggle.setBounds (reverbCard.getLocalBounds().reduced (8, 6).removeFromTop (22).removeFromRight (52));
    auto c = reverbCard.getContentBounds();
    for (auto& control : reverbControls)
    {
        auto row = c.removeFromTop (rowHeight);
        c.removeFromTop (rowGap);
        control.label.setBounds (row.removeFromLeft (labelWidth));
        control.slider.setBounds (row);
    }
}
