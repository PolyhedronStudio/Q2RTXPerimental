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
struct lua_edict_handle_t {
	//! The pointer at the time of creating the handle.
	svg_base_edict_t *edictPtr;
	//! The number of the edictPtr.
	int32_t number;
	//! The spawncount of the edictPtr.
	int32_t spawnCount;
};

/**
*
*	lua_edict_t:
*
**/
class lua_edict_t {
//private:
	//svg_base_edict_t *edict;

public:
	//svg_base_edict_t *edict;
	lua_edict_handle_t handle;

	/**
	* 
	*	Constructors: 
	* 
	**/
	lua_edict_t();
	lua_edict_t( svg_base_edict_t *_edict );


	/**
	*
	* 
	*	Entity Properties
	*
	* 
	**/
	//
	// General:
	//
	/**
	*	@return	The entity's number(stored in state, but, this is easy access route.)
	**/
	const int32_t get_number( sol::this_state s );


	//
	// State:
	//
	/**
	*	@return	Returns a lua userdata object for accessing the entity's entity_state_t.
	**/
	sol::object get_state( sol::this_state s );


	//
	//	UseTarget(s)
	//
	/**
	*	@brief
	**/
	const int32_t get_usetarget_flags( sol::this_state s );
	/**
	*	@return	
	**/
	void set_usetarget_flags( sol::this_state s, const int32_t flags );
	/**
	*	@brief
	**/
	const int32_t get_usetarget_state( sol::this_state s );
	/**
	*	@return	
	**/
	void set_usetarget_state( sol::this_state s, const int32_t state );


	//
	// Triggers
	//
	/**
	*	@brief
	**/
	const double get_trigger_wait( sol::this_state s );
	/**
	*	@return
	**/
	void set_trigger_wait( sol::this_state s, const double wait );
	/**
	*	@brief
	**/
	const double get_trigger_delay( sol::this_state s );
	/**
	*	@return
	**/
	void set_trigger_delay( sol::this_state s, const double delay );


	//
	// Strings.
	//
	/**
	*	@brief
	**/
	const std::string get_string_classname( sol::this_state s );

	/**
	*	@brief	
	**/
	const std::string get_string_target( sol::this_state s );
	/**
	*	@brief	
	**/
	void set_string_target( sol::this_state s, const char *luaStrTarget );

	/**
	*	@brief
	**/
	const std::string get_string_targetname( sol::this_state s );
	/**
	*	@brief
	**/
	void set_string_targetname( sol::this_state s, const char *luaStrTargetName );

	/**
	*	@brief
	**/
	const std::string get_string_luaname( sol::this_state s );
	/**
	*	@brief
	**/
	void set_string_luaname( sol::this_state s, const char *luaStrLuaName );
};

/**
*	@brief	Register a usertype for passing along svg_base_edict_t into lua.
**/
void UserType_Register_Edict_t( sol::state &solState );