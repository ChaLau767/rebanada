// Rebanada — slice sampler engine (no JUCE: it can be tested on its own)
// Copyright (C) 2026 Lautaro Martín Vieyra
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// Eight loop slots. Each slot holds a sample cut into slices. A sequencer plays the
// slices of the current slot in time (synced to the host tempo, or in the loop's own
// time), and MIDI notes play single slices. Every slice has its own pitch, level, pan,
// decay, filter offset, direction and output channel (mono or stereo pair). Every loop
// has its own envelope, filter, order, LFO and timing.

#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>
#include "Modulation.h"

namespace rebanada
{

constexpr int maxSlots   = 8;
constexpr int maxSlices  = 128;
constexpr int maxVoices  = 48;
constexpr int maxOutputs = 64;     // output channels
constexpr int slotBaseNote  = 12;  // C0 … G0 choose the loop (middle C = C4 = 60)
constexpr int sliceBaseNote = 36;  // C2 plays slice 1
constexpr float decayFull = 2000.0f;   // slice decay at its maximum: the slice plays to its end

// Modulation (Rebanada 0.3): every slice belongs to one of four groups. Each group has two LFOs,
// an envelope (ENV, started by every slice) and a random value per slice (RND), routed to the
// sound of its slices. Loop routes send the groups' LFOs to how the loop plays.
constexpr int numGroups = 4;
constexpr int groupRoutes = 6;
constexpr int loopRoutes = 4;
enum ModSource { srcNone = 0, srcLfo1, srcLfo2, srcEnv, srcRnd, numModSources };
// What a group route can move: the slice's own knobs and the loop's envelope and filter
// (append only: sessions store the index). Times are read when a slice starts; the rest moves all the time.
enum VoiceDest { vdPitch = 0, vdFine, vdLevel, vdPan, vdSliceDecay, vdFilter,
                 vdAttack, vdDecay, vdSustain, vdRelease, vdCutoff, vdReso, numVoiceDests };
// What a loop route can move (read at every step for repeat, chance and rate).
enum LoopDest { ldRepeat = 0, ldChance, ldRate, ldTranspose, ldLevel, ldLfoRate, ldLfoDepth, numLoopDests };
constexpr int numLoopSources = numGroups * 2;   // the LFOs of every group

struct ModRoute
{
    int source = 0;       // group route: ModSource; loop route: 0 none, 1 + group * 2 + lfo
    int dest = 0;
    float amount = 0.0f;  // -1 … +1 of the knob's range
};

struct GroupMod
{
    int shape[2] { 0, 0 };
    bool sync[2] { false, false };
    float rate[2] { 0.5f, 0.2f };     // Hz
    int division[2] { 4, 4 };         // modlib division index
    float phase[2] { 0.0f, 0.0f };    // 0…1
    float envAttack = 10.0f, envHold = 0.0f, envDecay = 300.0f;   // ms
    ModRoute routes[groupRoutes];
};

// The knob ranges, the same curves as the host parameters and the slice knobs.
inline const modlib::Range& voiceRange (int d) noexcept
{
    using R = modlib::Range;
    static const R r[numVoiceDests] {
        { R::linear, -24.0f, 24.0f, 1.0f },          // pitch, st
        { R::linear, -50.0f, 50.0f, 1.0f },          // fine, ct
        { R::linear, -36.0f, 6.0f, 1.0f },           // slice level, dB
        { R::linear, -1.0f, 1.0f, 1.0f },            // pan
        { R::logarithmic, 10.0f, decayFull, 1.0f },  // slice decay, ms
        { R::linear, -4.0f, 4.0f, 1.0f },            // filter, oct
        { R::skewed, 0.0f, 1000.0f, 0.30103f },      // attack (skew for a centre at 10 %)
        { R::logarithmic, 5.0f, 5000.0f, 1.0f },     // decay
        { R::linear, 0.0f, 100.0f, 1.0f },           // sustain
        { R::logarithmic, 5.0f, 5000.0f, 1.0f },     // release
        { R::logarithmic, 20.0f, 20000.0f, 1.0f },   // cutoff
        { R::linear, 0.0f, 100.0f, 1.0f },           // reso
    };
    return r[std::clamp (d, 0, numVoiceDests - 1)];
}
inline bool voiceDestAtStart (int d) noexcept { return d == vdSliceDecay || (d >= vdAttack && d <= vdRelease); }

inline const modlib::Range& loopRange (int d) noexcept
{
    using R = modlib::Range;
    static const R r[numLoopDests] {
        { R::linear, 0.0f, 100.0f, 1.0f },           // repeat
        { R::linear, 0.0f, 100.0f, 1.0f },           // chance
        { R::logarithmic, 0.25f, 4.0f, 1.0f },       // rate
        { R::linear, -24.0f, 24.0f, 1.0f },          // transpose
        { R::linear, -36.0f, 12.0f, 1.0f },          // level
        { R::logarithmic, 0.02f, 20.0f, 1.0f },      // motion LFO rate
        { R::linear, 0.0f, 100.0f, 1.0f },           // motion LFO depth
    };
    return r[std::clamp (d, 0, numLoopDests - 1)];
}

struct Sample
{
    std::vector<float> left, right;
    double sampleRate = 44100.0;
    int length() const noexcept { return (int) left.size(); }
};

struct SliceParams
{
    float pitch  = 0.0f;       // semitones, -24 … +24
    float fine   = 0.0f;       // cents, -50 … +50
    float level  = 0.0f;       // dB, -36 … +6
    float pan    = 0.0f;       // -1 … +1
    float decay  = decayFull;  // ms; decayFull = no fade
    float filter = 0.0f;       // octaves added to the cutoff, -4 … +4
    bool reverse = false;
    bool mute    = false;
    int  out     = 0;          // first output channel (0 = channel 1)
    bool stereo  = true;       // a pair (out, out + 1) or a single mono channel
    int  group   = 0;          // modulation group, 0 … 3 (A … D)
};

struct Slot
{
    std::shared_ptr<const Sample> sample;     // null while the slot is empty
    int numSlices = 0;
    std::array<int, maxSlices> starts {};     // sorted; starts[0] is always 0
    std::array<SliceParams, maxSlices> slices {};
    int bars = 2;                             // loop length in bars, for sync mode

    bool empty() const noexcept { return sample == nullptr || sample->length() < 2 || numSlices <= 0; }
    int length() const noexcept { return sample ? sample->length() : 0; }
    int sliceStart (int i) const noexcept { return starts[(size_t) i]; }
    int sliceEnd (int i) const noexcept { return i + 1 < numSlices ? starts[(size_t) i + 1] : length(); }
};

// Settings that each loop has for itself.
struct LoopParams
{
    int   mode = 0;          // 0 = sync to the host tempo, 1 = free (the loop's own time)
    int   order = 0;         // 0 forward, 1 random, 2 weighted
    float chance = 50.0f;    // weighted: % of steps that play another slice
    float repeat = 0.0f;     // % of steps that repeat the previous slice
    float lfoRate = 0.5f;    // Hz
    float lfoDepth = 0.0f;   // %
    int   lfoDest = 0;       // 0 cutoff, 1 pitch, 2 pan, 3 level
    float attack = 0.0f, decay = 400.0f, sustain = 100.0f, release = 40.0f;   // ms, ms, %, ms
    int   filterType = 0;    // 0 low-pass, 1 band-pass, 2 high-pass
    float cutoff = 20000.0f; // Hz
    float reso = 0.0f;       // %
    float transpose = 0.0f;  // semitones
    float rate = 1.0f;       // free mode: speed of the loop's clock
    float level = 0.0f;      // dB
};

struct Params
{
    int   switchAt = 2;      // loop changes: 0 instant, 1 next beat, 2 next bar, 3 end of the loop
    bool  play = true;       // loop playback on/off (MIDI notes always play)
    float gain = 0.0f;       // dB, all loops
    float tempo = 120.0f;    // internal tempo when there is no host transport
    std::array<LoopParams, maxSlots> loops {};
    std::array<GroupMod, numGroups> groups {};
    ModRoute loopRoute[loopRoutes];
};

// What the host says about time. When there is no host transport (standalone app),
// hostAvailable is false and the engine runs its own clock at Params::tempo.
struct Clock
{
    bool hostAvailable = false;
    bool hostPlaying = false;
    double ppq = 0.0;        // position in quarter notes
    double bpm = 120.0;
    int numerator = 4, denominator = 4;
};

struct Event
{
    enum Type { noteOn, noteOff, allNotesOff } type = noteOn;
    int offset = 0;
    int note = 0;
    float velocity = 1.0f;
};

struct Telemetry
{
    int currentSlot = 0, pendingSlot = -1;
    bool running = false;
    float loopFrac = -1.0f;          // position of the sequencer inside the loop, 0…1
    float bpm = 120.0f;
    bool hostTempo = false;
    int numVoicePos = 0;
    float voicePos[16] {};           // playing positions in the current slot, 0…1 of the sample
    float sliceLevel[maxSlices] {};  // how loud each slice of the current slot sounds now
    float outPeak = 0.0f;
    float groupLfo[numGroups][2] {};             // current LFO values, -1…+1
    float groupOffset[numGroups][numVoiceDests] {};  // how far the newest slice of each group is moved now
    float loopOffset[numLoopDests] {};           // how far the loop routes move each loop setting now
};

//==============================================================================
namespace detail
{
    inline float dbToGain (float db) noexcept { return std::pow (10.0f, db * 0.05f); }

    // simple and fast, good enough for musical choices
    struct Random
    {
        uint32_t state = 0x9e3779b9u;
        float next() noexcept
        {
            state ^= state << 13; state ^= state >> 17; state ^= state << 5;
            return (float) (state >> 8) * (1.0f / 16777216.0f);
        }
        int below (int n) noexcept { return n <= 1 ? 0 : std::min (n - 1, (int) (next() * (float) n)); }
    };

    inline float hermite (const float* d, int len, int lo, int hi, int i, float t) noexcept
    {
        auto at = [&] (int k) { return d[std::clamp (k, std::max (0, lo), std::min (len, hi) - 1)]; };
        const float y0 = at (i - 1), y1 = at (i), y2 = at (i + 1), y3 = at (i + 2);
        const float c1 = 0.5f * (y2 - y0);
        const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * t + c2) * t + c1) * t + y1;
    }
}

//==============================================================================
class Engine
{
public:
    std::array<Slot, maxSlots> slots;

    void prepare (double sampleRate)
    {
        sr = sampleRate > 0.0 ? sampleRate : 44100.0;
        reset();
    }

    void reset()
    {
        for (auto& v : voices) v.active = false;
        running = false;
        lastPlayed = -1;
        loopVoice = -1;
        internalPpq = 0.0;
        syncPpq = 0.0;
        smoothedGain = -1.0f;
        for (int g = 0; g < numGroups; ++g)
            for (int l = 0; l < 2; ++l) lfos[g][l].reset (0x51ed27u + (uint32_t) (g * 2 + l) * 0x9e3779b9u);
    }

    // Loop requested from the interface or from a note between C0 and G0.
    void requestSlot (int s) noexcept
    {
        if (s < 0 || s >= maxSlots) return;
        pendingSlot = s == currentSlot ? -1 : s;
    }

    int getCurrentSlot() const noexcept { return currentSlot; }
    int getPendingSlot() const noexcept { return pendingSlot; }

    // Call (with the engine locked) before freeing a sample a voice might still read.
    void stopVoicesOf (int slot) noexcept
    {
        for (int i = 0; i < maxVoices; ++i)
            if (voices[(size_t) i].active && voices[(size_t) i].slot == slot) voices[(size_t) i].active = false;
        if (slot == currentSlot) { loopVoice = -1; lastPlayed = -1; }
    }

    // Long blocks are cut into pieces of maxChunk samples, so the modulation stays smooth.
    void process (float* const* out, int numOut, int n, const Params& p, const Clock& clock,
                  const Event* events, int numEvents)
    {
        numOut = std::min (numOut, maxOutputs);
        if (n <= maxChunk) { processChunk (out, numOut, n, p, clock, events, numEvents); return; }

        const double bpm = clock.hostAvailable && clock.bpm > 1.0 ? clock.bpm : p.tempo;
        int done = 0, ei = 0;
        while (done < n)
        {
            const int m = std::min (maxChunk, n - done);
            const bool last = done + m >= n;
            int count = 0;
            while (ei + count < numEvents && (last || events[ei + count].offset < done + m)) ++count;
            const int used = std::min (count, (int) chunkEvents.size());
            for (int k = 0; k < used; ++k)
            {
                chunkEvents[(size_t) k] = events[ei + k];
                chunkEvents[(size_t) k].offset = std::clamp (events[ei + k].offset - done, 0, m - 1);
            }
            Clock c = clock;
            if (c.hostAvailable && c.hostPlaying) c.ppq += bpm / 60.0 / sr * done;
            float* ptrs[maxOutputs];
            for (int ch = 0; ch < numOut; ++ch) ptrs[ch] = out[ch] + done;
            processChunk (ptrs, numOut, m, p, c, chunkEvents.data(), used);
            ei += count;
            done += m;
        }
    }

private:
    static constexpr int maxChunk = 1024;
    static constexpr int maxTicks = maxChunk / 32 + 1;

    void processChunk (float* const* out, int numOut, int n, const Params& p, const Clock& clock,
                       const Event* events, int numEvents)
    {
        for (int ch = 0; ch < numOut; ++ch) std::fill (out[ch], out[ch] + n, 0.0f);
        if (n <= 0 || numOut <= 0) return;

        params = p;
        advanceLfos (n, clock);
        applyLoopRoutes();
        scheduleSequencer (n, clock);

        // merge the sequencer's actions and the incoming notes, in time order
        int si = 0, ei = 0, done = 0;
        while (done < n)
        {
            int next = n;
            if (si < numActions) next = std::min (next, actions[(size_t) si].offset);
            if (ei < numEvents)  next = std::min (next, std::clamp (events[ei].offset, 0, n - 1));
            next = std::max (next, done);

            render (out, numOut, done, next);
            done = next;
            if (done >= n) break;
            curOffset = done;

            while (si < numActions && actions[(size_t) si].offset <= done) apply (actions[(size_t) si++]);
            while (ei < numEvents && std::clamp (events[ei].offset, 0, n - 1) <= done) handle (events[ei++]);
        }
        curOffset = n - 1;
        while (si < numActions) apply (actions[(size_t) si++]);
        while (ei < numEvents) handle (events[ei++]);

        // output level, smoothed across the block
        const float target = detail::dbToGain (p.gain);
        if (smoothedGain < 0.0f) smoothedGain = target;
        const float step = (target - smoothedGain) / (float) n;
        float peak = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            const float g = smoothedGain + step * (float) (i + 1);
            for (int ch = 0; ch < numOut; ++ch)
            {
                float& s = out[ch][i];
                s *= g;
                if (! std::isfinite (s)) s = 0.0f;
                peak = std::max (peak, std::abs (s));
            }
        }
        smoothedGain = target;
        for (int l = 0; l < maxSlots; ++l)
            lfoPhase[(size_t) l] = std::fmod (lfoPhase[(size_t) l] + (double) params.loops[(size_t) l].lfoRate * n / sr, 1.0);
        peakSinceSnapshot = std::max (peakSinceSnapshot, peak);
    }

public:
    void snapshot (Telemetry& t) noexcept
    {
        t.currentSlot = currentSlot;
        t.pendingSlot = pendingSlot;
        t.running = running;
        t.loopFrac = running ? (float) loopFrac : -1.0f;
        t.bpm = (float) shownBpm;
        t.hostTempo = shownHostTempo;
        t.outPeak = std::max (t.outPeak, peakSinceSnapshot);
        peakSinceSnapshot = 0.0f;
        std::fill (std::begin (t.sliceLevel), std::end (t.sliceLevel), 0.0f);
        for (int g = 0; g < numGroups; ++g)
        {
            for (int l = 0; l < 2; ++l) t.groupLfo[g][l] = lfoVal[g][l][0];
            const Voice* newest = nullptr;
            for (const auto& v : voices)
                if (v.active && v.group == g && v.stage != 3 && (newest == nullptr || v.age - newest->age < 0x80000000u)) newest = &v;
            for (int d = 0; d < numVoiceDests; ++d) t.groupOffset[g][d] = newest != nullptr ? newest->offs[d] : 0.0f;
        }
        for (int d = 0; d < numLoopDests; ++d) t.loopOffset[d] = loopOff[d];
        t.numVoicePos = 0;
        const Slot& slot = slots[(size_t) currentSlot];
        for (const auto& v : voices)
        {
            if (! v.active || v.slot != currentSlot || slot.length() <= 0) continue;
            const float level = v.env * v.decayLevel;
            if (v.slice >= 0 && v.slice < maxSlices) t.sliceLevel[v.slice] = std::max (t.sliceLevel[v.slice], level);
            if (t.numVoicePos < 16) t.voicePos[t.numVoicePos++] = (float) (v.pos / slot.length());
        }
    }

    // How long the loop of a slot lasts in quarter notes, given a time signature.
    static double loopQuarters (const Slot& s, int num, int den) noexcept
    {
        const double barQ = (double) std::max (1, num) * 4.0 / (double) std::max (1, den);
        return std::max (1, s.bars) * barQ;
    }

private:
    struct Action
    {
        enum Type { trigger, switchSlot, stopLoop } type = trigger;
        int offset = 0;
        int slot = 0, step = 0;
        double startFrames = 0.0;
    };

    struct Voice
    {
        bool active = false;
        int slot = 0, slice = 0, note = -1;
        bool loop = false;
        const Sample* smp = nullptr;
        double pos = 0.0, baseInc = 1.0;
        int begin = 0, end = 0;
        bool reverse = false;
        int stage = 0;            // 0 attack, 1 decay, 2 sustain, 3 release
        float env = 0.0f, releaseStep = 0.0f;
        float decayMul = 1.0f, decayLevel = 1.0f;
        float velGain = 1.0f;
        float slicePitch = 0.0f, fine = 0.0f, levelDb = 0.0f, pan = 0.0f, filterOct = 0.0f;   // the slice's knobs
        float attackS = 1.0f, decayS = 1.0f, sustain = 1.0f, releaseMs = 40.0f;   // read when it starts
        int out = 0;
        bool stereo = true;
        uint32_t age = 0;
        int group = 0;
        float rnd = 0.0f;              // RND: one value per slice
        uint32_t elapsed = 0;          // samples since it started (ENV)
        float offs[numVoiceDests] {};  // how far each destination is moved now
        // per-voice filter (state-variable, trapezoidal)
        float s1L = 0, s2L = 0, s1R = 0, s2R = 0;
        float a1 = 1, a2 = 0, a3 = 0, k = 1.4f, open = 1.0f;
        float inc = 1.0f, gL = 1.0f, gR = 1.0f;
    };

    //--------------------------------------------------------------------------
    // The groups' LFOs, one value every 32 samples of this block.
    void advanceLfos (int n, const Clock& clock)
    {
        const double bpm = clock.hostAvailable && clock.bpm > 1.0 ? clock.bpm : params.tempo;
        const double qps = bpm / 60.0 / sr;
        if (clock.hostAvailable && clock.hostPlaying) syncPpq = clock.ppq;
        numTicks = std::clamp ((n + 31) / 32, 1, maxTicks);
        for (int g = 0; g < numGroups; ++g)
        {
            const GroupMod& gm = params.groups[(size_t) g];
            for (int l = 0; l < 2; ++l)
            {
                modlib::Lfo& lfo = lfos[g][l];
                lfo.shape = std::clamp (gm.shape[l], 0, modlib::numShapes - 1);
                lfo.phaseOffset = std::clamp (gm.phase[l], 0.0f, 1.0f);
                for (int t = 0; t < numTicks; ++t)
                {
                    const int len = std::min (32, n - t * 32);
                    if (gm.sync[l])
                        lfoVal[g][l][t] = lfo.syncTo (syncPpq + qps * t * 32, modlib::divisionQuarters (gm.division[l]));
                    else
                    {
                        lfoVal[g][l][t] = t == 0 ? lfo.advanceFree (0.0) : lfo.getValue();
                        lfo.advanceFree ((double) std::max (0.0f, gm.rate[l]) * std::max (0, len) / sr);
                    }
                }
            }
        }
        syncPpq += qps * n;
    }

    int tickAt (int i) const noexcept { return std::clamp (i >> 5, 0, numTicks - 1); }

    static float& loopField (LoopParams& lp, int d) noexcept
    {
        switch (d)
        {
            case ldRepeat:    return lp.repeat;
            case ldChance:    return lp.chance;
            case ldRate:      return lp.rate;
            case ldTranspose: return lp.transpose;
            case ldLevel:     return lp.level;
            case ldLfoRate:   return lp.lfoRate;
            default:          return lp.lfoDepth;
        }
    }

    // Loop routes move the settings of every loop, from the value of their LFO at the start of the block.
    void applyLoopRoutes() noexcept
    {
        std::fill (std::begin (loopOff), std::end (loopOff), 0.0f);
        for (const auto& r : params.loopRoute)
        {
            if (r.source <= 0 || r.source > numLoopSources || r.amount == 0.0f || r.dest < 0 || r.dest >= numLoopDests) continue;
            const int g = (r.source - 1) / 2, l = (r.source - 1) % 2;
            loopOff[r.dest] += lfoVal[g][l][0] * std::clamp (r.amount, -1.0f, 1.0f);
        }
        for (int d = 0; d < numLoopDests; ++d)
        {
            if (loopOff[d] == 0.0f) continue;
            for (auto& lp : params.loops)
            {
                float& f = loopField (lp, d);
                f = loopRange (d).apply (f, loopOff[d]);
            }
        }
    }

    float envValue (const Voice& v, const GroupMod& gm) const noexcept
    {
        float t = (float) v.elapsed * 1000.0f / (float) sr;
        const float a = std::max (0.1f, gm.envAttack);
        if (t < a) return t / a;
        t -= a;
        if (t < gm.envHold) return 1.0f;
        t -= std::max (0.0f, gm.envHold);
        return std::max (0.0f, 1.0f - t / std::max (1.0f, gm.envDecay));
    }

    // How far the group's routes move each destination of this voice, now.
    void voiceOffsets (const Voice& v, int tick, float* offs) const noexcept
    {
        std::fill (offs, offs + numVoiceDests, 0.0f);
        const int g = std::clamp (v.group, 0, numGroups - 1);
        const GroupMod& gm = params.groups[(size_t) g];
        for (const auto& r : gm.routes)
        {
            if (r.source <= srcNone || r.source >= numModSources || r.amount == 0.0f || r.dest < 0 || r.dest >= numVoiceDests) continue;
            float x = 0.0f;
            switch (r.source)
            {
                case srcLfo1: x = lfoVal[g][0][tick]; break;
                case srcLfo2: x = lfoVal[g][1][tick]; break;
                case srcEnv:  x = envValue (v, gm); break;
                default:      x = v.rnd; break;
            }
            offs[r.dest] += x * std::clamp (r.amount, -1.0f, 1.0f);
        }
    }

    static float moved (int d, float base, const float* offs) noexcept
    {
        return offs[d] == 0.0f ? base : voiceRange (d).apply (base, offs[d]);
    }

    //--------------------------------------------------------------------------
    // Works out, for this block, where the sequencer starts slices and changes loops.
    void scheduleSequencer (int n, const Clock& clock)
    {
        numActions = 0;
        const Params& p = params;

        // which clock drives the loop
        bool clockRuns = false;
        double ppq0 = 0.0, bpm = p.tempo;
        int num = 4, den = 4;
        if (clock.hostAvailable)
        {
            clockRuns = clock.hostPlaying;
            ppq0 = clock.ppq;
            bpm = clock.bpm > 1.0 ? clock.bpm : p.tempo;
            num = clock.numerator; den = clock.denominator;
        }
        else
        {
            clockRuns = true;
            ppq0 = internalPpq;
        }
        shownBpm = bpm;
        shownHostTempo = clock.hostAvailable && clock.bpm > 1.0;

        const bool wantRun = p.play && clockRuns;
        const bool startedNow = wantRun && ! running;

        if (! wantRun)
        {
            if (running) push ({ Action::stopLoop, 0, currentSlot, 0, 0.0 });
            running = false;
            if (pendingSlot >= 0) { push ({ Action::switchSlot, 0, pendingSlot, 0, 0.0 }); }
            if (! clock.hostAvailable && ! p.play) internalPpq = 0.0;
            loopFrac = -1.0;
            return;
        }

        if (! clock.hostAvailable && startedNow) { internalPpq = 0.0; ppq0 = 0.0; }
        if (startedNow && pendingSlot >= 0) { currentSlot = pendingSlot; pendingSlot = -1; }

        const double qps = bpm / 60.0 / sr;                // quarter notes per sample
        if (p.loops[(size_t) currentSlot].mode == 0)
            scheduleSync (n, ppq0, qps, num, den, startedNow);
        else
            scheduleFree (n, startedNow);

        running = true;
        if (! clock.hostAvailable) internalPpq = ppq0 + qps * n;
        expectedPpq = ppq0 + qps * n;
    }

    void push (const Action& a) noexcept
    {
        if (numActions < (int) actions.size()) actions[(size_t) numActions++] = a;
    }

    static int stepAt (const Slot& s, double frac) noexcept
    {
        const double pos = frac * s.length();
        int k = 0;
        for (int i = 1; i < s.numSlices; ++i) if ((double) s.starts[(size_t) i] <= pos) k = i; else break;
        return k;
    }

    void scheduleSync (int n, double ppq0, double qps, int num, int den, bool startedNow)
    {
        const double qEnd = ppq0 + qps * n;
        const bool jumped = startedNow || std::abs (ppq0 - expectedPpq) > std::max (qps * 8.0, 1.0e-6) || currentMode != 0;
        currentMode = 0;
        int slot = currentSlot;
        double q = ppq0;
        const double beatQ = 4.0 / (double) std::max (1, den);
        const double barQ = (double) std::max (1, num) * beatQ;

        auto containing = [&] (int offset, double atQ) {
            const Slot& s = slots[(size_t) slot];
            if (s.empty()) { push ({ Action::stopLoop, offset, slot, 0, 0.0 }); return; }
            const double loopQ = loopQuarters (s, num, den);
            double posQ = std::fmod (atQ, loopQ); if (posQ < 0) posQ += loopQ;
            const int k = stepAt (s, posQ / loopQ);
            const double stepQ = (double) s.starts[(size_t) k] / s.length() * loopQ;
            const double seconds = (posQ - stepQ) / (qps * sr);
            push ({ Action::trigger, offset, slot, k, std::max (0.0, seconds * s.sample->sampleRate) });
        };

        if (pendingSlot >= 0 && p_switchAt() == 0)
        {
            slot = pendingSlot;
            push ({ Action::switchSlot, 0, slot, 0, 0.0 });
            containing (0, q);
        }
        else if (jumped)
            containing (0, q);

        for (int guard = 0; guard < 400; ++guard)
        {
            const Slot& s = slots[(size_t) slot];
            const double loopQ = loopQuarters (s, num, den);
            double posQ = std::fmod (q, loopQ); if (posQ < 0) posQ += loopQ;
            const double loopStartAbs = q - posQ;

            double nextStepAbs = 1.0e300;
            int nextStep = 0;
            if (! s.empty())
            {
                const double frac = posQ / loopQ;
                nextStep = s.numSlices;   // = wrap to slice 0
                for (int i = 1; i < s.numSlices; ++i)
                    if ((double) s.starts[(size_t) i] / s.length() > frac + 1.0e-12) { nextStep = i; break; }
                const double stepQ = nextStep < s.numSlices ? (double) s.starts[(size_t) nextStep] / s.length() * loopQ : loopQ;
                nextStepAbs = loopStartAbs + stepQ;
                if (nextStep >= s.numSlices) nextStep = 0;
            }

            double switchAbs = 1.0e300;
            if (pendingSlot >= 0 && pendingSlot != slot)
            {
                const int mode = p_switchAt();
                const double unit = mode == 1 ? beatQ : (mode == 2 ? barQ : loopQ);
                switchAbs = std::ceil ((q - 1.0e-9) / unit) * unit;
                if (switchAbs < q) switchAbs = q;
                if (mode == 0) switchAbs = q;
            }

            const double next = std::min (nextStepAbs, switchAbs);
            if (next >= qEnd) break;
            const int offset = std::clamp ((int) std::llround ((next - ppq0) / qps), 0, n - 1);

            if (switchAbs <= nextStepAbs + 1.0e-9)
            {
                slot = pendingSlot;
                push ({ Action::switchSlot, offset, slot, 0, 0.0 });
                containing (offset, next);
            }
            else
                push ({ Action::trigger, offset, slot, nextStep, 0.0 });

            q = next + 1.0e-9;
        }

        const Slot& s = slots[(size_t) slot];
        const double loopQ = loopQuarters (s, num, den);
        double posQ = std::fmod (qEnd, loopQ); if (posQ < 0) posQ += loopQ;
        loopFrac = posQ / loopQ;
    }

    void scheduleFree (int n, bool startedNow)
    {
        const bool restart = startedNow || currentMode != 1;
        currentMode = 1;
        int slot = currentSlot;

        auto containing = [&] (int offset) {
            const Slot& s = slots[(size_t) slot];
            if (s.empty()) { push ({ Action::stopLoop, offset, slot, 0, 0.0 }); return; }
            const int k = stepAt (s, framePos / s.length());
            push ({ Action::trigger, offset, slot, k, std::max (0.0, framePos - s.starts[(size_t) k]) });
        };

        if (restart) framePos = 0.0;

        if (pendingSlot >= 0 && p_switchAt() == 0)
        {
            const Slot& old = slots[(size_t) slot];
            const double frac = old.length() > 0 ? framePos / old.length() : 0.0;
            slot = pendingSlot;
            framePos = frac * slots[(size_t) slot].length();
            push ({ Action::switchSlot, 0, slot, 0, 0.0 });
            containing (0);
        }
        else if (restart)
            containing (0);

        int t = 0;   // host samples consumed so far
        for (int guard = 0; guard < 400 && t < n; ++guard)
        {
            const Slot& s = slots[(size_t) slot];
            if (s.empty()) break;
            const double len = s.length();
            const double speed = std::max (0.01, (double) params.loops[(size_t) slot].rate) * s.sample->sampleRate / sr;   // frames per host sample

            int nextStep = 0;
            double nextFrame = len;
            for (int i = 1; i < s.numSlices; ++i)
                if (s.starts[(size_t) i] > framePos + 1.0e-9) { nextStep = i; nextFrame = s.starts[(size_t) i]; break; }

            double switchFrame = 1.0e300;
            if (pendingSlot >= 0 && pendingSlot != slot)
            {
                const int mode = p_switchAt();
                const double unit = mode == 1 ? len / (s.bars * 4.0) : (mode == 2 ? len / s.bars : len);
                switchFrame = std::ceil ((framePos - 1.0e-9) / unit) * unit;
                if (mode == 0) switchFrame = framePos;
            }

            const double target = std::min (nextFrame, switchFrame);
            const double samplesAway = (target - framePos) / speed;
            if (t + samplesAway >= n)
            {
                framePos += (n - t) * speed;
                t = n;
                break;
            }
            t = std::clamp ((int) std::llround (t + samplesAway), t, n - 1);
            framePos = target;

            if (switchFrame <= nextFrame + 1.0e-9)
            {
                const double frac = framePos / len;
                slot = pendingSlot;
                framePos = std::fmod (frac, 1.0) * slots[(size_t) slot].length();
                push ({ Action::switchSlot, t, slot, 0, 0.0 });
                containing (t);
            }
            else
            {
                if (nextFrame >= len) framePos = 0.0;
                push ({ Action::trigger, t, slot, nextStep, 0.0 });
            }
        }

        const Slot& s = slots[(size_t) slot];
        if (s.length() > 0)
        {
            framePos = std::fmod (framePos, (double) s.length());
            loopFrac = framePos / s.length();
        }
    }

    int p_switchAt() const noexcept { return params.switchAt; }

    //--------------------------------------------------------------------------
    void apply (const Action& a)
    {
        switch (a.type)
        {
            case Action::switchSlot:
                if (a.slot != currentSlot)
                {
                    releaseLoopVoice();
                    lastPlayed = -1;
                }
                currentSlot = a.slot;
                if (pendingSlot == a.slot) pendingSlot = -1;
                break;

            case Action::stopLoop:
                releaseLoopVoice();
                break;

            case Action::trigger:
            {
                const Slot& s = slots[(size_t) a.slot];
                releaseLoopVoice();
                if (s.empty()) break;
                int slice = std::clamp (a.step, 0, s.numSlices - 1);
                double startFrames = a.startFrames;
                const int original = slice;

                const LoopParams& lp = params.loops[(size_t) a.slot];
                if (lp.order == 1) slice = rng.below (s.numSlices);
                else if (lp.order == 2 && rng.next() * 100.0f < lp.chance) slice = rng.below (s.numSlices);
                if (lastPlayed >= 0 && lastPlayed < s.numSlices && lp.repeat > 0.0f && rng.next() * 100.0f < lp.repeat)
                    slice = lastPlayed;
                if (slice != original) startFrames = 0.0;

                lastPlayed = slice;
                if (s.slices[(size_t) slice].mute) break;
                loopVoice = startVoice (a.slot, slice, -1, 1.0f, startFrames, true);
                break;
            }
        }
    }

    void handle (const Event& e)
    {
        if (e.type == Event::allNotesOff)
        {
            for (auto& v : voices) if (v.active && ! v.loop) release (v);
            return;
        }
        if (e.type == Event::noteOn && e.velocity > 0.0f)
        {
            if (e.note >= slotBaseNote && e.note < slotBaseNote + maxSlots)
            {
                requestSlot (e.note - slotBaseNote);
                if (! running && pendingSlot >= 0) { currentSlot = pendingSlot; pendingSlot = -1; }
                return;
            }
            const Slot& s = slots[(size_t) currentSlot];
            const int slice = e.note - sliceBaseNote;
            if (s.empty() || slice < 0 || slice >= s.numSlices || s.slices[(size_t) slice].mute) return;
            for (auto& v : voices) if (v.active && ! v.loop && v.note == e.note) release (v);
            startVoice (currentSlot, slice, e.note, std::clamp (e.velocity, 0.0f, 1.0f), 0.0, false);
            return;
        }
        // note off (or note on with velocity 0)
        for (auto& v : voices) if (v.active && ! v.loop && v.note == e.note) release (v);
    }

    void releaseLoopVoice()
    {
        if (loopVoice >= 0)
        {
            auto& v = voices[(size_t) loopVoice];
            if (v.active && v.loop) release (v);
        }
        loopVoice = -1;
    }

    void release (Voice& v) noexcept
    {
        if (v.stage == 3) return;
        v.stage = 3;
        const float samples = std::max (1.0f, v.releaseMs * 0.001f * (float) sr);
        v.releaseStep = std::max (v.env, 1.0e-4f) / samples;
    }

    int startVoice (int slotIndex, int slice, int note, float velocity, double startFrames, bool loop)
    {
        const Slot& s = slots[(size_t) slotIndex];
        const SliceParams& sp = s.slices[(size_t) slice];

        int index = -1;
        uint32_t oldest = 0;
        for (int i = 0; i < maxVoices; ++i)
            if (! voices[(size_t) i].active) { index = i; break; }
        if (index < 0)
            for (int i = 0; i < maxVoices; ++i)
            {
                const auto& v = voices[(size_t) i];
                const uint32_t a = voiceCounter - v.age + (v.stage == 3 ? 0x40000000u : 0u);   // released voices go first
                if (index < 0 || a > oldest) { index = i; oldest = a; }
            }

        Voice& v = voices[(size_t) index];
        v = Voice {};
        v.active = true;
        v.slot = slotIndex;
        v.slice = slice;
        v.note = note;
        v.loop = loop;
        v.smp = s.sample.get();
        v.begin = s.sliceStart (slice);
        v.end = std::max (v.begin + 1, s.sliceEnd (slice));
        v.reverse = sp.reverse;
        const double offset = std::min (startFrames, (double) (v.end - v.begin - 1));
        v.pos = v.reverse ? (double) v.end - 1.0 - offset : (double) v.begin + offset;
        v.baseInc = s.sample->sampleRate / sr;
        v.slicePitch = sp.pitch;
        v.fine = sp.fine;
        v.levelDb = sp.level;
        v.velGain = detail::dbToGain (-24.0f * (1.0f - velocity));
        v.pan = sp.pan;
        v.filterOct = sp.filter;
        v.out = std::clamp (sp.out, 0, maxOutputs - 1);
        v.stereo = sp.stereo;
        v.group = std::clamp (sp.group, 0, numGroups - 1);
        v.rnd = modRng.bipolar();
        v.elapsed = 0;
        v.stage = 0;
        v.env = 0.0f;
        v.decayLevel = 1.0f;
        v.age = ++voiceCounter;

        // the times are read once, when the slice starts
        const int tick = tickAt (curOffset);
        voiceOffsets (v, tick, v.offs);
        const LoopParams& lp = params.loops[(size_t) slotIndex];
        const float attack  = moved (vdAttack, lp.attack, v.offs);
        const float decay   = moved (vdDecay, lp.decay, v.offs);
        v.sustain = std::clamp (moved (vdSustain, lp.sustain, v.offs) * 0.01f, 0.0f, 1.0f);
        v.releaseMs = moved (vdRelease, lp.release, v.offs);
        v.attackS = std::max (1.0f, std::max (1.0f, attack) * 0.001f * (float) sr);
        v.decayS  = std::max (1.0f, decay * 0.001f * (float) sr);
        const float sliceDecay = sp.decay >= decayFull - 0.5f && v.offs[vdSliceDecay] >= 0.0f ? decayFull
                               : moved (vdSliceDecay, sp.decay, v.offs);
        v.decayMul = sliceDecay >= decayFull - 0.5f ? 1.0f
                   : std::pow (0.001f, 1.0f / std::max (1.0f, sliceDecay * 0.001f * (float) sr));
        updateControl (v, 0.0f, tick);
        return index;
    }

    // Pitch, level, pan and filter, refreshed every 32 samples.
    void updateControl (Voice& v, float lfo, int tick) noexcept
    {
        const LoopParams& p = params.loops[(size_t) v.slot];
        float offs[numVoiceDests];
        voiceOffsets (v, tick, offs);
        for (int d = 0; d < numVoiceDests; ++d) if (! voiceDestAtStart (d)) v.offs[d] = offs[d];

        const float depth = std::clamp (p.lfoDepth * 0.01f, 0.0f, 1.0f);
        float pitch = moved (vdPitch, v.slicePitch, offs) + moved (vdFine, v.fine, offs) * 0.01f + p.transpose;
        float oct = moved (vdFilter, v.filterOct, offs);
        float pan = moved (vdPan, v.pan, offs);
        float amp = detail::dbToGain (moved (vdLevel, v.levelDb, offs)) * v.velGain;
        switch (p.lfoDest)
        {
            case 0: oct += lfo * depth * 3.0f; break;
            case 1: pitch += lfo * depth * depth * 12.0f; break;
            case 2: pan += lfo * depth; break;
            default: amp = 1.0f - depth * (0.5f + 0.5f * lfo); break;
        }
        v.inc = (float) (v.baseInc * std::pow (2.0, pitch / 12.0));

        pan = std::clamp (pan, -1.0f, 1.0f);
        amp *= detail::dbToGain (p.level);
        v.gL = (pan <= 0.0f ? 1.0f : 1.0f - pan) * amp;
        v.gR = (pan >= 0.0f ? 1.0f : 1.0f + pan) * amp;

        const float cutoff = moved (vdCutoff, p.cutoff, offs);
        const float fc = std::clamp (cutoff * std::pow (2.0f, oct), 20.0f, (float) (sr * 0.45));
        const float g = std::tan (3.14159265f * fc / (float) sr);
        const float res = std::clamp (moved (vdReso, p.reso, offs) * 0.01f, 0.0f, 1.0f);
        v.k = 1.414f * (1.0f - res) + 0.06f * res;
        v.a1 = 1.0f / (1.0f + g * (g + v.k));
        v.a2 = g * v.a1;
        v.a3 = g * v.a2;
        // a wide-open low-pass is fully transparent
        v.open = p.filterType == 0 ? std::clamp ((fc - 15000.0f) / 4000.0f, 0.0f, 1.0f) : 0.0f;
        if (fc >= (float) (sr * 0.45) - 1.0f && p.filterType == 0) v.open = 1.0f;
    }

    void render (float* const* out, int numOut, int from, int to)
    {
        if (to <= from) return;

        for (int vi = 0; vi < maxVoices; ++vi)
        {
            Voice& v = voices[(size_t) vi];
            if (! v.active) continue;
            const LoopParams& p = params.loops[(size_t) v.slot];
            const float attackSamples = v.attackS;
            const float decaySamples  = v.decayS;
            const float sustain = v.sustain;
            const int type = p.filterType;
            const Sample* smp = v.smp;
            if (smp == nullptr || smp->length() < 2) { v.active = false; continue; }
            const float* L = smp->left.data();
            const float* R = smp->right.data();
            const int len = smp->length();

            // channels: a pair or a single channel; outside the host's channels, back to 1-2
            float* outL = nullptr;
            float* outR = nullptr;
            if (v.stereo)
            {
                const int c = v.out + 1 < numOut ? v.out : 0;
                outL = out[c];
                outR = numOut > 1 ? out[c + 1] : nullptr;
            }
            else if (v.out < numOut) outL = out[v.out];
            else { outL = out[0]; outR = numOut > 1 ? out[1] : nullptr; }
            const bool monoSum = outR == nullptr;
            const float fadeFrames = 48.0f;

            for (int i = from; i < to; ++i)
            {
                if ((i & 31) == 0 || i == from)
                {
                    const double ph = lfoPhase[(size_t) v.slot] + (double) p.lfoRate * i / sr;
                    updateControl (v, (float) std::sin (ph * 6.283185307179586), tickAt (i));
                }
                ++v.elapsed;

                // envelope
                switch (v.stage)
                {
                    case 0: v.env += 1.0f / attackSamples; if (v.env >= 1.0f) { v.env = 1.0f; v.stage = 1; } break;
                    case 1: v.env -= (1.0f - sustain) / decaySamples; if (v.env <= sustain) { v.env = sustain; v.stage = 2; } break;
                    case 2: v.env = sustain; break;
                    default: v.env -= v.releaseStep; break;
                }
                v.decayLevel *= v.decayMul;
                if ((v.stage == 3 && v.env <= 0.0f) || v.decayLevel < 1.0e-4f || (v.stage == 2 && sustain <= 0.0f))
                {
                    v.active = false;
                    break;
                }

                // read
                const int ip = (int) std::floor (v.pos);
                const float t = (float) (v.pos - ip);
                float l = detail::hermite (L, len, v.begin, v.end, ip, t);
                float r = detail::hermite (R, len, v.begin, v.end, ip, t);

                // short fades at the slice edges avoid clicks
                const double dist = v.reverse ? v.pos - v.begin : v.end - v.pos;
                const double fromStart = v.reverse ? v.end - v.pos : v.pos - v.begin;
                const float edge = (float) std::min (1.0, std::min (dist, fromStart + 8.0) / fadeFrames);

                // filter
                if (v.open < 1.0f)
                {
                    auto tick = [&] (float x, float& s1, float& s2) {
                        const float v3 = x - s2;
                        const float v1 = v.a1 * s1 + v.a2 * v3;
                        const float v2 = s2 + v.a2 * s1 + v.a3 * v3;
                        s1 = 2.0f * v1 - s1;
                        s2 = 2.0f * v2 - s2;
                        const float y = type == 0 ? v2 : (type == 1 ? v1 * v.k : x - v.k * v1 - v2);
                        return y + (x - y) * v.open;
                    };
                    l = tick (l, v.s1L, v.s2L);
                    r = tick (r, v.s1R, v.s2R);
                }

                const float a = v.env * v.decayLevel * std::max (0.0f, edge);
                if (! monoSum)
                {
                    outL[i] += l * a * v.gL;
                    outR[i] += r * a * v.gR;
                }
                else outL[i] += 0.5f * (l * v.gL + r * v.gR) * a;

                v.pos += v.reverse ? -(double) v.inc : (double) v.inc;
                if (v.reverse ? v.pos < (double) v.begin : v.pos >= (double) v.end - 1.0)
                {
                    v.active = false;
                    break;
                }
            }
        }
    }

    //--------------------------------------------------------------------------
    double sr = 44100.0;
    Params params;
    std::array<Voice, maxVoices> voices {};
    uint32_t voiceCounter = 0;
    std::array<Action, 1024> actions {};
    int numActions = 0;

    int currentSlot = 0, pendingSlot = -1;
    bool running = false;
    int currentMode = -1;
    double expectedPpq = -1.0e9, internalPpq = 0.0, framePos = 0.0, loopFrac = -1.0;
    double shownBpm = 120.0;
    bool shownHostTempo = false;
    int lastPlayed = -1, loopVoice = -1;
    std::array<double, maxSlots> lfoPhase {};
    float smoothedGain = -1.0f, peakSinceSnapshot = 0.0f;
    detail::Random rng;

    // modulation
    modlib::Lfo lfos[numGroups][2];
    float lfoVal[numGroups][2][maxTicks] {};
    int numTicks = 1, curOffset = 0;
    double syncPpq = 0.0;
    float loopOff[numLoopDests] {};
    modlib::Random modRng;
    std::array<Event, 512> chunkEvents {};
};

} // namespace rebanada
