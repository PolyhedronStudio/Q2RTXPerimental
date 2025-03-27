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
	//svg_entity_t *edict;

public:
	svg_entity_t *edict;

	/**
	*
	*	Constructors:
	*
	**/
	lua_edict_state_t();
	lua_edict_state_t( svg_entity_t *_edict );

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
	*	@return Get EntityEffects.
	**/
	const int32_t get_entity_effects( sol::this_state s ) const;
	/**
	*	@return Set EntityEffects.
	**/
	void set_entity_effects( sol::this_state s, const int32_t effects );

	/**
	*	@return Get RenderFx.
	**/
	const int32_t get_renderfx( sol::this_state s ) const;
	/**
	*	@brief Set RenderFx
	**/
	void set_renderfx( sol::this_state s, const int32_t renderFx );

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

	/**
	*	@return Get Sound.
	**/
	const int32_t get_sound( sol::this_state s ) const;
	/**
	*	@brief Set Sound.
	**/
	void set_sound( sol::this_state s, const int32_t sound );
	/**
	*	@return Get Event.
	**/
	const int32_t get_event( sol::this_state s ) const;
	/**
	*	@brief Set Event.
	**/
	void set_event( sol::this_state s, const int32_t event );
};


/**
*	@brief	Register a usertype for passing along entity_state_t into lua.
**/
void UserType_Register_EdictState_t( sol::state &solState );