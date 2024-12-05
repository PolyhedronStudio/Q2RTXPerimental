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



/**
*
*	Constructors:
*
**/
lua_edict_t::lua_edict_t() : edict(nullptr) {
	// TODO: Warn? 
	
}
lua_edict_t::lua_edict_t( edict_t *_edict ) : edict(_edict) {
	// TODO: Warn if invalid?
	//this->edict = _edict;
}



/**
*
*	property(number):
*
**/
/**
*	@brief
**/
const int32_t lua_edict_t::get_number() const {
	return ( this->edict != nullptr ? this->edict->s.number : -1 );
}
/**
*	@brief
**/
void lua_edict_t::set_number( const int32_t number ) const {
	// Do nothing.
	// TODO: Error out?
	return;
}

/**
*	@brief	Register a usertype for passing along edict_t into lua.
**/
void UserType_Register_Edict_t( sol::state &solState ) {
	/**
	*	Register 'User Types':
	**/
	sol::usertype<lua_edict_t> lua_edict_type = solState.new_usertype<lua_edict_t>( "lua_edict_t",
		//sol::no_constructor,
		sol::constructors< lua_edict_t(), lua_edict_t( edict_t* ) >()
	);

	/**
	*	Variables:
	**/
	// Read Only:
	//lua_edict_type.set( "number", sol::readonly( &lua_edict_t::edict->s.number ) );
	lua_edict_type[ "number" ] = sol::property( &lua_edict_t::get_number, &lua_edict_t::get_number );

	// Read/Write:

}