#include "VoltaSync.h"
#include <cstring>

// ============================================================
//  VoltaSync implementation
//  Uses a minimal hand-rolled WebSocket client over JUCE's
//  StreamingSocket.  Only supports text frames up to 65535 bytes
//  (more than enough for JSON pad-trigger messages).
// ============================================================

VoltaSync::VoltaSync()
    : juce::Thread("VoltaSync")
{
    startThread(juce::Thread::Priority::low);
}

VoltaSync::~VoltaSync()
{
    wantConnect = false;
    wsDisconnect();
    stopThread(2000);
}

//==============================================================================
void VoltaSync::connect(const juce::String& sessionCode)
{
    juce::ScopedLock sl(sessionLock);
    pendingSession = sessionCode.toUpperCase().retainCharacters("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789").substring(0, 8);
    wantConnect = true;
    notify();   // wake thread
}

void VoltaSync::disconnect()
{
    wantConnect = false;
    wsDisconnect();
}

void VoltaSync::sendTrigger(int padIdx, float vel)
{
    if (state.load() != State::Connected) return;
    juce::String json;
    json << "{\"action\":\"trigger\",\"pad\":" << padIdx
         << ",\"vel\":" << juce::String(vel, 3) << "}";
    enqueueJson(json);
    notify();
}

void VoltaSync::sendStepChange(int voice, int step, bool on,
                               int colorIdx, const juce::String& user)
{
    if (state.load() != State::Connected) return;
    // Escape user name for JSON (strip quotes)
    juce::String safeUser = user.replace("\"", "'").substring(0, 32);
    juce::String json;
    json << "{\"action\":\"step\",\"voice\":" << voice
         << ",\"step\":"  << step
         << ",\"on\":"    << (on ? "true" : "false")
         << ",\"color\":" << colorIdx
         << ",\"user\":\"" << safeUser << "\"}";
    enqueueJson(json);
    notify();
}

void VoltaSync::sendGridSync(const juce::String& grid192,
                             const juce::String& colors192)
{
    if (state.load() != State::Connected) return;
    enqueueJson("{\"action\":\"steps_sync\",\"grid\":\"" + grid192
                + "\",\"colors\":\"" + colors192 + "\"}");
    notify();
}

void VoltaSync::sendSampleChunk(int voice, int idx, int total,
                                const juce::String& name, int sr,
                                const juce::String& base64Data)
{
    if (state.load() != State::Connected) return;
    juce::String safeName = name.replace("\"", "'").substring(0, 64);
    juce::String json;
    json << "{\"action\":\"sample_chunk\",\"voice\":" << voice
         << ",\"idx\":"   << idx
         << ",\"total\":" << total
         << ",\"name\":\"" << safeName << "\""
         << ",\"sr\":"    << sr
         << ",\"data\":\"" << base64Data << "\"}";
    enqueueJson(json);
    notify();
}

//==============================================================================
void VoltaSync::run()
{
    while (!threadShouldExit())
    {
        if (!wantConnect)
        {
            wait(200);
            continue;
        }

        // Grab the pending session
        juce::String session;
        {
            juce::ScopedLock sl(sessionLock);
            session = pendingSession;
        }

        if (session.isEmpty()) { wait(200); continue; }

        // ── Connect ──────────────────────────────────────────
        if (listener) listener->voltaSyncStateChanged(State::Connecting);
        state = State::Connecting;

        if (!wsConnect() || !wsHandshake())
        {
            wsDisconnect();
            state = State::Disconnected;
            if (listener) listener->voltaSyncStateChanged(State::Disconnected);
            wait(3000);   // retry after 3s
            continue;
        }

        // Send join message
        {
            juce::ScopedLock sl(sessionLock);
            currentSession = session;
        }
        juce::String joinMsg;
        joinMsg << "{\"action\":\"join\",\"session\":\"" << session << "\"}";
        if (!wsSendText(joinMsg))
        {
            wsDisconnect();
            state = State::Disconnected;
            if (listener) listener->voltaSyncStateChanged(State::Disconnected);
            wait(2000);
            continue;
        }

        state = State::Connected;
        if (listener) listener->voltaSyncStateChanged(State::Connected);

        // ── Main loop ─────────────────────────────────────────
        int pingCounter = 0;
        while (!threadShouldExit() && wantConnect)
        {
            // Send queued outgoing messages
            {
                int numReady, start1, size1, start2, size2;
                numReady = outFifo.getNumReady();
                if (numReady > 0)
                {
                    outFifo.prepareToRead(numReady, start1, size1, start2, size2);
                    for (int i = 0; i < size1; ++i)
                        if (!wsSendText(outQueue[start1 + i].json)) goto reconnect;
                    for (int i = 0; i < size2; ++i)
                        if (!wsSendText(outQueue[start2 + i].json)) goto reconnect;
                    outFifo.finishedRead(size1 + size2);
                }
            }

            // Periodic ping to server
            if (++pingCounter > 600)   // ~30s at 50ms loop
            {
                pingCounter = 0;
                if (!wsSendText("{\"action\":\"ping\"}")) goto reconnect;
            }

            // Receive incoming frame (50ms timeout via waitUntilReady)
            {
                auto text = wsReceiveText();
                if (text == "__ERROR__") goto reconnect;

                if (text.isNotEmpty())
                {
                    // Parse minimal JSON
                    if (text.contains("\"trigger\""))
                    {
                        int pad = -1; float vel = 1.0f;
                        auto padStr = text.fromFirstOccurrenceOf("\"pad\":", false, false);
                        auto velStr = text.fromFirstOccurrenceOf("\"vel\":", false, false);
                        if (padStr.isNotEmpty()) pad = padStr.getIntValue();
                        if (velStr.isNotEmpty()) vel = velStr.getFloatValue();
                        if (pad >= 0 && pad < 12 && listener)
                            listener->voltaSyncTrigger(pad, vel);
                    }
                    else if (text.contains("\"peers\""))
                    {
                        auto countStr = text.fromFirstOccurrenceOf("\"count\":", false, false);
                        int count = countStr.isEmpty() ? 1 : countStr.getIntValue();
                        peerCount.store(count);
                        if (listener) listener->voltaSyncPeersChanged(count);
                    }
                    else if (text.contains("\"joined\""))
                    {
                        auto countStr = text.fromFirstOccurrenceOf("\"peers\":", false, false);
                        int count = countStr.isEmpty() ? 1 : countStr.getIntValue();
                        peerCount.store(count);
                        if (listener) listener->voltaSyncPeersChanged(count);
                    }
                    else if (text.contains("\"action\":\"step\""))
                    {
                        int  v  = text.fromFirstOccurrenceOf("\"voice\":", false, false).getIntValue();
                        int  s  = text.fromFirstOccurrenceOf("\"step\":",  false, false).getIntValue();
                        bool on = text.contains("\"on\":true");
                        // color field (default -1)
                        int color = -1;
                        auto colorStr = text.fromFirstOccurrenceOf("\"color\":", false, false);
                        if (colorStr.isNotEmpty()) color = colorStr.getIntValue();
                        // user name
                        juce::String user = text.fromFirstOccurrenceOf("\"user\":\"", false, false)
                                               .upToFirstOccurrenceOf("\"", false, false);
                        if (listener && v >= 0 && v < 12 && s >= 0 && s < 16)
                            listener->voltaSyncStepChanged(v, s, on, color, user);
                    }
                    else if (text.contains("\"action\":\"steps_sync\""))
                    {
                        // New format: {"action":"steps_sync","grid":"...","colors":"..."}
                        juce::String grid   = text.fromFirstOccurrenceOf("\"grid\":\"",   false, false)
                                                  .upToFirstOccurrenceOf("\"", false, false);
                        juce::String colors = text.fromFirstOccurrenceOf("\"colors\":\"", false, false)
                                                  .upToFirstOccurrenceOf("\"", false, false);
                        // Also support legacy single-field format
                        if (grid.isEmpty())
                            grid = text.fromFirstOccurrenceOf("\"data\":\"", false, false)
                                       .upToFirstOccurrenceOf("\"", false, false);
                        if (grid.length() == 192 && listener)
                            listener->voltaSyncGridSync(grid, colors);
                    }
                    else if (text.contains("\"action\":\"sample_chunk\""))
                    {
                        int  voice = text.fromFirstOccurrenceOf("\"voice\":", false, false).getIntValue();
                        int  idx   = text.fromFirstOccurrenceOf("\"idx\":",   false, false).getIntValue();
                        int  total = text.fromFirstOccurrenceOf("\"total\":", false, false).getIntValue();
                        int  sr    = text.fromFirstOccurrenceOf("\"sr\":",    false, false).getIntValue();
                        juce::String name = text.fromFirstOccurrenceOf("\"name\":\"", false, false)
                                               .upToFirstOccurrenceOf("\"", false, false);
                        juce::String data = text.fromFirstOccurrenceOf("\"data\":\"", false, false)
                                               .upToFirstOccurrenceOf("\"", false, false);
                        if (listener && voice >= 0 && voice < 12)
                            listener->voltaSyncSampleChunk(voice, idx, total, name, sr, data);
                    }
                }
            }

            wait(50);
            continue;

        reconnect:
            break;
        }

        wsDisconnect();
        peerCount.store(0);
        state = State::Disconnected;
        if (listener) listener->voltaSyncStateChanged(State::Disconnected);
        if (listener) listener->voltaSyncPeersChanged(0);

        if (wantConnect) wait(3000);   // pause before retry
    }
}

//==============================================================================
//  WebSocket helpers
//==============================================================================

bool VoltaSync::wsConnect()
{
    socket.close();
    return socket.connect(SERVER_HOST, SERVER_PORT, 5000);
}

bool VoltaSync::wsHandshake()
{
    // Generate random 16-byte nonce, base64-encode it
    juce::uint8 nonce[16];
    juce::Random rng;
    for (auto& b : nonce) b = static_cast<juce::uint8>(rng.nextInt(256));

    juce::MemoryBlock nonceBlock(nonce, 16);
    juce::String key = juce::Base64::toBase64(nonceBlock.getData(), 16);

    juce::String request;
    request << "GET / HTTP/1.1\r\n"
            << "Host: " << SERVER_HOST << "\r\n"
            << "Upgrade: websocket\r\n"
            << "Connection: Upgrade\r\n"
            << "Sec-WebSocket-Key: " << key << "\r\n"
            << "Sec-WebSocket-Version: 13\r\n"
            << "\r\n";

    auto utf8 = request.toUTF8();
    int len = (int)std::strlen(utf8.getAddress());
    if (socket.write(utf8.getAddress(), len) != len) return false;

    // Read HTTP response until \r\n\r\n
    juce::String response;
    char ch;
    int timeout = 5000;
    while (!response.contains("\r\n\r\n"))
    {
        if (socket.waitUntilReady(true, 50) == 1)
        {
            if (socket.read(&ch, 1, false) != 1) return false;
            response += ch;
        }
        else
        {
            timeout -= 50;
            if (timeout <= 0) return false;
        }
    }

    return response.contains("101");
}

void VoltaSync::wsDisconnect()
{
    socket.close();
}

bool VoltaSync::wsSendText(const juce::String& text)
{
    auto utf8 = text.toUTF8();
    int payloadLen = (int)std::strlen(utf8.getAddress());

    // Generate 4-byte masking key
    juce::uint8 maskKey[4];
    juce::Random rng;
    for (auto& b : maskKey) b = static_cast<juce::uint8>(rng.nextInt(256));

    // Build frame
    std::vector<juce::uint8> frame;
    frame.reserve(payloadLen + 10);

    frame.push_back(0x81);   // FIN=1, opcode=1 (text)

    // Masked bit set (0x80), length
    if (payloadLen <= 125)
    {
        frame.push_back(static_cast<juce::uint8>(0x80 | payloadLen));
    }
    else
    {
        frame.push_back(0x80 | 126);
        frame.push_back(static_cast<juce::uint8>((payloadLen >> 8) & 0xFF));
        frame.push_back(static_cast<juce::uint8>(payloadLen & 0xFF));
    }

    for (auto b : maskKey) frame.push_back(b);

    const char* payload = utf8.getAddress();
    for (int i = 0; i < payloadLen; ++i)
        frame.push_back(static_cast<juce::uint8>(payload[i]) ^ maskKey[i % 4]);

    return socket.write(frame.data(), (int)frame.size()) == (int)frame.size();
}

juce::String VoltaSync::wsReceiveText()
{
    int ready = socket.waitUntilReady(true, 50);
    if (ready == 0) return "";     // timeout — no data
    if (ready < 0)  return "__ERROR__";

    auto frame = readFrame();
    if (!frame.ok) return "__ERROR__";

    // Opcode 0x9 = ping → send pong
    if (frame.opcode == 0x9)
    {
        juce::uint8 pong[2] = { 0x8A, 0x00 };   // FIN + opcode=0xA (pong), no payload
        socket.write(pong, 2);
        return "";
    }

    // Opcode 0x8 = close
    if (frame.opcode == 0x8) return "__ERROR__";

    // Text frame
    if (frame.opcode == 0x1)
        return juce::String::fromUTF8(static_cast<const char*>(frame.payload.getData()),
                                       (int)frame.payload.getSize());

    return "";
}

VoltaSync::Frame VoltaSync::readFrame()
{
    Frame f { false, false, 0, {} };

    auto readByte = [&](juce::uint8& out) -> bool {
        return socket.read(&out, 1, false) == 1;
    };

    juce::uint8 b0, b1;
    if (!readByte(b0) || !readByte(b1)) return f;

    f.isFin  = (b0 & 0x80) != 0;
    f.opcode = b0 & 0x0F;
    bool masked = (b1 & 0x80) != 0;
    int  len    = b1 & 0x7F;

    if (len == 126)
    {
        juce::uint8 ext[2];
        if (!readByte(ext[0]) || !readByte(ext[1])) return f;
        len = (ext[0] << 8) | ext[1];
    }
    else if (len == 127) return f;   // too large, unsupported

    juce::uint8 maskKey[4] = {};
    if (masked) {
        for (auto& b : maskKey) if (!readByte(b)) return f;
    }

    f.payload.setSize(len);
    auto* dest = static_cast<juce::uint8*>(f.payload.getData());
    for (int i = 0; i < len; ++i) {
        if (!readByte(dest[i])) return f;
        if (masked) dest[i] ^= maskKey[i % 4];
    }

    f.ok = true;
    return f;
}

void VoltaSync::enqueueJson(const juce::String& json)
{
    int start1, size1, start2, size2;
    outFifo.prepareToWrite(1, start1, size1, start2, size2);
    if (size1 > 0)      outQueue[start1].json = json;
    else if (size2 > 0) outQueue[start2].json = json;
    else return;   // queue full — drop
    outFifo.finishedWrite(size1 > 0 ? 1 : (size2 > 0 ? 1 : 0));
}
