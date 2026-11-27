/*
  ==============================================================================

    SoundMapComponent.h

    Map of Freesound search results: each sound is a circle placed with its
    t-SNE position (similar sounds are close) and coloured with its
    tristimulus. The map can be moved, zoomed and rotated:
      - mouse: drag to move, wheel to zoom, Alt+drag (or right-drag) to rotate
      - trackpad: two-finger scroll to move, pinch to zoom
      - touch: one finger moves, two fingers move, zoom and rotate together
    Tapping/clicking a sound selects it (and plays it, see onSoundClicked).

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "FreesoundSearch.h"


class SoundMapComponent : public juce::Component
{
public:
    SoundMapComponent();

    void setResults (std::shared_ptr<const std::vector<FreesoundSearchResult>> newResults);
    void setSelectedIndex (int index);
    void setHighlightedIds (std::set<int> ids);  // e.g. sounds already in the preset
    void resetView();

    std::function<void (int index)> onSoundClicked;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;
    void mouseMagnify (const juce::MouseEvent& e, float scaleFactor) override;
    void mouseMove (const juce::MouseEvent& e) override;
    void mouseExit (const juce::MouseEvent& e) override;

    /** Index of the sound closest to a point (in component coordinates) within the given radius, or -1. */
    int getSoundAt (juce::Point<float> position, float radius = 14.0f) const;
    juce::Point<float> getScreenPosition (int index) const;
    const juce::AffineTransform& getViewTransform() const { return viewTransform; }

private:
    juce::AffineTransform getFitTransform() const;
    float getZoom() const;       // Relative to the initial view
    float getDotRadius() const;  // Radius of the circles on screen, depends on the zoom
    void applyGesture (juce::Point<float> fromCentre, juce::Point<float> toCentre, float scale, float rotation);

    std::shared_ptr<const std::vector<FreesoundSearchResult>> results;
    int selectedIndex = -1, hoverIndex = -1;
    std::set<int> highlightedIds;
    juce::AffineTransform viewTransform;  // World ([-1, 1] map coordinates) to component coordinates
    bool userMovedView = false;

    // Active pointers (mouse or fingers) and their last positions
    std::map<int, juce::Point<float>> pointers;
    float totalMovement = 0.0f;
    bool gestureHadMultiplePointers = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SoundMapComponent)
};
