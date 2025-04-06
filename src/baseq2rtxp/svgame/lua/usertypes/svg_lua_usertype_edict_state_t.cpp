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
#define LUA_VALIDATE_EDICT_POINTER() \
do { \
	if ( edict == nullptr ) { \
		Lua_DeveloperPrintf( "%s: lua_edict_state_t::edict == (nullptr) !\n", (__FUNCTION__) ); \
		return; \
	} \
} while (false)

#define LUA_VALIDATE_EDICT_POINTER_RETVAL(returnValue) \
do { \
	if ( edict == nullptr ) { \
		Lua_DeveloperPrintf( "%s: lua_edict_state_t::edict == (nullptr) !\n", (__FUNCTION__) ); \
		return returnValue; \
	} \
} while (false)



/**
*
*	Constructors:
*
**/
lua_edict_state_t::lua_edict_state_t() : edict( nullptr ) {
	// Returns if invalid.
	LUA_VALIDATE_EDICT_POINTER();
}
lua_edict_state_t::lua_edict_state_t( edict_t *_edict ) : edict( _edict ) {
	// Returns if invalid.
	LUA_VALIDATE_EDICT_POINTER();
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
const int32_t lua_edict_state_t::get_number( sol::this_state s ) const {
	// Returns if invalid.
	LUA_VALIDATE_EDICT_POINTER_RETVAL( -1 );
	// Return number.
	return this->edict->s.number;
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
const int32_t lua_edict_state_t::get_entity_effects( sol::this_state s ) const {
	// Returns if invalid.
	LUA_VALIDATE_EDICT_POINTER_RETVAL( EF_NONE );
	// Return entity state effects.
	return this->edict->s.effects;
}
/**
*	@return Set EntityEffects.
**/
void lua_edict_state_t::set_entity_effects( sol::this_state s, const int32_t effects ) {
	// Returns if invalid.
	LUA_VALIDATE_EDICT_POINTER( );
	// Assign entity state effects.
	this->edict->s.effects = effects;
}

//
// (renderfx):
//
/**
*	@return Get RenderFx.
**/
const int32_t lua_edict_state_t::get_renderfx( sol::this_state s ) const {
	// Returns if invalid.
	LUA_VALIDATE_EDICT_POINTER_RETVAL( RF_NONE );
	// Return entity state renderfx.
	return this->edict->s.renderfx;
}
/**
*	@brief Set RenderFx
**/
void lua_edict_state_t::set_renderfx( sol::this_state s, const int32_t renderFx ) {
	// Returns if invalid.
	LUA_VALIDATE_EDICT_POINTER();
	// Assign entity state renderfx.
	this->edict->s.renderfx = renderFx;
}

//
//	(frame):
//
/**
*	@return Get (Brush-)Entity Frame.
**/
const int32_t lua_edict_state_t::get_frame( sol::this_state s ) const {
	// Returns if invalid.
	LUA_VALIDATE_EDICT_POINTER_RETVAL( 0 );
	// Return frame number.
	return this->edict->s.frame;
}
/**
*	@brief Set (Brush-)Entity Frame.
**/
void lua_edict_state_t::set_frame( sol::this_state s, const int32_t frame ) {
	// Returns if invalid.
	LUA_VALIDATE_EDICT_POINTER();
	// Assign frame number.
	this->edict->s.frame = frame;
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
const int32_t lua_edict_state_t::get_model_index( sol::this_state s ) const {
	// Returns if invalid.
	LUA_VALIDATE_EDICT_POINTER_RETVAL( -1 );
	// Return modelindex.
	return this->edict->s.modelindex;
}
const int32_t lua_edict_state_t::get_model_index2( sol::this_state s ) const {
	// Returns if invalid.
	LUA_VALIDATE_EDICT_POINTER_RETVAL( 0 );
	// Return modelindex.
	return this->edict->s.modelindex2;
}
const int32_t lua_edict_state_t::get_model_index3( sol::this_state s ) const {
	// Returns if invalid.
	LUA_VALIDATE_EDICT_POINTER_RETVAL( 0 );
	// Return modelindex.
	return this->edict->s.modelindex3;
}
const int32_t lua_edict_state_t::get_model_index4( sol::this_state s ) const {
	// Returns if invalid.
	LUA_VALIDATE_EDICT_POINTER_RETVAL( 0 );
	// Return modelindex.
	return this->edict->s.modelindex4;
}
/**
*	@brief Set ModelIndex.
**/
void lua_edict_state_t::set_model_index( sol::this_state s, const int32_t modelIndex ) {
	// Returns if invalid.
	LUA_VALIDATE_EDICT_POINTER();
	// Set modelindex.
	this->edict->s.modelindex = modelIndex;
}
void lua_edict_state_t::set_model_index2( sol::this_state s, const int32_t modelIndex ) {
	// Returns if invalid.
	LUA_VALIDATE_EDICT_POINTER();
	// Set modelindex.
	this->edict->s.modelindex2 = modelIndex;
}
void lua_edict_state_t::set_model_index3( sol::this_state s, const int32_t modelIndex ) {
	// Returns if invalid.
	LUA_VALIDATE_EDICT_POINTER();
	// Set modelindex.
	this->edict->s.modelindex3 = modelIndex;
}
void lua_edict_state_t::set_model_index4( sol::this_state s, const int32_t modelIndex ) {
	// Returns if invalid.
	LUA_VALIDATE_EDICT_POINTER();
	// Set modelindex.
	this->edict->s.modelindex4 = modelIndex;
}

/**
*	@return Get Sound.
**/
const int32_t lua_edict_state_t::get_sound( sol::this_state s ) const {
	// Returns if invalid.
	LUA_VALIDATE_EDICT_POINTER_RETVAL( 0 );
	// Return sound.
	return this->edict->s.sound;
}
/**
*	@brief Set Sound.
**/
void lua_edict_state_t::set_sound( sol::this_state s, const int32_t sound ) {
	// Returns if invalid.
	LUA_VALIDATE_EDICT_POINTER();
	// Set sound.
	this->edict->s.sound = sound;
}
/**
*	@return Get Event.
**/
const int32_t lua_edict_state_t::get_event( sol::this_state s ) const {
	// Returns if invalid.
	LUA_VALIDATE_EDICT_POINTER_RETVAL( 0 );
	// Return event.
	return this->edict->s.event;
}
/**
*	@brief Set Event.
**/
void lua_edict_state_t::set_event( sol::this_state s, const int32_t event ) {
	// Returns if invalid.
	LUA_VALIDATE_EDICT_POINTER();
	// Set event.
	this->edict->s.event = event;
}



/**
*	@brief	Register a usertype for passing along edict_t into lua.
**/
void UserType_Register_EdictState_t( sol::state &solState ) {
	/**
	*	Register 'User Type':
	**/
	// We simply point to the entity that owns the state, the addresses won't change during the lifetime of the LUA VM.
	sol::usertype<lua_edict_state_t> lua_edict_state_type = solState.new_usertype<lua_edict_state_t>( "lua_edict_state_t",
		//sol::no_constructor,
		sol::constructors< lua_edict_state_t(), lua_edict_state_t( edict_t * ) >()
	);

	// Register Lua 'Properties', so we got get and setters for each accessible entity_state_t member.
	lua_edict_state_type[ "number" ] = sol::property( &lua_edict_state_t::get_number/*, &lua_edict_state_t::set_number */);

	lua_edict_state_type[ "effects" ] = sol::property( &lua_edict_state_t::get_entity_effects, &lua_edict_state_t::set_entity_effects);
	lua_edict_state_type[ "renderFx" ] = sol::property( &lua_edict_state_t::get_renderfx, &lua_edict_state_t::set_renderfx );

	lua_edict_state_type[ "frame" ] = sol::property( &lua_edict_state_t::get_frame, &lua_edict_state_t::set_frame );
	lua_edict_state_type[ "modelIndex" ] = sol::property( &lua_edict_state_t::get_model_index, &lua_edict_state_t::set_model_index );
	lua_edict_state_type[ "modelIndex2" ] = sol::property( &lua_edict_state_t::get_model_index2, &lua_edict_state_t::set_model_index2 );
	lua_edict_state_type[ "modelIndex3" ] = sol::property( &lua_edict_state_t::get_model_index3, &lua_edict_state_t::set_model_index3 );
	lua_edict_state_type[ "modelIndex4" ] = sol::property( &lua_edict_state_t::get_model_index4, &lua_edict_state_t::set_model_index4 );

	lua_edict_state_type[ "sound" ] = sol::property( &lua_edict_state_t::get_sound, &lua_edict_state_t::set_sound );
	lua_edict_state_type[ "event" ] = sol::property( &lua_edict_state_t::get_event, &lua_edict_state_t::set_event );
}