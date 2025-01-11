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
		Lua_DeveloperPrintf( "%s: lua_edict_t::edict == (nullptr) !\n", (__FUNCTION__) ); \
		return; \
	} \
} while (false)

#define LUA_VALIDATE_EDICT_POINTER_RETVAL(returnValue) \
do { \
	if ( edict == nullptr ) { \
		Lua_DeveloperPrintf( "%s: lua_edict_t::edict == (nullptr) !\n", (__FUNCTION__) ); \
		return returnValue; \
	} \
} while (false)



/**
*
*	Constructors:
*
**/
lua_edict_t::lua_edict_t() : edict(nullptr) {
	// Returns if invalid.
	LUA_VALIDATE_EDICT_POINTER();
	
}
lua_edict_t::lua_edict_t( edict_t *_edict ) : edict(_edict) {
	// Returns if invalid.
	LUA_VALIDATE_EDICT_POINTER();
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
const int32_t lua_edict_t::get_number( sol::this_state s ) const {
	// Returns if invalid.
	LUA_VALIDATE_EDICT_POINTER_RETVAL( -1 );
	// Return number.
	return this->edict->s.number;
}
/**
*	@brief
**/
//void lua_edict_t::set_number( sol::this_state s, const int32_t number ) {
//	// Error notify.
//	LUA_ErrorPrintf( "%s: Can't change an entities number!\n", __func__ );
//}


//
// State:
//
/**
*	@return	Returns a lua userdata object for accessing the entity's entity_state_t.
**/
sol::object lua_edict_t::get_state( sol::this_state s ) {
	// Returns if invalid.
	LUA_VALIDATE_EDICT_POINTER_RETVAL( sol::nil );

	// Get state.
	sol::state_view solState( s );
	// Create a reference object of entity state userdata.
	return sol::make_object_userdata<lua_edict_state_t>(solState, this->edict);
}


//
// UseTargets:
//
/**
*	@brief
**/
const int32_t lua_edict_t::get_usetarget_flags( sol::this_state s ) const {
	// Returns if invalid.
	LUA_VALIDATE_EDICT_POINTER_RETVAL( entity_usetarget_flags_t::ENTITY_USETARGET_FLAG_NONE );
	// Return number.
	return this->edict->useTarget.flags;
}
/**
*	@brief
**/
void lua_edict_t::set_usetarget_flags( sol::this_state s, const int32_t flags ) {
	// Returns if invalid.
	LUA_VALIDATE_EDICT_POINTER();
	// Assign new flags.
	this->edict->useTarget.flags = (entity_usetarget_flags_t)flags;
}
/**
*	@brief
**/
const int32_t lua_edict_t::get_usetarget_state( sol::this_state s ) const {
	// Returns if invalid.
	LUA_VALIDATE_EDICT_POINTER_RETVAL( entity_usetarget_state_t::ENTITY_USETARGET_STATE_DEFAULT );
	// Return number.
	return this->edict->useTarget.state;
}
/**
*	@brief
**/
void lua_edict_t::set_usetarget_state( sol::this_state s, const int32_t state ) {
	// Returns if invalid.
	LUA_VALIDATE_EDICT_POINTER();
	// Assign new flags.
	this->edict->useTarget.state = (entity_usetarget_state_t)state;
}


//
// Triggers
//
/**
*	@brief
**/
const double lua_edict_t::get_trigger_wait( sol::this_state s ) const {
	// Returns if invalid.
	LUA_VALIDATE_EDICT_POINTER_RETVAL( 0 );
	// Return value.
	return this->edict->wait;
}
/**
*	@return
**/
void lua_edict_t::set_trigger_wait( sol::this_state s, const double wait ) {
	// Returns if invalid.
	LUA_VALIDATE_EDICT_POINTER();
	// Assign new value.
	this->edict->wait = wait;
}

/**
*	@brief
**/
const double lua_edict_t::get_trigger_delay( sol::this_state s ) const {
	// Returns if invalid.
	LUA_VALIDATE_EDICT_POINTER_RETVAL(0);
	// Return value.
	return this->edict->delay;
}
/**
*	@return
**/
void lua_edict_t::set_trigger_delay( sol::this_state s, const double delay ) {
	// Returns if invalid.
	LUA_VALIDATE_EDICT_POINTER();
	// Assign new value.
	this->edict->delay = delay;
}


/***
*
*
*
*	Registering of Lua Type:
*
*
*
**/
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
	lua_edict_type[ "number" ] = sol::property( &lua_edict_t::get_number/*, &lua_edict_t::set_number */);

	/**
	*	Modifyable Variables:
	**/
	// Returns the member entity_state_t of edict_t.
	lua_edict_type[ "state" ] = sol::property( &lua_edict_t::get_state );

	// For useTargets:
	lua_edict_type[ "useTargetFlags" ] = sol::property( &lua_edict_t::get_usetarget_flags, &lua_edict_t::set_usetarget_flags );
	lua_edict_type[ "useTargetState" ] = sol::property( &lua_edict_t::get_usetarget_state, &lua_edict_t::set_usetarget_state );

	// Triggers:
	lua_edict_type[ "wait" ] = sol::property( &lua_edict_t::get_trigger_wait, &lua_edict_t::set_trigger_wait );
	lua_edict_type[ "delay" ] = sol::property( &lua_edict_t::get_trigger_delay, &lua_edict_t::set_trigger_delay );
}