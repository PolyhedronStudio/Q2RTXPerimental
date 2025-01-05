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
lua_edict_t::lua_edict_t() : edict(nullptr) {
	// TODO: Warn? 
	
}
lua_edict_t::lua_edict_t( edict_t *_edict ) : edict(_edict) {
	// TODO: Warn if invalid?
	//this->edict = _edict;
}


/**
*
*
*	Entity Properties:
*
*
**/
//
//	(number):
//
/**
*	@brief
**/
const int32_t lua_edict_t::get_number() const {
	return ( this->edict != nullptr ? this->edict->s.number : -1 );
}
/**
*	@brief
**/
void lua_edict_t::set_number( const int32_t number ) {
	// Error notify.
	LUA_ErrorPrintf( "%s: Can't change an entities number!\n", __func__ );
}

/**
*	@return	Returns a lua userdata object for accessing the entity's entity_state_t.
**/
sol::object lua_edict_t::get_state( sol::this_state s ) {
	sol::state_view solState( s );

	return sol::make_object_userdata<lua_edict_state_t>(solState, this->edict);
}

/**
*	@brief	Register a usertype for passing along edict_t into lua.
**/
void UserType_Register_Edict_t( sol::state &solState ) {
	/**
	*	Register 'User Type':
	**/
	sol::usertype<lua_edict_t> lua_edict_type = solState.new_usertype<lua_edict_t>( "lua_edict_t",
		//sol::no_constructor,
		sol::constructors< lua_edict_t(), lua_edict_t( edict_t* ) >()
	);

	/**
	*	(Read Only-) Variables:
	**/
	// Technically, this is a part of entity_state_t however it's just easier to write 'entity.number' than 'entity.state.number' in Lua.
	lua_edict_type[ "number" ] = sol::property( &lua_edict_t::get_number, &lua_edict_t::set_number );

	/**
	*	Modifyable Variables:
	**/
	// Returns the member entity_state_t of edict_t.
	lua_edict_type[ "state" ] = sol::property( &lua_edict_t::get_state );
}