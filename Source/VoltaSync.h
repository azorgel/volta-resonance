#pragma once
#include <JuceHeader.h>
#include <atomic>
#include <array>
#include <functional>

// ============================================================
//  VoltaSync — lightweight WebSocket client
//  Runs on its own thread; thread-safe public API.
// ============================================================

class VoltaSync : private juce::Thread
{
public:
    static constexpr const char* SERVER_HOST = "volta-sync.fly.dev";
    static constexpr int         SERVER_PORT = 80;

    enum class State { Disconnected, Connecting, Connected };

    // Called on the network thread — use atomics only, no JUCE GUI objects
    struct Listener {
        virtual ~Listener() = default;
        virtual void voltaSyncTrigger(int padIdx, float vel) = 0;
        virtual void voltaSyncStateChanged(State s) = 0;
        virtual void voltaSyncPeersChanged(int count) = 0;
        // Step sequencer — now carries color index and user name
        virtual void voltaSyncStepChanged(int /*v*/, int /*s*/, bool /*on*/,
                                          int /*color*/, const juce::String& /*user*/) {}
        // Full grid sync — on/off grid + per-step color index
        virtual void voltaSyncGridSync(const juce::String& /*grid192*/,
                                       const juce::String& /*colors192*/) {}
        // Custom sample chunk (chunked base64 PCM transfer)
        virtual void voltaSyncSampleChunk(int /*voice*/, int /*idx*/, int /*total*/,
                                          const juce::String& /*name*/, int /*sr*/,
                                          const juce::String& /*data*/) {}
    };

    VoltaSync();
    ~VoltaSync() override;

    // ── Public API (call from any thread) ────────────────────
    void connect(const juce::String& sessionCode);
    void disconnect();
    void sendTrigger(int padIdx, float vel);
    void sendStepChange(int voice, int step, bool on,
                        int colorIdx, const juce::String& user);
    void sendGridSync(const juce::String& grid192,
                      const juce::String& colors192);
    void sendSampleChunk(int voice, int idx, int total,
                         const juce::String& name, int sr,
                         const juce::String& base64Data);

    State        getState()     const { return state.load(); }
    int          getPeerCount() const { return peerCount.load(); }
    juce::String getSession()   const { return currentSession; }
    bool         isConnected()  const { return state.load() == State::Connected; }

    void setListener(Listener* l) { listener = l; }

private:
    void run() override;

    bool         wsConnect();
    bool         wsHandshake();
    void         wsDisconnect();
    bool         wsSendText(const juce::String& text);
    juce::String wsReceiveText();

    struct Frame { bool ok; bool isFin; int opcode; juce::MemoryBlock payload; };
    Frame readFrame();

    struct OutMsg { juce::String json; };
    static constexpr int QUEUE_SIZE = 128;
    juce::AbstractFifo             outFifo { QUEUE_SIZE };
    std::array<OutMsg, QUEUE_SIZE> outQueue;

    void enqueueJson(const juce::String& json);

    juce::StreamingSocket  socket;
    std::atomic<State>     state     { State::Disconnected };
    std::atomic<int>       peerCount { 0 };
    juce::String           pendingSession;
    juce::String           currentSession;
    juce::CriticalSection  sessionLock;

    Listener* listener = nullptr;

    std::atomic<bool> wantConnect { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VoltaSync)
};
