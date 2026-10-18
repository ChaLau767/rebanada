// Development tool: opens the interface with two loops loaded, plays a few seconds
// and saves a PNG. "--state" checks that a session saves and restores its slices.
// Only built with -DREBANADA_SNAPSHOT=ON.

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include <cstdio>

// A two-bar test loop: kick, snare and hats with different pitches.
static juce::File writeTestLoop (const juce::String& name, int variant)
{
    const double sr = 44100.0;
    const int len = (int) (sr * 4.0);
    juce::AudioBuffer<float> buf (2, len);
    buf.clear();
    juce::Random rng (7 + variant);
    for (int step = 0; step < 16; ++step)
    {
        const bool kick = step % 4 == 0, snare = step % 8 == 4;
        if (! kick && ! snare && (step + variant) % 3 == 0) continue;
        const int a = step * len / 16;
        for (int i = 0; i < len / 16 && a + i < len; ++i)
        {
            const double t = i / sr;
            double v = 0.0;
            if (kick)       v = 0.9 * std::exp (-t * 14.0) * std::sin (2.0 * juce::MathConstants<double>::pi * (55.0 + 90.0 * std::exp (-t * 30.0)) * t);
            else if (snare) v = 0.6 * std::exp (-t * 22.0) * (rng.nextFloat() * 2.0 - 1.0);
            else            v = 0.3 * std::exp (-t * 60.0) * (rng.nextFloat() * 2.0 - 1.0);
            buf.setSample (0, a + i, (float) v);
            buf.setSample (1, a + i, (float) (v * 0.9));
        }
    }
    auto f = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile (name);
    f.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (f), sr, 2, 24, {}, 0));
    if (w != nullptr) w->writeFromAudioSampleBuffer (buf, 0, len);
    return f;
}

static void run (RebanadaProcessor& proc, int blocks)
{
    juce::AudioBuffer<float> buf (2, 512);
    juce::MidiBuffer midi;
    for (int b = 0; b < blocks; ++b)
    {
        buf.clear();
        proc.processBlock (buf, midi);
        if (b % 4 == 0) juce::MessageManager::getInstance()->runDispatchLoopUntil (1);
    }
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;

    if (argc > 1 && juce::String (argv[1]) == "--state")
    {
        RebanadaProcessor a;
        a.setPlayConfigDetails (0, 2, 44100.0, 512);
        a.prepareToPlay (44100.0, 512);
        juce::String error;
        const bool loaded = a.loadFile (3, writeTestLoop ("rebanada-state.wav", 0), error);
        auto sp = a.getSlice (3, 2);
        sp.pitch = 5.0f; sp.reverse = true; sp.out = 1;
        a.setSlice (3, 2, sp);
        a.setBars (3, 4);
        const int slices = a.slotInfo (3).numSlices;
        juce::MemoryBlock block;
        a.getStateInformation (block);

        RebanadaProcessor b;
        b.setStateInformation (block.getData(), (int) block.getSize());
        const auto info = b.slotInfo (3);
        const auto back = b.getSlice (3, 2);
        const bool ok = loaded && info.loaded() && info.numSlices == slices && info.bars == 4
                        && back.pitch == 5.0f && back.reverse && back.out == 1;
        std::printf ("%s  session restored: %d slices, %d bars, slice 3 pitch %.1f\n", ok ? "ok  " : "FAIL",
                     info.numSlices, info.bars, (double) back.pitch);
        return ok ? 0 : 1;
    }

    if (argc > 1 && juce::String (argv[1]) == "--play")
    {
        // the processor as a host drives it: transport, then notes, with two outputs routed
        struct Head : juce::AudioPlayHead
        {
            double ppq = 0.0; bool playing = true;
            juce::Optional<PositionInfo> getPosition() const override
            {
                PositionInfo p; p.setIsPlaying (playing); p.setBpm (100.0); p.setPpqPosition (ppq);
                p.setTimeSignature (TimeSignature { 4, 4 });
                return p;
            }
        } head;
        int failures = 0;
        auto check = [&failures] (bool ok, const juce::String& what) { std::printf ("%s  %s\n", ok ? "ok  " : "FAIL", what.toRawUTF8()); if (! ok) ++failures; };

        RebanadaProcessor proc;
        juce::AudioProcessor::BusesLayout layout;
        for (int i = 0; i < proc.getBusCount (false); ++i) layout.outputBuses.add (i < 2 ? juce::AudioChannelSet::stereo() : juce::AudioChannelSet::disabled());
        check (proc.setBusesLayout (layout) && proc.getTotalNumOutputChannels() == 4, "main and Out 3-4 enabled");
        proc.setPlayHead (&head);
        proc.prepareToPlay (48000.0, 512);
        juce::String error;
        check (proc.loadFile (0, writeTestLoop ("rebanada-play.wav", 0), error), "the loop loads " + error);
        const int slices = proc.slotInfo (0).numSlices;
        check (slices >= 8 && slices <= 16, "the loop is cut into " + juce::String (slices) + " slices");
        auto sp = proc.getSlice (0, 1); sp.out = 1; proc.setSlice (0, 1, sp);

        auto play = [&] (int blocks, int note) {
            juce::AudioBuffer<float> buf (4, 512);
            std::array<float, 4> peak {};
            bool finite = true;
            for (int b = 0; b < blocks; ++b)
            {
                buf.clear();
                juce::MidiBuffer midi;
                if (note >= 0 && b == 0) midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 110), 10);
                if (note >= 0 && b == 30) midi.addEvent (juce::MidiMessage::noteOff (1, note), 0);
                proc.processBlock (buf, midi);
                for (int ch = 0; ch < 4; ++ch)
                    for (int i = 0; i < 512; ++i)
                    {
                        const float v = buf.getSample (ch, i);
                        if (! std::isfinite (v)) finite = false;
                        peak[(size_t) ch] = juce::jmax (peak[(size_t) ch], std::abs (v));
                    }
                if (head.playing) head.ppq += 100.0 / 60.0 * 512 / 48000.0;
            }
            return std::make_pair (finite, peak);
        };

        auto [f1, p1] = play (400, -1);
        check (f1 && p1[0] > 0.2f, "the loop plays with the transport (peak " + juce::String (p1[0], 2) + ")");
        check (p1[2] > 0.05f, "slice 2 sounds on Out 3-4 (peak " + juce::String (p1[2], 2) + ")");
        head.playing = false;
        auto [f2, p2] = play (60, -1);
        check (f2 && p2[0] < 0.02f, "silence when the transport stops");
        auto [f3, p3] = play (60, 36);
        check (f3 && p3[0] > 0.2f, "C2 plays slice 1 (peak " + juce::String (p3[0], 2) + ")");
        auto [f4, p4] = play (60, 37);
        check (f4 && p4[2] > 0.05f && p4[0] < 0.02f, "C#2 plays slice 2, routed to Out 3-4 only");
        std::printf (failures == 0 ? "\nall playback tests passed\n" : "\n%d playback tests failed\n", failures);
        return failures == 0 ? 0 : 1;
    }

    const juce::String out = argc > 1 ? argv[1] : "rebanada.png";
    const bool spanish = argc > 2 && juce::String (argv[2]) == "es";
    const int width  = argc > 3 ? juce::String (argv[3]).getIntValue() : 1200;
    const int height = argc > 4 ? juce::String (argv[4]).getIntValue() : 780;
    const bool empty = argc > 5 && juce::String (argv[5]) == "empty";

    auto proc = std::make_unique<RebanadaProcessor>();
    proc->setPlayConfigDetails (0, 2, 44100.0, 512);
    if (spanish) proc->setLanguage (rebanada::Lang::es);
    proc->prepareToPlay (44100.0, 512);
    if (! empty)
    {
        juce::String error;
        proc->loadFile (0, writeTestLoop ("rebanada-loop-1.wav", 0), error);
        proc->loadFile (2, writeTestLoop ("rebanada-loop-3.wav", 1), error);
        proc->setSelectedSlice (0, 3);
        auto sp = proc->getSlice (0, 5);
        sp.reverse = true;
        proc->setSlice (0, 5, sp);
        sp = proc->getSlice (0, 3);
        sp.pitch = 3.0f; sp.pan = -0.18f; sp.decay = 340.0f; sp.filter = -0.8f;
        proc->setSlice (0, 3, sp);
        proc->requestSlot (0);
    }

    std::unique_ptr<juce::AudioProcessorEditor> editor (proc->createEditor());
    editor->setSize (width, height);
    run (*proc, 140);
    for (int i = 0; i < 10; ++i)
    {
        run (*proc, 3);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (40);
    }

    auto img = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
    juce::File f = juce::File::getCurrentWorkingDirectory().getChildFile (out);
    f.deleteFile();
    juce::FileOutputStream os (f);
    juce::PNGImageFormat().writeImageToStream (img, os);
    editor.reset();
    return 0;
}
