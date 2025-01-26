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
	const int32_t get_number( sol::this_state s ) const;


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
	const int32_t get_usetarget_flags( sol::this_state s ) const;
	/**
	*	@return	
	**/
	void set_usetarget_flags( sol::this_state s, const int32_t flags );
	/**
	*	@brief
	**/
	const int32_t get_usetarget_state( sol::this_state s ) const;
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
	const double get_trigger_wait( sol::this_state s ) const;
	/**
	*	@return
	**/
	void set_trigger_wait( sol::this_state s, const double wait );
	/**
	*	@brief
	**/
	const double get_trigger_delay( sol::this_state s ) const;
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
	const std::string get_string_classname( sol::this_state s ) const;

	/**
	*	@brief	
	**/
	const std::string get_string_target( sol::this_state s ) const;
	/**
	*	@brief	
	**/
	void set_string_target( sol::this_state s, const char *luaStrTarget );

	/**
	*	@brief
	**/
	const std::string get_string_targetname( sol::this_state s ) const;
	/**
	*	@brief
	**/
	void set_string_targetname( sol::this_state s, const char *luaStrTargetName );
};

/**
*	@brief	Register a usertype for passing along edict_t into lua.
**/
void UserType_Register_Edict_t( sol::state &solState );