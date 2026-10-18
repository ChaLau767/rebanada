// Rebanada — processor
// Copyright (C) 2026 Lautaro Martín Vieyra
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "RebanadaEngine.h"
#include "Parameters.h"

class RebanadaProcessor : public juce::AudioProcessor
{
public:
    RebanadaProcessor();
    ~RebanadaProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Rebanada"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Rebanada"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==========================================================================
    // Everything below is called from the message thread (the interface).
    struct SlotInfo
    {
        std::shared_ptr<const rebanada::Sample> sample;
        juce::String name, path;
        bool missing = false;
        int bars = 2, numSlices = 0;
        std::vector<int> starts;
        std::vector<rebanada::SliceParams> params;
        float sensitivity = 50.0f;
        bool loaded() const { return sample != nullptr && sample->length() > 1; }
    };

    SlotInfo slotInfo (int slot);
    bool slotLoaded (int slot) const { return slot >= 0 && slot < rebanada::maxSlots && ! meta[(size_t) slot].mono.empty(); }
    bool loadFile (int slot, const juce::File& file, juce::String& error);
    void clearSlot (int slot);

    rebanada::SliceParams getSlice (int slot, int slice);
    void setSlice (int slot, int slice, const rebanada::SliceParams&);

    void setBars (int slot, int bars);
    void setSensitivity (int slot, float sensitivity);   // also re-slices
    void reslice (int slot);
    int  addMarker (int slot, int position);              // returns the new slice index, or -1
    void moveMarker (int slot, int index, int position);
    void removeMarker (int slot, int index);
    int  split (int slot, int slice);
    int  merge (int slot, int slice);

    bool getZeroSnap() const { return apvts.state.getProperty ("zeroSnap", true); }
    void setZeroSnap (bool on) { apvts.state.setProperty ("zeroSnap", on, nullptr); }
    bool getGrid() const { return apvts.state.getProperty ("grid", false); }
    void setGrid (bool on, int slot) { apvts.state.setProperty ("grid", on, nullptr); reslice (slot); }

    // The loop shown in the interface: the requested one while a change is pending.
    int editSlot() const;
    void requestSlot (int slot);
    int selectedSlice (int slot) const { return selected[(size_t) juce::jlimit (0, rebanada::maxSlots - 1, slot)]; }
    void setSelectedSlice (int slot, int slice) { selected[(size_t) juce::jlimit (0, rebanada::maxSlots - 1, slot)] = juce::jmax (0, slice); }

    rebanada::Lang getLanguage() const;
    void setLanguage (rebanada::Lang);

    // A MIDI file with one note per slice, in the loop's order, for dragging onto a track.
    juce::File writeMidiFile (int slot);

    bool readTelemetry (rebanada::Telemetry& dest);
    int getVersion() const { return version.load(); }
    int numOutputs() const { return getTotalNumOutputChannels(); }

    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboard;    // notes played on the interface's keys
    juce::AudioFormatManager formats;

private:
    static BusesProperties buses();
    rebanada::Params readParams() const noexcept;
    template <typename F> void edit (int slot, F&& f);
    void sliceSample (rebanada::Slot&, int slot);
    juce::ValueTree saveSlots() const;
    void restoreSlots (const juce::ValueTree&);

    rebanada::Engine engine;
    juce::SpinLock engineLock;

    struct Meta
    {
        juce::String path;
        bool missing = false;
        float sensitivity = 50.0f;
        std::vector<float> mono;
        juce::ValueTree saved;   // what the session said, kept while the file is missing
    };
    std::array<Meta, rebanada::maxSlots> meta;
    std::array<int, rebanada::maxSlots> selected {};
    std::atomic<int> requested { -1 };
    std::atomic<int> version { 0 };
    std::atomic<int> engineCurrent { 0 }, enginePending { -1 };

    std::array<rebanada::Event, 1024> events {};
    rebanada::Telemetry telemetry;
    juce::SpinLock telemetryLock;
    int samplesSinceTelemetry = 0;

    struct LoopRefs
    {
        std::atomic<float> *chance, *repeat, *lfoRate, *lfoDepth, *attack, *decay, *sustain, *release, *cutoff, *reso,
                           *transpose, *rate, *level, *mode, *order, *lfoDest, *filterType;
    };
    struct Refs
    {
        std::atomic<float> *gain = nullptr, *tempo = nullptr, *switchAt = nullptr, *play = nullptr;
        std::array<LoopRefs, rebanada::maxSlots> loops {};
    } refs;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RebanadaProcessor)
};
