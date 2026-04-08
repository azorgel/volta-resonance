#pragma once
#include <cmath>
#include <random>
#include <cstdint>
#include <array>

// ============================================================
//  Primitive DSP building blocks — no JUCE dependency
// ============================================================

struct ExpEnv {
    double value = 0.0;
    double target = 0.0;
    double coeff  = 1.0;   // per-sample multiplier

    // target0 → target1 over durationSec samples
    void set(double startVal, double endVal, double durationSec, double sr) {
        value  = startVal;
        target = endVal;
        // exponential decay: x[n] = x[n-1] * coeff + (1-coeff)*target
        // simplified: use coeff = exp(log(end/start) / N)
        double N = durationSec * sr;
        if (N < 1.0) N = 1.0;
        coeff = std::pow(endVal / startVal, 1.0 / N);
    }
    double tick() {
        value *= coeff;
        return value;
    }
};

struct FreqEnv {
    double freq   = 440.0;
    double target = 440.0;
    double coeff  = 1.0;

    void set(double startHz, double endHz, double durationSec, double sr) {
        freq   = startHz;
        target = endHz;
        double N = durationSec * sr;
        if (N < 1.0) N = 1.0;
        coeff = std::pow(endHz / startHz, 1.0 / N);
    }
    double tick() {
        freq *= coeff;
        if ((coeff < 1.0 && freq < target) || (coeff > 1.0 && freq > target))
            freq = target;
        return freq;
    }
};

struct Oscillator {
    enum class Shape { Sine, Square, Sawtooth, Triangle };
    Shape  shape = Shape::Sine;
    double phase = 0.0;

    void reset() { phase = 0.0; }

    float tick(double freq, double sr) {
        double inc = freq / sr;
        phase += inc;
        if (phase >= 1.0) phase -= 1.0;
        switch (shape) {
            case Shape::Sine:
                return static_cast<float>(std::sin(phase * 2.0 * M_PI));
            case Shape::Square:
                return phase < 0.5f ? 1.0f : -1.0f;
            case Shape::Sawtooth:
                return static_cast<float>(2.0 * phase - 1.0);
            case Shape::Triangle:
                return static_cast<float>(phase < 0.5 ? 4.0 * phase - 1.0
                                                       : 3.0 - 4.0 * phase);
            default: return 0.0f;
        }
    }
};

struct NoiseGen {
    std::mt19937 rng;
    std::uniform_real_distribution<float> dist { -1.0f, 1.0f };

    NoiseGen() : rng(std::random_device{}()) {}
    float tick() { return dist(rng); }
};

// Two-pole biquad filter (bilinear transform)
struct Biquad {
    enum class Type { HighPass, BandPass, LowPass };
    Type   type = Type::HighPass;
    double b0=1, b1=0, b2=0, a1=0, a2=0;
    double x1=0, x2=0, y1=0, y2=0;

    void setHP(double cutoffHz, double q, double sr) {
        type = Type::HighPass;
        double w0 = 2.0 * M_PI * cutoffHz / sr;
        double alpha = std::sin(w0) / (2.0 * q);
        double cosW = std::cos(w0);
        double norm  = 1.0 + alpha;
        b0 =  (1.0 + cosW) / 2.0 / norm;
        b1 = -(1.0 + cosW) / norm;
        b2 =  (1.0 + cosW) / 2.0 / norm;
        a1 = (-2.0 * cosW) / norm;
        a2 =  (1.0 - alpha) / norm;
    }

    void setBP(double cutoffHz, double q, double sr) {
        type = Type::BandPass;
        double w0 = 2.0 * M_PI * cutoffHz / sr;
        double alpha = std::sin(w0) / (2.0 * q);
        double cosW = std::cos(w0);
        double norm  = 1.0 + alpha;
        b0 =  (std::sin(w0) / 2.0) / norm;
        b1 =  0.0;
        b2 = -(std::sin(w0) / 2.0) / norm;
        a1 = (-2.0 * cosW) / norm;
        a2 =  (1.0 - alpha) / norm;
    }

    void setLP(double cutoffHz, double q, double sr) {
        type = Type::LowPass;
        double w0 = 2.0 * M_PI * cutoffHz / sr;
        double alpha = std::sin(w0) / (2.0 * q);
        double cosW = std::cos(w0);
        double norm  = 1.0 + alpha;
        b0 = (1.0 - cosW) / 2.0 / norm;
        b1 = (1.0 - cosW) / norm;
        b2 = (1.0 - cosW) / 2.0 / norm;
        a1 = (-2.0 * cosW) / norm;
        a2 =  (1.0 - alpha) / norm;
    }

    void reset() { x1=x2=y1=y2=0.0; }

    float tick(float in) {
        double x0 = in;
        double y0 = b0*x0 + b1*x1 + b2*x2 - a1*y1 - a2*y2;
        x2=x1; x1=x0; y2=y1; y1=y0;
        return static_cast<float>(y0);
    }
};

// ============================================================
//  DrumSynth — 12 voice drum synthesiser
// ============================================================
class DrumSynth {
public:
    static constexpr int NUM_VOICES = 12;

    // MIDI note mapping (GM Ch.10)
    enum VoiceIndex {
        KICK=0, SNARE, CLAP, RIM,
        HH_CL, HH_OP, RIDE, CRASH,
        TOM_H, TOM_M, TOM_L, PERC
    };
    static constexpr int MIDI_NOTES[NUM_VOICES] = {
        36, 38, 39, 37,
        42, 46, 51, 49,
        50, 48, 45, 56
    };

    void setSampleRate(double sr) {
        sampleRate = sr;
        toneFilter_.setLP(22000.0, 0.7, sr);  // default: fully open
    }
    void trigger(int voiceIdx, float velocity);
    float renderSample();

    // DSP parameter controls (called from audio thread)
    void setDecayMult(float m)   { decayMult_   = std::max(0.05f, m); }
    void setDriveAmount(float d) { driveAmount_ = std::max(0.0f, std::min(1.0f, d)); }
    void setToneFilter(float t)  {
        // t=0: 200Hz LP, t=1: 22kHz (transparent). log scale.
        double cutoff = 200.0 * std::pow(110.0, static_cast<double>(t));
        toneFilter_.setLP(std::min(cutoff, 22000.0), 0.7, sampleRate);
    }

private:
    double sampleRate = 44100.0;
    float  decayMult_   = 1.0f;
    float  driveAmount_ = 0.0f;
    Biquad toneFilter_;

    // ---- Voice structs ----------------------------------------

    struct KickVoice {
        bool    active = false;
        ExpEnv  gainEnv;
        FreqEnv freqEnv;
        Oscillator osc;
        // click transient
        Oscillator clickOsc;
        ExpEnv  clickEnv;
        float   vel = 1.0f;

        void trigger(float v, double sr, float dm = 1.0f);
        float tick(double sr);
    } kick;

    struct SnareVoice {
        bool   active = false;
        // BP noise body
        NoiseGen noise;
        Biquad   bp;
        ExpEnv   noiseEnv;
        // tonal component
        Oscillator toneOsc;
        FreqEnv    toneFreq;
        ExpEnv     toneEnv;
        float      vel = 1.0f;

        void trigger(float v, double sr, float dm = 1.0f);
        float tick(double sr);
    } snare;

    struct ClapVoice {
        bool    active   = false;
        static constexpr int NUM_BURSTS = 4;
        static constexpr double BURST_TIMES[4] = { 0.0, 0.012, 0.024, 0.038 };
        NoiseGen noise;
        Biquad   bp[4];
        ExpEnv   burstEnv[4];
        int      burstSamp[4];    // sample counter to start each burst
        float    vel = 1.0f;
        int      samplePos = 0;

        void trigger(float v, double sr, float dm = 1.0f);
        float tick(double sr);
    } clap;

    struct RimVoice {
        bool    active = false;
        Oscillator osc;
        FreqEnv    freqEnv;
        ExpEnv     oscEnv;
        NoiseGen   noise;
        ExpEnv     noiseEnv;
        float      vel = 1.0f;

        void trigger(float v, double sr, float dm = 1.0f);
        float tick(double sr);
    } rim;

    struct HHClosedVoice {
        bool     active = false;
        NoiseGen noise;
        Biquad   hp;
        ExpEnv   gainEnv;
        float    vel = 1.0f;

        void trigger(float v, double sr, float dm = 1.0f);
        float tick(double sr);
    } hhClosed;

    struct HHOpenVoice {
        bool     active = false;
        NoiseGen noise;
        Biquad   hp;
        ExpEnv   gainEnv;
        float    vel = 1.0f;

        void trigger(float v, double sr, float dm = 1.0f);
        float tick(double sr);
    } hhOpen;

    struct RideVoice {
        bool   active = false;
        static constexpr int NUM_PARTIALS = 4;
        static constexpr double PARTIAL_FREQS[4] = { 540.0, 872.0, 1380.0, 2200.0 };
        Oscillator osc[4];
        ExpEnv     gainEnv;
        float      vel = 1.0f;

        void trigger(float v, double sr, float dm = 1.0f);
        float tick(double sr);
    } ride;

    struct CrashVoice {
        bool    active = false;
        NoiseGen noise;
        Biquad   hp;
        ExpEnv   noiseEnv;
        static constexpr int NUM_SAWS = 3;
        static constexpr double SAW_FREQS[3] = { 320.0, 560.0, 920.0 };
        Oscillator sawOsc[3];
        ExpEnv     sawEnv;
        float      vel = 1.0f;

        void trigger(float v, double sr, float dm = 1.0f);
        float tick(double sr);
    } crash;

    struct TomVoice {
        bool    active = false;
        Oscillator osc;
        FreqEnv    freqEnv;
        ExpEnv     toneEnv;
        NoiseGen   noise;
        ExpEnv     noiseEnv;
        float      vel = 1.0f;
        // configuration set per-instance
        double startHz = 260.0, endHz = 160.0;
        double toneDur = 0.4, noiseDur = 0.06;

        void setConfig(double sHz, double eHz, double tDur, double nDur);
        void trigger(float v, double sr, float dm = 1.0f);
        float tick(double sr);
    } tomH, tomM, tomL;

    struct PercVoice {
        bool   active = false;
        static constexpr int NUM_OSCS = 3;
        static constexpr double PARTIAL_FREQS[3]   = { 880.0, 1320.0, 2200.0 };
        static constexpr double PARTIAL_OFFSETS[3] = { 0.0, 0.013, 0.027 };
        Oscillator osc[3];
        ExpEnv     gainEnv;
        float      vel = 1.0f;

        void trigger(float v, double sr, float dm = 1.0f);
        float tick(double sr);
    } perc;
};
