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
	lua_edict_t() ;
	lua_edict_t( edict_t *_edict );

	/**
	*
	*	property(number):
	*
	**/
	/**
	*	@brief
	**/
	const int32_t get_number() const;
	/**
	*	@brief
	**/
	void set_number( const int32_t number ) const;
};

/**
*	@brief	Register a usertype for passing along edict_t into lua.
**/
void UserType_Register_Edict_t( sol::state &solState );