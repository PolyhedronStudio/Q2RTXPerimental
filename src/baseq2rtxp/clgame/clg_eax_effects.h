/********************************************************************
*
*
*	ClientGame: EAX Environment Effects Types.
* 
*   NOTE: Intended to be included only by clg_precache.cpp
*
*
********************************************************************/
#pragma once

//{
//density: 1.000000,
//diffusion : 0.690000,
//gain : 0.316200,
//gain_hf : 0.794300,
//gain_lf : 0.891300,
//decay_time : 3.280000,
//decay_hf_ratio : 1.170000,
//decay_lf_ratio : 0.910000,
//reflections_gain : 0.446700,
//reflections_delay : 0.044000,
//reflections_pan : [0 0 0] ,
//late_reverb_gain : 0.281800,
//late_reverb_delay : 0.024000,
//late_reverb_pan : [0 0 0] ,
//echo_time : 0.250000,
//echo_depth : 0.200000,
//modulation_time : 0.250000,
//modulation_depth : 0.000000,
//air_absorbtion_hf : 0.996600,
//hf_reference : 5000.000000,
//lf_reference : 250.000000,
//room_rolloff_factor : 0.000000,
//decay_limit : 1
//}
/**
*   "DEFAULT" reverb effect type properties:
**/
static sfx_eax_properties_t cl_eax_default_properties = {
    .flDensity = 1.0,
    .flDiffusion = 1.0,

    .flGain = 0.f,
    .flGainHF = 1.0f,
    .flGainLF = 1.0f,

    .flDecayTime = 1.0f,
    .flDecayHFRatio = 1.0f,
    .flDecayLFRatio = 1.0f,

    .flReflectionsGain = 0.f,
    .flReflectionsDelay = 0.f,
    .flReflectionsPan = { 0.f, 0.f, 0.f },

    .flLateReverbGain = 1.f,
    .flLateReverbDelay = 0.f,
    .flLateReverbPan = { 0.f, 0.f, 0.f },

    .flEchoTime = 0.25f,
    .flEchoDepth = 0.f,

    .flModulationTime = 0.25f,
    .flModulationDepth = 0.f,

    .flAirAbsorptionGainHF = 1.f,
    .flHFReference = 5000.0f,
    .flLFReference = 250.f,

    .flRoomRolloffFactor = 0.f,

    // decay_limit
    .iDecayHFLimit = 1,
};
/**
*   "UNDERWATER" reverb effect type properties:
**/
static sfx_eax_properties_t cl_eax_underwater_properties = {
    .flDensity = 0.3645,
    .flDiffusion = 1.0,

    .flGain = 0.316200f,
    .flGainHF = 0.01f,
    .flGainLF = 1.0f,

    .flDecayTime = 1.49f,
    .flDecayHFRatio = 0.1f,
    .flDecayLFRatio = 1.0f,

    .flReflectionsGain = 0.5963f,
    .flReflectionsDelay = 0.007f,
    .flReflectionsPan = { 0.f, 0.f, 0.f },

    .flLateReverbGain = 7.0795f,
    .flLateReverbDelay = 0.011f,
    .flLateReverbPan = { 0.f, 0.f, 0.f },

    .flEchoTime = 0.25f,
    .flEchoDepth = 0.f,

    .flModulationTime = 1.18f,
    .flModulationDepth = 0.348f,

    .flAirAbsorptionGainHF = 0.9943f,
    .flHFReference = 5000.0f,
    .flLFReference = 250.f,

    .flRoomRolloffFactor = 0.f,

    // decay_limit
    .iDecayHFLimit = 1,
};
/**
*   "METAL_SMALL" reverb effect type properties:
**/
static sfx_eax_properties_t cl_eax_metal_s_properties = {
    .flDensity = 1.0,
    .flDiffusion = 0.7,

    .flGain = 0.316200f,
    .flGainHF = 0.4477f,
    .flGainLF = 1.0f,

    .flDecayTime = 1.51f,
    .flDecayHFRatio = 1.25f,
    .flDecayLFRatio = 1.14f,

    .flReflectionsGain = 0.8913f,
    .flReflectionsDelay = 0.02f,
    .flReflectionsPan = { 0.f, 0.f, 0.f },

    .flLateReverbGain = 1.4125f,
    .flLateReverbDelay = 0.03f,
    .flLateReverbPan = { 0.f, 0.f, 0.f },

    .flEchoTime = 0.179f,
    .flEchoDepth = 0.15f,

    .flModulationTime = 0.895f,
    .flModulationDepth = 0.19f,

    .flAirAbsorptionGainHF = 0.992f,
    .flHFReference = 5000.0f,
    .flLFReference = 250.f,

    .flRoomRolloffFactor = 0.f,

    // decay_limit
    .iDecayHFLimit = 0,
};
/**
*   "TUNNEL_SMALL" reverb effect type properties:
**/
static sfx_eax_properties_t cl_eax_tunnel_s_properties = {
    .flDensity = 1.0,
    .flDiffusion = 0.690,

    .flGain = 0.316200f,
    .flGainHF = 0.79430f,
    .flGainLF = 0.81930f,

    .flDecayTime = 3.28f,
    .flDecayHFRatio = 1.17f,
    .flDecayLFRatio = 0.91f,

    .flReflectionsGain = 0.4467f,
    .flReflectionsDelay = 0.4400f,
    .flReflectionsPan = { 0.f, 0.f, 0.f },

    .flLateReverbGain = 0.2818f,
    .flLateReverbDelay = 0.024f,
    .flLateReverbPan = { 0.f, 0.f, 0.f },

    .flEchoTime = 0.25f,
    .flEchoDepth = 0.20f,

    .flModulationTime = 0.25f,
    .flModulationDepth = 0.0f,

    .flAirAbsorptionGainHF = 0.9966f,
    .flHFReference = 5000.0f,
    .flLFReference = 250.f,

    .flRoomRolloffFactor = 0.f,

    // decay_limit
    .iDecayHFLimit = 1,
};
/**
*   "TUNNEL_SMALL" reverb effect type properties:
**/
static sfx_eax_properties_t cl_eax_tunnel_l_properties = {
    .flDensity = 1.0,
    .flDiffusion = 0.820,

    .flGain = 0.316200f,
    .flGainHF = 0.44670f,
    .flGainLF = 0.89130f,

    .flDecayTime = 3.57f,
    .flDecayHFRatio = 1.12f,
    .flDecayLFRatio = 0.91f,

    .flReflectionsGain = 0.3981f,
    .flReflectionsDelay = 0.0590f,
    .flReflectionsPan = { 0.f, 0.f, 0.f },

    .flLateReverbGain = 0.89130f,
    .flLateReverbDelay = 0.037f,
    .flLateReverbPan = { 0.f, 0.f, 0.f },

    .flEchoTime = 0.25f,
    .flEchoDepth = 0.14f,

    .flModulationTime = 0.25f,
    .flModulationDepth = 0.0f,

    .flAirAbsorptionGainHF = 0.992f,
    .flHFReference = 5000.0f,
    .flLFReference = 250.f,

    .flRoomRolloffFactor = 0.f,

    // decay_limit
    .iDecayHFLimit = 1,
};