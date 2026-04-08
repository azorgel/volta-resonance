#include "StepSequencer.h"
#include <cstring>
#include <cmath>

void StepSequencer::prepare(double sampleRate, double bpm)
{
    sr_  = sampleRate;
    bpm_ = bpm;
    resetPlayback();
}

void StepSequencer::setBpm(double bpm)
{
    bpm_ = bpm;
}

uint16_t StepSequencer::advance(int numSamples, double ppq,
                                 double bpm, bool transportPlaying)
{
    if (!transportPlaying)
    {
        lastPpqStep = -1;
        return 0;
    }

    setBpm(bpm);
    uint16_t fired = 0;

    if (ppq >= 0.0)
    {
        // DAW path — lock to host transport ppq position
        int newStep = static_cast<int>(ppq * 4.0) % STEPS;
        if (newStep < 0) newStep += STEPS;    // safety for negative ppq
        if (newStep != lastPpqStep)
        {
            lastPpqStep  = newStep;
            currentStep  = newStep;
            for (int v = 0; v < VOICES; ++v)
                if (grid[v][newStep]) fired |= static_cast<uint16_t>(1u << v);
        }
    }
    else
    {
        // Internal clock path — accumulate samples
        const double samplesPerStep = sr_ * 60.0 / bpm_ / 4.0;
        internalAccum += numSamples;
        while (internalAccum >= samplesPerStep)
        {
            internalAccum -= samplesPerStep;
            currentStep = (currentStep + 1) % STEPS;
            for (int v = 0; v < VOICES; ++v)
                if (grid[v][currentStep]) fired |= static_cast<uint16_t>(1u << v);
        }
    }

    return fired;
}

void StepSequencer::setStep(int voice, int step, bool on)
{
    if (voice >= 0 && voice < VOICES && step >= 0 && step < STEPS)
        grid[voice][step] = on;
}

bool StepSequencer::getStep(int voice, int step) const
{
    if (voice >= 0 && voice < VOICES && step >= 0 && step < STEPS)
        return grid[voice][step];
    return false;
}

int StepSequencer::getCurrentStep() const
{
    return currentStep;
}

void StepSequencer::resetPlayback()
{
    internalAccum = 0.0;
    currentStep   = 0;
    lastPpqStep   = -1;
}

void StepSequencer::serialise(char* out193) const
{
    for (int v = 0; v < VOICES; ++v)
        for (int s = 0; s < STEPS; ++s)
            out193[v * STEPS + s] = grid[v][s] ? '1' : '0';
    out193[VOICES * STEPS] = '\0';
}

bool StepSequencer::deserialise(const char* in193)
{
    if (!in193 || std::strlen(in193) < static_cast<std::size_t>(VOICES * STEPS))
        return false;
    for (int v = 0; v < VOICES; ++v)
        for (int s = 0; s < STEPS; ++s)
            grid[v][s] = (in193[v * STEPS + s] == '1');
    return true;
}
