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
                      + juce::String (p.filter, 2) + " " + (p.reverse ? "1" : "0") + " " + (p.mute ? "1" : "0") + " " + juce::String (p.out));
        }
        return rows.joinIntoString (";");
    }

    void decodeSlices (Slot& s, const juce::String& text)
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
            p.out = juce::jlimit (0, maxPairs - 1, f[9].getIntValue());
            ++n;
        }
        if (n > 0) s.numSlices = n;
    }
}

// Main output plus seven optional stereo outputs, for slices sent to their own tracks.
juce::AudioProcessor::BusesProperties RebanadaProcessor::buses()
{
        auto b = BusesProperties().withOutput ("Main 1-2", juce::AudioChannelSet::stereo(), true);
        for (int p = 1; p < maxPairs; ++p)
            b = b.withOutput ("Out " + juce::String (p * 2 + 1) + "-" + juce::String (p * 2 + 2), juce::AudioChannelSet::stereo(), false);
        return b;
}

RebanadaProcessor::RebanadaProcessor()
    : AudioProcessor (buses()),
      apvts (*this, nullptr, "REBANADA", createLayout())
{
    formats.registerBasicFormats();
    auto r = [this] (const char* id) { return apvts.getRawParameterValue (id); };
    refs = { r ("chance"), r ("repeat"), r ("lfoRate"), r ("lfoDepth"), r ("attack"), r ("decay"), r ("sustain"), r ("release"),
             r ("cutoff"), r ("reso"), r ("transpose"), r ("gain"), r ("rate"), r ("tempo"), r ("mode"), r ("switchAt"),
             r ("order"), r ("lfoDest"), r ("filterType"), r ("play") };
    if (! apvts.state.hasProperty ("lang")) apvts.state.setProperty ("lang", 0, nullptr);
}

bool RebanadaProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (! layouts.inputBuses.isEmpty() && layouts.getMainInputChannels() != 0) return false;
    if (layouts.outputBuses.isEmpty()) return false;
    const auto main = layouts.outputBuses[0];
    if (main != juce::AudioChannelSet::stereo() && main != juce::AudioChannelSet::mono()) return false;
    for (int i = 1; i < layouts.outputBuses.size(); ++i)
        if (! layouts.outputBuses[i].isDisabled() && layouts.outputBuses[i] != juce::AudioChannelSet::stereo()) return false;
    return true;
}

bool RebanadaProcessor::pairActive (int pair) const
{
    if (pair == 0) return true;
    auto* bus = getBus (false, pair);
    return bus != nullptr && bus->isEnabled() && bus->getNumberOfChannels() == 2;
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
    p.chance = refs.chance->load();     p.repeat = refs.repeat->load();
    p.lfoRate = refs.lfoRate->load();   p.lfoDepth = refs.lfoDepth->load();   p.lfoDest = (int) refs.lfoDest->load();
    p.attack = refs.attack->load();     p.decay = refs.decay->load();         p.sustain = refs.sustain->load();   p.release = refs.release->load();
    p.cutoff = refs.cutoff->load();     p.reso = refs.reso->load();           p.filterType = (int) refs.filterType->load();
    p.transpose = refs.transpose->load(); p.gain = refs.gain->load();
    p.rate = refs.rate->load();         p.tempo = refs.tempo->load();
    p.mode = (int) refs.mode->load();   p.switchAt = (int) refs.switchAt->load();
    p.order = (int) refs.order->load(); p.play = refs.play->load() > 0.5f;
    return p;
}

void RebanadaProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();

    // the main pair first, then every enabled extra pair; disabled pairs fall back to the main
    std::array<float*, maxOutputs> outs {};
    std::array<int, maxPairs> route {};
    int numOut = 0;
    {
        auto* mainBus = getBus (false, 0);
        const int mainCh = mainBus != nullptr ? mainBus->getNumberOfChannels() : 0;
        for (int c = 0; c < mainCh && c < 2 && c < buffer.getNumChannels(); ++c)
            outs[(size_t) numOut++] = buffer.getWritePointer (getChannelIndexInProcessBlockBuffer (false, 0, c));
        if (numOut == 2)
            for (int p = 1; p < maxPairs; ++p)
            {
                if (! pairActive (p)) continue;
                const int a = getChannelIndexInProcessBlockBuffer (false, p, 0), b = getChannelIndexInProcessBlockBuffer (false, p, 1);
                if (a < 0 || b < 0 || a >= buffer.getNumChannels() || b >= buffer.getNumChannels()) continue;
                route[(size_t) p] = numOut / 2;
                outs[(size_t) numOut++] = buffer.getWritePointer (a);
                outs[(size_t) numOut++] = buffer.getWritePointer (b);
            }
    }

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

    buffer.clear();
    engine.setRoute (route);
    engine.process (outs.data(), numOut, n, readParams(), clock, events.data(), numEvents);
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
            decodeSlices (s, t.getProperty ("slices").toString());
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
