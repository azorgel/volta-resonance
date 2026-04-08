#pragma once
#include <vector>
#include <string>
#include <atomic>
#include <cstdint>

// ============================================================
//  SamplePlayer — mono sample playback for one voice.
//  load() is called on the message/main thread.
//  trigger() and renderSample() are called on the audio thread.
//  Safety: loaded_ guards the buffer so the audio thread never
//  reads a partially-constructed vector.
// ============================================================
class SamplePlayer
{
public:
    static constexpr int MAX_SAMPLES = 44100 * 12;  // 12 seconds cap

    // Load raw float mono PCM.  sourceSr = file sample rate,
    // targetSr = plugin current sample rate (linear resample on load).
    void load(const float* data, int numSamples, double sourceSr, double targetSr);
    bool isLoaded()  const { return loaded_.load(std::memory_order_acquire); }
    bool isPlaying() const { return isLoaded() && playPos_.load() >= 0; }
    void clear();

    // Audio-thread calls
    void  trigger(float vel);
    float renderSample();

    // For network encoding / sync
    const std::vector<float>& getBuffer() const { return buffer_; }
    double                    getStoredSr() const { return storedSr_; }

    std::string name;   // display / sync name

private:
    std::vector<float>  buffer_;
    std::atomic<bool>   loaded_  { false };
    std::atomic<int>    playPos_ { -1 };
    double              storedSr_ = 44100.0;
    float               vel_      = 1.0f;
};
