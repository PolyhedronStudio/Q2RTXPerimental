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
*	lua_edict_state_t:
*
**/
class lua_edict_state_t {
//private:
	//edict_t *edict;

public:
	edict_t *edict;

	/**
	*
	*	Constructors:
	*
	**/
	lua_edict_state_t();
	lua_edict_state_t( edict_t *_edict );

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
	*	@return Get (Brush-)Entity Frame.
	**/
	const int32_t get_frame( sol::this_state s ) const;
	/**
	*	@brief Set (Brush-)Entity Frame.
	**/
	void set_frame( sol::this_state s, const int32_t frame );

	/**
*	@return Get ModelIndex:
**/
	const int32_t get_model_index( sol::this_state s ) const;
	const int32_t get_model_index2( sol::this_state s ) const;
	const int32_t get_model_index3( sol::this_state s ) const;
	const int32_t get_model_index4( sol::this_state s ) const;
	/**
	*	@brief Set ModelIndex.
	**/
	void set_model_index( sol::this_state s, const int32_t modelIndex );
	void set_model_index2( sol::this_state s, const int32_t modelIndex );
	void set_model_index3( sol::this_state s, const int32_t modelIndex );
	void set_model_index4( sol::this_state s, const int32_t modelIndex );
};


/**
*	@brief	Register a usertype for passing along entity_state_t into lua.
**/
void UserType_Register_EdictState_t( sol::state &solState );