/********************************************************************
*
*
*	ServerGame: Lua Binding API UserType.
*	NameSpace: "".
*
*
********************************************************************/
#pragma once



/**
*
*
*
*	NameSpace: "":
*
*
*
**/
/**
*
*	lua_edict_t:
*
**/
class lua_edict_t {
//private:
	//edict_t *edict;

public:
	edict_t *edict;

	/**
	* 
	*	Constructors: 
	* 
	**/
	lua_edict_t();
	lua_edict_t( edict_t *_edict );

	/**
	*
	* 
	*	Entity State Properties
	*
	* 
	**/
	/**
	*	@brief
	**/
	const int32_t get_number( sol::this_state s ) const;

	/**
	*	@return	Returns a lua userdata object for accessing the entity's entity_state_t.
	**/
	sol::object get_state( sol::this_state s );
};

/**
*	@brief	Register a usertype for passing along edict_t into lua.
**/
void UserType_Register_Edict_t( sol::state &solState );