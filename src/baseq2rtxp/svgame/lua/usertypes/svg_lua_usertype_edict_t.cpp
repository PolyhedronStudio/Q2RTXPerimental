/********************************************************************
*
*
*	ServerGame: Lua GameLib API UserType.
*	NameSpace: "".
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/svg_trigger.h"
#include "svgame/svg_utils.h"

#include "svgame/entities/svg_entities_pushermove.h"

#include "svgame/svg_lua.h"
#include "svgame/lua/svg_lua_gamelib.hpp"

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
		if ( edictInSlot->s.number != handle.number ) { \
			handle = { .number = -1 }; \
			return returnValue; \
		} \
		if ( edictInSlot->spawn_count != handle.spawnCount ) { \
			handle = { .number = -1 }; \
			return returnValue; \
		} \
	}\
} while (false)

/**
*
*	Constructors:
*
**/
lua_edict_t::lua_edict_t() : handle( {} ) {
	// Returns if invalid.
	LUA_VALIDATE_EDICT_HANDLE();
	
}
lua_edict_t::lua_edict_t( svg_base_edict_t *_edict ) /*: edict(_edict)*/ {
	//! Setup pointer.
	handle.edictPtr		= _edict;
	handle.number		= _edict->s.number;
	handle.spawnCount	= _edict->spawn_count;

	// Returns if invalid.
	LUA_VALIDATE_EDICT_HANDLE();
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
const int32_t lua_edict_t::get_number( sol::this_state s ) {
	// Returns if invalid.
	LUA_VALIDATE_EDICT_HANDLE_RETVAL( -1 );
	// Return number.
	return handle.edictPtr->s.number;
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
	LUA_VALIDATE_EDICT_HANDLE_RETVAL( sol::nil );

	// Get state.
	sol::state_view solState( s );
	// Create a reference object of entity state userdata.
	return sol::make_object_userdata<lua_edict_state_t>(solState, this->handle.edictPtr );
}


//
// UseTargets:
//
/**
*	@brief
**/
const int32_t lua_edict_t::get_usetarget_flags( sol::this_state s ) {
	// Returns if invalid.
	LUA_VALIDATE_EDICT_HANDLE_RETVAL( entity_usetarget_flags_t::ENTITY_USETARGET_FLAG_NONE );
	// Return number.
	//return this->edict->useTarget.flags;
	return handle.edictPtr->useTarget.flags;
}
/**
*	@brief
**/
void lua_edict_t::set_usetarget_flags( sol::this_state s, const int32_t flags ) {
	// Returns if invalid.
	LUA_VALIDATE_EDICT_HANDLE();
	// Assign new flags.
	//this->edict->useTarget.flags = (entity_usetarget_flags_t)flags;
	handle.edictPtr->useTarget.flags = (entity_usetarget_flags_t)flags;
}
/**
*	@brief
**/
const int32_t lua_edict_t::get_usetarget_state( sol::this_state s ) {
	// Returns if invalid.
	LUA_VALIDATE_EDICT_HANDLE_RETVAL( entity_usetarget_state_t::ENTITY_USETARGET_STATE_DEFAULT );
	// Return number.
	//return this->edict->useTarget.state;
	return handle.edictPtr->useTarget.state;
}
/**
*	@brief
**/
void lua_edict_t::set_usetarget_state( sol::this_state s, const int32_t state ) {
	// Returns if invalid.
	LUA_VALIDATE_EDICT_HANDLE();
	// Assign new flags.
	handle.edictPtr->useTarget.state = (entity_usetarget_state_t)state;
}


//
// Triggers
//
/**
*	@brief
**/
const double lua_edict_t::get_trigger_wait( sol::this_state s ) {
	// Returns if invalid.
	LUA_VALIDATE_EDICT_HANDLE_RETVAL( 0 );
	// Return value.
	return handle.edictPtr->wait;
}
/**
*	@return
**/
void lua_edict_t::set_trigger_wait( sol::this_state s, const double wait ) {
	// Returns if invalid.
	LUA_VALIDATE_EDICT_HANDLE();
	// Assign new value.
	handle.edictPtr->wait = wait;
}

/**
*	@brief
**/
const double lua_edict_t::get_trigger_delay( sol::this_state s ) {
	// Returns if invalid.
	LUA_VALIDATE_EDICT_HANDLE_RETVAL(0);
	// Return value.
	return handle.edictPtr->delay;
}
/**
*	@return
**/
void lua_edict_t::set_trigger_delay( sol::this_state s, const double delay ) {
	// Returns if invalid.
	LUA_VALIDATE_EDICT_HANDLE();
	// Assign new value.
	handle.edictPtr->delay = delay;
}



//
// Strings.
//
/**
*	@brief
**/
const std::string lua_edict_t::get_string_classname( sol::this_state s ) {
	// Returns if invalid.
	LUA_VALIDATE_EDICT_HANDLE_RETVAL( "\0" );
	// Return pointer to copy into std::string
	return ( handle.edictPtr->classname ? (const char *)handle.edictPtr->classname : "\0" );
}

/**
*	@brief
**/
const std::string lua_edict_t::get_string_target( sol::this_state s ) {
	// Returns if invalid.
	LUA_VALIDATE_EDICT_HANDLE_RETVAL( "\0" );
	// Return pointer to copy into std::string
	return ( handle.edictPtr->targetNames.target ? (const char *)handle.edictPtr->targetNames.target : "\0" );
}
/**
*	@brief
**/
void lua_edict_t::set_string_target( sol::this_state s, const char *luaStrTarget ) {
	// Returns if invalid.
	LUA_VALIDATE_EDICT_HANDLE();
	// nullptr it if the string is empty.
	if ( !luaStrTarget || luaStrTarget[ 0 ] == '\0' ) {
		handle.edictPtr->targetNames.target = nullptr;
	}
	// Assign new value.
	handle.edictPtr->targetNames.target = luaStrTarget;
}

/**
*	@brief
**/
const std::string lua_edict_t::get_string_targetname( sol::this_state s ) {
	// Returns if invalid.
	LUA_VALIDATE_EDICT_HANDLE_RETVAL( "\0" );
	// Return pointer to copy into std::string
	return ( handle.edictPtr->targetname ? (const char *)handle.edictPtr->targetname : "\0" );
}
/**
*	@brief
**/
void lua_edict_t::set_string_targetname( sol::this_state s, const char *luaStrTargetName ) {
	// Returns if invalid.
	LUA_VALIDATE_EDICT_HANDLE();
	// nullptr it if the string is empty.
	if ( !luaStrTargetName || luaStrTargetName[ 0 ] == '\0' ) {
		handle.edictPtr->targetname = nullptr;
	}
	// Assign new value.
	handle.edictPtr->targetname = luaStrTargetName;
}

/**
*	@brief
**/
const std::string lua_edict_t::get_string_luaname( sol::this_state s ) {
	// Returns if invalid.
	LUA_VALIDATE_EDICT_HANDLE_RETVAL( "\0" );
	// Return pointer to copy into std::string
	return ( handle.edictPtr->luaProperties.luaName ? (const char *)handle.edictPtr->luaProperties.luaName : "\0" );
}

/**
*	@brief
**/
void lua_edict_t::set_string_luaname( sol::this_state s, const char *luaStrLuaName ) {
	// Returns if invalid.
	LUA_VALIDATE_EDICT_HANDLE();
	// nullptr it if the string is empty.
	if ( !luaStrLuaName || luaStrLuaName[ 0 ] == '\0' ) {
		handle.edictPtr->luaProperties.luaName = nullptr;
	}
	// Assign new value.
	handle.edictPtr->luaProperties.luaName = luaStrLuaName;
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
*	@brief	Register a usertype for passing along svg_base_edict_t into lua.
**/
void UserType_Register_Edict_t( sol::state &solState ) {
	/**
	*	Register 'User Type':
	**/
	sol::usertype<lua_edict_t> lua_edict_type = solState.new_usertype<lua_edict_t>( "lua_edict_t",
		//sol::no_constructor,
		sol::constructors< lua_edict_t(), lua_edict_t( svg_base_edict_t* ) >()
	);

	/**
	*	(Read Only-) Variables:
	**/
	// Technically, this is a part of entity_state_t however it's just easier to write 'entity.number' than 'entity.state.number' in Lua.
	lua_edict_type[ "number" ] = sol::property( &lua_edict_t::get_number/*, &lua_edict_t::set_number */);

	/**
	*	Modifyable Variables:
	**/
	// Returns the member entity_state_t of svg_base_edict_t.
	lua_edict_type[ "state" ] = sol::property( &lua_edict_t::get_state );

	// For useTargets:
	lua_edict_type[ "useTargetFlags" ] = sol::property( &lua_edict_t::get_usetarget_flags, &lua_edict_t::set_usetarget_flags );
	lua_edict_type[ "useTargetState" ] = sol::property( &lua_edict_t::get_usetarget_state, &lua_edict_t::set_usetarget_state );

	// Triggers:
	lua_edict_type[ "wait" ] = sol::property( &lua_edict_t::get_trigger_wait, &lua_edict_t::set_trigger_wait );
	lua_edict_type[ "delay" ] = sol::property( &lua_edict_t::get_trigger_delay, &lua_edict_t::set_trigger_delay );

	// Strings:
	lua_edict_type[ "className" ] = sol::property( &lua_edict_t::get_string_classname /*, &lua_edict_t::set_string_classname */ );
	lua_edict_type[ "target" ] = sol::property( &lua_edict_t::get_string_target, &lua_edict_t::set_string_target );
	lua_edict_type[ "targetName" ] = sol::property( &lua_edict_t::get_string_targetname, &lua_edict_t::set_string_targetname );
	lua_edict_type[ "luaName" ] = sol::property( &lua_edict_t::get_string_luaname, &lua_edict_t::set_string_luaname );
}