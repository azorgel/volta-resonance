#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include <memory>

class VoltaResonanceEditor : public juce::AudioProcessorEditor,
                             private juce::Timer
{
public:
    explicit VoltaResonanceEditor(VoltaResonanceProcessor&);
    ~VoltaResonanceEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;

private:
    void timerCallback() override;

    // Sub-paint helpers
    void paintBackground(juce::Graphics&);
    void paintHeader(juce::Graphics&);
    void paintSignalStrip(juce::Graphics&);
    void paintPads(juce::Graphics&);
    void paintSequencer(juce::Graphics&);
    void paintSyncPanel(juce::Graphics&);
    void paintKnobs(juce::Graphics&);
    void paintFooter(juce::Graphics&);

    // Logo helper
    void drawVoltaLogo(juce::Graphics&, float x, float y, float scale, juce::Colour col);

    // Layout helpers
    juce::Rectangle<int> padBounds(int idx) const;
    juce::Rectangle<int> headerBounds()      const;
    juce::Rectangle<int> signalStripBounds() const;
    juce::Rectangle<int> padAreaBounds()     const;
    juce::Rectangle<int> seqBounds()         const;
    juce::Rectangle<int> seqPlayButtonBounds() const;
    juce::Rectangle<int> padLoadButtonBounds(int idx) const;
    juce::Rectangle<int> padClearButtonBounds(int idx) const;
    juce::Rectangle<int> syncPanelBounds()   const;
    juce::Rectangle<int> knobAreaBounds()    const;
    juce::Rectangle<int> footerBounds()      const;
    juce::Rectangle<int> knobBounds(int idx) const;
    juce::Rectangle<int> stepCellBounds(int voice, int step) const;
    int                  stepCellX(int step) const;
    float                getScale() const { return (float)getWidth() / (float)W; }

    VoltaResonanceProcessor& processor;

    // Network UI
    juce::TextEditor  sessionInput;
    juce::TextButton  connectBtn { "CONNECT" };

    // User identity
    juce::TextEditor  userNameInput;
    int               localColorIndex = 0;   // 0-5, into USER_COLORS palette

    // File chooser (kept alive during async dialog)
    std::unique_ptr<juce::FileChooser> fileChooser;

    // Knob drag state
    int   dragKnob    = -1;
    float dragStartY  = 0.0f;
    float dragStartVal = 0.0f;
    float knobValues[4] = { 0.85f, 0.5f, 1.0f, 0.0f };

    // Waveform bars (animated)
    static constexpr int NUM_BARS = 28;
    float barHeights[NUM_BARS] = {};
    float barTargets[NUM_BARS] = {};

    // Colours
    const juce::Colour colBg       { 0xff04040d };
    const juce::Colour colSurface  { 0xff0b0b16 };
    const juce::Colour colSurface2 { 0xff131320 };
    const juce::Colour colBorder   { 0xff1c1c2e };
    const juce::Colour colAccent   { 0xff00d4ff };
    const juce::Colour colGold     { 0xffffaa00 };
    const juce::Colour colGreen    { 0xff00ff99 };
    const juce::Colour colText     { 0xffeaeaf4 };
    const juce::Colour colMuted    { 0xff545480 };

    // 6-colour user palette (also declared in .cpp anonymous namespace)
    static juce::Colour userColor(int idx);

    // Static label data
    static const char* PAD_NAMES[DrumSynth::NUM_VOICES];
    static const char* PAD_LABELS[DrumSynth::NUM_VOICES];
    static const char* SEQ_VOICE_LABELS[DrumSynth::NUM_VOICES];
    static const char* KNOB_LABELS[4];

    // ── Layout constants ───────────────────────────────────
    static constexpr int W          = 700;
    static constexpr int H          = 730;
    static constexpr int HDR_H      = 60;
    static constexpr int SIG_H      = 30;
    static constexpr int PAD_ROWS   = 3;
    static constexpr int PAD_COLS   = 4;
    static constexpr int PAD_AREA_H = 190;
    static constexpr int SEQ_H      = 238;
    static constexpr int SEQ_HDR_H  = 36;   // header + user-identity row
    static constexpr int SEQ_ROW_H  = 16;   // 11px cell + 5px gap
    static constexpr int SEQ_CELL_H = 11;
    static constexpr int SEQ_CELL_W = 36;
    static constexpr int SEQ_LABEL_W = 50;
    static constexpr int SYNC_H     = 88;   // expanded for user identity row
    static constexpr int KNOB_H     = 95;   // fills remaining space
    static constexpr int FOOTER_H   = 27;
    static constexpr int MARGIN     = 14;
    static constexpr int GAP        = 9;

    // Total sanity: 60+30+190+238+88+95+27 = 728 → close enough, footer at H-27
    static_assert(HDR_H + SIG_H + PAD_AREA_H + SEQ_H + SYNC_H + KNOB_H + FOOTER_H <= H,
                  "Layout height overflow");

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VoltaResonanceEditor)
};
