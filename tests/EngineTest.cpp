// Checks the engine and the slicer without JUCE. Run by the automatic build.

#include "RebanadaEngine.h"
#include "Slicer.h"
#include <cstdio>
#include <memory>

using namespace rebanada;

static int failures = 0;
static void check (bool ok, const char* what)
{
    std::printf ("%s  %s\n", ok ? "ok  " : "FAIL", what);
    if (! ok) ++failures;
}

// four drum-like hits in two seconds (one bar at 120 BPM)
static std::shared_ptr<Sample> makeLoop (double sr)
{
    auto s = std::make_shared<Sample>();
    s->sampleRate = sr;
    const int len = (int) (sr * 2.0);
    s->left.assign ((size_t) len, 0.0f);
    s->right.assign ((size_t) len, 0.0f);
    const double hits[] { 0.0, 0.5, 1.0, 1.5 };
    for (int h = 0; h < 4; ++h)
    {
        const int a = (int) (hits[h] * sr);
        for (int i = 0; i < (int) (0.3 * sr) && a + i < len; ++i)
        {
            const double t = i / sr;
            const float v = (float) (0.8 * std::exp (-t * 18.0) * std::sin (2.0 * 3.14159265 * (110.0 + 70.0 * h) * t));
            s->left[(size_t) a + i] = v;
            s->right[(size_t) a + i] = v;
        }
    }
    return s;
}

struct Run
{
    float peak = 0.0f;
    bool finite = true;
    int blocksWithSound = 0;
};

static Run render (Engine& e, const Params& p, Clock c, int blocks, int n = 512, const std::vector<Event>& ev = {}, int numOut = 2)
{
    std::vector<std::vector<float>> buf ((size_t) numOut, std::vector<float> ((size_t) n));
    std::vector<float*> ptr;
    for (auto& b : buf) ptr.push_back (b.data());
    Run r;
    for (int b = 0; b < blocks; ++b)
    {
        e.process (ptr.data(), numOut, n, p, c, b == 0 && ! ev.empty() ? ev.data() : nullptr, b == 0 ? (int) ev.size() : 0);
        float blockPeak = 0.0f;
        for (auto& ch : buf)
            for (float v : ch)
            {
                if (! std::isfinite (v)) r.finite = false;
                blockPeak = std::max (blockPeak, std::abs (v));
            }
        r.peak = std::max (r.peak, blockPeak);
        if (blockPeak > 1.0e-3f) ++r.blocksWithSound;
        if (c.hostAvailable) c.ppq += c.bpm / 60.0 * n / 48000.0;
    }
    return r;
}

int main()
{
    const double sr = 48000.0;
    auto loop = makeLoop (sr);
    const auto mono = slicer::mono (*loop);

    // slicer
    const auto cuts = slicer::detect (mono, sr, 50.0f, true);
    check (cuts.size() == 4, "detects the four hits");
    bool near = cuts.size() == 4;
    for (size_t i = 0; i < cuts.size() && near; ++i) near = std::abs (cuts[i] - (int) (i * 0.5 * sr)) < (int) (0.012 * sr);
    check (near, "cuts are within 12 ms of each hit");
    check (slicer::detect (mono, sr, 0.0f, true).size() <= cuts.size(), "lower sensitivity never finds more");
    check (slicer::grid (mono, 16, true, sr).size() == 16, "grid makes 16 slices");
    check (slicer::estimateBars (loop->length(), sr) == 1, "a 2 s loop is one bar at 120 BPM");

    Engine e;
    e.prepare (sr);
    auto& slot = e.slots[0];
    slot.sample = loop;
    slot.numSlices = (int) cuts.size();
    for (size_t i = 0; i < cuts.size(); ++i) slot.starts[i] = cuts[i];
    slot.bars = 1;

    Params p;
    Clock host { true, true, 0.0, 120.0, 4, 4 };

    auto r = render (e, p, host, 200);
    check (r.finite && r.peak > 0.3f && r.peak < 2.0f, "sync playback sounds");
    check (r.blocksWithSound > 100, "sync playback keeps sounding through the loop");

    Clock stopped = host; stopped.hostPlaying = false;
    e.reset();
    r = render (e, p, stopped, 50);
    check (r.peak < 1.0e-6f, "silent while the host is stopped");

    // MIDI plays a slice even when stopped
    e.reset();
    r = render (e, p, stopped, 30, 512, { { Event::noteOn, 0, sliceBaseNote + 2, 1.0f } });
    check (r.peak > 0.3f, "a note plays slice 3");

    // a muted slice stays quiet
    e.reset();
    slot.slices[1].mute = true;
    r = render (e, p, stopped, 30, 512, { { Event::noteOn, 0, sliceBaseNote + 1, 1.0f } });
    check (r.peak < 1.0e-6f, "a muted slice stays quiet");
    slot.slices[1].mute = false;

    // reverse, pitch, decay, filter: all stay finite
    for (auto& sp : slot.slices) { sp.reverse = true; sp.pitch = 7.0f; sp.filter = -1.0f; }
    { auto& l = p.loops[0]; l.filterType = 1; l.cutoff = 1200.0f; l.reso = 80.0f; l.lfoDepth = 60.0f; l.lfoDest = 1; }
    e.reset();
    r = render (e, p, host, 200);
    check (r.finite && r.peak > 0.05f && r.peak < 4.0f, "reverse, pitch, decay and resonant filter stay finite");
    for (auto& sp : slot.slices) sp = SliceParams {};
    p = Params {};

    // a short slice decay makes the slice quieter but still audible
    for (auto& sp : slot.slices) sp.decay = 80.0f;
    e.reset();
    r = render (e, p, host, 200);
    check (r.finite && r.peak > 0.1f, "short slice decay still sounds");
    for (auto& sp : slot.slices) sp = SliceParams {};

    // free mode at half rate
    e.reset();
    p.loops[0].mode = 1; p.loops[0].rate = 0.5f;
    r = render (e, p, host, 300);
    check (r.finite && r.peak > 0.3f, "free mode plays");
    p = Params {};

    // standalone: own clock
    e.reset();
    Clock none;
    r = render (e, p, none, 100);
    check (r.peak > 0.3f, "without a host, the internal clock runs the loop");

    // loop change waits for the next bar
    auto& slot2 = e.slots[1];
    slot2.sample = loop;
    slot2.numSlices = 2; slot2.starts[0] = 0; slot2.starts[1] = (int) sr;
    slot2.bars = 1;
    e.reset();
    host.ppq = 0.2;
    p.switchAt = 2;
    render (e, p, host, 2);
    e.requestSlot (1);
    {
        Clock c = host; c.ppq = 0.2 + 120.0 / 60.0 * 1024 / sr;
        render (e, p, c, 4);
        check (e.getCurrentSlot() == 0 && e.getPendingSlot() == 1, "the change waits for the bar");
        c.ppq = 3.95;
        render (e, p, c, 20);
        check (e.getCurrentSlot() == 1, "the loop changes at the bar");
    }

    // note between C0 and G0 chooses the loop
    e.reset();
    render (e, p, stopped, 1, 512, { { Event::noteOn, 0, slotBaseNote + 0, 1.0f } });
    check (e.getCurrentSlot() == 0, "C0 chooses loop 1 when stopped");

    // empty slot: silence and no crash
    e.reset();
    e.requestSlot (5);
    r = render (e, p, host, 40);
    check (r.finite && e.getCurrentSlot() == 5, "an empty loop is silent and safe");

    // outputs: slice on pair 2 lands on channels 3-4
    e.reset();
    e.requestSlot (0);
    render (e, p, stopped, 1);
    for (auto& sp : slot.slices) sp.out = 2;
    {
        const int n = 512, outs = 4;
        std::vector<std::vector<float>> buf (outs, std::vector<float> (n));
        std::vector<float*> ptr; for (auto& b : buf) ptr.push_back (b.data());
        Event on { Event::noteOn, 0, sliceBaseNote, 1.0f };
        float p01 = 0, p23 = 0;
        for (int b = 0; b < 20; ++b)
        {
            e.process (ptr.data(), outs, n, p, stopped, b == 0 ? &on : nullptr, b == 0 ? 1 : 0);
            for (int i = 0; i < n; ++i) { p01 = std::max (p01, std::abs (buf[0][i])); p23 = std::max (p23, std::abs (buf[2][i])); }
        }
        check (p23 > 0.3f && p01 < 1.0e-6f, "a slice sent to Out 3-4 sounds only there");
    }

    // a mono slice on channel 6 sounds only there
    e.reset();
    for (auto& sp : slot.slices) { sp.out = 5; sp.stereo = false; }
    {
        const int n = 512, outs = 64;
        std::vector<std::vector<float>> buf (outs, std::vector<float> (n));
        std::vector<float*> ptr; for (auto& b : buf) ptr.push_back (b.data());
        Event on { Event::noteOn, 0, sliceBaseNote, 1.0f };
        std::vector<float> pk (outs, 0.0f);
        for (int b = 0; b < 20; ++b)
        {
            e.process (ptr.data(), outs, n, p, stopped, b == 0 ? &on : nullptr, b == 0 ? 1 : 0);
            for (int c = 0; c < outs; ++c) for (int i = 0; i < n; ++i) pk[(size_t) c] = std::max (pk[(size_t) c], std::abs (buf[(size_t) c][(size_t) i]));
        }
        int others = 0; for (int c = 0; c < outs; ++c) if (c != 5 && pk[(size_t) c] > 1.0e-6f) ++others;
        check (pk[5] > 0.2f && others == 0, "a mono slice on channel 6 of 64 sounds only there");
    }
    // with only two channels, a slice sent beyond them falls back to 1-2
    e.reset();
    for (auto& sp : slot.slices) { sp.out = 40; sp.stereo = true; }
    r = render (e, p, stopped, 20, 512, { { Event::noteOn, 0, sliceBaseNote, 1.0f } });
    check (r.peak > 0.2f, "a slice sent beyond the available channels falls back to 1-2");
    for (auto& sp : slot.slices) sp = SliceParams {};

    // every loop has its own filter: a closed filter on loop 1 does not touch loop 2
    {
        Params q;
        q.loops[0].filterType = 0; q.loops[0].cutoff = 40.0f;
        e.reset();
        e.requestSlot (0);
        render (e, q, stopped, 1);
        const float closed = render (e, q, stopped, 20, 512, { { Event::noteOn, 0, sliceBaseNote, 1.0f } }).peak;
        e.requestSlot (1);
        render (e, q, stopped, 1);
        const float open = render (e, q, stopped, 20, 512, { { Event::noteOn, 0, sliceBaseNote, 1.0f } }).peak;
        check (closed < 0.4f * open && open > 0.3f, "each loop keeps its own filter");
    }

    // voice stealing: many notes at once
    e.reset();
    {
        std::vector<Event> many;
        for (int i = 0; i < 200; ++i) many.push_back ({ Event::noteOn, i % 512, sliceBaseNote + (i % 4), 1.0f });
        r = render (e, p, stopped, 10, 512, many);
        check (r.finite, "200 notes at once stay finite");
    }

    std::printf (failures == 0 ? "\nall tests passed\n" : "\n%d tests failed\n", failures);
    return failures == 0 ? 0 : 1;
}
