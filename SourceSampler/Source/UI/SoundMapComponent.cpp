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
    stopAllButton.setTooltip ("Stop all the sounds playing");
    stopAllButton.onClick = [this] { if (onStopAllSounds != nullptr) onStopAllSounds(); };
    addChildComponent (stopAllButton);
}

void SoundMapComponent::setResults (std::shared_ptr<const std::vector<FreesoundSearchResult>> newResults)
{
    if (newResults == results)
        return;
    results = std::move (newResults);
    stopAllButton.setVisible (results != nullptr && ! results->empty());
    selectedIndex = hoverIndex = -1;
    animation.reset();
    stopTimer();
    resetView();
}

void SoundMapComponent::setAnimation (std::shared_ptr<const MapAnimationFrames> frames)
{
    if (frames == nullptr || frames->empty() || results == nullptr || frames->front().size() != results->size())
        return;
    animation = std::move (frames);
    animationStartTime = juce::Time::getMillisecondCounterHiRes();
    animationPosition = 0.0;
    startTimerHz (60);
    repaint();
}

void SoundMapComponent::timerCallback()
{
    animationPosition = juce::jlimit (0.0, 1.0, (juce::Time::getMillisecondCounterHiRes() - animationStartTime) / animationDurationMs);
    if (animationPosition >= 1.0)
    {
        animation.reset();
        stopTimer();
    }
    repaint();
}

juce::Point<float> SoundMapComponent::getWorldPosition (int index) const
{
    const auto& r = (*results)[(size_t) index];
    if (animation == nullptr)
        return { r.x, r.y };
    // Interpolate between the two frames around the current position of the animation
    const double framePosition = animationPosition * (double) (animation->size() - 1);
    const auto frame = (size_t) juce::jlimit (0, (int) animation->size() - 1, (int) std::floor (framePosition));
    const auto next = juce::jmin (frame + 1, animation->size() - 1);
    const float t = (float) (framePosition - (double) frame);
    const auto& a = (*animation)[frame][(size_t) index];
    const auto& b = (*animation)[next][(size_t) index];
    return { a[0] + t * (b[0] - a[0]), a[1] + t * (b[1] - a[1]) };
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
    stopAllButton.setBounds (getLocalBounds().reduced (8).removeFromBottom (28).removeFromRight (130));
    if (! userMovedView)
        viewTransform = getFitTransform();
}

juce::Point<float> SoundMapComponent::getScreenPosition (int index) const
{
    if (results == nullptr || index < 0 || index >= (int) results->size())
        return {};
    return getWorldPosition (index).transformedBy (viewTransform);
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
    g.setColour (juce::Colours::black);
    g.fillRoundedRectangle (bounds, 5.0f);
    g.setColour (SourceColours::control);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 5.0f, 1.0f);

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

        // Sounds. Circles grow when zooming in, but less than the distances between them (so very close sounds can
        // be separated by zooming, while still feeling that you get closer)
        const float radius = getDotRadius();
        for (int i = 0; i < (int) results->size(); ++i)
        {
            const auto& r = (*results)[(size_t) i];
            if (! r.hasPosition)
                continue;
            const auto p = getScreenPosition (i);
            if (! bounds.expanded (radius).contains (p))
                continue;
            g.setColour (r.getColour().withAlpha (0.5f));
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
            const float ring = getDotRadius() + 4.0f;
            g.setColour (isSelected ? juce::Colours::white : juce::Colours::white.withAlpha (0.6f));
            g.drawEllipse (p.x - ring, p.y - ring, ring * 2.0f, ring * 2.0f, isSelected ? 2.0f : 1.0f);
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
    const juce::String hint = "Drag a finger to listen, two fingers to move, zoom and rotate, double tap to reset";
   #else
    const juce::String hint = "Drag to move, scroll or pinch to zoom, Alt+drag to rotate, Cmd+hover to listen, double-click to reset";
   #endif
    g.drawText (hint, getLocalBounds().reduced (8, 4).removeFromBottom (14), juce::Justification::bottomLeft);
}

float SoundMapComponent::getZoom() const
{
    const float fitScale = std::sqrt (std::abs (getFitTransform().getDeterminant()));
    return fitScale > 0.0f ? std::sqrt (std::abs (viewTransform.getDeterminant())) / fitScale : 1.0f;
}

float SoundMapComponent::getDotRadius() const
{
    // Grows with the square root of the zoom (distances grow linearly)
    return juce::jlimit (4.0f, 22.0f, 8.0f * std::pow (getZoom(), 0.4f));
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

void SoundMapComponent::audition (juce::Point<float> position, bool isTouch)
{
    const int index = getSoundAt (position, getDotRadius() + (isTouch ? 10.0f : 3.0f));
    if (index != hoverIndex)
    {
        hoverIndex = index;
        repaint();
    }
    if (index >= 0 && index != lastAuditionedIndex && onSoundAuditioned != nullptr)
        onSoundAuditioned (index);
    lastAuditionedIndex = index;
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
    else if (e.source.isTouch())
    {
        // One finger plays the sounds it goes over (the map is moved with two fingers)
        audition (current, true);
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
    lastAuditionedIndex = -1;
    if (pointers.empty() && ! gestureHadMultiplePointers && totalMovement < 6.0f && ! e.mods.isPopupMenu())
    {
        const int index = getSoundAt (e.position, getDotRadius() + (e.source.isTouch() ? 12.0f : 5.0f));
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
    // Scrolling zooms around the pointer (with mouse wheels, Magic Mouse and trackpads; moving is done by dragging).
    // Smooth scrolling (Magic Mouse, trackpads) sends many small deltas, so it needs a bigger factor.
    const float delta = (std::abs (wheel.deltaY) >= std::abs (wheel.deltaX) ? wheel.deltaY : wheel.deltaX) * (wheel.isReversed ? -1.0f : 1.0f);
    applyGesture (e.position, e.position, std::exp (delta * (wheel.isSmooth ? 2.5f : 1.5f)), 0.0f);
}

void SoundMapComponent::mouseMagnify (const juce::MouseEvent& e, float scaleFactor)
{
    applyGesture (e.position, e.position, scaleFactor, 0.0f);
}

void SoundMapComponent::mouseMove (const juce::MouseEvent& e)
{
    if (e.mods.isCommandDown())
    {
        // Cmd + hover plays the sounds under the pointer
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        audition (e.position, false);
        return;
    }
    lastAuditionedIndex = -1;
    const int index = getSoundAt (e.position, getDotRadius() + 3.0f);
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
