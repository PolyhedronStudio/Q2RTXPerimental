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

//! Determines 'How' to be verbose, if at all.
typedef enum {
	//! Don't be verbose at all.
	LUA_CALLFUNCTION_VERBOSE_NOT		= 0,
	//! Only be verbose if the function is actually missing.
	LUA_CALLFUNCTION_VERBOSE_MISSING	= BIT( 0 ),
	//! Will also be verbose if something went wrong trying and during execution of the function.
	LUA_CALLFUNCTION_VERBOSE_MASK_ALL	= LUA_CALLFUNCTION_VERBOSE_MISSING /* | SOME_OTHER_VERBOSE */,
} svg_lua_callfunction_verbosity_t;


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
// const char*
template <typename... Rest>
static inline void LUA_CallFunction_PushStackValue( lua_State *L, const char *s, const Rest&... rest ) {
	lua_pushlstring( L, s, strlen( s ) );
	LUA_CallFunction_PushStackValue( L, rest... );
}
// const std::string &
template <typename... Rest>
static inline void LUA_CallFunction_PushStackValue( lua_State *L, const std::string &s, const Rest&... rest ) {
	lua_pushlstring( L, s.c_str(), s.length() );
	LUA_CallFunction_PushStackValue( L, rest... );
}
// svg_signal_argument_array_t
template <typename... Rest>
static inline void LUA_CallFunction_PushStackValue( lua_State *L, const svg_signal_argument_array_t &signalArguments, const Rest&... rest ) {
	if ( !signalArguments.empty() ) {
		// Create the table for the size of the argument array.
		lua_createtable( L, 0, signalArguments.size() );

		for ( int32_t i = 0; i < signalArguments.size(); i++ ) {
			// Pointer to argument.
			const svg_signal_argument_t *signalArgument = &signalArguments[ i ];
			// Push depending on its type.
			if ( signalArgument->type == SIGNAL_ARGUMENT_TYPE_BOOLEAN ) {
				// Push boolean value.
				lua_pushboolean( L, signalArgument->value.boolean );
				// Set key name.
				lua_setfield( L, -2, signalArgument->key );
			// TODO: Hmm?? Sol has no notion, maybe though using sol::object.as however, of an integer??
			#if 0
			} else if ( signalArgument->type == SIGNAL_ARGUMENT_TYPE_INTEGER) {
				// Push integer value.
				lua_pushinteger( L, signalArgument->value.integer );
				// Set key name.
				lua_setfield( L, -2, signalArgument->key );
			#endif
			} else if ( signalArgument->type == SIGNAL_ARGUMENT_TYPE_NUMBER) {
				// Push integer value.
				lua_pushnumber( L, signalArgument->value.number );
				// Set key name.
				lua_setfield( L, -2, signalArgument->key );
			} else if ( signalArgument->type == SIGNAL_ARGUMENT_TYPE_STRING ) {
				// Push integer value.
				lua_pushlstring( L, signalArgument->value.str, strlen( signalArgument->value.str ) );
				// Set key name.
				lua_setfield( L, -2, signalArgument->key );
			} else if ( signalArgument->type == SIGNAL_ARGUMENT_TYPE_NULLPTR ) {
				// Push nil value.
				lua_pushnil( L );
				// Set key name.
				lua_setfield( L, -2, signalArgument->key );
			} else {
				// WID: TODO: Warn?
			}
		}
	} else {
		// Push nil value.
		lua_pushnil( L );
	}

	//lua_pushlstring( L, s.c_str(), s.length() );
	LUA_CallFunction_PushStackValue( L, rest... );
}


/**
*	@brief	Looks up the specified function, if it exists, will call it with the variadic arguments supplied( if any ).
*	@param	numExpectedArgs	If left at 0 it ignores checking the stack for number of valid arguments.
*	@return	True in case it was a success, meaning the return values are now in the lua stack and need to be popped.
*	@note	One has to deal and pop return values themselves.
**/
template <typename... Rest> 
static const bool LUA_CallFunction( sol::state_view &stateView, const std::string &functionName, 
	const int32_t numExpectedArgs = 0, const int32_t numReturnValues = 0, const svg_lua_callfunction_verbosity_t verbosity = LUA_CALLFUNCTION_VERBOSE_NOT,
	const Rest&... rest ) {
	
	bool executedSuccessfully = false;

	if ( !stateView.lua_state() || functionName.empty() ) {
		return executedSuccessfully;
	}

	// Get function object.
	sol::protected_function funcRefCall = stateView[ functionName ];
	// Get type.
	sol::type funcRefType = funcRefCall.get_type();
	// Ensure it matches, accordingly
	if ( funcRefType != sol::type::function /*|| !funcRefSignalOut.is<std::function<void( Rest... )>>() */ ) {
		// Return if it is LUA_NOREF and luaState == nullptr again.
		return false;
	}

	try {
		funcRefCall( rest... );
	}
	catch ( std::exception &e ) {
		gi.bprintf( PRINT_ERROR, "%s: %s\n", __func__, e.what() );
		return false;
	}

	executedSuccessfully = true;
	#if 0
	// Get the global functionname value and push it to stack:
	lua_getglobal( L, functionName.c_str() );

	// Check if function even exists.
	if ( lua_isfunction( L, -1 ) ) {
		// Push all arguments to the lua stack.
		LUA_CallFunction_PushStackValue( L, rest... ); // Recursive call using pack expansion syntax

		// Check stack.
		if ( numExpectedArgs >= 1 ) {
			// Notify about the stack being incorrectly setup.
			luaL_checkstack( L, numExpectedArgs, "too many arguments" );
		}

		// Protect Call the pushed function name string.
		int32_t pCallReturnValue = lua_pcall( L, sizeof...( Rest ), numReturnValues, 0 );
		if ( pCallReturnValue == LUA_OK ) {
			// Success.
			executedSuccessfully = true;
		// Yielded -> TODO: Can we ever make yielding work by doing something here to
		// keeping save/load games in mind?
		} else if ( pCallReturnValue == LUA_YIELD ) {
			// Get error.
			const std::string errorStr = lua_tostring( L, -1 );
			// Remove the errorStr from the stack
			lua_pop( L, lua_gettop( L ) );
			// Print Error Notification.
			LUA_ErrorPrintf( "[%s]:[Yielding Not Supported!]:\n%s\n", __func__, errorStr.c_str() );
		// Runtime Error:
		} else if ( pCallReturnValue == LUA_ERRRUN ) {
			// Get error.
			const std::string errorStr = lua_tostring( L, -1 );
			// Remove the errorStr from the stack
			lua_pop( L, lua_gettop( L ) );
			// Print Error Notification.
			LUA_ErrorPrintf( "[%s]:[Runtime Error]:\n%s\n", __func__, errorStr.c_str() );
		// Syntax Error:
		} else if ( pCallReturnValue == LUA_ERRSYNTAX ) {
			// Get error.
			const std::string errorStr = lua_tostring( L, -1 );
			// Remove the errorStr from the stack
			lua_pop( L, lua_gettop( L ) );
			// Print Error Notification.
			LUA_ErrorPrintf( "[%s]:[Syntax Error]:\n%s\n", __func__, errorStr.c_str() );
		// Memory Error:
		} else if ( pCallReturnValue == LUA_ERRMEM ) {
			// Get error.
			const std::string errorStr = lua_tostring( L, -1 );
			// Remove the errorStr from the stack
			lua_pop( L, lua_gettop( L ) );
			// Print Error Notification.
			LUA_ErrorPrintf( "[%s]:[Memory Error]:\n%s\n", __func__, errorStr.c_str() );
		// Error Error? Lol: TODO: WTF?
		} else if ( pCallReturnValue == LUA_ERRERR ) {
			// Get error.
			const std::string errorStr = lua_tostring( L, -1 );
			// Remove the errorStr from the stack
			lua_pop( L, lua_gettop( L ) );
			// Print Error Notification.
			LUA_ErrorPrintf( "[%s]:[Error]:\n%s\n", __func__, errorStr.c_str() );
		} else {
			// Get error.
			const std::string errorStr = lua_tostring( L, -1 );
			// Remove the errorStr from the stack
			lua_pop( L, lua_gettop( L ) );
			// Print Error Notification.
			LUA_ErrorPrintf( "%s:\n%s\n", __func__, errorStr.c_str() );
		}
	} else {
		// Remove the errorStr from the stack
		lua_pop( L, lua_gettop( L ) );
		// There are use cases where we don't want to report an actual error and it is 
		// just fine if the function does not exist at all.
		if ( !( verbosity & LUA_CALLFUNCTION_VERBOSE_MISSING ) ) {
			// Print Error Notification.
			LUA_ErrorPrintf( "%s:\n\"%s\" is not a function\n", __func__, functionName.c_str() );
		}
	}
	#endif
	// Return failure.
	return executedSuccessfully;
}