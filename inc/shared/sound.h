/********************************************************************
*
*
*   Sound:
*
*
********************************************************************/
#pragma once


/**
*	Sound Channels:
*	
*	Channel 0 never willingly overrides
*	Other channels (1-7) allways override a playing sound on that channel
**/
#define CHAN_AUTO               0
#define CHAN_WEAPON             1
#define CHAN_VOICE              2
#define CHAN_ITEM               3
#define CHAN_BODY               4
//! Modifier flags:
#define CHAN_NO_PHS_ADD         8   //! Send to all clients, not just ones in PHS (ATTN 0 will also do this)
#define CHAN_RELIABLE           16  //! Send by reliable message, not datagram.


/**
*   Sound attenuation values:
*   WID: TODO: Implement support for the commented attenuation values.
**/
#define ATTN_NONE               0   //! Full volume the entire level.
//#define ATTN_DISTANT          0.5     //! Distant sound (most likely explosions).
#define ATTN_NORM               1   //! Players, Weapons, etc.
#define ATTN_IDLE               2   //! 'Stuff' around you.
//#define ATTN_IDLE             2.5     //! 'Stuff' around you.
#define ATTN_STATIC             3   //! Diminish very rapidly with distance.
//#define ATTN_STATIC           5       //! Diminish very rapidly with distance.
//#define ATTN_WHISPER          10      //! Must be very close to hear it.


/**
*   @brief  Special reverb effect settings, these can be defined by the client game code and
*           applied to the listener for special effects. (Underwater, hallways, etc.)
**/
typedef struct sfx_eax_properties_s {
    //! This structure is based on the OpenAL EAX Reverb effects properties.
    //struct {
        float flDensity;
        float flDiffusion;
        float flGain;
        float flGainHF;
        float flGainLF;
        float flDecayTime;
        float flDecayHFRatio;
        float flDecayLFRatio;
        float flReflectionsGain;
        float flReflectionsDelay;
        float flReflectionsPan[ 3 ];
        float flLateReverbGain;
        float flLateReverbDelay;
        float flLateReverbPan[ 3 ];
        float flEchoTime;
        float flEchoDepth;
        float flModulationTime;
        float flModulationDepth;
        float flAirAbsorptionGainHF;
        float flHFReference;
        float flLFReference;
        float flRoomRolloffFactor;
        int32_t iDecayHFLimit;
    //} properties;
} sfx_eax_properties_t;