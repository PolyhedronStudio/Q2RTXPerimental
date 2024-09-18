/********************************************************************
*
*
*	ServerGmae: Lua Utility: CallFunction - 
*
*	Uses variadic arg template method to allow for multiple arguments.
*	Default supported types are:
*		uint64_t
*		int64_t
*		uint32_t
*		int32_t
*		double
*		float
*		std::string
*		char*(because of std::string)
*		edict_t*(if not a nullptr, and entity is inuse, it will push to stack the entity number, -1 otherwise.)
* 
*	Implementation for custom types can be added.
*	
*
********************************************************************/
#pragma once

static inline void LUA_CallFunction_PushStackValue( lua_State *L ) {
	// Do absolutely nothing
	return;
}
// edict_t*
template <typename... Rest>
static inline void LUA_CallFunction_PushStackValue( lua_State *L, const edict_t *e, const Rest&... rest ) {
	if ( e != nullptr && e->inuse ) {
		lua_pushinteger( L, e->s.number );
	} else {
		lua_pushinteger( L, -1 );
	}
	LUA_CallFunction_PushStackValue( L, rest... );
}
// uint64_t
template <typename... Rest>
static inline void LUA_CallFunction_PushStackValue( lua_State *L, const uint64_t &i, const Rest&... rest ) {
	lua_pushinteger( L, i );
	LUA_CallFunction_PushStackValue( L, rest... );
}
// int32_t
template <typename... Rest>
static inline void LUA_CallFunction_PushStackValue( lua_State *L, const int64_t &i, const Rest&... rest ) {
	lua_pushinteger( L, i );
	LUA_CallFunction_PushStackValue( L, rest... );
}

// uint32_t
template <typename... Rest>
static inline void LUA_CallFunction_PushStackValue( lua_State *L, const uint32_t &i, const Rest&... rest ) {
	lua_pushinteger( L, i );
	LUA_CallFunction_PushStackValue( L, rest... );
}
// int32_t
template <typename... Rest>
static inline void LUA_CallFunction_PushStackValue( lua_State *L, const int32_t &i, const Rest&... rest ) {
	lua_pushinteger( L, i );
	LUA_CallFunction_PushStackValue( L, rest... );
}
// float
template <typename... Rest>
static inline void LUA_CallFunction_PushStackValue( lua_State *L, const float &f, const Rest&... rest ) {
	lua_pushnumber( L, f );
	LUA_CallFunction_PushStackValue( L, rest... );
}
// double
template <typename... Rest>
static inline void LUA_CallFunction_PushStackValue( lua_State *L, const double &d, const Rest&... rest ) {
	lua_pushnumber( L, d );
	LUA_CallFunction_PushStackValue( L, rest... );
}

template <typename... Rest>
static inline void LUA_CallFunction_PushStackValue( lua_State *L, const std::string &s, const Rest&... rest ) {
	lua_pushstring( L, s.c_str() );
	LUA_CallFunction_PushStackValue( L, rest... );
}

/**
*	@brief	Looks up the specified function, if it exists, will call it with the variadic arguments supplied( if any ).
*	@return	True in case it was a success, meaning the return values are now in the lua stack and need to be popped.
*	@note	One has to deal and pop return values themselves.
**/
template <typename... Rest> 
static const bool LUA_CallFunction( lua_State *L, const std::string &functionName, const int32_t numReturnValues, const Rest&... rest ) {
	bool executedSuccessfully = false;

	if ( !L || functionName.empty() ) {
		return executedSuccessfully;
	}

	// Get the global functionname value and push it to stack:
	lua_getglobal( L, functionName.c_str() );

	// Check if function even exists.
	if ( lua_isfunction( L, -1 ) ) {
		// Push all arguments to the lua stack.
		LUA_CallFunction_PushStackValue( L, rest... ); // Recursive call using pack expansion syntax

		// Protect Call the pushed function name string.
		int32_t pCallReturnValue = lua_pcall( L, sizeof...( Rest ), numReturnValues, 0 );
		if ( pCallReturnValue == LUA_OK ) {
			// Pop function name from stack.
			lua_pop( L, lua_gettop( L ) );
			// Success.
			executedSuccessfully = true;
		} else {
			// Get error.
			const std::string errorStr = lua_tostring( L, lua_gettop( L ) );
			// Remove the errorStr from the stack
			lua_pop( L, lua_gettop( L ) );
			// Print Error Notification.
			LUA_ErrorPrintf( "%s: %s\n", __func__, errorStr.c_str() );
		}
	} else {
		// Pop function name from stack.
		lua_pop( L, lua_gettop( L ) );
		// Print Error Notification.
		LUA_ErrorPrintf( "%s: %s is not a function\n", __func__, functionName );
	}

	// Return failure.
	return executedSuccessfully;
}