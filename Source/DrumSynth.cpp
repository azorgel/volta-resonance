#include "DrumSynth.h"
#include <algorithm>

// ================================================================
//  KickVoice
// ================================================================
void DrumSynth::KickVoice::trigger(float v, double sr, float dm) {
    vel = v;
    osc.reset();
    osc.shape = Oscillator::Shape::Sine;
    freqEnv.set(160.0, 38.0, 0.45 * dm, sr);
    gainEnv.set(1.0, 0.001, 0.55 * dm, sr);
    // click transient
    clickOsc.reset();
    clickOsc.shape = Oscillator::Shape::Sine;
    clickEnv.set(1.0, 0.001, 0.012, sr);
    active = true;
}

float DrumSynth::KickVoice::tick(double sr) {
    if (!active) return 0.0f;

    double freq  = freqEnv.tick();
    double gain  = gainEnv.tick();
    float  body  = osc.tick(freq, sr) * static_cast<float>(gain);

    double cGain = clickEnv.tick();
    float  click = clickOsc.tick(3000.0, sr) * static_cast<float>(cGain);

    float out = (body + click * 0.3f) * vel;

    if (gain < 0.0015 && cGain < 0.0015)
        active = false;

    return out;
}

// ================================================================
//  SnareVoice
// ================================================================
void DrumSynth::SnareVoice::trigger(float v, double sr, float dm) {
    vel = v;
    bp.setBP(1800.0, 0.6, sr);
    bp.reset();
    noiseEnv.set(1.0, 0.001, 0.28 * dm, sr);

    toneOsc.reset();
    toneOsc.shape = Oscillator::Shape::Sine;
    toneFreq.set(280.0, 180.0, 0.1 * dm, sr);
    toneEnv.set(0.5, 0.001, 0.1 * dm, sr);

    active = true;
}

float DrumSynth::SnareVoice::tick(double sr) {
    if (!active) return 0.0f;

    double nGain = noiseEnv.tick();
    float  noisy = bp.tick(noise.tick()) * static_cast<float>(nGain);

    double tFreq = toneFreq.tick();
    double tGain = toneEnv.tick();
    float  tone  = toneOsc.tick(tFreq, sr) * static_cast<float>(tGain);

    float out = (noisy + tone) * vel;

    if (nGain < 0.0015 && tGain < 0.0015)
        active = false;

    return out;
}

// ================================================================
//  ClapVoice
// ================================================================
void DrumSynth::ClapVoice::trigger(float v, double sr, float dm) {
    vel       = v;
    samplePos = 0;

    for (int i = 0; i < NUM_BURSTS; ++i) {
        burstSamp[i] = static_cast<int>(BURST_TIMES[i] * sr);
        double fc = 1200.0 + i * 200.0;
        bp[i].setBP(fc, 1.2, sr);
        bp[i].reset();
        burstEnv[i].set(1.0, 0.001, 0.03 * dm, sr);
    }
    active = true;
}

float DrumSynth::ClapVoice::tick(double sr) {
    if (!active) return 0.0f;

    float out = 0.0f;
    float rawNoise = noise.tick();

    for (int i = 0; i < NUM_BURSTS; ++i) {
        if (samplePos >= burstSamp[i]) {
            double g = burstEnv[i].tick();
            out += bp[i].tick(rawNoise) * static_cast<float>(g);
        }
    }

    ++samplePos;
    out *= vel;

    // Deactivate when last burst envelope dies
    double lastG = burstEnv[NUM_BURSTS - 1].value;
    if (samplePos > burstSamp[NUM_BURSTS - 1] && lastG < 0.0015)
        active = false;

    return out;
}

// ================================================================
//  RimVoice
// ================================================================
void DrumSynth::RimVoice::trigger(float v, double sr, float dm) {
    vel = v;
    osc.reset();
    osc.shape = Oscillator::Shape::Square;
    freqEnv.set(800.0, 400.0, 0.035 * dm, sr);
    oscEnv.set(1.0, 0.001, 0.035 * dm, sr);
    noiseEnv.set(0.4, 0.001, 0.02 * dm, sr);
    active = true;
}

float DrumSynth::RimVoice::tick(double sr) {
    if (!active) return 0.0f;

    double freq  = freqEnv.tick();
    double oGain = oscEnv.tick();
    float  oscOut= osc.tick(freq, sr) * static_cast<float>(oGain);

    double nGain = noiseEnv.tick();
    float  nOut  = noise.tick() * static_cast<float>(nGain);

    float out = (oscOut + nOut) * vel;

    if (oGain < 0.0015 && nGain < 0.0015)
        active = false;

    return out;
}

// ================================================================
//  HHClosedVoice
// ================================================================
void DrumSynth::HHClosedVoice::trigger(float v, double sr, float dm) {
    vel = v;
    hp.setHP(7000.0, 0.707, sr);
    hp.reset();
    gainEnv.set(1.0, 0.001, 0.09 * dm, sr);
    active = true;
}

float DrumSynth::HHClosedVoice::tick(double sr) {
    if (!active) return 0.0f;
    (void)sr;
    double g   = gainEnv.tick();
    float  out = hp.tick(noise.tick()) * static_cast<float>(g) * vel;
    if (g < 0.0015) active = false;
    return out;
}

// ================================================================
//  HHOpenVoice
// ================================================================
void DrumSynth::HHOpenVoice::trigger(float v, double sr, float dm) {
    vel = v;
    hp.setHP(6000.0, 0.707, sr);
    hp.reset();
    gainEnv.set(1.0, 0.001, 0.45 * dm, sr);
    active = true;
}

float DrumSynth::HHOpenVoice::tick(double sr) {
    if (!active) return 0.0f;
    (void)sr;
    double g   = gainEnv.tick();
    float  out = hp.tick(noise.tick()) * static_cast<float>(g) * vel;
    if (g < 0.0015) active = false;
    return out;
}

// ================================================================
//  RideVoice  — 4 sine partials, 1.2s decay
// ================================================================
void DrumSynth::RideVoice::trigger(float v, double sr, float dm) {
    vel = v;
    for (int i = 0; i < NUM_PARTIALS; ++i) {
        osc[i].reset();
        osc[i].shape = Oscillator::Shape::Sine;
    }
    gainEnv.set(0.8, 0.001, 1.2 * dm, sr);
    active = true;
}

float DrumSynth::RideVoice::tick(double sr) {
    if (!active) return 0.0f;

    double g = gainEnv.tick();
    float  out = 0.0f;
    for (int i = 0; i < NUM_PARTIALS; ++i)
        out += osc[i].tick(PARTIAL_FREQS[i], sr);

    out *= static_cast<float>(g) * vel * 0.25f;
    if (g < 0.0015) active = false;
    return out;
}

// ================================================================
//  CrashVoice — HP noise + 3 saw partials
// ================================================================
void DrumSynth::CrashVoice::trigger(float v, double sr, float dm) {
    vel = v;
    hp.setHP(4000.0, 0.707, sr);
    hp.reset();
    noiseEnv.set(1.0, 0.001, 1.6 * dm, sr);

    for (int i = 0; i < NUM_SAWS; ++i) {
        sawOsc[i].reset();
        sawOsc[i].shape = Oscillator::Shape::Sawtooth;
    }
    sawEnv.set(0.5, 0.001, 0.8 * dm, sr);
    active = true;
}

float DrumSynth::CrashVoice::tick(double sr) {
    if (!active) return 0.0f;

    double nG  = noiseEnv.tick();
    float noisy= hp.tick(noise.tick()) * static_cast<float>(nG);

    double sG  = sawEnv.tick();
    float saws = 0.0f;
    for (int i = 0; i < NUM_SAWS; ++i)
        saws += sawOsc[i].tick(SAW_FREQS[i], sr);
    saws *= static_cast<float>(sG) * (1.0f / NUM_SAWS);

    float out = (noisy + saws) * vel;
    if (nG < 0.0015 && sG < 0.0015) active = false;
    return out;
}

// ================================================================
//  TomVoice
// ================================================================
void DrumSynth::TomVoice::setConfig(double sHz, double eHz, double tDur, double nDur) {
    startHz = sHz; endHz = eHz; toneDur = tDur; noiseDur = nDur;
}

void DrumSynth::TomVoice::trigger(float v, double sr, float dm) {
    vel = v;
    osc.reset();
    osc.shape = Oscillator::Shape::Sine;
    freqEnv.set(startHz, endHz, toneDur * dm, sr);
    toneEnv.set(1.0, 0.001, toneDur * dm, sr);
    noiseEnv.set(0.4, 0.001, noiseDur * dm, sr);
    active = true;
}

float DrumSynth::TomVoice::tick(double sr) {
    if (!active) return 0.0f;

    double freq = freqEnv.tick();
    double tG   = toneEnv.tick();
    float  tone = osc.tick(freq, sr) * static_cast<float>(tG);

    double nG   = noiseEnv.tick();
    float  noisy= noise.tick() * static_cast<float>(nG);

    float out = (tone + noisy) * vel;
    if (tG < 0.0015 && nG < 0.0015) active = false;
    return out;
}

// ================================================================
//  PercVoice — 3 triangle partials, 0.18s
// ================================================================
void DrumSynth::PercVoice::trigger(float v, double sr, float dm) {
    vel = v;
    for (int i = 0; i < NUM_OSCS; ++i) {
        osc[i].reset();
        osc[i].shape = Oscillator::Shape::Triangle;
        // stagger phases slightly using offsets
        osc[i].phase = PARTIAL_OFFSETS[i];
    }
    gainEnv.set(0.9, 0.001, 0.18 * dm, sr);
    active = true;
}

float DrumSynth::PercVoice::tick(double sr) {
    if (!active) return 0.0f;

    double g = gainEnv.tick();
    float out = 0.0f;
    for (int i = 0; i < NUM_OSCS; ++i)
        out += osc[i].tick(PARTIAL_FREQS[i], sr);

    out *= static_cast<float>(g) * vel * (1.0f / NUM_OSCS);
    if (g < 0.0015) active = false;
    return out;
}

// ================================================================
//  DrumSynth — public interface
// ================================================================
void DrumSynth::trigger(int voiceIdx, float velocity) {
    float v = std::max(0.0f, std::min(1.0f, velocity));

    // Initialise Tom configs if not yet set
    // (safe to call multiple times — values don't change)
    tomH.setConfig(260.0, 160.0, 0.40, 0.06);
    tomM.setConfig(180.0, 105.0, 0.50, 0.06);
    tomL.setConfig(120.0,  60.0, 0.65, 0.07);

    switch (voiceIdx) {
        case KICK:   kick   .trigger(v, sampleRate, decayMult_); break;
        case SNARE:  snare  .trigger(v, sampleRate, decayMult_); break;
        case CLAP:   clap   .trigger(v, sampleRate, decayMult_); break;
        case RIM:    rim    .trigger(v, sampleRate, decayMult_); break;
        case HH_CL:  hhClosed.trigger(v, sampleRate, decayMult_); break;
        case HH_OP:  hhOpen  .trigger(v, sampleRate, decayMult_); break;
        case RIDE:   ride   .trigger(v, sampleRate, decayMult_); break;
        case CRASH:  crash  .trigger(v, sampleRate, decayMult_); break;
        case TOM_H:  tomH   .trigger(v, sampleRate, decayMult_); break;
        case TOM_M:  tomM   .trigger(v, sampleRate, decayMult_); break;
        case TOM_L:  tomL   .trigger(v, sampleRate, decayMult_); break;
        case PERC:   perc   .trigger(v, sampleRate, decayMult_); break;
        default: break;
    }
}

float DrumSynth::renderSample() {
    float s = 0.0f;
    s += kick    .tick(sampleRate);
    s += snare   .tick(sampleRate);
    s += clap    .tick(sampleRate);
    s += rim     .tick(sampleRate);
    s += hhClosed.tick(sampleRate);
    s += hhOpen  .tick(sampleRate);
    s += ride    .tick(sampleRate);
    s += crash   .tick(sampleRate);
    s += tomH    .tick(sampleRate);
    s += tomM    .tick(sampleRate);
    s += tomL    .tick(sampleRate);
    s += perc    .tick(sampleRate);

    // Drive: soft-clip with adjustable pre-gain (0.7 clean → 3.0 saturated)
    float driveGain = 0.7f + driveAmount_ * 2.3f;
    s = std::tanh(s * driveGain);

    // Tone: lowpass filter (cutoff set by setToneFilter)
    return toneFilter_.tick(s);
}
