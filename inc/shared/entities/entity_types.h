/********************************************************************
*
*
*   Client/Server Entity Types:
*
*
********************************************************************/
#pragma once


/**
*	@description	Determines the actual entity type, in order to allow for appropriate state transmission
*					and efficient client-side handling. 
(					(ET_SPOTLIGHT demands different data than 'generic' entities, and so on..)
*/
typedef enum entity_type_e {
	//! Types specific to client/server that are too complex requiring the client/server to know about them.
	ET_ENGINE_TYPES = 0,
	//! Generic type, defaults for all entities unless specified otherwise.
	ET_GENERIC = ET_ENGINE_TYPES,

	//! func_areaportal
	ET_AREA_PORTAL,
	//! spotlight
	ET_SPOTLIGHT,
	//! Beam Entity.
	ET_BEAM,

	//! Types that are specific to the game modules only.
	ET_GAME_TYPES,

	//! Maximum types supported.
	ET_MAX_TYPES = 255
} entity_type_t;