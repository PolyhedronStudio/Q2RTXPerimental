/********************************************************************
*
*
*   Entity State:
*
*
********************************************************************/
#pragma once

// entity_state_t is the information conveyed from the server
// in an update message about entities that the client will
// need to render in some way
typedef struct entity_state_s {
    //! Server edict index.
    int32_t	number;

    //! The specific type of entity.
    int32_t	entityType; // ET_GENERIC, ET_PLAYER, ET_MONSTER_PLAYER, ET_SPOTLIGHT etc..

    //! Entity origin.
    vec3_t  origin;
    //! Entity 'view' angles.
    vec3_t  angles;
    //! For lerping.
    vec3_t  old_origin;
    /**
    *   @brief  The following fields are for client side prediction: (solid,clipmask,hullContents,ownerNumber) 
    *           gi.linkentity sets these properly.
    **/
    //! The actual 'solid' type of entity.
    solid_t solid;
    //! Clipmask for collision.
    contents_t clipmask;
    //! The actual temporary hull's leaf and brush contents of this entity in case it is a SOLID_BBOX.
    contents_t hullContents;
    //! Entity who owns this entity.
    int32_t ownerNumber;

    //! Main entity model.
    int32_t	modelindex;
    //! Used for weapons, CTF flags, etc
    int32_t	modelindex2, modelindex3, modelindex4;
    //! Skinnumber, in case of clients, packs model index and weapon model.
    int32_t	skinnum;
    //! Render Effect Flags: RF_NOSHADOW etc.
    int32_t	renderfx;
    //! General Effect Flags: EF_ROTATE etc.
    uint32_t effects;

    //! Current animation frame.
    int32_t	frame;
    //! [Paril-KEX] for custom interpolation stuff
    int32_t old_frame;   //! Old animation frame.

    int32_t	sound;  //! For looping sounds, to guarantee shutoff
    int32_t	event;  //! Impulse events -- muzzle flashes, footsteps, etc.
    //! Events only go out for a single frame, and they are automatically cleared after that.

//
//! Spotlights
//
//! RGB Color of spotlight.
    vec3_t rgb;
    //! Intensity of the spotlight.
    float intensity;
    //! Angle width.
    float angle_width;
    //! Angle falloff.
    float angle_falloff;
} entity_state_t;