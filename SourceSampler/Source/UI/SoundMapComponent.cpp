/*
  ==============================================================================

    SoundMapComponent.cpp

  ==============================================================================
*/

#include "SoundMapComponent.h"
#include "SourceLookAndFeel.h"


SoundMapComponent::SoundMapComponent()
{
    setOpaque (false);
}

void SoundMapComponent::setResults (std::shared_ptr<const std::vector<FreesoundSearchResult>> newResults)
{
    if (newResults == results)
        return;
    results = std::move (newResults);
    selectedIndex = hoverIndex = -1;
    resetView();
}

void SoundMapComponent::setSelectedIndex (int index)
{
    if (index == selectedIndex)
        return;
    selectedIndex = index;
    repaint();
}

void SoundMapComponent::setHighlightedIds (std::set<int> ids)
{
    if (ids == highlightedIds)
        return;
    highlightedIds = std::move (ids);
    repaint();
}

juce::AffineTransform SoundMapComponent::getFitTransform() const
{
    // The map (most points within radius 1, outliers up to ~1.35) fits in the component with some margin
    const float scale = 0.36f * (float) juce::jmax (1, juce::jmin (getWidth(), getHeight()));
    return juce::AffineTransform::scale (scale).translated ((float) getWidth() * 0.5f, (float) getHeight() * 0.5f);
}

void SoundMapComponent::resetView()
{
    viewTransform = getFitTransform();
    userMovedView = false;
    repaint();
}

void SoundMapComponent::resized()
{
    if (! userMovedView)
        viewTransform = getFitTransform();
}

juce::Point<float> SoundMapComponent::getScreenPosition (int index) const
{
    if (results == nullptr || index < 0 || index >= (int) results->size())
        return {};
    const auto& r = (*results)[(size_t) index];
    return juce::Point<float> (r.x, r.y).transformedBy (viewTransform);
}

int SoundMapComponent::getSoundAt (juce::Point<float> position, float radius) const
{
    if (results == nullptr)
        return -1;
    int closest = -1;
    float closestDistance = radius;
    for (int i = 0; i < (int) results->size(); ++i)
    {
        if (! (*results)[(size_t) i].hasPosition)
            continue;
        const float distance = getScreenPosition (i).getDistanceFrom (position);
        if (distance <= closestDistance)
        {
            closestDistance = distance;
            closest = i;
        }
    }
    return closest;
}

void SoundMapComponent::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour (SourceColours::card);
    g.fillRoundedRectangle (bounds, 5.0f);

    if (results == nullptr || results->empty())
    {
        g.setColour (SourceColours::dimText);
        g.setFont (juce::Font (juce::FontOptions (14.0f)));
        g.drawText ("Search for sounds to see them in the map", getLocalBounds(), juce::Justification::centred);
        return;
    }

    {
        juce::Graphics::ScopedSaveState clip (g);
        juce::Path clipPath;
        clipPath.addRoundedRectangle (bounds, 5.0f);
        g.reduceClipRegion (clipPath);

        // Sounds (circles keep their size on screen when zooming)
        const float radius = 6.0f;
        for (int i = 0; i < (int) results->size(); ++i)
        {
            const auto& r = (*results)[(size_t) i];
            if (! r.hasPosition)
                continue;
            const auto p = getScreenPosition (i);
            if (! bounds.expanded (radius).contains (p))
                continue;
            g.setColour (r.getColour());
            g.fillEllipse (p.x - radius, p.y - radius, radius * 2.0f, radius * 2.0f);
            if (highlightedIds.count (r.id) > 0)
            {
                // Sounds already in the preset
                g.setColour (SourceColours::background);
                g.fillEllipse (p.x - 2.0f, p.y - 2.0f, 4.0f, 4.0f);
            }
        }

        // Hovered and selected sounds
        for (int index : { hoverIndex, selectedIndex })
        {
            if (index < 0 || index >= (int) results->size())
                continue;
            const auto p = getScreenPosition (index);
            const bool isSelected = index == selectedIndex;
            g.setColour (isSelected ? juce::Colours::white : juce::Colours::white.withAlpha (0.6f));
            g.drawEllipse (p.x - 10.0f, p.y - 10.0f, 20.0f, 20.0f, isSelected ? 2.0f : 1.0f);
        }
        if (hoverIndex >= 0 && hoverIndex < (int) results->size())
        {
            const auto p = getScreenPosition (hoverIndex);
            const auto name = (*results)[(size_t) hoverIndex].name;
            const juce::Font font (juce::FontOptions (12.0f));
            const float width = juce::jmin (260.0f, juce::GlyphArrangement::getStringWidth (font, name) + 12.0f);
            auto label = juce::Rectangle<float> (p.x + 12.0f, p.y - 22.0f, width, 18.0f);
            if (label.getRight() > bounds.getRight() - 4.0f)
                label.setX (p.x - 12.0f - width);
            label.setY (juce::jlimit (bounds.getY() + 2.0f, bounds.getBottom() - 20.0f, label.getY()));
            g.setColour (SourceColours::background.withAlpha (0.85f));
            g.fillRoundedRectangle (label, 3.0f);
            g.setColour (SourceColours::text);
            g.setFont (font);
            g.drawText (name, label.reduced (6.0f, 0.0f), juce::Justification::centredLeft, true);
        }
    }

    // How to navigate
    g.setColour (SourceColours::dimText.withAlpha (0.7f));
    g.setFont (juce::Font (juce::FontOptions (11.0f)));
   #if JUCE_IOS || JUCE_ANDROID
    const juce::String hint = "Drag to move, pinch to zoom, two fingers to rotate, double tap to reset";
   #else
    const juce::String hint = "Drag to move, scroll/pinch to zoom, Alt+drag to rotate, double-click to reset";
   #endif
    g.drawText (hint, getLocalBounds().reduced (8, 4).removeFromBottom (14), juce::Justification::bottomLeft);
}

void SoundMapComponent::applyGesture (juce::Point<float> fromCentre, juce::Point<float> toCentre, float scale, float rotation)
{
    // Limit the zoom (relative to the initial view)
    const float currentScale = std::sqrt (std::abs (viewTransform.getDeterminant()));
    const float fitScale = std::sqrt (std::abs (getFitTransform().getDeterminant()));
    if (fitScale > 0.0f)
        scale = juce::jlimit (0.3f * fitScale / currentScale, 60.0f * fitScale / currentScale, scale);
    viewTransform = viewTransform.followedBy (juce::AffineTransform::translation (-fromCentre)
                                                  .scaled (scale)
                                                  .rotated (rotation)
                                                  .translated (toCentre));
    userMovedView = true;
    repaint();
}

void SoundMapComponent::mouseDown (const juce::MouseEvent& e)
{
    if (pointers.empty())
    {
        totalMovement = 0.0f;
        gestureHadMultiplePointers = false;
    }
    pointers[e.source.getIndex()] = e.position;
    if (pointers.size() > 1)
        gestureHadMultiplePointers = true;
}

void SoundMapComponent::mouseDrag (const juce::MouseEvent& e)
{
    const int index = e.source.getIndex();
    auto it = pointers.find (index);
    if (it == pointers.end())
        return;
    const auto previous = it->second;
    const auto current = e.position;
    totalMovement += previous.getDistanceFrom (current);

    if (pointers.size() >= 2)
    {
        // Two fingers: move, zoom and rotate at the same time (relative to the other finger)
        juce::Point<float> other;
        for (const auto& [otherIndex, position] : pointers)
            if (otherIndex != index)
            {
                other = position;
                break;
            }
        const auto previousVector = previous - other, currentVector = current - other;
        const float previousLength = previousVector.getDistanceFromOrigin();
        if (previousLength > 1.0f)
        {
            const float scale = currentVector.getDistanceFromOrigin() / previousLength;
            const float rotation = std::atan2 (currentVector.y, currentVector.x) - std::atan2 (previousVector.y, previousVector.x);
            applyGesture ((previous + other) * 0.5f, (current + other) * 0.5f, scale, rotation);
        }
    }
    else if (e.mods.isAltDown() || e.mods.isPopupMenu())
    {
        // Rotate around the centre of the component
        const auto centre = getLocalBounds().getCentre().toFloat();
        const auto a = previous - centre, b = current - centre;
        if (a.getDistanceFromOrigin() > 4.0f && b.getDistanceFromOrigin() > 4.0f)
            applyGesture (centre, centre, 1.0f, std::atan2 (b.y, b.x) - std::atan2 (a.y, a.x));
    }
    else
    {
        applyGesture (previous, current, 1.0f, 0.0f);
    }
    it->second = current;
}

void SoundMapComponent::mouseUp (const juce::MouseEvent& e)
{
    pointers.erase (e.source.getIndex());
    if (pointers.empty() && ! gestureHadMultiplePointers && totalMovement < 6.0f && ! e.mods.isPopupMenu())
    {
        const int index = getSoundAt (e.position, e.source.isTouch() ? 22.0f : 14.0f);
        if (index >= 0 && onSoundClicked != nullptr)
            onSoundClicked (index);
    }
}

void SoundMapComponent::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (getSoundAt (e.position) < 0)
        resetView();
}

void SoundMapComponent::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    if (wheel.isSmooth && ! e.mods.isCtrlDown() && ! e.mods.isCommandDown())
    {
        // Trackpad two-finger scroll: move
        const float direction = wheel.isReversed ? -1.0f : 1.0f;
        const juce::Point<float> delta (wheel.deltaX * 300.0f * direction, wheel.deltaY * 300.0f * direction);
        applyGesture (e.position, e.position + delta, 1.0f, 0.0f);
    }
    else
    {
        // Mouse wheel: zoom around the pointer
        applyGesture (e.position, e.position, std::exp (wheel.deltaY * 1.5f), 0.0f);
    }
}

void SoundMapComponent::mouseMagnify (const juce::MouseEvent& e, float scaleFactor)
{
    applyGesture (e.position, e.position, scaleFactor, 0.0f);
}

void SoundMapComponent::mouseMove (const juce::MouseEvent& e)
{
    const int index = getSoundAt (e.position);
    if (index != hoverIndex)
    {
        hoverIndex = index;
        setMouseCursor (index >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
        repaint();
    }
}

void SoundMapComponent::mouseExit (const juce::MouseEvent&)
{
    if (hoverIndex >= 0)
    {
        hoverIndex = -1;
        repaint();
    }
}
