/*
  ==============================================================================

    SoundEditorComponent.cpp

  ==============================================================================
*/

#include "SoundEditorComponent.h"
#include "SourceSampler.h"


//==============================================================================
// WaveformComponent

void WaveformComponent::setAudio (const juce::AudioBuffer<float>* audio, double sampleRate)
{
    peaks.clear();
    duration = 0.0;
    resetZoom();
    if (audio != nullptr && audio->getNumSamples() > 0)
    {
        const int numSamples = audio->getNumSamples();
        duration = sampleRate > 0.0 ? (double) numSamples / sampleRate : 0.0;
        // Many bins, so that zoomed views still have detail
        const int numBins = juce::jmin (numPeakBins, numSamples);
        peaks.resize ((size_t) numBins, { 0.0f, 0.0f });
        for (int ch = 0; ch < audio->getNumChannels(); ++ch)
        {
            const float* data = audio->getReadPointer (ch);
            for (int bin = 0; bin < numBins; ++bin)
            {
                const int start = (int) ((juce::int64) bin * numSamples / numBins);
                const int end = juce::jmax (start + 1, (int) ((juce::int64) (bin + 1) * numSamples / numBins));
                auto range = juce::FloatVectorOperations::findMinAndMax (data + start, end - start);
                auto& peak = peaks[(size_t) bin];
                peak.first = juce::jmin (peak.first, range.getStart());
                peak.second = juce::jmax (peak.second, range.getEnd());
            }
        }
    }
    repaint();
}

void WaveformComponent::setMarkers (float start, float end, float loopStart, float loopEnd, bool showLoop)
{
    if (draggedMarker != Marker::none)
        return;  // Don't move the markers while the user drags one
    if (start == startPos && end == endPos && loopStart == loopStartPos && loopEnd == loopEndPos && showLoop == loopVisible)
        return;
    startPos = start; endPos = end; loopStartPos = loopStart; loopEndPos = loopEnd; loopVisible = showLoop;
    repaint();
}

void WaveformComponent::setPlayheads (std::vector<float> positions)
{
    if (positions == playheads)
        return;
    playheads = std::move (positions);
    repaint();
}

void WaveformComponent::setSlices (std::vector<float> positions, bool editable, bool active)
{
    if (draggedSlice >= 0)
        return;  // Don't move the markers while the user drags one
    if (positions == slices && editable == slicesEditable && active == slicesActive)
        return;
    slices = std::move (positions);
    slicesEditable = editable;
    slicesActive = active;
    repaint();
}

void WaveformComponent::resetZoom()
{
    setView (0.0f, 1.0f);
}

float WaveformComponent::xForPosition (float position) const
{
    return (position - viewStart) / juce::jmax (1.0e-6f, viewEnd - viewStart) * (float) getWidth();
}

float WaveformComponent::positionForX (float x) const
{
    return viewStart + x / (float) juce::jmax (1, getWidth()) * (viewEnd - viewStart);
}

void WaveformComponent::setView (float start, float end)
{
    // At most 1000x zoom (or ~32 samples per view for short sounds), always inside the sound
    const float minWidth = juce::jmax (0.001f, peaks.empty() ? 0.001f : 32.0f / (float) juce::jmax ((size_t) 1, peaks.size()));
    float width = juce::jlimit (minWidth, 1.0f, end - start);
    start = juce::jlimit (0.0f, 1.0f - width, start);
    if (start != viewStart || start + width != viewEnd)
    {
        viewStart = start;
        viewEnd = start + width;
        repaint();
    }
}

void WaveformComponent::zoomAround (float factor, float anchorX)
{
    const float anchor = positionForX (anchorX);
    const float width = (viewEnd - viewStart) / juce::jmax (0.01f, factor);
    const float start = anchor - anchorX / (float) juce::jmax (1, getWidth()) * width;
    setView (start, start + width);
}

int WaveformComponent::findSliceAt (juce::Point<float> point) const
{
    if (! slicesEditable)
        return -1;
    // The handle at the top, or the line itself (a bit wider for touch)
    int closest = -1;
    float closestDistance = 9.0f;
    for (size_t i = 0; i < slices.size(); ++i)
    {
        const float distance = std::abs (xForPosition (slices[i]) - point.x);
        if (distance < closestDistance)
        {
            closestDistance = distance;
            closest = (int) i;
        }
    }
    return closest;
}

WaveformComponent::Marker WaveformComponent::findMarkerAt (juce::Point<float> point) const
{
    if (peaks.empty())
        return Marker::none;
    const float h = (float) getHeight();
    auto closestOf = [&] (std::initializer_list<Marker> markers)
    {
        Marker closest = Marker::none;
        float closestDistance = 10.0f;
        for (auto m : markers)
        {
            const float distance = std::abs (xForPosition (getMarkerPosition (m)) - point.x);
            if (distance < closestDistance)
            {
                closestDistance = distance;
                closest = m;
            }
        }
        return closest;
    };
    // Loop markers: their handles at the bottom; start/end: the handles at the top or the line itself
    if (loopVisible && point.y >= h - handleSize - 4.0f)
        if (auto m = closestOf ({ Marker::loopStart, Marker::loopEnd }); m != Marker::none)
            return m;
    if (point.y <= handleSize + 4.0f && findSliceAt (point) >= 0
        && std::abs (xForPosition (slices[(size_t) findSliceAt (point)]) - point.x) < 6.0f)
        return Marker::none;  // A slice handle is closer
    return closestOf ({ Marker::start, Marker::end });
}

float WaveformComponent::getMarkerPosition (Marker marker) const
{
    switch (marker)
    {
        case Marker::start:     return startPos;
        case Marker::end:       return endPos;
        case Marker::loopStart: return loopStartPos;
        case Marker::loopEnd:   return loopEndPos;
        case Marker::none:      break;
    }
    return 0.0f;
}

void WaveformComponent::setMarkerPosition (Marker marker, float position)
{
    constexpr float minLength = 0.001f;
    juce::String name;
    switch (marker)
    {
        case Marker::start:     startPos = position = juce::jlimit (0.0f, endPos - minLength, position); name = "startPosition"; break;
        case Marker::end:       endPos = position = juce::jlimit (startPos + minLength, 1.0f, position); name = "endPosition"; break;
        case Marker::loopStart: loopStartPos = position = juce::jlimit (0.0f, loopEndPos - minLength, position); name = "loopStartPosition"; break;
        case Marker::loopEnd:   loopEndPos = position = juce::jlimit (loopStartPos + minLength, 1.0f, position); name = "loopEndPosition"; break;
        case Marker::none:      return;
    }
    repaint();
    if (onMarkerChanged)
        onMarkerChanged (name, position);
}

void WaveformComponent::mouseMove (const juce::MouseEvent& e)
{
    const bool overHandle = findMarkerAt (e.position) != Marker::none || findSliceAt (e.position) >= 0;
    setMouseCursor (overHandle ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::NormalCursor);
}

void WaveformComponent::mouseDown (const juce::MouseEvent& e)
{
    touches[e.source.getIndex()] = e.position;
    if (touches.size() >= 2)
    {
        // Two fingers: pinch to zoom (anchored at the point between the fingers)
        auto it = touches.begin();
        const auto a = it->second;
        const auto b = (++it)->second;
        pinching = true;
        draggedSlice = -1;
        draggedMarker = Marker::none;
        pinchStartDistance = juce::jmax (10.0f, std::abs (a.x - b.x));
        pinchAnchorPosition = positionForX ((a.x + b.x) * 0.5f);
        gestureStartView = getVisibleRange();
        return;
    }
    pinching = false;
    draggedMarker = findMarkerAt (e.position);
    draggedSlice = draggedMarker == Marker::none ? findSliceAt (e.position) : -1;
    gestureStartView = getVisibleRange();
    panStartX = e.position.x;
}

void WaveformComponent::mouseDrag (const juce::MouseEvent& e)
{
    touches[e.source.getIndex()] = e.position;
    if (pinching && touches.size() >= 2)
    {
        auto it = touches.begin();
        const auto a = it->second;
        const auto b = (++it)->second;
        const float distance = juce::jmax (10.0f, std::abs (a.x - b.x));
        const float width = gestureStartView.getLength() * pinchStartDistance / distance;
        const float centreX = (a.x + b.x) * 0.5f;
        const float start = pinchAnchorPosition - centreX / (float) juce::jmax (1, getWidth()) * width;
        setView (start, start + width);
        return;
    }
    if (pinching)
        return;  // One finger lifted: wait for both to be released
    if (draggedMarker != Marker::none)
    {
        setMarkerPosition (draggedMarker, positionForX (e.position.x));
        return;
    }
    if (draggedSlice >= 0 && draggedSlice < (int) slices.size())
    {
        slices[(size_t) draggedSlice] = juce::jlimit (0.0005f, 0.9995f, positionForX (e.position.x));
        repaint();
        if (onSlicesChanged)
        {
            auto sorted = slices;
            std::sort (sorted.begin(), sorted.end());
            onSlicesChanged (sorted);
        }
        return;
    }
    // Move the view (when zoomed)
    const float delta = (e.position.x - panStartX) / (float) juce::jmax (1, getWidth()) * gestureStartView.getLength();
    setView (gestureStartView.getStart() - delta, gestureStartView.getEnd() - delta);
}

void WaveformComponent::mouseUp (const juce::MouseEvent& e)
{
    touches.erase (e.source.getIndex());
    if (touches.empty())
        pinching = false;
    if (draggedMarker != Marker::none)
    {
        draggedMarker = Marker::none;
        repaint();
    }
    if (draggedSlice >= 0)
    {
        draggedSlice = -1;
        std::sort (slices.begin(), slices.end());
        if (onSlicesChanged)
            onSlicesChanged (slices);
    }
}

void WaveformComponent::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (const auto marker = findMarkerAt (e.position); marker != Marker::none)
    {
        // Double-click on a start/end or loop marker: back to the start/end of the sound
        setMarkerPosition (marker, marker == Marker::start || marker == Marker::loopStart ? 0.0f : 1.0f);
        draggedMarker = Marker::none;
        return;
    }
    const int slice = findSliceAt (e.position);
    if (slice >= 0 && e.position.y <= 14.0f)
    {
        // Double-click on a handle: remove the marker
        slices.erase (slices.begin() + slice);
        draggedSlice = -1;
        repaint();
        if (onSlicesChanged)
            onSlicesChanged (slices);
        return;
    }
    resetZoom();
}

void WaveformComponent::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    if (peaks.empty())
        return;
    if (std::abs (wheel.deltaX) > std::abs (wheel.deltaY))
    {
        // Horizontal scroll: move the view
        const float delta = -wheel.deltaX * 0.5f * (viewEnd - viewStart);
        setView (viewStart + delta, viewEnd + delta);
    }
    else
    {
        zoomAround (std::exp (wheel.deltaY * (wheel.isReversed ? -2.0f : 2.0f)), e.position.x);
    }
}

void WaveformComponent::mouseMagnify (const juce::MouseEvent& e, float scaleFactor)
{
    zoomAround (scaleFactor, e.position.x);
}

void WaveformComponent::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour (SourceColours::background);
    g.fillRoundedRectangle (bounds, 4.0f);

    if (peaks.empty())
    {
        g.setColour (SourceColours::dimText);
        g.setFont (juce::Font (juce::FontOptions (13.0f)));
        g.drawText (messageText, getLocalBounds(), juce::Justification::centred);
        return;
    }

    const float w = bounds.getWidth();
    const float h = bounds.getHeight();
    const float midY = bounds.getCentreY();
    auto xFor = [this] (float position) { return xForPosition (position); };
    juce::Graphics::ScopedSaveState clip (g);
    g.reduceClipRegion (getLocalBounds());

    // Selected region (start-end)
    g.setColour (SourceColours::control.withAlpha (0.35f));
    g.fillRect (juce::Rectangle<float>::leftTopRightBottom (xFor (startPos), 0.0f, xFor (endPos), h));

    // Waveform (the peaks of the visible part, one column per pixel)
    juce::Path path;
    const int numColumns = juce::jmax (1, (int) w);
    const double binsPerPosition = (double) peaks.size();
    for (int x = 0; x < numColumns; ++x)
    {
        const double from = (double) positionForX ((float) x) * binsPerPosition;
        const double to = (double) positionForX ((float) (x + 1)) * binsPerPosition;
        const size_t firstBin = (size_t) juce::jlimit (0.0, binsPerPosition - 1.0, std::floor (from));
        const size_t lastBin = juce::jmax (firstBin + 1, (size_t) juce::jlimit (0.0, binsPerPosition, std::ceil (to)));
        float minV = 0.0f, maxV = 0.0f;
        for (size_t b = firstBin; b < juce::jmin (lastBin, peaks.size()); ++b)
        {
            minV = juce::jmin (minV, peaks[b].first);
            maxV = juce::jmax (maxV, peaks[b].second);
        }
        const float top = midY - juce::jlimit (0.0f, 1.0f, maxV) * h * 0.48f;
        const float bottom = midY - juce::jlimit (-1.0f, 0.0f, minV) * h * 0.48f;
        path.addRectangle ((float) x, top, 1.0f, juce::jmax (1.0f, bottom - top));
    }
    g.setColour (SourceColours::accent.withAlpha (0.85f));
    g.fillPath (path);

    // Start/end markers (green, handles at the top pointing inwards) and loop markers (blue, handles at the bottom)
    auto drawMarker = [&] (Marker marker, juce::Colour colour, bool atTop, bool pointsRight)
    {
        const float x = juce::jlimit (0.0f, w, xFor (getMarkerPosition (marker)));
        g.setColour (marker == draggedMarker ? SourceColours::text : colour);
        g.fillRect (x - 0.75f, 0.0f, 1.5f, h);
        const float y0 = atTop ? 0.0f : h - handleSize;
        const float y1 = atTop ? handleSize : h;
        const float tipX = pointsRight ? x + handleSize * 0.85f : x - handleSize * 0.85f;
        juce::Path flag;
        flag.addTriangle (x, y0, x, y1, tipX, atTop ? y0 : y1);
        g.fillPath (flag);
    };
    const auto startEndColour = juce::Colour (0xff5fd38d);
    const auto loopColour = juce::Colour (0xff7fa8ff);
    drawMarker (Marker::start, startEndColour, true, true);
    drawMarker (Marker::end, startEndColour, true, false);
    if (loopVisible)
    {
        g.setColour (loopColour.withAlpha (0.12f));
        g.fillRect (juce::Rectangle<float>::leftTopRightBottom (xFor (loopStartPos), h - 3.0f, xFor (loopEndPos), h));
        drawMarker (Marker::loopStart, loopColour, false, true);
        drawMarker (Marker::loopEnd, loopColour, false, false);
    }

    // Slice markers (orange; faded when the note mapping doesn't use slices), with a handle if they can be dragged
    const auto sliceColour = SourceColours::warning.withAlpha (slicesActive ? 0.95f : 0.3f);
    for (size_t i = 0; i < slices.size(); ++i)
    {
        const float x = xFor (slices[i]);
        if (x < -6.0f || x > w + 6.0f)
            continue;
        g.setColour ((int) i == draggedSlice ? SourceColours::text : sliceColour);
        g.fillRect (x - 0.75f, 0.0f, 1.5f, h);
        if (slicesEditable)
            g.fillRect (x - 5.0f, 0.0f, 10.0f, 10.0f);
    }

    // Playheads
    g.setColour (juce::Colours::white);
    for (auto position : playheads)
        if (position >= 0.0f)
            g.fillRect (xFor (position) - 1.0f, 0.0f, 2.0f, h);

    // Zoom level
    if (viewEnd - viewStart < 0.999f)
    {
        g.setColour (SourceColours::dimText);
        g.setFont (juce::Font (juce::FontOptions (11.0f)));
        g.drawText ("x" + juce::String (1.0f / (viewEnd - viewStart), 1) + " (double-click to reset)",
                    getLocalBounds().reduced (6, 3), juce::Justification::bottomRight);
        // Overview of the visible part
        const float overviewY = h - 3.0f;
        g.setColour (SourceColours::control);
        g.fillRect (0.0f, overviewY, w, 3.0f);
        g.setColour (SourceColours::accent);
        g.fillRect (viewStart * w, overviewY, (viewEnd - viewStart) * w, 3.0f);
    }
}


//==============================================================================
// ParameterControl

static juce::StringArray getChoicesForParameter (const SoundParameterInfo& info)
{
    const juce::String name (info.name);
    if (name == "launchMode")      return { "Gate", "Loop", "Loop ping-pong", "Trigger", "Freeze" };
    if (name == "noteMappingMode") return { "Pitch", "Slice", "Pitch & slice", "Repeat" };
    if (name == "midiChannel")
    {
        juce::StringArray channels { "Global" };
        for (int i = 1; i <= 16; ++i)
            channels.add ("Ch " + juce::String (i));
        return channels;
    }
    return {};
}

static juce::String getDisplayNameForParameter (const juce::String& name)
{
    static const std::map<juce::String, juce::String> names {
        { "launchMode", "Launch mode" }, { "startPosition", "Start" }, { "endPosition", "End" },
        { "loopStartPosition", "Loop start" }, { "loopEndPosition", "Loop end" }, { "loopXFadeNSamples", "Loop x-fade" },
        { "reverse", "Reverse" }, { "noteMappingMode", "Note mapping" }, { "numSlices", "Num slices" },
        { "playheadPosition", "Playhead" }, { "freezePlayheadSpeed", "Freeze speed" }, { "filterCutoff", "Cutoff" },
        { "filterRessonance", "Resonance" }, { "filterKeyboardTracking", "KB tracking" }, { "filterAttack", "Attack" },
        { "filterDecay", "Decay" }, { "filterSustain", "Sustain" }, { "filterRelease", "Release" },
        { "filterADSR2CutoffAmt", "Env amount" }, { "gain", "Gain (dB)" }, { "attack", "Attack" }, { "decay", "Decay" },
        { "sustain", "Sustain" }, { "release", "Release" }, { "pan", "Pan" }, { "pitch", "Pitch (st)" },
        { "pitchBendRangeUp", "Bend up" }, { "pitchBendRangeDown", "Bend down" }, { "velSensitivity", "Vel sensitivity" },
        { "midiChannel", "MIDI channel" }, { "pitchShift", "Pitch shift" }, { "timeStretch", "Time stretch" },
    };
    auto it = names.find (name);
    return it != names.end() ? it->second : name;
}

ParameterControl::ParameterControl (const SoundParameterInfo& infoToUse, std::function<void (const SoundParameterInfo&, float)> changeCallback)
    : info (infoToUse), onChange (std::move (changeCallback))
{
    label.setText (getDisplayNameForParameter (info.name), juce::dontSendNotification);
    label.setFont (juce::Font (juce::FontOptions (12.0f)));
    label.setColour (juce::Label::textColourId, SourceColours::dimText);
    label.setTooltip (info.description);
    addAndMakeVisible (label);

    const auto choices = getChoicesForParameter (info);
    if (juce::String (info.name) == "numSlices")
    {
        // "Man" (manual slices, value 0) and 2 to 100 equal slices (1 is a legacy value, converted when sounds load)
        comboBox = std::make_unique<juce::ComboBox>();
        comboBox->addItem ("Man", 1);
        for (int n = 2; n <= (int) info.maxValue; ++n)
            comboBox->addItem (juce::String (n), n + 1);
        comboBox->setTooltip ("Man: slices set with the markers in the waveform. 2-100: equal slices between start and end");
        comboBox->onChange = [this] { onChange (info, (float) (comboBox->getSelectedId() - 1)); };
        addAndMakeVisible (*comboBox);
    }
    else if (! choices.isEmpty())
    {
        comboBox = std::make_unique<juce::ComboBox>();
        comboBox->addItemList (choices, 1);
        comboBox->onChange = [this] { onChange (info, (float) (comboBox->getSelectedId() - 1 + (int) info.minValue)); };
        addAndMakeVisible (*comboBox);
    }
    else if (info.isInt && info.minValue == 0.0f && info.maxValue == 1.0f)
    {
        toggle = std::make_unique<juce::ToggleButton>();
        toggle->onClick = [this] { onChange (info, toggle->getToggleState() ? 1.0f : 0.0f); };
        addAndMakeVisible (*toggle);
    }
    else
    {
        slider = std::make_unique<juce::Slider> (juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight);
        slider->setTextBoxStyle (juce::Slider::TextBoxRight, false, 56, 18);
        juce::NormalisableRange<double> range (info.minValue, info.maxValue, info.isInt ? 1.0 : 0.0);
        const juce::String name (info.name);
        if (name == "filterCutoff" || name == "freezePlayheadSpeed" || name == "loopXFadeNSamples")
            range.setSkewForCentre (name == "filterCutoff" ? 1000.0 : (name == "loopXFadeNSamples" ? 2000.0 : 300.0));
        else if (name == "attack" || name == "decay" || name == "release" || name == "filterAttack" || name == "filterDecay" || name == "filterRelease")
            range.setSkewForCentre (0.15);
        slider->setNormalisableRange (range);
        slider->setNumDecimalPlacesToDisplay (info.isInt ? 0 : (info.maxValue - info.minValue > 50.0f ? 1 : 2));
        slider->setDoubleClickReturnValue (true, info.defaultValue);
        slider->onValueChange = [this] { onChange (info, (float) slider->getValue()); };
        addAndMakeVisible (*slider);
    }
    setValue (info.defaultValue);
}

void ParameterControl::setValue (float value)
{
    if (slider != nullptr)
        slider->setValue (value, juce::dontSendNotification);
    else if (comboBox != nullptr)
        comboBox->setSelectedId (juce::roundToInt (value - info.minValue) + 1, juce::dontSendNotification);
    else if (toggle != nullptr)
        toggle->setToggleState (value > 0.5f, juce::dontSendNotification);
}

bool ParameterControl::isBeingEdited() const
{
    return (slider != nullptr && slider->isMouseButtonDown()) || (comboBox != nullptr && comboBox->isPopupActive());
}

void ParameterControl::resized()
{
    auto b = getLocalBounds();
    label.setBounds (b.removeFromLeft (96));
    if (slider != nullptr)        slider->setBounds (b);
    else if (comboBox != nullptr) comboBox->setBounds (b.withSizeKeepingCentre (b.getWidth(), 20));
    else if (toggle != nullptr)   toggle->setBounds (b.removeFromLeft (30));
}


static juce::String getMpeSourceDisplayName (const juce::String& name)
{
    if (name == MPE_SOURCE_SLIDE)  return "Slide";
    if (name == MPE_SOURCE_GLIDE)  return "Glide";
    if (name == MPE_SOURCE_STRIKE) return "Strike";
    return name;
}


//==============================================================================
// ModulatorScope

juce::Colour ModulatorScope::getVoiceColour (int voiceIndex)
{
    // Distinct hues per voice (golden ratio steps), starting at the accent colour
    const float hue = std::fmod (SourceColours::accent.getHue() + 0.618034f * (float) voiceIndex, 1.0f);
    return juce::Colour::fromHSV (hue, 0.6f, 0.95f, 1.0f);
}

void ModulatorScope::setRange (float newMinRange, float newMaxRange)
{
    if (newMinRange != minRange || newMaxRange != maxRange)
    {
        minRange = newMinRange;
        maxRange = newMaxRange;
        repaint();
    }
}

void ModulatorScope::setMessage (const juce::String& text, bool isWarning)
{
    if (text != message || isWarning != messageIsWarning)
    {
        message = text;
        messageIsWarning = isWarning;
        repaint();
    }
}

void ModulatorScope::addValues (int voiceIndex, uint32_t noteCounter, const std::vector<float>& values, double rate, double now)
{
    auto& trace = traces[voiceIndex];
    if (! trace.points.empty() && trace.noteCounter != noteCounter)
        trace.points.emplace_back (trace.points.back().first, std::numeric_limits<float>::quiet_NaN());  // New note: break the line
    trace.noteCounter = noteCounter;
    const double lastTime = trace.points.empty() ? 0.0 : trace.points.back().first;
    const int numValues = (int) values.size();
    for (int i = 0; i < numValues; ++i)
    {
        const double time = juce::jmax (lastTime + 1.0e-4 * (i + 1), now - (double) (numValues - 1 - i) / rate);
        trace.points.emplace_back (time, values[(size_t) i]);
    }
}

void ModulatorScope::advanceTime (double now)
{
    currentTime = now;
    for (auto it = traces.begin(); it != traces.end();)
    {
        auto& points = it->second.points;
        while (points.size() > 1 && points[1].first < now - windowSeconds)
            points.pop_front();
        if (points.empty() || points.back().first < now - windowSeconds)
            it = traces.erase (it);
        else
            ++it;
    }
    repaint();
}

void ModulatorScope::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour (juce::Colours::black);
    g.fillRoundedRectangle (bounds, 3.0f);
    const auto plot = bounds.reduced (2.0f, 4.0f);
    // Vertical scale: -1..1 for modulators (normalised offset added to the target parameter), 0..1 for mappings
    auto yFor = [this, &plot] (float value)
    {
        return juce::jmap (juce::jlimit (verticalBottom, verticalTop, value), verticalTop, verticalBottom, plot.getY(), plot.getBottom());
    };

    if (verticalBottom < 0.0f)
    {
        g.setColour (SourceColours::control);
        g.drawHorizontalLine ((int) yFor (0.0f), plot.getX(), plot.getRight());
    }
    const float dashes[] = { 3.0f, 3.0f };
    g.setColour (SourceColours::accent.withAlpha (0.5f));
    for (float level : { minRange, maxRange })
        g.drawDashedLine ({ plot.getX(), yFor (level), plot.getRight(), yFor (level) }, dashes, 2, 1.0f);

    for (const auto& [voiceIndex, trace] : traces)
    {
        juce::Path path;
        bool newSegment = true;
        for (const auto& [time, value] : trace.points)
        {
            if (std::isnan (value))
            {
                newSegment = true;
                continue;
            }
            const float x = juce::jmap ((float) (time - (currentTime - windowSeconds)), 0.0f, (float) windowSeconds, plot.getX(), plot.getRight());
            if (newSegment)
                path.startNewSubPath (x, yFor (value));
            else
                path.lineTo (x, yFor (value));
            newSegment = false;
        }
        g.setColour (getVoiceColour (voiceIndex));
        g.strokePath (path, juce::PathStrokeType (1.5f));
    }

    g.setColour (SourceColours::dimText.withAlpha (0.6f));
    g.setFont (juce::Font (juce::FontOptions (10.0f)));
    g.drawText (verticalTop > 0.999f && verticalBottom < -0.999f ? "+1" : juce::String (verticalTop, 0), bounds.reduced (3.0f, 1.0f), juce::Justification::topLeft);
    g.drawText (juce::String (verticalBottom, 0), bounds.reduced (3.0f, 1.0f), juce::Justification::bottomLeft);
    if (message.isNotEmpty())
    {
        g.setColour (messageIsWarning ? SourceColours::warning : SourceColours::dimText.withAlpha (0.8f));
        g.drawFittedText (message, getLocalBounds().reduced (18, 2), juce::Justification::centred, 2);
    }
}

//==============================================================================
// StepsEditor

void StepsEditor::setValues (const SourceHelpers::StepValues& newValues, int newNumSteps)
{
    if (dragging)
        return;
    newNumSteps = juce::jlimit (1, SourceHelpers::maxSteps, newNumSteps);
    if (newValues != values || newNumSteps != numSteps)
    {
        values = newValues;
        numSteps = newNumSteps;
        repaint();
    }
}

void StepsEditor::paint (juce::Graphics& g)
{
    g.setColour (juce::Colours::black);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 3.0f);
    const auto plot = getPlotArea();
    const float stepWidth = plot.getWidth() / (float) numSteps;
    for (int i = 0; i < numSteps; ++i)
    {
        auto column = juce::Rectangle<float> (plot.getX() + stepWidth * (float) i, plot.getY(), stepWidth, plot.getHeight()).reduced (juce::jmin (1.5f, stepWidth * 0.15f), 0.0f);
        g.setColour (SourceColours::control.withAlpha (0.35f));
        g.fillRect (column);
        const float value = values[(size_t) i];
        g.setColour (SourceColours::accent);
        g.fillRect (column.withTop (column.getBottom() - juce::jmax (1.5f, value * column.getHeight())));
    }
    g.setColour (SourceColours::dimText.withAlpha (0.6f));
    g.setFont (juce::Font (juce::FontOptions (10.0f)));
    g.drawText ("max", getLocalBounds().reduced (3, 1), juce::Justification::topRight);
    g.drawText ("min", getLocalBounds().reduced (3, 1), juce::Justification::bottomRight);
}

void StepsEditor::setValueAt (juce::Point<float> position)
{
    const auto plot = getPlotArea();
    const int index = juce::jlimit (0, numSteps - 1, (int) ((position.x - plot.getX()) / plot.getWidth() * (float) numSteps));
    const float value = juce::jlimit (0.0f, 1.0f, (plot.getBottom() - position.y) / plot.getHeight());
    // Fill the steps between the previous and the current position (fast drags skip steps)
    if (lastIndex >= 0 && lastIndex != index)
    {
        const int direction = index > lastIndex ? 1 : -1;
        for (int i = lastIndex + direction; i != index; i += direction)
            values[(size_t) i] = juce::jmap ((float) (i - lastIndex) / (float) (index - lastIndex), lastValue, value);
    }
    values[(size_t) index] = value;
    lastIndex = index;
    lastValue = value;
    repaint();
    if (onChange)
        onChange();
}

void StepsEditor::mouseDown (const juce::MouseEvent& e)
{
    dragging = true;
    lastIndex = -1;
    setValueAt (e.position);
}

void StepsEditor::mouseDrag (const juce::MouseEvent& e)
{
    setValueAt (e.position);
}

void StepsEditor::mouseUp (const juce::MouseEvent&)
{
    dragging = false;
    lastIndex = -1;
    if (onChange)
        onChange();
}

//==============================================================================
// ModulatorsComponent

ModulatorsComponent::LabelledSlider::LabelledSlider (const juce::String& name)
{
    label.setText (name, juce::dontSendNotification);
    label.setFont (juce::Font (juce::FontOptions (11.0f)));
    label.setColour (juce::Label::textColourId, SourceColours::dimText);
    label.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (label);
    slider.setSliderStyle (juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 48, 18);
    addAndMakeVisible (slider);
}

void ModulatorsComponent::LabelledSlider::resized()
{
    auto b = getLocalBounds();
    const int labelWidth = label.getText().length() > 4 ? 44 : (label.getText().length() > 1 ? 36 : 16);
    label.setBounds (b.removeFromLeft (labelWidth));
    slider.setBounds (b);
}

ModulatorsComponent::LabelledCombo::LabelledCombo (const juce::String& name)
{
    label.setText (name, juce::dontSendNotification);
    label.setFont (juce::Font (juce::FontOptions (11.0f)));
    label.setColour (juce::Label::textColourId, SourceColours::dimText);
    label.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (label);
    addAndMakeVisible (combo);
}

void ModulatorsComponent::LabelledCombo::resized()
{
    auto b = getLocalBounds();
    label.setBounds (b.removeFromLeft (label.getText().length() > 4 ? 44 : 36));
    b.removeFromLeft (4);
    combo.setBounds (b.removeFromLeft (juce::jmin (b.getWidth(), 110)));
}

void ModulatorsComponent::ModulatorRow::updateSyncVisibility()
{
    const bool sync = syncButton.getToggleState();
    if (type == SourceHelpers::ModulatorType::steps)
    {
        stepsRate.setVisible (! sync);
        stepsLength.setVisible (sync);
        return;
    }
    if (type != SourceHelpers::ModulatorType::lfo)
        return;
    rate.setVisible (! sync);
    lfoAttack.setVisible (! sync);
    rateDivision.setVisible (sync);
    attackDivision.setVisible (sync);
}

ModulatorsComponent::ModulatorRow::ModulatorRow (ModulatorsComponent& owner, SourceHelpers::ModulatorType typeToUse) : type (typeToUse)
{
    kind = SourceIDs::MODULATOR;
    const bool isLfo = type == SourceHelpers::ModulatorType::lfo;
    const char* typeNames[] = { "LFO", "ADSR", "Steps", "Aftertouch", "Velocity" };
    typeLabel.setText (typeNames[juce::jlimit (0, 4, (int) type)], juce::dontSendNotification);
    typeLabel.setFont (juce::Font (juce::FontOptions (12.0f, juce::Font::bold)));
    typeLabel.setColour (juce::Label::textColourId, SourceColours::accent);
    addAndMakeVisible (typeLabel);

    const auto parameterNames = SourceSound::getModulatableParameterNames();
    for (int i = 0; i < parameterNames.size(); ++i)
        target.addItem (getDisplayNameForParameter (parameterNames[i]), i + 1);
    target.setSelectedItemIndex (juce::jmax (0, parameterNames.indexOf (SourceDefaults::modulatorParameterName)), juce::dontSendNotification);
    target.setTooltip ("Parameter modulated by this modulator");
    addAndMakeVisible (target);

    auto setUp = [this, &owner] (LabelledSlider& s, double min, double max, double interval, double value, const juce::String& tooltip)
    {
        s.slider.setRange (min, max, interval);
        s.slider.setValue (value, juce::dontSendNotification);
        s.slider.setTooltip (tooltip);
        s.slider.onValueChange = [this, &owner] { owner.sendModulator (*this); };
        addAndMakeVisible (s);
    };
    setUp (minRange, -1.0, 1.0, 0.01, SourceDefaults::modulatorMinRange, "Output when the modulator's signal is at its minimum (normalised offset added to the parameter, -1..1)");
    setUp (maxRange, -1.0, 1.0, 0.01, SourceDefaults::modulatorMaxRange, "Output when the modulator's signal is at its maximum (normalised offset added to the parameter, -1..1)");
    minRange.slider.setDoubleClickReturnValue (true, 0.0);
    maxRange.slider.setDoubleClickReturnValue (true, 0.0);

    if (isLfo)
    {
        waveform.addItemList (SourceHelpers::getLfoWaveformNames(), 1);
        waveform.setSelectedItemIndex (SourceDefaults::lfoWaveform, juce::dontSendNotification);
        waveform.setTooltip ("LFO waveform");
        waveform.onChange = [this, &owner] { owner.sendModulator (*this); };
        waveform.setTooltip ("Drag the green handles (top) to set the start and end, and the blue ones (bottom, in loop modes) to set the loop. "
                         "Double-click a handle to reset it. Scroll or pinch to zoom");
    addAndMakeVisible (waveform);
        setUp (rate, SourceHelpers::minLfoRate, SourceHelpers::maxLfoRate, 0.01, SourceDefaults::lfoRate, "LFO rate (Hz)");
        rate.slider.setSkewFactorFromMidPoint (2.0);
        rate.slider.setTextValueSuffix (" Hz");
        rate.slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 18);
        setUp (lfoAttack, 0.0, SourceHelpers::maxModulatorEnvelopeTime, 0.01, SourceDefaults::lfoAttack,
               "Time for the LFO to speed up from 0 Hz to its rate after the note starts (exponential, seconds)");
        lfoAttack.slider.setSkewFactorFromMidPoint (1.0);
        lfoAttack.slider.setTextValueSuffix (" s");
        lfoAttack.slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 56, 18);

        // Synced: rate and attack as note divisions of the tempo
        for (const auto& division : SourceHelpers::getSyncDivisions())
            rateDivision.combo.addItem (division.name, rateDivision.combo.getNumItems() + 1);
        rateDivision.combo.setSelectedItemIndex (SourceDefaults::lfoSyncDivision, juce::dontSendNotification);
        rateDivision.combo.setTooltip ("One LFO cycle per note division (D = dotted, T = triplet)");
        attackDivision.combo.addItemList (SourceHelpers::getSyncAttackNames(), 1);
        attackDivision.combo.setSelectedItemIndex (SourceDefaults::lfoAttackDivision, juce::dontSendNotification);
        attackDivision.combo.setTooltip ("Time for the LFO to speed up from 0 Hz to its rate after the note starts (exponential)");
        for (auto* c : { &rateDivision, &attackDivision })
        {
            c->combo.onChange = [this, &owner] { owner.sendModulator (*this); };
            addChildComponent (c);
        }
        syncButton.setClickingTogglesState (true);
        syncButton.setToggleState (SourceDefaults::lfoSync, juce::dontSendNotification);
        syncButton.setTooltip ("Sync the LFO to the tempo (Ableton Link, the host or the BPM next to the voices)");
        syncButton.onClick = [this, &owner] { updateSyncVisibility(); owner.sendModulator (*this); };
        addAndMakeVisible (syncButton);
        updateSyncVisibility();
    }
    else if (type == SourceHelpers::ModulatorType::steps)
    {
        setUp (numSteps, 1.0, SourceHelpers::maxSteps, 1.0, SourceDefaults::numSteps, "Number of steps");
        numSteps.slider.setSliderStyle (juce::Slider::IncDecButtons);
        numSteps.slider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 36, 20);
        numSteps.slider.setIncDecButtonsMode (juce::Slider::incDecButtonsDraggable_Vertical);
        setUp (stepsRate, SourceHelpers::minStepsRate, SourceHelpers::maxStepsRate, 0.001, SourceDefaults::stepsRate,
               "Cycles of the whole sequence per second");
        stepsRate.slider.setSkewFactorFromMidPoint (0.5);
        stepsRate.slider.setTextValueSuffix (" Hz");
        stepsRate.slider.setNumDecimalPlacesToDisplay (3);
        stepsRate.slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 70, 18);
        for (const auto& length : SourceHelpers::getStepsCycleLengths())
            stepsLength.combo.addItem (length.name, stepsLength.combo.getNumItems() + 1);
        stepsLength.combo.setSelectedItemIndex (SourceDefaults::stepsSyncLength, juce::dontSendNotification);
        stepsLength.combo.setTooltip ("Length of the whole sequence");
        stepsLength.combo.onChange = [this, &owner] { owner.sendModulator (*this); };
        addChildComponent (stepsLength);
        syncButton.setClickingTogglesState (true);
        syncButton.setToggleState (SourceDefaults::lfoSync, juce::dontSendNotification);
        syncButton.setTooltip ("Sync to the tempo (Ableton Link, the host or the BPM next to the voices): the cycle lasts a number of beats/bars");
        syncButton.onClick = [this, &owner] { updateSyncVisibility(); owner.sendModulator (*this); };
        addAndMakeVisible (syncButton);
        stepsEditor.setTooltip ("Drag to draw the value of each step (bottom = min, top = max)");
        stepsEditor.onChange = [this, &owner] { owner.sendModulator (*this); };
        addAndMakeVisible (stepsEditor);
        updateSyncVisibility();
    }
    else if (type == SourceHelpers::ModulatorType::adsr)
    {
        const double maxTime = SourceHelpers::maxModulatorEnvelopeTime;
        setUp (attack, 0.0, maxTime, 0.01, SourceDefaults::modAttack, "Attack time (seconds)");
        setUp (decay, 0.0, maxTime, 0.01, SourceDefaults::modDecay, "Decay time (seconds)");
        setUp (sustain, 0.0, 1.0, 0.01, SourceDefaults::modSustain, "Sustain level (0..1)");
        setUp (release, 0.0, maxTime, 0.01, SourceDefaults::modRelease, "Release time (seconds)");
        for (auto* s : { &attack, &decay, &release })
            s->slider.setSkewFactorFromMidPoint (1.0);
    }

    target.onChange = [this, &owner] { owner.sendModulator (*this); };
    removeButton.onClick = [this, &owner] { owner.actions.send (ACTION_REMOVE_MODULATOR, { owner.currentSoundUUID, uuid }); };
    addAndMakeVisible (removeButton);
    scope.setTooltip ("Output of the modulator (one line per voice playing the sound); dashed lines are min and max");
    addAndMakeVisible (scope);
}

bool ModulatorsComponent::ModulatorRow::isBeingEdited() const
{
    for (auto* s : { &minRange, &maxRange, &rate, &lfoAttack, &attack, &decay, &sustain, &release, &numSteps, &stepsRate })
        if (s->slider.isMouseButtonDown())
            return true;
    return target.isPopupActive() || waveform.isPopupActive() || rateDivision.combo.isPopupActive() || attackDivision.combo.isPopupActive()
        || stepsLength.combo.isPopupActive() || stepsEditor.isDragging();
}

void ModulatorsComponent::Row::paint (juce::Graphics& g)
{
    g.setColour (SourceColours::control.withAlpha (0.35f));
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 4.0f);
}

//------------------------------------------------------------------------------
ModulatorsComponent::MappingRow::MappingRow (ModulatorsComponent& owner, bool mpe) : isMpe (mpe)
{
    kind = isMpe ? SourceIDs::MPE_MAPPING : SourceIDs::MIDI_CC_MAPPING;
    typeLabel.setText (isMpe ? "MPE" : "MIDI CC", juce::dontSendNotification);
    typeLabel.setFont (juce::Font (juce::FontOptions (12.0f, juce::Font::bold)));
    typeLabel.setColour (juce::Label::textColourId, SourceColours::accent);
    addAndMakeVisible (typeLabel);

    for (const auto& info : getSoundParametersInfo())
        if (info.midiCCEnabled && ! info.isInt)
            parameterNames.add (info.name);
    for (int i = 0; i < parameterNames.size(); ++i)
        target.addItem (getDisplayNameForParameter (parameterNames[i]), i + 1);
    target.setSelectedItemIndex (0, juce::dontSendNotification);
    target.setTooltip ("Parameter controlled by this mapping");
    target.onChange = [this, &owner] { owner.sendMapping (*this); };
    addAndMakeVisible (target);

    auto setUp = [this, &owner] (LabelledSlider& s, double min, double max, double interval, double value, const juce::String& tooltip)
    {
        s.slider.setRange (min, max, interval);
        s.slider.setValue (value, juce::dontSendNotification);
        s.slider.setTooltip (tooltip);
        s.slider.onValueChange = [this, &owner] { owner.sendMapping (*this); };
        addAndMakeVisible (s);
    };
    const juce::String sourceName = isMpe ? "the MPE dimension" : "the CC";
    setUp (minRange, 0.0, 1.0, 0.01, 0.0, "Parameter value (normalised) when " + sourceName + " is at its minimum");
    setUp (maxRange, 0.0, 1.0, 0.01, 1.0, "Parameter value (normalised) when " + sourceName + " is at its maximum");
    if (isMpe)
    {
        const auto sourceNames = SourceHelpers::getMpeSourceNames();
        for (int i = 0; i < sourceNames.size(); ++i)
            source.addItem (getMpeSourceDisplayName (sourceNames[i]), i + 1);
        source.setSelectedItemIndex (0, juce::dontSendNotification);
        source.setTooltip ("Slide: CC74, Glide: per-note pitch bend, Strike: note velocity. Per-note pressure is aftertouch: use the Modulation section");
        source.onChange = [this, &owner] { owner.sendMapping (*this); };
        addAndMakeVisible (source);
        setUp (curve, -1.0, 1.0, 0.01, 0.0, "Response curve: 0 = linear, positive = slow start, negative = fast start");
        curve.slider.setDoubleClickReturnValue (true, 0.0);
    }
    else
    {
        setUp (ccNumber, 0.0, 127.0, 1.0, 1.0, "MIDI CC number");
        ccNumber.slider.setSliderStyle (juce::Slider::IncDecButtons);
        ccNumber.slider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 36, 20);
        ccNumber.slider.setIncDecButtonsMode (juce::Slider::incDecButtonsDraggable_Vertical);
    }
    removeButton.onClick = [this, &owner]
    {
        owner.actions.send (isMpe ? ACTION_REMOVE_MPE_MAPPING : ACTION_REMOVE_CC_MAPPING, { owner.currentSoundUUID, uuid });
    };
    addAndMakeVisible (removeButton);
    scope.setVerticalRange (0.0f, 1.0f);
    scope.setTooltip (isMpe ? "Value set by the mapping (normalised parameter value), one line per note"
                            : "Value set by the mapping (normalised parameter value) when the CC is received; the last value is held");
    addAndMakeVisible (scope);
}

bool ModulatorsComponent::MappingRow::isBeingEdited() const
{
    for (auto* s : { &minRange, &maxRange, &curve, &ccNumber })
        if (s->slider.isMouseButtonDown())
            return true;
    return target.isPopupActive() || source.isPopupActive();
}

void ModulatorsComponent::MappingRow::resized()
{
    // Two lines: type, target, source or CC number | min, max (and curve). Information on the right.
    auto b = getLocalBounds().reduced (6, 4);
    scope.setBounds (b.removeFromRight (juce::jlimit (140, 260, b.getWidth() / 3)));
    b.removeFromRight (8);
    auto line1 = b.removeFromTop (b.getHeight() / 2);
    auto line2 = b;
    auto centred = [] (juce::Rectangle<int> r, int height) { return r.withSizeKeepingCentre (r.getWidth(), height); };
    removeButton.setBounds (line1.removeFromRight (22).withSizeKeepingCentre (22, 20));
    typeLabel.setBounds (line1.removeFromLeft (typeLabelWidth));
    target.setBounds (centred (line1.removeFromLeft (juce::jmin (170, line1.getWidth() / 2)), 22));
    line1.removeFromLeft (6);
    if (isMpe)
    {
        source.setBounds (centred (line1.removeFromLeft (juce::jmin (120, line1.getWidth() - 6)), 22));
        const int width = line2.getWidth() / 3;
        minRange.setBounds (centred (line2.removeFromLeft (width), 22));
        maxRange.setBounds (centred (line2.removeFromLeft (width), 22));
        curve.setBounds (centred (line2, 22));
    }
    else
    {
        ccNumber.setBounds (centred (line1.removeFromLeft (juce::jmin (130, line1.getWidth() - 6)), 24));
        const int halfWidth = line2.getWidth() / 2;
        minRange.setBounds (centred (line2.removeFromLeft (halfWidth), 22));
        maxRange.setBounds (centred (line2, 22));
    }
}



void ModulatorsComponent::ModulatorRow::resized()
{
    // Three lines: type, target (and waveform) | min, max | rate or A, D, S, R. The oscilloscope on the right.
    auto b = getLocalBounds().reduced (6, 4);
    scope.setBounds (b.removeFromRight (juce::jlimit (140, 260, b.getWidth() / 3)));
    b.removeFromRight (8);
    const int lineHeight = type == SourceHelpers::ModulatorType::steps ? 28 : b.getHeight() / (isSourceType() ? 2 : 3);
    auto line1 = b.removeFromTop (lineHeight);
    auto line2 = b.removeFromTop (lineHeight);
    auto line3 = type == SourceHelpers::ModulatorType::steps ? b.removeFromTop (lineHeight) : b;
    auto centred = [] (juce::Rectangle<int> r, int height) { return r.withSizeKeepingCentre (r.getWidth(), height); };

    removeButton.setBounds (line1.removeFromRight (22).withSizeKeepingCentre (22, 20));
    typeLabel.setBounds (line1.removeFromLeft (typeLabelWidth));
    target.setBounds (centred (line1.removeFromLeft (juce::jmin (170, line1.getWidth() / 2)), 22));
    line1.removeFromLeft (6);
    if (type == SourceHelpers::ModulatorType::lfo || type == SourceHelpers::ModulatorType::steps)
    {
        line1.removeFromRight (6);
        syncButton.setBounds (centred (line1.removeFromRight (54), 22));
        line1.removeFromRight (6);
        if (type == SourceHelpers::ModulatorType::lfo)
            waveform.setBounds (centred (line1.removeFromLeft (juce::jmin (120, line1.getWidth())), 22));
    }

    const int halfWidth = line2.getWidth() / 2;
    minRange.setBounds (centred (line2.removeFromLeft (halfWidth), 22));
    maxRange.setBounds (centred (line2, 22));

    if (type == SourceHelpers::ModulatorType::lfo)
    {
        const auto rateBounds = centred (line3.removeFromLeft (halfWidth), 22);
        const auto attackBounds = centred (line3, 22);
        rate.setBounds (rateBounds);
        rateDivision.setBounds (rateBounds);
        lfoAttack.setBounds (attackBounds);
        attackDivision.setBounds (attackBounds);
    }
    else if (type == SourceHelpers::ModulatorType::steps)
    {
        const auto rateBounds = centred (line3.removeFromLeft (halfWidth), 22);
        stepsRate.setBounds (rateBounds);
        stepsLength.setBounds (rateBounds);
        numSteps.setBounds (centred (line3.removeFromLeft (juce::jmin (line3.getWidth(), 150)), 24));
        b.removeFromTop (2);
        stepsEditor.setBounds (b);
    }
    else if (type == SourceHelpers::ModulatorType::adsr)
    {
        const int width = line3.getWidth() / 4;
        for (auto* s : { &attack, &decay, &sustain })
            s->setBounds (centred (line3.removeFromLeft (width), 22));
        release.setBounds (centred (line3, 22));
    }
}

ModulatorsComponent::ModulatorsComponent (SourceActionSender actionSender) : actions (std::move (actionSender))
{
    addLfoButton.onClick = [this] { addModulator (SourceHelpers::ModulatorType::lfo); };
    addAdsrButton.onClick = [this] { addModulator (SourceHelpers::ModulatorType::adsr); };
    addStepsButton.onClick = [this] { addModulator (SourceHelpers::ModulatorType::steps); };
    addAftertouchButton.onClick = [this] { addModulator (SourceHelpers::ModulatorType::aftertouch); };
    addVelocityButton.onClick = [this] { addModulator (SourceHelpers::ModulatorType::velocity); };
    addAftertouchButton.setTooltip ("Modulate a parameter with the aftertouch (channel or polyphonic pressure) plus the mod wheel");
    addVelocityButton.setTooltip ("Modulate a parameter with the velocity of each note");
    addAndMakeVisible (addAftertouchButton);
    addAndMakeVisible (addVelocityButton);
    addStepsButton.setTooltip ("Add a sequence of steps (cycled at a rate or synced to the tempo; restarts with each note)");
    addAndMakeVisible (addStepsButton);
    addLfoButton.setTooltip ("Add a low frequency oscillator (restarts with each note)");
    addAdsrButton.setTooltip ("Add an envelope (attack, decay, sustain, release; restarts with each note)");
    addMpeButton.onClick = [this] { addMapping (true); };
    addCcButton.onClick = [this] { addMapping (false); };
    addMpeButton.setTooltip ("Map a per-note MPE expression (slide, glide, strike) to a parameter of the note");
    addCcButton.setTooltip ("Map a MIDI CC to a parameter of the sound (changes the sound's value, for all notes)");
    addAndMakeVisible (addLfoButton);
    addAndMakeVisible (addAdsrButton);
    addAndMakeVisible (addMpeButton);
    addAndMakeVisible (addCcButton);
}

void ModulatorsComponent::addModulator (SourceHelpers::ModulatorType type)
{
    if (currentSoundUUID.isEmpty() || rows.size() >= SourceSound::maxModulationsPerSound)
        return;
    // Send the defaults of a new row (the row is then created from the state)
    ModulatorRow row (*this, type);
    row.uuid = juce::Uuid().toString();
    sendModulator (row);
}

void ModulatorsComponent::addMapping (bool isMpe)
{
    if (currentSoundUUID.isEmpty() || rows.size() >= SourceSound::maxModulationsPerSound)
        return;
    MappingRow row (*this, isMpe);
    row.uuid = juce::Uuid().toString();
    row.target.setSelectedItemIndex (juce::jmax (0, row.parameterNames.indexOf ("filterCutoff")), juce::dontSendNotification);
    sendMapping (row);
}

void ModulatorsComponent::sendMapping (MappingRow& row)
{
    if (currentSoundUUID.isEmpty())
        return;
    const auto parameterName = row.parameterNames[juce::jmax (0, row.target.getSelectedItemIndex())];
    if (row.isMpe)
        actions.send (ACTION_ADD_OR_UPDATE_MPE_MAPPING, {
            currentSoundUUID, row.uuid, SourceHelpers::getMpeSourceNames()[juce::jmax (0, row.source.getSelectedItemIndex())], parameterName,
            juce::String (row.minRange.slider.getValue()), juce::String (row.maxRange.slider.getValue()), juce::String (row.curve.slider.getValue()) });
    else
        actions.send (ACTION_ADD_OR_UPDATE_CC_MAPPING, {
            currentSoundUUID, row.uuid, juce::String ((int) row.ccNumber.slider.getValue()), parameterName,
            juce::String (row.minRange.slider.getValue()), juce::String (row.maxRange.slider.getValue()) });
}

void ModulatorsComponent::sendModulator (ModulatorRow& row)
{
    if (currentSoundUUID.isEmpty())
        return;
    const auto parameterNames = SourceSound::getModulatableParameterNames();
    actions.send (ACTION_ADD_OR_UPDATE_MODULATOR, {
        currentSoundUUID, row.uuid, SourceHelpers::getModulatorTypeNames()[(int) row.type],
        parameterNames[juce::jmax (0, row.target.getSelectedItemIndex())],
        juce::String (row.minRange.slider.getValue()), juce::String (row.maxRange.slider.getValue()),
        juce::String (juce::jmax (0, row.waveform.getSelectedItemIndex())), juce::String (row.rate.slider.getValue()),
        juce::String (row.attack.slider.getValue()), juce::String (row.decay.slider.getValue()),
        juce::String (row.sustain.slider.getValue()), juce::String (row.release.slider.getValue()),
        juce::String (row.lfoAttack.slider.getValue()), row.syncButton.getToggleState() ? "1" : "0",
        juce::String (juce::jmax (0, row.rateDivision.combo.getSelectedItemIndex())), juce::String (juce::jmax (0, row.attackDivision.combo.getSelectedItemIndex())),
        juce::String ((int) row.numSteps.slider.getValue()), juce::String (row.stepsRate.slider.getValue()),
        juce::String (juce::jmax (0, row.stepsLength.combo.getSelectedItemIndex())), SourceHelpers::formatStepValues (row.stepsEditor.getValues()) });
}

void ModulatorsComponent::updateFromState (const juce::String& soundUUID, const juce::ValueTree& soundState, bool mpeEnabled)
{
    if (soundUUID != currentSoundUUID)
    {
        rows.clear();
        modulatorRows.clear();
        mpeRows.clear();
        ccRows.clear();
        lastReadCounts.clear();
    }
    currentSoundUUID = soundUUID;

    std::vector<juce::ValueTree> states;
    for (const auto& child : soundState)
        if (child.hasType (SourceIDs::MODULATOR) || child.hasType (SourceIDs::MPE_MAPPING) || child.hasType (SourceIDs::MIDI_CC_MAPPING))
            states.push_back (child);
    auto getModulatorType = [] (const juce::ValueTree& state)
    {
        const auto type = SourceHelpers::modulatorTypeFromName (state.getProperty (SourceIDs::modulatorType).toString());
        return type == SourceHelpers::ModulatorType::numTypes ? SourceHelpers::ModulatorType::lfo : type;
    };

    // Rebuild the rows if the list (kinds or modulator types) changed
    bool needsRebuild = (int) states.size() != rows.size();
    for (size_t i = 0; i < states.size() && ! needsRebuild; ++i)
    {
        auto* row = rows[(int) i];
        needsRebuild = row->kind != states[i].getType();
        if (! needsRebuild && row->kind == SourceIDs::MODULATOR)
            needsRebuild = static_cast<ModulatorRow*> (row)->type != getModulatorType (states[i]);
    }
    if (needsRebuild)
    {
        rows.clear();
        modulatorRows.clear();
        mpeRows.clear();
        ccRows.clear();
        for (const auto& state : states)
        {
            if (state.hasType (SourceIDs::MODULATOR))
            {
                auto* row = new ModulatorRow (*this, getModulatorType (state));
                modulatorRows.push_back (row);
                addAndMakeVisible (rows.add (row));
            }
            else
            {
                auto* row = new MappingRow (*this, state.hasType (SourceIDs::MPE_MAPPING));
                (row->isMpe ? mpeRows : ccRows).push_back (row);
                addAndMakeVisible (rows.add (row));
            }
        }
    }

    for (size_t i = 0; i < states.size(); ++i)
    {
        auto* row = rows[(int) i];
        row->uuid = states[i].getProperty (SourceIDs::uuid).toString();
        if (row->kind == SourceIDs::MODULATOR)
            updateModulatorRow (*static_cast<ModulatorRow*> (row), states[i]);
        else
            updateMappingRow (*static_cast<MappingRow*> (row), states[i], mpeEnabled);
    }
    const bool canAdd = soundUUID.isNotEmpty() && rows.size() < SourceSound::maxModulationsPerSound;
    for (auto* button : getAddButtons())
        button->setEnabled (canAdd);
    if (needsRebuild)
        resized();
}

void ModulatorsComponent::updateModulatorRow (ModulatorRow& modulatorRow, const juce::ValueTree& state)
{
    auto* row = &modulatorRow;
    const auto parameterNames = SourceSound::getModulatableParameterNames();
        const float minRange = state.getProperty (SourceIDs::minRange, SourceDefaults::modulatorMinRange);
        const float maxRange = state.getProperty (SourceIDs::maxRange, SourceDefaults::modulatorMaxRange);
        row->scope.setRange (minRange, maxRange);
        if (row->isBeingEdited())
            return;
        row->target.setSelectedItemIndex (juce::jmax (0, parameterNames.indexOf (state.getProperty (SourceIDs::parameterName).toString())), juce::dontSendNotification);
        row->minRange.slider.setValue (minRange, juce::dontSendNotification);
        row->maxRange.slider.setValue (maxRange, juce::dontSendNotification);
        row->waveform.setSelectedItemIndex ((int) state.getProperty (SourceIDs::lfoWaveform, SourceDefaults::lfoWaveform), juce::dontSendNotification);
        row->rate.slider.setValue ((float) state.getProperty (SourceIDs::lfoRate, SourceDefaults::lfoRate), juce::dontSendNotification);
        row->attack.slider.setValue ((float) state.getProperty (SourceIDs::modAttack, SourceDefaults::modAttack), juce::dontSendNotification);
        row->decay.slider.setValue ((float) state.getProperty (SourceIDs::modDecay, SourceDefaults::modDecay), juce::dontSendNotification);
        row->sustain.slider.setValue ((float) state.getProperty (SourceIDs::modSustain, SourceDefaults::modSustain), juce::dontSendNotification);
        row->release.slider.setValue ((float) state.getProperty (SourceIDs::modRelease, SourceDefaults::modRelease), juce::dontSendNotification);
        row->lfoAttack.slider.setValue ((float) state.getProperty (SourceIDs::lfoAttack, SourceDefaults::lfoAttack), juce::dontSendNotification);
        row->rateDivision.combo.setSelectedItemIndex ((int) state.getProperty (SourceIDs::lfoSyncDivision, SourceDefaults::lfoSyncDivision), juce::dontSendNotification);
        row->attackDivision.combo.setSelectedItemIndex ((int) state.getProperty (SourceIDs::lfoAttackDivision, SourceDefaults::lfoAttackDivision), juce::dontSendNotification);
        row->numSteps.slider.setValue ((int) state.getProperty (SourceIDs::numSteps, SourceDefaults::numSteps), juce::dontSendNotification);
        row->stepsRate.slider.setValue ((float) state.getProperty (SourceIDs::stepsRate, SourceDefaults::stepsRate), juce::dontSendNotification);
        row->stepsLength.combo.setSelectedItemIndex ((int) state.getProperty (SourceIDs::stepsSyncLength, SourceDefaults::stepsSyncLength), juce::dontSendNotification);
        row->stepsEditor.setValues (SourceHelpers::parseStepValues (state.getProperty (SourceIDs::stepValues).toString()), (int) row->numSteps.slider.getValue());
        const bool sync = state.getProperty (SourceIDs::lfoSync, SourceDefaults::lfoSync);
        if (sync != row->syncButton.getToggleState())
        {
            row->syncButton.setToggleState (sync, juce::dontSendNotification);
            row->updateSyncVisibility();
        }
}

void ModulatorsComponent::updateMappingRow (MappingRow& row, const juce::ValueTree& state, bool mpeEnabled)
{
    const juce::String parameterName = state.getProperty (SourceIDs::parameterName).toString();
    juce::String info;
    bool warning = false;
    if (row.isMpe)
    {
        const auto source = state.getProperty (SourceIDs::mpeSource).toString();
        info = mpeEnabled ? "Per note: " + getMpeSourceDisplayName (source).toLowerCase() + " sets the value of the note"
                          : "MPE input is disabled (enable it in the Settings tab)";
        warning = ! mpeEnabled;
    }
    else
    {
        info = "CC " + state.getProperty (SourceIDs::ccNumber, 1).toString() + " sets the sound's value (all notes)";
    }
    row.scope.setMessage (info, warning);
    row.scope.setRange ((float) state.getProperty (SourceIDs::minRange, 0.0f), (float) state.getProperty (SourceIDs::maxRange, 1.0f));
    if (row.isBeingEdited())
        return;
    row.target.setSelectedItemIndex (juce::jmax (0, row.parameterNames.indexOf (parameterName)), juce::dontSendNotification);
    row.minRange.slider.setValue ((float) state.getProperty (SourceIDs::minRange, 0.0f), juce::dontSendNotification);
    row.maxRange.slider.setValue ((float) state.getProperty (SourceIDs::maxRange, 1.0f), juce::dontSendNotification);
    if (row.isMpe)
    {
        row.source.setSelectedItemIndex (juce::jmax (0, SourceHelpers::getMpeSourceNames().indexOf (state.getProperty (SourceIDs::mpeSource).toString())), juce::dontSendNotification);
        row.curve.slider.setValue ((float) state.getProperty (SourceIDs::curve, 0.0f), juce::dontSendNotification);
    }
    else
    {
        row.ccNumber.slider.setValue ((int) state.getProperty (SourceIDs::ccNumber, 1), juce::dontSendNotification);
    }
}

void ModulatorsComponent::updateScopes (SourceSampler& sampler)
{
    if (currentSoundUUID.isEmpty() || rows.isEmpty())
        return;
    const double now = juce::Time::getMillisecondCounterHiRes() / 1000.0;

    // MIDI CC mappings: the value set by the last CC received (held until a new one arrives)
    if (! ccRows.empty())
    {
        if (auto* sound = sampler.getSoundWithUUID (currentSoundUUID))
        {
            for (auto* row : ccRows)
            {
                const float ccValue = sound->getLastCcValue ((int) row->ccNumber.slider.getValue());
                if (ccValue >= 0.0f)
                    row->scope.addValues (0, 0, { juce::jmap (ccValue, (float) row->minRange.slider.getValue(), (float) row->maxRange.slider.getValue()) },
                                          SourceSamplerVoice::modulatorHistoryRate, now);
            }
        }
    }

    std::map<int, uint32_t> newReadCounts;
    sampler.forEachVoicePlayingSound (currentSoundUUID, [&] (int voiceIndex, const SourceSamplerVoice& voice)
    {
        // MPE mappings: the value of each note (sampled at the UI rate, expressions change slowly)
        const uint32_t voiceNoteCounter = voice.getModulatorNoteCounter();
        const int numMpeValues = voice.getPublishedNumMpeValues();
        for (int m = 0; m < juce::jmin ((int) mpeRows.size(), numMpeValues); ++m)
            mpeRows[(size_t) m]->scope.addValues (voiceIndex, voiceNoteCounter, { voice.getPublishedMpeValue (m) }, SourceSamplerVoice::modulatorHistoryRate, now);
        if (modulatorRows.empty())
            return;

        const uint32_t count = voice.getModulatorHistoryCount();
        const uint32_t noteCounter = voice.getModulatorNoteCounter();
        // New voices: show the last 100 ms. Never read the part of the history that may be being overwritten.
        auto previous = lastReadCounts.find (voiceIndex);
        uint32_t available = previous != lastReadCounts.end() ? count - previous->second : 20u;
        available = juce::jmin (available, count, (uint32_t) SourceSamplerVoice::modulatorHistorySize / 2);
        newReadCounts[voiceIndex] = count;
        if (available == 0)
            return;
        std::vector<float> values (available);
        for (int m = 0; m < juce::jmin ((int) modulatorRows.size(), SourceSound::maxModulators); ++m)
        {
            for (uint32_t k = 0; k < available; ++k)
                values[k] = voice.getModulatorHistoryValue (m, count - available + k);
            modulatorRows[(size_t) m]->scope.addValues (voiceIndex, noteCounter, values, SourceSamplerVoice::modulatorHistoryRate, now);
        }
    });
    lastReadCounts = std::move (newReadCounts);  // Voices that stopped playing the sound are forgotten
    for (auto* row : modulatorRows)
        row->scope.advanceTime (now);
    for (auto* row : mpeRows)
        row->scope.advanceTime (now);
    for (auto* row : ccRows)
        row->scope.advanceTime (now);
}

int ModulatorsComponent::getIdealHeight() const
{
    int height = 28 + 30;
    for (auto* row : rows)
        height += row->getPreferredHeight() + 4;
    return height;
}

void ModulatorsComponent::paint (juce::Graphics& g)
{
    auto header = getLocalBounds().removeFromTop (24);
    g.setColour (SourceColours::text);
    g.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
    g.drawText ("Modulators", header, juce::Justification::centredLeft);
    g.setColour (SourceColours::dimText);
    g.setFont (juce::Font (juce::FontOptions (11.0f)));
    g.drawText (rows.isEmpty() ? "LFOs, envelopes, steps, MPE and MIDI CC mappings that modulate the sound's parameters"
                               : "LFO/ADSR/steps output = min..max, added to the parameter (normalised); 0 = no modulation",
                header.withTrimmedLeft (90), juce::Justification::centredLeft);
    g.drawText (juce::String (rows.size()) + " / " + juce::String (SourceSound::maxModulationsPerSound), header, juce::Justification::centredRight);
}

void ModulatorsComponent::resized()
{
    auto b = getLocalBounds();
    b.removeFromTop (28);
    for (auto* row : rows)
    {
        row->setBounds (b.removeFromTop (row->getPreferredHeight()));
        b.removeFromTop (4);
    }
    auto buttons = b.removeFromTop (22);
    const auto addButtons = getAddButtons();
    const int width = juce::jmin (100, (buttons.getWidth() - 6 * ((int) addButtons.size() - 1)) / (int) addButtons.size());
    for (auto* button : addButtons)
    {
        button->setBounds (buttons.removeFromLeft (width));
        buttons.removeFromLeft (6);
    }
}

//==============================================================================
// SoundEditorComponent

using SourceHelpers::getShortLicenseName;

namespace
{
    const std::vector<std::pair<juce::String, juce::StringArray>>& getParameterGroups()
    {
        static const std::vector<std::pair<juce::String, juce::StringArray>> groups {
            { "Playback",   { "launchMode", "startPosition", "endPosition", "loopStartPosition", "loopEndPosition", "loopXFadeNSamples", "reverse", "playheadPosition", "freezePlayheadSpeed" } },
            { "Mapping",    { "noteMappingMode", "numSlices", "midiChannel" } },
            { "Pitch",      { "pitch", "pitchBendRangeUp", "pitchBendRangeDown", "pitchShift", "timeStretch" } },
            { "Amplitude",  { "gain", "pan", "attack", "decay", "sustain", "release", "velSensitivity" } },
            { "Filter",     { "filterCutoff", "filterRessonance", "filterKeyboardTracking", "filterAttack", "filterDecay", "filterSustain", "filterRelease", "filterADSR2CutoffAmt" } },
        };
        return groups;
    }

    bool isWaveformMarkerParameter (const juce::String& name)
    {
        return name == "startPosition" || name == "endPosition" || name == "loopStartPosition" || name == "loopEndPosition";
    }
}

SoundEditorComponent::SoundEditorComponent (SourceSampler& s, SourceActionSender actionSender)
    : sampler (s), actions (actionSender), modulators (actionSender)
{
    titleLabel.setFont (juce::Font (juce::FontOptions (18.0f, juce::Font::bold)));
    addAndMakeVisible (titleLabel);
    detailsLabel.setFont (juce::Font (juce::FontOptions (12.0f)));
    detailsLabel.setColour (juce::Label::textColourId, SourceColours::dimText);
    addAndMakeVisible (detailsLabel);


    addAndMakeVisible (waveform);

    rootNoteLabel.setText ("Root note", juce::dontSendNotification);
    rootNoteLabel.setFont (juce::Font (juce::FontOptions (12.0f)));
    rootNoteLabel.setColour (juce::Label::textColourId, SourceColours::dimText);
    addAndMakeVisible (rootNoteLabel);
    rootNoteSlider.setSliderStyle (juce::Slider::IncDecButtons);
    rootNoteSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 60, 20);
    rootNoteSlider.setRange (0, 127, 1);
    rootNoteSlider.textFromValueFunction = [] (double v) { return juce::MidiMessage::getMidiNoteName ((int) v, true, true, 3); };
    rootNoteSlider.valueFromTextFunction = [] (const juce::String& text)
    {
        for (int n = 0; n < 128; ++n)
            if (juce::MidiMessage::getMidiNoteName (n, true, true, 3).equalsIgnoreCase (text.trim()))
                return (double) n;
        return text.getDoubleValue();
    };
    rootNoteSlider.onValueChange = [this]
    {
        if (soundUUID.isNotEmpty())
            actions.send (ACTION_SET_SOUND_ASSIGNED_NOTES, { soundUUID, "", juce::String ((int) rootNoteSlider.getValue()) });
    };
    addAndMakeVisible (rootNoteSlider);
    similarButton.setTooltip ("Replace the sound with a similar one from Freesound (keeping its settings)");
    similarButton.onClick = [this] { if (soundUUID.isNotEmpty()) actions.send (ACTION_REPLACE_SOUND_WITH_SIMILAR, { soundUUID }); };
    addAndMakeVisible (similarButton);

    // Manual slices: markers in the waveform
    addSliceButton.setTooltip ("Add a slice marker (in the largest slice in view); drag its handle to move it, double-click the handle to remove it");
    addSliceButton.onClick = [this]
    {
        const double duration = waveform.getDuration();
        if (duration <= 0.0 || (int) manualSliceTimes.size() >= MAX_MANUAL_SLICES)
            return;
        // Middle of the largest gap between markers in the visible part of the sound
        const auto view = waveform.getVisibleRange();
        std::vector<float> bounds { view.getStart() };
        for (auto t : manualSliceTimes)
            if ((float) (t / duration) > view.getStart() && (float) (t / duration) < view.getEnd())
                bounds.push_back ((float) (t / duration));
        bounds.push_back (view.getEnd());
        float bestStart = bounds[0], bestLength = 0.0f;
        for (size_t i = 0; i + 1 < bounds.size(); ++i)
            if (bounds[i + 1] - bounds[i] > bestLength)
            {
                bestLength = bounds[i + 1] - bounds[i];
                bestStart = bounds[i];
            }
        auto times = manualSliceTimes;
        times.push_back ((float) ((bestStart + bestLength * 0.5f) * duration));
        sendManualSlices (times);
    };
    addChildComponent (addSliceButton);
    slicesFromOnsetsButton.onClick = [this] { if (soundUUID.isNotEmpty()) actions.send (ACTION_SET_SOUND_SLICES_FROM_ONSETS, { soundUUID }); };
    addChildComponent (slicesFromOnsetsButton);
    waveform.onMarkerChanged = [this] (const juce::String& name, float value)
    {
        if (auto* info = findSoundParameterInfo (name); info != nullptr && soundUUID.isNotEmpty())
            parameterChanged (*info, value);
    };
    waveform.onSlicesChanged = [this] (const std::vector<float>& positions)
    {
        std::vector<float> times;
        for (auto p : positions)
            times.push_back ((float) (p * waveform.getDuration()));
        sendManualSlices (times);
    };

    // Create one control per parameter, organised in groups (parameters not in any group go to "Other")
    juce::StringArray grouped;
    for (const auto& [groupName, names] : getParameterGroups())
    {
        Group group;
        group.name = groupName;
        for (const auto& name : names)
        {
            if (auto* info = findSoundParameterInfo (name))
            {
                group.controls.push_back (parameterControls.add (new ParameterControl (*info, [this] (const SoundParameterInfo& i, float v) { parameterChanged (i, v); })));
                grouped.add (name);
            }
        }
        groups.push_back (std::move (group));
    }
    Group other;
    other.name = "Other";
    for (const auto& info : getSoundParametersInfo())
        if (! grouped.contains (info.name))
            other.controls.push_back (parameterControls.add (new ParameterControl (info, [this] (const SoundParameterInfo& i, float v) { parameterChanged (i, v); })));
    if (! other.controls.empty())
        groups.push_back (std::move (other));

    for (auto& group : groups)
    {
        group.header = std::make_unique<juce::Label> (group.name, group.name);
        group.header->setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
        group.header->setColour (juce::Label::textColourId, SourceColours::accent);
        parametersContent.addAndMakeVisible (*group.header);
        for (auto* control : group.controls)
            parametersContent.addAndMakeVisible (control);
    }
    parametersContent.addAndMakeVisible (modulators);

    parametersViewport.setViewedComponent (&parametersContent, false);
    parametersViewport.setScrollBarsShown (true, false);
    addAndMakeVisible (parametersViewport);

    setSound ({});
}

juce::ValueTree SoundEditorComponent::getSoundState() const
{
    if (soundUUID.isEmpty())
        return {};
    return sampler.state.getChildWithName (SourceIDs::PRESET).getChildWithProperty (SourceIDs::uuid, soundUUID);
}

void SoundEditorComponent::sendManualSlices (const std::vector<float>& times)
{
    if (soundUUID.isEmpty())
        return;
    auto sorted = times;
    std::sort (sorted.begin(), sorted.end());
    manualSliceTimes = sorted;
    actions.send (ACTION_SET_SOUND_SLICES, { soundUUID, SourceHelpers::formatSliceTimes (sorted) });
}

void SoundEditorComponent::setSound (const juce::String& newSoundUUID)
{
    soundUUID = newSoundUUID;
    loadedWaveformForSampleUUID = {};
    waveform.setAudio (nullptr, 0.0);
    waveform.setPlayheads ({});
    updateFromState();
    resized();
}

void SoundEditorComponent::parameterChanged (const SoundParameterInfo& info, float value)
{
    // An empty sound UUID means "all sounds"
    if (info.isInt)
        actions.send (ACTION_SET_SOUND_PARAMETER_INT, { soundUUID, info.name, juce::String (juce::roundToInt (value)) });
    else
        actions.send (ACTION_SET_SOUND_PARAMETER_FLOAT, { soundUUID, info.name, juce::String (value) });
}

void SoundEditorComponent::updateFromState()
{
    const bool allSounds = soundUUID.isEmpty();
    auto soundState = getSoundState();
    if (! allSounds && ! soundState.isValid())
        return;  // Sound was removed, the owner will select another one

    waveform.setVisible (! allSounds);
    rootNoteSlider.setVisible (! allSounds);
    rootNoteLabel.setVisible (! allSounds);
    similarButton.setVisible (! allSounds);
    modulators.setVisible (! allSounds);
    // Start/end and loop positions are edited with the waveform's handles when one sound is shown (sliders for all sounds)
    bool layoutChanged = false;
    for (auto* control : parameterControls)
        if (isWaveformMarkerParameter (control->getInfo().name) && control->isVisible() != allSounds)
        {
            control->setVisible (allSounds);
            layoutChanged = true;
        }
    if (layoutChanged)
        layoutParameters();

    if (allSounds)
    {
        titleLabel.setText ("All sounds", juce::dontSendNotification);
        detailsLabel.setText ("Changes made here are applied to all loaded sounds.", juce::dontSendNotification);
        // Show the values of the first sound as reference (or the defaults if there are no sounds)
        auto firstSound = sampler.state.getChildWithName (SourceIDs::PRESET).getChildWithName (SourceIDs::SOUND);
        for (auto* control : parameterControls)
            if (! control->isBeingEdited())
                control->setValue (firstSound.isValid() ? (float) firstSound.getProperty (control->getInfo().name, control->getInfo().defaultValue)
                                                        : control->getInfo().defaultValue);
        return;
    }

    // Title and details
    juce::ValueTree firstSample = soundState.getChildWithName (SourceIDs::SOUND_SAMPLE);
    titleLabel.setText (firstSample.getProperty (SourceIDs::name, "(no name)").toString(), juce::dontSendNotification);
    juce::StringArray details;
    if (firstSample.getProperty (SourceIDs::username).toString().isNotEmpty())
        details.add ("by " + firstSample.getProperty (SourceIDs::username).toString());
    if ((int) firstSample.getProperty (SourceIDs::soundId, -1) > -1)
        details.add ("Freesound #" + firstSample.getProperty (SourceIDs::soundId).toString());
    const float duration = firstSample.getProperty (SourceIDs::duration, -1.0f);
    if (duration > 0.0f)
        details.add (juce::String (duration, 2) + "s");
    if (firstSample.getProperty (SourceIDs::license).toString().isNotEmpty())
        details.add (getShortLicenseName (firstSample.getProperty (SourceIDs::license).toString()));
    if ((bool) firstSample.getProperty (SourceIDs::usesPreview, false))
        details.add ("preview quality");
    detailsLabel.setText (details.joinIntoString ("  |  "), juce::dontSendNotification);

    // Parameter values
    for (auto* control : parameterControls)
        if (! control->isBeingEdited())
            control->setValue ((float) soundState.getProperty (control->getInfo().name, control->getInfo().defaultValue));

    {
        // Only sounds from Freesound can be replaced by similar ones
        const bool fromFreesound = (bool) firstSample.getProperty (SourceIDs::soundFromFreesound, false) && (int) firstSample.getProperty (SourceIDs::soundId, -1) > 0;
        const bool findingSimilar = soundState.getProperty (SourceIDs::findingSimilar, false);
        similarButton.setEnabled (fromFreesound && ! findingSimilar && ! (bool) soundState.getProperty (SourceIDs::willBeDeleted, false));
        similarButton.setButtonText (findingSimilar ? "Finding a similar sound..." : "Replace with similar");
    }
    if (! rootNoteSlider.isMouseButtonDown())
        rootNoteSlider.setValue ((int) firstSample.getProperty (SourceIDs::midiRootNote, SourceDefaults::midiRootNote), juce::dontSendNotification);

    // Waveform (only reload if the loaded sample changed)
    const bool loaded = soundState.getProperty (SourceIDs::allSoundsLoaded, false);
    const juce::String sampleUUID = firstSample.getProperty (SourceIDs::uuid).toString();
    if (loaded && sampleUUID != loadedWaveformForSampleUUID)
    {
        if (auto* sound = sampler.getSoundWithUUID (soundUUID))
        {
            if (auto* samplerSound = sound->getFirstLinkedSourceSamplerSound())
            {
                waveform.setAudio (samplerSound->getAudioData(), samplerSound->getSoundSampleRate());
                loadedWaveformForSampleUUID = sampleUUID;
            }
        }
    }
    if (! waveform.hasAudio())
        waveform.setMessage (loaded ? "No audio" : "Loading " + juce::String ((float) firstSample.getProperty (SourceIDs::downloadProgress, 0.0f), 0) + "%...");

    const int launchMode = soundState.getProperty (SourceIDs::launchMode, 0);
    waveform.setMarkers (soundState.getProperty (SourceIDs::startPosition, 0.0f), soundState.getProperty (SourceIDs::endPosition, 1.0f),
                         soundState.getProperty (SourceIDs::loopStartPosition, 0.0f), soundState.getProperty (SourceIDs::loopEndPosition, 1.0f),
                         launchMode == LAUNCH_MODE_LOOP || launchMode == LAUNCH_MODE_LOOP_FW_BW);

    // Slices: equal slices between start and end, or the manual slices (which can be edited)
    {
        const int numSlices = soundState.getProperty (SourceIDs::numSlices, 0);
        const int mappingMode = soundState.getProperty (SourceIDs::noteMappingMode, NOTE_MAPPING_MODE_PITCH);
        const bool slicesUsed = mappingMode == NOTE_MAPPING_MODE_SLICE || mappingMode == NOTE_MAPPING_MODE_BOTH;
        const bool manual = numSlices == SLICE_MODE_MANUAL;
        manualSliceTimes = SourceHelpers::parseSliceTimes (soundState.getProperty (SourceIDs::slices).toString());
        std::vector<float> positions;
        const double duration = waveform.getDuration();
        if (manual && duration > 0.0)
        {
            for (auto t : manualSliceTimes)
                positions.push_back ((float) (t / duration));
        }
        else if (numSlices >= 2)
        {
            const float start = soundState.getProperty (SourceIDs::startPosition, 0.0f);
            const float end = soundState.getProperty (SourceIDs::endPosition, 1.0f);
            for (int i = 1; i < numSlices; ++i)
                positions.push_back (start + (end - start) * (float) i / (float) numSlices);
        }
        waveform.setSlices (positions, manual, slicesUsed);
        const bool showButtons = manual && slicesUsed;
        addSliceButton.setVisible (showButtons);
        slicesFromOnsetsButton.setVisible (showButtons);
        addSliceButton.setEnabled (duration > 0.0 && (int) manualSliceTimes.size() < MAX_MANUAL_SLICES);
        const int numOnsets = soundState.getChildWithName (SourceIDs::SOUND_SAMPLE).getChildWithName (SourceIDs::ANALYSIS)
                                       .getChildWithName (SourceIDs::onsets).getNumChildren();
        slicesFromOnsetsButton.setEnabled (numOnsets > 0);
        slicesFromOnsetsButton.setTooltip (numOnsets > 0 ? "Replace the slices with the " + juce::String (numOnsets) + " onsets detected by Freesound"
                                                         : "No onsets data for this sound (only sounds from Freesound have it)");
    }

    modulators.updateFromState (soundUUID, soundState, sampler.state.getProperty (SourceIDs::mpeEnabled, false));
    if (modulators.getIdealHeight() != modulators.getHeight())
        layoutParameters();
}

void SoundEditorComponent::paint (juce::Graphics& g)
{
    g.setColour (SourceColours::card);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 5.0f);
}

void SoundEditorComponent::resized()
{
    auto b = getLocalBounds().reduced (10);
    titleLabel.setBounds (b.removeFromTop (24));
    detailsLabel.setBounds (b.removeFromTop (18));
    b.removeFromTop (6);
    if (soundUUID.isNotEmpty())
    {
        waveform.setBounds (b.removeFromTop (juce::jlimit (110, 200, b.getHeight() / 4)));
        b.removeFromTop (6);
        auto rootRow = b.removeFromTop (22);
        rootNoteLabel.setBounds (rootRow.removeFromLeft (96));
        rootNoteSlider.setBounds (rootRow.removeFromLeft (130));
        similarButton.setBounds (rootRow.removeFromRight (juce::jmin (rootRow.getWidth(), 170)));
        rootRow.removeFromLeft (12);
        addSliceButton.setBounds (rootRow.removeFromLeft (juce::jmin (rootRow.getWidth(), 84)));
        rootRow.removeFromLeft (4);
        slicesFromOnsetsButton.setBounds (rootRow.removeFromLeft (juce::jmin (rootRow.getWidth(), 150)));
        b.removeFromTop (6);
    }
    parametersViewport.setBounds (b);
    layoutParameters();
}

void SoundEditorComponent::layoutParameters()
{
    // Groups are laid out in columns (as many as fit), each control is a row
    const int width = parametersViewport.getWidth() - parametersViewport.getScrollBarThickness() - 4;
    const int minColumnWidth = 300;
    const int numColumns = juce::jmax (1, width / minColumnWidth);
    const int columnWidth = width / numColumns;
    const int rowHeight = 24;
    const int headerHeight = 22;

    std::vector<int> columnHeights ((size_t) numColumns, 0);
    for (auto& group : groups)
    {
        // Put each group in the shortest column
        auto column = (size_t) std::distance (columnHeights.begin(), std::min_element (columnHeights.begin(), columnHeights.end()));
        int x = (int) column * columnWidth;
        int y = columnHeights[column];
        group.header->setBounds (x, y, columnWidth - 10, headerHeight);
        y += headerHeight;
        for (auto* control : group.controls)
        {
            if (! control->isVisible())
                continue;
            control->setBounds (x, y, columnWidth - 12, rowHeight);
            y += rowHeight;
        }
        columnHeights[column] = y + 10;
    }
    int contentHeight = *std::max_element (columnHeights.begin(), columnHeights.end());
    if (modulators.isVisible())
    {
        modulators.setBounds (0, contentHeight, width - 12, modulators.getIdealHeight());
        contentHeight += modulators.getIdealHeight() + 6;
    }
    parametersContent.setSize (width, contentHeight + 10);
}
