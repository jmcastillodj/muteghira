// Logic tests for MUTEGHIRA, plus an optional UI snapshot tool:
//   MuteghiraTests                 -> run logic tests (exit code != 0 on failure)
//   MuteghiraTests --snapshot DIR  -> additionally render the editor to PNGs in DIR
#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    int failures = 0;

    void check (bool ok, const juce::String& what)
    {
        std::cout << (ok ? "  ok   " : "  FAIL ") << what << std::endl;
        if (! ok) ++failures;
    }

    struct FakeHost : juce::AudioPlayHead
    {
        bool playing = false, rec = false;
        juce::Optional<PositionInfo> getPosition() const override
        {
            PositionInfo p;
            p.setIsPlaying (playing);
            p.setIsRecording (rec);
            return p;
        }
    };

    constexpr int blockSize = 512;

    // Runs enough blocks for the fade to settle and returns the RMS of the last one.
    float run (MuteghiraProcessor& proc, float amplitude = 0.5f, int blocks = 8)
    {
        juce::AudioBuffer<float> buf (2, blockSize);
        juce::MidiBuffer midi;
        float rms = 0.0f;
        for (int b = 0; b < blocks; ++b)
        {
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < blockSize; ++i)
                    buf.setSample (ch, i, amplitude * std::sin (0.1f * (float) i));
            proc.processBlock (buf, midi);
            rms = buf.getRMSLevel (0, 0, blockSize);
        }
        return rms;
    }

    void setParam (MuteghiraProcessor& p, const char* id, bool v)
    {
        p.apvts.getParameter (id)->setValueNotifyingHost (v ? 1.0f : 0.0f);
    }

    void logicTests()
    {
        std::cout << "Logic" << std::endl;
        MuteghiraProcessor proc;
        FakeHost host;
        proc.setPlayHead (&host);
        proc.setRateAndBufferSizeDetails (48000.0, blockSize);
        proc.prepareToPlay (48000.0, blockSize);

        // Defaults = talkback: open when stopped, muted when playing / recording.
        host.playing = false; host.rec = false;
        check (run (proc) > 0.3f, "stopped: passes audio (defaults)");

        host.playing = true;
        check (run (proc) < 1.0e-6f, "playing: muted (defaults)");
        check (proc.muted.load() && proc.autoMuted.load(), "playing: UI flags report muted");

        host.rec = true;
        check (run (proc) < 1.0e-6f, "recording: muted (defaults)");

        // Push-to-talk while muted
        setParam (proc, muteghira::ids::muteOverride, true);
        check (run (proc) > 0.3f, "override while muted: passes audio");
        check (proc.muted.load() == false && proc.autoMuted.load() == true, "override: effective != transport rule");
        setParam (proc, muteghira::ids::muteOverride, false);
        check (run (proc) < 1.0e-6f, "override released: muted again");

        // Effect-tail-cutter setup: mute on Stop, pass on Play
        host.playing = false; host.rec = false;
        setParam (proc, muteghira::ids::muteStop, true);
        setParam (proc, muteghira::ids::mutePlay, false);
        check (run (proc) < 1.0e-6f, "tail cutter: stopped is muted");
        host.playing = true;
        check (run (proc) > 0.3f, "tail cutter: playing passes");

        // Push-to-mute while passing
        setParam (proc, muteghira::ids::muteOverride, true);
        check (run (proc) < 1.0e-6f, "push-to-mute while passing: muted");
        setParam (proc, muteghira::ids::muteOverride, false);

        // Rec button off -> recording follows rec flag only
        host.rec = true;
        setParam (proc, muteghira::ids::muteRec, false);
        check (run (proc) > 0.3f, "rec button off: recording passes");

        // Fade: no hard step when switching (first sample after switch is still near full level)
        {
            host.rec = false; host.playing = true;
            setParam (proc, muteghira::ids::mutePlay, false);
            run (proc);
            setParam (proc, muteghira::ids::mutePlay, true);
            juce::AudioBuffer<float> buf (2, blockSize);
            juce::MidiBuffer midi;
            for (int i = 0; i < blockSize; ++i) { buf.setSample (0, i, 0.5f); buf.setSample (1, i, 0.5f); }
            proc.processBlock (buf, midi);
            float maxStep = 0.0f;
            for (int i = 1; i < blockSize; ++i)
                maxStep = juce::jmax (maxStep, std::abs (buf.getSample (0, i) - buf.getSample (0, i - 1)));
            // The 12 ms ramp is longer than one block, so silence is reached in the next one.
            for (int i = 0; i < blockSize; ++i) { buf.setSample (0, i, 0.5f); buf.setSample (1, i, 0.5f); }
            proc.processBlock (buf, midi);
            check (maxStep < 0.01f && std::abs (buf.getSample (0, blockSize - 1)) < 1.0e-6f,
                   "mute is faded (click-free) and reaches silence");
        }

        // State round trip
        setParam (proc, muteghira::ids::muteStop, true);
        setParam (proc, muteghira::ids::muteRec, true);
        setParam (proc, muteghira::ids::muteOverride, true);
        juce::MemoryBlock mb;
        proc.getStateInformation (mb);
        MuteghiraProcessor other;
        other.setStateInformation (mb.getData(), (int) mb.getSize());
        check (other.apvts.getRawParameterValue (muteghira::ids::muteStop)->load() > 0.5f, "state restores buttons");
        check (other.apvts.getRawParameterValue (muteghira::ids::muteOverride)->load() < 0.5f, "state never restores a held override");

        // No host at all (null playhead) -> treated as stopped
        MuteghiraProcessor bare;
        bare.setRateAndBufferSizeDetails (48000.0, blockSize);
        bare.prepareToPlay (48000.0, blockSize);
        check (run (bare) > 0.3f, "no playhead: treated as stopped");

        // Layout rules
        juce::AudioProcessor::BusesLayout l;
        l.inputBuses.add (juce::AudioChannelSet::mono());
        l.outputBuses.add (juce::AudioChannelSet::mono());
        check (proc.checkBusesLayoutSupported (l), "mono in/out supported");
        l.outputBuses.set (0, juce::AudioChannelSet::stereo());
        check (! proc.checkBusesLayoutSupported (l), "mono in / stereo out rejected");
    }

    void writePng (juce::Component& c, const juce::File& file)
    {
        auto img = c.createComponentSnapshot (c.getLocalBounds(), true, 2.0f);
        file.deleteFile();
        juce::FileOutputStream out (file);
        juce::PNGImageFormat().writeImageToStream (img, out);
        std::cout << "  wrote " << file.getFullPathName() << std::endl;
    }

    void snapshots (const juce::File& dir)
    {
        std::cout << "Snapshots" << std::endl;
        dir.createDirectory();

        MuteghiraProcessor proc;
        FakeHost host;
        proc.setPlayHead (&host);
        proc.setRateAndBufferSizeDetails (48000.0, blockSize);
        proc.prepareToPlay (48000.0, blockSize);

        std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
        auto* editor = dynamic_cast<MuteghiraEditor*> (ed.get());

        // Muted (playing)
        host.playing = true;
        run (proc, 0.5f, 4);
        editor->refreshFromProcessor();
        writePng (*ed, dir.getChildFile ("muted_playing.png"));

        // Pass-thru (stopped) with a varying signal for the waveform
        host.playing = false;
        juce::Random rng (7);
        for (int i = 0; i < 60; ++i)
        {
            const float env = 0.1f + 0.9f * std::abs (std::sin ((float) i * 0.23f)) * rng.nextFloat();
            run (proc, env, 1);
            editor->refreshFromProcessor();
        }
        writePng (*ed, dir.getChildFile ("passthru_stopped.png"));

        // Push-to-talk held while playing
        host.playing = true;
        setParam (proc, muteghira::ids::muteOverride, true);
        for (int i = 0; i < 30; ++i)
        {
            run (proc, 0.2f + 0.7f * rng.nextFloat(), 1);
            editor->refreshFromProcessor();
        }
        writePng (*ed, dir.getChildFile ("push_to_talk.png"));
        setParam (proc, muteghira::ids::muteOverride, false);

        // Recording
        host.rec = true;
        run (proc, 0.5f, 4);
        editor->refreshFromProcessor();
        writePng (*ed, dir.getChildFile ("recording.png"));

        // No audio: fresh instance that never processed
        MuteghiraProcessor idle;
        std::unique_ptr<juce::AudioProcessorEditor> ed2 (idle.createEditor());
        auto* e2 = dynamic_cast<MuteghiraEditor*> (ed2.get());
        setParam (idle, muteghira::ids::mutePlay, false);
        setParam (idle, muteghira::ids::muteStop, false);
        e2->refreshFromProcessor();
        writePng (*ed2, dir.getChildFile ("no_audio.png"));
    }
}

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI init;

    logicTests();

    for (int i = 1; i + 1 < argc; ++i)
        if (juce::String (argv[i]) == "--snapshot")
            snapshots (juce::File (juce::String (argv[i + 1])));

    std::cout << (failures == 0 ? "ALL PASSED" : "FAILURES: " + juce::String (failures)) << std::endl;
    return failures == 0 ? 0 : 1;
}
