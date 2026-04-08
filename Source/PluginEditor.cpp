#include "PluginEditor.h"
#include <cmath>
#include <algorithm>

// ── Static label data ──────────────────────────────────────────
const char* VoltaResonanceEditor::PAD_NAMES[DrumSynth::NUM_VOICES] = {
    "KICK","SNARE","CLAP","RIM","HH CL","HH OP","RIDE","CRASH","TOM H","TOM M","TOM L","PERC"
};
const char* VoltaResonanceEditor::PAD_LABELS[DrumSynth::NUM_VOICES] = {
    "P01","P02","P03","P04","P05","P06","P07","P08","P09","P10","P11","P12"
};
const char* VoltaResonanceEditor::SEQ_VOICE_LABELS[DrumSynth::NUM_VOICES] = {
    "BD","SD","CP","RM","HC","HO","RD","CR","TH","TM","TL","PC"
};
const char* VoltaResonanceEditor::KNOB_LABELS[4] = { "LEVEL","DECAY","TONE","DRIVE" };

// ── User colour palette ────────────────────────────────────────
juce::Colour VoltaResonanceEditor::userColor(int idx)
{
    static const juce::Colour palette[6] = {
        juce::Colour(0xff00d4ff),   // 0 cyan
        juce::Colour(0xffffaa00),   // 1 gold
        juce::Colour(0xff00ff99),   // 2 green
        juce::Colour(0xffff4488),   // 3 pink
        juce::Colour(0xffaa44ff),   // 4 purple
        juce::Colour(0xffff7700),   // 5 orange
    };
    return palette[juce::jlimit(0, 5, idx)];
}

// ── Layout helpers ─────────────────────────────────────────────
juce::Rectangle<int> VoltaResonanceEditor::headerBounds()      const { return { 0, 0, W, HDR_H }; }
juce::Rectangle<int> VoltaResonanceEditor::signalStripBounds() const { return { 0, HDR_H, W, SIG_H }; }
juce::Rectangle<int> VoltaResonanceEditor::padAreaBounds()     const { return { 0, HDR_H + SIG_H, W, PAD_AREA_H }; }
juce::Rectangle<int> VoltaResonanceEditor::seqBounds()         const { return { 0, HDR_H + SIG_H + PAD_AREA_H, W, SEQ_H }; }
juce::Rectangle<int> VoltaResonanceEditor::syncPanelBounds()   const { return { 0, HDR_H + SIG_H + PAD_AREA_H + SEQ_H, W, SYNC_H }; }
juce::Rectangle<int> VoltaResonanceEditor::knobAreaBounds()    const { return { 0, HDR_H + SIG_H + PAD_AREA_H + SEQ_H + SYNC_H, W, KNOB_H }; }
juce::Rectangle<int> VoltaResonanceEditor::footerBounds()      const { return { 0, H - FOOTER_H, W, FOOTER_H }; }

juce::Rectangle<int> VoltaResonanceEditor::seqPlayButtonBounds() const
{
    int seqY = seqBounds().getY();
    return { W - MARGIN - 74, seqY + 10, 72, 16 };
}

juce::Rectangle<int> VoltaResonanceEditor::padLoadButtonBounds(int idx) const
{
    auto p = padBounds(idx);
    return { p.getX() + 4, p.getBottom() - 14, 60, 12 };
}

int VoltaResonanceEditor::stepCellX(int step) const
{
    // gridX = MARGIN + SEQ_LABEL_W = 64
    // group stride = 4*36 + 3*2 + 4 = 154
    // within-group stride = 36 + 2 = 38
    const int gridX        = MARGIN + SEQ_LABEL_W;
    const int groupStride  = 4 * SEQ_CELL_W + 3 * 2 + 4;
    const int withinStride = SEQ_CELL_W + 2;
    return gridX + (step / 4) * groupStride + (step % 4) * withinStride;
}

juce::Rectangle<int> VoltaResonanceEditor::stepCellBounds(int voice, int step) const
{
    int seqY = seqBounds().getY();
    int x = stepCellX(step);
    int y = seqY + SEQ_HDR_H + voice * SEQ_ROW_H;
    return { x, y, SEQ_CELL_W, SEQ_CELL_H };
}

juce::Rectangle<int> VoltaResonanceEditor::padClearButtonBounds(int idx) const
{
    auto lb = padLoadButtonBounds(idx);
    return { lb.getRight() - 14, lb.getY(), 14, lb.getHeight() };
}

juce::Rectangle<int> VoltaResonanceEditor::padBounds(int idx) const
{
    const int padW = (W - 2 * MARGIN - (PAD_COLS - 1) * GAP) / PAD_COLS;
    const int padH = (PAD_AREA_H - 8 - (PAD_ROWS - 1) * GAP) / PAD_ROWS;
    int col = idx % PAD_COLS;
    int row = idx / PAD_COLS;
    int x = MARGIN + col * (padW + GAP);
    int y = HDR_H + SIG_H + 4 + row * (padH + GAP);
    return { x, y, padW, padH };
}

juce::Rectangle<int> VoltaResonanceEditor::knobBounds(int idx) const
{
    const int knobW  = 56;
    const int totalW = 4 * knobW + 3 * 24;
    int startX = (W - totalW) / 2;
    int x = startX + idx * (knobW + 24);
    int y = knobAreaBounds().getY() + 10;
    return { x, y, knobW, KNOB_H - 20 };
}

// ── Constructor / Destructor ───────────────────────────────────
VoltaResonanceEditor::VoltaResonanceEditor(VoltaResonanceProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setResizable(true, false);
    setResizeLimits(350, 365, 1400, 1460);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio((double)W / H);
    setSize(W, H);

    // Session input
    sessionInput.setFont(juce::Font(juce::FontOptions().withHeight(13.0f)));
    sessionInput.setMultiLine(false);
    sessionInput.setReturnKeyStartsNewLine(false);
    sessionInput.setInputRestrictions(8, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789");
    sessionInput.setTextToShowWhenEmpty("SESSION CODE", juce::Colour(0xff404060));
    sessionInput.setColour(juce::TextEditor::backgroundColourId,    juce::Colour(0xff0d0d1a));
    sessionInput.setColour(juce::TextEditor::outlineColourId,        juce::Colour(0xff1e1e30));
    sessionInput.setColour(juce::TextEditor::focusedOutlineColourId, juce::Colour(0xff00d4ff));
    sessionInput.setColour(juce::TextEditor::textColourId,           juce::Colour(0xffeaeaf4));
    sessionInput.setJustification(juce::Justification::centred);
    addAndMakeVisible(sessionInput);

    // Connect button
    connectBtn.setColour(juce::TextButton::buttonColourId,   juce::Colour(0xff0a1a22));
    connectBtn.setColour(juce::TextButton::buttonOnColourId,  juce::Colour(0xff003344));
    connectBtn.setColour(juce::TextButton::textColourOffId,   juce::Colour(0xff00d4ff));
    connectBtn.setColour(juce::TextButton::textColourOnId,    juce::Colour(0xffffaa00));
    connectBtn.onClick = [this] {
        auto state = processor.voltaSync.getState();
        if (state == VoltaSync::State::Disconnected) {
            auto code = sessionInput.getText().toUpperCase();
            if (code.isNotEmpty()) {
                processor.voltaSync.connect(code);
                connectBtn.setButtonText("DISCONNECT");
            }
        } else {
            processor.voltaSync.disconnect();
            connectBtn.setButtonText("CONNECT");
        }
    };
    addAndMakeVisible(connectBtn);

    // User name input
    userNameInput.setFont(juce::Font(juce::FontOptions().withHeight(12.0f)));
    userNameInput.setMultiLine(false);
    userNameInput.setReturnKeyStartsNewLine(false);
    userNameInput.setInputRestrictions(16);
    userNameInput.setTextToShowWhenEmpty("YOUR NAME", juce::Colour(0xff404060));
    userNameInput.setColour(juce::TextEditor::backgroundColourId,    juce::Colour(0xff0d0d1a));
    userNameInput.setColour(juce::TextEditor::outlineColourId,        juce::Colour(0xff1e1e30));
    userNameInput.setColour(juce::TextEditor::focusedOutlineColourId, juce::Colour(0xff00d4ff));
    userNameInput.setColour(juce::TextEditor::textColourId,           juce::Colour(0xffeaeaf4));
    userNameInput.setJustification(juce::Justification::centredLeft);
    addAndMakeVisible(userNameInput);

    // Init bars
    juce::Random rng;
    for (int i = 0; i < NUM_BARS; ++i)
        barHeights[i] = barTargets[i] = rng.nextFloat() * 0.15f;

    startTimerHz(30);
}

VoltaResonanceEditor::~VoltaResonanceEditor() { stopTimer(); }

// ── resized ────────────────────────────────────────────────────
void VoltaResonanceEditor::resized()
{
    float s = getScale();
    auto sync = syncPanelBounds();

    // Row 1: session input + connect button
    int row1Y = sync.getY() + 8;
    int inputX = MARGIN + 110;
    int inputW = 150;
    sessionInput.setBounds(juce::roundToInt(inputX * s), juce::roundToInt(row1Y * s),
                           juce::roundToInt(inputW * s), juce::roundToInt(24 * s));
    connectBtn.setBounds(juce::roundToInt((inputX + inputW + 8) * s), juce::roundToInt(row1Y * s),
                         juce::roundToInt(100 * s), juce::roundToInt(24 * s));

    // Row 2: user name input (right side)
    int row2Y = sync.getY() + 46;
    int nameX = W - MARGIN - 108 - 8 - 110;
    userNameInput.setBounds(juce::roundToInt(nameX * s), juce::roundToInt(row2Y * s),
                            juce::roundToInt(110 * s), juce::roundToInt(22 * s));
}

// ── paint ──────────────────────────────────────────────────────
void VoltaResonanceEditor::paint(juce::Graphics& g)
{
    float s = getScale();
    if (s != 1.0f)
        g.addTransform(juce::AffineTransform::scale(s));

    paintBackground(g);
    paintHeader(g);
    paintSignalStrip(g);
    paintPads(g);
    paintSequencer(g);
    paintSyncPanel(g);
    paintKnobs(g);
    paintFooter(g);
}

// ── Background texture ─────────────────────────────────────────
void VoltaResonanceEditor::paintBackground(juce::Graphics& g)
{
    g.fillAll(colBg);

    // Subtle dot grid
    g.setColour(juce::Colour(0xff0c0c18));
    for (int y = 0; y < H; y += 20)
        for (int x = 0; x < W; x += 20)
            g.fillEllipse((float)x - 0.5f, (float)y - 0.5f, 1.5f, 1.5f);

    // Very faint horizontal scanlines
    g.setColour(juce::Colour(0x06000000));
    for (int y = 0; y < H; y += 3)
        g.fillRect(0, y, W, 1);

    // Subtle vignette — darken corners
    juce::ColourGradient vignette(juce::Colours::transparentBlack, W * 0.5f, H * 0.5f,
                                   juce::Colour(0x40000000), 0.0f, 0.0f, true);
    g.setGradientFill(vignette);
    g.fillRect(0, 0, W, H);
}

// ── Logo helper ────────────────────────────────────────────────
void VoltaResonanceEditor::drawVoltaLogo(juce::Graphics& g,
                                          float x, float y, float scale,
                                          juce::Colour col)
{
    float hw = 13.0f * scale;
    float hh = 20.0f * scale;

    juce::Path v;
    v.startNewSubPath(x,          y);
    v.lineTo(x + hw,       y + hh);
    v.lineTo(x + hw * 2.f, y);

    // Glow
    g.setColour(col.withAlpha(0.18f));
    g.strokePath(v, juce::PathStrokeType(6.0f, juce::PathStrokeType::curved,
                                          juce::PathStrokeType::rounded));
    // Bright edge
    g.setColour(col);
    g.strokePath(v, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved,
                                          juce::PathStrokeType::rounded));
}

// ── Header ─────────────────────────────────────────────────────
void VoltaResonanceEditor::paintHeader(juce::Graphics& g)
{
    auto r = headerBounds();

    // Background gradient
    g.setGradientFill(juce::ColourGradient(
        juce::Colour(0xff0e0e1e), 0.0f, 0.0f,
        colBg, 0.0f, (float)r.getBottom(), false));
    g.fillRect(r);

    // Bottom border with glow
    g.setColour(colBorder);
    g.drawHorizontalLine(r.getBottom() - 1, 0.0f, (float)W);
    g.setColour(colAccent.withAlpha(0.08f));
    g.fillRect(0, r.getBottom() - 2, W, 2);

    // Logo V shape
    drawVoltaLogo(g, (float)MARGIN + 2.0f, 14.0f, 1.0f, colAccent);

    // Brand text
    g.setColour(colMuted);
    g.setFont(juce::Font(juce::FontOptions().withHeight(8.5f)));
    g.drawText("VOLTA LABS", MARGIN + 34, 12, 120, 12, juce::Justification::centredLeft);

    g.setColour(colText);
    g.setFont(juce::Font(juce::FontOptions().withHeight(15.5f).withStyle("Bold")));
    g.drawText("OLTA RESONANCE", MARGIN + 33, 26, 200, 20, juce::Justification::centredLeft);

    // Accent underline under wordmark
    g.setColour(colAccent.withAlpha(0.5f));
    g.fillRect(MARGIN + 33, 44, 160, 1);

    // LED indicators — centre area
    int ledX = 240, ledY = r.getCentreY() - 3;
    auto drawLed = [&](int x, juce::Colour col, bool on) {
        if (on) {
            g.setColour(col.withAlpha(0.3f));
            g.fillEllipse((float)(x - 4), (float)(ledY - 4), 14.0f, 14.0f);
        }
        g.setColour(on ? col : colBorder);
        g.fillEllipse((float)x, (float)ledY, 6.0f, 6.0f);
    };
    int syncState = processor.syncState.load();
    drawLed(ledX,      colGreen, true);
    drawLed(ledX + 14, colAccent, syncState == 2);
    drawLed(ledX + 28, colBorder, false);
    drawLed(ledX + 42, colGold,   syncState == 2);
    drawLed(ledX + 56, colBorder, false);
    drawLed(ledX + 70, colAccent, true);

    // VU meter
    float vu = std::min(1.0f, processor.vuLevel.load());
    int vuX = W - MARGIN - 180;
    int vuY = r.getCentreY() - 3;
    int vuW = 150;
    g.setColour(colBorder);
    g.fillRoundedRectangle((float)vuX, (float)vuY, (float)vuW, 6.0f, 3.0f);
    int fillW = (int)(vu * vuW);
    if (fillW > 0) {
        g.setGradientFill(juce::ColourGradient(colAccent, (float)vuX, 0,
                                                colGold, (float)(vuX + vuW), 0, false));
        g.fillRoundedRectangle((float)vuX, (float)vuY, (float)fillW, 6.0f, 3.0f);
    }
    g.setColour(colMuted);
    g.setFont(juce::Font(juce::FontOptions().withHeight(8.5f)));
    g.drawText("OUT", vuX - 28, vuY - 1, 26, 8, juce::Justification::centredRight);
}

// ── Signal strip ───────────────────────────────────────────────
void VoltaResonanceEditor::paintSignalStrip(juce::Graphics& g)
{
    auto r = signalStripBounds();
    g.setColour(juce::Colour(0xff060611));
    g.fillRect(r);
    g.setColour(colBorder);
    g.drawHorizontalLine(r.getBottom() - 1, 0.0f, (float)W);

    g.setColour(colAccent.withAlpha(0.5f));
    g.setFont(juce::Font(juce::FontOptions().withHeight(9.0f)));
    g.drawText("SIG", MARGIN, r.getY(), 24, SIG_H, juce::Justification::centred);

    const int barW   = 3, barGap = 2;
    const int barsX  = MARGIN + 30;
    const int barsEndX = W - MARGIN - 80;
    const int maxH   = SIG_H - 8;
    const int barY0  = r.getCentreY();
    int barsToRender = std::min(NUM_BARS, (barsEndX - barsX) / (barW + barGap));

    for (int i = 0; i < barsToRender; ++i) {
        float h = std::max(1.0f, barHeights[i] * maxH);
        float alpha = 0.35f + barHeights[i] * 0.65f;
        g.setColour(colAccent.withAlpha(alpha));
        g.fillRoundedRectangle((float)(barsX + i * (barW + barGap)),
                                (float)(barY0 - h / 2), (float)barW, h, 1.5f);
    }

    // Live BPM from DAW (placeholder — reads whatever bpm the seq has)
    g.setColour(colGold);
    g.setFont(juce::Font(juce::FontOptions().withHeight(12.0f).withStyle("Bold")));
    g.drawText("BPM", W - MARGIN - 72, r.getY(), 72, SIG_H, juce::Justification::centredRight);
}

// ── Pads ───────────────────────────────────────────────────────
void VoltaResonanceEditor::paintPads(juce::Graphics& g)
{
    auto nowMs = (int64_t)juce::Time::currentTimeMillis();
    constexpr int64_t LIT_MS = 150;

    for (int i = 0; i < DrumSynth::NUM_VOICES; ++i)
    {
        auto b = padBounds(i).toFloat();
        int64_t last = processor.lastTriggerMs[i].load();
        bool isLit   = (nowMs - last) < LIT_MS;
        float litFade = isLit ? std::max(0.0f, 1.0f - (float)(nowMs - last) / LIT_MS) : 0.0f;

        // Outer glow
        if (litFade > 0.01f) {
            g.setColour(colAccent.withAlpha(litFade * 0.12f));
            g.fillRoundedRectangle(b.expanded(5.0f), 10.0f);
        }

        // Background
        juce::Colour fill = isLit
            ? juce::Colour(0xff131322).interpolatedWith(colAccent.withAlpha(0.22f), litFade)
            : juce::Colour(0xff0b0b1a);
        g.setColour(fill);
        g.fillRoundedRectangle(b, 6.0f);

        // Top shimmer
        g.setColour(juce::Colours::white.withAlpha(0.025f));
        g.fillRoundedRectangle(b.withHeight(b.getHeight() * 0.45f), 6.0f);

        // Border
        juce::Colour border = isLit ? colAccent.withAlpha(0.5f + litFade * 0.5f) : colBorder;
        g.setColour(border);
        g.drawRoundedRectangle(b, 6.0f, isLit ? 1.5f : 1.0f);

        // Label top-left
        g.setColour(isLit ? colAccent : colMuted);
        g.setFont(juce::Font(juce::FontOptions().withHeight(9.0f)));
        g.drawText(PAD_LABELS[i], (int)b.getX() + 7, (int)b.getY() + 6, 36, 11,
                   juce::Justification::centredLeft);

        // MIDI note top-right
        g.setColour(colMuted.withAlpha(0.55f));
        g.setFont(juce::Font(juce::FontOptions().withHeight(9.0f)));
        g.drawText(juce::String(DrumSynth::MIDI_NOTES[i]),
                   (int)b.getRight() - 26, (int)b.getY() + 6, 22, 11,
                   juce::Justification::centredRight);

        // Drum name centred
        g.setColour(isLit ? colAccent : colText);
        g.setFont(juce::Font(juce::FontOptions().withHeight(12.5f).withStyle("Bold")));
        g.drawText(PAD_NAMES[i], (int)b.getX(), (int)b.getCentreY() - 9,
                   (int)b.getWidth(), 18, juce::Justification::centred);

        // Active dot
        if (litFade > 0.1f) {
            g.setColour(colAccent.withAlpha(litFade));
            g.fillEllipse(b.getCentreX() - 3.0f, b.getBottom() - 11.0f, 6.0f, 6.0f);
        }

        // Sample load button area at pad bottom
        auto loadBtn = padLoadButtonBounds(i).toFloat();
        bool hasSample = processor.customSampleLoaded[i];
        if (hasSample) {
            // Name area (left portion, leaving 14px for CLR)
            auto nameArea = loadBtn.withWidth(loadBtn.getWidth() - 16.0f);
            g.setColour(colGreen.withAlpha(0.15f));
            g.fillRoundedRectangle(nameArea, 2.0f);
            g.setColour(colGreen);
            g.setFont(juce::Font(juce::FontOptions().withHeight(8.5f)));
            juce::String sname(processor.samplePlayers[i].name.c_str());
            g.drawText(sname.substring(0, 7), nameArea, juce::Justification::centred, true);
            // CLR button
            auto clrArea = loadBtn.withLeft(loadBtn.getRight() - 14.0f);
            g.setColour(juce::Colour(0xff330000));
            g.fillRoundedRectangle(clrArea, 2.0f);
            g.setColour(juce::Colour(0xffff4444));
            g.setFont(juce::Font(juce::FontOptions().withHeight(9.0f).withStyle("Bold")));
            g.drawText("X", clrArea, juce::Justification::centred);
        } else {
            g.setColour(colSurface2.withAlpha(0.6f));
            g.fillRoundedRectangle(loadBtn, 2.0f);
            g.setColour(colMuted.withAlpha(0.5f));
            g.setFont(juce::Font(juce::FontOptions().withHeight(8.5f)));
            g.drawText("LOAD", loadBtn, juce::Justification::centred);
        }
    }
}

// ── Sequencer ──────────────────────────────────────────────────
void VoltaResonanceEditor::paintSequencer(juce::Graphics& g)
{
    auto r = seqBounds();

    // Panel
    g.setColour(colSurface);
    g.fillRect(r);
    g.setColour(colBorder);
    g.drawHorizontalLine(r.getY(), 0.0f, (float)W);
    g.drawHorizontalLine(r.getBottom() - 1, 0.0f, (float)W);

    // Left accent bar
    g.setColour(colAccent.withAlpha(0.8f));
    g.fillRect(0, r.getY() + 6, 3, 22);

    int hdrY = r.getY();
    int curStep = processor.getSeqCurrentStep();

    // ── Header bar (top 18px) ─────────────────────────────
    // "SEQ" label
    g.setColour(colAccent);
    g.setFont(juce::Font(juce::FontOptions().withHeight(9.5f).withStyle("Bold")));
    g.drawText("SEQ", MARGIN + 8, hdrY + 5, 28, 12, juce::Justification::centredLeft);

    // Current step indicator: small filled square + number
    g.setColour(colAccent);
    g.fillRoundedRectangle((float)(MARGIN + 42), (float)(hdrY + 6), 8.0f, 8.0f, 1.5f);
    g.setFont(juce::Font(juce::FontOptions().withHeight(9.5f).withStyle("Bold")));
    g.drawText(juce::String::formatted("%02d", curStep + 1),
               MARGIN + 54, hdrY + 4, 24, 12, juce::Justification::centredLeft);

    // Beat group numbers above grid (offset into 2nd row of SEQ_HDR_H)
    int beatY = hdrY + 19;
    static const char* beatLabels[4] = { "1-4", "5-8", "9-12", "13-16" };
    for (int grp = 0; grp < 4; ++grp)
    {
        int x0 = stepCellX(grp * 4);
        int x1 = stepCellX(grp * 4 + 3) + SEQ_CELL_W;
        g.setColour(colMuted.withAlpha(0.6f));
        g.setFont(juce::Font(juce::FontOptions().withHeight(8.5f)));
        g.drawText(beatLabels[grp], x0, beatY, x1 - x0, 12, juce::Justification::centred);
    }

    // BPM display
    g.setColour(colGold);
    g.setFont(juce::Font(juce::FontOptions().withHeight(9.5f)));
    g.drawText("120 BPM", W / 2 - 32, hdrY + 5, 64, 12, juce::Justification::centred);

    // PLAY / STOP button (drawn as shape + label)
    auto playBtn = seqPlayButtonBounds();
    bool seqPlaying = processor.internalSeqPlaying.load();
    g.setColour(seqPlaying ? colAccent.withAlpha(0.12f) : colSurface2);
    g.fillRoundedRectangle(playBtn.toFloat(), 3.0f);
    g.setColour(seqPlaying ? colAccent.withAlpha(0.5f) : colBorder);
    g.drawRoundedRectangle(playBtn.toFloat(), 3.0f, 1.0f);

    // Draw triangle (play) or square (stop)
    float bx = (float)playBtn.getX() + 6.0f;
    float by = (float)playBtn.getCentreY();
    if (seqPlaying) {
        g.setColour(colAccent);
        g.fillRect((int)bx, (int)by - 4, 6, 8);
    } else {
        juce::Path tri;
        tri.addTriangle(bx, by - 5.0f, bx, by + 5.0f, bx + 9.0f, by);
        g.setColour(colMuted);
        g.fillPath(tri);
    }
    g.setColour(seqPlaying ? colAccent : colMuted);
    g.setFont(juce::Font(juce::FontOptions().withHeight(9.0f).withStyle("Bold")));
    g.drawText(seqPlaying ? "STOP" : "PLAY",
               playBtn.getX() + 18, playBtn.getY(), playBtn.getWidth() - 18,
               playBtn.getHeight(), juce::Justification::centredLeft);

    // Separator
    g.setColour(colBorder);
    g.drawHorizontalLine(hdrY + SEQ_HDR_H - 1, (float)MARGIN, (float)(W - MARGIN));

    // ── Grid ──────────────────────────────────────────────
    const int gridTop    = r.getY() + SEQ_HDR_H;
    const int gridBottom = gridTop + StepSequencer::VOICES * SEQ_ROW_H;

    // Beat group backgrounds
    for (int grp = 0; grp < 4; ++grp)
    {
        int x0 = stepCellX(grp * 4);
        int x1 = stepCellX(grp * 4 + 3) + SEQ_CELL_W;
        juce::Colour bg = (grp % 2 == 0) ? colSurface : colSurface2;
        g.setColour(bg.withAlpha(0.35f));
        g.fillRect(x0, gridTop, x1 - x0, gridBottom - gridTop);
    }

    // Playhead column tint
    {
        int px = stepCellX(curStep);
        g.setColour(colAccent.withAlpha(0.06f));
        g.fillRect(px, gridTop, SEQ_CELL_W, gridBottom - gridTop);
        g.setColour(colAccent.withAlpha(0.25f));
        g.fillRect(px, gridTop, SEQ_CELL_W, 2);
    }

    // Voice rows
    for (int v = 0; v < StepSequencer::VOICES; ++v)
    {
        int rowY = gridTop + v * SEQ_ROW_H;

        // Voice label
        g.setColour(colMuted);
        g.setFont(juce::Font(juce::FontOptions().withHeight(9.0f)));
        g.drawText(SEQ_VOICE_LABELS[v],
                   MARGIN + 6, rowY, SEQ_LABEL_W - 10, SEQ_CELL_H,
                   juce::Justification::centredLeft);

        for (int s = 0; s < StepSequencer::STEPS; ++s)
        {
            auto cell = stepCellBounds(v, s).toFloat();
            bool active    = processor.getSequencerStep(v, s);
            bool isCurrent = (s == curStep);
            int  ownerColor = processor.stepOwnerColor[v][s];

            juce::Colour cellCol = (ownerColor >= 0 && ownerColor < 6)
                                    ? userColor(ownerColor) : colAccent;

            if (active) {
                // Glow
                g.setColour(cellCol.withAlpha(0.14f));
                g.fillRoundedRectangle(cell.expanded(2.0f), 3.0f);
                // Fill gradient
                g.setGradientFill(juce::ColourGradient(
                    cellCol, cell.getCentreX(), cell.getY(),
                    cellCol.darker(0.4f), cell.getCentreX(), cell.getBottom(), false));
                g.fillRoundedRectangle(cell, 2.0f);
                g.setColour(cellCol.withAlpha(0.7f));
                g.drawRoundedRectangle(cell, 2.0f, 1.0f);
            } else {
                g.setColour(isCurrent ? colSurface2.brighter(0.08f) : colSurface2.darker(0.25f));
                g.fillRoundedRectangle(cell, 2.0f);
                g.setColour(isCurrent ? colAccent.withAlpha(0.35f) : colBorder);
                g.drawRoundedRectangle(cell, 2.0f, 0.8f);
            }
        }
    }

    // User colour swatches — right side of header (colour picker)
    // 6 swatches: 10px each, 2px gap → 70px total, right-aligned
    const int swatchStartX = W - MARGIN - 70;  // = 616
    for (int c = 0; c < 6; ++c)
    {
        auto swatch = juce::Rectangle<float>((float)(swatchStartX + c * 12),
                                              (float)(hdrY + 5), 10.0f, 10.0f);
        g.setColour(userColor(c));
        g.fillRoundedRectangle(swatch, 2.0f);
        if (c == localColorIndex) {
            g.setColour(juce::Colours::white);
            g.drawRoundedRectangle(swatch, 2.0f, 1.5f);
        }
    }
    // "YOU" label before swatches
    g.setColour(colMuted);
    g.setFont(juce::Font(juce::FontOptions().withHeight(8.0f)));
    g.drawText("YOU", swatchStartX - 30, hdrY + 5, 28, 10, juce::Justification::centredRight);
}

// ── Sync panel ─────────────────────────────────────────────────
void VoltaResonanceEditor::paintSyncPanel(juce::Graphics& g)
{
    auto r = syncPanelBounds();

    g.setColour(colSurface.withAlpha(0.9f));
    g.fillRect(r);
    g.setColour(colBorder);
    g.drawHorizontalLine(r.getY(), 0.0f, (float)W);
    g.drawHorizontalLine(r.getBottom() - 1, 0.0f, (float)W);

    g.setColour(colAccent);
    g.fillRect(0, r.getY() + 8, 3, SYNC_H - 16);

    // "VOLTA SYNC" label
    g.setColour(colAccent);
    g.setFont(juce::Font(juce::FontOptions().withHeight(9.5f)));
    g.drawText("VOLTA SYNC", MARGIN + 8, r.getY() + 10, 90, 13,
               juce::Justification::centredLeft);

    // Status row
    int syncState = processor.syncState.load();
    int peers     = processor.syncPeerCount.load();

    juce::Colour dotCol = syncState == 2 ? colGreen :
                          syncState == 1 ? colGold  : colMuted;
    juce::String statusText = syncState == 2
        ? "CONNECTED - " + juce::String(peers) + " peer" + (peers != 1 ? "s" : "")
        : syncState == 1 ? "CONNECTING..."
        : "OFFLINE";

    int dotX = MARGIN + 8, dotY = r.getY() + 36;
    if (syncState == 2) {
        g.setColour(dotCol.withAlpha(0.2f));
        g.fillEllipse((float)(dotX - 3), (float)(dotY - 3), 12.0f, 12.0f);
    }
    g.setColour(dotCol);
    g.fillEllipse((float)dotX, (float)dotY, 6.0f, 6.0f);
    g.setFont(juce::Font(juce::FontOptions().withHeight(9.5f)));
    g.drawText(statusText, MARGIN + 18, dotY - 1, 200, 12, juce::Justification::centredLeft);

    // User identity row
    int row2Y = r.getY() + 46;
    // "YOU:" label
    g.setColour(colMuted);
    g.setFont(juce::Font(juce::FontOptions().withHeight(9.0f)));
    g.drawText("YOU:", MARGIN + 8, row2Y + 2, 32, 18, juce::Justification::centredLeft);

    // Selected colour swatch
    g.setColour(userColor(localColorIndex));
    g.fillRoundedRectangle((float)(MARGIN + 44), (float)(row2Y + 4), 14.0f, 14.0f, 2.5f);

    // 6 colour pickers in sync panel
    for (int c = 0; c < 6; ++c) {
        auto sw = juce::Rectangle<float>((float)(W - MARGIN - 86 + c * 14), (float)(row2Y + 3),
                                          12.0f, 16.0f);
        g.setColour(userColor(c));
        g.fillRoundedRectangle(sw, 2.0f);
        if (c == localColorIndex) {
            g.setColour(juce::Colours::white.withAlpha(0.8f));
            g.drawRoundedRectangle(sw, 2.0f, 1.5f);
        }
    }

    // Peer count badge
    if (syncState == 2 && peers > 0) {
        int bx = W - MARGIN - 170, by = r.getY() + 8;
        g.setColour(colAccent.withAlpha(0.1f));
        g.fillRoundedRectangle((float)bx, (float)by, 80.0f, 20.0f, 4.0f);
        g.setColour(colAccent.withAlpha(0.35f));
        g.drawRoundedRectangle((float)bx, (float)by, 80.0f, 20.0f, 4.0f, 1.0f);
        g.setColour(colAccent);
        g.setFont(juce::Font(juce::FontOptions().withHeight(9.0f)));
        g.drawText(juce::String(peers) + "/" + juce::String(peers) + " ONLINE",
                   bx, by, 80, 20, juce::Justification::centred);
    }
}

// ── Knobs ──────────────────────────────────────────────────────
void VoltaResonanceEditor::paintKnobs(juce::Graphics& g)
{
    auto r = knobAreaBounds();
    g.setColour(colSurface);
    g.fillRect(r);
    g.setColour(colBorder);
    g.drawHorizontalLine(r.getBottom() - 1, 0.0f, (float)W);

    for (int i = 0; i < 4; ++i)
    {
        auto b   = knobBounds(i);
        float val = (i == 0) ? processor.masterLevel.load() : knobValues[i];

        float cx = (float)b.getCentreX(), cy = (float)(b.getY() + 22);
        float radius = 16.0f;

        float startAngle = juce::MathConstants<float>::pi * 1.25f;
        float endAngle   = juce::MathConstants<float>::pi * 2.75f;
        float fillAngle  = startAngle + val * (endAngle - startAngle);

        juce::Path trackPath;
        trackPath.addCentredArc(cx, cy, radius, radius, 0, startAngle, endAngle, true);
        g.setColour(colBorder);
        g.strokePath(trackPath, juce::PathStrokeType(2.5f,
                     juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        juce::Path fillPath;
        fillPath.addCentredArc(cx, cy, radius, radius, 0, startAngle, fillAngle, true);
        g.setColour(i == 0 ? colAccent : colGold);
        g.strokePath(fillPath, juce::PathStrokeType(2.5f,
                     juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        g.setGradientFill(juce::ColourGradient(
            juce::Colour(0xff202038), cx - radius * 0.4f, cy - radius * 0.4f,
            juce::Colour(0xff080810), cx, cy + radius, false));
        g.fillEllipse(cx - radius + 1.5f, cy - radius + 1.5f,
                      (radius - 1.5f) * 2, (radius - 1.5f) * 2);
        g.setColour(colBorder);
        g.drawEllipse(cx - radius + 1.5f, cy - radius + 1.5f,
                      (radius - 1.5f) * 2, (radius - 1.5f) * 2, 1.0f);

        float angle = startAngle + val * (endAngle - startAngle);
        float dotX  = cx + (radius - 5.5f) * std::sin(angle);
        float dotY  = cy - (radius - 5.5f) * std::cos(angle);
        g.setColour(i == 0 ? colAccent : colGold);
        g.fillEllipse(dotX - 2.0f, dotY - 2.0f, 4.0f, 4.0f);

        g.setColour(colMuted);
        g.setFont(juce::Font(juce::FontOptions().withHeight(9.0f)));
        g.drawText(KNOB_LABELS[i], b.getX(), b.getBottom() - 14, b.getWidth(), 12,
                   juce::Justification::centred);
    }
}

// ── Footer ─────────────────────────────────────────────────────
void VoltaResonanceEditor::paintFooter(juce::Graphics& g)
{
    auto r = footerBounds();
    g.setColour(colSurface);
    g.fillRect(r);
    g.setColour(colBorder);
    g.drawHorizontalLine(r.getY(), 0.0f, (float)W);

    g.setColour(colMuted);
    g.setFont(juce::Font(juce::FontOptions().withHeight(9.5f)));
    g.drawText("VOLTA RESONANCE  |  Pack 01  |  v1.1",
               MARGIN, r.getY(), 280, FOOTER_H, juce::Justification::centredLeft);

    int sc = processor.syncState.load();
    juce::String peerStr;
    if (sc == 2)      peerStr = "SYNC: " + juce::String(processor.syncPeerCount.load()) + " peers connected";
    else if (sc == 1) peerStr = "SYNC: connecting...";
    if (peerStr.isNotEmpty()) {
        g.setColour(colAccent.withAlpha(0.7f));
        g.drawText(peerStr, W - MARGIN - 240, r.getY(), 240, FOOTER_H,
                   juce::Justification::centredRight);
    }
}

// ── Mouse ──────────────────────────────────────────────────────
void VoltaResonanceEditor::mouseDown(const juce::MouseEvent& e)
{
    float scale = getScale();
    juce::Point<int> pos { juce::roundToInt(e.position.x / scale),
                           juce::roundToInt(e.position.y / scale) };

    // ── Sequencer area ────────────────────────────────────────
    {
        auto seq = seqBounds();
        if (seq.contains(pos))
        {
            // PLAY/STOP button
            if (seqPlayButtonBounds().contains(pos)) {
                processor.internalSeqPlaying.store(!processor.internalSeqPlaying.load());
                repaint();
                return;
            }

            // Colour swatches in seq header
            {
                const int swatchStartX = W - MARGIN - 70;
                int seqHdrY = seq.getY();
                for (int c = 0; c < 6; ++c) {
                    auto swatch = juce::Rectangle<int>(swatchStartX + c * 12, seqHdrY + 5, 10, 10);
                    if (swatch.contains(pos)) {
                        localColorIndex = c;
                        repaint();
                        return;
                    }
                }
            }

            // Step cell hit test
            for (int v = 0; v < StepSequencer::VOICES; ++v)
                for (int st = 0; st < StepSequencer::STEPS; ++st)
                    if (stepCellBounds(v, st).contains(pos)) {
                        bool newVal = !processor.getSequencerStep(v, st);
                        processor.setSequencerStep(v, st, newVal);
                        processor.stepOwnerColor[v][st] = newVal ? localColorIndex : -1;
                        processor.voltaSync.sendStepChange(v, st, newVal,
                            localColorIndex, userNameInput.getText());
                        repaint();
                        return;
                    }
            return;
        }
    }

    // ── Sync panel colour swatches ────────────────────────────
    {
        auto sync = syncPanelBounds();
        int row2Y = sync.getY() + 46;
        for (int c = 0; c < 6; ++c) {
            auto sw = juce::Rectangle<int>(W - MARGIN - 86 + c * 14, row2Y + 3, 12, 16);
            if (sw.contains(pos)) {
                localColorIndex = c;
                repaint();
                return;
            }
        }
    }

    // ── Pad CLR button (only when sample loaded) ──────────────
    for (int i = 0; i < DrumSynth::NUM_VOICES; ++i) {
        if (!processor.customSampleLoaded[i]) continue;
        if (padClearButtonBounds(i).contains(pos)) {
            processor.clearSample(i);
            repaint();
            return;
        }
    }

    // ── Pad LOAD button ───────────────────────────────────────
    for (int i = 0; i < DrumSynth::NUM_VOICES; ++i) {
        if (padLoadButtonBounds(i).contains(pos)) {
            const int voice = i;
            fileChooser = std::make_unique<juce::FileChooser>(
                "Load Sample for " + juce::String(PAD_NAMES[voice]),
                juce::File::getSpecialLocation(juce::File::userDesktopDirectory),
                "*.wav;*.aiff;*.mp3;*.flac");
            fileChooser->launchAsync(
                juce::FileBrowserComponent::openMode |
                juce::FileBrowserComponent::canSelectFiles,
                [this, voice](const juce::FileChooser& fc) {
                    auto file = fc.getResult();
                    if (file.existsAsFile()) {
                        processor.loadSample(voice, file);
                        if (processor.voltaSync.isConnected())
                            processor.sendSampleSync(voice);
                        repaint();
                    }
                });
            return;
        }
    }

    // ── Knob hit test ──────────────────────────────────────────
    for (int i = 0; i < 4; ++i) {
        auto b = knobBounds(i);
        float cx = (float)b.getCentreX(), cy = (float)(b.getY() + 22);
        float px = e.position.x / scale;
        float py = e.position.y / scale;
        float dx = px - cx, dy = py - cy;
        if (dx * dx + dy * dy <= 18.0f * 18.0f) {
            dragKnob     = i;
            dragStartY   = py;
            dragStartVal = (i == 0) ? processor.masterLevel.load() : knobValues[i];
            return;
        }
    }

    // ── Pad hit test ───────────────────────────────────────────
    for (int i = 0; i < DrumSynth::NUM_VOICES; ++i) {
        if (padBounds(i).contains(pos)) {
            processor.pendingTrigger[i].store(true);
            processor.lastTriggerMs[i].store((int64_t)juce::Time::currentTimeMillis());
            repaint();
            return;
        }
    }
}

void VoltaResonanceEditor::mouseDrag(const juce::MouseEvent& e)
{
    if (dragKnob < 0) return;
    float scale  = getScale();
    float delta  = (dragStartY - e.position.y / scale) / 120.0f;
    float newVal = std::max(0.0f, std::min(1.0f, dragStartVal + delta));
    if (dragKnob == 0) {
        processor.masterLevel.store(newVal);
    } else if (dragKnob == 1) {
        knobValues[1] = newVal;
        processor.paramDecay.store(newVal);
    } else if (dragKnob == 2) {
        knobValues[2] = newVal;
        processor.paramTone.store(newVal);
    } else if (dragKnob == 3) {
        knobValues[3] = newVal;
        processor.paramDrive.store(newVal);
    }
    repaint();
}

void VoltaResonanceEditor::mouseUp(const juce::MouseEvent&)
{
    dragKnob = -1;
}

// ── Timer ──────────────────────────────────────────────────────
void VoltaResonanceEditor::timerCallback()
{
    float vu = std::min(1.0f, processor.vuLevel.load());
    juce::Random rng;
    for (int i = 0; i < NUM_BARS; ++i) {
        float energy = vu * (0.4f + rng.nextFloat() * 0.6f);
        if (rng.nextFloat() < 0.3f)
            barTargets[i] = std::max(0.05f, energy + (rng.nextFloat() - 0.5f) * 0.3f);
        float speed = barHeights[i] < barTargets[i] ? 0.35f : 0.15f;
        barHeights[i] += (barTargets[i] - barHeights[i]) * speed;
    }

    auto state = processor.voltaSync.getState();
    connectBtn.setButtonText(state != VoltaSync::State::Disconnected ? "DISCONNECT" : "CONNECT");

    repaint();
}
