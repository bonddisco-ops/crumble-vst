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

    // Wet filter
    inline constexpr const char* fltOn     = "flt_on";
    inline constexpr const char* fltTarget = "flt_target";
    inline constexpr const char* lowCut    = "lowcut";
    inline constexpr const char* highCut   = "highcut";

    // Color
    inline constexpr const char* colOn     = "col_on";
    inline constexpr const char* colTarget = "colortarget";
    inline constexpr const char* driveType = "drive_type";
    inline constexpr const char* drive     = "drive";
    inline constexpr const char* tone      = "tone";
    inline constexpr const char* squash    = "squash";

    // Tremolo
    inline constexpr const char* trOn     = "tr_on";
    inline constexpr const char* trTarget = "tr_target";
    inline constexpr const char* trShape  = "tr_shape";
    inline constexpr const char* trDepth  = "tr_depth";
    inline constexpr const char* trRate   = "tr_rate";
    inline constexpr const char* trPan    = "tr_pan";

    // Follow
    inline constexpr const char* folOn     = "fol_on";
    inline constexpr const char* folThresh = "fol_thresh";
    inline constexpr const char* folMode   = "fol_mode";

    // Clip
    inline constexpr const char* clipOn   = "clip_on";
    inline constexpr const char* clipMode = "clip_mode";
    inline constexpr const char* ceiling  = "ceiling";
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
    inline const juce::StringArray driveTypes { "Soft", "Tape", "Tube", "Fuzz", "Hard", "Fold" };
    inline const juce::StringArray trShapes   { "Sine", "Triangle", "Square", "Ramp Up", "Ramp Down", "Sidechain" };
    inline const juce::StringArray folModes   { "Above", "Below" };
    inline const juce::StringArray clipModes  { "Soft", "Hard" };
}
