#pragma once

#include "PluginProcessor.h"

namespace muteghira
{
    namespace colours
    {
        const juce::Colour bgTop     { 0xff2b2c30 };
        const juce::Colour bgBottom  { 0xff151618 };
        const juce::Colour panel     { 0xff1c1d20 };
        const juce::Colour line      { 0xff3a3b40 };
        const juce::Colour accent    { 0xffe63b2e }; // Fender-style red: "muted"
        const juce::Colour accentHi  { 0xffff5a4a };
        const juce::Colour ink       { 0xfff2f4f7 }; // "pass-thru" white
        const juce::Colour textDim   { 0xff8a8c94 };
        const juce::Colour textMid   { 0xffb6b8bf };
    }

    /** The big circle: static red "muted" toggle, or white ring + live waveform. */
    class StateCircle : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        static constexpr int numBars = 44;

        explicit StateCircle (juce::RangedAudioParameter& overrideParam) : param (overrideParam) {}

        void setView (bool isMuted, bool isNoAudio, bool isHeld);
        void setHistory (const std::array<float, numBars>& h) { history = h; }

        void paint (juce::Graphics&) override;
        bool hitTest (int x, int y) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;

    private:
        juce::Rectangle<float> circleBounds() const;

        juce::RangedAudioParameter& param;
        std::array<float, numBars> history {};
        bool muted = false, noAudio = false, held = false;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StateCircle)
    };

    /** Round toggle with a struck-through stop / play / record icon. */
    class TransportButton : public juce::Button
    {
    public:
        enum class Icon { stop, play, rec };

        TransportButton (Icon i, const juce::String& caption);

        void setTransportActive (bool a);
        void paintButton (juce::Graphics&, bool isOver, bool isDown) override;

    private:
        Icon icon;
        juce::String text;
        bool transportActive = false;
    };
}

class MuteghiraEditor : public juce::AudioProcessorEditor,
                        private juce::Timer
{
public:
    explicit MuteghiraEditor (MuteghiraProcessor&);
    ~MuteghiraEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    /** Pulls the latest state from the processor. Called by the timer; public so tests can drive it. */
    void refreshFromProcessor();

    static constexpr int baseWidth = 320, baseHeight = 500;

private:
    void timerCallback() override { refreshFromProcessor(); }

    MuteghiraProcessor& processor;

    muteghira::StateCircle circle;
    muteghira::TransportButton stopButton { muteghira::TransportButton::Icon::stop, "STOP" };
    muteghira::TransportButton playButton { muteghira::TransportButton::Icon::play, "PLAY" };
    muteghira::TransportButton recButton  { muteghira::TransportButton::Icon::rec,  "REC" };

    juce::AudioProcessorValueTreeState::ButtonAttachment stopAttach, playAttach, recAttach;
    juce::TooltipWindow tooltips { this, 600 };

    std::array<float, muteghira::StateCircle::numBars> history {};
    juce::String hint;
    juce::Rectangle<int> hintArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MuteghiraEditor)
};
