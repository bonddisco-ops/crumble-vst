#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

// Parameter IDs and value tables shared by the processor and the editor
namespace ParamIDs
{
    // Global
    inline constexpr const char* input    = "input";
    inline constexpr const char* output   = "output";
    inline constexpr const char* mix      = "mix";
    inline constexpr const char* autoGain = "autogain";
    inline constexpr const char* order    = "order";        // false: Crumble -> Bitcrush, true: Bitcrush -> Crumble
    inline constexpr const char* chain    = "chain";        // order of Color, Comp, Tremolo (see Tables::chainOrders)

    // Crumble
    inline constexpr const char* crOn     = "cr_on";
    inline constexpr const char* mode     = "mode";
    inline constexpr const char* freq     = "freq";
    inline constexpr const char* rate     = "rate";
    inline constexpr const char* texture  = "texture";
    inline constexpr const char* amount   = "amount";
    inline constexpr const char* stereo   = "stereo";
    inline constexpr const char* feedback = "feedback";

    // Bitcrush
    inline constexpr const char* bcOn     = "bc_on";
    inline constexpr const char* bcFreq   = "bc_freq";
    inline constexpr const char* bits     = "bc_bits";
    inline constexpr const char* quantize = "bc_quantize";
    inline constexpr const char* dither   = "bc_dither";
    inline constexpr const char* adcQ     = "bc_adcq";
    inline constexpr const char* dacQ     = "bc_dacq";

    // Color
    inline constexpr const char* colOn     = "col_on";
    inline constexpr const char* colTarget = "colortarget";
    inline constexpr const char* driveType = "drive_type";
    inline constexpr const char* drive     = "drive";
    inline constexpr const char* tone      = "tone";
    inline constexpr const char* colMix    = "col_mix";

    // Comp
    inline constexpr const char* compOn      = "comp_on";
    inline constexpr const char* compTarget  = "comp_target";
    inline constexpr const char* compMode    = "comp_mode";
    inline constexpr const char* ottDepth    = "ott_depth";
    inline constexpr const char* ottTime     = "ott_time";
    inline constexpr const char* ottUp       = "ott_up";
    inline constexpr const char* ottDown     = "ott_down";
    inline constexpr const char* glueThresh  = "glue_thresh";
    inline constexpr const char* glueRatio   = "glue_ratio";
    inline constexpr const char* glueAttack  = "glue_attack";
    inline constexpr const char* glueRelease = "glue_release";
    inline constexpr const char* compMix     = "comp_mix";

    // Tremolo
    inline constexpr const char* trOn       = "tr_on";
    inline constexpr const char* trTarget   = "tr_target";
    inline constexpr const char* trShape    = "tr_shape";
    inline constexpr const char* trDepth    = "tr_depth";
    inline constexpr const char* trRateMode = "tr_ratemode";
    inline constexpr const char* trRate     = "tr_rate";
    inline constexpr const char* trMs       = "tr_ms";
    inline constexpr const char* trPan      = "tr_pan";       // shown as "Stereo"

    // Follow
    inline constexpr const char* folOn     = "fol_on";
    inline constexpr const char* folThresh = "fol_thresh";
    inline constexpr const char* folMode   = "fol_mode";

    // Clip
    inline constexpr const char* clipOn   = "clip_on";
    inline constexpr const char* clipMode = "clip_mode";
    inline constexpr const char* ceiling  = "ceiling";

    // Filters: "pre_" or "post_" + one of these
    inline constexpr const char* fOn      = "on";
    inline constexpr const char* fTarget  = "target";
    inline constexpr const char* fLc      = "lc";
    inline constexpr const char* fHc      = "hc";
    inline constexpr const char* fLcSlope = "lcslope";
    inline constexpr const char* fHcSlope = "hcslope";
    inline constexpr const char* fPkFreq  = "pkfreq";
    inline constexpr const char* fPkGain  = "pkgain";
    inline constexpr const char* fPkQ     = "pkq";

    inline juce::String filterId (bool post, const char* name) { return juce::String (post ? "post_" : "pre_") + name; }
}

namespace Tables
{
    // Note values from slow to fast, with their length in beats
    inline const juce::StringArray noteNames { "1/1", "1/2", "1/4.", "1/4", "1/8.", "1/4T", "1/8", "1/16.", "1/8T",
                                               "1/16", "1/16T", "1/32", "1/32T", "1/64", "1/128", "1/256" };
    inline constexpr double noteBeats[] = { 4.0, 2.0, 1.5, 1.0, 0.75, 2.0 / 3.0, 0.5, 0.375, 1.0 / 3.0,
                                            0.25, 1.0 / 6.0, 0.125, 1.0 / 12.0, 0.0625, 0.03125, 0.015625 };

    inline const juce::StringArray modes      { "Sine", "Sine Sync", "Noise" };
    inline const juce::StringArray targets    { "Wet", "All" };
    inline const juce::StringArray fxTargets  { "FX", "All" };
    inline const juce::StringArray driveTypes { "Soft", "Tape", "Tube", "Fuzz", "Hard", "Fold" };
    inline const juce::StringArray compModes  { "OTT", "Glue" };
    inline const juce::StringArray glueRatios { "2:1", "4:1", "10:1" };
    inline constexpr float glueRatioValues[] = { 2.0f, 4.0f, 10.0f };
    inline const juce::StringArray trShapes   { "Sine", "Triangle", "Square", "Ramp Up", "Ramp Down", "Sidechain" };
    inline const juce::StringArray rateModes  { "Sync", "Free" };
    inline const juce::StringArray folModes   { "Above", "Below" };
    inline const juce::StringArray clipModes  { "Soft", "Hard" };
    inline const juce::StringArray slopes     { "12 dB/oct", "24 dB/oct", "48 dB/oct" };

    // Order of the three movable bottom blocks: 0 Color, 1 Comp, 2 Tremolo
    inline constexpr int chainOrders[6][3] = { { 0, 1, 2 }, { 0, 2, 1 }, { 1, 0, 2 }, { 1, 2, 0 }, { 2, 0, 1 }, { 2, 1, 0 } };
    inline const juce::StringArray chainNames { "Color > Comp > Tremolo", "Color > Tremolo > Comp", "Comp > Color > Tremolo",
                                                "Comp > Tremolo > Color", "Tremolo > Color > Comp", "Tremolo > Comp > Color" };
    inline int chainIndexFor (const int order[3])
    {
        for (int i = 0; i < 6; ++i)
            if (chainOrders[i][0] == order[0] && chainOrders[i][1] == order[1] && chainOrders[i][2] == order[2])
                return i;
        return 0;
    }
}
