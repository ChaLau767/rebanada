// Rebanada — finding slices in a sample (no JUCE)
// Copyright (C) 2026 Lautaro Martín Vieyra
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include "RebanadaEngine.h"
#include <vector>

namespace rebanada::slicer
{

inline std::vector<float> mono (const Sample& s)
{
    std::vector<float> m ((size_t) s.length());
    for (int i = 0; i < s.length(); ++i) m[(size_t) i] = 0.5f * (s.left[(size_t) i] + s.right[(size_t) i]);
    return m;
}

// Moves a cut to the nearest zero crossing within 1.5 ms, so slices start without a click.
inline int snapToZero (const std::vector<float>& x, int pos, double sr)
{
    const int len = (int) x.size();
    if (pos <= 0 || pos >= len - 1) return std::clamp (pos, 0, std::max (0, len - 1));
    const int reach = std::max (1, (int) (sr * 0.0015));
    for (int d = 0; d <= reach; ++d)
        for (int sign : { -1, 1 })
        {
            const int i = pos + sign * d;
            if (i <= 0 || i >= len) continue;
            const float a = x[(size_t) i - 1], b = x[(size_t) i];
            if ((a <= 0.0f && b > 0.0f) || (a >= 0.0f && b < 0.0f) || std::abs (b) < 1.0e-20f)
                return std::abs (a) < std::abs (b) ? i - 1 : i;
        }
    return pos;
}

// Equal divisions (for loops with a steady grid).
inline std::vector<int> grid (const std::vector<float>& x, int divisions, bool zeroSnap, double sr)
{
    const int len = (int) x.size();
    std::vector<int> cuts { 0 };
    divisions = std::clamp (divisions, 1, maxSlices);
    for (int k = 1; k < divisions; ++k)
    {
        int c = (int) ((long long) len * k / divisions);
        if (zeroSnap) c = snapToZero (x, c, sr);
        if (c > cuts.back() + 16 && c < len - 16) cuts.push_back (c);
    }
    return cuts;
}

// Finds attacks. sensitivity 0…100: higher finds more (and softer) attacks.
inline std::vector<int> detect (const std::vector<float>& x, double sr, float sensitivity, bool zeroSnap, int maxCount = maxSlices)
{
    const int len = (int) x.size();
    std::vector<int> cuts { 0 };
    if (len < 1024) return cuts;

    const float s = std::clamp (sensitivity, 0.0f, 100.0f) * 0.01f;
    const int hop = std::max (64, (int) std::lround (sr * 0.0058));
    const int frame = hop * 2;
    const int frames = (len - frame) / hop;
    if (frames < 4) return cuts;

    // energy of the signal with its highs emphasised: attacks stand out
    std::vector<float> db ((size_t) frames);
    float loudest = -200.0f;
    for (int f = 0; f < frames; ++f)
    {
        double e = 0.0;
        const int a = f * hop;
        for (int i = a; i < a + frame; ++i)
        {
            const float d = x[(size_t) i] - (i > 0 ? 0.95f * x[(size_t) i - 1] : 0.0f);
            e += (double) d * d + 0.15 * (double) x[(size_t) i] * x[(size_t) i];
        }
        db[(size_t) f] = (float) (10.0 * std::log10 (e / frame + 1.0e-12));
        loudest = std::max (loudest, db[(size_t) f]);
    }

    std::vector<float> rise ((size_t) frames, 0.0f);
    for (int f = 2; f < frames; ++f)
        rise[(size_t) f] = db[(size_t) f] - 0.5f * (db[(size_t) f - 1] + db[(size_t) f - 2]);

    const float threshold = 1.8f + 13.0f * std::pow (1.0f - s, 1.5f);   // dB of sudden rise
    const float floorDb = loudest - (30.0f + 25.0f * s);                // ignore the noise floor
    const int minGap = std::max (2, (int) (sr * (0.075 - 0.045 * s) / hop));

    struct Onset { int frame; float strength; };
    std::vector<Onset> found;
    int last = -minGap;
    for (int f = 2; f < frames - 1; ++f)
    {
        const float r = rise[(size_t) f];
        if (r < threshold || db[(size_t) f] < floorDb) continue;
        if (r < rise[(size_t) f - 1] || r < rise[(size_t) f + 1]) continue;
        if (f - last < minGap)
        {
            if (! found.empty() && r > found.back().strength) { found.back() = { f, r }; last = f; }
            continue;
        }
        found.push_back ({ f, r });
        last = f;
    }

    if ((int) found.size() > maxCount - 1)
    {
        std::sort (found.begin(), found.end(), [] (const Onset& a, const Onset& b) { return a.strength > b.strength; });
        found.resize ((size_t) maxCount - 1);
        std::sort (found.begin(), found.end(), [] (const Onset& a, const Onset& b) { return a.frame < b.frame; });
    }

    for (const auto& o : found)
    {
        // refine: the first sample that clearly belongs to the attack, a little earlier
        const int a = std::max (0, o.frame * hop - hop), b = std::min (len, o.frame * hop + frame);
        float peak = 0.0f;
        for (int i = a; i < b; ++i) peak = std::max (peak, std::abs (x[(size_t) i]));
        int pos = o.frame * hop;
        for (int i = a; i < b; ++i)
            if (std::abs (x[(size_t) i]) > 0.2f * peak) { pos = i; break; }
        pos = std::max (0, pos - (int) (sr * 0.001));
        if (zeroSnap) pos = snapToZero (x, pos, sr);
        if (pos > cuts.back() + (int) (sr * 0.012) && pos < len - (int) (sr * 0.01)) cuts.push_back (pos);
    }
    return cuts;
}

// A sensible bar count for a loop: the tempo closest to 120 BPM between 80 and 160.
inline int estimateBars (int length, double sr)
{
    const double seconds = length / std::max (1.0, sr);
    if (seconds < 0.4) return 1;
    int best = 1;
    double bestDist = 1.0e9;
    for (int bars : { 1, 2, 4, 8, 16, 32 })
    {
        const double bpm = bars * 4.0 * 60.0 / seconds;
        const double dist = (bpm >= 80.0 && bpm < 160.0) ? std::abs (bpm - 120.0) : 1000.0 + std::abs (bpm - 120.0);
        if (dist < bestDist) { bestDist = dist; best = bars; }
    }
    return best;
}

} // namespace rebanada::slicer
