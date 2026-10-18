// Rebanada — "Rack" interface
// Copyright (C) 2026 Lautaro Martín Vieyra
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "PluginEditor.h"
#include "BinaryData.h"

namespace rebanada::rack
{

//==============================================================================
namespace
{
    struct Typefaces
    {
        juce::Typeface::Ptr regular, semibold, extrabold, mono, monoMedium;
        static Typefaces& get()
        {
            static Typefaces t {
                juce::Typeface::createSystemTypefaceFor (BinaryData::ArchivoRegular_ttf, BinaryData::ArchivoRegular_ttfSize),
                juce::Typeface::createSystemTypefaceFor (BinaryData::ArchivoSemiBold_ttf, BinaryData::ArchivoSemiBold_ttfSize),
                juce::Typeface::createSystemTypefaceFor (BinaryData::ArchivoExtraBold_ttf, BinaryData::ArchivoExtraBold_ttfSize),
                juce::Typeface::createSystemTypefaceFor (BinaryData::IBMPlexMonoRegular_ttf, BinaryData::IBMPlexMonoRegular_ttfSize),
                juce::Typeface::createSystemTypefaceFor (BinaryData::IBMPlexMonoMedium_ttf, BinaryData::IBMPlexMonoMedium_ttfSize)
            };
            return t;
        }
    };

    constexpr float pi = juce::MathConstants<float>::pi;
    const juce::Colour dark = colours::ground;

    void drawCaps (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> r,
                   juce::Justification j = juce::Justification::centredLeft, float size = 11.0f)
    {
        g.setFont (semi (size).withExtraKerningFactor (0.12f));
        g.drawText (caps (text), r, j, false);
    }

    juce::String audioWildcard (juce::AudioFormatManager& f) { return f.getWildcardForAllFormats(); }
}

juce::Font sans (float h)     { return juce::Font (juce::FontOptions (Typefaces::get().regular).withHeight (h)); }
juce::Font semi (float h)     { return juce::Font (juce::FontOptions (Typefaces::get().semibold).withHeight (h)); }
juce::Font heavy (float h)    { return juce::Font (juce::FontOptions (Typefaces::get().extrabold).withHeight (h)); }
juce::Font mono (float h)     { return juce::Font (juce::FontOptions (Typefaces::get().mono).withHeight (h)); }
juce::Font monoBold (float h) { return juce::Font (juce::FontOptions (Typefaces::get().monoMedium).withHeight (h)); }

//==============================================================================
LookAndFeel::LookAndFeel()
{
    setColour (juce::PopupMenu::backgroundColourId, colours::panel);
    setColour (juce::PopupMenu::textColourId, colours::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, colours::accent);
    setColour (juce::PopupMenu::highlightedTextColourId, dark);
    setColour (juce::ComboBox::textColourId, colours::text);
    setColour (juce::ComboBox::backgroundColourId, colours::ground);
    setColour (juce::ComboBox::outlineColourId, colours::line);
    setColour (juce::ComboBox::arrowColourId, colours::sub);
    setColour (juce::AlertWindow::backgroundColourId, colours::panel);
    setColour (juce::AlertWindow::textColourId, colours::text);
    setColour (juce::AlertWindow::outlineColourId, colours::line);
    setColour (juce::TextButton::textColourOffId, colours::text);
    setColour (juce::TextButton::buttonColourId, colours::panel);
}

void LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                    float start, float end, juce::Slider& s)
{
    const bool on = s.isEnabled();
    const float r = juce::jmin (w, h) * 0.5f - 1.0f;
    const float cx = x + w * 0.5f, cy = y + h * 0.5f;
    const float ringR = r - 3.0f;

    juce::Path track;
    track.addCentredArc (cx, cy, ringR, ringR, 0.0f, start, end, true);
    g.setColour (colours::line);
    g.strokePath (track, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::butt));

    const float angle = start + pos * (end - start);
    const bool bipolar = s.getProperties().getWithDefault ("bipolar", false);
    const float from = bipolar ? (start + end) * 0.5f : start;
    if (std::abs (angle - from) > 0.01f)
    {
        juce::Path value;
        value.addCentredArc (cx, cy, ringR, ringR, 0.0f, juce::jmin (from, angle), juce::jmax (from, angle), true);
        g.setColour (on ? colours::accent : colours::faint);
        g.strokePath (value, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
    }

    const float capR = r - 7.0f;
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillEllipse (cx - capR, cy - capR + 2.5f, capR * 2, capR * 2);
    juce::ColourGradient cap (juce::Colour (0xff4a443d), cx - capR * 0.35f, cy - capR * 0.45f,
                              juce::Colour (0xff1f1c19), cx + capR * 0.9f, cy + capR, true);
    g.setGradientFill (cap);
    g.fillEllipse (cx - capR, cy - capR, capR * 2, capR * 2);
    g.setColour (juce::Colours::white.withAlpha (0.05f));
    g.drawEllipse (cx - capR + 1.0f, cy - capR + 1.0f, capR * 2 - 2, capR * 2 - 2, 1.0f);

    const float sx = std::sin (angle), sy = -std::cos (angle);
    g.setColour (on ? colours::text : colours::faint);
    g.drawLine (cx + sx * capR * 0.3f, cy + sy * capR * 0.3f, cx + sx * capR * 0.86f, cy + sy * capR * 0.86f, 2.4f);

    if (s.hasKeyboardFocus (true))
    {
        g.setColour (colours::accent.withAlpha (0.6f));
        g.drawEllipse (cx - r, cy - r, r * 2, r * 2, 1.0f);
    }
}

void LookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                                    juce::Slider::SliderStyle, juce::Slider& s)
{
    const float cy = y + h * 0.5f;
    const auto track = juce::Rectangle<float> ((float) x, cy - 2.0f, (float) w, 4.0f);
    g.setColour (colours::line);
    g.fillRoundedRectangle (track, 2.0f);
    g.setColour (s.isEnabled() ? colours::accent : colours::faint);
    g.fillRoundedRectangle (track.withRight (pos), 2.0f);
    g.setColour (s.isEnabled() ? colours::text : colours::faint);
    g.fillEllipse (pos - 7.0f, cy - 7.0f, 14.0f, 14.0f);
}

void LookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();
    const auto& props = b.getProperties();
    const juce::Colour fill = props.getWithDefault ("lightOn", false) ? colours::text : colours::accent;
    const juce::Colour base = props.getWithDefault ("onGround", false) ? colours::ground : colours::panel;

    if (on)
    {
        g.setColour (down ? fill.darker (0.15f) : fill);
        g.fillRoundedRectangle (r, 6.0f);
    }
    else
    {
        g.setColour (down ? base.brighter (0.08f) : base);
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (over && b.isEnabled() ? colours::sub : colours::line);
        g.drawRoundedRectangle (r, 6.0f, 1.0f);
    }

    if (props.getWithDefault ("play", false))
    {
        const auto c = r.getCentre();
        juce::Path tri;
        tri.addTriangle (c.x - 6.0f, c.y - 8.0f, c.x - 6.0f, c.y + 8.0f, c.x + 8.0f, c.y);
        g.setColour (on ? dark : colours::text);
        g.fillPath (tri);
    }
    if (props.getWithDefault ("chevron", 0) != juce::var (0))
    {
        const int dir = props["chevron"];
        const auto c = r.getCentre();
        juce::Path p;
        p.startNewSubPath (c.x + 2.5f * dir, c.y - 4.5f);
        p.lineTo (c.x - 2.0f * dir, c.y);
        p.lineTo (c.x + 2.5f * dir, c.y + 4.5f);
        g.setColour (b.isEnabled() ? colours::text : colours::faint);
        g.strokePath (p, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
}

void LookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    if (b.getButtonText().isEmpty()) return;
    const bool on = b.getToggleState();
    auto colour = on ? dark : (b.getProperties().getWithDefault ("dimWhenOff", false) ? colours::sub : colours::text);
    if (! b.isEnabled()) colour = colours::faint;
    g.setColour (colour);
    auto area = b.getLocalBounds();
    if (b.getProperties().getWithDefault ("reverseIcon", false))
    {
        auto icon = area.removeFromLeft (30).toFloat();
        const auto c = icon.getCentre().translated (6.0f, 0.0f);
        juce::Path p;
        p.startNewSubPath (c.x + 6.0f, c.y);
        p.lineTo (c.x - 5.0f, c.y);
        p.startNewSubPath (c.x - 1.5f, c.y - 3.5f);
        p.lineTo (c.x - 5.0f, c.y);
        p.lineTo (c.x - 1.5f, c.y + 3.5f);
        g.strokePath (p, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        area.removeFromRight (8);
    }
    const bool small = b.getProperties().getWithDefault ("small", false);
    g.setFont (b.getProperties().getWithDefault ("monoText", false) ? mono (13.0f) : semi (small ? 12.0f : 13.0f));
    g.drawText (b.getButtonText(), area, juce::Justification::centred);
}

void LookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (colours::ground);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (box.isMouseOver (true) ? colours::sub : colours::line);
    g.drawRoundedRectangle (r, 6.0f, 1.0f);
    juce::Path p;
    const float ax = (float) w - 14.0f, ay = h * 0.5f;
    p.startNewSubPath (ax - 4.0f, ay - 2.0f);
    p.lineTo (ax, ay + 2.0f);
    p.lineTo (ax + 4.0f, ay - 2.0f);
    g.setColour (colours::sub);
    g.strokePath (p, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void LookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (6, 1, box.getWidth() - 26, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}

void LookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& text, int w, int h)
{
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h);
    g.setColour (colours::screen);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (colours::screenEdge);
    g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
    g.setColour (colours::text);
    g.setFont (sans (13.0f));
    g.drawFittedText (text, juce::Rectangle<int> (0, 0, w, h).reduced (10, 6), juce::Justification::centredLeft, 4);
}

juce::Rectangle<int> LookAndFeel::getTooltipBounds (const juce::String& text, juce::Point<int> pos, juce::Rectangle<int> parent)
{
    const int width = juce::jmin (360, (int) juce::GlyphArrangement::getStringWidth (sans (13.0f), text) + 24);
    const int lines = (int) std::ceil ((juce::GlyphArrangement::getStringWidth (sans (13.0f), text) + 24) / 360.0f);
    const int height = 14 + 17 * juce::jmax (1, lines);
    return juce::Rectangle<int> (pos.x > parent.getCentreX() ? pos.x - (width + 12) : pos.x + 18,
                                 pos.y > parent.getCentreY() ? pos.y - (height + 6) : pos.y + 18, width, height)
        .constrainedWithin (parent);
}

//==============================================================================
Knob::Knob (const FloatSpec& s, bool bipolar) : spec (s)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    slider.setRotaryParameters (pi * 1.25f, pi * 2.75f, true);
    slider.setMouseDragSensitivity (220);
    slider.setScrollWheelEnabled (true);
    slider.setVelocityBasedMode (false);
    slider.getProperties().set ("bipolar", bipolar);
    const auto range = rangeFor (spec);
    slider.setNormalisableRange ({ (double) range.start, (double) range.end,
        [range] (double, double, double x) { return (double) range.convertFrom0to1 ((float) x); },
        [range] (double, double, double v) { return (double) range.convertTo0to1 ((float) v); },
        [range] (double, double, double v) { return (double) range.snapToLegalValue ((float) v); } });
    slider.setValue (spec.def, juce::dontSendNotification);
    slider.setDoubleClickReturnValue (true, spec.def);
    slider.onValueChange = [this] {
        repaint();
        if (onChange) onChange ((float) slider.getValue());
    };
    addAndMakeVisible (slider);
}

void Knob::attach (juce::AudioProcessorValueTreeState& apvts)
{
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, spec.id, slider);
    slider.setDoubleClickReturnValue (true, spec.def);
}

void Knob::setLanguage (Lang l)
{
    lang = l;
    const juce::String note = u8 (lang == Lang::es ? spec.noteEs : spec.noteEn);
    slider.setTooltip ((note.isEmpty() ? juce::String() : note + ". ") + tr (lang, "dblclick") + ".");
    repaint();
}

void Knob::resized()
{
    slider.setBounds ((width - dial) / 2, 0, dial, dial);
}

void Knob::paint (juce::Graphics& g)
{
    const float alpha = isEnabled() ? 1.0f : 0.45f;
    g.setColour (colours::sub.withMultipliedAlpha (alpha));
    drawCaps (g, u8 (lang == Lang::es ? spec.es : spec.en), { 0.0f, (float) dial + 3.0f, (float) width, 14.0f },
              juce::Justification::centred, 10.0f);
    g.setColour (colours::text.withMultipliedAlpha (alpha));
    g.setFont (mono (12.0f));
    g.drawText (localValue (formatValue (spec, (float) slider.getValue()), lang),
                juce::Rectangle<float> (0.0f, (float) dial + 17.0f, (float) width, 16.0f), juce::Justification::centred, false);
}

//==============================================================================
Segmented::Segmented (std::vector<const char*> k, bool v, std::function<int()> g, std::function<void (int)> s)
    : keys (std::move (k)), vertical (v), get (std::move (g)), set (std::move (s))
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

int Segmented::indexAt (juce::Point<float> p) const
{
    const int n = (int) keys.size();
    const float t = vertical ? p.y / (float) getHeight() : p.x / (float) getWidth();
    return juce::jlimit (0, n - 1, (int) (t * n));
}

void Segmented::mouseDown (const juce::MouseEvent& e)
{
    if (isEnabled() && set) set (indexAt (e.position));
    repaint();
}

void Segmented::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    const int n = (int) keys.size();
    const int current = get ? get() : 0;
    juce::Path clip;
    clip.addRoundedRectangle (r, 6.0f);
    g.saveState();
    g.reduceClipRegion (clip);
    for (int i = 0; i < n; ++i)
    {
        auto cell = vertical ? juce::Rectangle<float> (r.getX(), r.getY() + r.getHeight() * i / n, r.getWidth(), r.getHeight() / n)
                             : juce::Rectangle<float> (r.getX() + r.getWidth() * i / n, r.getY(), r.getWidth() / n, r.getHeight());
        const bool on = i == current;
        if (on) { g.setColour (isEnabled() ? colours::accent : colours::faint); g.fillRect (cell); }
        else if (i > 0)
        {
            g.setColour (colours::line);
            if (vertical) g.fillRect (cell.getX(), cell.getY(), cell.getWidth(), 1.0f);
            else g.fillRect (cell.getX(), cell.getY(), 1.0f, cell.getHeight());
        }
        g.setColour (on ? dark : (isEnabled() ? colours::sub : colours::faint));
        auto text = plainLabels ? juce::String (keys[(size_t) i]) : tr (lang, keys[(size_t) i]);
        if (capsLabels) text = caps (text);
        if (monoFont) { g.setFont (mono (fontSize)); g.drawText (text, cell, juce::Justification::centred); }
        else if (vertical || ! plainLabels) { g.setFont (semi (fontSize)); g.drawText (text, cell, juce::Justification::centred); }
    }
    g.restoreState();
    g.setColour (colours::line);
    g.drawRoundedRectangle (r, 6.0f, 1.0f);
}

//==============================================================================
SlotStrip::SlotStrip (RebanadaProcessor& p) : processor (p)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

int SlotStrip::slotAt (juce::Point<float> p) const
{
    const float cell = (getWidth() + 5.0f) / maxSlots;
    return juce::jlimit (0, maxSlots - 1, (int) (p.x / cell));
}

void SlotStrip::update (const Telemetry& t, int editSlot, const std::array<bool, maxSlots>& l, Lang lng)
{
    tele = t;
    edit = editSlot;
    loaded = l;
    lang = lng;
    blink = (blink + 1) % 16;
    repaint();
}

void SlotStrip::mouseDown (const juce::MouseEvent& e)
{
    const int s = slotAt (e.position);
    if (e.mods.isPopupMenu())
    {
        juce::PopupMenu m;
        m.addItem (1, tr (lang, "load"));
        m.addItem (2, tr (lang, "clear"), loaded[(size_t) s]);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [this, s] (int r) {
            if (r == 1 && onLoadRequest) onLoadRequest (s);
            if (r == 2 && onClearRequest) onClearRequest (s);
        });
        return;
    }
    processor.requestSlot (s);
    edit = s;
    repaint();
}

void SlotStrip::paint (juce::Graphics& g)
{
    const float gap = 5.0f;
    const float w = (getWidth() - gap * (maxSlots - 1)) / maxSlots;
    for (int i = 0; i < maxSlots; ++i)
    {
        auto r = juce::Rectangle<float> (i * (w + gap), 0.0f, w, (float) getHeight()).reduced (0.5f);
        const bool selected = i == edit;
        const bool playing = tele.running && tele.currentSlot == i && loaded[(size_t) i];
        const bool pending = tele.pendingSlot == i;
        if (selected) { g.setColour (colours::accent); g.fillRoundedRectangle (r, 6.0f); }
        else
        {
            g.setColour (colours::panel);
            g.fillRoundedRectangle (r, 6.0f);
            g.setColour (pending && blink < 8 ? colours::accent : colours::line);
            g.drawRoundedRectangle (r, 6.0f, pending ? 1.6f : 1.0f);
        }
        g.setColour (selected ? dark : (loaded[(size_t) i] ? colours::text : colours::faint));
        g.setFont (mono (14.0f));
        g.drawText (juce::String (i + 1), r, juce::Justification::centred);
        if (playing)
        {
            g.setColour (selected ? dark : colours::accent);
            g.fillEllipse (r.getRight() - 9.0f, r.getY() + 5.0f, 4.5f, 4.5f);
        }
        if (pending && selected && blink < 8)
        {
            g.setColour (dark);
            g.drawRoundedRectangle (r.reduced (3.0f), 4.0f, 1.0f);
        }
    }
}

//==============================================================================
TempoDisplay::TempoDisplay (RebanadaProcessor& p) : processor (p) {}

juce::RangedAudioParameter* TempoDisplay::target() const
{
    if (processor.apvts.getRawParameterValue ("mode")->load() > 0.5f) return processor.apvts.getParameter ("rate");
    if (! tele.hostTempo) return processor.apvts.getParameter ("tempo");
    return nullptr;
}

void TempoDisplay::update (const Telemetry& t, Lang l)
{
    tele = t;
    if (lang != l) { lang = l; setTooltip (tr (lang, "tempoTip")); }
    if (getTooltip().isEmpty()) setTooltip (tr (lang, "tempoTip"));
    setMouseCursor (target() != nullptr ? juce::MouseCursor::UpDownResizeCursor : juce::MouseCursor::NormalCursor);
    repaint();
}

void TempoDisplay::mouseDown (const juce::MouseEvent&)
{
    dragging = target();
    if (dragging == nullptr) return;
    dragStart = dragging->getValue();
    dragging->beginChangeGesture();
}

void TempoDisplay::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging == nullptr) return;
    const float speed = e.mods.isShiftDown() ? 0.0006f : 0.004f;
    dragging->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, dragStart - e.getDistanceFromDragStartY() * speed));
}

void TempoDisplay::mouseUp (const juce::MouseEvent&)
{
    if (dragging != nullptr) dragging->endChangeGesture();
    dragging = nullptr;
}

void TempoDisplay::mouseDoubleClick (const juce::MouseEvent&)
{
    if (auto* p = target()) p->setValueNotifyingHost (p->getDefaultValue());
}

void TempoDisplay::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (colours::screen);
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (colours::screenEdge);
    g.drawRoundedRectangle (r, 8.0f, 1.0f);

    const bool free = processor.apvts.getRawParameterValue ("mode")->load() > 0.5f;
    juce::String main, sub;
    if (free)
    {
        main = localNumber (juce::String (processor.apvts.getRawParameterValue ("rate")->load(), 2), lang) + u8 ("×");
        sub = tr (lang, "rateFree");
    }
    else if (tele.hostTempo)
    {
        main = localNumber (juce::String (tele.bpm, 2), lang);
        sub = tr (lang, "bpmHost");
    }
    else
    {
        main = localNumber (juce::String (processor.apvts.getRawParameterValue ("tempo")->load(), 1), lang);
        sub = tr (lang, "bpmOwn");
    }
    auto inner = r.reduced (12.0f, 0.0f);
    g.setColour (colours::accent);
    g.setFont (mono (19.0f));
    const float mw = juce::GlyphArrangement::getStringWidth (mono (19.0f), main);
    g.drawText (main, inner.removeFromLeft (mw + 2.0f), juce::Justification::centredLeft);
    inner.removeFromLeft (8.0f);
    g.setColour (colours::sub);
    g.setFont (mono (10.5f));
    g.drawFittedText (sub, inner.toNearestInt(), juce::Justification::centredLeft, 2, 0.8f);
}

//==============================================================================
WaveView::WaveView (RebanadaProcessor& p) : processor (p) {}

juce::Rectangle<float> WaveView::waveArea() const
{
    return getLocalBounds().toFloat().withTrimmedTop ((float) labelHeight + 2.0f);
}

float WaveView::xFor (int frame) const
{
    const auto a = waveArea();
    const int len = info.sample ? info.sample->length() : 1;
    return a.getX() + a.getWidth() * (float) frame / (float) juce::jmax (1, len);
}

int WaveView::frameFor (float x) const
{
    const auto a = waveArea();
    const int len = info.sample ? info.sample->length() : 1;
    return juce::jlimit (0, len, (int) std::lround ((x - a.getX()) / a.getWidth() * len));
}

int WaveView::sliceAt (float x) const
{
    const int f = frameFor (x);
    int k = 0;
    for (int i = 1; i < (int) info.starts.size(); ++i) if (info.starts[(size_t) i] <= f) k = i;
    return k;
}

int WaveView::markerAt (juce::Point<float> p) const
{
    if (! info.loaded()) return -1;
    const int n = (int) info.starts.size();
    if (p.y < (float) labelHeight + 2.0f)
    {
        // labels: the last one whose box contains the point
        for (int i = n - 1; i >= 0; --i)
        {
            const float x = xFor (info.starts[(size_t) i]);
            if (p.x >= x - 1.0f && p.x <= x + 26.0f) return i;
        }
        return -1;
    }
    int best = -1;
    float bestDist = 5.0f;
    for (int i = 1; i < n; ++i)
    {
        const float d = std::abs (p.x - xFor (info.starts[(size_t) i]));
        if (d < bestDist) { bestDist = d; best = i; }
    }
    return best;
}

void WaveView::setInfo (int s, const RebanadaProcessor::SlotInfo& i)
{
    slot = s;
    info = i;
    setTooltip (info.loaded() ? tr (lang, "waveTip") : juce::String());
    repaint();
}

void WaveView::computePeaks()
{
    peaksFor = info.sample.get();
    peaks.clear();
    if (! info.loaded()) return;
    const int columns = juce::jmax (1, (int) (waveArea().getWidth() / 3.0f));
    const auto& L = info.sample->left;
    const auto& R = info.sample->right;
    const int len = info.sample->length();
    peaks.assign ((size_t) columns, 0.0f);
    float loudest = 1.0e-6f;
    for (int c = 0; c < columns; ++c)
    {
        const int a = (int) ((long long) len * c / columns), b = juce::jmax (a + 1, (int) ((long long) len * (c + 1) / columns));
        const int step = juce::jmax (1, (b - a) / 400);
        float m = 0.0f;
        for (int i = a; i < b; i += step) m = juce::jmax (m, std::abs (L[(size_t) i]), std::abs (R[(size_t) i]));
        peaks[(size_t) c] = m;
        loudest = juce::jmax (loudest, m);
    }
    for (auto& v : peaks) v = juce::jmin (1.0f, v / loudest);
}

void WaveView::update (const Telemetry& t, Lang l)
{
    tele = t;
    if (lang != l) { lang = l; setTooltip (info.loaded() ? tr (lang, "waveTip") : juce::String()); }
    repaint();
}

void WaveView::paint (juce::Graphics& g)
{
    const auto a = waveArea();
    if (! info.loaded())
    {
        auto box = a.reduced (2.0f);
        g.setColour (colours::screenEdge);
        const float dash[] { 6.0f, 5.0f };
        juce::Path outline;
        outline.addRoundedRectangle (box, 8.0f);
        juce::Path dashed;
        juce::PathStrokeType (1.2f).createDashedStroke (dashed, outline, dash, 2);
        g.fillPath (dashed);
        g.setColour (info.missing ? colours::accent : colours::sub);
        g.setFont (sans (15.0f));
        const auto text = info.missing ? tr (lang, "missing") + ": " + info.name : tr (lang, "empty");
        g.drawText (text, box, juce::Justification::centred);
        return;
    }

    if (info.sample.get() != peaksFor || (int) peaks.size() != juce::jmax (1, (int) (waveArea().getWidth() / 3.0f))) computePeaks();
    const int sel = juce::jlimit (0, juce::jmax (0, info.numSlices - 1), processor.selectedSlice (slot));
    const int n = (int) info.starts.size();
    const auto& params = info.params;
    const bool here = tele.currentSlot == slot;

    // selected slice region
    const float selX0 = xFor (info.starts[(size_t) sel]);
    const float selX1 = sel + 1 < n ? xFor (info.starts[(size_t) sel + 1]) : a.getRight();
    g.setColour (colours::accent.withAlpha (0.10f));
    g.fillRect (juce::Rectangle<float> (selX0, a.getY(), selX1 - selX0, a.getHeight()));

    g.setColour (colours::screenEdge);
    g.fillRect (a.getX(), a.getCentreY(), a.getWidth(), 1.0f);

    // waveform as columns
    const int columns = (int) peaks.size();
    int k = 0;
    const int len = info.sample->length();
    for (int c = 0; c < columns; ++c)
    {
        const int frame = (int) ((long long) len * c / columns);
        while (k + 1 < n && info.starts[(size_t) k + 1] <= frame) ++k;
        const float x = a.getX() + c * 3.0f;
        const float h = juce::jmax (2.0f, peaks[(size_t) c] * a.getHeight() * 0.92f);
        juce::Colour col = k == sel ? colours::accent : colours::waveDim;
        if (params[(size_t) k].mute) col = colours::waveMuted;
        else if (here && tele.sliceLevel[k] > 0.01f && k != sel)
            col = col.interpolatedWith (colours::accent, juce::jlimit (0.0f, 0.8f, tele.sliceLevel[k]));
        g.setColour (col);
        g.fillRoundedRectangle (x, a.getCentreY() - h * 0.5f, 2.0f, h, 1.0f);
    }

    // markers and their labels
    float lastLabelEnd = -100.0f;
    g.setFont (mono (10.5f));
    for (int i = 0; i < n; ++i)
    {
        const float x = xFor (info.starts[(size_t) i]);
        const bool on = i == sel;
        const bool hot = i == hoverMarker || i == dragMarker;
        if (i > 0)
        {
            g.setColour (hot ? colours::text : (on ? colours::accent : colours::accent.withAlpha (0.35f)));
            g.fillRect (x, a.getY(), hot ? 2.0f : 1.0f, a.getHeight());
        }
        const juce::String label = (i < 9 ? "0" : "") + juce::String (i + 1);
        const float w = juce::GlyphArrangement::getStringWidth (mono (10.5f), label) + 10.0f;
        if (! on && x < lastLabelEnd + 2.0f) continue;
        auto box = juce::Rectangle<float> (x, 0.0f, w, (float) labelHeight - 2.0f);
        if (box.getRight() > (float) getWidth()) box.setX ((float) getWidth() - w);
        const auto& sp = params[(size_t) i];
        if (on)
        {
            g.setColour (colours::accent);
            g.fillRoundedRectangle (box, 3.0f);
            g.setColour (dark);
        }
        else
        {
            g.setColour (colours::screen);
            g.fillRoundedRectangle (box, 3.0f);
            g.setColour (colours::accent.withAlpha (sp.mute ? 0.2f : 0.45f));
            g.drawRoundedRectangle (box.reduced (0.5f), 3.0f, 1.0f);
            g.setColour (sp.mute ? colours::faint : (sp.reverse ? colours::reverse : colours::accent));
        }
        g.drawText (label, box, juce::Justification::centred);
        lastLabelEnd = box.getRight();
    }

    // where the sound is
    if (here)
    {
        g.setColour (colours::accent.withAlpha (0.9f));
        for (int i = 0; i < tele.numVoicePos; ++i)
        {
            const float x = a.getX() + a.getWidth() * tele.voicePos[i];
            g.fillRect (x, a.getY() + 6.0f, 1.0f, a.getHeight() - 12.0f);
        }
        if (tele.running && tele.loopFrac >= 0.0f)
        {
            const float x = a.getX() + a.getWidth() * tele.loopFrac;
            g.setColour (colours::text.withAlpha (0.85f));
            g.fillRect (x - 1.0f, a.getY() - 4.0f, 2.0f, a.getHeight() + 8.0f);
        }
    }
}

void WaveView::mouseMove (const juce::MouseEvent& e)
{
    const int m = markerAt (e.position);
    const int hover = e.position.y >= (float) labelHeight ? m : -1;
    if (hover != hoverMarker) { hoverMarker = hover; repaint(); }
    setMouseCursor (! info.loaded() ? juce::MouseCursor::PointingHandCursor
                    : (hover > 0 ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::NormalCursor));
}

void WaveView::mouseDown (const juce::MouseEvent& e)
{
    dragMoved = false;
    if (! info.loaded())
    {
        if (onLoadRequest) onLoadRequest();
        return;
    }
    const int m = markerAt (e.position);
    if (e.mods.isPopupMenu())
    {
        if (m > 0)
        {
            processor.removeMarker (slot, m);
            processor.setSelectedSlice (slot, m - 1);
            setInfo (slot, processor.slotInfo (slot));
            if (onSelectionChange) onSelectionChange();
        }
        return;
    }
    if (m >= 0)
    {
        dragMarker = m > 0 ? m : -1;
        processor.setSelectedSlice (slot, m);
    }
    else
        processor.setSelectedSlice (slot, sliceAt (e.position.x));
    if (onSelectionChange) onSelectionChange();
    repaint();
}

void WaveView::mouseDrag (const juce::MouseEvent& e)
{
    if (dragMarker <= 0 || e.getDistanceFromDragStart() < 2) return;
    dragMoved = true;
    processor.moveMarker (slot, dragMarker, frameFor (e.position.x));
    info = processor.slotInfo (slot);
    repaint();
}

void WaveView::mouseUp (const juce::MouseEvent&)
{
    dragMarker = -1;
    repaint();
}

void WaveView::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (! info.loaded() || e.position.y < (float) labelHeight || markerAt (e.position) > 0) return;
    const int idx = processor.addMarker (slot, frameFor (e.position.x));
    if (idx >= 0) processor.setSelectedSlice (slot, idx);
    setInfo (slot, processor.slotInfo (slot));
    if (onSelectionChange) onSelectionChange();
}

//==============================================================================
void MidiDragger::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    const auto col = isEnabled() ? colours::accent : colours::faint;
    juce::Path outline;
    outline.addRoundedRectangle (r, 6.0f);
    const float dash[] { 4.0f, 3.0f };
    juce::Path dashed;
    juce::PathStrokeType (1.2f).createDashedStroke (dashed, outline, dash, 2);
    g.setColour (col);
    g.fillPath (dashed);

    auto inner = r.reduced (14.0f, 0.0f);
    auto icon = inner.removeFromLeft (18.0f);
    const float cy = icon.getCentreY();
    for (int i = -1; i <= 1; ++i) g.fillEllipse (icon.getX() + 1.0f, cy + i * 4.5f - 1.3f, 2.6f, 2.6f);
    juce::Path arrow;
    arrow.startNewSubPath (icon.getX() + 7.0f, cy);
    arrow.lineTo (icon.getRight(), cy);
    arrow.startNewSubPath (icon.getRight() - 3.5f, cy - 3.5f);
    arrow.lineTo (icon.getRight(), cy);
    arrow.lineTo (icon.getRight() - 3.5f, cy + 3.5f);
    g.strokePath (arrow, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    inner.removeFromLeft (10.0f);
    g.setFont (semi (13.0f));
    g.drawText (tr (lang, "dragMidi"), inner, juce::Justification::centredLeft);
}

void MidiDragger::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging || ! isEnabled() || e.getDistanceFromDragStart() < 5) return;
    dragging = true;
    const auto f = processor.writeMidiFile (slot);
    if (f.existsAsFile())
        juce::DragAndDropContainer::performExternalDragDropOfFiles ({ f.getFullPathName() }, false, this);
}

//==============================================================================
KeyStrip::KeyStrip (RebanadaProcessor& p) : processor (p)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

juce::Rectangle<float> KeyStrip::keyArea() const
{
    return getLocalBounds().toFloat().withTrimmedLeft (146.0f);
}

void KeyStrip::update (const Telemetry& t, int s, int n, int sel, Lang l)
{
    const bool changed = s != slot || n != numSlices || sel != selected || l != lang;
    tele = t;
    slot = s; numSlices = n; selected = sel;
    if (l != lang || getTooltip().isEmpty()) setTooltip (tr (l, "keysTip"));
    lang = l;
    juce::ignoreUnused (changed);
    repaint();
}

int KeyStrip::keyAt (juce::Point<float> p) const
{
    const auto a = keyArea();
    if (! a.contains (p) || numSlices <= 0) return -1;
    const int shown = juce::jmin (perPage, numSlices - first());
    const float w = a.getWidth() / (float) juce::jmax (shown, 8);
    const int i = (int) ((p.x - a.getX()) / w);
    return i < shown ? first() + i : -1;
}

void KeyStrip::mouseDown (const juce::MouseEvent& e)
{
    const int k = keyAt (e.position);
    if (k < 0) return;
    held = k;
    if (onPick) onPick (k);
    processor.keyboard.noteOn (1, sliceBaseNote + k, 0.85f);
}

void KeyStrip::mouseUp (const juce::MouseEvent&)
{
    if (held >= 0) processor.keyboard.noteOff (1, sliceBaseNote + held, 0.0f);
    held = -1;
}

void KeyStrip::paint (juce::Graphics& g)
{
    auto label = getLocalBounds().toFloat().withWidth (138.0f);
    g.setColour (colours::sub);
    drawCaps (g, tr (lang, "keys"), label.withHeight (getHeight() * 0.5f).withTrimmedTop (8.0f), juce::Justification::centredLeft, 10.0f);
    g.setColour (colours::text);
    g.setFont (mono (12.0f));
    g.drawText (tr (lang, "base") + " " + noteName (sliceBaseNote, lang), label.withTrimmedTop (getHeight() * 0.5f).withHeight (16.0f),
                juce::Justification::centredLeft);
    g.setColour (colours::faint);
    g.setFont (mono (10.0f));
    g.drawText (tr (lang, "loopNotes"), label.withTrimmedTop (getHeight() * 0.5f + 16.0f).withHeight (14.0f), juce::Justification::centredLeft);

    const auto a = keyArea();
    if (numSlices <= 0)
    {
        g.setColour (colours::line);
        g.drawRoundedRectangle (a.reduced (0.5f), 6.0f, 1.0f);
        return;
    }
    const int f = first();
    const int shown = juce::jmin (perPage, numSlices - f);
    const float w = a.getWidth() / (float) juce::jmax (shown, 8);
    const bool here = tele.currentSlot == slot;
    for (int i = 0; i < shown; ++i)
    {
        const int s = f + i;
        const int note = sliceBaseNote + s;
        const bool black = juce::MidiMessage::isMidiNoteBlack (note);
        const bool on = s == selected;
        auto r = juce::Rectangle<float> (a.getX() + i * w, a.getY(), w - 3.0f, a.getHeight());
        juce::Colour bg = on ? colours::accent : (black ? colours::keyBlack : colours::keyWhite);
        juce::Colour fg = on ? dark : (black ? colours::text : dark);
        g.setColour (bg);
        juce::Path key;
        key.addRoundedRectangle (r.getX(), r.getY(), r.getWidth(), r.getHeight(), 6.0f, 6.0f, false, false, true, true);
        g.fillPath (key);
        if (black && ! on) { g.setColour (colours::keyBlackEdge); g.strokePath (key, juce::PathStrokeType (1.0f)); }
        if (here && s < maxSlices && tele.sliceLevel[s] > 0.02f)
        {
            g.setColour ((on ? dark : colours::accent).withAlpha (juce::jlimit (0.3f, 1.0f, tele.sliceLevel[s] + 0.2f)));
            g.fillRoundedRectangle (r.getX() + 4.0f, r.getBottom() - 6.0f, r.getWidth() - 8.0f, 3.0f, 1.5f);
        }
        g.setColour (fg);
        if (w > 26.0f)
        {
            g.setFont (mono (9.5f));
            g.drawText (noteName (note, lang), r.withHeight (18.0f).translated (0.0f, 4.0f), juce::Justification::centred);
        }
        g.setFont (monoBold (12.0f));
        g.drawText (juce::String (s + 1), r.withTrimmedTop (r.getHeight() - 28.0f).withHeight (18.0f), juce::Justification::centred);
    }
}

//==============================================================================
namespace
{
    int choiceIndex (RebanadaProcessor& p, const char* id) { return juce::roundToInt (p.apvts.getRawParameterValue (id)->load()); }
    void setChoice (RebanadaProcessor& p, const char* id, int index)
    {
        if (auto* param = p.apvts.getParameter (id))
        {
            param->beginChangeGesture();
            param->setValueNotifyingHost (param->convertTo0to1 ((float) index));
            param->endChangeGesture();
        }
    }
}

Content::Content (RebanadaProcessor& p)
    : processor (p),
      slots (p),
      modeSwitch ({ "sync", "free" }, false, [&p] { return choiceIndex (p, "mode"); }, [&p] (int i) { setChoice (p, "mode", i); }),
      tempo (p),
      wave (p),
      midiDragger (p),
      filterSwitch ({ "LP", "BP", "HP" }, true, [&p] { return choiceIndex (p, "filterType"); }, [&p] (int i) { setChoice (p, "filterType", i); }),
      orderSwitch ({ "forward", "random", "weighted" }, false, [&p] { return choiceIndex (p, "order"); }, [&p] (int i) { setChoice (p, "order", i); }),
      keys (p)
{
    setLookAndFeel (&lnf);
    lang = processor.getLanguage();

    for (auto* b : { &langEn, &langEs })
    {
        b->setClickingTogglesState (false);
        b->getProperties().set ("monoText", true);
        b->getProperties().set ("small", true);
        addAndMakeVisible (b);
    }
    langEn.onClick = [this] { setLanguage (Lang::en); };
    langEs.onClick = [this] { setLanguage (Lang::es); };

    addAndMakeVisible (slots);
    slots.onLoadRequest = [this] (int s) { chooseFile (s); };
    slots.onClearRequest = [this] (int s) { processor.clearSlot (s); refreshSlot (true); };

    playButton.setClickingTogglesState (true);
    playButton.getProperties().set ("play", true);
    playAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (processor.apvts, "play", playButton);
    addAndMakeVisible (playButton);

    modeSwitch.fontSize = 12.5f;
    modeSwitch.capsLabels = true;
    addAndMakeVisible (modeSwitch);
    addAndMakeVisible (tempo);

    for (auto* box : { &switchBox, &lfoBox, &outBox }) addAndMakeVisible (box);
    switchBox.addItemList ({ "Instant", "1 beat", "1 bar", "Loop end" }, 1);
    lfoBox.addItemList ({ "Cutoff", "Pitch", "Pan", "Level" }, 1);
    switchAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (processor.apvts, "switchAt", switchBox);
    lfoAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (processor.apvts, "lfoDest", lfoBox);
    for (int i = 0; i < maxPairs; ++i) outBox.addItem (juce::String (i), i + 1);
    outBox.onChange = [this] {
        const int pair = outBox.getSelectedId() - 1;
        if (pair >= 0) editSlice ([pair] (SliceParams& sp) { sp.out = pair; });
    };

    addAndMakeVisible (wave);
    wave.onSelectionChange = [this] { refreshSlice(); };
    wave.onLoadRequest = [this] { chooseFile (processor.editSlot()); };

    detect.setSliderStyle (juce::Slider::LinearHorizontal);
    detect.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    detect.setRange (0.0, 100.0, 1.0);
    detect.setValue (50.0, juce::dontSendNotification);
    detect.setDoubleClickReturnValue (true, 50.0);
    detect.onValueChange = [this] {
        const int s = processor.editSlot();
        processor.setSensitivity (s, (float) detect.getValue());
        refreshSlot (true);
    };
    addAndMakeVisible (detect);

    barsDown.onClick = [this] { const int s = processor.editSlot(); processor.setBars (s, juce::jmax (1, processor.slotInfo (s).bars - 1)); };
    barsUp.onClick   = [this] { const int s = processor.editSlot(); processor.setBars (s, processor.slotInfo (s).bars + 1); };
    for (auto* b : { &barsDown, &barsUp })
    {
        b->getProperties().set ("monoText", true);
        b->getProperties().set ("onGround", true);
        addAndMakeVisible (b);
    }

    resliceButton.onClick = [this] { processor.reslice (processor.editSlot()); refreshSlot (true); };
    splitButton.onClick = [this] {
        const int s = processor.editSlot();
        const int idx = processor.split (s, processor.selectedSlice (s));
        if (idx >= 0) processor.setSelectedSlice (s, idx);
        refreshSlot (true);
    };
    mergeButton.onClick = [this] {
        const int s = processor.editSlot();
        processor.setSelectedSlice (s, juce::jmax (0, processor.merge (s, processor.selectedSlice (s))));
        refreshSlot (true);
    };
    snapButton.setClickingTogglesState (true);
    snapButton.setToggleState (processor.getZeroSnap(), juce::dontSendNotification);
    snapButton.onClick = [this] { processor.setZeroSnap (snapButton.getToggleState()); };
    gridButton.setClickingTogglesState (true);
    gridButton.setToggleState (processor.getGrid(), juce::dontSendNotification);
    gridButton.onClick = [this] { processor.setGrid (gridButton.getToggleState(), processor.editSlot()); refreshSlot (true); };
    for (auto* b : { &resliceButton, &splitButton, &mergeButton, &snapButton, &gridButton })
    {
        b->getProperties().set ("onGround", false);
        addAndMakeVisible (b);
    }
    addAndMakeVisible (midiDragger);

    // slice knobs
    for (const char* id : { "pitch", "fine", "level", "pan", "decay", "filter" })
    {
        const juce::String sid (id);
        auto* k = sliceKnobs.add (new Knob (sliceSpec (id), sid == "pitch" || sid == "fine" || sid == "pan" || sid == "filter"));
        k->onChange = [this, sid] (float v) {
            editSlice ([sid, v] (SliceParams& sp) {
                if (sid == "pitch") sp.pitch = v;
                else if (sid == "fine") sp.fine = v;
                else if (sid == "level") sp.level = v;
                else if (sid == "pan") sp.pan = v / 100.0f;
                else if (sid == "decay") sp.decay = v;
                else sp.filter = v;
            });
        };
        addAndMakeVisible (k);
    }
    for (const char* id : { "attack", "decay", "sustain", "release", "cutoff", "reso", "transpose", "gain" })
    {
        const juce::String sid (id);
        auto* k = voiceKnobs.add (new Knob (spec (id), sid == "transpose" || sid == "gain"));
        k->attach (processor.apvts);
        addAndMakeVisible (k);
    }
    for (const char* id : { "chance", "repeat", "lfoRate", "lfoDepth" })
    {
        auto* k = motionKnobs.add (new Knob (spec (id), false));
        k->attach (processor.apvts);
        addAndMakeVisible (k);
    }

    prevButton.getProperties().set ("chevron", 1);
    nextButton.getProperties().set ("chevron", -1);
    prevButton.onClick = [this] { const int n = info.numSlices; if (n > 0) selectSlice ((processor.selectedSlice (processor.editSlot()) - 1 + n) % n); };
    nextButton.onClick = [this] { const int n = info.numSlices; if (n > 0) selectSlice ((processor.selectedSlice (processor.editSlot()) + 1) % n); };
    for (auto* b : { &prevButton, &nextButton }) { b->getProperties().set ("onGround", true); addAndMakeVisible (b); }

    reverseButton.setClickingTogglesState (true);
    reverseButton.getProperties().set ("reverseIcon", true);
    reverseButton.onClick = [this] { const bool on = reverseButton.getToggleState(); editSlice ([on] (SliceParams& sp) { sp.reverse = on; }); };
    muteButton.setClickingTogglesState (true);
    muteButton.getProperties().set ("lightOn", true);
    muteButton.onClick = [this] { const bool on = muteButton.getToggleState(); editSlice ([on] (SliceParams& sp) { sp.mute = on; }); };
    for (auto* b : { &reverseButton, &muteButton }) { b->getProperties().set ("onGround", true); addAndMakeVisible (b); }

    filterSwitch.setPlainLabels (true);
    filterSwitch.monoFont = true;
    filterSwitch.fontSize = 11.5f;
    addAndMakeVisible (filterSwitch);
    orderSwitch.fontSize = 12.5f;
    addAndMakeVisible (orderSwitch);

    addAndMakeVisible (keys);
    keys.onPick = [this] (int i) { selectSlice (i); };

    setLanguage (lang);
    refreshSlot (true);
    startTimerHz (30);
}

Content::~Content()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void Content::setLanguage (Lang l)
{
    lang = l;
    processor.setLanguage (l);
    langEn.setToggleState (l == Lang::en, juce::dontSendNotification);
    langEs.setToggleState (l == Lang::es, juce::dontSendNotification);
    for (auto* k : sliceKnobs) k->setLanguage (l);
    for (auto* k : voiceKnobs) k->setLanguage (l);
    for (auto* k : motionKnobs) k->setLanguage (l);
    for (auto* s : { &modeSwitch, &filterSwitch, &orderSwitch }) s->setLanguage (l);
    midiDragger.setLanguage (l);
    updateTexts();
    repaint();
}

void Content::updateTexts()
{
    const char* switchKeys[] { "instant", "beat", "bar", "loopEnd" };
    for (int i = 0; i < 4; ++i) switchBox.changeItemText (i + 1, tr (lang, switchKeys[i]));
    const char* lfoKeys[] { "cutoffDest", "pitchDest", "panDest", "levelDest" };
    for (int i = 0; i < 4; ++i) lfoBox.changeItemText (i + 1, tr (lang, lfoKeys[i]));
    for (int i = 0; i < maxPairs; ++i)
    {
        const auto chans = juce::String (i * 2 + 1) + u8 ("–") + juce::String (i * 2 + 2);
        outBox.changeItemText (i + 1, i == 0 ? (lang == Lang::es ? "Princ. " : "Main ") + chans : (lang == Lang::es ? "Sal. " : "Out ") + chans);
    }
    // the shown text does not follow changeItemText: select again
    switchBox.setSelectedItemIndex (choiceIndex (processor, "switchAt"), juce::dontSendNotification);
    lfoBox.setSelectedItemIndex (choiceIndex (processor, "lfoDest"), juce::dontSendNotification);
    switchBox.setText (switchBox.getItemText (choiceIndex (processor, "switchAt")), juce::dontSendNotification);
    lfoBox.setText (lfoBox.getItemText (choiceIndex (processor, "lfoDest")), juce::dontSendNotification);
    resliceButton.setButtonText (tr (lang, "reslice"));
    splitButton.setButtonText (tr (lang, "split"));
    mergeButton.setButtonText (tr (lang, "merge"));
    snapButton.setButtonText (tr (lang, "snap"));
    gridButton.setButtonText (tr (lang, "grid"));
    reverseButton.setButtonText (tr (lang, "reverse"));
    muteButton.setButtonText (tr (lang, "mute"));
    playButton.setTooltip (tr (lang, "playTip"));
    detect.setTooltip (tr (lang, "detectTip"));
    barsDown.setTooltip (tr (lang, "barsTip"));
    barsUp.setTooltip (tr (lang, "barsTip"));
    midiDragger.setTooltip (tr (lang, "dragMidiTip"));
    slots.setTooltip (tr (lang, "slotTip"));
    modeSwitch.setTooltip (lang == Lang::es ? u8 ("Sync: el loop sigue el tempo del anfitrión. Libre: el loop va a su propio tiempo")
                                            : "Sync: the loop follows the host tempo. Free: the loop runs in its own time");
    orderSwitch.setTooltip (lang == Lang::es ? u8 ("En qué orden toca las rebanadas el loop") : "The order in which the loop plays its slices");
}

void Content::refreshSlot (bool force)
{
    const int s = processor.editSlot();
    const int v = processor.getVersion();
    if (! force && s == shownSlot && v == shownVersion) return;
    shownSlot = s;
    shownVersion = v;
    info = processor.slotInfo (s);
    wave.setInfo (s, info);
    midiDragger.slot = s;
    detect.setValue (info.sensitivity, juce::dontSendNotification);
    const bool loaded = info.loaded();
    for (juce::Component* c : { (juce::Component*) &detect, (juce::Component*) &resliceButton, (juce::Component*) &splitButton,
                                (juce::Component*) &mergeButton, (juce::Component*) &midiDragger, (juce::Component*) &barsDown,
                                (juce::Component*) &barsUp, (juce::Component*) &prevButton, (juce::Component*) &nextButton,
                                (juce::Component*) &reverseButton, (juce::Component*) &muteButton, (juce::Component*) &outBox })
        c->setEnabled (loaded);
    for (auto* k : sliceKnobs) k->setEnabled (loaded);
    barsDown.setVisible (loaded);
    barsUp.setVisible (loaded);
    refreshSlice();
    repaint();
}

void Content::refreshSlice()
{
    const int s = processor.editSlot();
    const int sel = juce::jlimit (0, juce::jmax (0, info.numSlices - 1), processor.selectedSlice (s));
    shownSelected = sel;
    const auto sp = processor.getSlice (s, sel);
    const float values[] { sp.pitch, sp.fine, sp.level, sp.pan * 100.0f, sp.decay, sp.filter };
    for (int i = 0; i < sliceKnobs.size(); ++i)
    {
        sliceKnobs[i]->slider.setValue (values[i], juce::dontSendNotification);
        sliceKnobs[i]->repaint();
    }
    reverseButton.setToggleState (sp.reverse, juce::dontSendNotification);
    muteButton.setToggleState (sp.mute, juce::dontSendNotification);
    outBox.setSelectedId (sp.out + 1, juce::dontSendNotification);
    wave.repaint();
    repaint (20, 440, 434, 50);
}

void Content::selectSlice (int index)
{
    processor.setSelectedSlice (processor.editSlot(), index);
    refreshSlice();
}

void Content::editSlice (const std::function<void (SliceParams&)>& change)
{
    const int s = processor.editSlot();
    if (! info.loaded()) return;
    const int sel = juce::jlimit (0, juce::jmax (0, info.numSlices - 1), processor.selectedSlice (s));
    auto sp = processor.getSlice (s, sel);
    change (sp);
    processor.setSlice (s, sel, sp);
    shownVersion = processor.getVersion();   // only this slice changed: no need to refresh the controls
    info = processor.slotInfo (s);
    wave.setInfo (s, info);
}

void Content::chooseFile (int slot)
{
    chooser = std::make_unique<juce::FileChooser> (tr (lang, "openFile"), juce::File(), audioWildcard (processor.formats));
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this, slot] (const juce::FileChooser& fc) {
                              const auto f = fc.getResult();
                              if (f.existsAsFile()) loadInto (slot, f);
                          });
}

void Content::loadInto (int slot, const juce::File& f)
{
    juce::String error;
    if (! processor.loadFile (slot, f, error))
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Rebanada", f.getFileName() + ": " + error);
        return;
    }
    processor.requestSlot (slot);
    refreshSlot (true);
}

bool Content::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (const auto& f : files)
        if (processor.formats.findFormatForFileExtension (juce::File (f).getFileExtension()) != nullptr) return true;
    return false;
}

void Content::filesDropped (const juce::StringArray& files, int x, int y)
{
    dropHover = false;
    int slot = processor.editSlot();
    if (slots.getBounds().contains (x, y))
    {
        const auto local = slots.getLocalPoint (this, juce::Point<int> (x, y));
        slot = juce::jlimit (0, maxSlots - 1, (int) (local.x / ((slots.getWidth() + 5.0f) / maxSlots)));
    }
    for (const auto& path : files)
    {
        const juce::File f (path);
        if (processor.formats.findFormatForFileExtension (f.getFileExtension()) == nullptr) continue;
        if (slot >= maxSlots) break;
        juce::String error;
        if (processor.loadFile (slot, f, error)) processor.requestSlot (slot), ++slot;
    }
    refreshSlot (true);
    repaint();
}

void Content::mouseDown (const juce::MouseEvent& e)
{
    // the file name opens the file chooser
    if (juce::Rectangle<int> (36, 100, 560, 22).contains (e.getPosition())) chooseFile (processor.editSlot());
}

void Content::timerCallback()
{
    processor.readTelemetry (telemetry);
    const int s = processor.editSlot();
    if (s != shownSlot || processor.getVersion() != shownVersion) refreshSlot (s != shownSlot);
    if (processor.selectedSlice (s) != shownSelected) refreshSlice();

    int outs = 0;
    for (int i = 0; i < maxPairs; ++i) if (processor.pairActive (i)) outs |= 1 << i;
    if (outs != shownOutputs)
    {
        shownOutputs = outs;
        for (int i = 0; i < maxPairs; ++i) outBox.setItemEnabled (i + 1, (outs >> i) & 1);
    }

    std::array<bool, maxSlots> loaded {};
    for (int i = 0; i < maxSlots; ++i) loaded[(size_t) i] = processor.slotLoaded (i);
    slots.update (telemetry, s, loaded, lang);
    tempo.update (telemetry, lang);
    wave.update (telemetry, lang);
    keys.update (telemetry, s, info.numSlices, shownSelected, lang);
    modeSwitch.repaint();
    filterSwitch.repaint();
    orderSwitch.repaint();
}

//==============================================================================
void Content::paint (juce::Graphics& g)
{
    using namespace colours;
    g.fillAll (ground);

    // header: logo
    {
        juce::Path box;
        box.addRoundedRectangle (21.0f, 40.0f, 28.0f, 13.0f, 2.0f);
        g.setColour (accent);
        g.strokePath (box, juce::PathStrokeType (2.0f));
        g.setColour (text);
        g.drawLine (31.0f, 34.0f, 39.0f, 59.0f, 2.2f);
        g.setFont (heavy (23.0f).withExtraKerningFactor (0.16f));
        g.drawText ("REBANADA", 60, 25, 190, 26, juce::Justification::centredLeft);
        g.setColour (sub);
        g.setFont (mono (11.0f));
        g.drawText (tr (lang, "subtitle") + u8 (" · ") + JucePlugin_VersionString, 61, 50, 190, 16, juce::Justification::centredLeft);
        g.setColour (line);
        g.fillRect (246, 30, 1, 36);
        g.setColour (sub);
        drawCaps (g, tr (lang, "loops"), { 258.0f, 40.0f, 44.0f, 16.0f });
        drawCaps (g, tr (lang, "switchAt"), { 997.0f, 24.0f, 112.0f, 14.0f }, juce::Justification::centredLeft, 10.0f);
    }

    // screen
    const auto screenBox = juce::Rectangle<float> (20.0f, 90.0f, 1160.0f, 336.0f);
    g.setColour (screen);
    g.fillRoundedRectangle (screenBox, 10.0f);
    g.setColour (dropHover ? accent : screenEdge);
    g.drawRoundedRectangle (screenBox.reduced (0.5f), 10.0f, dropHover ? 2.0f : 1.0f);
    {
        const int s = processor.editSlot();
        g.setFont (mono (12.5f));
        g.setColour (accent);
        g.drawText (tr (lang, "slot") + " " + juce::String (s + 1) + u8 (" — ") + (info.loaded() ? info.name : tr (lang, "load")),
                    36, 102, 560, 18, juce::Justification::centredLeft);
        if (info.loaded())
        {
            const double secs = info.sample->length() / info.sample->sampleRate;
            const double bpm = info.bars * 4.0 * 60.0 / secs;
            g.setColour (sub);
            g.drawText (localNumber (juce::String (bpm, 1), lang) + " " + tr (lang, "origBpm") + u8 ("  ·  ") + juce::String (info.numSlices) + " "
                            + tr (lang, "slices") + u8 ("  ·  ") + localNumber (juce::String (info.sample->sampleRate / 1000.0, 1), lang) + " kHz",
                        560, 102, 486, 18, juce::Justification::centredRight);
            g.setColour (text);
            g.drawText (juce::String (info.bars) + " " + tr (lang, info.bars == 1 ? "bar1" : "bars"), 1072, 102, 80, 18, juce::Justification::centred);
        }
        g.setColour (sub);
        drawCaps (g, tr (lang, "detect"), { 36.0f, 376.0f, 70.0f, 28.0f });
        g.setColour (info.loaded() ? text : faint);
        g.setFont (mono (12.0f));
        g.drawText (juce::String ((int) detect.getValue()) + " %", 262, 376, 44, 28, juce::Justification::centredLeft);
        g.setColour (screenEdge);
        g.fillRect (308, 378, 1, 24);
    }

    // panels
    const float panelY = 440.0f, panelH = 236.0f;
    const juce::Rectangle<float> panels[] { { 20.0f, panelY, 433.0f, panelH }, { 467.0f, panelY, 350.0f, panelH }, { 831.0f, panelY, 349.0f, panelH } };
    for (const auto& r : panels)
    {
        g.setColour (panel);
        g.fillRoundedRectangle (r, 10.0f);
        g.setColour (panelEdge);
        g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.0f);
    }

    // slice panel header
    {
        const int s = processor.editSlot();
        const int sel = shownSelected < 0 ? 0 : shownSelected;
        g.setColour (sub);
        drawCaps (g, tr (lang, "slice"), { 36.0f, 454.0f, 80.0f, 28.0f });
        const float lw = juce::GlyphArrangement::getStringWidth (semi (11.0f).withExtraKerningFactor (0.12f), caps (tr (lang, "slice"))) + 12.0f;
        if (info.loaded())
        {
            g.setColour (accent);
            g.setFont (mono (23.0f));
            const auto num = (sel < 9 ? "0" : "") + juce::String (sel + 1);
            g.drawText (num, (int) (36 + lw), 452, 40, 32, juce::Justification::centredLeft);
            const int a = info.starts[(size_t) sel];
            const int b = sel + 1 < info.numSlices ? info.starts[(size_t) sel + 1] : info.sample->length();
            const double ms = (b - a) / info.sample->sampleRate * 1000.0;
            g.setColour (sub);
            g.setFont (mono (12.0f));
            g.drawText (noteName (sliceBaseNote + sel, lang) + u8 (" · ") + localNumber (juce::String (juce::roundToInt (ms)), lang) + " ms",
                        (int) (36 + lw + 44), 454, 160, 28, juce::Justification::centredLeft);
        }
        juce::ignoreUnused (s);
        g.setColour (sub);
        drawCaps (g, tr (lang, "out"), { 268.0f, 598.0f, 64.0f, 32.0f }, juce::Justification::centredRight, 10.0f);
        drawCaps (g, tr (lang, "voice"), { 483.0f, 452.0f, 320.0f, 16.0f }, juce::Justification::centredLeft, 10.5f);
        drawCaps (g, tr (lang, "motion"), { 847.0f, 452.0f, 320.0f, 16.0f }, juce::Justification::centredLeft, 10.5f);
        drawCaps (g, tr (lang, "lfoTo") + u8 (" →"), { 847.0f, 628.0f, 60.0f, 30.0f }, juce::Justification::centredLeft, 10.5f);
    }
}

void Content::resized()
{
    // header
    slots.setBounds (302, 26, 323, 44);
    playButton.setBounds (637, 26, 54, 44);
    modeSwitch.setBounds (701, 26, 116, 44);
    tempo.setBounds (827, 26, 160, 44);
    switchBox.setBounds (997, 40, 112, 28);
    langEn.setBounds (1117, 40, 30, 28);
    langEs.setBounds (1150, 40, 30, 28);

    // screen
    barsDown.setBounds (1052, 101, 20, 20);
    barsUp.setBounds (1152, 101, 20, 20);
    wave.setBounds (36, 132, 1128, 224);
    detect.setBounds (104, 376, 150, 28);
    resliceButton.setBounds (322, 370, 92, 40);
    splitButton.setBounds (420, 370, 80, 40);
    mergeButton.setBounds (506, 370, 80, 40);
    snapButton.setBounds (592, 370, 176, 40);
    gridButton.setBounds (774, 370, 116, 40);
    midiDragger.setBounds (1164 - 236, 370, 236, 40);

    // slice panel
    prevButton.setBounds (367, 454, 32, 28);
    nextButton.setBounds (405, 454, 32, 28);
    for (int i = 0; i < sliceKnobs.size(); ++i) sliceKnobs[i]->setBounds (27 + i * 68, 494, Knob::width, Knob::height);
    reverseButton.setBounds (36, 596, 112, 38);
    muteButton.setBounds (156, 596, 96, 38);
    outBox.setBounds (337, 599, 100, 32);

    // amp envelope and filter
    for (int i = 0; i < 4; ++i) voiceKnobs[i]->setBounds (474 + i * 80, 478, Knob::width, Knob::height);
    filterSwitch.setBounds (483, 576, 46, 78);
    for (int i = 4; i < 8; ++i) voiceKnobs[i]->setBounds (534 + (i - 4) * 66, 572, Knob::width, Knob::height);

    // order and motion
    orderSwitch.setBounds (847, 476, 317, 34);
    for (int i = 0; i < motionKnobs.size(); ++i) motionKnobs[i]->setBounds (842 + i * 80, 524, Knob::width, Knob::height);
    lfoBox.setBounds (906, 628, 150, 30);

    keys.setBounds (20, 690, 1160, 64);
}

} // namespace rebanada::rack

//==============================================================================
RebanadaEditor::RebanadaEditor (RebanadaProcessor& p)
    : AudioProcessorEditor (p), content (p)
{
    using C = rebanada::rack::Content;
    addAndMakeVisible (content);
    setResizable (true, true);
    if (auto* c = getConstrainer())
    {
        c->setFixedAspectRatio ((double) C::baseWidth / C::baseHeight);
        c->setSizeLimits (C::baseWidth * 6 / 10, C::baseHeight * 6 / 10, C::baseWidth * 3 / 2, C::baseHeight * 3 / 2);
    }
    setSize (C::baseWidth * 9 / 10, C::baseHeight * 9 / 10);
}

void RebanadaEditor::resized()
{
    using C = rebanada::rack::Content;
    const float scale = (float) getWidth() / (float) C::baseWidth;
    content.setTransform (juce::AffineTransform::scale (scale));
    content.setBounds (0, 0, C::baseWidth, C::baseHeight);
}
