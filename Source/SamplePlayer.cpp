#include "SamplePlayer.h"
#include <cmath>
#include <algorithm>

void SamplePlayer::load(const float* data, int numSamples, double sourceSr, double targetSr)
{
    // Signal audio thread: buffer is being replaced
    loaded_.store(false, std::memory_order_release);

    double ratio  = (targetSr > 0.0 && sourceSr > 0.0) ? sourceSr / targetSr : 1.0;
    int    outLen = static_cast<int>(numSamples / ratio);
    outLen = std::max(1, std::min(outLen, MAX_SAMPLES));

    std::vector<float> newBuf(static_cast<size_t>(outLen));
    for (int i = 0; i < outLen; ++i)
    {
        double srcPos = i * ratio;
        int    idx    = static_cast<int>(srcPos);
        float  frac   = static_cast<float>(srcPos - idx);
        float  s0     = (idx     < numSamples) ? data[idx]     : 0.0f;
        float  s1     = (idx + 1 < numSamples) ? data[idx + 1] : 0.0f;
        newBuf[static_cast<size_t>(i)] = s0 + frac * (s1 - s0);
    }

    buffer_   = std::move(newBuf);
    storedSr_ = targetSr;
    playPos_.store(-1, std::memory_order_relaxed);
    loaded_.store(true, std::memory_order_release);
}

void SamplePlayer::clear()
{
    loaded_.store(false, std::memory_order_release);
    playPos_.store(-1, std::memory_order_relaxed);
    buffer_.clear();
    name.clear();
}

void SamplePlayer::trigger(float vel)
{
    if (!loaded_.load(std::memory_order_acquire)) return;
    vel_ = vel;
    playPos_.store(0, std::memory_order_relaxed);
}

float SamplePlayer::renderSample()
{
    if (!loaded_.load(std::memory_order_acquire)) return 0.0f;
    int pos = playPos_.load(std::memory_order_relaxed);
    if (pos < 0 || pos >= static_cast<int>(buffer_.size())) return 0.0f;
    float out = buffer_[static_cast<size_t>(pos)] * vel_;
    playPos_.store(pos + 1, std::memory_order_relaxed);
    return out;
}
