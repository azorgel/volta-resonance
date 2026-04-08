#pragma once
#include <cstdint>

// ============================================================
//  StepSequencer — 12-voice × 16-step grid sequencer
//  Pure C++17, no JUCE dependency.
//  Audio thread reads grid[]; GUI/network thread writes via
//  setStep(). The rare write/read race is benign (one bool).
// ============================================================

class StepSequencer
{
public:
    static constexpr int VOICES = 12;
    static constexpr int STEPS  = 16;

    bool grid[VOICES][STEPS] = {};

    void     prepare(double sampleRate, double bpm);
    void     setBpm(double bpm);

    // Returns bitmask: bit N set => voice N triggered this block.
    // ppq < 0 means no DAW position available → use internal clock.
    // transportPlaying = combined (DAW || internal) playing state.
    uint16_t advance(int numSamples, double ppq, double bpm, bool transportPlaying);

    void setStep(int voice, int step, bool on);
    bool getStep(int voice, int step) const;
    int  getCurrentStep() const;
    void resetPlayback();

    // 192 '0'/'1' chars + null terminator
    void serialise(char* out193) const;
    bool deserialise(const char* in193);

private:
    double sr_   = 44100.0;
    double bpm_  = 120.0;
    double internalAccum = 0.0;
    int    currentStep   = 0;
    int    lastPpqStep   = -1;
};
