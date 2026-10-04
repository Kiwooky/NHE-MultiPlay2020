#ifndef DISTRHO_PLUGIN_INFO_H_INCLUDED
#define DISTRHO_PLUGIN_INFO_H_INCLUDED

#define DISTRHO_PLUGIN_BRAND       "New Horizon"
#define DISTRHO_PLUGIN_NAME        "MultiPlay 20/20"
#define DISTRHO_PLUGIN_URI         "https://github.com/Kiwooky/NHE-MultiPlay2020"

#define DISTRHO_PLUGIN_HAS_UI       0
#define DISTRHO_PLUGIN_IS_RT_SAFE   1
#define DISTRHO_PLUGIN_NUM_INPUTS   1
#define DISTRHO_PLUGIN_NUM_OUTPUTS  2

enum Parameters {
    kDelayTime = 0,
    kWidth,
    kSpeed,
    kRegen,
    kMix,
    kRange,
    kSlamLevel,
    kTimeMod,
    kHold,
    kSlam,
    kRampTime,
    kBypass,
    kParameterCount
};

#endif
