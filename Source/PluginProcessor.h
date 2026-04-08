#pragma once
#include <JuceHeader.h>
#include "DrumSynth.h"
#include "VoltaSync.h"
#include "StepSequencer.h"
#include "SamplePlayer.h"
#include <atomic>
#include <map>

class VoltaResonanceProcessor : public juce::AudioProcessor,
                                public VoltaSync::Listener
{
public:
    VoltaResonanceProcessor();
    ~VoltaResonanceProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool   acceptsMidi()  const override { return true; }
    bool   producesMidi() const override { return false; }
    bool   isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int  getNumPrograms()    override { return 1; }
    int  getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Default"; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // ── GUI ↔ Audio thread ──────────────────────────────────
    std::atomic<int64_t> lastTriggerMs[DrumSynth::NUM_VOICES];
    std::atomic<bool>    pendingTrigger[DrumSynth::NUM_VOICES];
    std::atomic<float>   masterLevel { 0.85f };
    std::atomic<float>   paramDecay  { 0.5f };   // 0..1
    std::atomic<float>   paramTone   { 1.0f };   // 0..1, 1=open
    std::atomic<float>   paramDrive  { 0.0f };   // 0..1

    // ── VoltaSync ───────────────────────────────────────────
    VoltaSync voltaSync;

    std::atomic<bool>    remoteTrigger[DrumSynth::NUM_VOICES];
    std::atomic<float>   remoteTriggerVel[DrumSynth::NUM_VOICES];

    // ── Step Sequencer ──────────────────────────────────────
    StepSequencer        stepSequencer;
    std::atomic<int>     seqCurrentStep    { 0 };
    std::atomic<bool>    internalSeqPlaying { false };

    // Per-step owner color index (-1 = no owner, 0-5 = color palette)
    int stepOwnerColor[StepSequencer::VOICES][StepSequencer::STEPS] = {};

    void setSequencerStep(int voice, int step, bool on);
    bool getSequencerStep(int voice, int step) const;
    int  getSeqCurrentStep() const { return seqCurrentStep.load(); }

    // ── Custom Samples ───────────────────────────────────────
    SamplePlayer samplePlayers[DrumSynth::NUM_VOICES];
    bool         customSampleLoaded[DrumSynth::NUM_VOICES] = {};

    void loadSample(int voice, const juce::File& file);
    void sendSampleSync(int voice);
    void clearSample(int voice);

    // Sample chunk reassembly (network thread writes, message thread reads)
    struct SampleTransfer {
        juce::String           name;
        int                    totalChunks = 0;
        int                    sr          = 44100;
        std::map<int,juce::String> chunks;
        bool complete() const { return totalChunks > 0 && (int)chunks.size() == totalChunks; }
    };
    SampleTransfer pendingTransfers[DrumSynth::NUM_VOICES];
    juce::CriticalSection transferLock;

    // ── VoltaSync Listener ──────────────────────────────────
    void voltaSyncTrigger(int padIdx, float vel) override;
    void voltaSyncStateChanged(VoltaSync::State s) override;
    void voltaSyncPeersChanged(int count) override;
    void voltaSyncStepChanged(int v, int s, bool on, int color,
                              const juce::String& user) override;
    void voltaSyncGridSync(const juce::String& grid192,
                           const juce::String& colors192) override;
    void voltaSyncSampleChunk(int voice, int idx, int total,
                              const juce::String& name, int sr,
                              const juce::String& data) override;

    // ── Sync state (read by GUI) ─────────────────────────────
    std::atomic<int>  syncPeerCount { 0 };
    std::atomic<int>  syncState     { 0 };  // 0=off 1=connecting 2=connected
    std::atomic<float> vuLevel      { 0.0f };

    // Current sample rate (set in prepareToPlay, read by loadSample)
    double currentSampleRate = 44100.0;

    // Format manager for loading audio files
    juce::AudioFormatManager formatManager;

private:
    DrumSynth drumSynth;
    int midiNoteToVoice(int note) const;
    void assembleAndLoadSample(int voice);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VoltaResonanceProcessor)
};
