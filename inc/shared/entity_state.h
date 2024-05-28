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
    /**
    *   Core Entity State Data:
    **/
    //! Server edict index.
    int32_t	number;
    //! The entity's clientinfo index.
    //uint8_t client;
    //! The specific type of entity.
    int32_t	entityType; // ET_GENERIC, ET_PLAYER, ET_MONSTER_PLAYER, ET_SPOTLIGHT etc..
    //! Entity origin.
    vec3_t  origin;
    //! Entity 'view' angles.
    vec3_t  angles;
    //! For lerping. Also used for RF_BEAM its termination point.
    vec3_t  old_origin;

    /**
    *   The following fields are communicated to the client for prediction needs,
    *   these are set properly using LinkEntity.
    **/
    //! The actual 'solid' type of entity.
    solid_t solid;
    //! The actual 'bounding box' mins/maxs for the solid type in question.
    uint32_t bounds;
    //! Clipmask for collision.
    contents_t clipmask;
    //! The actual temporary hull's leaf and brush contents of this entity in case it is a SOLID_BOUNDS_BOX.
    contents_t hullContents;
    //! Entity who owns this entity.
    int32_t ownerNumber;

    /**
    *   Model/Effects:
    **/
    //! Main entity model.
    int32_t	modelindex;
    //! Used for weapons, CTF flags, etc
    int32_t	modelindex2, modelindex3, modelindex4;
    //! Skinnumber, in case of clients, packs player (client)number index and weapon model index.
    int32_t	skinnum;
    //! Render Effect Flags: RF_NOSHADOW etc.
    int32_t	renderfx;
    //! General Effect Flags: EF_ROTATE etc.
    uint32_t effects;

    /**
    *   Animation:
    **/
    //! Current animation frame.
    int32_t	frame;
    //! [Paril-KEX] for custom interpolation stuff
    int32_t old_frame;   //! Old animation frame.

    /**
    *   Sound, Event(s) & Misc:
    **/
    int32_t	sound;  //! For looping sounds, to guarantee shutoff
    int32_t	event;  //! Impulse events -- muzzle flashes, footsteps, etc.
                    //! Events only go out for a single frame, and they are automatically cleared after that.

    /**
    *   ET Specifics:
    **/
    union {
        //! Spotlight:
        struct {
            //! RGB Color of spotlight.
            vec3_t rgb;
            //! Intensity of the spotlight.
            float intensity;
            //! Angle width.
            float angle_width;
            //! Angle falloff.
            float angle_falloff;
        } spotlight;
    };
} entity_state_t;