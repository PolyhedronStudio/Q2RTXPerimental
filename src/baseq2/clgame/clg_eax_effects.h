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
