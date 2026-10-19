// Development tool: opens the interface with two loops loaded, plays a few seconds
// and saves a PNG. "--state" checks that a session saves and restores its slices.
// Only built with -DREBANADA_SNAPSHOT=ON.

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"
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
        sp.pitch = 5.0f; sp.reverse = true; sp.out = 7; sp.stereo = false; sp.group = 2;
        a.setSlice (3, 2, sp);
        a.assignGroupMod (2, rebanada::vdCutoff, rebanada::srcEnv);
        a.assignLoopMod (rebanada::ldRepeat, 1 + 3 * 2);   // D LFO 1
        a.setBars (3, 4);
        const int slices = a.slotInfo (3).numSlices;
        juce::MemoryBlock block;
        a.getStateInformation (block);

        RebanadaProcessor b;
        b.setStateInformation (block.getData(), (int) block.getSize());
        const auto info = b.slotInfo (3);
        const auto back = b.getSlice (3, 2);
        const bool ok = loaded && info.loaded() && info.numSlices == slices && info.bars == 4
                        && back.pitch == 5.0f && back.reverse && back.out == 7 && ! back.stereo && back.group == 2
                        && b.groupSourceOf (2, rebanada::vdCutoff) == rebanada::srcEnv && b.loopSourceOf (rebanada::ldRepeat) == 7
                        && std::abs (b.groupDepth (2, rebanada::vdCutoff) - 0.3f) < 0.01f;
        std::printf ("%s  session restored: %d slices, %d bars, slice 3 pitch %.1f, mono channel %d, group %c, routes kept\n", ok ? "ok  " : "FAIL",
                     info.numSlices, info.bars, (double) back.pitch, back.out + 1, (char) ('A' + back.group));

        // a session saved by 0.1: shared settings go to every loop, output pairs become channels
        juce::XmlElement old ("REBANADA");
        auto* cut = old.createNewChildElement ("PARAM"); cut->setAttribute ("id", "cutoff"); cut->setAttribute ("value", 500.0);
        auto* ord = old.createNewChildElement ("PARAM"); ord->setAttribute ("id", "order"); ord->setAttribute ("value", 2.0);
        auto* sl = old.createNewChildElement ("SLOTS")->createNewChildElement ("SLOT");
        sl->setAttribute ("index", 1);
        sl->setAttribute ("path", writeTestLoop ("rebanada-old.wav", 1).getFullPathName());
        sl->setAttribute ("bars", 2);
        sl->setAttribute ("slices", "0 0 0 0 0 2000 0 0 0 3;44100 0 0 0 0 2000 0 0 0 0");
        juce::MemoryBlock oldBlock;
        juce::AudioProcessor::copyXmlToBinary (old, oldBlock);
        RebanadaProcessor c;
        c.setStateInformation (oldBlock.getData(), (int) oldBlock.getSize());
        const float c3 = c.apvts.getRawParameterValue ("L3_cutoff")->load();
        const int o8 = (int) c.apvts.getRawParameterValue ("L8_order")->load();
        const auto first = c.getSlice (1, 0);
        const bool ok2 = std::abs (c3 - 500.0f) < 1.0f && o8 == 2 && first.out == 6 && first.stereo;
        std::printf ("%s  0.1 session: cutoff %.0f on loop 3, order %d on loop 8, pair 4 became channels %d-%d\n", ok2 ? "ok  " : "FAIL",
                     (double) c3, o8, first.out + 1, first.out + 2);
        return ok && ok2 ? 0 : 1;
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
        layout.outputBuses.add (juce::AudioChannelSet::discreteChannels (4));
        check (proc.setBusesLayout (layout) && proc.getTotalNumOutputChannels() == 4, "four output channels");
        proc.setPlayHead (&head);
        proc.prepareToPlay (48000.0, 512);
        juce::String error;
        check (proc.loadFile (0, writeTestLoop ("rebanada-play.wav", 0), error), "the loop loads " + error);
        const int slices = proc.slotInfo (0).numSlices;
        check (slices >= 8 && slices <= 16, "the loop is cut into " + juce::String (slices) + " slices");
        auto sp = proc.getSlice (0, 1); sp.out = 2; sp.stereo = true; proc.setSlice (0, 1, sp);

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

        // modulation: slice 2 joins group B, whose LFO 1 (a slow square, now at +1) pulls its level down
        sp.group = 1; proc.setSlice (0, 1, sp);
        proc.setParam ("gB_lfo1Shape", 3.0f);
        proc.setParam ("gB_lfo1Rate", 0.01f);
        proc.assignGroupMod (1, rebanada::vdLevel, rebanada::srcLfo1);
        proc.setParam ("gB_r1Amt", -100.0f);
        auto [f5, p5] = play (60, 37);
        check (f5 && p5[2] < 0.3f * p4[2], "a group B route turns slice 2 down (peak " + juce::String (p5[2], 3) + ")");
        auto [f6, p6] = play (60, 36);
        check (f6 && p6[0] > 0.2f, "slice 1, in group A, is untouched");
        std::printf (failures == 0 ? "\nall playback tests passed\n" : "\n%d playback tests failed\n", failures);
        return failures == 0 ? 0 : 1;
    }

    const juce::String out = argc > 1 ? argv[1] : "rebanada.png";
    const bool spanish = argc > 2 && juce::String (argv[2]) == "es";
    const int width  = argc > 3 ? juce::String (argv[3]).getIntValue() : 1200;
    const int height = argc > 4 ? juce::String (argv[4]).getIntValue() : 810;
    const bool empty = argc > 5 && juce::String (argv[5]) == "empty";
    const bool zoom = argc > 5 && juce::String (argv[5]) == "zoom";
    const juce::String modView = argc > 5 && juce::String (argv[5]).startsWith ("mod") ? juce::String (argv[5]) : juce::String();
    const int outs = argc > 6 ? juce::String (argv[6]).getIntValue() : 2;

    auto proc = std::make_unique<RebanadaProcessor>();
    proc->setPlayConfigDetails (0, outs, 44100.0, 512);
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
        // some modulation, so the arcs and the groups show
        for (int i : { 1, 5, 9 }) { auto q = proc->getSlice (0, i); q.group = 1; proc->setSlice (0, i, q); }
        for (int i : { 2, 6 }) { auto q = proc->getSlice (0, i); q.group = 2; proc->setSlice (0, i, q); }
        { auto q = proc->getSlice (0, 7); q.group = 3; proc->setSlice (0, 7, q); }
        proc->assignGroupMod (0, rebanada::vdPitch, rebanada::srcLfo1);
        proc->assignGroupMod (0, rebanada::vdCutoff, rebanada::srcEnv);
        proc->assignGroupMod (0, rebanada::vdPan, rebanada::srcRnd);
        proc->assignGroupMod (0, rebanada::vdAttack, rebanada::srcLfo2);
        proc->setParam ("gA_r1Amt", 20.0f);
        proc->setParam ("gA_r2Amt", -45.0f);
        proc->setParam ("gA_r3Amt", 80.0f);
        proc->setParam ("gA_lfo1Shape", 1.0f);
        proc->setParam ("gA_lfo2Sync", 1.0f);
        proc->assignLoopMod (rebanada::ldRepeat, 1);
        proc->assignLoopMod (rebanada::ldChance, 4);
        proc->setParam ("lr1Amt", 55.0f);
        proc->setParam ("lr2Amt", -35.0f);
        proc->requestSlot (0);
    }

    std::unique_ptr<juce::AudioProcessorEditor> editor (proc->createEditor());
    editor->setSize (width, height);
    run (*proc, 140);
    if (zoom)
    {
        std::function<rebanada::rack::WaveView* (juce::Component*)> find = [&] (juce::Component* c) -> rebanada::rack::WaveView* {
            if (auto* w = dynamic_cast<rebanada::rack::WaveView*> (c)) return w;
            for (auto* child : c->getChildren()) if (auto* w = find (child)) return w;
            return nullptr;
        };
        if (auto* w = find (editor.get())) w->zoomAround (44100.0 * 1.1, 0.3);
    }
    if (modView.isNotEmpty())
    {
        std::function<rebanada::rack::Content* (juce::Component*)> find = [&] (juce::Component* c) -> rebanada::rack::Content* {
            if (auto* w = dynamic_cast<rebanada::rack::Content*> (c)) return w;
            for (auto* child : c->getChildren()) if (auto* w = find (child)) return w;
            return nullptr;
        };
        if (auto* c = find (editor.get()))
        {
            c->showPage (1);
            if (modView == "modloop") c->showModTarget (4);
            if (modView == "modenv") c->showModSource (2);
            if (modView == "modsync") c->showModSource (1);
        }
    }
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
