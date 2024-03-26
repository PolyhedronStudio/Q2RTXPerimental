/********************************************************************
*
*
*   Entity Types:
*
*
********************************************************************/
#pragma once


/**
*	@description	Determines the actual entity type, in order to allow for appropriate state transmission
*					and efficient client-side handling. (ET_SPOTLIGHT demands different data than 'generic' entities.
*/
typedef enum {
	ET_ENGINE_TYPES = 0,
	ET_GENERIC = ET_ENGINE_TYPES,
	ET_SPOTLIGHT,

	ET_GAME_TYPES,

	// TODO: Game Types.
	//ET_PLAYER,
	//ET_CORPSE,
	//ET_BEAM,
	//ET_PORTALSURFACE,
	//ET_PUSH_TRIGGER,

	//ET_GIB,         // leave a trail
	//ET_BLASTER,     // redlight + trail
	//ET_ELECTRO_WEAK,
	//ET_ROCKET,      // redlight + trail
	//ET_GRENADE,
	//ET_PLASMA,

	//ET_SPRITE,

	//ET_ITEM,        // for simple items
	//ET_LASERBEAM,   // for continuous beams
	////ET_CURVELASERBEAM, // for curved beams

	//ET_PARTICLES,

	//ET_MONSTER_PLAYER,
	//ET_MONSTER_CORPSE,

	// eventual entities: types below this will get event treatment
	//ET_EVENT = EVENT_ENTITIES_START,
	//ET_SOUNDEVENT,

	//ET_TOTAL_TYPES, // current count
	MAX_ENTITY_TYPES = 128
} entity_type_t;