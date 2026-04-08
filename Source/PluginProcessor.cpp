#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

VoltaResonanceProcessor::VoltaResonanceProcessor()
    : AudioProcessor(BusesProperties()
          .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
    for (int i = 0; i < DrumSynth::NUM_VOICES; ++i) {
        lastTriggerMs[i].store(0);
        pendingTrigger[i].store(false);
        remoteTrigger[i].store(false);
        remoteTriggerVel[i].store(1.0f);
        customSampleLoaded[i] = false;
    }
    for (int v = 0; v < StepSequencer::VOICES; ++v)
        for (int s = 0; s < StepSequencer::STEPS; ++s)
            stepOwnerColor[v][s] = -1;

    formatManager.registerBasicFormats();
    voltaSync.setListener(this);
}

VoltaResonanceProcessor::~VoltaResonanceProcessor()
{
    voltaSync.setListener(nullptr);
    voltaSync.disconnect();
}

//==============================================================================
void VoltaResonanceProcessor::prepareToPlay(double sampleRate, int)
{
    currentSampleRate = sampleRate;
    drumSynth.setSampleRate(sampleRate);
    stepSequencer.prepare(sampleRate, 120.0);
}

void VoltaResonanceProcessor::releaseResources() {}

bool VoltaResonanceProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    if (!layouts.getMainInputChannelSet().isDisabled())
        return false;
    return true;
}

//==============================================================================
void VoltaResonanceProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                           juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    const int numSamples = buffer.getNumSamples();
    auto nowMs = (int64_t)juce::Time::currentTimeMillis();

    // ── Apply knob DSP parameters ────────────────────────────
    {
        float decay = paramDecay.load();
        float tone  = paramTone.load();
        float drive = paramDrive.load();
        // decay: centered at 0.5 = 1x multiplier; 0.0 = 0.25x; 1.0 = 4x
        drumSynth.setDecayMult(std::pow(4.0f, 2.0f * decay - 1.0f));
        drumSynth.setToneFilter(tone);
        drumSynth.setDriveAmount(drive);
    }

    // ── Step sequencer advance ───────────────────────────────
    {
        double ppq = -1.0, bpm = 120.0;
        bool playing = false;
        if (auto* ph = getPlayHead())
        {
            if (auto pos = ph->getPosition())
            {
                if (pos->getIsPlaying())
                {
                    playing = true;
                    if (auto p = pos->getPpqPosition()) ppq = *p;
                }
                if (auto b = pos->getBpm()) bpm = *b;
            }
        }
        if (!playing) playing = internalSeqPlaying.load();

        stepSequencer.setBpm(bpm);
        uint16_t fired = stepSequencer.advance(numSamples, ppq, bpm, playing);
        seqCurrentStep.store(stepSequencer.getCurrentStep());

        for (int v = 0; v < DrumSynth::NUM_VOICES; ++v)
        {
            if (fired & (1u << v))
            {
                if (customSampleLoaded[v])
                    samplePlayers[v].trigger(1.0f);
                else
                    drumSynth.trigger(v, 1.0f);
                lastTriggerMs[v].store(nowMs);
            }
        }
    }

    // ── GUI-triggered pads ──────────────────────────────────
    for (int v = 0; v < DrumSynth::NUM_VOICES; ++v) {
        bool expected = true;
        if (pendingTrigger[v].compare_exchange_strong(expected, false)) {
            if (customSampleLoaded[v])
                samplePlayers[v].trigger(1.0f);
            else
                drumSynth.trigger(v, 1.0f);
            lastTriggerMs[v].store(nowMs);
            voltaSync.sendTrigger(v, 1.0f);
        }
    }

    // ── Remote triggers from VoltaSync ──────────────────────
    for (int v = 0; v < DrumSynth::NUM_VOICES; ++v) {
        bool expected = true;
        if (remoteTrigger[v].compare_exchange_strong(expected, false)) {
            float vel = remoteTriggerVel[v].load();
            if (customSampleLoaded[v])
                samplePlayers[v].trigger(vel);
            else
                drumSynth.trigger(v, vel);
            lastTriggerMs[v].store(nowMs);
        }
    }

    // ── Sample-by-sample render ──────────────────────────────
    auto midiIt = midiMessages.begin();
    for (int s = 0; s < numSamples; ++s)
    {
        while (midiIt != midiMessages.end()) {
            const auto meta = *midiIt;
            if (meta.samplePosition > s) break;
            const auto msg = meta.getMessage();
            if (msg.isNoteOn() && msg.getVelocity() > 0) {
                int voice = midiNoteToVoice(msg.getNoteNumber());
                if (voice >= 0) {
                    float vel = msg.getFloatVelocity();
                    if (customSampleLoaded[voice])
                        samplePlayers[voice].trigger(vel);
                    else
                        drumSynth.trigger(voice, vel);
                    lastTriggerMs[voice].store((int64_t)juce::Time::currentTimeMillis());
                    voltaSync.sendTrigger(voice, vel);
                }
            }
            ++midiIt;
        }

        float smp = drumSynth.renderSample();
        for (int v = 0; v < DrumSynth::NUM_VOICES; ++v)
            smp += samplePlayers[v].renderSample();
        smp *= masterLevel.load();

        if (buffer.getNumChannels() >= 1) buffer.getWritePointer(0)[s] = smp;
        if (buffer.getNumChannels() >= 2) buffer.getWritePointer(1)[s] = smp;
    }

    // VU meter
    float peak = 0.0f;
    if (buffer.getNumChannels() > 0) {
        auto* data = buffer.getReadPointer(0);
        for (int i = 0; i < numSamples; ++i)
            peak = std::max(peak, std::abs(data[i]));
    }
    vuLevel.store(std::max(peak, vuLevel.load() * 0.92f));
    midiMessages.clear();
}

//==============================================================================
int VoltaResonanceProcessor::midiNoteToVoice(int note) const
{
    for (int i = 0; i < DrumSynth::NUM_VOICES; ++i)
        if (DrumSynth::MIDI_NOTES[i] == note) return i;
    return -1;
}

//==============================================================================
// Sequencer helpers
void VoltaResonanceProcessor::setSequencerStep(int voice, int step, bool on)
{
    stepSequencer.setStep(voice, step, on);
}

bool VoltaResonanceProcessor::getSequencerStep(int voice, int step) const
{
    return stepSequencer.getStep(voice, step);
}

//==============================================================================
// Sample loading
void VoltaResonanceProcessor::clearSample(int voice)
{
    if (voice < 0 || voice >= DrumSynth::NUM_VOICES) return;
    samplePlayers[voice].clear();
    customSampleLoaded[voice] = false;
}

void VoltaResonanceProcessor::loadSample(int voice, const juce::File& file)
{
    if (voice < 0 || voice >= DrumSynth::NUM_VOICES) return;
    auto* reader = formatManager.createReaderFor(file);
    if (!reader) return;

    const int maxSamples = 44100 * 12;
    int numSamples = (int)std::min((juce::int64)reader->lengthInSamples, (juce::int64)maxSamples);
    juce::AudioBuffer<float> buf(1, numSamples);
    reader->read(&buf, 0, numSamples, 0, true, false);

    samplePlayers[voice].load(buf.getReadPointer(0), numSamples,
                               reader->sampleRate, currentSampleRate);
    samplePlayers[voice].name = file.getFileName().toStdString();
    customSampleLoaded[voice] = true;
    delete reader;
}

void VoltaResonanceProcessor::sendSampleSync(int voice)
{
    if (!customSampleLoaded[voice] || !voltaSync.isConnected()) return;
    auto& player = samplePlayers[voice];
    if (!player.isLoaded()) return;

    // Convert float buffer → 16-bit PCM
    const auto& fbuf = player.getBuffer();
    int numSamples = (int)fbuf.size();
    juce::MemoryBlock pcmBlock(static_cast<size_t>(numSamples) * 2);
    auto* pcm16 = static_cast<int16_t*>(pcmBlock.getData());
    for (int i = 0; i < numSamples; ++i)
        pcm16[i] = (int16_t)juce::jlimit(-32768, 32767, (int)(fbuf[(size_t)i] * 32767.0f));

    // Base64 encode
    juce::String fullB64 = juce::Base64::toBase64(pcmBlock.getData(), pcmBlock.getSize());

    // Chunk and send (40000 chars per chunk stays well under 65535-byte frame limit)
    const int CHUNK = 40000;
    int total = (fullB64.length() + CHUNK - 1) / CHUNK;
    juce::String name(player.name.c_str());
    int sr = (int)player.getStoredSr();

    for (int i = 0; i < total; ++i)
    {
        juce::String chunk = fullB64.substring(i * CHUNK, (i + 1) * CHUNK);
        voltaSync.sendSampleChunk(voice, i, total, name, sr, chunk);
    }
}

void VoltaResonanceProcessor::assembleAndLoadSample(int voice)
{
    juce::ScopedLock sl(transferLock);
    auto& t = pendingTransfers[voice];
    if (!t.complete()) return;

    // Reassemble base64
    juce::String fullB64;
    for (int i = 0; i < t.totalChunks; ++i)
        fullB64 += t.chunks[i];

    // Decode base64 → 16-bit PCM
    juce::MemoryOutputStream decoded;
    if (!juce::Base64::convertFromBase64(decoded, fullB64)) return;

    int numSamples = (int)(decoded.getDataSize() / 2);
    if (numSamples <= 0) return;
    const int16_t* pcm16 = static_cast<const int16_t*>(decoded.getData());

    std::vector<float> floatBuf((size_t)numSamples);
    for (int i = 0; i < numSamples; ++i)
        floatBuf[(size_t)i] = pcm16[i] / 32767.0f;

    samplePlayers[voice].load(floatBuf.data(), numSamples, t.sr, currentSampleRate);
    samplePlayers[voice].name = t.name.toStdString();
    customSampleLoaded[voice] = true;

    t = SampleTransfer{};   // clear
}

//==============================================================================
// VoltaSync::Listener
void VoltaResonanceProcessor::voltaSyncTrigger(int padIdx, float vel)
{
    if (padIdx < 0 || padIdx >= DrumSynth::NUM_VOICES) return;
    remoteTriggerVel[padIdx].store(vel);
    remoteTrigger[padIdx].store(true);
}

void VoltaResonanceProcessor::voltaSyncStateChanged(VoltaSync::State s)
{
    using S = VoltaSync::State;
    syncState.store(s == S::Connected ? 2 : s == S::Connecting ? 1 : 0);
}

void VoltaResonanceProcessor::voltaSyncPeersChanged(int count)
{
    syncPeerCount.store(count);
    if (count >= 2)
    {
        // Send full grid + colors
        char gridBuf[193];
        stepSequencer.serialise(gridBuf);

        char colorBuf[193];
        for (int v = 0; v < StepSequencer::VOICES; ++v)
            for (int s = 0; s < StepSequencer::STEPS; ++s) {
                int c = stepOwnerColor[v][s];
                colorBuf[v * StepSequencer::STEPS + s] =
                    (c >= 0 && c < 6) ? (char)('0' + c) : '-';
            }
        colorBuf[192] = '\0';
        voltaSync.sendGridSync(juce::String(gridBuf), juce::String(colorBuf));

        // Send any loaded samples
        for (int v = 0; v < DrumSynth::NUM_VOICES; ++v)
            if (customSampleLoaded[v])
                sendSampleSync(v);
    }
}

void VoltaResonanceProcessor::voltaSyncStepChanged(int v, int s, bool on,
                                                    int color,
                                                    const juce::String& /*user*/)
{
    stepSequencer.setStep(v, s, on);
    if (v >= 0 && v < StepSequencer::VOICES && s >= 0 && s < StepSequencer::STEPS)
        stepOwnerColor[v][s] = on ? color : -1;
}

void VoltaResonanceProcessor::voltaSyncGridSync(const juce::String& grid192,
                                                 const juce::String& colors192)
{
    stepSequencer.deserialise(grid192.toRawUTF8());
    if (colors192.length() >= 192)
    {
        for (int v = 0; v < StepSequencer::VOICES; ++v)
            for (int s = 0; s < StepSequencer::STEPS; ++s) {
                char c = colors192[v * StepSequencer::STEPS + s];
                stepOwnerColor[v][s] = (c >= '0' && c <= '5') ? (c - '0') : -1;
            }
    }
}

void VoltaResonanceProcessor::voltaSyncSampleChunk(int voice, int idx, int total,
                                                    const juce::String& name, int sr,
                                                    const juce::String& data)
{
    if (voice < 0 || voice >= DrumSynth::NUM_VOICES || idx < 0 || total <= 0) return;
    {
        juce::ScopedLock sl(transferLock);
        auto& t = pendingTransfers[voice];
        if (t.totalChunks == 0) { t.totalChunks = total; t.name = name; t.sr = sr; }
        t.chunks[idx] = data;
    }
    assembleAndLoadSample(voice);
}

//==============================================================================
juce::AudioProcessorEditor* VoltaResonanceProcessor::createEditor()
{
    return new VoltaResonanceEditor(*this);
}

void VoltaResonanceProcessor::getStateInformation(juce::MemoryBlock&) {}
void VoltaResonanceProcessor::setStateInformation(const void*, int) {}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VoltaResonanceProcessor();
}
