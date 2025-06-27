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
*		svg_base_edict_t*(if not a nullptr, and entity is inuse, it will push to stack the entity number, -1 otherwise.)
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


//static inline void LUA_CallFunction_PushStackValue( lua_State *L ) {
//	// Do absolutely nothing
//	return;
//}
//// svg_base_edict_t*
//template <typename... Rest>
//static inline void LUA_CallFunction_PushStackValue( lua_State *L, const svg_base_edict_t *e, const Rest&... rest ) {
//	if ( e != nullptr && e->inuse ) {
//		lua_pushinteger( L, e->s.number );
//	} else {
//		lua_pushinteger( L, -1 );
//	}
//	LUA_CallFunction_PushStackValue( L, rest... );
//}
//// uint64_t
//template <typename... Rest>
//static inline void LUA_CallFunction_PushStackValue( lua_State *L, const uint64_t &i, const Rest&... rest ) {
//	lua_pushinteger( L, i );
//	LUA_CallFunction_PushStackValue( L, rest... );
//}
//// int32_t
//template <typename... Rest>
//static inline void LUA_CallFunction_PushStackValue( lua_State *L, const int64_t &i, const Rest&... rest ) {
//	lua_pushinteger( L, i );
//	LUA_CallFunction_PushStackValue( L, rest... );
//}
//
//// uint32_t
//template <typename... Rest>
//static inline void LUA_CallFunction_PushStackValue( lua_State *L, const uint32_t &i, const Rest&... rest ) {
//	lua_pushinteger( L, i );
//	LUA_CallFunction_PushStackValue( L, rest... );
//}
//// int32_t
//template <typename... Rest>
//static inline void LUA_CallFunction_PushStackValue( lua_State *L, const int32_t &i, const Rest&... rest ) {
//	lua_pushinteger( L, i );
//	LUA_CallFunction_PushStackValue( L, rest... );
//}
//// float
//template <typename... Rest>
//static inline void LUA_CallFunction_PushStackValue( lua_State *L, const float &f, const Rest&... rest ) {
//	lua_pushnumber( L, f );
//	LUA_CallFunction_PushStackValue( L, rest... );
//}
//// double
//template <typename... Rest>
//static inline void LUA_CallFunction_PushStackValue( lua_State *L, const double &d, const Rest&... rest ) {
//	lua_pushnumber( L, d );
//	LUA_CallFunction_PushStackValue( L, rest... );
//}
//// const char*
//template <typename... Rest>
//static inline void LUA_CallFunction_PushStackValue( lua_State *L, const char *s, const Rest&... rest ) {
//	lua_pushlstring( L, s, strlen( s ) );
//	LUA_CallFunction_PushStackValue( L, rest... );
//}
//// const std::string &
//template <typename... Rest>
//static inline void LUA_CallFunction_PushStackValue( lua_State *L, const std::string &s, const Rest&... rest ) {
//	lua_pushlstring( L, s.c_str(), s.length() );
//	LUA_CallFunction_PushStackValue( L, rest... );
//}
//// svg_signal_argument_array_t
//template <typename... Rest>
//static inline void LUA_CallFunction_PushStackValue( lua_State *L, const svg_signal_argument_array_t &signalArguments, const Rest&... rest ) {
//	if ( !signalArguments.empty() ) {
//		// Create the table for the size of the argument array.
//		lua_createtable( L, 0, signalArguments.size() );
//
//		for ( int32_t i = 0; i < signalArguments.size(); i++ ) {
//			// Pointer to argument.
//			const svg_signal_argument_t *signalArgument = &signalArguments[ i ];
//			// Push depending on its type.
//			if ( signalArgument->type == SIGNAL_ARGUMENT_TYPE_BOOLEAN ) {
//				// Push boolean value.
//				lua_pushboolean( L, signalArgument->value.boolean );
//				// Set key name.
//				lua_setfield( L, -2, signalArgument->key );
//			// TODO: Hmm?? Sol has no notion, maybe though using sol::object.as however, of an integer??
//			#if 0
//			} else if ( signalArgument->type == SIGNAL_ARGUMENT_TYPE_INTEGER) {
//				// Push integer value.
//				lua_pushinteger( L, signalArgument->value.integer );
//				// Set key name.
//				lua_setfield( L, -2, signalArgument->key );
//			#endif
//			} else if ( signalArgument->type == SIGNAL_ARGUMENT_TYPE_NUMBER) {
//				// Push integer value.
//				lua_pushnumber( L, signalArgument->value.number );
//				// Set key name.
//				lua_setfield( L, -2, signalArgument->key );
//			} else if ( signalArgument->type == SIGNAL_ARGUMENT_TYPE_STRING ) {
//				// Push integer value.
//				lua_pushlstring( L, signalArgument->value.str, strlen( signalArgument->value.str ) );
//				// Set key name.
//				lua_setfield( L, -2, signalArgument->key );
//			} else if ( signalArgument->type == SIGNAL_ARGUMENT_TYPE_NULLPTR ) {
//				// Push nil value.
//				lua_pushnil( L );
//				// Set key name.
//				lua_setfield( L, -2, signalArgument->key );
//			} else {
//				// WID: TODO: Warn?
//			}
//		}
//	} else {
//		// Push nil value.
//		lua_pushnil( L );
//	}
//
//	//lua_pushlstring( L, s.c_str(), s.length() );
//	LUA_CallFunction_PushStackValue( L, rest... );
//}


/**
*	@brief	Looks up the specified function, if it exists, will call it with the variadic arguments supplied( if any ).
*	@param	numExpectedArgs	If left at 0 it ignores checking the stack for number of valid arguments.
*	@return	True in case it was a success, meaning the return values are now in the lua stack and need to be popped.
*	@note	One has to deal and pop return values themselves.
**/
template <typename RetType, typename... Rest> 
static const bool LUA_CallFunction( sol::state_view &stateView, 
	const std::string &functionName, RetType &returnValue,
	const svg_lua_callfunction_verbosity_t verbosity = LUA_CALLFUNCTION_VERBOSE_NOT,
	// WID: TODO: Do proper argument inspectations perhaps?
	//const int32_t numExpectedArgs = 0, const int32_t numReturnValues = 0,
	const Rest&... rest ) {
	
	// Default to false.
	bool executedSuccessfully = false;

	// Exit, without warning.
	if ( !stateView.lua_state() || functionName.empty() ) {
		return executedSuccessfully;
	}

	// Get protected function object.
	sol::protected_function funcRefCall = stateView[ functionName ];
	// Get type.
	sol::type funcRefType = funcRefCall.get_type();
	// Ensure it matches, accordingly
	if ( funcRefType != sol::type::function && verbosity != LUA_CALLFUNCTION_VERBOSE_MISSING /*|| !funcRefSignalOut.is<std::function<void( Rest... )>>() */ ) {
		// Return if it is LUA_NOREF and luaState == nullptr again.
		gi.bprintf( PRINT_ERROR, "%s: %s but is %s instead!\n", 
			__func__, "funcRefType != sol::type::function", 
			sol::type_name( stateView, funcRefType ).c_str()
		);
		// Exit.
		return executedSuccessfully;
	}

	// Expect true.
	sol::protected_function_result callResult = funcRefCall( rest ... );

	// Error if invalid.
	if ( !callResult.valid() || callResult.status() == sol::call_status::yielded ) { // We don't support yielding. (Impossible to save/load aka restore state.)
		if ( verbosity != LUA_CALLFUNCTION_VERBOSE_MISSING ) {
			// Acquire error object.
			sol::error resultError = callResult;
			// Get error string.
			const std::string errorStr = resultError.what();
			// Print the error in case of failure.
			gi.bprintf( PRINT_ERROR, "%s:\nCallStatus[%s]: %s\n ", __func__, sol::to_string( callResult.status() ).c_str(), errorStr.c_str() );
		}
	} else {
		// Cast to expected return value type.
		returnValue = callResult.get<RetType>();

		// Let it be known we executed function.
		executedSuccessfully = true;
	}

	// Return success or failure.
	return executedSuccessfully;
}

/**
*	@brief	Looks up the specified function, if it exists, will call it with the variadic arguments supplied( if any ).
*	@param	numExpectedArgs	If left at 0 it ignores checking the stack for number of valid arguments.
*	@return	True in case it was a success, meaning the return values are now in the lua stack and need to be popped.
*	@note	One has to deal and pop return values themselves.
**/
template <typename RetType, typename... Rest>
static const bool LUA_CallLuaNameEntityFunction( svg_base_edict_t *ent, const std::string callBackName, sol::state_view &stateView,
	RetType &returnValue,
	// WID: TODO: Do proper argument inspectations perhaps?
	//const int32_t numExpectedArgs = 0, const int32_t numReturnValues = 0,
	const Rest&... rest ) {

	bool calledFunction = false;
	
	// Generate function 'callback' name.
	if ( ent->luaProperties.luaName ) {
		const std::string luaFunctionName = std::string( ent->luaProperties.luaName.ptr ) + "_" + callBackName;
		calledFunction = LUA_CallFunction( 
			stateView, luaFunctionName,
			// WID: TODO: Do proper argument inspectations perhaps?
			//const int32_t numExpectedArgs = 0, const int32_t numReturnValues = 0,
			returnValue, LUA_CALLFUNCTION_VERBOSE_MISSING, rest ... );
	}

	// There was no luaName set, so we didn't know which function to callback into.
	return calledFunction;
}