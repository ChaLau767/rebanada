// Rebanada — processor
// Copyright (C) 2026 Lautaro Martín Vieyra
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Slicer.h"

using namespace rebanada;

namespace
{
    constexpr int minGap = 64;    // frames between two markers

    juce::String encodeSlices (const Slot& s)
    {
        juce::StringArray rows;
        for (int i = 0; i < s.numSlices; ++i)
        {
            const auto& p = s.slices[(size_t) i];
            rows.add (juce::String (s.starts[(size_t) i]) + " " + juce::String (p.pitch, 2) + " " + juce::String (p.fine, 1) + " "
                      + juce::String (p.level, 2) + " " + juce::String (p.pan, 3) + " " + juce::String (p.decay, 1) + " "
                      + juce::String (p.filter, 2) + " " + (p.reverse ? "1" : "0") + " " + (p.mute ? "1" : "0") + " " + juce::String (p.out)
                      + " " + (p.stereo ? "1" : "0") + " " + juce::String (p.group));
        }
        return rows.joinIntoString (";");
    }

    // format 1 (Rebanada 0.1): the output was a stereo pair; format 2: a channel, mono or stereo;
    // format 3 (0.3): also the modulation group
    void decodeSlices (Slot& s, const juce::String& text, int format)
    {
        auto rows = juce::StringArray::fromTokens (text, ";", "");
        rows.removeEmptyStrings();
        int n = 0;
        for (const auto& row : rows)
        {
            if (n >= maxSlices) break;
            auto f = juce::StringArray::fromTokens (row, " ", "");
            if (f.size() < 10) continue;
            const int start = f[0].getIntValue();
            if (start < 0 || start >= s.length() || (n > 0 && start <= s.starts[(size_t) n - 1])) continue;
            s.starts[(size_t) n] = n == 0 ? 0 : start;
            auto& p = s.slices[(size_t) n];
            p.pitch = juce::jlimit (-24.0f, 24.0f, f[1].getFloatValue());
            p.fine = juce::jlimit (-50.0f, 50.0f, f[2].getFloatValue());
            p.level = juce::jlimit (-36.0f, 6.0f, f[3].getFloatValue());
            p.pan = juce::jlimit (-1.0f, 1.0f, f[4].getFloatValue());
            p.decay = juce::jlimit (10.0f, decayFull, f[5].getFloatValue());
            p.filter = juce::jlimit (-4.0f, 4.0f, f[6].getFloatValue());
            p.reverse = f[7].getIntValue() != 0;
            p.mute = f[8].getIntValue() != 0;
            if (format < 2) { p.out = juce::jlimit (0, 7, f[9].getIntValue()) * 2; p.stereo = true; }
            else { p.out = juce::jlimit (0, maxOutputs - 1, f[9].getIntValue()); p.stereo = f.size() < 11 || f[10].getIntValue() != 0; }
            p.group = format >= 3 && f.size() > 11 ? juce::jlimit (0, numGroups - 1, f[11].getIntValue()) : 0;
            ++n;
        }
        if (n > 0) s.numSlices = n;
    }
}

// One output bus with as many channels as the track has (up to 64); stereo by default.
juce::AudioProcessor::BusesProperties RebanadaProcessor::buses()
{
    return BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true);
}

RebanadaProcessor::RebanadaProcessor()
    : AudioProcessor (buses()),
      apvts (*this, nullptr, "REBANADA", createLayout())
{
    formats.registerBasicFormats();
    auto r = [this] (const juce::String& id) { return apvts.getRawParameterValue (id); };
    refs.gain = r ("gain"); refs.tempo = r ("tempo"); refs.switchAt = r ("switchAt"); refs.play = r ("play");
    for (int l = 0; l < maxSlots; ++l)
    {
        auto& L = refs.loops[(size_t) l];
        auto q = [&] (const char* base) { return r (loopId (l, base)); };
        L = { q ("chance"), q ("repeat"), q ("lfoRate"), q ("lfoDepth"), q ("attack"), q ("decay"), q ("sustain"), q ("release"),
              q ("cutoff"), q ("reso"), q ("transpose"), q ("rate"), q ("level"), q ("mode"), q ("order"), q ("lfoDest"), q ("filterType") };
    }
    for (int g = 0; g < numGroups; ++g)
    {
        auto& G = refs.groups[(size_t) g];
        auto q = [&] (const juce::String& base) { return r (groupId (g, base)); };
        for (int l = 0; l < 2; ++l)
        {
            const juce::String n ("lfo" + juce::String (l + 1));
            G.shape[l] = q (n + "Shape"); G.sync[l] = q (n + "Sync"); G.div[l] = q (n + "Div");
            G.rate[l] = q (n + "Rate"); G.phase[l] = q (n + "Phase");
        }
        G.envAttack = q ("envAttack"); G.envHold = q ("envHold"); G.envDecay = q ("envDecay");
        for (int k = 0; k < groupRoutes; ++k)
            G.routes[(size_t) k] = { r (groupRouteId (g, k, "Src")), r (groupRouteId (g, k, "Dest")), r (groupRouteId (g, k, "Amt")) };
    }
    for (int k = 0; k < loopRoutes; ++k)
        refs.loopRoutes[(size_t) k] = { r (loopRouteId (k, "Src")), r (loopRouteId (k, "Dest")), r (loopRouteId (k, "Amt")) };
    if (! apvts.state.hasProperty ("lang")) apvts.state.setProperty ("lang", 0, nullptr);
}

bool RebanadaProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // any output count from 1 to 64, whatever the host calls the channels
    if (! layouts.inputBuses.isEmpty() && layouts.getMainInputChannels() != 0) return false;
    const int outs = layouts.getMainOutputChannels();
    return outs >= 1 && outs <= maxOutputs;
}

void RebanadaProcessor::prepareToPlay (double sampleRate, int)
{
    const juce::SpinLock::ScopedLockType lock (engineLock);
    engine.prepare (sampleRate);
    samplesSinceTelemetry = 0;
    keyboard.reset();
}

Params RebanadaProcessor::readParams() const noexcept
{
    Params p;
    p.gain = refs.gain->load();
    p.tempo = refs.tempo->load();
    p.switchAt = (int) refs.switchAt->load();
    p.play = refs.play->load() > 0.5f;
    for (int l = 0; l < maxSlots; ++l)
    {
        const auto& L = refs.loops[(size_t) l];
        auto& o = p.loops[(size_t) l];
        o.chance = L.chance->load();     o.repeat = L.repeat->load();
        o.lfoRate = L.lfoRate->load();   o.lfoDepth = L.lfoDepth->load();   o.lfoDest = (int) L.lfoDest->load();
        o.attack = L.attack->load();     o.decay = L.decay->load();         o.sustain = L.sustain->load();   o.release = L.release->load();
        o.cutoff = L.cutoff->load();     o.reso = L.reso->load();           o.filterType = (int) L.filterType->load();
        o.transpose = L.transpose->load(); o.rate = L.rate->load();         o.level = L.level->load();
        o.mode = (int) L.mode->load();   o.order = (int) L.order->load();
    }
    auto route = [] (const RouteRefs& R) {
        return ModRoute { juce::roundToInt (R.src->load()), juce::roundToInt (R.dest->load()), R.amt->load() * 0.01f };
    };
    for (int g = 0; g < numGroups; ++g)
    {
        const auto& G = refs.groups[(size_t) g];
        auto& o = p.groups[(size_t) g];
        for (int l = 0; l < 2; ++l)
        {
            o.shape[l] = juce::roundToInt (G.shape[l]->load());
            o.sync[l] = G.sync[l]->load() > 0.5f;
            o.division[l] = juce::roundToInt (G.div[l]->load());
            o.rate[l] = G.rate[l]->load();
            o.phase[l] = G.phase[l]->load() / 360.0f;
        }
        o.envAttack = G.envAttack->load(); o.envHold = G.envHold->load(); o.envDecay = G.envDecay->load();
        for (int k = 0; k < groupRoutes; ++k) o.routes[k] = route (G.routes[(size_t) k]);
    }
    for (int k = 0; k < loopRoutes; ++k) p.loopRoute[k] = route (refs.loopRoutes[(size_t) k]);
    return p;
}

//==============================================================================
void RebanadaProcessor::setParam (const juce::String& id, float value)
{
    if (auto* param = apvts.getParameter (id))
    {
        param->beginChangeGesture();
        param->setValueNotifyingHost (param->convertTo0to1 (value));
        param->endChangeGesture();
    }
}

bool RebanadaProcessor::groupHasRoute (int g, int dest, int source) const
{
    if (! juce::isPositiveAndBelow (g, numGroups)) return false;
    for (const auto& R : refs.groups[(size_t) g].routes)
        if (juce::roundToInt (R.src->load()) == source && juce::roundToInt (R.dest->load()) == dest) return true;
    return false;
}

bool RebanadaProcessor::loopHasRoute (int dest, int source) const
{
    for (const auto& R : refs.loopRoutes)
        if (juce::roundToInt (R.src->load()) == source && juce::roundToInt (R.dest->load()) == dest) return true;
    return false;
}

int RebanadaProcessor::groupSourceOf (int g, int dest) const
{
    if (! juce::isPositiveAndBelow (g, numGroups)) return 0;
    for (const auto& R : refs.groups[(size_t) g].routes)
    {
        const int s = juce::roundToInt (R.src->load());
        if (s > 0 && juce::roundToInt (R.dest->load()) == dest) return s;
    }
    return 0;
}

float RebanadaProcessor::groupDepth (int g, int dest) const
{
    if (! juce::isPositiveAndBelow (g, numGroups)) return 0.0f;
    float d = 0.0f;
    for (const auto& R : refs.groups[(size_t) g].routes)
        if (juce::roundToInt (R.src->load()) > 0 && juce::roundToInt (R.dest->load()) == dest) d += std::abs (R.amt->load()) * 0.01f;
    return juce::jmin (1.0f, d);
}

void RebanadaProcessor::assignGroupMod (int g, int dest, int source)
{
    if (! juce::isPositiveAndBelow (g, numGroups) || dest < 0 || source <= 0) return;
    const auto& G = refs.groups[(size_t) g];
    for (int k = 0; k < groupRoutes; ++k)   // already there
        if (juce::roundToInt (G.routes[(size_t) k].src->load()) == source && juce::roundToInt (G.routes[(size_t) k].dest->load()) == dest) return;
    for (int k = 0; k < groupRoutes; ++k)
        if (juce::roundToInt (G.routes[(size_t) k].src->load()) == 0)
        {
            setParam (groupRouteId (g, k, "Dest"), (float) dest);
            setParam (groupRouteId (g, k, "Src"), (float) source);
            if (std::abs (G.routes[(size_t) k].amt->load()) < 0.5f) setParam (groupRouteId (g, k, "Amt"), 30.0f);
            return;
        }
}

void RebanadaProcessor::removeGroupMod (int g, int dest)
{
    if (! juce::isPositiveAndBelow (g, numGroups)) return;
    const auto& G = refs.groups[(size_t) g];
    for (int k = 0; k < groupRoutes; ++k)
        if (juce::roundToInt (G.routes[(size_t) k].dest->load()) == dest && juce::roundToInt (G.routes[(size_t) k].src->load()) > 0)
            setParam (groupRouteId (g, k, "Src"), 0.0f);
}

int RebanadaProcessor::loopSourceOf (int dest) const
{
    for (const auto& R : refs.loopRoutes)
    {
        const int s = juce::roundToInt (R.src->load());
        if (s > 0 && juce::roundToInt (R.dest->load()) == dest) return s;
    }
    return 0;
}

float RebanadaProcessor::loopDepth (int dest) const
{
    float d = 0.0f;
    for (const auto& R : refs.loopRoutes)
        if (juce::roundToInt (R.src->load()) > 0 && juce::roundToInt (R.dest->load()) == dest) d += std::abs (R.amt->load()) * 0.01f;
    return juce::jmin (1.0f, d);
}

void RebanadaProcessor::assignLoopMod (int dest, int source)
{
    if (dest < 0 || source <= 0) return;
    for (int k = 0; k < loopRoutes; ++k)
        if (juce::roundToInt (refs.loopRoutes[(size_t) k].src->load()) == source && juce::roundToInt (refs.loopRoutes[(size_t) k].dest->load()) == dest) return;
    for (int k = 0; k < loopRoutes; ++k)
        if (juce::roundToInt (refs.loopRoutes[(size_t) k].src->load()) == 0)
        {
            setParam (loopRouteId (k, "Dest"), (float) dest);
            setParam (loopRouteId (k, "Src"), (float) source);
            if (std::abs (refs.loopRoutes[(size_t) k].amt->load()) < 0.5f) setParam (loopRouteId (k, "Amt"), 30.0f);
            return;
        }
}

void RebanadaProcessor::removeLoopMod (int dest)
{
    for (int k = 0; k < loopRoutes; ++k)
        if (juce::roundToInt (refs.loopRoutes[(size_t) k].dest->load()) == dest && juce::roundToInt (refs.loopRoutes[(size_t) k].src->load()) > 0)
            setParam (loopRouteId (k, "Src"), 0.0f);
}

void RebanadaProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();

    const int numOut = juce::jmin (getTotalNumOutputChannels(), buffer.getNumChannels(), maxOutputs);

    keyboard.processNextMidiBuffer (midi, 0, n, true);

    int numEvents = 0;
    for (const auto ev : midi)
    {
        if (numEvents >= (int) events.size()) break;
        const auto m = ev.getMessage();
        Event e;
        e.offset = ev.samplePosition;
        if (m.isNoteOn())                         { e.type = Event::noteOn; e.note = m.getNoteNumber(); e.velocity = m.getFloatVelocity(); }
        else if (m.isNoteOff())                   { e.type = Event::noteOff; e.note = m.getNoteNumber(); }
        else if (m.isAllNotesOff() || m.isAllSoundOff()) e.type = Event::allNotesOff;
        else continue;
        events[(size_t) numEvents++] = e;
    }

    Clock clock;
    if (! juce::JUCEApplicationBase::isStandaloneApp())
        if (auto* head = getPlayHead())
            if (auto pos = head->getPosition())
                if (auto ppq = pos->getPpqPosition())
                {
                    clock.hostAvailable = true;
                    clock.hostPlaying = pos->getIsPlaying();
                    clock.ppq = *ppq;
                    if (auto bpm = pos->getBpm()) clock.bpm = *bpm;
                    if (auto sig = pos->getTimeSignature()) { clock.numerator = sig->numerator; clock.denominator = sig->denominator; }
                }

    const juce::SpinLock::ScopedTryLockType lock (engineLock);
    if (! lock.isLocked() || numOut < 1)
    {
        buffer.clear();
        return;
    }

    const int req = requested.exchange (-1);
    if (req >= 0) engine.requestSlot (req);

    engine.process (buffer.getArrayOfWritePointers(), numOut, n, readParams(), clock, events.data(), numEvents);
    for (int ch = numOut; ch < buffer.getNumChannels(); ++ch) buffer.clear (ch, 0, n);
    midi.clear();

    engineCurrent = engine.getCurrentSlot();
    enginePending = engine.getPendingSlot();

    samplesSinceTelemetry += n;
    if (samplesSinceTelemetry >= (int) (getSampleRate() / 40.0))
    {
        const juce::SpinLock::ScopedTryLockType tl (telemetryLock);
        if (tl.isLocked())
        {
            engine.snapshot (telemetry);
            samplesSinceTelemetry = 0;
        }
    }
}

bool RebanadaProcessor::readTelemetry (Telemetry& dest)
{
    const juce::SpinLock::ScopedLockType lock (telemetryLock);
    dest = telemetry;
    telemetry.outPeak = 0.0f;
    return true;
}

int RebanadaProcessor::editSlot() const
{
    const int r = requested.load();
    if (r >= 0) return r;
    const int p = enginePending.load();
    return p >= 0 ? p : engineCurrent.load();
}

void RebanadaProcessor::requestSlot (int slot)
{
    if (slot >= 0 && slot < maxSlots) requested = slot;
}

//==============================================================================
template <typename F>
void RebanadaProcessor::edit (int slot, F&& f)
{
    if (slot < 0 || slot >= maxSlots) return;
    {
        const juce::SpinLock::ScopedLockType lock (engineLock);
        f (engine.slots[(size_t) slot]);
    }
    ++version;
}

RebanadaProcessor::SlotInfo RebanadaProcessor::slotInfo (int slot)
{
    SlotInfo info;
    if (slot < 0 || slot >= maxSlots) return info;
    const auto& m = meta[(size_t) slot];
    info.path = m.path;
    info.name = m.path.isEmpty() ? juce::String() : juce::File (m.path).getFileName();
    info.missing = m.missing;
    info.sensitivity = m.sensitivity;
    const juce::SpinLock::ScopedLockType lock (engineLock);
    const auto& s = engine.slots[(size_t) slot];
    info.sample = s.sample;
    info.bars = s.bars;
    info.numSlices = s.sample ? s.numSlices : 0;
    info.starts.assign (s.starts.begin(), s.starts.begin() + info.numSlices);
    info.params.assign (s.slices.begin(), s.slices.begin() + info.numSlices);
    return info;
}

void RebanadaProcessor::sliceSample (Slot& s, int slot)
{
    const auto& m = meta[(size_t) slot];
    std::vector<int> cuts;
    const double sr = s.sample ? s.sample->sampleRate : 44100.0;
    if (m.mono.empty()) cuts = { 0 };
    else if (getGrid()) cuts = slicer::grid (m.mono, juce::jlimit (1, maxSlices, s.bars * 16), getZeroSnap(), sr);
    else cuts = slicer::detect (m.mono, sr, m.sensitivity, getZeroSnap());
    s.numSlices = juce::jlimit (1, maxSlices, (int) cuts.size());
    for (int i = 0; i < s.numSlices; ++i)
    {
        s.starts[(size_t) i] = i == 0 ? 0 : cuts[(size_t) i];
        s.slices[(size_t) i] = SliceParams {};
    }
}

bool RebanadaProcessor::loadFile (int slot, const juce::File& file, juce::String& error)
{
    if (slot < 0 || slot >= maxSlots) return false;
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
    if (reader == nullptr) { error = "Unsupported or unreadable file"; return false; }
    const auto maxLength = (juce::int64) (reader->sampleRate * 60.0 * 20.0);
    const int length = (int) juce::jmin (reader->lengthInSamples, maxLength);
    if (length < 64) { error = "The file is too short"; return false; }

    juce::AudioBuffer<float> buf ((int) juce::jmax (1u, juce::jmin (2u, reader->numChannels)), length);
    reader->read (&buf, 0, length, 0, true, buf.getNumChannels() > 1);

    auto smp = std::make_shared<Sample>();
    smp->sampleRate = reader->sampleRate > 0 ? reader->sampleRate : 44100.0;
    smp->left.assign (buf.getReadPointer (0), buf.getReadPointer (0) + length);
    const float* r = buf.getReadPointer (buf.getNumChannels() > 1 ? 1 : 0);
    smp->right.assign (r, r + length);

    auto& m = meta[(size_t) slot];
    m.path = file.getFullPathName();
    m.missing = false;
    m.mono = slicer::mono (*smp);
    m.saved = {};

    std::shared_ptr<const Sample> old;
    const int bars = slicer::estimateBars (length, smp->sampleRate);
    {
        const juce::SpinLock::ScopedLockType lock (engineLock);
        auto& s = engine.slots[(size_t) slot];
        engine.stopVoicesOf (slot);
        old = std::move (s.sample);
        s.sample = smp;
        s.bars = bars;
        sliceSample (s, slot);
    }
    old.reset();    // freed here, on the message thread
    selected[(size_t) slot] = 0;
    ++version;
    return true;
}

void RebanadaProcessor::clearSlot (int slot)
{
    if (slot < 0 || slot >= maxSlots) return;
    std::shared_ptr<const Sample> old;
    {
        const juce::SpinLock::ScopedLockType lock (engineLock);
        auto& s = engine.slots[(size_t) slot];
        engine.stopVoicesOf (slot);
        old = std::move (s.sample);
        s.numSlices = 0;
    }
    meta[(size_t) slot] = Meta {};
    selected[(size_t) slot] = 0;
    ++version;
}

SliceParams RebanadaProcessor::getSlice (int slot, int slice)
{
    if (slot < 0 || slot >= maxSlots || slice < 0 || slice >= maxSlices) return {};
    const juce::SpinLock::ScopedLockType lock (engineLock);
    return engine.slots[(size_t) slot].slices[(size_t) slice];
}

void RebanadaProcessor::setSlice (int slot, int slice, const SliceParams& p)
{
    if (slice < 0 || slice >= maxSlices) return;
    edit (slot, [&] (Slot& s) { s.slices[(size_t) slice] = p; });
}

void RebanadaProcessor::setBars (int slot, int bars)
{
    edit (slot, [&] (Slot& s) { s.bars = juce::jlimit (1, 64, bars); });
}

void RebanadaProcessor::setSensitivity (int slot, float sensitivity)
{
    if (slot < 0 || slot >= maxSlots) return;
    meta[(size_t) slot].sensitivity = juce::jlimit (0.0f, 100.0f, sensitivity);
    reslice (slot);
}

void RebanadaProcessor::reslice (int slot)
{
    if (slot < 0 || slot >= maxSlots || meta[(size_t) slot].mono.empty()) return;
    edit (slot, [&] (Slot& s) {
        engine.stopVoicesOf (slot);
        sliceSample (s, slot);
    });
    selected[(size_t) slot] = juce::jmin (selected[(size_t) slot], slotInfo (slot).numSlices - 1);
}

int RebanadaProcessor::addMarker (int slot, int position)
{
    int result = -1;
    if (slot < 0 || slot >= maxSlots) return -1;
    const auto& mono = meta[(size_t) slot].mono;
    edit (slot, [&] (Slot& s) {
        if (s.empty() || s.numSlices >= maxSlices) return;
        int pos = position;
        if (getZeroSnap() && ! mono.empty()) pos = slicer::snapToZero (mono, pos, s.sample->sampleRate);
        if (pos < minGap || pos > s.length() - minGap) return;
        int i = 0;
        while (i + 1 < s.numSlices && s.starts[(size_t) i + 1] <= pos) ++i;
        if (pos - s.starts[(size_t) i] < minGap || s.sliceEnd (i) - pos < minGap) return;
        for (int k = s.numSlices; k > i + 1; --k)
        {
            s.starts[(size_t) k] = s.starts[(size_t) k - 1];
            s.slices[(size_t) k] = s.slices[(size_t) k - 1];
        }
        s.starts[(size_t) i + 1] = pos;
        s.slices[(size_t) i + 1] = s.slices[(size_t) i];
        ++s.numSlices;
        result = i + 1;
    });
    return result;
}

void RebanadaProcessor::moveMarker (int slot, int index, int position)
{
    edit (slot, [&] (Slot& s) {
        if (s.empty() || index <= 0 || index >= s.numSlices) return;
        const int lo = s.starts[(size_t) index - 1] + minGap;
        const int hi = (index + 1 < s.numSlices ? s.starts[(size_t) index + 1] : s.length()) - minGap;
        if (hi <= lo) return;
        s.starts[(size_t) index] = juce::jlimit (lo, hi, position);
    });
}

void RebanadaProcessor::removeMarker (int slot, int index)
{
    edit (slot, [&] (Slot& s) {
        if (s.empty() || index <= 0 || index >= s.numSlices) return;
        for (int k = index; k + 1 < s.numSlices; ++k)
        {
            s.starts[(size_t) k] = s.starts[(size_t) k + 1];
            s.slices[(size_t) k] = s.slices[(size_t) k + 1];
        }
        --s.numSlices;
    });
}

int RebanadaProcessor::split (int slot, int slice)
{
    const auto info = slotInfo (slot);
    if (! info.loaded() || slice < 0 || slice >= info.numSlices) return -1;
    const int a = info.starts[(size_t) slice];
    const int b = slice + 1 < info.numSlices ? info.starts[(size_t) slice + 1] : info.sample->length();
    return addMarker (slot, (a + b) / 2);
}

int RebanadaProcessor::merge (int slot, int slice)
{
    const auto info = slotInfo (slot);
    if (! info.loaded() || info.numSlices < 2) return slice;
    if (slice + 1 < info.numSlices) { removeMarker (slot, slice + 1); return slice; }
    removeMarker (slot, slice);
    return slice - 1;
}

//==============================================================================
Lang RebanadaProcessor::getLanguage() const
{
    return (int) apvts.state.getProperty ("lang", 0) == 1 ? Lang::es : Lang::en;
}

void RebanadaProcessor::setLanguage (Lang lang)
{
    apvts.state.setProperty ("lang", lang == Lang::es ? 1 : 0, nullptr);
}

juce::File RebanadaProcessor::writeMidiFile (int slot)
{
    const auto info = slotInfo (slot);
    if (! info.loaded()) return {};

    Telemetry t;
    readTelemetry (t);
    const double bpm = t.bpm > 1.0f ? t.bpm : 120.0;
    constexpr int ppq = 960;
    const double loopTicks = info.bars * 4.0 * ppq;
    const int len = info.sample->length();

    juce::MidiMessageSequence seq;
    seq.addEvent (juce::MidiMessage::tempoMetaEvent ((int) std::lround (60000000.0 / bpm)), 0);
    seq.addEvent (juce::MidiMessage::timeSignatureMetaEvent (4, 4), 0);
    for (int i = 0; i < info.numSlices; ++i)
    {
        const double a = (double) info.starts[(size_t) i] / len * loopTicks;
        const double b = (i + 1 < info.numSlices ? (double) info.starts[(size_t) i + 1] / len : 1.0) * loopTicks;
        const int note = sliceBaseNote + i;
        if (note > 127) break;
        seq.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), std::round (a));
        seq.addEvent (juce::MidiMessage::noteOff (1, note), std::max (std::round (a) + 1.0, std::round (b) - 1.0));
    }
    seq.updateMatchedPairs();

    juce::MidiFile mf;
    mf.setTicksPerQuarterNote (ppq);
    mf.addTrack (seq);

    auto name = juce::File (info.path).getFileNameWithoutExtension();
    if (name.isEmpty()) name = "loop";
    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("Rebanada");
    dir.createDirectory();
    auto f = dir.getChildFile ("Rebanada - " + juce::File::createLegalFileName (name) + ".mid");
    f.deleteFile();
    juce::FileOutputStream os (f);
    if (! os.openedOk() || ! mf.writeTo (os)) return {};
    return f;
}

//==============================================================================
juce::ValueTree RebanadaProcessor::saveSlots() const
{
    juce::ValueTree slots ("SLOTS");
    for (int i = 0; i < maxSlots; ++i)
    {
        const auto& m = meta[(size_t) i];
        if (m.path.isEmpty()) continue;
        if (m.missing && m.saved.isValid()) { slots.appendChild (m.saved.createCopy(), nullptr); continue; }
        juce::ValueTree t ("SLOT");
        t.setProperty ("index", i, nullptr);
        t.setProperty ("path", m.path, nullptr);
        t.setProperty ("sens", m.sensitivity, nullptr);
        t.setProperty ("selected", selected[(size_t) i], nullptr);
        const auto& s = engine.slots[(size_t) i];
        t.setProperty ("bars", s.bars, nullptr);
        t.setProperty ("slices", encodeSlices (s), nullptr);
        t.setProperty ("fmt", 3, nullptr);
        slots.appendChild (t, nullptr);
    }
    return slots;
}

void RebanadaProcessor::restoreSlots (const juce::ValueTree& slots)
{
    for (int i = 0; i < maxSlots; ++i) clearSlot (i);
    if (! slots.isValid()) return;
    for (const auto& t : slots)
    {
        const int i = t.getProperty ("index", -1);
        if (i < 0 || i >= maxSlots) continue;
        const juce::String path = t.getProperty ("path").toString();
        juce::String error;
        const juce::File file (path);
        if (path.isEmpty() || ! file.existsAsFile() || ! loadFile (i, file, error))
        {
            meta[(size_t) i].path = path;
            meta[(size_t) i].missing = true;
            meta[(size_t) i].saved = t.createCopy();
            continue;
        }
        meta[(size_t) i].sensitivity = t.getProperty ("sens", 50.0f);
        edit (i, [&] (Slot& s) {
            s.bars = juce::jlimit (1, 64, (int) t.getProperty ("bars", s.bars));
            decodeSlices (s, t.getProperty ("slices").toString(), (int) t.getProperty ("fmt", 1));
        });
        selected[(size_t) i] = juce::jlimit (0, juce::jmax (0, slotInfo (i).numSlices - 1), (int) t.getProperty ("selected", 0));
    }
}

void RebanadaProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("version", JucePlugin_VersionString, nullptr);
    state.setProperty ("editSlot", editSlot(), nullptr);
    state.removeChild (state.getChildWithName ("SLOTS"), nullptr);
    {
        const juce::SpinLock::ScopedLockType lock (engineLock);
        state.appendChild (saveSlots(), nullptr);
    }
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void RebanadaProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;

    // Sessions from 0.1: settings that were shared by all the loops now belong to each one
    for (const char* base : { "mode", "order", "lfoDest", "filterType", "chance", "repeat", "lfoRate", "lfoDepth", "attack",
                              "decay", "sustain", "release", "cutoff", "reso", "transpose", "rate" })
        if (auto* old = xml->getChildByAttribute ("id", base))
        {
            const auto value = old->getStringAttribute ("value");
            for (int l = 0; l < maxSlots; ++l)
                if (xml->getChildByAttribute ("id", loopId (l, base)) == nullptr)
                {
                    auto* e = xml->createNewChildElement ("PARAM");
                    e->setAttribute ("id", loopId (l, base));
                    e->setAttribute ("value", value);
                }
            xml->removeChildElement (old, true);
        }
    auto state = juce::ValueTree::fromXml (*xml);
    auto slots = state.getChildWithName ("SLOTS").createCopy();
    state.removeChild (state.getChildWithName ("SLOTS"), nullptr);
    apvts.replaceState (state);
    if (! apvts.state.hasProperty ("lang")) apvts.state.setProperty ("lang", 0, nullptr);
    restoreSlots (slots);
    requestSlot (juce::jlimit (0, maxSlots - 1, (int) state.getProperty ("editSlot", 0)));
    ++version;
}

juce::AudioProcessorEditor* RebanadaProcessor::createEditor() { return new RebanadaEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new RebanadaProcessor(); }
