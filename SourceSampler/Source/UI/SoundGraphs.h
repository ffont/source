/*
  ==============================================================================

    SoundGraphs.h

    Graphs of the sound editor: the filter's frequency response (with a point
    that sets the cutoff, horizontally, and the resonance, vertically) and the
    shape of the amplitude and filter ADSR envelopes. The parameter sliders are
    kept; these graphs show (and, for the filter, also set) the same values.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "SourceLookAndFeel.h"
#include "helpers_source.h"


/** Frequency response of the filter (log frequency axis, 20 Hz - 20 kHz). Drag the point to change the cutoff
    (horizontally) and the resonance (vertically, relative to where the drag starts; the point is drawn on the curve,
    whose peak grows with the resonance). Double-click to reset both. */
class FilterGraphComponent : public juce::Component,
                             public juce::SettableTooltipClient
{
public:
    FilterGraphComponent()
    {
        setViewportIgnoreDragFlag (true);  // Dragging the point must not scroll the parameters panel
        setTooltip ("Filter response. Drag the point to set the cutoff (left/right) and the resonance (up/down); double-click to reset");
    }

    /** Called while the point is dragged (cutoff in Hz, resonance 0..1). */
    std::function<void (float cutoff, float resonance)> onChange;

    void setValues (float newCutoff, float newResonance)
    {
        if (dragging || (newCutoff == cutoff && newResonance == resonance))
            return;
        cutoff = newCutoff;
        resonance = newResonance;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        g.setColour (SourceColours::background);
        g.fillRoundedRectangle (bounds, 4.0f);
        auto plot = getPlotArea();

        // Grid: decades and 0 dB
        g.setFont (juce::Font (juce::FontOptions (10.0f)));
        for (float f : { 100.0f, 1000.0f, 10000.0f })
        {
            const float x = xForFrequency (f);
            g.setColour (SourceColours::control.withAlpha (0.6f));
            g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());
            g.setColour (SourceColours::dimText.withAlpha (0.7f));
            g.drawText (f >= 1000.0f ? juce::String ((int) (f / 1000.0f)) + "k" : juce::String ((int) f),
                        juce::Rectangle<float> (x + 2.0f, plot.getBottom() - 12.0f, 30.0f, 12.0f), juce::Justification::centredLeft);
        }
        g.setColour (SourceColours::control.withAlpha (0.6f));
        g.drawHorizontalLine ((int) yForDb (0.0f), plot.getX(), plot.getRight());

        // Response curve
        juce::Path curve;
        const int numPoints = juce::jmax (2, (int) plot.getWidth());
        for (int i = 0; i < numPoints; ++i)
        {
            const float x = plot.getX() + plot.getWidth() * (float) i / (float) (numPoints - 1);
            const double magnitude = SourceHelpers::ladderFilterMagnitude (frequencyForX (x), cutoff, resonance, displaySampleRate);
            const float y = yForDb ((float) juce::Decibels::gainToDecibels (magnitude, (double) minDb - 6.0));
            if (i == 0)
                curve.startNewSubPath (x, y);
            else
                curve.lineTo (x, y);
        }
        juce::Path fill (curve);
        fill.lineTo (plot.getRight(), plot.getBottom());
        fill.lineTo (plot.getX(), plot.getBottom());
        fill.closeSubPath();
        g.setColour (SourceColours::accent.withAlpha (0.18f));
        g.fillPath (fill);
        g.setColour (SourceColours::accent);
        g.strokePath (curve, juce::PathStrokeType (1.8f));

        // Handle: x = cutoff, y = resonance
        const auto handle = getHandlePosition();
        g.setColour (SourceColours::text.withAlpha (0.35f));
        g.drawVerticalLine ((int) handle.x, plot.getY(), plot.getBottom());
        g.setColour (dragging ? SourceColours::text : SourceColours::warning);
        g.fillEllipse (juce::Rectangle<float> (12.0f, 12.0f).withCentre (handle));
        g.setColour (SourceColours::background);
        g.drawEllipse (juce::Rectangle<float> (12.0f, 12.0f).withCentre (handle), 1.5f);

        // Values
        g.setColour (SourceColours::dimText);
        g.setFont (juce::Font (juce::FontOptions (11.0f)));
        const juce::String cutoffText = cutoff >= 1000.0f ? juce::String (cutoff / 1000.0f, 2) + " kHz" : juce::String (juce::roundToInt (cutoff)) + " Hz";
        g.drawText (cutoffText + "  res " + juce::String (resonance, 2), getLocalBounds().reduced (6, 3), juce::Justification::topRight);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        dragging = true;
        dragStartResonance = resonance;
        // Clicking away from the point moves the cutoff there; then dragging changes the cutoff (horizontally,
        // following the finger) and the resonance (vertically, relative: the whole height of the graph is 0..1)
        dragOffsetX = std::abs (e.position.x - getHandlePosition().x) > 10.0f ? 0.0f : getHandlePosition().x - e.position.x;
        if (dragOffsetX == 0.0f)
            setValuesFromDrag (e.position.x, 0.0f);
        repaint();
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        setValuesFromDrag (e.position.x + dragOffsetX, (float) -e.getDistanceFromDragStartY());
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        dragging = false;
        repaint();
    }

    void mouseDoubleClick (const juce::MouseEvent&) override
    {
        cutoff = defaultCutoff;
        resonance = 0.0f;
        repaint();
        if (onChange)
            onChange (cutoff, resonance);
    }

    static constexpr float minFrequency = 20.0f, maxFrequency = 20000.0f;
    static constexpr float defaultCutoff = 20000.0f;

private:
    juce::Rectangle<float> getPlotArea() const { return getLocalBounds().toFloat().reduced (8.0f, 8.0f); }

    float xForFrequency (float f) const
    {
        auto plot = getPlotArea();
        const float norm = std::log (juce::jlimit (minFrequency, maxFrequency, f) / minFrequency) / std::log (maxFrequency / minFrequency);
        return plot.getX() + norm * plot.getWidth();
    }

    double frequencyForX (float x) const
    {
        auto plot = getPlotArea();
        const float norm = juce::jlimit (0.0f, 1.0f, (x - plot.getX()) / juce::jmax (1.0f, plot.getWidth()));
        return minFrequency * std::pow (maxFrequency / minFrequency, norm);
    }

    float yForDb (float db) const
    {
        auto plot = getPlotArea();
        return juce::jmap (juce::jlimit (minDb, maxDb, db), maxDb, minDb, plot.getY(), plot.getBottom());
    }

    /** Level of the response at the cutoff frequency (it grows with the resonance). */
    static float peakDb (float cutoffHz, float res)
    {
        return (float) juce::Decibels::gainToDecibels (SourceHelpers::ladderFilterMagnitude (cutoffHz, cutoffHz, res, displaySampleRate), -100.0);
    }

    /** The point is on the curve, at the cutoff frequency. */
    juce::Point<float> getHandlePosition() const
    {
        auto plot = getPlotArea();
        return { xForFrequency (cutoff), juce::jlimit (plot.getY(), plot.getBottom(), yForDb (peakDb (cutoff, resonance))) };
    }

    void setValuesFromDrag (float x, float upwardsDistance)
    {
        cutoff = (float) juce::jlimit (10.0, 20000.0, frequencyForX (x));
        cutoff = (float) (juce::roundToInt (cutoff * (cutoff < 1000.0f ? 1.0f : 0.1f)) / (cutoff < 1000.0f ? 1.0f : 0.1f));
        resonance = juce::jlimit (0.0f, 1.0f, dragStartResonance + upwardsDistance / juce::jmax (1.0f, getPlotArea().getHeight()));
        resonance = (float) juce::roundToInt (resonance * 100.0f) / 100.0f;
        repaint();
        if (onChange)
            onChange (cutoff, resonance);
    }

    static constexpr float minDb = -48.0f, maxDb = 24.0f;
    static constexpr double displaySampleRate = 48000.0;
    float cutoff = defaultCutoff, resonance = 0.0f;
    bool dragging = false;
    float dragOffsetX = 0.0f, dragStartResonance = 0.0f;
};


/** Shape of an ADSR envelope (attack, decay, sustain held for a while, release), with its times. */
class EnvelopeGraphComponent : public juce::Component
{
public:
    void setValues (float newAttack, float newDecay, float newSustain, float newRelease)
    {
        if (newAttack == attack && newDecay == decay && newSustain == sustain && newRelease == release)
            return;
        attack = newAttack; decay = newDecay; sustain = newSustain; release = newRelease;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        g.setColour (SourceColours::background);
        g.fillRoundedRectangle (bounds, 4.0f);
        auto plot = bounds.reduced (6.0f, 6.0f).withTrimmedTop (12.0f);  // Room for the values at the top

        // The sustain part is drawn with a fixed length (a quarter of the graph) so short envelopes stay readable
        const float envelopeTime = juce::jmax (0.05f, attack + decay + release);
        const float holdTime = envelopeTime / 3.0f;
        const float totalTime = envelopeTime + holdTime;
        const float releaseTime = attack + decay + holdTime;
        auto xFor = [&] (float t) { return plot.getX() + plot.getWidth() * t / totalTime; };
        auto yFor = [&] (float level) { return juce::jmap (level, plot.getBottom(), plot.getY()); };

        juce::Path curve;
        const int numPoints = juce::jmax (2, (int) plot.getWidth());
        for (int i = 0; i < numPoints; ++i)
        {
            const float t = totalTime * (float) i / (float) (numPoints - 1);
            const float x = xFor (t), y = yFor (SourceHelpers::adsrLevelAt (t, attack, decay, sustain, release, releaseTime));
            if (i == 0)
                curve.startNewSubPath (x, y);
            else
                curve.lineTo (x, y);
        }
        juce::Path fill (curve);
        fill.lineTo (plot.getRight(), plot.getBottom());
        fill.lineTo (plot.getX(), plot.getBottom());
        fill.closeSubPath();
        g.setColour (SourceColours::accent.withAlpha (0.18f));
        g.fillPath (fill);

        // Segment boundaries (note on, end of attack and decay, note off)
        g.setColour (SourceColours::control.withAlpha (0.6f));
        for (float t : { attack, attack + decay, releaseTime })
            g.drawVerticalLine ((int) xFor (t), plot.getY(), plot.getBottom());

        g.setColour (SourceColours::accent);
        g.strokePath (curve, juce::PathStrokeType (1.8f));

        g.setColour (SourceColours::dimText);
        g.setFont (juce::Font (juce::FontOptions (11.0f)));
        auto formatTime = [] (float seconds) { return seconds < 1.0f ? juce::String (juce::roundToInt (seconds * 1000.0f)) + " ms" : juce::String (seconds, 2) + " s"; };
        g.drawText ("A " + formatTime (attack) + "  D " + formatTime (decay) + "  S " + juce::String (sustain, 2) + "  R " + formatTime (release),
                    getLocalBounds().reduced (6, 3), juce::Justification::topRight);
    }

private:
    float attack = 0.01f, decay = 0.0f, sustain = 1.0f, release = 0.01f;
};
