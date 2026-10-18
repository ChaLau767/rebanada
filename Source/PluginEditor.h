// Rebanada — "Rack" interface
// Copyright (C) 2026 Lautaro Martín Vieyra
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

namespace rebanada::rack
{
namespace colours
{
    const juce::Colour ground      { 0xff1c1a18 };
    const juce::Colour panel       { 0xff26231f };
    const juce::Colour panelEdge   { 0xff33302b };
    const juce::Colour screen      { 0xff14100b };
    const juce::Colour screenEdge  { 0xff3a2e1e };
    const juce::Colour text        { 0xffede6d6 };
    const juce::Colour sub         { 0xffa89f8e };
    const juce::Colour faint       { 0xff6f675b };
    const juce::Colour accent      { 0xfff0a640 };
    const juce::Colour waveDim     { 0xff6e5634 };
    const juce::Colour waveMuted   { 0xff3d3121 };
    const juce::Colour line        { 0xff3b3631 };
    const juce::Colour keyWhite    { 0xffe9e1d0 };
    const juce::Colour keyBlack    { 0xff2e2a26 };
    const juce::Colour keyBlackEdge{ 0xff45403a };
    const juce::Colour reverse     { 0xff9db4ff };
}

juce::Font sans (float height);    // Archivo regular
juce::Font semi (float height);    // Archivo semibold: labels
juce::Font heavy (float height);   // Archivo extrabold: logo
juce::Font mono (float height);    // IBM Plex Mono: values
juce::Font monoBold (float height);

class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    LookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool over, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override { return mono (12.5f); }
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getPopupMenuFont() override { return sans (14.0f); }
    void drawTooltip (juce::Graphics&, const juce::String&, int w, int h) override;
    juce::Rectangle<int> getTooltipBounds (const juce::String&, juce::Point<int>, juce::Rectangle<int>) override;
};

//==============================================================================
// A rotary control with its name and value underneath.
class Knob : public juce::Component
{
public:
    Knob (const FloatSpec&, bool bipolar);
    void paint (juce::Graphics&) override;
    void resized() override;
    void setLanguage (Lang);
    void attach (juce::AudioProcessorValueTreeState&, const juce::String& paramId);   // for host parameters
    juce::Slider slider;
    std::function<void (float)> onChange;                  // for slice settings
    static constexpr int width = 72, dial = 54, height = 88;

private:
    const FloatSpec& spec;
    Lang lang = Lang::en;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

// A row (or column) of mutually exclusive buttons.
class Segmented : public juce::Component, public juce::SettableTooltipClient
{
public:
    Segmented (std::vector<const char*> keys, bool vertical, std::function<int()> get, std::function<void (int)> set);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void setLanguage (Lang l) { lang = l; repaint(); }
    void setPlainLabels (bool plain) { plainLabels = plain; }
    float fontSize = 12.5f;
    bool monoFont = false, capsLabels = false;

private:
    int indexAt (juce::Point<float>) const;
    std::vector<const char*> keys;
    bool vertical, plainLabels = false;
    std::function<int()> get;
    std::function<void (int)> set;
    Lang lang = Lang::en;
};

// The eight loop buttons.
class SlotStrip : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit SlotStrip (RebanadaProcessor&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void update (const Telemetry&, int editSlot, const std::array<bool, maxSlots>& loaded, Lang);
    std::function<void (int)> onLoadRequest, onClearRequest;

private:
    int slotAt (juce::Point<float>) const;
    RebanadaProcessor& processor;
    Telemetry tele;
    int edit = 0;
    std::array<bool, maxSlots> loaded {};
    Lang lang = Lang::en;
    int blink = 0;
};

// Tempo readout: host tempo, internal tempo (drag) or free rate (drag).
class TempoDisplay : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit TempoDisplay (RebanadaProcessor&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void update (const Telemetry&, Lang);
    bool output = false;     // shows and drags the output level instead

private:
    juce::RangedAudioParameter* target() const;
    bool freeMode() const;
    RebanadaProcessor& processor;
    Telemetry tele;
    Lang lang = Lang::en;
    float dragStart = 0.0f;
    juce::RangedAudioParameter* dragging = nullptr;
};

// The waveform with its slices.
class WaveView : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit WaveView (RebanadaProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override { peaksFor = nullptr; }
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseMagnify (const juce::MouseEvent&, float scale) override;
    void setInfo (int slot, const RebanadaProcessor::SlotInfo&);
    void update (const Telemetry&, Lang);
    void zoomAround (double frame, double factor);   // factor < 1 zooms in
    std::function<void()> onSelectionChange, onLoadRequest;
    static constexpr int labelHeight = 22, barHeight = 12;

private:
    juce::Rectangle<float> waveArea() const;
    juce::Rectangle<float> scrollArea() const;
    double totalFrames() const;
    double visStart() const;
    double visLen() const;
    void panBy (double frames);
    float xFor (int frame) const;
    int frameFor (float x) const;
    int markerAt (juce::Point<float>) const;   // marker index (>0), or -1
    int sliceAt (float x) const;
    void computePeaks();

    RebanadaProcessor& processor;
    RebanadaProcessor::SlotInfo info;
    int slot = 0;
    const Sample* peaksFor = nullptr;
    std::vector<float> peaks;
    float loudest = 1.0f;
    double peaksStart = -1.0, peaksLen = -1.0;
    double viewStart = 0.0, viewLen = 0.0;     // viewLen 0: the whole sample
    bool scrolling = false;
    double scrollGrab = 0.0;
    Telemetry tele;
    Lang lang = Lang::en;
    int dragMarker = -1, hoverMarker = -1;
    bool dragMoved = false;
};

// Drag handle that hands a MIDI file to the host.
class MidiDragger : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit MidiDragger (RebanadaProcessor& p) : processor (p) { setMouseCursor (juce::MouseCursor::DraggingHandCursor); }
    void paint (juce::Graphics&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override { dragging = false; }
    void setLanguage (Lang l) { lang = l; repaint(); }
    int slot = 0;

private:
    RebanadaProcessor& processor;
    Lang lang = Lang::en;
    bool dragging = false;
};

// Keys mapped to slices.
class KeyStrip : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit KeyStrip (RebanadaProcessor&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void update (const Telemetry&, int slot, int numSlices, int selected, Lang);
    std::function<void (int)> onPick;
    static constexpr int perPage = 32;

private:
    juce::Rectangle<float> keyArea() const;
    int keyAt (juce::Point<float>) const;
    int first() const { return (selected / perPage) * perPage; }
    RebanadaProcessor& processor;
    Telemetry tele;
    int slot = 0, numSlices = 0, selected = 0, held = -1;
    Lang lang = Lang::en;
};

//==============================================================================
class Content : public juce::Component, public juce::FileDragAndDropTarget, private juce::Timer
{
public:
    explicit Content (RebanadaProcessor&);
    ~Content() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    static constexpr int baseWidth = 1200, baseHeight = 780;

    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void filesDropped (const juce::StringArray&, int x, int y) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { dropHover = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override { dropHover = false; repaint(); }

private:
    void timerCallback() override;
    void setLanguage (Lang);
    void refreshSlot (bool force);
    void refreshSlice();
    void attachLoop (int slot);
    void fillOutputs (const SliceParams&);
    void selectSlice (int index);
    void editSlice (const std::function<void (SliceParams&)>&);
    void chooseFile (int slot);
    void loadInto (int slot, const juce::File&);
    void updateTexts();

    RebanadaProcessor& processor;
    LookAndFeel lnf;
    Lang lang = Lang::en;

    juce::TextButton langEn { "EN" }, langEs { "ES" };
    SlotStrip slots;
    juce::TextButton playButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> playAttachment;
    Segmented modeSwitch;
    TempoDisplay tempo, outputDisplay;
    juce::ComboBox switchBox, lfoBox, outBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> switchAttachment, lfoAttachment;

    WaveView wave;
    juce::Slider detect;
    juce::TextButton barsDown { "-" }, barsUp { "+" };
    juce::TextButton resliceButton, splitButton, mergeButton, snapButton, gridButton;
    MidiDragger midiDragger;

    juce::OwnedArray<Knob> sliceKnobs, voiceKnobs, motionKnobs;
    juce::TextButton prevButton, nextButton, reverseButton, muteButton;
    Segmented filterSwitch, orderSwitch, stereoSwitch;
    juce::StringArray voiceIds, motionIds;
    KeyStrip keys;

    std::unique_ptr<juce::FileChooser> chooser;
    RebanadaProcessor::SlotInfo info;
    int shownSlot = -1, shownVersion = -1, shownSelected = -1, shownOutputs = -1, attachedSlot = -1;
    bool shownStereo = true;
    bool dropHover = false;
    Telemetry telemetry;
};

} // namespace rebanada::rack

class RebanadaEditor : public juce::AudioProcessorEditor
{
public:
    explicit RebanadaEditor (RebanadaProcessor&);
    void resized() override;
    void paint (juce::Graphics& g) override { g.fillAll (rebanada::rack::colours::ground); }

private:
    rebanada::rack::Content content;
    juce::TooltipWindow tooltips { this, 700 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RebanadaEditor)
};
