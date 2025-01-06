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
	lua_edict_state_type[ "frame" ] = sol::property( &lua_edict_state_t::get_frame, &lua_edict_state_t::set_frame );

	lua_edict_state_type[ "modelIndex" ] = sol::property( &lua_edict_state_t::get_model_index, &lua_edict_state_t::set_model_index );
	lua_edict_state_type[ "modelIndex2" ] = sol::property( &lua_edict_state_t::get_model_index2, &lua_edict_state_t::set_model_index2 );
	lua_edict_state_type[ "modelIndex3" ] = sol::property( &lua_edict_state_t::get_model_index3, &lua_edict_state_t::set_model_index3 );
	lua_edict_state_type[ "modelIndex4" ] = sol::property( &lua_edict_state_t::get_model_index4, &lua_edict_state_t::set_model_index4 );
}