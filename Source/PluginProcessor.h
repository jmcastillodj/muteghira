#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace muteghira::ids
{
    inline constexpr const char* muteStop     = "muteStop";
    inline constexpr const char* mutePlay     = "mutePlay";
    inline constexpr const char* muteRec      = "muteRec";
    inline constexpr const char* muteOverride = "muteOverride";
}

/** MUTEGHIRA: mutes (or passes) its channel depending on the host transport.

    Each of the three buttons (Stop / Play / Rec) says "mute the channel while
    the transport is in this state". The "override" parameter inverts whatever
    the transport rule decided (push-to-talk / push-to-mute).
*/
class MuteghiraProcessor : public juce::AudioProcessor
{
public:
    enum Transport { stopped = 0, playing = 1, recording = 2 };

    MuteghiraProcessor();
    ~MuteghiraProcessor() override = default;

    //==========================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;

    //==========================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "MUTEGHIRA"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    //==========================================================================
    /** True if the transport rule (ignoring the override) says "mute". */
    static bool shouldMute (Transport t, bool muteOnStop, bool muteOnPlay, bool muteOnRec) noexcept
    {
        if (t == recording) return muteOnRec;
        if (t == playing)   return muteOnPlay;
        return muteOnStop;
    }

    juce::AudioProcessorValueTreeState apvts;

    // Read by the editor (message thread), written by the audio thread.
    std::atomic<bool>          muted { false };      // effective state, override included
    std::atomic<bool>          autoMuted { false };  // transport rule only
    std::atomic<int>           transportState { stopped };
    std::atomic<float>         inputPeak { 0.0f };   // max input peak since last read
    std::atomic<juce::uint32>  lastBlockMs { 0 };
    std::atomic<bool>          hasProcessed { false };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    Transport readTransport() const;

    std::atomic<float>* pMuteStop = nullptr;
    std::atomic<float>* pMutePlay = nullptr;
    std::atomic<float>* pMuteRec = nullptr;
    std::atomic<float>* pOverride = nullptr;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> gain { 1.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MuteghiraProcessor)
};
