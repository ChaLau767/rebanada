// Rebanada — parameters and interface texts
// Copyright (C) 2026 Lautaro Martín Vieyra
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <cstring>
#include "RebanadaEngine.h"

namespace rebanada
{

// Source texts are UTF-8: always convert them with this.
inline juce::String u8 (const char* text) { return juce::String::fromUTF8 (text); }

enum class Lang { en = 0, es = 1 };

// toUpperCase() does not change accented letters: finish the job for Spanish.
inline juce::String caps (const juce::String& t)
{
    auto r = t.toUpperCase();
    const char* pairs[][2] { { "á", "Á" }, { "é", "É" }, { "í", "Í" }, { "ó", "Ó" }, { "ú", "Ú" }, { "ñ", "Ñ" }, { "ü", "Ü" } };
    for (auto& pr : pairs) r = r.replace (juce::String::fromUTF8 (pr[0]), juce::String::fromUTF8 (pr[1]));
    return r;
}

inline juce::String localNumber (juce::String t, Lang lang) { return lang == Lang::es ? t.replaceCharacter ('.', ',') : t; }

//==============================================================================
// Knob specifications: host parameters and per-slice settings share this shape.
struct FloatSpec
{
    const char* id;
    const char* en;
    const char* es;
    float min, max, def;
    const char* unit;
    int decimals;
    enum Scale { linear, logarithmic, skewed } scale;
    bool signedDisplay;
    const char* noteEn;
    const char* noteEs;
    float interval = 0.0f;
};

inline const std::vector<FloatSpec>& floatSpecs()
{
    using S = FloatSpec;
    static const std::vector<FloatSpec> specs {
        { "chance",    "Chance",     "Azar",       0.0f,   100.0f,   50.0f,    "%",   0, S::linear,      false, "Weighted order: share of steps that play another slice", "Orden ponderado: proporción de pasos que tocan otra rebanada" },
        { "repeat",    "Repeat",     "Repetir",    0.0f,   100.0f,   0.0f,     "%",   0, S::linear,      false, "Chance that a step repeats the slice before it", "Probabilidad de que un paso repita la rebanada anterior" },
        { "lfoRate",   "LFO rate",   "Vel. LFO",   0.02f,  20.0f,    0.5f,     "Hz",  2, S::logarithmic, false, "", "" },
        { "lfoDepth",  "LFO depth",  "Prof. LFO",  0.0f,   100.0f,   0.0f,     "%",   0, S::linear,      false, "How much the LFO moves its destination", "Cuánto mueve el LFO a su destino" },
        { "attack",    "Attack",     "Ataque",     0.0f,   1000.0f,  0.0f,     "ms",  0, S::skewed,      false, "", "" },
        { "decay",     "Decay",      "Caída",      5.0f,   5000.0f,  400.0f,   "ms",  0, S::logarithmic, false, "Time to fall to the sustain level", "Tiempo hasta llegar al nivel de sostén" },
        { "sustain",   "Sustain",    "Sostén",     0.0f,   100.0f,   100.0f,   "%",   0, S::linear,      false, "", "" },
        { "release",   "Release",    "Extinción",  5.0f,   5000.0f,  40.0f,    "ms",  0, S::logarithmic, false, "Fade after the slice is released", "Desvanecimiento al soltar la rebanada" },
        { "cutoff",    "Cutoff",     "Corte",      20.0f,  20000.0f, 20000.0f, "Hz",  0, S::logarithmic, false, "Filter frequency for every slice (each slice can shift it)", "Frecuencia del filtro para todas las rebanadas (cada una puede correrla)" },
        { "reso",      "Reso",       "Reson.",     0.0f,   100.0f,   0.0f,     "%",   0, S::linear,      false, "Filter resonance", "Resonancia del filtro" },
        { "transpose", "Transpose",  "Transp.",    -24.0f, 24.0f,    0.0f,     "st",  0, S::linear,      true,  "Transposes every slice of this loop", "Transpone todas las rebanadas de este loop", 1.0f },
        { "level",     "Level",      "Nivel",      -36.0f, 12.0f,    0.0f,     "dB",  1, S::linear,      true,  "Level of this loop", "Nivel de este loop" },
        { "gain",      "Output",     "Salida",     -36.0f, 12.0f,    0.0f,     "dB",  1, S::linear,      true,  "Output level of all the loops", "Nivel de salida de todos los loops" },
        { "rate",      "Rate",       "Velocidad",  0.25f,  4.0f,     1.0f,     "x",   2, S::logarithmic, false, "Free mode: speed of the loop's own clock (the slices keep their pitch)", "Modo libre: velocidad del reloj del loop (las rebanadas conservan su altura)" },
        { "tempo",     "Tempo",      "Tempo",      40.0f,  240.0f,   120.0f,   "BPM", 1, S::linear,      false, "Tempo used when there is no host transport (standalone app)", "Tempo cuando no hay transporte del anfitrión (aplicación independiente)" },
    };
    return specs;
}

// Settings that belong to each slice (stored with the session, not automatable).
inline const std::vector<FloatSpec>& sliceSpecs()
{
    using S = FloatSpec;
    static const std::vector<FloatSpec> specs {
        { "pitch",  "Pitch",  "Altura",  -24.0f, 24.0f, 0.0f,      "st",  0, S::linear,      true,  "Transposition of this slice in semitones", "Transposición de esta rebanada en semitonos", 1.0f },
        { "fine",   "Fine",   "Fino",    -50.0f, 50.0f, 0.0f,      "ct",  0, S::linear,      true,  "Fine tuning in cents", "Afinación fina en cents", 1.0f },
        { "level",  "Level",  "Nivel",   -36.0f, 6.0f,  0.0f,      "dB",  1, S::linear,      true,  "", "" },
        { "pan",    "Pan",    "Paneo",   -100.0f, 100.0f, 0.0f,    "",    0, S::linear,      false, "", "", 1.0f },
        { "decay",  "Decay",  "Caída",   10.0f,  decayFull, decayFull, "ms", 0, S::logarithmic, false, "Shortens the slice with a fade. At the top: the whole slice", "Acorta la rebanada con un desvanecimiento. Al máximo: la rebanada entera" },
        { "filter", "Filter", "Filtro",  -4.0f,  4.0f,  0.0f,      "oct", 1, S::linear,      true,  "Moves the filter cutoff for this slice", "Corre la frecuencia de corte para esta rebanada" },
    };
    return specs;
}

inline const FloatSpec& findSpec (const std::vector<FloatSpec>& list, const juce::String& id)
{
    for (const auto& s : list) if (id == s.id) return s;
    jassertfalse;
    return list.front();
}
// Host parameters of each loop are called "L1_cutoff" … "L8_cutoff".
inline juce::String loopId (int slot, const char* base) { return "L" + juce::String (slot + 1) + "_" + base; }
inline juce::String baseId (const juce::String& id) { return id.startsWith ("L") && id.containsChar ('_') ? id.fromFirstOccurrenceOf ("_", false, false) : id; }
inline bool isGlobal (const juce::String& id) { return id == "gain" || id == "tempo" || id == "play" || id == "switchAt"; }

inline const FloatSpec& spec (const juce::String& id)      { return findSpec (floatSpecs(), baseId (id)); }
inline const FloatSpec& sliceSpec (const juce::String& id) { return findSpec (sliceSpecs(), id); }

inline const juce::StringArray& modeChoices()    { static const juce::StringArray c { "Sync", "Free" }; return c; }
inline const juce::StringArray& switchChoices()  { static const juce::StringArray c { "Instant", "Next beat", "Next bar", "Loop end" }; return c; }
inline const juce::StringArray& orderChoices()   { static const juce::StringArray c { "Forward", "Random", "Weighted" }; return c; }
inline const juce::StringArray& lfoDestChoices() { static const juce::StringArray c { "Cutoff", "Pitch", "Pan", "Level" }; return c; }
inline const juce::StringArray& filterChoices()  { static const juce::StringArray c { "LP", "BP", "HP" }; return c; }

// Value text. The words "Open", "Full", "C", "L", "R" are translated by the interface.
inline juce::String formatValue (const FloatSpec& s, float v)
{
    const juce::String unit (s.unit), id (s.id);
    if (id == "cutoff")
    {
        if (v >= 19900.0f) return "Open";
        return v >= 1000.0f ? juce::String (v / 1000.0f, v >= 10000.0f ? 1 : 2) + " kHz" : juce::String (juce::roundToInt (v)) + " Hz";
    }
    if (id == "decay" && std::abs (s.max - decayFull) < 0.5f && v >= decayFull - 0.5f) return "Full";
    if (id == "pan")
    {
        const int p = juce::roundToInt (v);
        return p == 0 ? juce::String ("C") : (p < 0 ? "L" + juce::String (-p) : "R" + juce::String (p));
    }
    juce::String t;
    if (unit == "ms" && v >= 1000.0f) t = juce::String (v / 1000.0f, 2) + " s";
    else t = (s.decimals == 0 ? juce::String (juce::roundToInt (v)) : juce::String (v, s.decimals)) + (unit.isEmpty() ? "" : " " + unit);
    if (s.signedDisplay && v > 0.0f) t = "+" + t;
    return t;
}

inline float parseValue (const FloatSpec& s, juce::String t)
{
    t = t.trim().replaceCharacter (',', '.');
    const juce::String id (s.id);
    if (id == "cutoff" && t.startsWithIgnoreCase ("open")) return s.max;
    if (t.startsWithIgnoreCase ("full")) return s.max;
    if (id == "pan")
    {
        if (t.startsWithIgnoreCase ("c")) return 0.0f;
        if (t.startsWithIgnoreCase ("l")) return -t.substring (1).getFloatValue();
        if (t.startsWithIgnoreCase ("r")) return t.substring (1).getFloatValue();
    }
    const float v = t.getFloatValue();
    if (t.endsWithIgnoreCase ("khz")) return v * 1000.0f;
    if (juce::String (s.unit) == "ms" && t.endsWithIgnoreCase ("s") && ! t.endsWithIgnoreCase ("ms")) return v * 1000.0f;
    return v;
}

inline juce::NormalisableRange<float> rangeFor (const FloatSpec& s)
{
    if (s.scale == FloatSpec::logarithmic)
    {
        const float lmin = std::log (s.min), lmax = std::log (s.max);
        return juce::NormalisableRange<float> (s.min, s.max,
            [lmin, lmax] (float, float, float x) { return std::exp (lmin + x * (lmax - lmin)); },
            [lmin, lmax] (float, float, float v) { return (std::log (std::max (v, 1.0e-6f)) - lmin) / (lmax - lmin); });
    }
    juce::NormalisableRange<float> r (s.min, s.max, s.interval);
    if (s.scale == FloatSpec::skewed) r.setSkewForCentre (s.min + (s.max - s.min) * 0.1f);
    return r;
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    const int v2 = 2;   // version 2: every loop has its own parameters (Rebanada 0.2)

    auto floatParam = [] (const juce::String& id, const juce::String& name, const FloatSpec& s, int version) {
        const auto sp = s;
        return std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { id, version }, name, rangeFor (s), s.def,
            juce::AudioParameterFloatAttributes()
                .withLabel (s.unit)
                .withStringFromValueFunction ([sp] (float v, int) { return formatValue (sp, v); })
                .withValueFromStringFunction ([sp] (const juce::String& t) { return parseValue (sp, t); }));
    };

    layout.add (std::make_unique<juce::AudioParameterBool>   (juce::ParameterID { "play", 1 },     "Loop play", true));
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "switchAt", 1 }, "Switch at", switchChoices(), 2));
    layout.add (floatParam ("tempo", "Tempo", spec ("tempo"), 1));
    layout.add (floatParam ("gain", "Output", spec ("gain"), 1));

    for (int l = 0; l < maxSlots; ++l)
    {
        const juce::String prefix = "L" + juce::String (l + 1) + " ";
        auto group = std::make_unique<juce::AudioProcessorParameterGroup> ("loop" + juce::String (l + 1), "Loop " + juce::String (l + 1), "|");
        group->addChild (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { loopId (l, "mode"), v2 }, prefix + "Mode", modeChoices(), 0));
        group->addChild (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { loopId (l, "order"), v2 }, prefix + "Order", orderChoices(), 0));
        group->addChild (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { loopId (l, "filterType"), v2 }, prefix + "Filter type", filterChoices(), 0));
        group->addChild (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { loopId (l, "lfoDest"), v2 }, prefix + "LFO target", lfoDestChoices(), 0));
        for (const auto& s : floatSpecs())
        {
            if (isGlobal (s.id)) continue;
            group->addChild (floatParam (loopId (l, s.id), prefix + u8 (s.en), s, v2));
        }
        layout.add (std::move (group));
    }
    return layout;
}

//==============================================================================
// Interface texts that are not parameter names.
inline juce::String tr (Lang lang, const char* key)
{
    struct Entry { const char* key; const char* en; const char* es; };
    static const Entry table[] {
        { "subtitle",    "slice sampler",                 "sampler de rebanadas" },
        { "loops",       "Loops",                         "Loops" },
        { "play",        "Play",                          "Play" },
        { "sync",        "Sync",                          "Sync" },
        { "free",        "Free",                          "Libre" },
        { "switchAt",    "Switch at",                     "Cambio" },
        { "instant",     "Instant",                       "Inmediato" },
        { "beat",        "1 beat",                        "1 pulso" },
        { "bar",         "1 bar",                         "1 compás" },
        { "loopEnd",     "Loop end",                      "Fin del loop" },
        { "bpmHost",     "BPM · host",                    "BPM · anfitrión" },
        { "bpmOwn",      "BPM · internal",                "BPM · interno" },
        { "rateFree",    "rate · free",                   "velocidad · libre" },
        { "tempoTip",    "Drag up or down to change. Double-click: default", "Arrastrá hacia arriba o abajo para cambiar. Doble clic: valor inicial" },
        { "slot",        "Slot",                          "Loop" },
        { "empty",       "Empty — drop an audio file here or click to load", "Vacío — soltá un archivo de audio acá o hacé clic para cargar" },
        { "missing",     "File not found",                "No encuentro el archivo" },
        { "load",        "Load…",                         "Cargar…" },
        { "clear",       "Clear slot",                    "Vaciar loop" },
        { "bars",        "bars",                          "compases" },
        { "bar1",        "bar",                           "compás" },
        { "slices",      "slices",                        "rebanadas" },
        { "origBpm",     "BPM orig.",                     "BPM orig." },
        { "detect",      "Detect",                        "Detectar" },
        { "reslice",     "Re-slice",                      "Rebanar" },
        { "split",       "Split",                         "Dividir" },
        { "merge",       "Merge",                         "Unir" },
        { "snap",        "Snap: zero-cross",              "Ajuste: cruce por cero" },
        { "grid",        "Grid 1/16",                     "Grilla 1/16" },
        { "dragMidi",    "Drag MIDI to track",            "Arrastrá el MIDI a la pista" },
        { "dragMidiTip", "Drag this onto a track: one note per slice, in the loop's order", "Arrastralo a una pista: una nota por rebanada, en el orden del loop" },
        { "slice",       "Slice",                         "Rebanada" },
        { "reverse",     "Reverse",                       "Reversa" },
        { "mute",        "Mute",                          "Silenciar" },
        { "out",         "Out",                           "Salida" },
        { "voice",       "Amp envelope · filter",         "Envolvente · filtro" },
        { "motion",      "Playback order · motion",       "Orden · movimiento" },
        { "forward",     "Forward",                       "En orden" },
        { "random",      "Random",                        "Al azar" },
        { "weighted",    "Weighted",                      "Ponderado" },
        { "lfoTo",       "LFO",                           "LFO" },
        { "cutoffDest",  "Cutoff",                        "Corte" },
        { "pitchDest",   "Pitch",                         "Altura" },
        { "panDest",     "Pan",                           "Paneo" },
        { "levelDest",   "Level",                         "Nivel" },
        { "keys",        "Keys → slices",                 "Teclas → rebanadas" },
        { "base",        "Base",                          "Base" },
        { "loopNotes",   "C0–G0 choose the loop",         "Do0–Sol0 eligen el loop" },
        { "open",        "Open",                          "Abierto" },
        { "full",        "Full",                          "Entera" },
        { "dblclick",    "Double-click: default value",   "Doble clic: valor inicial" },
        { "waveTip",     "Click: choose a slice · Drag a marker to move it · Double-click: new marker · Right-click a marker: delete · Wheel or pinch: zoom · Shift+wheel or the bar below: move", "Clic: elegir rebanada · Arrastrá un marcador para moverlo · Doble clic: marcador nuevo · Clic derecho en un marcador: borrarlo · Rueda o pellizco: zoom · Mayúsculas+rueda o la barra de abajo: desplazarse" },
        { "slotTip",     "Click: choose the loop (it changes at the point set in «Switch at») · Right-click: load or clear", "Clic: elegir el loop (cambia en el punto elegido en «Cambio») · Clic derecho: cargar o vaciar" },
        { "keysTip",     "Click a key to hear its slice", "Hacé clic en una tecla para escuchar su rebanada" },
        { "playTip",     "Plays the loop while the host plays (always, in the standalone app)", "Toca el loop mientras el anfitrión reproduce (siempre, en la aplicación independiente)" },
        { "outputs",     "outputs",                       "salidas" },
        { "outputOne",   "output",                        "salida" },
        { "outputLevel", "out",                           "salida" },
        { "channelsShort", "ch",                          "can." },
        { "outputTip",   "Output level of all the loops. Drag up or down. Double-click: 0 dB", "Nivel de salida de todos los loops. Arrastrá hacia arriba o abajo. Doble clic: 0 dB" },
        { "monoShort",   "M",                             "M" },
        { "stereoShort", "ST",                            "EST" },
        { "stereoTip",   "M: the slice goes to one channel. ST: to a pair of channels, with its pan", "M: la rebanada sale por un canal. EST: por un par de canales, con su paneo" },
        { "outTip",      "Output channel. Give the track more channels in your DAW (up to 64) to use them", "Canal de salida. Para usar más, dale más canales a la pista en tu DAW (hasta 64)" },
        { "detectTip",   "Higher: more and softer attacks become slices", "Más alto: más ataques, y más suaves, se vuelven rebanadas" },
        { "barsTip",     "Loop length in bars, for sync mode", "Duración del loop en compases, para el modo sync" },
        { "openFile",    "Choose an audio file",          "Elegí un archivo de audio" },
    };
    for (const auto& e : table)
        if (std::strcmp (e.key, key) == 0) return u8 (lang == Lang::es ? e.es : e.en);
    return key;
}

// Value texts in the interface's language.
inline juce::String localValue (juce::String t, Lang lang)
{
    if (lang == Lang::es)
    {
        if (t == "Open") return tr (lang, "open");
        if (t == "Full") return tr (lang, "full");
        if (t.length() > 1 && juce::CharacterFunctions::isDigit (t[1]))
        {
            if (t.startsWith ("L")) return "I" + t.substring (1);
            if (t.startsWith ("R")) return "D" + t.substring (1);
        }
    }
    else
    {
        if (t == "Open") return tr (lang, "open");
        if (t == "Full") return tr (lang, "full");
    }
    return localNumber (t, lang);
}

// Note names: English letters, or Spanish solfège.
inline juce::String noteName (int midi, Lang lang)
{
    static const char* en[] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    static const char* es[] { "Do", "Do#", "Re", "Re#", "Mi", "Fa", "Fa#", "Sol", "Sol#", "La", "La#", "Si" };
    const int octave = midi / 12 - 1;
    return u8 ((lang == Lang::es ? es : en)[midi % 12]) + juce::String (octave);
}

} // namespace rebanada
