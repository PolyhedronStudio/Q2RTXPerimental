/********************************************************************
*
*
*	ServerGmae: Lua Signal Utilities: -
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

//! For debugging purposes.
#define SVG_LUA_REPORT_SIGNAL_OUT 1

/**
*	@brief	Fires an 'Out' signal, calling and passing it over to the entity's luaName OnSignal function.
*	@return	True if the signal was processed.
*	@note	<discard_this>One has to deal and pop return values themselves.</discard_this>
**/
template <typename... Rest>
static const bool SVG_Lua_SignalOut( sol::state_view &stateView, edict_t *ent, edict_t *other, edict_t *activator, const char *signalName, const svg_signal_argument_array_t &signalArguments, const svg_lua_callfunction_verbosity_t verbosity = LUA_CALLFUNCTION_VERBOSE_MISSING, const Rest&... rest ) {
	// False by default:
	bool executedSuccessfully = false;

	// Entity has to be non (nullptr) and active(in use).
	if ( !SVG_IsValidLuaEntity( ent, true ) 
		// has to be non (nullptr) and active(in use).
		|| ( activator && !SVG_IsActiveEntity( activator ) ) 
		// Other is optional.
		//|| ( other && !SVG_IsActiveEntity( other ) )
		// And a valid Signal Name.
		|| !signalName ) {
		return executedSuccessfully;
	}

	// Generate function name.
	const std::string luaName = ent->luaProperties.luaName;
	const std::string functionName = luaName + "_OnSignalIn";

	// Get function object.
	sol::protected_function funcRefSignalOut = stateView[ functionName ];
	// Get type.
	sol::type funcRefType = funcRefSignalOut.get_type();
	// Ensure it matches, accordingly
	if ( funcRefType != sol::type::function /*|| !funcRefSignalOut.is<std::function<void( Rest... )>>() */) {
		// Return if it is LUA_NOREF and luaState == nullptr again.
		return false;
	}

	try {
		funcRefSignalOut( lua_edict_t(ent), lua_edict_t(other), lua_edict_t(activator), signalName, signalArguments, rest... );
	} catch ( std::exception &e ) {
		gi.bprintf( PRINT_ERROR, "%s: %s\n", __func__, e.what() );
		return false;
	}

	// Debug print.
	#ifdef SVG_LUA_REPORT_SIGNAL_OUT
	gi.dprintf( "luaName(%s): function(%s) fired signal(\"%s\")\n", luaName.c_str(), functionName.c_str(), signalName, ent->luaProperties.luaName );
	#endif

	return true;
}