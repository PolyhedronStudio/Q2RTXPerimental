/********************************************************************
*
*
*	ServerGame: Level Locals Data
*	NameSpace: "".
*
*
********************************************************************/
#pragma once



/**
*   @brief  This structure is cleared as each map is entered.
*           It is read/written to the level.sav file for savegames.
**/
struct svg_level_locals_t {
    //! Used for registering the fields which need save and restoring 
    //! of the session's level locals.
    static svg_save_descriptor_field_t saveDescriptorFields[];

    uint64_t    frameNumber = 0;
    QMTime		time = 0_ms;

    double      gravity = 0.;

    const cm_entity_t **cm_entities = nullptr;

    char        level_name[ MAX_QPATH ] = {};  // the descriptive name (Outer Base, etc)
    char        mapname[ MAX_QPATH ] = {};     // the server name (base1, etc)
    char        nextmap[ MAX_QPATH ] = {};     // go here when fraglimit is hit

    
	/**
	*	Level 'Intermission' state data. Whenever players are awaiting something in a match,
	*	whether this me the end of the level, or waiting for players to join, etc.
	* 
	*	They go into 'intermission' mode, where they can't move around, and a special
	* 	camera view is used to show the level exit, or other players, etc.
	**/
	//! Time the intermission was started.
    int64_t         intermissionFrameNumber = 0;  
	//! The level change map string.
    svg_level_qstring_t changemap = nullptr;
	//! Whether to exit the intermission.
    int64_t     exitintermission = 0;
	//! The origin for the intermission camera.
    Vector3     intermission_origin = QM_Vector3Zero();
	//! The angle for the intermission camera.
    Vector3     intermission_angle = QM_Vector3Zero();


	/**
	*	Sighting entities that are being used for visibility checks.
	* 
	*	The client and sound entities are changed once per frame.
	* 
	*	The sight_entity is changed as needed.
	**/
	//! The client entity for sighting (for visibility checks).
	//! <Q2RTXP>: TODO: // changed once each frame for coop games
    svg_base_edict_t	*sight_client = nullptr;  
	//! Frame number for sight_client to avoid duplicates.
	int64_t sight_client_framenum = 0;

	//! The entity currently being sighted (for visibility checks).
    svg_base_edict_t	*sight_entity = nullptr;
	//! Frame number for sight_entity to avoid duplicates.
    int64_t sight_entity_framenum = 0;
	
	/**
	*	The entities that are making a sound (for sound sighting).
	*	There are 3 types of sound:
	*           - PNOISE_PERSONAL: An entity indicating a client's own personal noise (jumping, pain, weapon firing)
	*			- PNOISE_WEAPONL weapon target noise (weapon firing)
	*           - PLAYER_NOISE_IMPACT: A target noise (bullet wall impacts)
	**/
	//! The weapon sound entity (for sound sighting).
    svg_base_edict_t     *weapon_sound_entity = nullptr;
	QMTime weapon_sound_entity_time = 0_ms; //! To avoid duplicates.
	//! The impact sound entity (for sound sighting).
    svg_base_edict_t     *impact_sound_entity = nullptr;
    QMTime impact_sound_entity_time = 0_ms; //! To avoid duplicates.
	//! The personal noise sound entity (for sound sighting).
	svg_base_edict_t     *personal_sound_entity = nullptr;
	QMTime personal_sound_entity_time = 0_ms; //! To avoid duplicates.

	/**
	*	The entity currently being processed.
	**/
    svg_base_edict_t *processingEntity = nullptr;   // entity running from SVG_RunFrame

	//! Stack of entities currently being used for dead player bodies.
    int32_t     body_que = 0;
};

//! Extern it.
extern svg_level_locals_t   level;