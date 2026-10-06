#include "PluginEditor.h"

using namespace muteghira;

namespace
{
    float textWidth (const juce::Font& f, const juce::String& t)
    {
        juce::GlyphArrangement ga;
        ga.addLineOfText (f, t, 0.0f, 0.0f);
        return ga.getBoundingBox (0, -1, true).getWidth();
    }

    juce::Font makeFont (float height, bool bold, float kerning = 0.0f)
    {
        return juce::Font (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain))
                   .withExtraKerningFactor (kerning);
    }
}

//==============================================================================
// StateCircle
//==============================================================================
void StateCircle::setView (bool isMuted, bool isNoAudio, bool isHeld)
{
    if (muted != isMuted || noAudio != isNoAudio || held != isHeld)
    {
        muted = isMuted;
        noAudio = isNoAudio;
        held = isHeld;
    }
    repaint();
}

juce::Rectangle<float> StateCircle::circleBounds() const
{
    const auto b = getLocalBounds().toFloat();
    const float d = juce::jmin (b.getWidth(), b.getHeight()) * 0.76f;
    return juce::Rectangle<float> (d, d).withCentre (b.getCentre());
}

bool StateCircle::hitTest (int x, int y)
{
    const auto c = circleBounds();
    return c.getCentre().getDistanceFrom ({ (float) x, (float) y }) <= c.getWidth() * 0.5f;
}

void StateCircle::mouseDown (const juce::MouseEvent&)
{
    param.beginChangeGesture();
    param.setValueNotifyingHost (1.0f);
}

void StateCircle::mouseUp (const juce::MouseEvent&)
{
    param.setValueNotifyingHost (0.0f);
    param.endChangeGesture();
}

void StateCircle::paint (juce::Graphics& g)
{
    using namespace colours;

    const auto c = circleBounds();
    const float r = c.getWidth() * 0.5f;
    const float ring = juce::jmax (3.0f, r * 0.07f);
    const auto ringRect = c.reduced (ring * 0.5f);

    // Disc
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff222327), c.getCentreX(), c.getY(),
                                             juce::Colour (0xff131416), c.getCentreX(), c.getBottom(), false));
    g.fillEllipse (c);

    if (muted)
    {
        g.setColour (accent);
        g.drawEllipse (ringRect, ring);

        // The classic "no" slash.
        const auto ctr = c.getCentre();
        const float k = r * 0.52f;
        g.setColour (accent);
        g.drawLine (ctr.x - k, ctr.y - k, ctr.x + k, ctr.y + k, ring * 1.15f);

        g.setColour (accent.withAlpha (0.9f));
        g.setFont (makeFont (r * 0.12f, true, 0.2f));
        g.drawText ("MUTED", c.withTrimmedTop (r * 1.45f).withHeight (r * 0.3f), juce::Justification::centred);
        return;
    }

    // Soft glow around the white ring.
    for (int i = 6; i >= 1; --i)
    {
        g.setColour (ink.withAlpha (0.030f * (float) (7 - i)));
        g.drawEllipse (ringRect.expanded ((float) i * r * 0.018f), ring + (float) i * r * 0.03f);
    }
    g.setColour (ink);
    g.drawEllipse (ringRect, ring);

    if (noAudio)
    {
        g.setColour (textMid);
        g.setFont (makeFont (r * 0.15f, true, 0.18f));
        g.drawFittedText ("NO AUDIO\nINPUT", c.reduced (r * 0.3f).toNearestInt(), juce::Justification::centred, 2);
        return;
    }

    // Live waveform, clipped to the inside of the ring.
    const auto inner = c.reduced (ring * 2.2f);
    juce::Path clip;
    clip.addEllipse (inner);
    g.saveState();
    g.reduceClipRegion (clip);

    const float slot = inner.getWidth() / (float) numBars;
    const float barW = juce::jmax (1.5f, slot * 0.42f);
    const float maxH = inner.getHeight() * 0.86f;

    g.setColour (ink);
    for (int i = 0; i < numBars; ++i)
    {
        const float h = juce::jmax (barW, history[(size_t) i] * maxH);
        const float x = inner.getX() + ((float) i + 0.5f) * slot - barW * 0.5f;
        g.fillRoundedRectangle (x, inner.getCentreY() - h * 0.5f, barW, h, barW * 0.5f);
    }
    g.restoreState();
}

//==============================================================================
// TransportButton
//==============================================================================
TransportButton::TransportButton (Icon i, const juce::String& caption)
    : juce::Button (caption), icon (i), text (caption)
{
    setClickingTogglesState (true);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void TransportButton::setTransportActive (bool a)
{
    if (transportActive != a)
    {
        transportActive = a;
        repaint();
    }
}

void TransportButton::paintButton (juce::Graphics& g, bool isOver, bool isDown)
{
    using namespace colours;

    const auto b = getLocalBounds().toFloat();
    const float d = juce::jmin (b.getWidth(), b.getHeight() * 0.76f);
    const auto disc = juce::Rectangle<float> (d, d).withCentre ({ b.getCentreX(), b.getY() + d * 0.5f + 1.0f });
    const bool on = getToggleState();

    const auto fill = on ? (isOver ? accentHi : accent) : juce::Colours::transparentBlack;
    const auto fg = on ? juce::Colour (0xff17181a) : (isOver ? ink : textMid);

    if (on)
    {
        g.setColour (fill);
        g.fillEllipse (disc);
    }
    else
    {
        g.setColour (isOver ? juce::Colour (0xff26272b) : juce::Colours::transparentBlack);
        g.fillEllipse (disc);
        g.setColour (isOver ? accent : line.brighter (0.25f));
        g.drawEllipse (disc.reduced (1.0f), juce::jmax (1.5f, d * 0.03f));
    }

    if (isDown)
    {
        g.setColour (juce::Colours::black.withAlpha (0.25f));
        g.fillEllipse (disc);
    }

    // Icon
    const auto ic = disc.withSizeKeepingCentre (d * 0.36f, d * 0.36f);
    g.setColour (fg);
    switch (icon)
    {
        case Icon::stop:
            g.fillRoundedRectangle (ic, ic.getWidth() * 0.12f);
            break;
        case Icon::play:
        {
            juce::Path p;
            p.addTriangle (ic.getX() + ic.getWidth() * 0.08f, ic.getY(),
                           ic.getX() + ic.getWidth() * 0.08f, ic.getBottom(),
                           ic.getRight() + ic.getWidth() * 0.05f, ic.getCentreY());
            g.fillPath (p.createPathWithRoundedCorners (ic.getWidth() * 0.08f));
            break;
        }
        case Icon::rec:
            g.fillEllipse (ic);
            break;
    }

    // Strike-through: a gap in the background colour, then the slash itself.
    const auto bgForGap = on ? fill : juce::Colour (0xff1a1b1e);
    const float e = ic.getWidth() * 0.78f;
    const auto ctr = ic.getCentre();
    g.setColour (bgForGap);
    g.drawLine (ctr.x - e, ctr.y - e, ctr.x + e, ctr.y + e, d * 0.075f);
    g.setColour (fg);
    g.drawLine (ctr.x - e, ctr.y - e, ctr.x + e, ctr.y + e, juce::jmax (1.5f, d * 0.03f));

    // Caption (brighter while the host transport is in this state)
    g.setColour (transportActive ? ink : textDim);
    g.setFont (makeFont (juce::jmax (9.0f, d * 0.17f), true, 0.18f));
    g.drawText (text, getLocalBounds().withTop ((int) (disc.getBottom() + 4.0f)), juce::Justification::centredTop);

    if (transportActive)
    {
        g.setColour (accent);
        const float dot = juce::jmax (3.0f, d * 0.06f);
        g.fillEllipse (juce::Rectangle<float> (dot, dot).withCentre ({ b.getCentreX(), b.getBottom() - dot }));
    }
}

//==============================================================================
// Editor
//==============================================================================
MuteghiraEditor::MuteghiraEditor (MuteghiraProcessor& p)
    : AudioProcessorEditor (&p),
      processor (p),
      circle (*p.apvts.getParameter (ids::muteOverride)),
      stopAttach (p.apvts, ids::muteStop, stopButton),
      playAttach (p.apvts, ids::mutePlay, playButton),
      recAttach  (p.apvts, ids::muteRec,  recButton)
{
    stopButton.setTooltip ("Filled: mute this channel while the transport is stopped");
    playButton.setTooltip ("Filled: mute this channel while the transport is playing");
    recButton.setTooltip  ("Filled: mute this channel while the transport is recording\n"
                           "(only works in hosts that report the record state)");
    circle.setTooltip ("Click and hold to temporarily flip the state (push-to-talk / push-to-mute)");

    addAndMakeVisible (circle);
    addAndMakeVisible (stopButton);
    addAndMakeVisible (playButton);
    addAndMakeVisible (recButton);

    setResizeLimits (baseWidth * 3 / 4, baseHeight * 3 / 4, baseWidth * 2, baseHeight * 2);
    getConstrainer()->setFixedAspectRatio ((double) baseWidth / (double) baseHeight);
    setResizable (true, true);
    setSize (baseWidth, baseHeight);

    refreshFromProcessor();
    startTimerHz (30);
}

MuteghiraEditor::~MuteghiraEditor()
{
    stopTimer();
}

void MuteghiraEditor::refreshFromProcessor()
{
    const bool autoMuted = processor.autoMuted.load();
    const bool held = processor.apvts.getRawParameterValue (ids::muteOverride)->load() >= 0.5f;
    const bool muted = processor.hasProcessed.load() ? processor.muted.load() : (autoMuted != held);

    const auto age = juce::Time::getMillisecondCounter() - processor.lastBlockMs.load();
    const bool noAudio = ! processor.hasProcessed.load() || age > 500;

    // Envelope history for the waveform (about 1.5 s on screen).
    const float peak = processor.inputPeak.exchange (0.0f);
    const float target = std::sqrt (juce::jlimit (0.0f, 1.0f, peak));
    const float last = history.back();
    const float next = target > last ? target : last * 0.7f + target * 0.3f;
    std::rotate (history.begin(), history.begin() + 1, history.end());
    history.back() = next;

    circle.setHistory (history);
    circle.setView (muted, noAudio, held);

    const auto t = processor.transportState.load();
    stopButton.setTransportActive (t == MuteghiraProcessor::stopped);
    playButton.setTransportActive (t == MuteghiraProcessor::playing);
    recButton.setTransportActive  (t == MuteghiraProcessor::recording);

    juce::String newHint;
    if (held)  newHint = autoMuted ? "PUSH TO TALK" : "PUSH TO MUTE";
    else       newHint = autoMuted ? "CLICK & HOLD TO TALK" : "CLICK & HOLD TO MUTE";

    if (newHint != hint)
    {
        hint = newHint;
        repaint (hintArea);
    }
}

void MuteghiraEditor::resized()
{
    const float s = (float) getWidth() / (float) baseWidth;
    auto S = [s] (float v) { return juce::roundToInt (v * s); };

    circle.setBounds (S (10), S (34), S (300), S (300));
    hintArea = { 0, S (336), getWidth(), S (22) };

    const int bw = S (76), gap = S (28), by = S (366), bh = S (84);
    const int x0 = (getWidth() - (3 * bw + 2 * gap)) / 2;
    stopButton.setBounds (x0, by, bw, bh);
    playButton.setBounds (x0 + bw + gap, by, bw, bh);
    recButton.setBounds  (x0 + 2 * (bw + gap), by, bw, bh);
}

void MuteghiraEditor::paint (juce::Graphics& g)
{
    using namespace colours;

    const float s = (float) getWidth() / (float) baseWidth;
    const auto b = getLocalBounds().toFloat();

    g.setGradientFill (juce::ColourGradient (bgTop, 0.0f, 0.0f, bgBottom, 0.0f, b.getBottom(), false));
    g.fillAll();

    // Header
    g.setColour (accent);
    g.fillRect (0.0f, 0.0f, b.getWidth(), 3.0f * s);
    g.setColour (textDim);
    g.setFont (makeFont (10.5f * s, true, 0.22f));
    g.drawText ("TALKBACK AUTO-MUTE", juce::Rectangle<float> (20.0f * s, 14.0f * s, 200.0f * s, 16.0f * s),
                juce::Justification::centredLeft);
    g.drawText ("VST3", juce::Rectangle<float> (b.getWidth() - 70.0f * s, 14.0f * s, 50.0f * s, 16.0f * s),
                juce::Justification::centredRight);

    // Hint under the circle
    g.setColour (hint.startsWith ("PUSH") ? accentHi : textDim);
    g.setFont (makeFont (10.0f * s, true, 0.2f));
    g.drawText (hint, hintArea, juce::Justification::centred);

    // Footer
    const auto footer = juce::Rectangle<float> (0.0f, b.getBottom() - 42.0f * s, b.getWidth(), 42.0f * s);
    g.setColour (panel);
    g.fillRect (footer);
    g.setColour (line);
    g.fillRect (footer.withHeight (1.0f));

    g.setFont (makeFont (17.0f * s, true, 0.16f));
    const juce::String a = "MUTE", z = "GHIRA";
    const float wa = textWidth (makeFont (17.0f * s, true, 0.16f), a);
    const float wz = textWidth (makeFont (17.0f * s, true, 0.16f), z);
    const float x = footer.getCentreX() - (wa + wz) * 0.5f;
    g.setColour (ink);
    g.drawText (a, juce::Rectangle<float> (x, footer.getY(), wa + 2.0f, footer.getHeight()), juce::Justification::centredLeft);
    g.setColour (accent);
    g.drawText (z, juce::Rectangle<float> (x + wa, footer.getY(), wz + 2.0f, footer.getHeight()), juce::Justification::centredLeft);
}
