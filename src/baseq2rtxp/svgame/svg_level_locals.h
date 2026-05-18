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
*	@brief	A level's intermission state descriptor.
*
*	@details	Level 'Intermission' state data stores related information to a state where
*				players are awaiting a specific match event or interaction.
*				
*				Whether this be the end of the level, or waiting for more players to join, etc.
*
*				They go into 'intermission' mode. Which is defined by disabling movement
*				and clients entering a special state where they can't interact with the world,
*				and are instead placed into a special camera view.
*
*				This is used for showing the level exit in SP mode, or for showing the final
*				end-game scores in MP modes.
**/
struct svg_level_intermission_state_t {
	//! The level change map string to execute once intermission completed.
	svg_level_qstring_t changemap = nullptr;

	//! The origin for the intermission camera.
	Vector3 cameraOrigin = QM_Vector3Zero();
	//! The angle for the intermission camera.
	Vector3 cameraAngles = QM_Vector3Zero();

	//! The exact frame number on which the intermission was engaged.
	//! When zero it means the intermission state is not currently engaged.
	int64_t	engagedFrameNumber = 0;
	//! Whether to exit the intermission.
	int64_t shouldExit = 0;
};


/**
*   @brief  This structure is cleared as each map is entered.
*           It is read/written to the level.sav file for savegames.
**/
struct svg_level_locals_t {
    //! Used for registering the fields which need save and restoring 
    //! of the session's level locals.
    static svg_save_descriptor_field_t saveDescriptorFields[];

	//! The current frame number, which is incremented for each server frame, and can be used for timing purposes.
    uint64_t frameNumber = 0;
	//! The current time in the level, which is used for various timing and scheduling purposes.
    QMTime time = 0_ms;

	//! The gravity value for the level, which is used for physics calculations.
    double gravity = 0.;

	//! A pointer into the initial entities in the map as loaded by the collision model, 
	//! this is used for spawning the entities during map initialization as it stores their
	//! key/value pairs and other important information.
    const cm_entity_t **cm_entities = nullptr;

	//! The descriptive name (Outer Base, etc).
	char mapNameDescriptor[ MAX_QPATH ] = {};
	//! The server map name (base1, etc).
    char mapname[ MAX_QPATH ] = {};
	//! Go here when fraglimit is hit.
    char nextmap[ MAX_QPATH ] = {};

    
	/**
	*	Level 'Intermission' state data stores related information to a state where
	*	players are awaiting a specific match event or interaction.
	*
	*	Whether this be the end of the level, or waiting for more players to join, etc.
	* 
	*	They go into 'intermission' mode. Which is defined by disabling movement 
	*	and clients entering a special state where they can't interact with the world, 
	*	and are instead placed into a special camera view. 
	*
	*	This is used for showing the level exit in SP mode, or for showing the final
	*	end-game scores in MP modes.
	*
	**/
	//! A level's intermission state descriptor.
	svg_level_intermission_state_t intermissionState = { };


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