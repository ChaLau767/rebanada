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
    const juce::Colour fill = s.getProperties().contains ("fill") ? juce::Colour ((juce::uint32) (juce::int64) s.getProperties()["fill"]) : colours::accent;
    g.setColour (s.isEnabled() ? fill : colours::faint);
    if (s.getProperties().getWithDefault ("bipolar", false))
    {
        const float mid = x + w * 0.5f;
        g.fillRoundedRectangle (juce::Rectangle<float> (juce::jmin (mid, pos), cy - 2.0f, std::abs (pos - mid), 4.0f), 2.0f);
        g.setColour (colours::sub);
        g.fillRect (mid - 0.5f, cy - 5.0f, 1.0f, 10.0f);
    }
    else g.fillRoundedRectangle (track.withRight (pos), 2.0f);
    g.setColour (s.isEnabled() ? colours::text : colours::faint);
    const float t = s.getProperties().getWithDefault ("bipolar", false) ? 5.0f : 7.0f;
    g.fillEllipse (pos - t, cy - t, t * 2.0f, t * 2.0f);
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
    slider.onPopup = [this] { if (onModMenu) onModMenu(); };
    addAndMakeVisible (slider);
}

void Knob::setModulation (float depth, float offset, juce::Colour c, const juce::String& badge)
{
    if (std::abs (depth - modDepth) < 1.0e-4f && std::abs (offset - modOffset) < 1.0e-3f && c == modColour && badge == modBadge) return;
    modDepth = depth; modOffset = offset; modColour = c; modBadge = badge;
    repaint();
}

void Knob::paintOverChildren (juce::Graphics& g)
{
    if (modDepth <= 0.0f) return;
    const auto b = slider.getBounds().toFloat();
    const float cx = b.getCentreX(), cy = b.getCentreY();
    const float r = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f - 1.5f;
    const float start = pi * 1.25f, end = pi * 2.75f;
    const float base = (float) slider.valueToProportionOfLength (slider.getValue());
    const float lo = juce::jlimit (0.0f, 1.0f, base - modDepth), hi = juce::jlimit (0.0f, 1.0f, base + modDepth);
    const float alpha = isEnabled() ? 1.0f : 0.4f;
    juce::Path arc;
    arc.addCentredArc (cx, cy, r, r, 0.0f, start + lo * (end - start), start + juce::jmax (hi, lo + 0.004f) * (end - start), true);
    g.setColour (colours::panel);
    g.strokePath (arc, juce::PathStrokeType (4.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (modColour.withMultipliedAlpha (alpha));
    g.strokePath (arc, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    const float now = juce::jlimit (0.0f, 1.0f, base + modOffset);
    const float a = start + now * (end - start);
    const float dx = cx + std::sin (a) * r, dy = cy - std::cos (a) * r;
    g.setColour (colours::ground);
    g.fillEllipse (dx - 4.0f, dy - 4.0f, 8.0f, 8.0f);
    g.setColour (modColour.withMultipliedAlpha (alpha));
    g.fillEllipse (dx - 2.6f, dy - 2.6f, 5.2f, 5.2f);
    if (modBadge.isNotEmpty())
    {
        g.setFont (monoBold (9.5f));
        g.drawText (modBadge, juce::Rectangle<float> ((float) width - 22.0f, 0.0f, 22.0f, 11.0f), juce::Justification::centredRight, false);
    }
}

void Knob::attach (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId)
{
    attachment.reset();
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, paramId, slider);
    slider.setDoubleClickReturnValue (true, spec.def);
    repaint();
}

void Knob::setLanguage (Lang l)
{
    lang = l;
    const juce::String note = u8 (lang == Lang::es ? spec.noteEs : spec.noteEn);
    slider.setTooltip ((note.isEmpty() ? juce::String() : note + ". ") + tr (lang, "dblclick") + "."
                       + (onModMenu ? " " + tr (lang, "rightClickMod") + "." : juce::String()));
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
    const char* name = lang == Lang::es ? (nameEs != nullptr ? nameEs : spec.es) : (nameEn != nullptr ? nameEn : spec.en);
    drawCaps (g, u8 (name), { 0.0f, (float) dial + 3.0f, (float) width, 14.0f },
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
        if (on) { g.setColour (isEnabled() ? onColour : colours::faint); g.fillRect (cell); }
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

bool TempoDisplay::freeMode() const
{
    return processor.apvts.getRawParameterValue (loopId (processor.editSlot(), "mode"))->load() > 0.5f;
}

juce::RangedAudioParameter* TempoDisplay::target() const
{
    if (output) return processor.apvts.getParameter ("gain");
    if (freeMode()) return processor.apvts.getParameter (loopId (processor.editSlot(), "rate"));
    if (! tele.hostTempo) return processor.apvts.getParameter ("tempo");
    return nullptr;
}

void TempoDisplay::update (const Telemetry& t, Lang l)
{
    tele = t;
    if (lang != l || getTooltip().isEmpty())
    {
        lang = l;
        setTooltip (output ? tr (lang, "outputTip") : tr (lang, "tempoTip"));
    }
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

    juce::String main, sub;
    if (output)
    {
        main = localValue (formatValue (spec ("gain"), processor.apvts.getRawParameterValue ("gain")->load()), lang);
        sub = tr (lang, "outputLevel") + u8 (" · ") + juce::String (processor.numOutputs()) + " " + tr (lang, "channelsShort");
    }
    else if (freeMode())
    {
        main = localNumber (juce::String (processor.apvts.getRawParameterValue (loopId (processor.editSlot(), "rate"))->load(), 2), lang) + u8 ("×");
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
    auto inner = r.reduced (10.0f, 4.0f);
    g.setColour (colours::accent);
    g.setFont (mono (17.0f));
    g.drawText (main, inner.removeFromTop (inner.getHeight() * 0.58f), juce::Justification::bottomLeft);
    g.setColour (colours::sub);
    g.setFont (mono (9.5f));
    g.drawFittedText (sub, inner.toNearestInt(), juce::Justification::topLeft, 1, 0.7f);
}

//==============================================================================
WaveView::WaveView (RebanadaProcessor& p) : processor (p) {}

juce::Rectangle<float> WaveView::waveArea() const
{
    return getLocalBounds().toFloat().withTrimmedTop ((float) labelHeight + 2.0f).withTrimmedBottom ((float) barHeight);
}

juce::Rectangle<float> WaveView::scrollArea() const
{
    return getLocalBounds().toFloat().removeFromBottom ((float) barHeight - 2.0f).withTrimmedTop (2.0f);
}

double WaveView::totalFrames() const { return info.sample ? (double) juce::jmax (1, info.sample->length()) : 1.0; }
double WaveView::visStart() const    { return viewLen > 0.0 ? viewStart : 0.0; }
double WaveView::visLen() const      { return viewLen > 0.0 ? viewLen : totalFrames(); }

float WaveView::xFor (int frame) const
{
    const auto a = waveArea();
    return a.getX() + a.getWidth() * (float) ((frame - visStart()) / visLen());
}

int WaveView::frameFor (float x) const
{
    const auto a = waveArea();
    return juce::jlimit (0, (int) totalFrames(), (int) std::llround (visStart() + (x - a.getX()) / a.getWidth() * visLen()));
}

// Zoom keeping the frame under the mouse in place. factor < 1 zooms in.
void WaveView::zoomAround (double frame, double factor)
{
    const double total = totalFrames();
    const double minLen = juce::jmax (64.0, (double) waveArea().getWidth());
    const double len = juce::jlimit (juce::jmin (minLen, total), total, visLen() * factor);
    if (len >= total - 0.5) { viewLen = 0.0; viewStart = 0.0; repaint(); return; }
    const double start = frame - (frame - visStart()) * len / visLen();
    viewLen = len;
    viewStart = juce::jlimit (0.0, total - len, start);
    repaint();
}

void WaveView::panBy (double frames)
{
    if (viewLen <= 0.0) return;
    viewStart = juce::jlimit (0.0, totalFrames() - viewLen, viewStart + frames);
    repaint();
}

void WaveView::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    if (! info.loaded()) return;
    const bool horizontal = std::abs (w.deltaX) > std::abs (w.deltaY) || e.mods.isShiftDown();
    if (horizontal)
        panBy (-(std::abs (w.deltaX) > std::abs (w.deltaY) ? w.deltaX : w.deltaY) * visLen() * 0.6);
    else
        zoomAround (frameFor (e.position.x), std::pow (2.0, -w.deltaY * 3.0));
}

void WaveView::mouseMagnify (const juce::MouseEvent& e, float scale)
{
    if (info.loaded() && scale > 0.0f) zoomAround (frameFor (e.position.x), 1.0 / scale);
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
    if (s != slot || i.sample != info.sample) { viewStart = 0.0; viewLen = 0.0; }
    slot = s;
    info = i;
    setTooltip (info.loaded() ? tr (lang, "waveTip") : juce::String());
    repaint();
}

void WaveView::computePeaks()
{
    peaks.clear();
    if (! info.loaded()) { peaksFor = nullptr; return; }
    const auto& L = info.sample->left;
    const auto& R = info.sample->right;
    const int len = info.sample->length();

    // the loudest point of the whole sample: zooming does not change the scale
    if (peaksFor != info.sample.get())
    {
        loudest = 1.0e-6f;
        const int step = juce::jmax (1, len / 200000);
        for (int i = 0; i < len; i += step) loudest = juce::jmax (loudest, std::abs (L[(size_t) i]), std::abs (R[(size_t) i]));
        peaksFor = info.sample.get();
    }
    peaksStart = visStart();
    peaksLen = visLen();

    const int columns = juce::jmax (1, (int) (waveArea().getWidth() / 3.0f));
    peaks.assign ((size_t) columns, 0.0f);
    for (int c = 0; c < columns; ++c)
    {
        const int a = juce::jlimit (0, len - 1, (int) (peaksStart + peaksLen * c / columns));
        const int b = juce::jlimit (a + 1, len, (int) (peaksStart + peaksLen * (c + 1) / columns));
        const int step = juce::jmax (1, (b - a) / 400);
        float m = 0.0f;
        for (int i = a; i < b; i += step) m = juce::jmax (m, std::abs (L[(size_t) i]), std::abs (R[(size_t) i]));
        peaks[(size_t) c] = juce::jmin (1.0f, m / loudest);
    }
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

    if (info.sample.get() != peaksFor || std::abs (peaksStart - visStart()) > 0.5 || std::abs (peaksLen - visLen()) > 0.5
        || (int) peaks.size() != juce::jmax (1, (int) (waveArea().getWidth() / 3.0f))) computePeaks();
    const int sel = juce::jlimit (0, juce::jmax (0, info.numSlices - 1), processor.selectedSlice (slot));
    const int n = (int) info.starts.size();
    const auto& params = info.params;
    const bool here = tele.currentSlot == slot;

    const float left = a.getX(), right = a.getRight();
    auto visible = [&] (float x) { return x >= left - 0.5f && x <= right + 0.5f; };

    // selected slice region
    {
        const float x0 = juce::jlimit (left, right, xFor (info.starts[(size_t) sel]));
        const float x1 = juce::jlimit (left, right, sel + 1 < n ? xFor (info.starts[(size_t) sel + 1]) : xFor (info.sample->length()));
        g.setColour (colours::accent.withAlpha (0.10f));
        g.fillRect (juce::Rectangle<float> (x0, a.getY(), juce::jmax (0.0f, x1 - x0), a.getHeight()));
    }

    g.setColour (colours::screenEdge);
    g.fillRect (a.getX(), a.getCentreY(), a.getWidth(), 1.0f);

    // waveform as columns
    const int columns = (int) peaks.size();
    int k = 0;
    for (int c = 0; c < columns; ++c)
    {
        const int frame = (int) (peaksStart + peaksLen * c / columns);
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
        if (! visible (x)) continue;
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
        const auto gc = colours::group (sp.group);   // the label shows the slice's modulation group
        if (on)
        {
            g.setColour (gc);
            g.fillRoundedRectangle (box, 3.0f);
            g.setColour (dark);
        }
        else
        {
            g.setColour (colours::screen);
            g.fillRoundedRectangle (box, 3.0f);
            g.setColour (gc.withAlpha (sp.mute ? 0.2f : 0.5f));
            g.drawRoundedRectangle (box.reduced (0.5f), 3.0f, 1.0f);
            g.setColour (sp.mute ? colours::faint : (sp.reverse ? colours::reverse : gc));
        }
        g.drawText (label, box, juce::Justification::centred);
        lastLabelEnd = box.getRight();
    }

    // where the sound is
    if (here)
    {
        const float total = (float) totalFrames();
        g.setColour (colours::accent.withAlpha (0.9f));
        for (int i = 0; i < tele.numVoicePos; ++i)
        {
            const float x = xFor ((int) (tele.voicePos[i] * total));
            if (visible (x)) g.fillRect (x, a.getY() + 6.0f, 1.0f, a.getHeight() - 12.0f);
        }
        if (tele.running && tele.loopFrac >= 0.0f)
        {
            const float x = xFor ((int) (tele.loopFrac * total));
            g.setColour (colours::text.withAlpha (0.85f));
            if (visible (x)) g.fillRect (x - 1.0f, a.getY() - 4.0f, 2.0f, a.getHeight() + 8.0f);
        }
    }

    // overview bar: where the view is inside the whole sample
    {
        const auto bar = scrollArea();
        g.setColour (colours::screenEdge);
        g.fillRoundedRectangle (bar, bar.getHeight() * 0.5f);
        const float t0 = (float) (visStart() / totalFrames()), t1 = (float) ((visStart() + visLen()) / totalFrames());
        auto thumb = juce::Rectangle<float> (bar.getX() + bar.getWidth() * t0, bar.getY(), juce::jmax (8.0f, bar.getWidth() * (t1 - t0)), bar.getHeight());
        g.setColour (viewLen > 0.0 ? colours::sub : colours::line);
        g.fillRoundedRectangle (thumb, bar.getHeight() * 0.5f);
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
    scrolling = false;
    if (! info.loaded())
    {
        if (onLoadRequest) onLoadRequest();
        return;
    }
    if (e.position.y >= scrollArea().getY() - 2.0f)
    {
        // the overview bar: grab the view, or jump to where it was clicked
        const auto bar = scrollArea();
        const double frame = (e.position.x - bar.getX()) / bar.getWidth() * totalFrames();
        if (viewLen > 0.0 && (frame < visStart() || frame > visStart() + visLen()))
            viewStart = juce::jlimit (0.0, totalFrames() - viewLen, frame - viewLen * 0.5);
        scrolling = true;
        scrollGrab = frame - visStart();
        repaint();
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
    if (scrolling)
    {
        if (viewLen <= 0.0) return;
        const auto bar = scrollArea();
        const double frame = (e.position.x - bar.getX()) / bar.getWidth() * totalFrames();
        viewStart = juce::jlimit (0.0, totalFrames() - viewLen, frame - scrollGrab);
        repaint();
        return;
    }
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
    if (e.position.y >= scrollArea().getY() - 2.0f) { viewStart = 0.0; viewLen = 0.0; repaint(); return; }
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
    int choiceIndex (RebanadaProcessor& p, const juce::String& id) { return juce::roundToInt (p.apvts.getRawParameterValue (id)->load()); }
    void setChoice (RebanadaProcessor& p, const juce::String& id, int index)
    {
        if (auto* param = p.apvts.getParameter (id))
        {
            param->beginChangeGesture();
            param->setValueNotifyingHost (param->convertTo0to1 ((float) index));
            param->endChangeGesture();
        }
    }
}

//==============================================================================
namespace
{
    const FloatSpec& divisionSpec()
    {
        static const FloatSpec s { "div", "Division", "División", 0.0f, (float) (modlib::numDivisions - 1), 4.0f, "", 0, FloatSpec::linear, false,
                                   "Length of one cycle, locked to the tempo", "Duración de un ciclo, atada al tempo", 1.0f };
        return s;
    }
    const FloatSpec& modSpec (const char* id) { return findSpec (modSpecs(), id); }

    juce::Path shapeIcon (int shape, juce::Rectangle<float> r)
    {
        juce::Path p;
        auto pt = [&] (float x, float y) { return juce::Point<float> (r.getX() + x * r.getWidth(), r.getY() + (0.5f - 0.5f * y) * r.getHeight()); };
        if (shape == modlib::steps)
        {
            const float v[] { 0.2f, 0.9f, -0.6f, 0.4f, -0.9f };
            p.startNewSubPath (pt (0.0f, v[0]));
            for (int i = 0; i < 5; ++i) { p.lineTo (pt (i / 5.0f, v[i])); p.lineTo (pt ((i + 1) / 5.0f, v[i])); }
            return p;
        }
        const float a[] { 0.2f, 0.9f, -0.6f, 0.4f, -0.9f, 0.1f };
        for (int i = 0; i <= 60; ++i)
        {
            const float t = i / 60.0f;
            float y = 0.0f;
            if (shape == modlib::smooth)
            {
                const int k = juce::jmin (4, (int) (t * 5.0f));
                const float f = t * 5.0f - k, sm = 0.5f - 0.5f * std::cos (f * pi);
                y = a[k] + (a[k + 1] - a[k]) * sm;
            }
            else y = modlib::shapeValue (shape, t * 2.0, 0.0f, 0.0f);
            if (i == 0) p.startNewSubPath (pt (t, y)); else p.lineTo (pt (t, y));
        }
        return p;
    }

    juce::String groupSourceName (int s, Lang lang)
    {
        if (s <= 0) return tr (lang, "none");
        static const char* n[] { "", "LFO 1", "LFO 2", "ENV", "RND" };
        return n[juce::jlimit (0, 4, s)];
    }
    juce::String loopSourceName (int s, Lang lang)
    {
        if (s <= 0) return tr (lang, "none");
        return groupLetter ((s - 1) / 2) + u8 ("·LFO ") + juce::String ((s - 1) % 2 + 1);
    }
    juce::String badgeFor (int s, bool loop)
    {
        if (s <= 0) return {};
        if (loop) return groupLetter ((s - 1) / 2) + juce::String ((s - 1) % 2 + 1);
        static const char* n[] { "", "L1", "L2", "ENV", "RND" };
        return n[juce::jlimit (0, 4, s)];
    }
}

//==============================================================================
void GroupPicker::paint (juce::Graphics& g)
{
    const float w = getWidth() / (float) numGroups;
    for (int i = 0; i < numGroups; ++i)
    {
        auto cell = juce::Rectangle<float> (i * w, 0.0f, w, (float) getHeight()).reduced (2.0f, 0.5f);
        const auto c = colours::group (i);
        const bool on = i == value;
        const float alpha = isEnabled() ? 1.0f : 0.4f;
        if (on) { g.setColour (c.withMultipliedAlpha (alpha)); g.fillRoundedRectangle (cell, 5.0f); }
        else
        {
            g.setColour (colours::ground);
            g.fillRoundedRectangle (cell, 5.0f);
            g.setColour (c.withAlpha (0.5f * alpha));
            g.drawRoundedRectangle (cell.reduced (0.5f), 5.0f, 1.0f);
        }
        g.setColour (on ? dark : c.withMultipliedAlpha (alpha));
        g.setFont (monoBold (13.0f));
        g.drawText (groupLetter (i), cell, juce::Justification::centred);
    }
}

void GroupPicker::mouseDown (const juce::MouseEvent& e)
{
    if (! isEnabled()) return;
    const int i = juce::jlimit (0, numGroups - 1, (int) (e.position.x / getWidth() * numGroups));
    value = i;
    if (onPick) onPick (i);
    repaint();
}

//==============================================================================
void ShapePicker::paint (juce::Graphics& g)
{
    if (param == nullptr) return;
    const int n = modlib::numShapes;
    const int current = juce::roundToInt (param->getValue() * (n - 1));
    const float w = getWidth() / (float) n;
    for (int i = 0; i < n; ++i)
    {
        auto cell = juce::Rectangle<float> (i * w, 0.0f, w, (float) getHeight()).reduced (2.0f, 0.5f);
        const bool on = i == current;
        g.setColour (colours::ground);
        g.fillRoundedRectangle (cell, 5.0f);
        g.setColour (on ? colour : colours::line);
        g.drawRoundedRectangle (cell.reduced (0.5f), 5.0f, 1.0f);
        g.setColour (on ? colour : colours::sub);
        g.strokePath (shapeIcon (i, cell.reduced (7.0f, 7.0f)), juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
}

void ShapePicker::mouseDown (const juce::MouseEvent& e)
{
    if (param == nullptr) return;
    const int n = modlib::numShapes;
    const int i = juce::jlimit (0, n - 1, (int) (e.position.x / getWidth() * n));
    param->beginChangeGesture();
    param->setValueNotifyingHost ((float) i / (float) (n - 1));
    param->endChangeGesture();
    repaint();
}

//==============================================================================
RouteRow::RouteRow (RebanadaProcessor& p) : processor (p)
{
    amount.setSliderStyle (juce::Slider::LinearHorizontal);
    amount.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    amount.setDoubleClickReturnValue (true, 0.0);
    amount.getProperties().set ("bipolar", true);
    amount.onValueChange = [this] { repaint(); };
    source.onChange = [this] { repaint(); };
    for (auto* b : { &source, &target }) b->getProperties().set ("small", true);
    for (auto* c : { (juce::Component*) &source, (juce::Component*) &target, (juce::Component*) &amount }) addAndMakeVisible (c);
}

juce::String RouteRow::id (const char* what) const { return group < 0 ? loopRouteId (index, what) : groupRouteId (group, index, what); }

void RouteRow::attach (int g, int i)
{
    group = g; index = i;
    sourceAttachment.reset(); targetAttachment.reset(); amountAttachment.reset();
    source.clear (juce::dontSendNotification);
    target.clear (juce::dontSendNotification);
    source.addItemList (group < 0 ? loopSourceChoices() : groupSourceChoices(), 1);
    juce::StringArray names;
    for (const auto& d : group < 0 ? loopDestinations() : voiceDestinations()) names.add (u8 (d.en));
    target.addItemList (names, 1);
    sourceAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (processor.apvts, id ("Src"), source);
    targetAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (processor.apvts, id ("Dest"), target);
    amountAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.apvts, id ("Amt"), amount);
    amount.setDoubleClickReturnValue (true, 0.0);
    setLanguage (lang);
}

void RouteRow::setLanguage (Lang l)
{
    lang = l;
    // read the selection before renaming: a combo box forgets it when its text changes
    const int srcIndex = juce::roundToInt (processor.apvts.getRawParameterValue (id ("Src"))->load());
    const int dstIndex = juce::roundToInt (processor.apvts.getRawParameterValue (id ("Dest"))->load());
    for (int k = 0; k < source.getNumItems(); ++k)
        source.changeItemText (k + 1, group < 0 ? loopSourceName (k, l) : groupSourceName (k, l));
    const auto& d = group < 0 ? loopDestinations() : voiceDestinations();
    for (int k = 0; k < (int) d.size(); ++k) target.changeItemText (k + 1, u8 (l == Lang::es ? d[(size_t) k].es : d[(size_t) k].en));
    source.setText (source.getItemText (srcIndex), juce::dontSendNotification);
    target.setText (target.getItemText (dstIndex), juce::dontSendNotification);
    amount.setTooltip (u8 (l == Lang::es ? routeAmountSpec().noteEs : routeAmountSpec().noteEn));
    repaint();
}

void RouteRow::resized()
{
    source.setBounds (18, 0, 82, getHeight());
    target.setBounds (114, 0, 104, getHeight());
    amount.setBounds (222, 0, 66, getHeight());
}

void RouteRow::paint (juce::Graphics& g)
{
    amount.getProperties().set ("fill", (juce::int64) colour.getARGB());
    const bool active = source.getSelectedItemIndex() > 0;
    g.setColour (active ? colour : colours::faint);
    g.setFont (mono (11.0f));
    g.drawText (juce::String (index + 1), 0, 0, 16, getHeight(), juce::Justification::centred);
    g.setColour (colours::sub);
    g.drawText (u8 ("→"), 100, 0, 14, getHeight(), juce::Justification::centred);
    g.setColour (active ? colours::text : colours::faint);
    g.drawText (localNumber (formatValue (routeAmountSpec(), (float) amount.getValue()), lang), getWidth() - 40, 0, 40, getHeight(), juce::Justification::centredRight);
}

//==============================================================================
ModPage::ModPage (RebanadaProcessor& p)
    : processor (p),
      sources ({ "LFO 1", "LFO 2", "ENV", "RND" }, false, [this] { return source; }, [this] (int i) { showSource (i); }),
      sync ({ "free", "sync" }, false,
            [this] { return target < loopTarget && source < 2 ? choiceIndex (processor, groupId (target, "lfo" + juce::String (source + 1) + "Sync")) : 0; },
            [this] (int i) { if (target < loopTarget && source < 2) setChoice (processor, groupId (target, "lfo" + juce::String (source + 1) + "Sync"), i); resized(); }),
      rate (modSpec ("lfo1Rate"), false), division (divisionSpec(), false), phase (modSpec ("lfo1Phase"), false),
      envAttack (modSpec ("envAttack"), false), envHold (modSpec ("envHold"), false), envDecay (modSpec ("envDecay"), false)
{
    sources.setPlainLabels (true);
    sources.monoFont = true;
    sources.fontSize = 12.0f;
    sync.fontSize = 11.5f;
    sync.capsLabels = true;
    rate.setNames ("Rate", "Velocidad");
    division.setNames ("Division", "División");
    phase.setNames ("Phase", "Fase");
    envAttack.setNames ("Attack", "Ataque");
    envHold.setNames ("Hold", "Sostén");
    envDecay.setNames ("Decay", "Caída");
    for (auto* c : { (juce::Component*) &sources, (juce::Component*) &sync, (juce::Component*) &shapes, (juce::Component*) &rate,
                     (juce::Component*) &division, (juce::Component*) &phase, (juce::Component*) &envAttack,
                     (juce::Component*) &envHold, (juce::Component*) &envDecay })
        addChildComponent (c);
    for (int i = 0; i < groupRoutes; ++i) addChildComponent (rows.add (new RouteRow (p)));
    showTarget (0);
}

void ModPage::setLanguage (Lang l)
{
    lang = l;
    for (auto* k : { &rate, &division, &phase, &envAttack, &envHold, &envDecay }) k->setLanguage (l);
    sources.setLanguage (l);
    sync.setLanguage (l);
    for (auto* r : rows) r->setLanguage (l);
    shapes.setTooltip (tr (l, "shapeTip"));
    sync.setTooltip (tr (l, "syncTip"));
    repaint();
}

juce::Rectangle<int> ModPage::tabArea (int t) const
{
    if (t < loopTarget) return { 16 + t * 42, 12, 38, 26 };
    return { 16 + numGroups * 42 + 4, 12, 66, 26 };
}

void ModPage::showTarget (int t)
{
    target = juce::jlimit (0, loopTarget, t);
    attachControls();
    repaint();
}

void ModPage::showSource (int s)
{
    source = juce::jlimit (0, 3, s);
    attachControls();
    repaint();
}

void ModPage::attachControls()
{
    auto& apvts = processor.apvts;
    const bool group = target < loopTarget;
    const auto c = colour();
    sources.onColour = c;
    sync.onColour = c;
    shapes.colour = c;
    if (group)
    {
        const juce::String n ("lfo" + juce::String ((source < 2 ? source : 0) + 1));
        shapes.param = apvts.getParameter (groupId (target, n + "Shape"));
        rate.attach (apvts, groupId (target, n + "Rate"));
        division.attach (apvts, groupId (target, n + "Div"));
        phase.attach (apvts, groupId (target, n + "Phase"));
        envAttack.attach (apvts, groupId (target, "envAttack"));
        envHold.attach (apvts, groupId (target, "envHold"));
        envDecay.attach (apvts, groupId (target, "envDecay"));
    }
    for (int i = 0; i < rows.size(); ++i)
    {
        const bool shown = group || i < loopRoutes;
        rows[i]->setVisible (shown);
        if (shown) rows[i]->attach (group ? target : -1, i);
        rows[i]->colour = c;
    }
    resized();
}

void ModPage::resized()
{
    const bool group = target < loopTarget;
    const bool lfo = group && source < 2;
    const bool synced = lfo && choiceIndex (processor, groupId (target, "lfo" + juce::String (source + 1) + "Sync")) == 1;
    sources.setVisible (group);
    sources.setBounds (16, 48, 340, 28);
    shapes.setVisible (lfo);
    shapes.setBounds (12, 84, 216, 28);
    sync.setVisible (lfo);
    sync.setBounds (236, 84, 120, 28);
    rate.setVisible (lfo && ! synced);
    division.setVisible (lfo && synced);
    phase.setVisible (lfo);
    rate.setBounds (12, 118, Knob::width, Knob::height);
    division.setBounds (12, 118, Knob::width, Knob::height);
    phase.setBounds (88, 118, Knob::width, Knob::height);
    const bool env = group && source == 2;
    for (auto* k : { &envAttack, &envHold, &envDecay }) k->setVisible (env);
    envAttack.setBounds (12, 104, Knob::width, Knob::height);
    envHold.setBounds (88, 104, Knob::width, Knob::height);
    envDecay.setBounds (164, 104, Knob::width, Knob::height);
    for (int i = 0; i < rows.size(); ++i) rows[i]->setBounds (372, 74 + i * 26, 325, 24);
}

void ModPage::update (const Telemetry& t, int, const std::array<int, numGroups>& m)
{
    tele = t;
    members = m;
    if (target < loopTarget && source < 2)
    {
        const bool synced = choiceIndex (processor, groupId (target, "lfo" + juce::String (source + 1) + "Sync")) == 1;
        if (synced != division.isVisible()) resized();
        shapes.repaint();
        sync.repaint();
    }
    repaint (previewArea().expanded (4.0f).getSmallestIntegerContainer());
    if (target == loopTarget) repaint (0, 110, 360, 120);
}

void ModPage::mouseDown (const juce::MouseEvent& e)
{
    for (int t = 0; t <= loopTarget; ++t)
        if (tabArea (t).contains (e.getPosition())) { showTarget (t); return; }
}

void ModPage::paintPreview (juce::Graphics& g)
{
    const auto box = previewArea();
    g.setColour (colours::screen);
    g.fillRoundedRectangle (box, 6.0f);
    g.setColour (colours::screenEdge);
    g.drawRoundedRectangle (box.reduced (0.5f), 6.0f, 1.0f);
    const auto r = box.reduced (8.0f, 10.0f);
    g.setColour (colours::line);
    g.fillRect (r.getX(), r.getCentreY(), r.getWidth(), 1.0f);
    const auto c = colour();
    juce::Path p;
    if (source < 2)
    {
        const int shape = choiceIndex (processor, groupId (target, "lfo" + juce::String (source + 1) + "Shape"));
        p = shapeIcon (shape, r);
        g.setColour (c);
        g.strokePath (p, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        // where the LFO is now
        const float v = tele.groupLfo[target][source];
        const float y = r.getY() + (0.5f - 0.5f * v) * r.getHeight();
        g.setColour (colours::text);
        g.fillEllipse (r.getRight() - 3.0f, y - 3.5f, 7.0f, 7.0f);
    }
    else if (source == 2)
    {
        const float a = processor.apvts.getRawParameterValue (groupId (target, "envAttack"))->load();
        const float h = processor.apvts.getRawParameterValue (groupId (target, "envHold"))->load();
        const float d = processor.apvts.getRawParameterValue (groupId (target, "envDecay"))->load();
        const float total = juce::jmax (1.0f, a + h + d);
        auto x = [&] (float ms) { return r.getX() + r.getWidth() * std::sqrt (ms / total); };   // a square-root time axis shows short attacks
        p.startNewSubPath (r.getX(), r.getBottom());
        p.lineTo (x (a), r.getY());
        p.lineTo (x (a + h), r.getY());
        p.lineTo (r.getRight(), r.getBottom());
        g.setColour (c);
        g.strokePath (p, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    else
    {
        const float v[] { 0.5f, -0.7f, 0.2f, 0.9f, -0.3f, -0.9f, 0.6f, 0.0f };
        const float w = r.getWidth() / 8.0f;
        g.setColour (c);
        for (int i = 0; i < 8; ++i)
        {
            const float y = r.getCentreY() - v[i] * r.getHeight() * 0.5f;
            g.fillRect (juce::Rectangle<float> (r.getX() + i * w + 2.0f, juce::jmin (y, r.getCentreY()), w - 4.0f, std::abs (y - r.getCentreY()) + 1.0f));
        }
    }
}

void ModPage::paint (juce::Graphics& g)
{
    using namespace colours;
    const auto b = getLocalBounds().toFloat();
    g.setColour (panel);
    g.fillRoundedRectangle (b, 10.0f);
    g.setColour (panelEdge);
    g.drawRoundedRectangle (b.reduced (0.5f), 10.0f, 1.0f);

    // what is being edited: a group or the loop routes
    for (int t = 0; t <= loopTarget; ++t)
    {
        const auto r = tabArea (t).toFloat();
        const auto c = t < loopTarget ? group (t) : loopMod;
        const bool on = t == target;
        if (on) { g.setColour (c); g.fillRoundedRectangle (r, 5.0f); }
        else
        {
            g.setColour (ground);
            g.fillRoundedRectangle (r, 5.0f);
            g.setColour (c.withAlpha (0.45f));
            g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, 1.0f);
        }
        g.setColour (on ? dark : c);
        if (t < loopTarget) { g.setFont (monoBold (13.0f)); g.drawText (groupLetter (t), r, juce::Justification::centred); }
        else drawCaps (g, tr (lang, "loopTab"), r, juce::Justification::centred, 10.5f);
    }
    g.setColour (sub);
    g.setFont (mono (11.5f));
    const float infoX = (float) tabArea (loopTarget).getRight() + 14.0f;
    if (target < loopTarget)
    {
        const int n = members[(size_t) target];
        g.drawText (juce::String (n) + " " + tr (lang, n == 1 ? "sliceInGroup" : "slicesInGroup") + " " + groupLetter (target),
                    juce::Rectangle<float> (infoX, 12.0f, 690.0f - infoX, 26.0f), juce::Justification::centredLeft);
    }
    else g.drawText (tr (lang, "loopRoutesInfo"), juce::Rectangle<float> (infoX, 12.0f, 690.0f - infoX, 26.0f), juce::Justification::centredLeft);

    // routes
    g.setColour (sub);
    drawCaps (g, tr (lang, "routes"), { 372.0f, 48.0f, 120.0f, 20.0f }, juce::Justification::centredLeft, 10.5f);
    g.setColour (faint);
    g.setFont (sans (12.0f));
    g.drawText (tr (lang, target < loopTarget ? "routesGroup" : "routesLoop"), juce::Rectangle<float> (440.0f, 48.0f, 257.0f, 20.0f), juce::Justification::centredRight);
    g.setColour (line);
    g.fillRect (362.0f, 50.0f, 1.0f, 172.0f);

    if (target < loopTarget)
    {
        if (source == 3)
        {
            g.setColour (sub);
            g.setFont (sans (12.5f));
            g.drawFittedText (tr (lang, "rndMore"), juce::Rectangle<int> (16, 84, 226, 110), juce::Justification::topLeft, 6);
        }
        paintPreview (g);
        g.setColour (sub);
        g.setFont (sans (12.0f));
        const char* help[] { "lfoHelp", "lfoHelp", "envHelp", "rndHelp" };
        g.drawFittedText (tr (lang, help[source]), juce::Rectangle<int> (16, 206, 340, 24), juce::Justification::centredLeft, 2);
    }
    else
    {
        g.setColour (sub);
        g.setFont (sans (12.5f));
        g.drawFittedText (tr (lang, "loopHelp"), juce::Rectangle<int> (16, 48, 336, 56), juce::Justification::topLeft, 3);
        // the eight LFOs, live
        const float w = 340.0f / 8.0f;
        for (int i = 0; i < numGroups * 2; ++i)
        {
            const int gr = i / 2, l = i % 2;
            const auto cell = juce::Rectangle<float> (16.0f + i * w, 112.0f, w - 6.0f, 104.0f);
            g.setColour (screen);
            g.fillRoundedRectangle (cell, 5.0f);
            const auto bar = cell.reduced (8.0f, 8.0f).withTrimmedBottom (16.0f);
            g.setColour (line);
            g.fillRect (bar.getX(), bar.getCentreY(), bar.getWidth(), 1.0f);
            const float v = tele.groupLfo[gr][l];
            const float y = bar.getCentreY() - v * bar.getHeight() * 0.5f;
            g.setColour (group (gr));
            g.fillRect (juce::Rectangle<float> (bar.getX() + 4.0f, juce::jmin (y, bar.getCentreY()), bar.getWidth() - 8.0f, std::abs (y - bar.getCentreY()) + 1.0f));
            g.setFont (monoBold (10.5f));
            g.drawText (groupLetter (gr) + juce::String (l + 1), cell.withTrimmedTop (cell.getHeight() - 20.0f), juce::Justification::centred);
        }
    }
}

Content::Content (RebanadaProcessor& p)
    : processor (p),
      slots (p),
      modeSwitch ({ "sync", "free" }, false, [&p] { return choiceIndex (p, loopId (p.editSlot(), "mode")); },
                  [&p] (int i) { setChoice (p, loopId (p.editSlot(), "mode"), i); }),
      tempo (p),
      outputDisplay (p),
      wave (p),
      midiDragger (p),
      filterSwitch ({ "LP", "BP", "HP" }, true, [&p] { return choiceIndex (p, loopId (p.editSlot(), "filterType")); },
                    [&p] (int i) { setChoice (p, loopId (p.editSlot(), "filterType"), i); }),
      orderSwitch ({ "forward", "random", "weighted" }, false, [&p] { return choiceIndex (p, loopId (p.editSlot(), "order")); },
                   [&p] (int i) { setChoice (p, loopId (p.editSlot(), "order"), i); }),
      stereoSwitch ({ "monoShort", "stereoShort" }, false, [this] { return shownStereo ? 1 : 0; },
                    [this] (int i) { const bool st = i == 1; editSlice ([st] (SliceParams& sp) { sp.stereo = st; if (st) sp.out &= ~1; }); refreshSlice(); }),
      keys (p),
      modPage (p)
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
    outputDisplay.output = true;
    addAndMakeVisible (outputDisplay);

    for (auto* box : { &switchBox, &lfoBox, &outBox }) addAndMakeVisible (box);
    switchBox.addItemList ({ "Instant", "1 beat", "1 bar", "Loop end" }, 1);
    lfoBox.addItemList ({ "Cutoff", "Pitch", "Pan", "Level" }, 1);
    switchAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (processor.apvts, "switchAt", switchBox);
    outBox.onChange = [this] {
        const int id = outBox.getSelectedId();
        if (id <= 0) return;
        const int channel = shownStereo ? (id - 1) * 2 : id - 1;
        editSlice ([channel] (SliceParams& sp) { sp.out = channel; });
    };
    stereoSwitch.setPlainLabels (false);
    stereoSwitch.fontSize = 11.5f;
    addAndMakeVisible (stereoSwitch);

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
        const int dest = voiceDestOf (sid, true);
        k->onModMenu = [this, k, dest, sid] { showModMenu (*k, dest, -1, true, u8 (lang == Lang::es ? sliceSpec (sid).es : sliceSpec (sid).en)); };
        addAndMakeVisible (k);
    }
    for (const char* id : { "attack", "decay", "sustain", "release", "cutoff", "reso", "transpose", "level" })
    {
        const juce::String sid (id);
        voiceIds.add (sid);
        auto* k = voiceKnobs.add (new Knob (spec (id), sid == "transpose" || sid == "level"));
        const int vd = voiceDestOf (sid, false), ld = vd < 0 ? loopDestOf (sid) : -1;
        k->onModMenu = [this, k, vd, ld, sid] { showModMenu (*k, vd, ld, false, u8 (lang == Lang::es ? spec (sid).es : spec (sid).en)); };
        addAndMakeVisible (k);
    }
    for (const char* id : { "chance", "repeat", "lfoRate", "lfoDepth" })
    {
        const juce::String sid (id);
        motionIds.add (id);
        auto* k = motionKnobs.add (new Knob (spec (id), false));
        const int ld = loopDestOf (sid);
        k->onModMenu = [this, k, ld, sid] { showModMenu (*k, -1, ld, false, u8 (lang == Lang::es ? spec (sid).es : spec (sid).en)); };
        addAndMakeVisible (k);
    }

    // modulation
    for (auto* b : { &tabLoop, &tabMod })
    {
        b->setClickingTogglesState (false);
        b->getProperties().set ("small", true);
        addAndMakeVisible (b);
    }
    tabLoop.onClick = [this] { showPage (0); };
    tabMod.onClick = [this] { showPage (1); };
    groupPicker.onPick = [this] (int g) {
        editSlice ([g] (SliceParams& sp) { sp.group = g; });
        wave.repaint();
        if (modPage.getTarget() != ModPage::loopTarget) modPage.showTarget (g);
        shownGroup = g;
        repaint (20, 440, 434, 266);
    };
    addAndMakeVisible (groupPicker);
    showModButton.getProperties().set ("onGround", true);
    showModButton.getProperties().set ("small", true);
    showModButton.onClick = [this] { modPage.showTarget (sliceGroup()); showPage (1); };
    addAndMakeVisible (showModButton);
    addChildComponent (modPage);
    page = (int) processor.apvts.state.getProperty ("page", 0);

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
    showPage (page);
    startTimerHz (30);
}

void Content::showPage (int p)
{
    page = juce::jlimit (0, 1, p);
    processor.apvts.state.setProperty ("page", page, nullptr);
    const bool sound = page == 0;
    for (auto* k : voiceKnobs) k->setVisible (sound);
    for (auto* k : motionKnobs) k->setVisible (sound);
    for (auto* c : { (juce::Component*) &filterSwitch, (juce::Component*) &orderSwitch, (juce::Component*) &lfoBox }) c->setVisible (sound);
    modPage.setVisible (! sound);
    tabLoop.setToggleState (sound, juce::dontSendNotification);
    tabMod.setToggleState (! sound, juce::dontSendNotification);
    repaint();
}

int Content::sliceGroup() const
{
    if (! info.loaded() || info.params.empty()) return 0;
    const int sel = juce::jlimit (0, (int) info.params.size() - 1, shownSelected < 0 ? 0 : shownSelected);
    return juce::jlimit (0, numGroups - 1, info.params[(size_t) sel].group);
}

// The arcs on the knobs: how far the routes move each one, and where it is now.
void Content::updateModulation()
{
    const int g = sliceGroup();
    auto groupMod = [&] (Knob& k, int d) {
        k.setModulation (processor.groupDepth (g, d), telemetry.groupOffset[g][d], colours::group (g), badgeFor (processor.groupSourceOf (g, d), false));
    };
    auto loopMod = [&] (Knob& k, int d) {
        k.setModulation (processor.loopDepth (d), telemetry.loopOffset[d], colours::loopMod, badgeFor (processor.loopSourceOf (d), true));
    };
    for (int i = 0; i < sliceKnobs.size(); ++i) groupMod (*sliceKnobs[i], i);
    for (int i = 0; i < voiceKnobs.size(); ++i)
    {
        const int vd = voiceDestOf (voiceIds[i], false);
        if (vd >= 0) groupMod (*voiceKnobs[i], vd); else loopMod (*voiceKnobs[i], loopDestOf (voiceIds[i]));
    }
    for (int i = 0; i < motionKnobs.size(); ++i) loopMod (*motionKnobs[i], loopDestOf (motionIds[i]));

    std::array<int, numGroups> members {};
    for (const auto& sp : info.params) ++members[(size_t) juce::jlimit (0, numGroups - 1, sp.group)];
    if (page == 1) modPage.update (telemetry, g, members);
}

void Content::showModMenu (Knob& knob, int voiceDest, int loopDest, bool sliceKnob, const juce::String& name)
{
    juce::PopupMenu m;
    m.addSectionHeader (caps (name) + u8 (" · ") + caps (tr (lang, "modWith")));
    const int g = sliceGroup();
    if (voiceDest >= 0)
    {
        auto addGroup = [&] (juce::PopupMenu& menu, int gr) {
            for (int s = srcLfo1; s <= srcRnd; ++s)
                menu.addItem (100 + gr * 10 + s, groupSourceName (s, lang), true, processor.groupHasRoute (gr, voiceDest, s));
        };
        m.addSectionHeader (tr (lang, "group") + " " + groupLetter (g) + (sliceKnob ? juce::String() : " (" + tr (lang, "thisSlice") + ")"));
        addGroup (m, g);
        if (! sliceKnob)
            for (int gr = 0; gr < numGroups; ++gr)
            {
                if (gr == g) continue;
                juce::PopupMenu sub;
                addGroup (sub, gr);
                m.addSubMenu (tr (lang, "group") + " " + groupLetter (gr), sub, true, nullptr, processor.groupSourceOf (gr, voiceDest) > 0);
            }
        m.addSeparator();
        m.addItem (200 + g, tr (lang, "removeMod") + " (" + groupLetter (g) + ")", processor.groupSourceOf (g, voiceDest) > 0);
    }
    else if (loopDest >= 0)
    {
        for (int s = 1; s <= numLoopSources; ++s)
            m.addItem (s, loopSourceName (s, lang), true, processor.loopHasRoute (loopDest, s));
        m.addSeparator();
        m.addItem (50, tr (lang, "removeMod"), processor.loopSourceOf (loopDest) > 0);
    }
    else return;
    m.addItem (60, tr (lang, "showModPage"));

    juce::Component::SafePointer<Content> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&knob.slider), [safe, voiceDest, loopDest, g] (int r) {
        if (safe == nullptr || r == 0) return;
        auto& p = safe->processor;
        auto removeOne = [&p] (const std::function<juce::String (int, const char*)>& idOf, int count, int dest, int src) {
            for (int k = 0; k < count; ++k)
                if (juce::roundToInt (p.apvts.getRawParameterValue (idOf (k, "Src"))->load()) == src
                    && juce::roundToInt (p.apvts.getRawParameterValue (idOf (k, "Dest"))->load()) == dest)
                    p.setParam (idOf (k, "Src"), 0.0f);
        };
        if (r > 100 && r < 200)
        {
            const int gr = (r - 100) / 10, src = (r - 100) % 10;
            if (p.groupHasRoute (gr, voiceDest, src))
                removeOne ([gr] (int k, const char* w) { return groupRouteId (gr, k, w); }, groupRoutes, voiceDest, src);
            else p.assignGroupMod (gr, voiceDest, src);
        }
        else if (r >= 200 && r < 210) p.removeGroupMod (r - 200, voiceDest);
        else if (r >= 1 && r <= numLoopSources)
        {
            if (p.loopHasRoute (loopDest, r)) removeOne ([] (int k, const char* w) { return loopRouteId (k, w); }, loopRoutes, loopDest, r);
            else p.assignLoopMod (loopDest, r);
        }
        else if (r == 50) p.removeLoopMod (loopDest);
        else if (r == 60)
        {
            safe->modPage.showTarget (voiceDest >= 0 ? g : ModPage::loopTarget);
            safe->showPage (1);
        }
        safe->updateModulation();
    });
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
    modPage.setLanguage (l);
    for (auto* s : { &modeSwitch, &filterSwitch, &orderSwitch, &stereoSwitch }) s->setLanguage (l);
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
    // the shown text does not follow changeItemText: select again
    const int lfoIndex = choiceIndex (processor, loopId (processor.editSlot(), "lfoDest"));
    switchBox.setSelectedItemIndex (choiceIndex (processor, "switchAt"), juce::dontSendNotification);
    lfoBox.setSelectedItemIndex (lfoIndex, juce::dontSendNotification);
    switchBox.setText (switchBox.getItemText (choiceIndex (processor, "switchAt")), juce::dontSendNotification);
    lfoBox.setText (lfoBox.getItemText (lfoIndex), juce::dontSendNotification);
    stereoSwitch.setTooltip (tr (lang, "stereoTip"));
    outBox.setTooltip (tr (lang, "outTip"));
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
    tabLoop.setButtonText (caps (tr (lang, "loopTab") + " " + juce::String (processor.editSlot() + 1) + u8 (" · ") + tr (lang, "tabSound")));
    tabMod.setButtonText (caps (tr (lang, "tabMod")));
    showModButton.setButtonText (tr (lang, "showMod") + u8 ("  ›"));
    groupPicker.setTooltip (tr (lang, "modGroupTip"));
    showModButton.setTooltip (tr (lang, "showModTip"));
}

void Content::attachLoop (int slot)
{
    attachedSlot = slot;
    for (int i = 0; i < voiceKnobs.size(); ++i) voiceKnobs[i]->attach (processor.apvts, loopId (slot, voiceIds[i].toRawUTF8()));
    for (int i = 0; i < motionKnobs.size(); ++i) motionKnobs[i]->attach (processor.apvts, loopId (slot, motionIds[i].toRawUTF8()));
    lfoAttachment.reset();
    lfoAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (processor.apvts, loopId (slot, "lfoDest"), lfoBox);
    updateTexts();
    for (auto* c : { (juce::Component*) &modeSwitch, (juce::Component*) &filterSwitch, (juce::Component*) &orderSwitch, (juce::Component*) &tempo })
        c->repaint();
}

void Content::refreshSlot (bool force)
{
    const int s = processor.editSlot();
    const int v = processor.getVersion();
    if (! force && s == shownSlot && v == shownVersion) return;
    if (s != attachedSlot) attachLoop (s);
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
                                (juce::Component*) &reverseButton, (juce::Component*) &muteButton, (juce::Component*) &outBox,
                                (juce::Component*) &stereoSwitch })
        c->setEnabled (loaded);
    for (auto* k : sliceKnobs) k->setEnabled (loaded);
    groupPicker.setEnabled (loaded);
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
    groupPicker.value = juce::jlimit (0, numGroups - 1, sp.group);
    groupPicker.repaint();
    reverseButton.setToggleState (sp.reverse, juce::dontSendNotification);
    muteButton.setToggleState (sp.mute, juce::dontSendNotification);
    fillOutputs (sp);
    wave.repaint();
    repaint (20, 440, 434, 50);
    if (sp.group != shownGroup)
    {
        shownGroup = sp.group;
        if (modPage.getTarget() != ModPage::loopTarget) modPage.showTarget (shownGroup);
    }
}

// The channel menu lists pairs (1–2 … 63–64) or single channels (1 … 64);
// the ones the track does not have are greyed out.
void Content::fillOutputs (const SliceParams& sp)
{
    const int outs = processor.numOutputs();
    shownStereo = sp.stereo;
    shownOutputs = outs;
    outBox.clear (juce::dontSendNotification);
    if (sp.stereo)
        for (int i = 0; i < maxOutputs / 2; ++i)
            outBox.addItem (juce::String (i * 2 + 1) + u8 ("–") + juce::String (i * 2 + 2), i + 1);
    else
        for (int i = 0; i < maxOutputs; ++i) outBox.addItem (juce::String (i + 1), i + 1);
    for (int i = 0; i < outBox.getNumItems(); ++i)
    {
        const int last = sp.stereo ? i * 2 + 1 : i;
        outBox.setItemEnabled (i + 1, last < outs || (sp.stereo && i == 0));
    }
    outBox.setSelectedId (sp.stereo ? sp.out / 2 + 1 : sp.out + 1, juce::dontSendNotification);
    stereoSwitch.repaint();
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

    if (processor.numOutputs() != shownOutputs)
    {
        refreshSlice();
        outputDisplay.repaint();   // it shows the channel count
    }

    std::array<bool, maxSlots> loaded {};
    for (int i = 0; i < maxSlots; ++i) loaded[(size_t) i] = processor.slotLoaded (i);
    slots.update (telemetry, s, loaded, lang);
    tempo.update (telemetry, lang);
    outputDisplay.update (telemetry, lang);
    wave.update (telemetry, lang);
    keys.update (telemetry, s, info.numSlices, shownSelected, lang);
    modeSwitch.repaint();
    filterSwitch.repaint();
    orderSwitch.repaint();
    updateModulation();
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
        drawCaps (g, tr (lang, "switchAt"), { 999.0f, 24.0f, 112.0f, 14.0f }, juce::Justification::centredLeft, 10.0f);
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
    const float panelY = 470.0f, panelH = 236.0f;
    const juce::Rectangle<float> panels[] { { 20.0f, 440.0f, 433.0f, 266.0f }, { 467.0f, panelY, 350.0f, panelH }, { 831.0f, panelY, 349.0f, panelH } };
    for (int i = 0; i < (page == 0 ? 3 : 1); ++i)
    {
        const auto& r = panels[i];
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
        drawCaps (g, tr (lang, "out"), { 232.0f, 598.0f, 48.0f, 32.0f }, juce::Justification::centredRight, 10.0f);
        drawCaps (g, tr (lang, "modGroup"), { 36.0f, 652.0f, 104.0f, 30.0f }, juce::Justification::centredLeft, 10.0f);
        if (page == 0)
        {
            drawCaps (g, tr (lang, "voice"), { 483.0f, 482.0f, 320.0f, 16.0f }, juce::Justification::centredLeft, 10.5f);
            drawCaps (g, tr (lang, "motion"), { 847.0f, 482.0f, 320.0f, 16.0f }, juce::Justification::centredLeft, 10.5f);
            drawCaps (g, tr (lang, "lfoTo") + u8 (" →"), { 847.0f, 658.0f, 60.0f, 30.0f }, juce::Justification::centredLeft, 10.5f);
        }
    }
}

void Content::resized()
{
    // header
    slots.setBounds (260, 26, 323, 44);
    playButton.setBounds (593, 26, 54, 44);
    modeSwitch.setBounds (657, 26, 116, 44);
    tempo.setBounds (783, 26, 112, 44);
    outputDisplay.setBounds (903, 26, 88, 44);
    switchBox.setBounds (999, 40, 110, 28);
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
    reverseButton.setBounds (36, 596, 104, 38);
    muteButton.setBounds (146, 596, 84, 38);
    stereoSwitch.setBounds (284, 599, 66, 32);
    outBox.setBounds (356, 599, 81, 32);

    // amp envelope and filter
    groupPicker.setBounds (140, 652, 168, 30);
    showModButton.setBounds (318, 652, 119, 30);

    // the loop's sound and order, or the modulation
    tabLoop.setBounds (467, 440, 230, 26);
    tabMod.setBounds (703, 440, 150, 26);
    for (int i = 0; i < 4; ++i) voiceKnobs[i]->setBounds (474 + i * 80, 508, Knob::width, Knob::height);
    filterSwitch.setBounds (483, 606, 46, 78);
    for (int i = 4; i < 8; ++i) voiceKnobs[i]->setBounds (534 + (i - 4) * 66, 602, Knob::width, Knob::height);
    orderSwitch.setBounds (847, 506, 317, 34);
    for (int i = 0; i < motionKnobs.size(); ++i) motionKnobs[i]->setBounds (842 + i * 80, 554, Knob::width, Knob::height);
    lfoBox.setBounds (906, 658, 150, 30);
    modPage.setBounds (467, 470, 713, 236);

    keys.setBounds (20, 720, 1160, 64);
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
