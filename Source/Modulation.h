// Modulation shared by Baraja and Rebanada: LFOs, an envelope follower and routes
// that move any continuous parameter. No JUCE: it can be tested on its own.
// Copyright (C) 2026 Lautaro Martín Vieyra
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// Routes work in each parameter's normalised range (0…1, the same space the knob
// turns in), so an amount of +50 % moves any parameter by half of its knob.

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace modlib
{

enum Shape { sine = 0, triangle, saw, square, steps, smooth, numShapes };

// Tempo divisions for synced LFOs, in quarter notes (append only: sessions store the index).
constexpr int numDivisions = 11;
inline double divisionQuarters (int index) noexcept
{
    static const double q[numDivisions] { 16.0, 8.0, 4.0, 2.0, 1.0, 0.5, 0.25, 0.125, 2.0 / 3.0, 1.0 / 3.0, 1.0 / 6.0 };
    return q[std::clamp (index, 0, numDivisions - 1)];
}
inline const char* divisionName (int index) noexcept
{
    static const char* n[numDivisions] { "4 bars", "2 bars", "1 bar", "1/2", "1/4", "1/8", "1/16", "1/32", "1/4 T", "1/8 T", "1/16 T" };
    return n[std::clamp (index, 0, numDivisions - 1)];
}

// How a parameter's value maps to its knob (the same curves as the host parameters).
struct Range
{
    enum Curve { linear = 0, logarithmic, skewed } curve = linear;
    float min = 0.0f, max = 1.0f, skew = 1.0f;   // skew: as in juce::NormalisableRange

    float toNorm (float v) const noexcept
    {
        v = std::clamp (v, min, max);
        if (max <= min) return 0.0f;
        switch (curve)
        {
            case logarithmic: return (std::log (std::max (v, 1.0e-9f)) - std::log (std::max (min, 1.0e-9f))) / (std::log (max) - std::log (std::max (min, 1.0e-9f)));
            case skewed:      return std::pow ((v - min) / (max - min), skew);
            default:          return (v - min) / (max - min);
        }
    }
    float fromNorm (float n) const noexcept
    {
        n = std::clamp (n, 0.0f, 1.0f);
        switch (curve)
        {
            case logarithmic: return std::exp (std::log (std::max (min, 1.0e-9f)) + n * (std::log (max) - std::log (std::max (min, 1.0e-9f))));
            case skewed:      return min + (max - min) * std::pow (n, 1.0f / skew);
            default:          return min + (max - min) * n;
        }
    }
    // the value moved by "offset" knob turns (offset in -1…+1 of the whole range)
    float apply (float value, float offset) const noexcept { return fromNorm (toNorm (value) + offset); }
};

struct Random
{
    uint32_t state = 0x1234567u;
    float bipolar() noexcept   // -1 … +1
    {
        state ^= state << 13; state ^= state >> 17; state ^= state << 5;
        return (float) (state >> 8) * (2.0f / 16777216.0f) - 1.0f;
    }
};

// Value of a shape at phase 0…1. a, b: random values of this cycle and the next (steps and smooth).
inline float shapeValue (int shape, double phase, float a, float b) noexcept
{
    const float p = (float) (phase - std::floor (phase));
    switch (shape)
    {
        case sine:     return std::sin (p * 6.2831853f);
        case triangle: return p < 0.5f ? p * 4.0f - 1.0f : 3.0f - p * 4.0f;
        case saw:      return 1.0f - 2.0f * p;
        case square:   return p < 0.5f ? 1.0f : -1.0f;
        case steps:    return a;
        default:       { const float s = 0.5f - 0.5f * std::cos (p * 3.14159265f); return a + (b - a) * s; }
    }
}

// A low-frequency oscillator, free (Hz) or locked to the host's position (quarter notes).
class Lfo
{
public:
    int shape = sine;
    float phaseOffset = 0.0f;   // 0…1

    void reset (uint32_t seed) noexcept
    {
        rng.state = seed | 1u;
        phase = 0.0;
        a = rng.bipolar();
        b = rng.bipolar();
        value = current();
    }

    // Free running: advance by seconds × Hz.
    float advanceFree (double cycles) noexcept
    {
        setPhase (phase + cycles);
        return value;
    }

    // Synced: the phase follows the song position.
    float syncTo (double ppq, double quartersPerCycle) noexcept
    {
        setPhase (ppq / std::max (1.0e-6, quartersPerCycle));
        return value;
    }

    float getValue() const noexcept { return value; }
    double getPhase() const noexcept { return phase - std::floor (phase); }

private:
    void setPhase (double p) noexcept
    {
        const double cycleBefore = std::floor (phase + phaseOffset);
        phase = p;
        const double cycleNow = std::floor (phase + phaseOffset);
        if (cycleNow != cycleBefore)   // a new cycle: new random values
        {
            const double jumps = std::abs (cycleNow - cycleBefore);
            if (jumps > 1.5) { a = rng.bipolar(); b = rng.bipolar(); }
            else { a = b; b = rng.bipolar(); }
        }
        value = current();
    }
    float current() const noexcept { return shapeValue (shape, phase + phaseOffset, a, b); }

    double phase = 0.0;
    float a = 0.0f, b = 0.0f, value = 0.0f;
    Random rng;
};

// Envelope follower: 0…1 from the level of a signal, with attack and release.
class Follower
{
public:
    void reset() noexcept { env = 0.0f; }

    float process (const float* l, const float* r, int n, float attackMs, float releaseMs, float gainDb, double sr) noexcept
    {
        const float att = 1.0f - std::exp (-1.0f / (std::max (0.1f, attackMs) * 0.001f * (float) sr));
        const float rel = 1.0f - std::exp (-1.0f / (std::max (1.0f, releaseMs) * 0.001f * (float) sr));
        const float g = std::pow (10.0f, gainDb * 0.05f);
        for (int i = 0; i < n; ++i)
        {
            const float x = std::max (std::abs (l ? l[i] : 0.0f), std::abs (r ? r[i] : 0.0f)) * g;
            env += (x - env) * (x > env ? att : rel);
        }
        if (! std::isfinite (env)) env = 0.0f;
        return std::min (1.0f, env);
    }
    float getValue() const noexcept { return std::min (1.0f, env); }

private:
    float env = 0.0f;
};

} // namespace modlib
