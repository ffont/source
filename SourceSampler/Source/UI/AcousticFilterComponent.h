/*
  ==============================================================================

    AcousticFilterComponent.h

    Filter for an Audio Commons acoustic property of Freesound sounds (e.g.
    brightness): its name and three squares (low, mid, high). Only one square
    can be selected; clicking the selected one clears the filter.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "SourceLookAndFeel.h"
#include "FreesoundSearch.h"


/** Filter for an acoustic property: its name and three squares (low, mid, high). Only one square can be selected;
    clicking the selected one clears the filter. */
class AcousticFilterComponent : public juce::Component,
                                    public juce::SettableTooltipClient
{
public:
   #if JUCE_IOS
    static constexpr int labelWidth = 66, squareSize = 18, squareGap = 3;  // Bigger for fingers
   #else
    static constexpr int labelWidth = 66, squareSize = 15, squareGap = 3;
   #endif
    static constexpr int preferredWidth = labelWidth + 3 * squareSize + 2 * squareGap;

    explicit AcousticFilterComponent (const juce::String& propertyName) : name (propertyName)
    {
        setTooltip (name.substring (0, 1).toUpperCase() + name.substring (1) + ": low, mid or high (click again to clear)");
    }

    FreesoundSearchOptions::Level getLevel() const { return level; }
    void setLevel (FreesoundSearchOptions::Level newLevel) { level = newLevel; repaint(); }

    void paint (juce::Graphics& g) override
    {
        g.setColour (level == FreesoundSearchOptions::Level::any ? SourceColours::dimText : SourceColours::text);
        g.setFont (juce::Font (juce::FontOptions (getLabelWidth() < labelWidth ? 11.0f : 12.0f)));
        g.drawText (name.substring (0, 1).toUpperCase() + name.substring (1), getLocalBounds().withWidth (getLabelWidth() - 2), juce::Justification::centredLeft, true);
        for (int i = 0; i < 3; ++i)
        {
            const auto square = getSquare (i).toFloat();
            const bool selected = (int) level == i;
            g.setColour (selected ? SourceColours::accent : SourceColours::control);
            g.fillRoundedRectangle (square, 2.0f);
            // Bars of increasing height show low/mid/high
            g.setColour (selected ? SourceColours::background : SourceColours::dimText);
            const float barHeight = square.getHeight() * (0.3f + 0.25f * (float) i);
            g.fillRect (square.reduced (4.0f, 0.0f).withTrimmedTop (square.getHeight() - barHeight).withTrimmedBottom (2.0f));
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        for (int i = 0; i < 3; ++i)
        {
            if (getSquare (i).expanded (1).contains (e.getPosition()))
            {
                setLevel ((int) level == i ? FreesoundSearchOptions::Level::any : (FreesoundSearchOptions::Level) i);
                if (onChange != nullptr)
                    onChange();
                return;
            }
        }
    }

    std::function<void()> onChange;

private:
    juce::Rectangle<int> getSquare (int i) const
    {
        return { getLabelWidth() + i * (squareSize + squareGap), (getHeight() - squareSize) / 2, squareSize, squareSize };
    }

    int getLabelWidth() const { return juce::jmax (30, getWidth() - 3 * squareSize - 2 * squareGap); }

    juce::String name;
    FreesoundSearchOptions::Level level = FreesoundSearchOptions::Level::any;
};

/** Lays out filter components in rows of at most perRow (the last row centred), returns the height used. */
inline int layoutAcousticFilterRows (juce::OwnedArray<AcousticFilterComponent>& filters, juce::Rectangle<int> area, int perRow, bool apply)
{
    const int height = AcousticFilterComponent::squareSize + 6, rowGap = 3;
    const int numRows = (filters.size() + perRow - 1) / perRow;
    if (apply)
    {
        // Filters are made narrower (shorter labels) if the preferred width doesn't fit
        const int width = juce::jmin (AcousticFilterComponent::preferredWidth, (area.getWidth() - (perRow - 1) * 4) / perRow);
        for (int row = 0; row < numRows; ++row)
        {
            const int first = row * perRow;
            const int count = juce::jmin (perRow, filters.size() - first);
            // Filters spread over the row width (as with a full row), the last row centred
            const int gap = perRow > 1 ? juce::jmax (4, (area.getWidth() - perRow * width) / (perRow - 1)) : 0;
            const int rowWidth = count * width + (count - 1) * gap;
            const int x0 = area.getX() + (count < perRow ? (area.getWidth() - rowWidth) / 2 : 0);
            for (int i = 0; i < count; ++i)
                filters[first + i]->setBounds (x0 + i * (width + gap), area.getY() + row * (height + rowGap), width, height);
        }
    }
    return numRows * (height + rowGap) - rowGap;
}
