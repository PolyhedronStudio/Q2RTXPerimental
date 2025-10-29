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
	//! A Temporary Entity Event. To get the value of the event, subtract ET_TEMP_ENTITY_EVENT from the entityType value.
	//ET_TEMP_ENTITY_EVENT,
	//! func_areaportal
	ET_AREA_PORTAL,
	//! spotlight
	ET_SPOTLIGHT,
	//! Beam Entity.
	ET_BEAM,

	//! Maximum engine types.
	ET_ENGINE_MAX_TYPES
} entity_type_t;