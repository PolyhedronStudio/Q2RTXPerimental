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
*
*	Constructors:
*
**/
lua_edict_state_t::lua_edict_state_t() : edict( nullptr ) {
	// TODO: Warn? 

}
lua_edict_state_t::lua_edict_state_t( edict_t *_edict ) : edict( _edict ) {
	// TODO: Warn if invalid?
	//this->edict = _edict;
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
const int32_t lua_edict_state_t::get_number() const {
	return ( this->edict != nullptr ? this->edict->s.number : -1 );
}
/**
*	@brief
**/
void lua_edict_state_t::set_number( const int32_t number ) {
	// Do nothing.
	// TODO: Error out?
	return;
}
//
//	(frame):
//
/**
*	@return Get (Brush-)Entity Frame.
**/
const int32_t lua_edict_state_t::get_frame() const {
	return ( this->edict != nullptr ? this->edict->s.frame : 0 );
}
/**
*	@brief Set (Brush-)Entity Frame.
**/
void lua_edict_state_t::set_frame( const int32_t number ) {
	// Do nothing.
	if ( this->edict != nullptr ) {
		this->edict->s.frame = number;
	} else {
		// TODO: Error out?
		return;
	}
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
	lua_edict_state_type[ "number" ] = sol::property( &lua_edict_state_t::get_number, &lua_edict_state_t::set_number );
	lua_edict_state_type[ "frame" ] = sol::property( &lua_edict_state_t::get_frame, &lua_edict_state_t::set_frame );
}