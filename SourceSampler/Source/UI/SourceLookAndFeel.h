/*
  ==============================================================================

    SourceLookAndFeel.h

    Colours, look and feel and small reusable widgets for the native UI. The
    palette follows the one used by the legacy browser-based UI.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "helpers_source.h"


namespace SourceColours
{
    const juce::Colour background   { 0xff000000 };
    const juce::Colour card         { 0xff303030 };
    const juce::Colour control      { 0xff4d4c4c };
    const juce::Colour controlHover { 0xff777676 };
    const juce::Colour accent       { 0xff4eb8a4 };  // rgb(78,184,164)
    const juce::Colour text         { 0xffffffff };
    const juce::Colour dimText      { 0xffa0a0a0 };
    const juce::Colour warning      { 0xffe0a040 };
}


class SourceLookAndFeel : public juce::LookAndFeel_V4
{
public:
    SourceLookAndFeel()
    {
        using namespace SourceColours;
        setColourScheme ({ background, card, card, control, text, accent, text, accent, text });

        setColour (juce::ResizableWindow::backgroundColourId, background);
        setColour (juce::TextButton::buttonColourId, control);
        setColour (juce::TextButton::buttonOnColourId, accent);
        setColour (juce::TextButton::textColourOffId, text);
        setColour (juce::TextButton::textColourOnId, juce::Colours::black);
        setColour (juce::ComboBox::backgroundColourId, control);
        setColour (juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
        setColour (juce::ComboBox::arrowColourId, dimText);
        setColour (juce::TextEditor::backgroundColourId, control);
        setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
        setColour (juce::TextEditor::focusedOutlineColourId, accent);
        setColour (juce::TextEditor::highlightColourId, accent.withAlpha (0.5f));
        setColour (juce::Slider::trackColourId, accent);
        setColour (juce::Slider::backgroundColourId, control);
        setColour (juce::Slider::thumbColourId, text);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxTextColourId, dimText);
        setColour (juce::Label::textColourId, text);
        setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::ScrollBar::thumbColourId, control);
        setColour (juce::PopupMenu::backgroundColourId, card);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, accent);
        setColour (juce::ToggleButton::tickColourId, accent);
        setColour (juce::ToggleButton::tickDisabledColourId, dimText);
        setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, accent);
        setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, accent.withAlpha (0.4f));
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour& backgroundColour,
                               bool isMouseOver, bool isButtonDown) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
        auto colour = button.getToggleState() ? button.findColour (juce::TextButton::buttonOnColourId) : backgroundColour;
        if (isButtonDown)
            colour = colour.brighter (0.2f);
        else if (isMouseOver)
            colour = SourceColours::controlHover;
        g.setColour (colour);
        g.fillRoundedRectangle (bounds, 4.0f);
    }

    juce::Label* createSliderTextBox (juce::Slider& slider) override
    {
        auto* label = LookAndFeel_V4::createSliderTextBox (slider);
        label->setColour (juce::Label::backgroundColourId, SourceColours::background.withAlpha (0.6f));
        label->setColour (juce::Label::textColourId, SourceColours::text);
        label->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
        label->setColour (juce::TextEditor::textColourId, SourceColours::text);
        label->setColour (juce::TextEditor::backgroundColourId, SourceColours::control);
        label->setFont (juce::Font (juce::FontOptions (12.0f)));
        return label;
    }

    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override
    {
        return juce::Font (juce::FontOptions (juce::jmin (13.0f, (float) buttonHeight * 0.6f)));
    }
};


/** A rounded panel with a title, like the "cards" of the web UI. Child components are laid out by the owner. */
class CardComponent : public juce::Component
{
public:
    explicit CardComponent (const juce::String& cardTitle = {}, juce::Colour colour = SourceColours::card)
        : title (cardTitle), backgroundColour (colour) {}

    void setTitle (const juce::String& newTitle) { title = newTitle; repaint(); }

    void paint (juce::Graphics& g) override
    {
        g.setColour (backgroundColour);
        g.fillRoundedRectangle (getLocalBounds().toFloat(), 5.0f);
        if (title.isNotEmpty())
        {
            g.setColour (SourceColours::text);
            g.setFont (juce::Font (juce::FontOptions (16.0f, juce::Font::bold)));
            g.drawText (title, getLocalBounds().reduced (10, 6).removeFromTop (20), juce::Justification::centredLeft);
        }
    }

    /** Area available for content (below the title). */
    juce::Rectangle<int> getContentBounds() const
    {
        auto b = getLocalBounds().reduced (10, 8);
        if (title.isNotEmpty())
            b.removeFromTop (26);
        return b;
    }

private:
    juce::String title;
    juce::Colour backgroundColour;
};


/** Small button that draws an icon (play, stop, remove) instead of text. */
class IconButton : public juce::Button
{
public:
    enum class Icon { play, stop, remove, previous, next, retry };

    IconButton (const juce::String& name, Icon iconToUse) : juce::Button (name), icon (iconToUse)
    {
        setTooltip (name);
    }

    void paintButton (juce::Graphics& g, bool isMouseOver, bool isButtonDown) override
    {
        auto bounds = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (isButtonDown ? SourceColours::accent : (isMouseOver ? SourceColours::controlHover : SourceColours::control));
        g.fillRoundedRectangle (bounds, 4.0f);

        auto iconBounds = bounds.withSizeKeepingCentre (bounds.getHeight() * 0.45f, bounds.getHeight() * 0.45f);
        g.setColour (isEnabled() ? SourceColours::text : SourceColours::dimText.withAlpha (0.4f));
        juce::Path p;
        switch (icon)
        {
            case Icon::play:
                p.addTriangle (iconBounds.getTopLeft(), iconBounds.getBottomLeft(),
                               { iconBounds.getRight(), iconBounds.getCentreY() });
                g.fillPath (p);
                break;
            case Icon::stop:
                g.fillRect (iconBounds);
                break;
            case Icon::remove:
                p.addLineSegment ({ iconBounds.getTopLeft(), iconBounds.getBottomRight() }, 1.5f);
                p.addLineSegment ({ iconBounds.getTopRight(), iconBounds.getBottomLeft() }, 1.5f);
                g.fillPath (p);
                break;
            case Icon::previous:
                p.addTriangle (iconBounds.getTopRight(), iconBounds.getBottomRight(),
                               { iconBounds.getX(), iconBounds.getCentreY() });
                g.fillPath (p);
                break;
            case Icon::next:
                p.addTriangle (iconBounds.getTopLeft(), iconBounds.getBottomLeft(),
                               { iconBounds.getRight(), iconBounds.getCentreY() });
                g.fillPath (p);
                break;
            case Icon::retry:
            {
                // Circular arrow: an open arc with an arrow head at its end
                const auto c = iconBounds.getCentre();
                const float r = iconBounds.getWidth() * 0.5f;
                const float start = 0.6f, end = juce::MathConstants<float>::twoPi - 0.2f;
                p.addCentredArc (c.x, c.y, r, r, 0.0f, start, end, true);
                g.strokePath (p, juce::PathStrokeType (1.6f));
                const auto tip = c.getPointOnCircumference (r, end);
                juce::Path head;
                const float s = r * 0.75f;
                head.addTriangle (tip.translated (-s * 0.6f, -s * 0.55f), tip.translated (s * 0.6f, -s * 0.2f), tip.translated (-s * 0.1f, s * 0.6f));
                g.fillPath (head);
                break;
            }
        }
    }

private:
    Icon icon;
};


/** Sends action messages (the sampler's internal API, see defines_source.h) to the SourceSampler. */
struct SourceActionSender
{
    std::function<void (const juce::String&)> sendMessage;

    void send (const juce::String& actionName, const juce::StringArray& parameters = {}) const
    {
        if (sendMessage != nullptr)
            sendMessage (SourceHelpers::serializeAction (actionName, parameters));
    }

    /** Values are joined with ";" in action messages, so make sure user-entered text does not contain it. */
    static juce::String sanitize (const juce::String& text)
    {
        return text.replace (SERIALIZATION_SEPARATOR, ",");
    }
};
