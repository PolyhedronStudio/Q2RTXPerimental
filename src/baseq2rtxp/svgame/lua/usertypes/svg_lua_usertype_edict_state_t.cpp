/********************************************************************
*
*
*	ServerGame: Lua GameLib API UserType.
*	NameSpace: "".
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/svg_lua.h"
#include "svgame/lua/svg_lua_gamelib.hpp"
#include "svgame/entities/svg_entities_pushermove.h"
#include "svgame/lua/usertypes/svg_lua_usertype_edict_state_t.hpp"

/**
*	Utility for checking if 'edict' pointer is valid.
**/
#define LUA_VALIDATE_EDICT_HANDLE() \
do { \
	if ( handle.edictPtr == nullptr ) { \
		Lua_DeveloperPrintf( "%s: lua_edict_state_t::handle.edictPtr == (nullptr) !\n", (__func__) ); \
		handle = { .number = -1 }; \
		return; \
	} else { \
		svg_base_edict_t *edictInSlot = g_edict_pool.EdictForNumber( handle.number ); \
		if ( !edictInSlot || handle.edictPtr != edictInSlot ) { \
			handle = { .number = -1 }; \
			return; \
		} \
		if ( &edictInSlot->s != &handle.edictPtr->s ) { \
			handle = { .number = -1 }; \
			return; \
		} \
		if ( edictInSlot->s.number != handle.number ) { \
			handle = { .number = -1 }; \
			return; \
		} \
		if ( edictInSlot->spawn_count != handle.spawnCount ) { \
			handle = { .number = -1 }; \
			return; \
		} \
	} \
} while (false)

#define LUA_VALIDATE_EDICT_HANDLE_RETVAL(returnValue) \
do { \
	if ( handle.edictPtr == nullptr ) { \
		Lua_DeveloperPrintf( "%s: lua_edict_state_t::handle.edictPtr == (nullptr) !\n", (__func__) ); \
		handle = { .number = -1 }; \
		return returnValue; \
	} else { \
		svg_base_edict_t *edictInSlot = g_edict_pool.EdictForNumber( handle.number ); \
		if ( !edictInSlot || handle.edictPtr != edictInSlot ) { \
			handle = { .number = -1 }; \
			return returnValue; \
		} \
		if ( &edictInSlot->s != &handle.edictPtr->s ) { \
			handle = { .number = -1 }; \
			return returnValue; \
		} \
		if ( edictInSlot->s.number != handle.number ) { \
			handle = { .number = -1 }; \
			return returnValue; \
		} \
		if ( edictInSlot->spawn_count != handle.spawnCount ) { \
			handle = { .number = -1 }; \
			return returnValue; \
		} \
	} \
} while (false)



/**
*
*	Constructors:
*
**/
lua_edict_state_t::lua_edict_state_t() : handle( {} ) {
	// Returns if invalid.
	LUA_VALIDATE_EDICT_HANDLE();
}
lua_edict_state_t::lua_edict_state_t( svg_base_edict_t *_edict ) {
	//! Setup pointer.
	handle.edictPtr = _edict;
	handle.number = _edict->s.number;
	handle.spawnCount = _edict->spawn_count;

	// Returns if invalid.
	LUA_VALIDATE_EDICT_HANDLE();
}



/**
*
*
*	Entity State Properties:
*
*
**/
//
//	(number):
//
/**
*	@brief
**/
const int32_t lua_edict_state_t::get_number( sol::this_state s ) {
	// Returns if invalid.
	LUA_VALIDATE_EDICT_HANDLE_RETVAL( -1 );
	// Return number.
	return handle.edictPtr->s.number;
}
/**
*	@brief
**/
//
// (effects):
//
/**
*	@return Get EntityEffects.
**/
const int32_t lua_edict_state_t::get_entity_entityFlags( sol::this_state s ) {
	// Returns if invalid.
	LUA_VALIDATE_EDICT_HANDLE_RETVAL( EF_NONE );
	// Return entity state effects.
	return handle.edictPtr->s.entityFlags;
}
/**
*	@return Set EntityEffects.
**/
void lua_edict_state_t::set_entity_effects( sol::this_state s, const int32_t effects ) {
	// Returns if invalid.
	LUA_VALIDATE_EDICT_HANDLE( );
	// Assign entity state effects.
	handle.edictPtr->s.entityFlags = effects;
}

//
// (renderfx):
//
/**
*	@return Get RenderFx.
**/
const int32_t lua_edict_state_t::get_renderfx( sol::this_state s ) {
	// Returns if invalid.
	LUA_VALIDATE_EDICT_HANDLE_RETVAL( RF_NONE );
	// Return entity state renderfx.
	return handle.edictPtr->s.renderfx;
}
/**
*	@brief Set RenderFx
**/
void lua_edict_state_t::set_renderfx( sol::this_state s, const int32_t renderFx ) {
	// Returns if invalid.
	LUA_VALIDATE_EDICT_HANDLE();
	// Assign entity state renderfx.
	handle.edictPtr->s.renderfx = renderFx;
}

//
//	(frame):
//
/**
*	@return Get (Brush-)Entity Frame.
**/
const int32_t lua_edict_state_t::get_frame( sol::this_state s ) {
	// Returns if invalid.
	LUA_VALIDATE_EDICT_HANDLE_RETVAL( 0 );
	// Return frame number.
	return handle.edictPtr->s.frame;
}
/**
*	@brief Set (Brush-)Entity Frame.
**/
void lua_edict_state_t::set_frame( sol::this_state s, const int32_t frame ) {
	// Returns if invalid.
	LUA_VALIDATE_EDICT_HANDLE();
	// Assign frame number.
	handle.edictPtr->s.frame = frame;
}

//
//	(modelindex),
//	(modelindex2)
//	(modelindex3)
//	(modelindex4):
//
/**
*	@return Get ModelIndex:
**/
const int32_t lua_edict_state_t::get_model_index( sol::this_state s ) {
	// Returns if invalid.
	LUA_VALIDATE_EDICT_HANDLE_RETVAL( -1 );
	// Return modelindex.
	return handle.edictPtr->s.modelindex;
}
const int32_t lua_edict_state_t::get_model_index2( sol::this_state s ) {
	// Returns if invalid.
	LUA_VALIDATE_EDICT_HANDLE_RETVAL( 0 );
	// Return modelindex.
	return handle.edictPtr->s.modelindex2;
}
const int32_t lua_edict_state_t::get_model_index3( sol::this_state s ) {
	// Returns if invalid.
	LUA_VALIDATE_EDICT_HANDLE_RETVAL( 0 );
	// Return modelindex.
	return handle.edictPtr->s.modelindex3;
}
const int32_t lua_edict_state_t::get_model_index4( sol::this_state s ) {
	// Returns if invalid.
	LUA_VALIDATE_EDICT_HANDLE_RETVAL( 0 );
	// Return modelindex.
	return handle.edictPtr->s.modelindex4;
}
/**
*	@brief Set ModelIndex.
**/
void lua_edict_state_t::set_model_index( sol::this_state s, const int32_t modelIndex ) {
	// Returns if invalid.
	LUA_VALIDATE_EDICT_HANDLE();
	// Set modelindex.
	handle.edictPtr->s.modelindex = modelIndex;
}
void lua_edict_state_t::set_model_index2( sol::this_state s, const int32_t modelIndex ) {
	// Returns if invalid.
	LUA_VALIDATE_EDICT_HANDLE();
	// Set modelindex.
	handle.edictPtr->s.modelindex2 = modelIndex;
}
void lua_edict_state_t::set_model_index3( sol::this_state s, const int32_t modelIndex ) {
	// Returns if invalid.
	LUA_VALIDATE_EDICT_HANDLE();
	// Set modelindex.
	handle.edictPtr->s.modelindex3 = modelIndex;
}
void lua_edict_state_t::set_model_index4( sol::this_state s, const int32_t modelIndex ) {
	// Returns if invalid.
	LUA_VALIDATE_EDICT_HANDLE();
	// Set modelindex.
	handle.edictPtr->s.modelindex4 = modelIndex;
}

/**
*	@return Get Sound.
**/
const int32_t lua_edict_state_t::get_sound( sol::this_state s ) {
	// Returns if invalid.
	LUA_VALIDATE_EDICT_HANDLE_RETVAL( 0 );
	// Return sound.
	return handle.edictPtr->s.sound;
}
/**
*	@brief Set Sound.
**/
void lua_edict_state_t::set_sound( sol::this_state s, const int32_t sound ) {
	// Returns if invalid.
	LUA_VALIDATE_EDICT_HANDLE();
	// Set sound.
	handle.edictPtr->s.sound = sound;
}
/**
*	@return Get Event.
**/
const int32_t lua_edict_state_t::get_event( sol::this_state s ) {
	// Returns if invalid.
	LUA_VALIDATE_EDICT_HANDLE_RETVAL( 0 );
	// Return event.
	return handle.edictPtr->s.event;
}
/**
*	@brief Set Event.
**/
void lua_edict_state_t::set_event( sol::this_state s, const int32_t event ) {
	// Returns if invalid.
	LUA_VALIDATE_EDICT_HANDLE();
	// Set event.
	handle.edictPtr->s.event = event;
}



/**
*	@brief	Register a usertype for passing along svg_base_edict_t into lua.
**/
void UserType_Register_EdictState_t( sol::state &solState ) {
	/**
	*	Register 'User Type':
	**/
	// We simply point to the entity that owns the state, the addresses won't change during the lifetime of the LUA VM.
	sol::usertype<lua_edict_state_t> lua_edict_state_type = solState.new_usertype<lua_edict_state_t>( "lua_edict_state_t",
		//sol::no_constructor,
		sol::constructors< lua_edict_state_t(), lua_edict_state_t( svg_base_edict_t * ) >()
	);

	// Register Lua 'Properties', so we got get and setters for each accessible entity_state_t member.
	lua_edict_state_type[ "number" ] = sol::property( &lua_edict_state_t::get_number/*, &lua_edict_state_t::set_number */);

	lua_edict_state_type[ "entityFlags" ] = sol::property( &lua_edict_state_t::get_entity_entityFlags, &lua_edict_state_t::set_entity_effects);
	lua_edict_state_type[ "renderFx" ] = sol::property( &lua_edict_state_t::get_renderfx, &lua_edict_state_t::set_renderfx );

	lua_edict_state_type[ "frame" ] = sol::property( &lua_edict_state_t::get_frame, &lua_edict_state_t::set_frame );
	lua_edict_state_type[ "modelIndex" ] = sol::property( &lua_edict_state_t::get_model_index, &lua_edict_state_t::set_model_index );
	lua_edict_state_type[ "modelIndex2" ] = sol::property( &lua_edict_state_t::get_model_index2, &lua_edict_state_t::set_model_index2 );
	lua_edict_state_type[ "modelIndex3" ] = sol::property( &lua_edict_state_t::get_model_index3, &lua_edict_state_t::set_model_index3 );
	lua_edict_state_type[ "modelIndex4" ] = sol::property( &lua_edict_state_t::get_model_index4, &lua_edict_state_t::set_model_index4 );

	lua_edict_state_type[ "sound" ] = sol::property( &lua_edict_state_t::get_sound, &lua_edict_state_t::set_sound );
	lua_edict_state_type[ "event" ] = sol::property( &lua_edict_state_t::get_event, &lua_edict_state_t::set_event );
}