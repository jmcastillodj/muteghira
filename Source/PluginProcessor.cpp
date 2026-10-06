#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace muteghira;

MuteghiraProcessor::MuteghiraProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "MUTEGHIRA", createLayout())
{
    pMuteStop = apvts.getRawParameterValue (ids::muteStop);
    pMutePlay = apvts.getRawParameterValue (ids::mutePlay);
    pMuteRec  = apvts.getRawParameterValue (ids::muteRec);
    pOverride = apvts.getRawParameterValue (ids::muteOverride);
}

juce::AudioProcessorValueTreeState::ParameterLayout MuteghiraProcessor::createLayout()
{
    using juce::AudioParameterBool;
    using juce::ParameterID;

    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    // Defaults match the classic talkback setup: open while stopped, muted while playing/recording.
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ids::muteStop, 1 }, "Mute When Stopped",   false));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ids::mutePlay, 1 }, "Mute When Playing",   true));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ids::muteRec,  1 }, "Mute When Recording", true));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ids::muteOverride, 1 }, "Mute Override",   false));
    return layout;
}

void MuteghiraProcessor::prepareToPlay (double sampleRate, int)
{
    gain.reset (sampleRate, 0.012); // 12 ms fade: no clicks when muting/unmuting
    gain.setCurrentAndTargetValue (1.0f);
}

bool MuteghiraProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return ! out.isDisabled() && out == layouts.getMainInputChannelSet();
}

MuteghiraProcessor::Transport MuteghiraProcessor::readTransport() const
{
    if (auto* head = getPlayHead())
        if (auto pos = head->getPosition())
        {
            if (pos->getIsRecording()) return recording;
            if (pos->getIsPlaying())   return playing;
        }

    return stopped;
}

void MuteghiraProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int numIn      = getTotalNumInputChannels();

    for (int ch = numIn; ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);

    const auto transport = readTransport();
    const bool byTransport = shouldMute (transport,
                                         pMuteStop->load() >= 0.5f,
                                         pMutePlay->load() >= 0.5f,
                                         pMuteRec->load()  >= 0.5f);
    const bool override_ = pOverride->load() >= 0.5f;
    const bool mute      = byTransport != override_;

    // Level meter feed: input peak *before* muting, so the waveform shows the channel itself.
    float peak = 0.0f;
    for (int ch = 0; ch < numIn; ++ch)
        peak = juce::jmax (peak, buffer.getMagnitude (ch, 0, numSamples));

    float seen = inputPeak.load (std::memory_order_relaxed);
    while (peak > seen && ! inputPeak.compare_exchange_weak (seen, peak)) {}

    gain.setTargetValue (mute ? 0.0f : 1.0f);

    if (gain.isSmoothing())
    {
        const int numCh = buffer.getNumChannels();
        for (int i = 0; i < numSamples; ++i)
        {
            const float g = gain.getNextValue();
            for (int ch = 0; ch < numCh; ++ch)
                buffer.getWritePointer (ch)[i] *= g;
        }
    }
    else if (mute)
    {
        buffer.clear();
    }

    transportState.store (transport);
    autoMuted.store (byTransport);
    muted.store (mute);
    lastBlockMs.store (juce::Time::getMillisecondCounter());
    hasProcessed.store (true);
}

juce::AudioProcessorEditor* MuteghiraProcessor::createEditor()
{
    return new MuteghiraEditor (*this);
}

void MuteghiraProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void MuteghiraProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));

    // The override is momentary: never restore it as "held".
    if (auto* p = apvts.getParameter (ids::muteOverride))
        p->setValueNotifyingHost (0.0f);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MuteghiraProcessor();
}
