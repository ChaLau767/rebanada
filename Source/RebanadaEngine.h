// Rebanada — slice sampler engine (no JUCE: it can be tested on its own)
// Copyright (C) 2026 Lautaro Martín Vieyra
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// Eight loop slots. Each slot holds a sample cut into slices. A sequencer plays the
// slices of the current slot in time (synced to the host tempo, or in the loop's own
// time), and MIDI notes play single slices. Every slice has its own pitch, level, pan,
// decay, filter offset, direction and output pair.

#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>

namespace rebanada
{

constexpr int maxSlots   = 8;
constexpr int maxSlices  = 128;
constexpr int maxVoices  = 48;
constexpr int maxPairs   = 8;      // stereo output pairs: up to 16 channels
constexpr int maxOutputs = maxPairs * 2;
constexpr int slotBaseNote  = 12;  // C0 … G0 choose the loop (middle C = C4 = 60)
constexpr int sliceBaseNote = 36;  // C2 plays slice 1
constexpr float decayFull = 2000.0f;   // slice decay at its maximum: the slice plays to its end

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
    int  out     = 0;          // output pair, 0 = main
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

struct Params
{
    int   mode = 0;          // 0 = sync to the host tempo, 1 = free (the loop's own time)
    int   switchAt = 2;      // loop changes: 0 instant, 1 next beat, 2 next bar, 3 end of the loop
    bool  play = true;       // loop playback on/off (MIDI notes always play)
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
    float gain = 0.0f;       // dB
    float transpose = 0.0f;  // semitones
    float rate = 1.0f;       // free mode: speed of the loop's clock
    float tempo = 120.0f;    // internal tempo when there is no host transport
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
        smoothedGain = -1.0f;
    }

    // Loop requested from the interface or from a note between C0 and G0.
    void requestSlot (int s) noexcept
    {
        if (s < 0 || s >= maxSlots) return;
        pendingSlot = s == currentSlot ? -1 : s;
    }

    int getCurrentSlot() const noexcept { return currentSlot; }

    // Which output pair (in the buffer given to process) each slice output goes to.
    void setRoute (const std::array<int, maxPairs>& r) noexcept { route = r; }
    int getPendingSlot() const noexcept { return pendingSlot; }

    // Call (with the engine locked) before freeing a sample a voice might still read.
    void stopVoicesOf (int slot) noexcept
    {
        for (int i = 0; i < maxVoices; ++i)
            if (voices[(size_t) i].active && voices[(size_t) i].slot == slot) voices[(size_t) i].active = false;
        if (slot == currentSlot) { loopVoice = -1; lastPlayed = -1; }
    }

    void process (float* const* out, int numOut, int n, const Params& p, const Clock& clock,
                  const Event* events, int numEvents)
    {
        for (int ch = 0; ch < numOut; ++ch) std::fill (out[ch], out[ch] + n, 0.0f);
        if (n <= 0 || numOut <= 0) return;

        params = p;
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

            while (si < numActions && actions[(size_t) si].offset <= done) apply (actions[(size_t) si++]);
            while (ei < numEvents && std::clamp (events[ei].offset, 0, n - 1) <= done) handle (events[ei++]);
        }
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
        lfoPhase = std::fmod (lfoPhase + (double) p.lfoRate * n / sr, 1.0);
        peakSinceSnapshot = std::max (peakSinceSnapshot, peak);
    }

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
        float gain = 1.0f, pan = 0.0f, filterOct = 0.0f, pitch = 0.0f;
        int pair = 0;
        uint32_t age = 0;
        // per-voice filter (state-variable, trapezoidal)
        float s1L = 0, s2L = 0, s1R = 0, s2R = 0;
        float a1 = 1, a2 = 0, a3 = 0, k = 1.4f, open = 1.0f;
        float inc = 1.0f, gL = 1.0f, gR = 1.0f;
    };

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
        if (p.mode == 0)
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
            const double speed = std::max (0.01, (double) params.rate) * s.sample->sampleRate / sr;   // frames per host sample

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

                if (params.order == 1) slice = rng.below (s.numSlices);
                else if (params.order == 2 && rng.next() * 100.0f < params.chance) slice = rng.below (s.numSlices);
                if (lastPlayed >= 0 && lastPlayed < s.numSlices && params.repeat > 0.0f && rng.next() * 100.0f < params.repeat)
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
        const float samples = std::max (1.0f, params.release * 0.001f * (float) sr);
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
        v.pitch = sp.pitch + sp.fine * 0.01f;
        v.gain = detail::dbToGain (sp.level) * detail::dbToGain (-24.0f * (1.0f - velocity));
        v.pan = sp.pan;
        v.filterOct = sp.filter;
        v.pair = std::clamp (sp.out, 0, maxPairs - 1);
        v.stage = 0;
        v.env = 0.0f;
        v.decayLevel = 1.0f;
        v.decayMul = sp.decay >= decayFull - 0.5f ? 1.0f
                   : std::pow (0.001f, 1.0f / std::max (1.0f, sp.decay * 0.001f * (float) sr));
        v.age = ++voiceCounter;
        // starting in the middle of a slice (after a jump): skip the attack click
        if (offset > 0.0) v.env = 0.0f;
        updateControl (v, 0.0f);
        return index;
    }

    // Pitch, filter and pan, refreshed every few samples.
    void updateControl (Voice& v, float lfo) noexcept
    {
        const Params& p = params;
        const float depth = std::clamp (p.lfoDepth * 0.01f, 0.0f, 1.0f);
        float pitch = v.pitch + p.transpose, oct = v.filterOct, pan = v.pan, amp = 1.0f;
        switch (p.lfoDest)
        {
            case 0: oct += lfo * depth * 3.0f; break;
            case 1: pitch += lfo * depth * depth * 12.0f; break;
            case 2: pan += lfo * depth; break;
            default: amp = 1.0f - depth * (0.5f + 0.5f * lfo); break;
        }
        v.inc = (float) (v.baseInc * std::pow (2.0, pitch / 12.0));

        pan = std::clamp (pan, -1.0f, 1.0f);
        v.gL = (pan <= 0.0f ? 1.0f : 1.0f - pan) * v.gain * amp;
        v.gR = (pan >= 0.0f ? 1.0f : 1.0f + pan) * v.gain * amp;

        const float fc = std::clamp (p.cutoff * std::pow (2.0f, oct), 20.0f, (float) (sr * 0.45));
        const float g = std::tan (3.14159265f * fc / (float) sr);
        const float res = std::clamp (p.reso * 0.01f, 0.0f, 1.0f);
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
        const Params& p = params;
        const float attackSamples  = std::max (1.0f, std::max (1.0f, p.attack) * 0.001f * (float) sr);
        const float decaySamples   = std::max (1.0f, p.decay * 0.001f * (float) sr);
        const float sustain = std::clamp (p.sustain * 0.01f, 0.0f, 1.0f);
        const int type = p.filterType;

        for (int vi = 0; vi < maxVoices; ++vi)
        {
            Voice& v = voices[(size_t) vi];
            if (! v.active) continue;
            const Sample* smp = v.smp;
            if (smp == nullptr || smp->length() < 2) { v.active = false; continue; }
            const float* L = smp->left.data();
            const float* R = smp->right.data();
            const int len = smp->length();

            int pair = route[(size_t) v.pair];
            if (pair * 2 + 1 >= numOut) pair = 0;
            float* outL = out[std::min (pair * 2, numOut - 1)];
            float* outR = numOut > 1 ? out[std::min (pair * 2 + 1, numOut - 1)] : nullptr;
            const float fadeFrames = 48.0f;

            for (int i = from; i < to; ++i)
            {
                if ((i & 31) == 0 || i == from)
                {
                    const double ph = lfoPhase + (double) p.lfoRate * i / sr;
                    updateControl (v, (float) std::sin (ph * 6.283185307179586));
                }

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
                if (outR != nullptr)
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
    std::array<int, maxPairs> route { 0, 1, 2, 3, 4, 5, 6, 7 };
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
    double lfoPhase = 0.0;
    float smoothedGain = -1.0f, peakSinceSnapshot = 0.0f;
    detail::Random rng;
};

} // namespace rebanada
