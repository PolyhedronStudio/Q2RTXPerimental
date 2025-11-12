/********************************************************************
*
*
*   Entity State:
*
*
********************************************************************/
#pragma once


/**
*   @brief  Encodes client index, weapon index, and viewheight.
*           Used in entity_state_t skinnum field, whenever we're dealing with
*           a player entity.
**/
typedef union encoded_skinnum_s {
    //! Actual value.
    int32_t         skinnum;
    //! Union for easy access.
    struct {
        uint8_t     clientNumber;
        uint8_t     viewWeaponIndex;
        int8_t      viewHeight;
        uint8_t     reserved;
    };
} encoded_skinnum_t;


/**
*   @brief  entity_state_t is the information conveyed from the server
*           in an update message about entities that the client will
*           need to render in some way
**/
typedef struct entity_state_s {
    /**
    *   Core Entity State Data:
    **/
    //! Server edict index.
    int32_t	number;
    //! The specific type of entity.
    int32_t	entityType; // ET_GENERIC, ET_PLAYER, ET_MONSTER_PLAYER, ET_SPOTLIGHT etc..
    //! For entity events.
    int32_t otherEntityNumber;

    //! Entity origin.
    Vector3  origin;
    //! For lerping. Also used for RF_BEAM its termination point.
    Vector3  old_origin;
    //! Entity 'view' angles.
    Vector3  angles;

    /**
    *   The following fields are communicated to the client for prediction needs,
    *   these are set properly using LinkEntity.
    **/
    //! The actual 'solid' type of entity.
    cm_solid_t solid;
    //! The actual 'bounding box' mins/maxs for the solid type in question.
    uint32_t bounds;
    //! Clipmask for collision.
    cm_contents_t clipMask;
    //! The actual temporary hull's leaf and brush contents of this entity in case it is a SOLID_BOUNDS_BOX.
    cm_contents_t hullContents;
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
    int32_t skinnum;
    //! Render Effect Flags: RF_NOSHADOW etc.
    int32_t	renderfx;
    //! General Entity Flags: EF_ROTATE etc.
    uint32_t entityFlags;

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
	int32_t eventParm0; //! Even parameter.
	// <Q2RTXP>: TODO: For use with temporarily storing additional event data if needed.
    int32_t eventParm1; //! Even parameter.

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