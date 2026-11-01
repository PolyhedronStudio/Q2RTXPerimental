/********************************************************************
*
*
*	ClientGame / Engine UI: Q2RTXP Menu Script MicroUI Drop-In Template
*
*	Complete immediate-mode MicroUI translation of `baseq2rtxp/q2rtxp.menu`.
*	Contains all 29 script menus, widget helpers for every `.menu` directive
*	(`action`, `toggle`, `range`, `pairs`, `values`, `strings`, `field`,
*	`bind`, `savegame`, `loadgame`, `blank`, `ifeq`, `ifneq`), and a
*	self-contained menu stack manager (`pushmenu`, `popmenu`, `forcemenuoff`).
*
*
********************************************************************/
#if defined( CLGAME_INCLUDE )
// Local ClientGame includes.
#include "clgame/clg_local.h"
#include "clgame/clg_precache.h"
#include "clgame/clg_screen.h"
#include "clgame/game_ui/clg_ui_main.h"
#include "clgame/game_ui/menus/clg_ui_menu_q2rtxp.h"
#include "sharedgame/sg_game_ui.h"
#else
// Engine Client UI includes.
#include "ui.h"
#include "common/files.h"
#include "server/server.h"
#endif

// MicroUI immediate-mode UI library.
extern "C" {
#if __has_include( "clgame/game_ui/microui-2.02/src/microui.h" )
	#include "clgame/game_ui/microui-2.02/src/microui.h"
#elif __has_include( "../../baseq2rtxp/clgame/game_ui/microui-2.02/src/microui.h" )
	#include "../../baseq2rtxp/clgame/game_ui/microui-2.02/src/microui.h"
#else
	#include "microui.h"
#endif
}

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>



/**
*
*
*
*	Menu Identifiers & Data Structures:
*
*
*
**/
/**
*	@brief	Enumeration of all menus defined in `baseq2rtxp/q2rtxp.menu` plus built-in menu hooks.
**/
enum class mui_script_menu_id_t : int32_t {
	//! No active menu.
	NONE = 0,

	//! `begin developer_menu` - Developer (Test-)Maps.
	DEVELOPER_MENU,
	//! `begin environment` - Environment Settings.
	ENVIRONMENT,
	//! `begin resolution` - Dynamic Resolution Scaling.
	RESOLUTION,
	//! `begin hdr` - HDR Settings.
	HDR,
	//! `begin photomode` - Photo Mode Settings.
	PHOTOMODE,
	//! `begin video` - Video Setup.
	VIDEO,
	//! `begin options` - Options.
	OPTIONS,
	//! `begin sound` - Sound Setup.
	SOUND,
	//! `begin railtrail` - Railgun Trail Setup.
	RAILTRAIL,
	//! `begin effects` - Effects Setup.
	EFFECTS,
	//! `begin crosshair` - Crosshair Setup.
	CROSSHAIR,
	//! `begin screen` - Screen Setup.
	SCREEN,
	//! `begin downloads` - Download Options.
	DOWNLOADS,
	//! `begin gameflags` - Deathmatch Flags.
	GAMEFLAGS,
	//! `begin startserver` - Start Server.
	STARTSERVER,
	//! `begin multiplayer` - Multiplayer.
	MULTIPLAYER,
	//! `begin singleplayer` - Singleplayer.
	SINGLEPLAYER,
	//! `begin loadgame` - Load Game.
	LOADGAME,
	//! `begin savegame` - Save Game.
	SAVEGAME,
	//! `begin quit` - Quit Game?.
	QUIT,
	//! `begin main` - Q2RTXPerimental Main Menu.
	MAIN,
	//! `begin game` - In-Game Menu.
	GAME,
	//! `begin addressbook` - Address Book.
	ADDRESSBOOK,
	//! `begin input` - Input Setup.
	INPUT,
	//! `begin keys` - Action Key Bindings.
	KEYS,
	//! `begin keys_function` - Function Key Bindings.
	KEYS_FUNCTION,
	//! `begin keys_legacy` - Legacy Key Bindings.
	KEYS_LEGACY,
	//! `begin keys_developer` - Developer Key Bindings.
	KEYS_DEVELOPER,
	//! `begin weapons` - Weapon and Item Bindings.
	WEAPONS,

	//! Built-in hardcoded menu hook: Server Browser (`servers`).
	BUILTIN_SERVERS,
	//! Built-in hardcoded menu hook: Demos Browser (`demos`).
	BUILTIN_DEMOS,
	//! Built-in hardcoded menu hook: Player Config (`players`).
	BUILTIN_PLAYERS,

	//! Total number of menu identifiers.
	MAX_MENUS
};

/**
*	@brief	Key-value pair descriptor for `pairs <name> <cvar> <label1> <value1> [...]` controls.
**/
struct mui_pair_option_t {
	//! Display string shown in the spin control.
	const char *label = nullptr;
	//! CVar string value written when this option is selected.
	const char *value = nullptr;
};

/**
*	@brief	Retained text-editing state for a `field` control bound to a cvar.
**/
struct mui_field_buffer_t {
	//! Name of the bound cvar.
	std::string cvarName;
	//! Last known cvar string value used to detect external changes.
	std::string lastSyncedValue;
	//! Editable character buffer passed to `mu_textbox_ex`.
	char buffer[ 128 ] = {};
};

/**
*	@brief	Entry mapping a script menu name string to its enum identifier and render callback.
**/
struct mui_menu_registry_entry_t {
	//! Menu identifier enum.
	mui_script_menu_id_t id = mui_script_menu_id_t::NONE;
	//! Script name used by `begin <name>` and `pushmenu <name>`.
	const char *name = nullptr;
	//! Window title string from `title "<title>"`.
	const char *title = nullptr;
	//! Per-frame MicroUI build function for this menu.
	void ( *drawFunc )( mu_Context *ctx ) = nullptr;
};



/**
*
*
*
*	Static State & Theme Constants:
*
*
*
**/
//! Maximum depth of nested menus in the navigation stack.
static constexpr int32_t MUI_MAX_MENU_DEPTH = 16;

//! Default menu window width in pixels.
static constexpr int32_t MUI_DEFAULT_WINDOW_WIDTH = 620;

//! Maximum menu window height as a fraction of screen height.
static constexpr float MUI_MAX_WINDOW_HEIGHT_RATIO = 0.82f;

//! Standard control row height minimum in pixels.
static constexpr int32_t MUI_MIN_ROW_HEIGHT = 24;

//! Blank separator row height in pixels.
static constexpr int32_t MUI_BLANK_ROW_HEIGHT = 12;

//! HUD / Weapon Bar dark translucent amber background container fill (`COLOR_HUD_BG` / `COLOR_WEAPON_BAR_ORANGE_BG`).
static constexpr mu_Color MUI_HUD_COLOR_BG = { 35, 18, 8, 225 };

//! HUD / Weapon Bar dim translucent amber background (`COLOR_WEAPON_BAR_ORANGE_BG_DIM`).
static constexpr mu_Color MUI_HUD_COLOR_BG_DIM = { 25, 16, 10, 175 };

//! HUD / Weapon Bar alternating row dark tint (blended with Scoreboard row dark).
static constexpr mu_Color MUI_HUD_COLOR_ROW_DARK = { 42, 26, 16, 196 };

//! HUD / Weapon Bar alternating row light tint (blended with Scoreboard row light).
static constexpr mu_Color MUI_HUD_COLOR_ROW_LIGHT = { 56, 36, 22, 196 };

//! Primary HL1 / HUD base amber-orange (`COLOR_HUD_ORANGE_BASE`: #d27d2c).
static constexpr mu_Color MUI_HUD_COLOR_ORANGE_BASE = { 210, 125, 44, 220 };

//! Primary HL1 / Weapon Bar bright orange highlight (`COLOR_WEAPON_BAR_ORANGE_BRIGHT`).
static constexpr mu_Color MUI_HUD_COLOR_ORANGE_BRIGHT = { 255, 175, 75, 255 };

//! Primary HL1 / HUD vivid orange (`COLOR_HUD_ORANGE_BRIGHT`: #df7126).
static constexpr mu_Color MUI_HUD_COLOR_ORANGE_VIVID = { 223, 113, 38, 255 };

//! Scoreboard & GameUI warm orange stroke outline (`#FF905A`).
static constexpr mu_Color MUI_HUD_COLOR_STROKE = { 255, 144, 90, 235 };

//! HUD / Weapon Bar dim amber-orange for subtle strokes (`COLOR_HUD_ORANGE_DIM`).
static constexpr mu_Color MUI_HUD_COLOR_ORANGE_DIM = { 210, 125, 44, 140 };

//! Scoreboard & Weapon Bar button hover outer glow (`#FF7800`).
static constexpr mu_Color MUI_HUD_COLOR_GLOW_HOVER = { 255, 120, 0, 210 };

//! Scoreboard & Weapon Bar button focus/active outer glow (`#FFA040`).
static constexpr mu_Color MUI_HUD_COLOR_GLOW_FOCUS = { 255, 160, 64, 255 };

//! Weapon Bar unowned/disabled background fill (`COLOR_WEAPON_BAR_ORANGE_UNOWNED_BG`).
static constexpr mu_Color MUI_HUD_COLOR_DISABLED_BG = { 14, 10, 8, 120 };

//! Weapon Bar unowned/disabled border stroke (`COLOR_WEAPON_BAR_ORANGE_UNOWNED_STROKE`).
static constexpr mu_Color MUI_HUD_COLOR_DISABLED_STROKE = { 80, 50, 30, 80 };

//! Weapon Bar unowned/disabled text color (`COLOR_WEAPON_BAR_ORANGE_UNOWNED_TEXT`).
static constexpr mu_Color MUI_HUD_COLOR_DISABLED_TEXT = { 100, 70, 45, 140 };

//! Active menu navigation stack (`layers` equivalent).
static std::array<mui_script_menu_id_t, MUI_MAX_MENU_DEPTH> s_menuStack = {};

//! Current depth of the menu navigation stack.
static int32_t s_menuStackDepth = 0;

//! Status bar tooltip text for the control hovered or focused during the current frame.
static const char *s_currentStatusText = nullptr;

//! Running row index within the active menu window used for Scoreboard-style alternating row fills.
static int32_t s_currentMenuRowIndex = 0;

//! True while `CLG_MUI_ProcessQ2RTXPMenus` is actively building a MicroUI frame (`mu_begin` .. `mu_end`).
static bool s_isBuildingMenuFrame = false;

//! Command string currently awaiting a key press for a `bind` control (empty if not waiting).
static std::string s_keybindCaptureCmd;

//! Retained edit buffers for `field` controls so in-progress edits persist across frames.
static std::vector<mui_field_buffer_t> s_fieldBuffers;

//! Arguments passed to `pushmenu servers ...` when launching the server browser hook.
static std::string s_serverBrowserArgs;



/**
*
*
*
*	Backend Abstraction Layer (ClientGame `clgi` vs Engine `client`):
*
*
*
**/
/**
*	@brief	Retrieve current screen width in pixels.
*	@return	Screen width in pixels.
**/
static int32_t MUI_GetScreenWidth( void ) {
	/**
	*	Query the active screen dimensions from the appropriate module backend.
	**/
#if defined( CLGAME_INCLUDE )
	// Return ClientGame screen width if valid.
	if ( clgi.screen != nullptr && clgi.screen->screenWidth > 0 ) {
		return clgi.screen->screenWidth;
	}
	return 1280;
#else
	// Return Engine UI scaled width if initialized.
	if ( uis.width > 0 ) {
		return uis.width;
	}
	return 1280;
#endif
}

/**
*	@brief	Retrieve current screen height in pixels.
*	@return	Screen height in pixels.
**/
static int32_t MUI_GetScreenHeight( void ) {
	/**
	*	Query the active screen dimensions from the appropriate module backend.
	**/
#if defined( CLGAME_INCLUDE )
	// Return ClientGame screen height if valid.
	if ( clgi.screen != nullptr && clgi.screen->screenHeight > 0 ) {
		return clgi.screen->screenHeight;
	}
	return 720;
#else
	// Return Engine UI scaled height if initialized.
	if ( uis.height > 0 ) {
		return uis.height;
	}
	return 720;
#endif
}

/**
*	@brief	Fetch or weak-get a console variable by name.
*	@param	varName	Name of the cvar to look up.
*	@return	Pointer to the `cvar_t` instance, or `nullptr` if unavailable.
**/
static cvar_t *MUI_GetCvar( const char *varName ) {
	/**
	*	Sanity check the cvar name string.
	**/
	// Reject null or empty cvar names.
	if ( varName == nullptr || varName[ 0 ] == '\0' ) {
		return nullptr;
	}

	/**
	*	Resolve the cvar pointer through the active backend.
	**/
#if defined( CLGAME_INCLUDE )
	// Use ClientGame import table to weak-get or create the cvar reference.
	if ( clgi.CVar_WeakGet != nullptr ) {
		return clgi.CVar_WeakGet( varName );
	}
	if ( clgi.CVar_Get != nullptr ) {
		return clgi.CVar_Get( varName, "", 0 );
	}
	return nullptr;
#else
	// Use Engine Cvar_WeakGet directly.
	return Cvar_WeakGet( varName );
#endif
}

/**
*	@brief	Read a cvar's current string value safely.
*	@param	varName		Name of the cvar to read.
*	@param	fallback	Fallback string returned if the cvar does not exist.
*	@return	Current string value of the cvar.
**/
static const char *MUI_GetCvarString( const char *varName, const char *fallback = "" ) {
	/**
	*	Look up the cvar and return its string buffer if valid.
	**/
	cvar_t *var = MUI_GetCvar( varName );
	// Ensure the cvar and its string pointer are valid before returning.
	if ( var != nullptr && var->string != nullptr ) {
		return var->string;
	}
	return fallback;
}

/**
*	@brief	Read a cvar's current integer value safely.
*	@param	varName		Name of the cvar to read.
*	@param	fallback	Fallback integer returned if the cvar does not exist.
*	@return	Current integer value of the cvar.
**/
static int32_t MUI_GetCvarInteger( const char *varName, const int32_t fallback = 0 ) {
	/**
	*	Look up the cvar and return its integer field if valid.
	**/
	cvar_t *var = MUI_GetCvar( varName );
	// Return the cached integer value when present.
	if ( var != nullptr && var->string != nullptr ) {
		return var->integer;
	}
	return fallback;
}

/**
*	@brief	Read a cvar's current floating-point value safely.
*	@param	varName		Name of the cvar to read.
*	@param	fallback	Fallback float returned if the cvar does not exist.
*	@return	Current float value of the cvar.
**/
static float MUI_GetCvarFloat( const char *varName, const float fallback = 0.0f ) {
	/**
	*	Look up the cvar and return its float value field if valid.
	**/
	cvar_t *var = MUI_GetCvar( varName );
	// Return the cached float value when present.
	if ( var != nullptr && var->string != nullptr ) {
		return var->value;
	}
	return fallback;
}

/**
*	@brief	Write a string value to a cvar by name.
*	@param	varName	Target cvar name.
*	@param	value	New string value to assign.
**/
static void MUI_SetCvarString( const char *varName, const char *value ) {
	/**
	*	Validate inputs before mutating cvar state.
	**/
	// Ignore invalid cvar names or null value pointers.
	if ( varName == nullptr || varName[ 0 ] == '\0' || value == nullptr ) {
		return;
	}

	/**
	*	Assign the string value via the active backend.
	**/
#if defined( CLGAME_INCLUDE )
	// Set the cvar string through the ClientGame import API.
	if ( clgi.CVar_Set != nullptr ) {
		clgi.CVar_Set( varName, value );
	}
#else
	// Set the cvar string with FROM_MENU origin in the engine.
	cvar_t *var = Cvar_WeakGet( varName );
	if ( var != nullptr ) {
		Cvar_SetByVar( var, value, FROM_MENU );
	}
#endif
}

/**
*	@brief	Write an integer value to a cvar by name.
*	@param	varName	Target cvar name.
*	@param	value	New integer value to assign.
**/
static void MUI_SetCvarInteger( const char *varName, const int32_t value ) {
	/**
	*	Validate cvar name before updating.
	**/
	// Ignore null or empty cvar names.
	if ( varName == nullptr || varName[ 0 ] == '\0' ) {
		return;
	}

	/**
	*	Assign the integer value via the active backend.
	**/
#if defined( CLGAME_INCLUDE )
	// Use CVar_SetInteger if available, otherwise format as string.
	cvar_t *var = MUI_GetCvar( varName );
	if ( var != nullptr && clgi.CVar_SetInteger != nullptr ) {
		clgi.CVar_SetInteger( var, value, FROM_MENU );
	} else {
		char buf[ 32 ] = {};
		std::snprintf( buf, sizeof( buf ), "%d", value );
		MUI_SetCvarString( varName, buf );
	}
#else
	// Use Engine Cvar_SetInteger with FROM_MENU origin.
	cvar_t *var = Cvar_WeakGet( varName );
	if ( var != nullptr ) {
		Cvar_SetInteger( var, value, FROM_MENU );
	}
#endif
}

/**
*	@brief	Write a floating-point value to a cvar by name.
*	@param	varName	Target cvar name.
*	@param	value	New float value to assign.
**/
static void MUI_SetCvarFloat( const char *varName, const float value ) {
	/**
	*	Validate cvar name before updating.
	**/
	// Ignore null or empty cvar names.
	if ( varName == nullptr || varName[ 0 ] == '\0' ) {
		return;
	}

	/**
	*	Assign the float value via the active backend.
	**/
#if defined( CLGAME_INCLUDE )
	// Format the float cleanly and assign through CVar_Set.
	char buf[ 64 ] = {};
	std::snprintf( buf, sizeof( buf ), "%.6g", static_cast< double >( value ) );
	MUI_SetCvarString( varName, buf );
#else
	// Use Engine Cvar_SetValue with FROM_MENU origin.
	cvar_t *var = Cvar_WeakGet( varName );
	if ( var != nullptr ) {
		Cvar_SetValue( var, value, FROM_MENU );
	}
#endif
}

/**
*	@brief	Evaluate an `ifeq` / `ifneq` condition from `q2rtxp.menu`.
*	@param	cvarName		Name of the cvar to inspect.
*	@param	expectedValue	Integer value to compare against.
*	@param	checkEquals		True for `ifeq`, false for `ifneq`.
*	@return	True if the condition is satisfied.
**/
static bool MUI_CheckCondition( const char *cvarName, const int32_t expectedValue, const bool checkEquals ) {
	/**
	*	Compare the cvar's integer value against expectedValue.
	**/
	const int32_t actualValue = MUI_GetCvarInteger( cvarName, 0 );
	const bool isEqual = ( actualValue == expectedValue );
	return checkEquals ? isEqual : !isEqual;
}

/**
*	@brief	Expand an engine macro or cvar string (e.g. `$$vid_modelist`, `$vid_displaylist`, `$$com_maplist`)
*			into individual whitespace/quoted tokens.
*	@param	identName	Macro or cvar name without leading `$` symbols.
*	@param	outTokens	[out] Vector populated with parsed token strings.
**/
static void MUI_ExpandTokens( const char *identName, std::vector<std::string> &outTokens ) {
	/**
	*	Sanity check identifier string.
	**/
	outTokens.clear();
	// Return early if no identifier name was provided.
	if ( identName == nullptr || identName[ 0 ] == '\0' ) {
		return;
	}

	/**
	*	Expand from a registered command macro first, or fallback to a cvar string.
	**/
	char macroBuf[ MAX_STRING_CHARS ] = {};
	const char *rawData = nullptr;

#if defined( CLGAME_INCLUDE )
	// Check if a command macro matches identName via ClientGame imports.
	cmd_macro_t *macro = ( clgi.Cmd_FindMacro != nullptr ) ? clgi.Cmd_FindMacro( identName ) : nullptr;
#else
	// Check if a command macro matches identName in Engine.
	cmd_macro_t *macro = Cmd_FindMacro( identName );
#endif
	// If a macro was found, execute it into macroBuf; otherwise look up the cvar string.
	if ( macro != nullptr && macro->function != nullptr ) {
		macro->function( macroBuf, sizeof( macroBuf ) );
		rawData = macroBuf;
	} else {
		cvar_t *var = MUI_GetCvar( identName );
		// Only expand non-private cvars.
		if ( var != nullptr && !( var->flags & CVAR_PRIVATE ) ) {
			rawData = var->string;
		}
	}

	// Bail out if neither a macro nor a cvar produced text.
	if ( rawData == nullptr || rawData[ 0 ] == '\0' ) {
		return;
	}

	/**
	*	Tokenize the expanded string using COM_Parse, matching `add_expand` in `script.cpp`.
	**/
	const char *parsePtr = rawData;
	// Loop over all tokens in the expanded buffer until exhausted.
	while ( parsePtr != nullptr ) {
		const char *tok = COM_Parse( &parsePtr );
		// Stop once COM_Parse signals the end of the buffer.
		if ( parsePtr == nullptr ) {
			break;
		}
		outTokens.emplace_back( tok );
	}
}

/**
*	@brief	Format the current key binding display string for a command (matching `Keybind_Push`).
*	@param	cmd			Command string to search bindings for.
*	@param	outBuffer	[out] Destination character buffer.
*	@param	bufferSize	Capacity of `outBuffer` in bytes.
**/
static void MUI_GetKeybindDisplayString( const char *cmd, char *outBuffer, const size_t bufferSize ) {
	/**
	*	Validate output buffer and command string.
	**/
	// Ensure we have a valid output buffer.
	if ( outBuffer == nullptr || bufferSize == 0 ) {
		return;
	}
	outBuffer[ 0 ] = '\0';

	// Default to "???" if the command is empty.
	if ( cmd == nullptr || cmd[ 0 ] == '\0' ) {
		Q_strlcpy( outBuffer, "???", bufferSize );
		return;
	}

	/**
	*	Query primary and secondary keybindings for the command.
	**/
#if defined( CLGAME_INCLUDE )
	// In ClientGame, query the primary binding string via clgi.Key_GetBinding.
	if ( clgi.Key_GetBinding != nullptr ) {
		const char *boundKey = clgi.Key_GetBinding( cmd );
		if ( boundKey != nullptr && boundKey[ 0 ] != '\0' ) {
			Q_strlcpy( outBuffer, boundKey, bufferSize );
			return;
		}
	}
	Q_strlcpy( outBuffer, "???", bufferSize );
#else
	// In Engine UI, enumerate up to two bound keys using Key_EnumBindings.
	const int32_t firstKey = Key_EnumBindings( 0, cmd );
	if ( firstKey == -1 ) {
		Q_strlcpy( outBuffer, "???", bufferSize );
	} else {
		const char *firstKeyName = Key_KeynumToString( firstKey );
		const int32_t secondKey = Key_EnumBindings( firstKey + 1, cmd );
		// If a second key is also bound, display "Key1 or Key2".
		if ( secondKey != -1 ) {
			const char *secondKeyName = Key_KeynumToString( secondKey );
			std::snprintf( outBuffer, bufferSize, "%s or %s", firstKeyName, secondKeyName );
		} else {
			Q_strlcpy( outBuffer, firstKeyName, bufferSize );
		}
	}
#endif
}

/**
*	@brief	Remove all existing keybindings for a command string.
*	@param	cmd	Command string whose keybindings should be cleared.
**/
static void MUI_ClearKeybindingsForCommand( const char *cmd ) {
	/**
	*	Sanity check command string.
	**/
	// Ignore null or empty commands.
	if ( cmd == nullptr || cmd[ 0 ] == '\0' ) {
		return;
	}

	/**
	*	Unbind all keys mapped to this command.
	**/
#if defined( CLGAME_INCLUDE )
	// In ClientGame, dispatch an unbindaction command to the local command buffer.
	if ( clgi.Cbuf_AddText != nullptr ) {
		char buf[ 256 ] = {};
		std::snprintf( buf, sizeof( buf ), "unbindaction \"%s\"\n", cmd );
		clgi.Cbuf_AddText( buf );
	} else if ( clgi.CL_ClientCommand != nullptr ) {
		char buf[ 256 ] = {};
		std::snprintf( buf, sizeof( buf ), "unbindaction \"%s\"", cmd );
		clgi.CL_ClientCommand( buf );
	}
#else
	// In Engine UI, walk all bindings via Key_EnumBindings and clear each one.
	for ( int32_t key = 0; ; key++ ) {
		key = Key_EnumBindings( key, cmd );
		// Stop when no more keys are bound to cmd.
		if ( key == -1 ) {
			break;
		}
		Key_SetBinding( key, nullptr );
	}
#endif
}

/**
*	@brief	Format the label for a savegame or loadgame slot (`Savegame_Push` equivalent).
*	@param	slotDir		Save slot directory name (e.g. `"save0"`, `"quick"`, `"save1"`).
*	@param	outLabel	[out] Formatted display string for the slot button.
*	@param	outIsEmpty	[out] True if the save slot currently has no save data.
**/
static void MUI_GetSavegameSlotLabel( const char *slotDir, std::string &outLabel, bool &outIsEmpty ) {
	/**
	*	Initialize defaults for an empty save slot.
	**/
	outLabel = "<EMPTY>";
	outIsEmpty = true;

	// Guard against null slot directory strings.
	if ( slotDir == nullptr || slotDir[ 0 ] == '\0' ) {
		return;
	}

	/**
	*	Query the server save metadata if running in Engine context, or check filesystem.
	**/
#if !defined( CLGAME_INCLUDE )
	// Query SV_GetSaveInfo exactly like Savegame_Push in src/client/ui/menu.cpp.
	char *info = SV_GetSaveInfo( slotDir );
	if ( info != nullptr ) {
		outLabel = info;
		outIsEmpty = false;
		Z_Free( info );
	}
#else
	// In ClientGame context, check if the save file exists on disk.
	if ( clgi.FS_FileExistsEx != nullptr ) {
		char savePath[ MAX_QPATH ] = {};
		std::snprintf( savePath, sizeof( savePath ), "save/%s/server.ssv", slotDir );
		if ( clgi.FS_FileExistsEx( savePath, 0 ) != 0 ) {
			outLabel = slotDir;
			outIsEmpty = false;
		}
	}
#endif

	/**
	*	Append "Auto Save" or "Quick Save" suffixes for special slots `save0` and `quick`.
	**/
	const bool isAutoSave = ( std::strcmp( slotDir, "save0" ) == 0 );
	const bool isQuickSave = ( std::strcmp( slotDir, "quick" ) == 0 );
	// Append suffix when slot is `save0` or `quick`.
	if ( isAutoSave || isQuickSave ) {
		outLabel += isAutoSave ? " - Auto Save" : " - Quick Save";
	}
}



/**
*
*
*
*	Forward Declarations for Menu Navigation & All 29 Menu Functions:
*
*
*
**/
void MUI_PushMenu( const mui_script_menu_id_t menuId );
bool MUI_PushMenuByName( const char *menuName );
void MUI_PopMenu( void );
void MUI_ForceMenuOff( void );
void MUI_ExecuteCommand( const char *cmdString );

static void MUI_Menu_Developer( mu_Context *ctx );
static void MUI_Menu_Environment( mu_Context *ctx );
static void MUI_Menu_Resolution( mu_Context *ctx );
static void MUI_Menu_HDR( mu_Context *ctx );
static void MUI_Menu_PhotoMode( mu_Context *ctx );
static void MUI_Menu_Video( mu_Context *ctx );
static void MUI_Menu_Options( mu_Context *ctx );
static void MUI_Menu_Sound( mu_Context *ctx );
static void MUI_Menu_RailTrail( mu_Context *ctx );
static void MUI_Menu_Effects( mu_Context *ctx );
static void MUI_Menu_Crosshair( mu_Context *ctx );
static void MUI_Menu_Screen( mu_Context *ctx );
static void MUI_Menu_Downloads( mu_Context *ctx );
static void MUI_Menu_GameFlags( mu_Context *ctx );
static void MUI_Menu_StartServer( mu_Context *ctx );
static void MUI_Menu_Multiplayer( mu_Context *ctx );
static void MUI_Menu_Singleplayer( mu_Context *ctx );
static void MUI_Menu_LoadGame( mu_Context *ctx );
static void MUI_Menu_SaveGame( mu_Context *ctx );
static void MUI_Menu_Quit( mu_Context *ctx );
static void MUI_Menu_Main( mu_Context *ctx );
static void MUI_Menu_Game( mu_Context *ctx );
static void MUI_Menu_AddressBook( mu_Context *ctx );
static void MUI_Menu_Input( mu_Context *ctx );
static void MUI_Menu_Keys( mu_Context *ctx );
static void MUI_Menu_KeysFunction( mu_Context *ctx );
static void MUI_Menu_KeysLegacy( mu_Context *ctx );
static void MUI_Menu_KeysDeveloper( mu_Context *ctx );
static void MUI_Menu_Weapons( mu_Context *ctx );
static void MUI_Menu_BuiltinPlaceholder( mu_Context *ctx );

//! Master registry table mapping all `q2rtxp.menu` menu names to their MicroUI callbacks.
static const mui_menu_registry_entry_t s_menuRegistry[] = {
	{ mui_script_menu_id_t::DEVELOPER_MENU, "developer_menu", "Developer (Test-)Maps",      MUI_Menu_Developer },
	{ mui_script_menu_id_t::ENVIRONMENT,    "environment",    "Environment Settings",       MUI_Menu_Environment },
	{ mui_script_menu_id_t::RESOLUTION,     "resolution",     "Dynamic Resolution Scaling", MUI_Menu_Resolution },
	{ mui_script_menu_id_t::HDR,            "hdr",            "HDR Settings",               MUI_Menu_HDR },
	{ mui_script_menu_id_t::PHOTOMODE,      "photomode",      "Photo Mode Settings",        MUI_Menu_PhotoMode },
	{ mui_script_menu_id_t::VIDEO,          "video",          "Video Setup",                MUI_Menu_Video },
	{ mui_script_menu_id_t::OPTIONS,        "options",        "Options",                    MUI_Menu_Options },
	{ mui_script_menu_id_t::SOUND,          "sound",          "Sound Setup",                MUI_Menu_Sound },
	{ mui_script_menu_id_t::RAILTRAIL,      "railtrail",      "Railgun Trail Setup",        MUI_Menu_RailTrail },
	{ mui_script_menu_id_t::EFFECTS,        "effects",        "Effects Setup",              MUI_Menu_Effects },
	{ mui_script_menu_id_t::CROSSHAIR,      "crosshair",      "Crosshair Setup",            MUI_Menu_Crosshair },
	{ mui_script_menu_id_t::SCREEN,         "screen",         "Screen Setup",               MUI_Menu_Screen },
	{ mui_script_menu_id_t::DOWNLOADS,      "downloads",      "Download Options",           MUI_Menu_Downloads },
	{ mui_script_menu_id_t::GAMEFLAGS,      "gameflags",      "Deathmatch Flags",           MUI_Menu_GameFlags },
	{ mui_script_menu_id_t::STARTSERVER,    "startserver",    "Start Server",               MUI_Menu_StartServer },
	{ mui_script_menu_id_t::MULTIPLAYER,    "multiplayer",    "Multiplayer",                MUI_Menu_Multiplayer },
	{ mui_script_menu_id_t::SINGLEPLAYER,   "singleplayer",   "Singleplayer",               MUI_Menu_Singleplayer },
	{ mui_script_menu_id_t::LOADGAME,       "loadgame",       "Load Game",                  MUI_Menu_LoadGame },
	{ mui_script_menu_id_t::SAVEGAME,       "savegame",       "Save Game",                  MUI_Menu_SaveGame },
	{ mui_script_menu_id_t::QUIT,           "quit",           "Quit Game?",                 MUI_Menu_Quit },
	{ mui_script_menu_id_t::MAIN,           "main",           "Q2RTXPerimental",            MUI_Menu_Main },
	{ mui_script_menu_id_t::GAME,           "game",           "In-Game Menu",               MUI_Menu_Game },
	{ mui_script_menu_id_t::ADDRESSBOOK,    "addressbook",    "Address Book",               MUI_Menu_AddressBook },
	{ mui_script_menu_id_t::INPUT,          "input",          "Input Setup",                MUI_Menu_Input },
	{ mui_script_menu_id_t::KEYS,           "keys",           "Action Key Bindings",        MUI_Menu_Keys },
	{ mui_script_menu_id_t::KEYS_FUNCTION,  "keys_function",  "Function Key Bindings",      MUI_Menu_KeysFunction },
	{ mui_script_menu_id_t::KEYS_LEGACY,    "keys_legacy",    "Legacy Key Bindings",        MUI_Menu_KeysLegacy },
	{ mui_script_menu_id_t::KEYS_DEVELOPER, "keys_developer", "Developer Key Bindings",     MUI_Menu_KeysDeveloper },
	{ mui_script_menu_id_t::WEAPONS,        "weapons",        "Weapon and Item Bindings",   MUI_Menu_Weapons },
	{ mui_script_menu_id_t::BUILTIN_SERVERS,"servers",        "Server Browser",             MUI_Menu_BuiltinPlaceholder },
	{ mui_script_menu_id_t::BUILTIN_DEMOS,  "demos",          "Browse Demos",               MUI_Menu_BuiltinPlaceholder },
	{ mui_script_menu_id_t::BUILTIN_PLAYERS,"players",        "Player Setup",               MUI_Menu_BuiltinPlaceholder },
};



/**
*
*
*
*	Menu Stack & Command Execution Implementation:
*
*
*
**/
/**
*	@brief	Look up a menu registry entry by its identifier enum.
*	@param	menuId	Menu identifier to search for.
*	@return	Pointer to the matching registry entry, or `nullptr` if not found.
**/
static const mui_menu_registry_entry_t *MUI_FindMenuEntryById( const mui_script_menu_id_t menuId ) {
	/**
	*	Linearly scan the static menu registry table.
	**/
	// Check each entry in s_menuRegistry for a matching ID.
	for ( const mui_menu_registry_entry_t &entry : s_menuRegistry ) {
		if ( entry.id == menuId ) {
			return &entry;
		}
	}
	return nullptr;
}

/**
*	@brief	Look up a menu registry entry by its script name (`begin <name>`).
*	@param	menuName	Null-terminated script menu name.
*	@return	Pointer to the matching registry entry, or `nullptr` if not found.
**/
static const mui_menu_registry_entry_t *MUI_FindMenuEntryByName( const char *menuName ) {
	/**
	*	Validate input name string.
	**/
	// Reject null or empty menu names.
	if ( menuName == nullptr || menuName[ 0 ] == '\0' ) {
		return nullptr;
	}

	/**
	*	Scan the registry for a case-insensitive match.
	**/
	// Check each entry in s_menuRegistry.
	for ( const mui_menu_registry_entry_t &entry : s_menuRegistry ) {
		if ( entry.name != nullptr && Q_stricmp( entry.name, menuName ) == 0 ) {
			return &entry;
		}
	}
	return nullptr;
}

/**
*	@brief	Push a menu onto the MicroUI menu stack, unwinding if already present in the stack.
*	@param	menuId	Identifier of the menu to push.
**/
void MUI_PushMenu( const mui_script_menu_id_t menuId ) {
	/**
	*	Validate menu identifier.
	**/
	// Ignore invalid menu IDs.
	if ( menuId <= mui_script_menu_id_t::NONE || menuId >= mui_script_menu_id_t::MAX_MENUS ) {
		return;
	}

	/**
	*	Clear any active keybind capture state when switching menus.
	**/
	s_keybindCaptureCmd.clear();

	/**
	*	If this menu is already on the stack, drop back to its level (matching `UI_PushMenu`).
	**/
	// Search existing stack layers for menuId.
	for ( int32_t i = 0; i < s_menuStackDepth; i++ ) {
		if ( s_menuStack[ i ] == menuId ) {
			s_menuStackDepth = i + 1;
			return;
		}
	}

	/**
	*	Push the new menu onto the stack if capacity permits.
	**/
	// Append to stack if below MUI_MAX_MENU_DEPTH.
	if ( s_menuStackDepth < MUI_MAX_MENU_DEPTH ) {
		s_menuStack[ s_menuStackDepth++ ] = menuId;
	}
}

/**
*	@brief	Push a menu onto the stack by its `.menu` script name (e.g. `"video"`, `"options"`).
*	@param	menuName	Name of the menu to push.
*	@return	True if the menu was found and pushed.
**/
bool MUI_PushMenuByName( const char *menuName ) {
	/**
	*	Look up the menu entry by script name and push it.
	**/
	const mui_menu_registry_entry_t *entry = MUI_FindMenuEntryByName( menuName );
	// Return false if no menu with that name exists.
	if ( entry == nullptr ) {
		return false;
	}

	MUI_PushMenu( entry->id );
	return true;
}

/**
*	@brief	Pop the top-most menu from the stack, closing the menu system when depth reaches zero.
**/
void MUI_PopMenu( void ) {
	/**
	*	Cancel any active keybind capture before popping.
	**/
	s_keybindCaptureCmd.clear();

	/**
	*	Decrement stack depth and close all menus when the root menu is popped.
	**/
	// Pop one layer if stack is non-empty.
	if ( s_menuStackDepth > 0 ) {
		s_menuStackDepth--;
	}
	// If no menus remain on the stack, force menu off.
	if ( s_menuStackDepth <= 0 ) {
		MUI_ForceMenuOff();
	}
}

/**
*	@brief	Close all open MicroUI script menus and return focus to gameplay.
**/
void MUI_ForceMenuOff( void ) {
	/**
	*	Reset stack depth and transient editing state.
	**/
	s_menuStackDepth = 0;
	s_keybindCaptureCmd.clear();
	s_currentStatusText = nullptr;

#if defined( CLGAME_INCLUDE )
	// Only call CLG_UI_CloseMenu immediately when outside `mu_begin`..`mu_end`;
	// during a frame, `__process_frame` closes the menu cleanly after `mu_end( ctx )`.
	if ( !s_isBuildingMenuFrame ) {
		CLG_UI_CloseMenu();
	}
#else
	// Notify Engine UI manager to clear KEY_MENU destination.
	UI_ForceMenuOff();
#endif
}

/**
*	@brief	Execute a menu command string, intercepting `pushmenu`, `popmenu`, and `forcemenuoff`
*			for local MicroUI menu stack navigation before forwarding console commands.
*	@param	cmdString	Command string from an `action`, `savegame`, or `loadgame` item.
**/
void MUI_ExecuteCommand( const char *cmdString ) {
	/**
	*	Sanity check command string and ignore `_ignore`.
	**/
	// Ignore null, empty, or "_ignore" sentinel commands.
	if ( cmdString == nullptr || cmdString[ 0 ] == '\0' || std::strcmp( cmdString, "_ignore" ) == 0 ) {
		return;
	}

	/**
	*	Check if the command is a direct `pushmenu <name> [args]`, `popmenu`, or `forcemenuoff`.
	**/
	// Skip leading whitespace.
	const char *p = cmdString;
	while ( *p == ' ' || *p == '\t' ) {
		p++;
	}

	// Intercept `popmenu`.
	if ( Q_stricmpn( p, "popmenu", 7 ) == 0 && ( p[ 7 ] == '\0' || p[ 7 ] == ' ' || p[ 7 ] == ';' ) ) {
		MUI_PopMenu();
		return;
	}

	// Intercept `forcemenuoff` when standalone or at the start of a compound command.
	if ( Q_stricmpn( p, "forcemenuoff", 12 ) == 0 && ( p[ 12 ] == '\0' || p[ 12 ] == ' ' ) ) {
		MUI_ForceMenuOff();
		return;
	}

	// Intercept `pushmenu <menuname> [optional_args]`.
	if ( Q_stricmpn( p, "pushmenu", 8 ) == 0 && ( p[ 8 ] == ' ' || p[ 8 ] == '\t' ) ) {
		const char *argPtr = p + 8;
		const char *targetMenu = COM_Parse( &argPtr );
		// Store any trailing arguments (e.g. server browser URLs).
		if ( argPtr != nullptr ) {
			while ( *argPtr == ' ' || *argPtr == '\t' ) {
				argPtr++;
			}
			s_serverBrowserArgs = argPtr;
		} else {
			s_serverBrowserArgs.clear();
		}

		// Push the target menu onto our MicroUI stack.
		if ( targetMenu != nullptr && targetMenu[ 0 ] != '\0' ) {
			MUI_PushMenuByName( targetMenu );
		}
		return;
	}

	/**
	*	 Check if a compound command starts with `forcemenuoff;` (such as `startserver`'s begin game!).
	**/
	if ( Q_stricmpn( p, "forcemenuoff;", 13 ) == 0 ) {
		MUI_ForceMenuOff();
	}

	/**
	*	Forward the command string to the engine command buffer.
	**/
#if defined( CLGAME_INCLUDE )
	// In ClientGame, append the command string and newline to the local command buffer.
	if ( clgi.Cbuf_AddText != nullptr ) {
		clgi.Cbuf_AddText( cmdString );
		clgi.Cbuf_AddText( "\n" );
	} else if ( clgi.CL_ClientCommand != nullptr ) {
		clgi.CL_ClientCommand( cmdString );
	}
#else
	// In Engine UI, append the command string and a newline to cmd_buffer.
	Cbuf_AddText( &cmd_buffer, cmdString );
	Cbuf_AddText( &cmd_buffer, "\n" );
#endif
}



/**
*
*
*
*	MicroUI Theme & Window Frame Helpers:
*
*
*
**/
/**
*	@brief	Apply the ClientGame HUD, Weapon Bar HUD, and Scoreboard MicroUI styling to the MicroUI context.
*	@param	ctx			Active MicroUI context.
*	@param	windowTitle	Title of the window about to be begun.
**/
static void MUI_ApplyMenuTheme( mu_Context *ctx, const char *windowTitle ) {
	/**
	*	Sanity check MicroUI context.
	**/
	// Return early if context or style is null.
	if ( ctx == nullptr || ctx->style == nullptr ) {
		return;
	}

	/**
	*	Populate base MicroUI style colors matching `clg_ui_menu_scoreboard.cpp`, `clg_hud.cpp`,
	*	and `clg_hud_weaponmenu.cpp`.
	**/
	ctx->style->colors[ MU_COLOR_WINDOWBG ]         = MUI_HUD_COLOR_BG;
	ctx->style->colors[ MU_COLOR_PANELBG ]          = mu_color( 0, 0, 0, 0 );
	ctx->style->colors[ MU_COLOR_TEXT ]             = mu_color( 230, 230, 230, 255 );
	ctx->style->colors[ MU_COLOR_TITLEBG_ACTIVE ]   = mu_color( 255, 120, 0, 255 );
	ctx->style->colors[ MU_COLOR_TITLEBG_INACTIVE ] = mu_color( 255, 100, 0, 128 );
	ctx->style->colors[ MU_COLOR_TITLETEXT ]        = mu_color( 255, 255, 255, 255 );
	ctx->style->colors[ MU_COLOR_BORDER ]           = MUI_HUD_COLOR_STROKE;
	ctx->style->colors[ MU_COLOR_BUTTON ]           = mu_color( 255, 145, 90, 156 );
	ctx->style->colors[ MU_COLOR_BUTTONHOVER ]      = mu_color( 255, 147, 54, 210 );
	ctx->style->colors[ MU_COLOR_BUTTONFOCUS ]      = mu_color( 255, 120, 48, 210 );
	ctx->style->colors[ MU_COLOR_BUTTONDISABLED ]   = MUI_HUD_COLOR_DISABLED_BG;
	ctx->style->colors[ MU_COLOR_TEXTDISABLED ]     = MUI_HUD_COLOR_DISABLED_TEXT;
	ctx->style->colors[ MU_COLOR_BASE ]             = MUI_HUD_COLOR_BG_DIM;
	ctx->style->colors[ MU_COLOR_BASEHOVER ]        = mu_color( 255, 126, 90, 64 );
	ctx->style->colors[ MU_COLOR_BASEFOCUS ]        = mu_color( 165, 118, 76, 120 );
	ctx->style->colors[ MU_COLOR_SCROLLBASE ]       = mu_color( 170, 110, 58, 160 );
	ctx->style->colors[ MU_COLOR_SCROLLTHUMB ]      = mu_color( 255, 120, 0, 180 );

	/**
	*	Apply rounded corners, outer shadow glow, and warm orange border stroke to the window container
	*	(matching `clg_ui_menu_scoreboard.cpp`).
	**/
	const int32_t windowCornerRadii[ 4 ] = { 8, 8, 8, 8 };
	const mu_Color windowGlowColor = mu_color( 0, 0, 0, 191 );
	if ( windowTitle != nullptr && windowTitle[ 0 ] != '\0' ) {
		mu_set_window_style( ctx, windowTitle, windowGlowColor, 16, windowCornerRadii, MUI_HUD_COLOR_STROKE, 1 );
	}

	/**
	*	Configure multi-state styling for buttons (`MU_CONTROL_BUTTON`) modeled after the Weapon Bar HUD
	*	and Scoreboard interactive controls.
	**/
	mu_ControlStyle btnStyle = {};
	btnStyle.glow_margin = 4;
	btnStyle.has_style = 1;

	// Normal button state: dark HUD amber base with warm orange stroke and bright orange-white text.
	btnStyle.states[ MU_STATE_NORMAL ].bg_color = MUI_HUD_COLOR_BG_DIM;
	btnStyle.states[ MU_STATE_NORMAL ].text_color = MUI_HUD_COLOR_ORANGE_BRIGHT;
	btnStyle.states[ MU_STATE_NORMAL ].stroke_color = MUI_HUD_COLOR_STROKE;
	btnStyle.states[ MU_STATE_NORMAL ].stroke_thickness = 1;
	btnStyle.states[ MU_STATE_NORMAL ].corner_radii[ 0 ] = 4;
	btnStyle.states[ MU_STATE_NORMAL ].corner_radii[ 1 ] = 4;
	btnStyle.states[ MU_STATE_NORMAL ].corner_radii[ 2 ] = 4;
	btnStyle.states[ MU_STATE_NORMAL ].corner_radii[ 3 ] = 4;
	btnStyle.states[ MU_STATE_NORMAL ].has_style = 1;

	// Hover button state: vivid HUD orange fill with outer orange glow.
	btnStyle.states[ MU_STATE_HOVER ].bg_color = mu_color( 255, 147, 54, 210 );
	btnStyle.states[ MU_STATE_HOVER ].text_color = mu_color( 255, 255, 255, 255 );
	btnStyle.states[ MU_STATE_HOVER ].stroke_color = MUI_HUD_COLOR_ORANGE_BRIGHT;
	btnStyle.states[ MU_STATE_HOVER ].stroke_thickness = 1;
	btnStyle.states[ MU_STATE_HOVER ].glow_color = MUI_HUD_COLOR_GLOW_HOVER;
	btnStyle.states[ MU_STATE_HOVER ].glow_radius = 6;
	btnStyle.states[ MU_STATE_HOVER ].corner_radii[ 0 ] = 4;
	btnStyle.states[ MU_STATE_HOVER ].corner_radii[ 1 ] = 4;
	btnStyle.states[ MU_STATE_HOVER ].corner_radii[ 2 ] = 4;
	btnStyle.states[ MU_STATE_HOVER ].corner_radii[ 3 ] = 4;
	btnStyle.states[ MU_STATE_HOVER ].has_style = 1;

	// Focused / pressed button state: deep active orange fill with bright highlight glow.
	btnStyle.states[ MU_STATE_FOCUS ].bg_color = mu_color( 255, 120, 48, 230 );
	btnStyle.states[ MU_STATE_FOCUS ].text_color = mu_color( 255, 255, 255, 255 );
	btnStyle.states[ MU_STATE_FOCUS ].stroke_color = mu_color( 255, 210, 140, 255 );
	btnStyle.states[ MU_STATE_FOCUS ].stroke_thickness = 1;
	btnStyle.states[ MU_STATE_FOCUS ].glow_color = MUI_HUD_COLOR_GLOW_FOCUS;
	btnStyle.states[ MU_STATE_FOCUS ].glow_radius = 6;
	btnStyle.states[ MU_STATE_FOCUS ].corner_radii[ 0 ] = 4;
	btnStyle.states[ MU_STATE_FOCUS ].corner_radii[ 1 ] = 4;
	btnStyle.states[ MU_STATE_FOCUS ].corner_radii[ 2 ] = 4;
	btnStyle.states[ MU_STATE_FOCUS ].corner_radii[ 3 ] = 4;
	btnStyle.states[ MU_STATE_FOCUS ].has_style = 1;

	// Disabled button state: muted dark weapon-bar disabled slot styling.
	btnStyle.states[ MU_STATE_DISABLED ].bg_color = MUI_HUD_COLOR_DISABLED_BG;
	btnStyle.states[ MU_STATE_DISABLED ].text_color = MUI_HUD_COLOR_DISABLED_TEXT;
	btnStyle.states[ MU_STATE_DISABLED ].stroke_color = MUI_HUD_COLOR_DISABLED_STROKE;
	btnStyle.states[ MU_STATE_DISABLED ].stroke_thickness = 1;
	btnStyle.states[ MU_STATE_DISABLED ].corner_radii[ 0 ] = 4;
	btnStyle.states[ MU_STATE_DISABLED ].corner_radii[ 1 ] = 4;
	btnStyle.states[ MU_STATE_DISABLED ].corner_radii[ 2 ] = 4;
	btnStyle.states[ MU_STATE_DISABLED ].corner_radii[ 3 ] = 4;
	btnStyle.states[ MU_STATE_DISABLED ].has_style = 1;

	mu_set_control_style( ctx, MU_CONTROL_BUTTON, &btnStyle );

	/**
	*	Configure multi-state styling for checkboxes (`MU_CONTROL_CHECKBOX`) matching `clg_ui_menu_scoreboard.cpp`.
	**/
	mu_ControlStyle chkStyle = {};
	chkStyle.glow_margin = 4;
	chkStyle.has_style = 1;

	// Normal checkbox state.
	chkStyle.states[ MU_STATE_NORMAL ].bg_color = MUI_HUD_COLOR_BG_DIM;
	chkStyle.states[ MU_STATE_NORMAL ].text_color = ctx->style->colors[ MU_COLOR_TEXT ];
	chkStyle.states[ MU_STATE_NORMAL ].stroke_color = MUI_HUD_COLOR_STROKE;
	chkStyle.states[ MU_STATE_NORMAL ].stroke_thickness = 1;
	chkStyle.states[ MU_STATE_NORMAL ].corner_radii[ 0 ] = 3;
	chkStyle.states[ MU_STATE_NORMAL ].corner_radii[ 1 ] = 3;
	chkStyle.states[ MU_STATE_NORMAL ].corner_radii[ 2 ] = 3;
	chkStyle.states[ MU_STATE_NORMAL ].corner_radii[ 3 ] = 3;
	chkStyle.states[ MU_STATE_NORMAL ].has_style = 1;

	// Hover checkbox state.
	chkStyle.states[ MU_STATE_HOVER ].bg_color = ctx->style->colors[ MU_COLOR_BASEHOVER ];
	chkStyle.states[ MU_STATE_HOVER ].text_color = mu_color( 255, 255, 255, 255 );
	chkStyle.states[ MU_STATE_HOVER ].stroke_color = MUI_HUD_COLOR_ORANGE_BRIGHT;
	chkStyle.states[ MU_STATE_HOVER ].stroke_thickness = 1;
	chkStyle.states[ MU_STATE_HOVER ].glow_color = MUI_HUD_COLOR_GLOW_HOVER;
	chkStyle.states[ MU_STATE_HOVER ].glow_radius = 6;
	chkStyle.states[ MU_STATE_HOVER ].corner_radii[ 0 ] = 3;
	chkStyle.states[ MU_STATE_HOVER ].corner_radii[ 1 ] = 3;
	chkStyle.states[ MU_STATE_HOVER ].corner_radii[ 2 ] = 3;
	chkStyle.states[ MU_STATE_HOVER ].corner_radii[ 3 ] = 3;
	chkStyle.states[ MU_STATE_HOVER ].has_style = 1;

	// Focus checkbox state.
	chkStyle.states[ MU_STATE_FOCUS ].bg_color = ctx->style->colors[ MU_COLOR_BASEFOCUS ];
	chkStyle.states[ MU_STATE_FOCUS ].text_color = mu_color( 255, 255, 255, 255 );
	chkStyle.states[ MU_STATE_FOCUS ].stroke_color = MUI_HUD_COLOR_ORANGE_BRIGHT;
	chkStyle.states[ MU_STATE_FOCUS ].stroke_thickness = 1;
	chkStyle.states[ MU_STATE_FOCUS ].glow_color = MUI_HUD_COLOR_GLOW_FOCUS;
	chkStyle.states[ MU_STATE_FOCUS ].glow_radius = 6;
	chkStyle.states[ MU_STATE_FOCUS ].corner_radii[ 0 ] = 3;
	chkStyle.states[ MU_STATE_FOCUS ].corner_radii[ 1 ] = 3;
	chkStyle.states[ MU_STATE_FOCUS ].corner_radii[ 2 ] = 3;
	chkStyle.states[ MU_STATE_FOCUS ].corner_radii[ 3 ] = 3;
	chkStyle.states[ MU_STATE_FOCUS ].has_style = 1;

	// Disabled checkbox state.
	chkStyle.states[ MU_STATE_DISABLED ].bg_color = MUI_HUD_COLOR_DISABLED_BG;
	chkStyle.states[ MU_STATE_DISABLED ].text_color = MUI_HUD_COLOR_DISABLED_TEXT;
	chkStyle.states[ MU_STATE_DISABLED ].stroke_color = MUI_HUD_COLOR_DISABLED_STROKE;
	chkStyle.states[ MU_STATE_DISABLED ].stroke_thickness = 1;
	chkStyle.states[ MU_STATE_DISABLED ].corner_radii[ 0 ] = 3;
	chkStyle.states[ MU_STATE_DISABLED ].corner_radii[ 1 ] = 3;
	chkStyle.states[ MU_STATE_DISABLED ].corner_radii[ 2 ] = 3;
	chkStyle.states[ MU_STATE_DISABLED ].corner_radii[ 3 ] = 3;
	chkStyle.states[ MU_STATE_DISABLED ].has_style = 1;

	mu_set_control_style( ctx, MU_CONTROL_CHECKBOX, &chkStyle );

	/**
	*	Configure multi-state styling for textboxes (`MU_CONTROL_TEXTBOX`) and slider tracks (`MU_CONTROL_SLIDER_BASE`).
	**/
	mu_ControlStyle baseStyle = {};
	baseStyle.glow_margin = 3;
	baseStyle.has_style = 1;

	// Normal base track / textbox state.
	baseStyle.states[ MU_STATE_NORMAL ].bg_color = MUI_HUD_COLOR_BG_DIM;
	baseStyle.states[ MU_STATE_NORMAL ].text_color = ctx->style->colors[ MU_COLOR_TEXT ];
	baseStyle.states[ MU_STATE_NORMAL ].stroke_color = MUI_HUD_COLOR_STROKE;
	baseStyle.states[ MU_STATE_NORMAL ].stroke_thickness = 1;
	baseStyle.states[ MU_STATE_NORMAL ].corner_radii[ 0 ] = 4;
	baseStyle.states[ MU_STATE_NORMAL ].corner_radii[ 1 ] = 4;
	baseStyle.states[ MU_STATE_NORMAL ].corner_radii[ 2 ] = 4;
	baseStyle.states[ MU_STATE_NORMAL ].corner_radii[ 3 ] = 4;
	baseStyle.states[ MU_STATE_NORMAL ].has_style = 1;

	// Hover base track / textbox state.
	baseStyle.states[ MU_STATE_HOVER ].bg_color = mu_color( 50, 28, 14, 210 );
	baseStyle.states[ MU_STATE_HOVER ].text_color = mu_color( 255, 255, 255, 255 );
	baseStyle.states[ MU_STATE_HOVER ].stroke_color = MUI_HUD_COLOR_ORANGE_BRIGHT;
	baseStyle.states[ MU_STATE_HOVER ].stroke_thickness = 1;
	baseStyle.states[ MU_STATE_HOVER ].glow_color = MUI_HUD_COLOR_GLOW_HOVER;
	baseStyle.states[ MU_STATE_HOVER ].glow_radius = 5;
	baseStyle.states[ MU_STATE_HOVER ].corner_radii[ 0 ] = 4;
	baseStyle.states[ MU_STATE_HOVER ].corner_radii[ 1 ] = 4;
	baseStyle.states[ MU_STATE_HOVER ].corner_radii[ 2 ] = 4;
	baseStyle.states[ MU_STATE_HOVER ].corner_radii[ 3 ] = 4;
	baseStyle.states[ MU_STATE_HOVER ].has_style = 1;

	// Focus base track / textbox state.
	baseStyle.states[ MU_STATE_FOCUS ].bg_color = mu_color( 68, 36, 16, 225 );
	baseStyle.states[ MU_STATE_FOCUS ].text_color = mu_color( 255, 255, 255, 255 );
	baseStyle.states[ MU_STATE_FOCUS ].stroke_color = mu_color( 255, 210, 140, 255 );
	baseStyle.states[ MU_STATE_FOCUS ].stroke_thickness = 1;
	baseStyle.states[ MU_STATE_FOCUS ].glow_color = MUI_HUD_COLOR_GLOW_FOCUS;
	baseStyle.states[ MU_STATE_FOCUS ].glow_radius = 6;
	baseStyle.states[ MU_STATE_FOCUS ].corner_radii[ 0 ] = 4;
	baseStyle.states[ MU_STATE_FOCUS ].corner_radii[ 1 ] = 4;
	baseStyle.states[ MU_STATE_FOCUS ].corner_radii[ 2 ] = 4;
	baseStyle.states[ MU_STATE_FOCUS ].corner_radii[ 3 ] = 4;
	baseStyle.states[ MU_STATE_FOCUS ].has_style = 1;

	// Disabled base track / textbox state.
	baseStyle.states[ MU_STATE_DISABLED ].bg_color = MUI_HUD_COLOR_DISABLED_BG;
	baseStyle.states[ MU_STATE_DISABLED ].text_color = MUI_HUD_COLOR_DISABLED_TEXT;
	baseStyle.states[ MU_STATE_DISABLED ].stroke_color = MUI_HUD_COLOR_DISABLED_STROKE;
	baseStyle.states[ MU_STATE_DISABLED ].stroke_thickness = 1;
	baseStyle.states[ MU_STATE_DISABLED ].corner_radii[ 0 ] = 4;
	baseStyle.states[ MU_STATE_DISABLED ].corner_radii[ 1 ] = 4;
	baseStyle.states[ MU_STATE_DISABLED ].corner_radii[ 2 ] = 4;
	baseStyle.states[ MU_STATE_DISABLED ].corner_radii[ 3 ] = 4;
	baseStyle.states[ MU_STATE_DISABLED ].has_style = 1;

	mu_set_control_style( ctx, MU_CONTROL_TEXTBOX, &baseStyle );
	mu_set_control_style( ctx, MU_CONTROL_SLIDER_BASE, &baseStyle );

	/**
	*	Configure multi-state styling for slider thumbs (`MU_CONTROL_SLIDER_THUMB`).
	**/
	mu_ControlStyle thumbStyle = {};
	thumbStyle.glow_margin = 0;
	thumbStyle.has_style = 1;

	// Normal slider thumb.
	thumbStyle.states[ MU_STATE_NORMAL ].bg_color = MUI_HUD_COLOR_ORANGE_VIVID;
	thumbStyle.states[ MU_STATE_NORMAL ].stroke_color = MUI_HUD_COLOR_ORANGE_BRIGHT;
	thumbStyle.states[ MU_STATE_NORMAL ].stroke_thickness = 1;
	thumbStyle.states[ MU_STATE_NORMAL ].corner_radii[ 0 ] = 3;
	thumbStyle.states[ MU_STATE_NORMAL ].corner_radii[ 1 ] = 3;
	thumbStyle.states[ MU_STATE_NORMAL ].corner_radii[ 2 ] = 3;
	thumbStyle.states[ MU_STATE_NORMAL ].corner_radii[ 3 ] = 3;
	thumbStyle.states[ MU_STATE_NORMAL ].has_style = 1;

	// Hover slider thumb.
	thumbStyle.states[ MU_STATE_HOVER ].bg_color = MUI_HUD_COLOR_ORANGE_BRIGHT;
	thumbStyle.states[ MU_STATE_HOVER ].stroke_color = mu_color( 255, 255, 255, 255 );
	thumbStyle.states[ MU_STATE_HOVER ].stroke_thickness = 1;
	thumbStyle.states[ MU_STATE_HOVER ].corner_radii[ 0 ] = 3;
	thumbStyle.states[ MU_STATE_HOVER ].corner_radii[ 1 ] = 3;
	thumbStyle.states[ MU_STATE_HOVER ].corner_radii[ 2 ] = 3;
	thumbStyle.states[ MU_STATE_HOVER ].corner_radii[ 3 ] = 3;
	thumbStyle.states[ MU_STATE_HOVER ].has_style = 1;

	// Focus slider thumb.
	thumbStyle.states[ MU_STATE_FOCUS ].bg_color = mu_color( 255, 200, 110, 255 );
	thumbStyle.states[ MU_STATE_FOCUS ].stroke_color = mu_color( 255, 255, 255, 255 );
	thumbStyle.states[ MU_STATE_FOCUS ].stroke_thickness = 1;
	thumbStyle.states[ MU_STATE_FOCUS ].corner_radii[ 0 ] = 3;
	thumbStyle.states[ MU_STATE_FOCUS ].corner_radii[ 1 ] = 3;
	thumbStyle.states[ MU_STATE_FOCUS ].corner_radii[ 2 ] = 3;
	thumbStyle.states[ MU_STATE_FOCUS ].corner_radii[ 3 ] = 3;
	thumbStyle.states[ MU_STATE_FOCUS ].has_style = 1;

	mu_set_control_style( ctx, MU_CONTROL_SLIDER_THUMB, &thumbStyle );
}

/**
*	@brief	Calculate standard control row height for the active font metrics.
*	@param	ctx	Active MicroUI context.
*	@return	Row height in pixels.
**/
static int32_t MUI_GetRowHeight( mu_Context *ctx ) {
	/**
	*	Compute row height from TTF font height plus vertical padding.
	**/
	const int32_t fontH = ( ctx != nullptr && ctx->text_height != nullptr )
		? ctx->text_height( ctx->style->font )
		: 18;
	const int32_t pad = ( ctx != nullptr && ctx->style != nullptr ) ? ctx->style->padding : 4;
	return std::max( MUI_MIN_ROW_HEIGHT, fontH + ( pad * 2 ) + 4 );
}

/**
*	@brief	Record the status bar tooltip text if the most recently laid-out control is hovered or focused.
*	@param	ctx		Active MicroUI context.
*	@param	rect	Bounding rectangle of the control row or widget.
*	@param	status	Tooltip string from `--status` / `-s`.
**/
static void MUI_TrackControlStatus( mu_Context *ctx, const mu_Rect &rect, const char *status ) {
	/**
	*	Update `s_currentStatusText` when the cursor is over `rect`.
	**/
	// Ignore controls without a status string.
	if ( status == nullptr || status[ 0 ] == '\0' ) {
		return;
	}
	// If the mouse is over the control rectangle, display its status text in the footer.
	if ( mu_mouse_over( ctx, rect ) != 0 ) {
		s_currentStatusText = status;
	}
}

/**
*	@brief	Draw a Scoreboard-style alternating row background and optional vertical column separator
*			behind a setting row (`clg_ui_menu_scoreboard.cpp`).
*	@param	ctx			Active MicroUI context.
*	@param	labelRect	Bounding rectangle of the left-column label cell.
*	@param	drawSep		True to draw the 1px vertical orange separator on the right edge of `labelRect`.
**/
static void MUI_DrawSettingRowBackdrop( mu_Context *ctx, const mu_Rect &labelRect, const bool drawSep = true ) {
	/**
	*	Fetch the active panel container and paint an alternating dark/light row band across its body.
	**/
	mu_Container *cnt = mu_get_current_container( ctx );
	const mu_Color rowBg = ( ( s_currentMenuRowIndex++ & 1 ) != 0 )
		? MUI_HUD_COLOR_ROW_LIGHT
		: MUI_HUD_COLOR_ROW_DARK;

	if ( cnt != nullptr && cnt->body.w > 0 ) {
		const mu_Rect fullRowRect = mu_rect( cnt->body.x + 2, labelRect.y, std::max( 0, cnt->body.w - 4 ), labelRect.h );
		mu_draw_rect( ctx, fullRowRect, rowBg );
	} else {
		mu_draw_rect( ctx, labelRect, rowBg );
	}

	/**
	*	Draw a 1-pixel vertical orange column separator between the label column and the control column.
	**/
	if ( drawSep ) {
		const int32_t sepX = labelRect.x + labelRect.w;
		mu_draw_rect( ctx, mu_rect( sepX, labelRect.y + 2, 1, std::max( 0, labelRect.h - 4 ) ), MUI_HUD_COLOR_STROKE );
	}
}

/**
*	@brief	Render a left-column setting label inside a Scoreboard-styled alternating row cell.
*	@param	ctx		Active MicroUI context.
*	@param	label	Setting label text.
*	@param	status	Optional `--status` tooltip string.
**/
static void MUI_DrawSettingRowLabel( mu_Context *ctx, const char *label, const char *status ) {
	/**
	*	Acquire the label cell rectangle, draw the alternating row backdrop + vertical separator,
	*	and render the label text in the HUD warm orange-white color.
	**/
	const mu_Rect labelRect = mu_layout_next( ctx );
	MUI_DrawSettingRowBackdrop( ctx, labelRect, true );
	if ( label != nullptr && label[ 0 ] != '\0' ) {
		mu_draw_control_text( ctx, label, labelRect, MU_COLOR_TEXT, 0 );
	}
	MUI_TrackControlStatus( ctx, labelRect, status );
}

/**
*	@brief	Begin a standardized centered menu window with a scrollable content panel.
*	@param	ctx				Active MicroUI context.
*	@param	title			Window title string (`title "..."`).
*	@param	estimatedRows	Approximate number of rows used to size compact windows.
*	@return	True if the window is open and ready for widget emission.
**/
static bool MUI_BeginMenuWindow( mu_Context *ctx, const char *title, const int32_t estimatedRows ) {
	/**
	*	Reset the per-frame hovered status text, alternating row counter, and apply the HUD/Scoreboard theme.
	**/
	s_currentStatusText = nullptr;
	s_currentMenuRowIndex = 0;
	MUI_ApplyMenuTheme( ctx, title );

	/**
	*	Calculate centered window dimensions clamped to screen height.
	**/
	const int32_t screenW = MUI_GetScreenWidth();
	const int32_t screenH = MUI_GetScreenHeight();
	const int32_t rowH = MUI_GetRowHeight( ctx );
	const int32_t spacing = ctx->style->spacing;
	const int32_t padding = ctx->style->padding;
	const bool hasBackRow = ( s_menuStackDepth > 1 );

	// Reserve space for the content rows, optional Back button row, and status bar.
	const int32_t footerRows = hasBackRow ? 2 : 1;
	const int32_t totalRows = std::max( 4, estimatedRows + footerRows );
	const int32_t desiredBodyH = ( totalRows * ( rowH + spacing ) ) + ( padding * 4 );
	const int32_t maxWinH = std::max( 280, static_cast< int32_t >( screenH * MUI_MAX_WINDOW_HEIGHT_RATIO ) );
	const int32_t winW = std::min( MUI_DEFAULT_WINDOW_WIDTH, std::max( 360, screenW - 64 ) );
	const int32_t winH = std::min( maxWinH, desiredBodyH + ctx->style->title_height );
	const int32_t winX = ( screenW - winW ) / 2;
	const int32_t winY = ( screenH - winH ) / 2;

	/**
	*	Open the MicroUI window and an inner frameless scrollable body panel (`MU_OPT_NOFRAME`).
	**/
	const int32_t winOpt = MU_OPT_ALIGNCENTER | MU_OPT_NOCLOSE | MU_OPT_HOLDFOCUS |
		MU_OPT_KEEPFOCUS | MU_OPT_NODRAG | MU_OPT_NORESIZE | MU_OPT_NOSCROLL;

	// Begin the outer menu window.
	if ( mu_begin_window_ex( ctx, title, mu_rect( winX, winY, winW, winH ), winOpt ) == 0 ) {
		return false;
	}

	// Pin the window rectangle to stay centered if screen resolution changes.
	mu_Container *win = mu_get_current_container( ctx );
	if ( win != nullptr ) {
		win->rect.x = winX;
		win->rect.y = winY;
		win->rect.w = winW;
		win->rect.h = winH;
	}

	// Reserve bottom space for the status tooltip line (and Back button row when in a submenu).
	const int32_t footerReserveH = hasBackRow
		? -( ( rowH * 2 ) + ( spacing * 3 ) + 4 )
		: -( rowH + ( spacing * 2 ) + 4 );
	const int32_t panelWidths[ 1 ] = { -1 };
	mu_layout_row( ctx, 1, panelWidths, footerReserveH );
	mu_begin_panel_ex( ctx, title, MU_OPT_NOFRAME );

	return true;
}

/**
*	@brief	End the scrollable menu body panel, draw the status tooltip bar and optional Back button,
*			and close the MicroUI window scope.
*	@param	ctx	Active MicroUI context.
**/
static void MUI_EndMenuWindow( mu_Context *ctx ) {
	/**
	*	Close the inner scrollable body panel first.
	**/
	mu_end_panel( ctx );

	const int32_t rowH = MUI_GetRowHeight( ctx );

	/**
	*	Draw a horizontal orange divider separating the body panel from the footer area
	*	(matching `clg_ui_menu_scoreboard.cpp`).
	**/
	const int32_t sepWidths[ 1 ] = { -1 };
	mu_layout_row( ctx, 1, sepWidths, 2 );
	const mu_Rect footerSepRect = mu_layout_next( ctx );
	mu_draw_rect( ctx, mu_rect( footerSepRect.x, footerSepRect.y, footerSepRect.w, 1 ), MUI_HUD_COLOR_STROKE );

	/**
	*	Render the status bar tooltip row (`--status` / `-s`) in a HUD dark amber box.
	**/
	const int32_t statusWidths[ 1 ] = { -1 };
	mu_layout_row( ctx, 1, statusWidths, rowH - 4 );
	const mu_Rect statusRect = mu_layout_next( ctx );
	mu_draw_rect( ctx, statusRect, MUI_HUD_COLOR_BG_DIM );
	if ( s_currentStatusText != nullptr && s_currentStatusText[ 0 ] != '\0' ) {
		mu_draw_control_text( ctx, s_currentStatusText, statusRect, MU_COLOR_TEXT, MU_OPT_ALIGNCENTER );
	}

	/**
	*	Render the Back button row when inside a nested submenu (`s_menuStackDepth > 1`).
	**/
	if ( s_menuStackDepth > 1 ) {
		const int32_t navWidths[ 1 ] = { -1 };
		mu_layout_row( ctx, 1, navWidths, rowH );
		// Pop one menu level when Back is clicked.
		if ( mu_button_ex( ctx, "< Back", 0, MU_OPT_ALIGNCENTER ) != 0 ) {
			MUI_PopMenu();
		}
	}

	/**
	*	End the outer MicroUI window scope.
	**/
	mu_end_window( ctx );
}



/**
*
*
*
*	MicroUI Widget Builders for `.menu` Directives:
*
*
*
**/
/**
*	@brief	Emit a `blank` separator row (`MTYPE_SEPARATOR`).
*	@param	ctx	Active MicroUI context.
**/
static void MUI_Widget_Blank( mu_Context *ctx ) {
	/**
	*	Configure a compact vertical gap row with a Scoreboard-style orange horizontal divider line.
	**/
	const int32_t widths[ 1 ] = { -1 };
	mu_layout_row( ctx, 1, widths, MUI_BLANK_ROW_HEIGHT );
	const mu_Rect rect = mu_layout_next( ctx );
	const int32_t lineY = rect.y + ( rect.h / 2 );
	mu_draw_rect( ctx, mu_rect( rect.x + 4, lineY, std::max( 0, rect.w - 8 ), 1 ), mu_color( 255, 144, 90, 90 ) );
}

/**
*	@brief	Emit an `action` button (`MTYPE_ACTION`).
*	@param	ctx			Active MicroUI context.
*	@param	label		Button display label.
*	@param	cmd			Command string executed when clicked.
*	@param	alignLeft	True when `--align` (`-a`) was specified in `.menu`.
*	@param	status		Optional `--status` tooltip string.
**/
static void MUI_Widget_Action(
	mu_Context *ctx,
	const char *label,
	const char *cmd,
	const bool alignLeft = false,
	const char *status = nullptr )
{
	/**
	*	Push a unique scope ID combining label and command so duplicate labels in a menu do not collide.
	**/
	mu_push_id( ctx, cmd, static_cast< int32_t >( std::strlen( cmd ) ) );

	/**
	*	Lay out a full-width action button and execute `cmd` on click.
	**/
	const int32_t widths[ 1 ] = { -1 };
	mu_layout_row( ctx, 1, widths, MUI_GetRowHeight( ctx ) );

	const int32_t opt = alignLeft ? 0 : MU_OPT_ALIGNCENTER;
	const int32_t res = mu_button_ex( ctx, label, 0, opt );
	MUI_TrackControlStatus( ctx, ctx->last_rect, status );

	// Execute the associated command when the button is clicked.
	if ( ( res & MU_RES_SUBMIT ) != 0 ) {
		MUI_ExecuteCommand( cmd );
	}

	mu_pop_id( ctx );
}

/**
*	@brief	Emit a `toggle` control (`MTYPE_TOGGLE` or `MTYPE_BITFIELD`).
*	@param	ctx			Active MicroUI context.
*	@param	label		Control label displayed in the left column.
*	@param	cvarName	Target cvar name.
*	@param	negate		True when `~` prefix was specified on the bit/toggle.
*	@param	bitIndex	Bit index (`0..31`) for bitfield toggles, or `-1` for standard boolean toggles.
*	@param	status		Optional `--status` tooltip string.
**/
static void MUI_Widget_Toggle(
	mu_Context *ctx,
	const char *label,
	const char *cvarName,
	const bool negate = false,
	const int32_t bitIndex = -1,
	const char *status = nullptr )
{
	/**
	*	Push a unique ID scope for this toggle control.
	**/
	mu_push_id( ctx, label, static_cast< int32_t >( std::strlen( label ) ) );

	/**
	*	Read current cvar state and evaluate boolean/bitfield toggle state.
	**/
	const int32_t rawInt = MUI_GetCvarInteger( cvarName, 0 );
	int32_t isChecked = 0;

	// Check bitfield mask when bitIndex >= 0, otherwise treat as 0/1 boolean toggle.
	if ( bitIndex >= 0 && bitIndex < 32 ) {
		const int32_t mask = ( 1 << bitIndex );
		const int32_t bitSet = ( ( rawInt & mask ) != 0 ) ? 1 : 0;
		isChecked = bitSet ^ ( negate ? 1 : 0 );
	} else {
		const int32_t boolSet = ( rawInt != 0 ) ? 1 : 0;
		isChecked = boolSet ^ ( negate ? 1 : 0 );
	}

	/**
	*	Render two-column Scoreboard-styled row: left label + separator, and right checkbox.
	**/
	const int32_t widths[ 2 ] = { -220, -1 };
	mu_layout_row( ctx, 2, widths, MUI_GetRowHeight( ctx ) );

	MUI_DrawSettingRowLabel( ctx, label, status );

	const int32_t res = mu_checkbox( ctx, isChecked ? "yes" : "no", &isChecked );
	MUI_TrackControlStatus( ctx, ctx->last_rect, status );

	/**
	*	Write updated state back to the cvar when toggled.
	**/
	// Commit new value to cvarName if the checkbox state changed.
	if ( ( res & MU_RES_CHANGE ) != 0 ) {
		if ( bitIndex >= 0 && bitIndex < 32 ) {
			const int32_t mask = ( 1 << bitIndex );
			const int32_t wantBitSet = isChecked ^ ( negate ? 1 : 0 );
			const int32_t newRawInt = wantBitSet ? ( rawInt | mask ) : ( rawInt & ~mask );
			MUI_SetCvarInteger( cvarName, newRawInt );
		} else {
			const int32_t newBoolVal = isChecked ^ ( negate ? 1 : 0 );
			MUI_SetCvarInteger( cvarName, newBoolVal );
		}
	}

	mu_pop_id( ctx );
}

/**
*	@brief	Emit a `range` slider control (`MTYPE_SLIDER`).
*	@param	ctx				Active MicroUI context.
*	@param	label			Control label displayed in the left column.
*	@param	cvarName		Target cvar name.
*	@param	minVal			Minimum cvar value (may be greater than `maxVal` for inverted sliders).
*	@param	maxVal			Maximum cvar value.
*	@param	step			Quantization step (`0.0f` for automatic `(max - min) / 10`).
*	@param	fmt				Optional printf format string (`-f`).
*	@param	isPercentage	True when `-p` (`--percentage`) was specified in `.menu`.
*	@param	status			Optional `--status` tooltip string.
**/
static void MUI_Widget_Range(
	mu_Context *ctx,
	const char *label,
	const char *cvarName,
	const float minVal,
	const float maxVal,
	const float step = 0.0f,
	const char *fmt = nullptr,
	const bool isPercentage = false,
	const char *status = nullptr )
{
	/**
	*	Push a unique ID scope for this range control.
	**/
	mu_push_id( ctx, cvarName, static_cast< int32_t >( std::strlen( cvarName ) ) );

	/**
	*	Read and clamp the current cvar float value, handling both normal and inverted ranges
	*	(such as `vid_gamma 1.3 0.3` and `gl_picmip 3 0 -1`).
	**/
	const bool isInverted = ( minVal > maxVal );
	const float lowBound = isInverted ? maxVal : minVal;
	const float highBound = isInverted ? minVal : maxVal;
	const float absStep = ( step != 0.0f )
		? std::fabs( step )
		: ( ( highBound - lowBound ) / 10.0f );

	float curVal = MUI_GetCvarFloat( cvarName, minVal );
	curVal = std::clamp( curVal, lowBound, highBound );

	/**
	*	Map to display space (`* 100.0f` if `-p` percentage flag is set).
	**/
	const float displayScale = isPercentage ? 100.0f : 1.0f;
	float sliderVal = isInverted ? ( highBound - ( curVal - lowBound ) ) : curVal;
	float displayVal = sliderVal * displayScale;
	const float displayLow = lowBound * displayScale;
	const float displayHigh = highBound * displayScale;
	const float displayStep = absStep * displayScale;
	const char *effectiveFmt = ( fmt != nullptr && fmt[ 0 ] != '\0' ) ? fmt : "%.1f";

	/**
	*	Render two-column Scoreboard-styled row: left label + separator, and right slider.
	**/
	const int32_t widths[ 2 ] = { -220, -1 };
	mu_layout_row( ctx, 2, widths, MUI_GetRowHeight( ctx ) );

	MUI_DrawSettingRowLabel( ctx, label, status );

	const int32_t res = mu_slider_ex(
		ctx,
		&displayVal,
		displayLow,
		displayHigh,
		displayStep,
		effectiveFmt,
		MU_OPT_ALIGNCENTER
	);
	MUI_TrackControlStatus( ctx, ctx->last_rect, status );

	/**
	*	Convert back from display space and update the cvar if the slider value changed.
	**/
	// Commit the modified float value back to cvarName.
	if ( ( res & MU_RES_CHANGE ) != 0 ) {
		float newSliderVal = displayVal / displayScale;
		newSliderVal = std::clamp( newSliderVal, lowBound, highBound );
		const float newCvarVal = isInverted ? ( highBound - ( newSliderVal - lowBound ) ) : newSliderVal;
		MUI_SetCvarFloat( cvarName, newCvarVal );
	}

	mu_pop_id( ctx );
}

/**
*	@brief	Internal helper to render a 3-part spin control (`[ < ] [ Current Option ] [ > ]`).
*	@param	ctx				Active MicroUI context.
*	@param	label			Left-column control label.
*	@param	currentDisplay	Text of the currently selected option.
*	@param	status			Optional `--status` tooltip string.
*	@return	Direction delta (`-1` for previous, `+1` for next, `0` for no change).
**/
static int32_t MUI_DrawSpinRow(
	mu_Context *ctx,
	const char *label,
	const char *currentDisplay,
	const char *status )
{
	/**
	*	Lay out a 4-column Scoreboard-styled row: `[ Label | < | Current Value | > ]`.
	**/
	const int32_t widths[ 4 ] = { -220, 32, -36, -1 };
	mu_layout_row( ctx, 4, widths, MUI_GetRowHeight( ctx ) );

	MUI_DrawSettingRowLabel( ctx, label, status );

	int32_t delta = 0;

	// Left arrow button steps backward.
	if ( mu_button_ex( ctx, "<", 0, MU_OPT_ALIGNCENTER ) != 0 ) {
		delta = -1;
	}
	MUI_TrackControlStatus( ctx, ctx->last_rect, status );

	// Center button displays the current option and cycles forward when clicked.
	const char *shownText = ( currentDisplay != nullptr && currentDisplay[ 0 ] != '\0' ) ? currentDisplay : "???";
	if ( mu_button_ex( ctx, shownText, 0, MU_OPT_ALIGNCENTER ) != 0 ) {
		delta = 1;
	}
	MUI_TrackControlStatus( ctx, ctx->last_rect, status );

	// Right arrow button steps forward.
	if ( mu_button_ex( ctx, ">", 0, MU_OPT_ALIGNCENTER ) != 0 ) {
		delta = 1;
	}
	MUI_TrackControlStatus( ctx, ctx->last_rect, status );

	return delta;
}

/**
*	@brief	Emit a `pairs` spin control (`MTYPE_PAIRS`) mapping display labels to cvar string values.
*	@param	ctx			Active MicroUI context.
*	@param	label		Control label displayed in the left column.
*	@param	cvarName	Target cvar name.
*	@param	options		Array of `(label, value)` pairs.
*	@param	optionCount	Number of elements in `options`.
*	@param	status		Optional `--status` tooltip string.
**/
static void MUI_Widget_Pairs(
	mu_Context *ctx,
	const char *label,
	const char *cvarName,
	const mui_pair_option_t *options,
	const int32_t optionCount,
	const char *status = nullptr )
{
	/**
	*	Validate option array.
	**/
	// Bail out if there are no options to display.
	if ( options == nullptr || optionCount <= 0 ) {
		return;
	}

	mu_push_id( ctx, label, static_cast< int32_t >( std::strlen( label ) ) );

	/**
	*	Locate the currently active pair index by matching the cvar's string or float value.
	**/
	const char *curStr = MUI_GetCvarString( cvarName, "" );
	const float curFloat = MUI_GetCvarFloat( cvarName, 0.0f );
	int32_t curIndex = -1;

	// Search for an exact case-insensitive string match first.
	for ( int32_t i = 0; i < optionCount; i++ ) {
		if ( options[ i ].value != nullptr && Q_stricmp( options[ i ].value, curStr ) == 0 ) {
			curIndex = i;
			break;
		}
	}
	// Fallback to numeric comparison if the cvar is formatted differently (e.g. "0.500000" vs "0.5").
	if ( curIndex < 0 && curStr[ 0 ] != '\0' ) {
		for ( int32_t i = 0; i < optionCount; i++ ) {
			if ( options[ i ].value != nullptr ) {
				char *endPtr = nullptr;
				const float optVal = std::strtof( options[ i ].value, &endPtr );
				if ( endPtr != options[ i ].value && std::fabs( optVal - curFloat ) < 0.0001f ) {
					curIndex = i;
					break;
				}
			}
		}
	}

	/**
	*	Render the spin control row and cycle the selected pair when clicked.
	**/
	const char *currentLabel = ( curIndex >= 0 && curIndex < optionCount ) ? options[ curIndex ].label : "???";
	const int32_t delta = MUI_DrawSpinRow( ctx, label, currentLabel, status );

	// Apply index change and update the cvar when the user interacts with the spin control.
	if ( delta != 0 ) {
		int32_t nextIndex = ( curIndex < 0 ) ? 0 : ( curIndex + delta );
		if ( nextIndex < 0 ) {
			nextIndex = optionCount - 1;
		} else if ( nextIndex >= optionCount ) {
			nextIndex = 0;
		}
		MUI_SetCvarString( cvarName, options[ nextIndex ].value );
	}

	mu_pop_id( ctx );
}

/**
*	@brief	Emit a `values` spin control (`MTYPE_SPINCONTROL`) writing a 0-based integer index to `cvarName`.
*	@param	ctx			Active MicroUI context.
*	@param	label		Control label displayed in the left column.
*	@param	cvarName	Target cvar name.
*	@param	itemNames	Array of display strings corresponding to indices `0 .. itemCount - 1`.
*	@param	itemCount	Number of elements in `itemNames`.
*	@param	status		Optional `--status` tooltip string.
**/
static void MUI_Widget_Values(
	mu_Context *ctx,
	const char *label,
	const char *cvarName,
	const char *const *itemNames,
	const int32_t itemCount,
	const char *status = nullptr )
{
	/**
	*	Validate item array.
	**/
	// Return early if no items are provided.
	if ( itemNames == nullptr || itemCount <= 0 ) {
		return;
	}

	mu_push_id( ctx, label, static_cast< int32_t >( std::strlen( label ) ) );

	/**
	*	Read current integer index from `cvarName` and render the spin control.
	**/
	const int32_t curIndex = MUI_GetCvarInteger( cvarName, 0 );
	const char *currentLabel = ( curIndex >= 0 && curIndex < itemCount ) ? itemNames[ curIndex ] : "???";
	const int32_t delta = MUI_DrawSpinRow( ctx, label, currentLabel, status );

	// Update the integer cvar when the user steps the spin control.
	if ( delta != 0 ) {
		int32_t nextIndex = ( curIndex < 0 || curIndex >= itemCount ) ? 0 : ( curIndex + delta );
		if ( nextIndex < 0 ) {
			nextIndex = itemCount - 1;
		} else if ( nextIndex >= itemCount ) {
			nextIndex = 0;
		}
		MUI_SetCvarInteger( cvarName, nextIndex );
	}

	mu_pop_id( ctx );
}

/**
*	@brief	Emit a `strings` spin control (`MTYPE_STRINGS`) writing the selected string token to `cvarName`.
*	@param	ctx			Active MicroUI context.
*	@param	label		Control label displayed in the left column.
*	@param	cvarName	Target cvar name.
*	@param	itemNames	Array of string values.
*	@param	itemCount	Number of elements in `itemNames`.
*	@param	status		Optional `--status` tooltip string.
**/
static void MUI_Widget_Strings(
	mu_Context *ctx,
	const char *label,
	const char *cvarName,
	const char *const *itemNames,
	const int32_t itemCount,
	const char *status = nullptr )
{
	/**
	*	Validate item array.
	**/
	// Return early if no string items are provided.
	if ( itemNames == nullptr || itemCount <= 0 ) {
		return;
	}

	mu_push_id( ctx, label, static_cast< int32_t >( std::strlen( label ) ) );

	/**
	*	Find the current string token matching the cvar value.
	**/
	const char *curStr = MUI_GetCvarString( cvarName, "" );
	int32_t curIndex = -1;
	// Search itemNames for a case-insensitive match.
	for ( int32_t i = 0; i < itemCount; i++ ) {
		if ( itemNames[ i ] != nullptr && Q_stricmp( itemNames[ i ], curStr ) == 0 ) {
			curIndex = i;
			break;
		}
	}

	/**
	*	Render the spin control and commit the newly selected string token when changed.
	**/
	const char *currentLabel = ( curIndex >= 0 && curIndex < itemCount ) ? itemNames[ curIndex ] : ( curStr[ 0 ] != '\0' ? curStr : "???" );
	const int32_t delta = MUI_DrawSpinRow( ctx, label, currentLabel, status );

	// Write the new string value to cvarName when stepped.
	if ( delta != 0 ) {
		int32_t nextIndex = ( curIndex < 0 ) ? 0 : ( curIndex + delta );
		if ( nextIndex < 0 ) {
			nextIndex = itemCount - 1;
		} else if ( nextIndex >= itemCount ) {
			nextIndex = 0;
		}
		MUI_SetCvarString( cvarName, itemNames[ nextIndex ] );
	}

	mu_pop_id( ctx );
}

/**
*	@brief	Emit a `field` text input control (`MTYPE_FIELD`) bound to a cvar.
*	@param	ctx			Active MicroUI context.
*	@param	label		Optional left-column label (`nullptr` when `--center` is used).
*	@param	cvarName	Target cvar name.
*	@param	widthChars	Maximum visible character width (`--width`).
*	@param	center		True when `--center` (`-c`) was specified.
*	@param	numericOnly	True when `--integer` (`-i`) or `--numeric` (`-n`) was specified.
*	@param	status		Optional `--status` tooltip string.
**/
static void MUI_Widget_Field(
	mu_Context *ctx,
	const char *label,
	const char *cvarName,
	const int32_t widthChars = 16,
	const bool center = false,
	const bool numericOnly = false,
	const char *status = nullptr )
{
	/**
	*	Validate cvar name.
	**/
	// Ignore fields without a valid target cvar.
	if ( cvarName == nullptr || cvarName[ 0 ] == '\0' ) {
		return;
	}

	mu_push_id( ctx, cvarName, static_cast< int32_t >( std::strlen( cvarName ) ) );

	/**
	*	Look up or allocate a retained edit buffer for `cvarName`.
	**/
	mui_field_buffer_t *fieldState = nullptr;
	// Search existing field buffers for cvarName.
	for ( mui_field_buffer_t &entry : s_fieldBuffers ) {
		if ( entry.cvarName == cvarName ) {
			fieldState = &entry;
			break;
		}
	}
	// Allocate a new buffer entry if this is the first time seeing cvarName.
	if ( fieldState == nullptr ) {
		mui_field_buffer_t newEntry = {};
		newEntry.cvarName = cvarName;
		s_fieldBuffers.push_back( newEntry );
		fieldState = &s_fieldBuffers.back();
	}

	// Synchronize buffer if the underlying cvar changed externally.
	const char *liveCvarStr = MUI_GetCvarString( cvarName, "" );
	if ( fieldState->lastSyncedValue != liveCvarStr ) {
		fieldState->lastSyncedValue = liveCvarStr;
		Q_strlcpy( fieldState->buffer, liveCvarStr, sizeof( fieldState->buffer ) );
	}

	/**
	*	Lay out either a centered single-column field (`addressbook`) or a labeled 2-column Scoreboard-styled field.
	**/
	if ( center || label == nullptr ) {
		const int32_t widths[ 1 ] = { -1 };
		mu_layout_row( ctx, 1, widths, MUI_GetRowHeight( ctx ) );
	} else {
		const int32_t widths[ 2 ] = { -220, -1 };
		mu_layout_row( ctx, 2, widths, MUI_GetRowHeight( ctx ) );
		MUI_DrawSettingRowLabel( ctx, label, status );
	}

	/**
	*	Render the MicroUI textbox and commit changes back to the cvar.
	**/
	const int32_t maxBufSize = std::min<int32_t>(
		static_cast< int32_t >( sizeof( fieldState->buffer ) ),
		std::clamp( widthChars + 1, 2, 127 )
	);
	const int32_t res = mu_textbox_ex( ctx, fieldState->buffer, maxBufSize, 0 );
	MUI_TrackControlStatus( ctx, ctx->last_rect, status );

	// Filter non-numeric characters if numericOnly is enabled and commit to the cvar on change/submit.
	if ( ( res & ( MU_RES_CHANGE | MU_RES_SUBMIT ) ) != 0 ) {
		if ( numericOnly ) {
			char *src = fieldState->buffer;
			char *dst = fieldState->buffer;
			// Strip any character that is not a digit, '+', '-', or '.'.
			while ( *src != '\0' ) {
				if ( ( *src >= '0' && *src <= '9' ) || *src == '+' || *src == '-' || *src == '.' ) {
					*dst++ = *src;
				}
				src++;
			}
			*dst = '\0';
		}
		fieldState->lastSyncedValue = fieldState->buffer;
		MUI_SetCvarString( cvarName, fieldState->buffer );
	}

	mu_pop_id( ctx );
}

/**
*	@brief	Emit a `bind` keybinding control (`MTYPE_KEYBIND`).
*	@param	ctx			Active MicroUI context.
*	@param	label		Action description label.
*	@param	cmd			Command string to bind/unbind.
*	@param	status		Default hover status text.
*	@param	altStatus	Status text displayed while waiting for a key press.
**/
static void MUI_Widget_Bind(
	mu_Context *ctx,
	const char *label,
	const char *cmd,
	const char *status = "Press Enter or Click to change, Clear to unbind",
	const char *altStatus = "Press the desired key, Escape to cancel" )
{
	/**
	*	Push a unique ID scope for this keybind row.
	**/
	mu_push_id( ctx, cmd, static_cast< int32_t >( std::strlen( cmd ) ) );

	const bool isCapturing = ( s_keybindCaptureCmd == cmd );

	/**
	*	Query current keybinding string and render a 3-column Scoreboard-styled row: `[ Label | Key Button | Clear ]`.
	**/
	char bindDisplay[ 128 ] = {};
	if ( isCapturing ) {
		Q_strlcpy( bindDisplay, "< Press a key... >", sizeof( bindDisplay ) );
	} else {
		MUI_GetKeybindDisplayString( cmd, bindDisplay, sizeof( bindDisplay ) );
	}

	const int32_t widths[ 3 ] = { -220, -68, -1 };
	mu_layout_row( ctx, 3, widths, MUI_GetRowHeight( ctx ) );

	MUI_DrawSettingRowLabel( ctx, label, isCapturing ? altStatus : status );

	// Clicking the binding button toggles key-capture mode for this command.
	if ( mu_button_ex( ctx, bindDisplay, 0, MU_OPT_ALIGNCENTER ) != 0 ) {
		if ( isCapturing ) {
			s_keybindCaptureCmd.clear();
		} else {
			s_keybindCaptureCmd = cmd;
#if !defined( CLGAME_INCLUDE )
			// In Engine UI, register a key-wait callback so the next physical key press binds immediately.
			Key_WaitKey( []( void * /*arg*/, int32_t key ) -> bool {
				if ( key != '`' && key != K_ESCAPE && !s_keybindCaptureCmd.empty() ) {
					Key_SetBinding( key, s_keybindCaptureCmd.c_str() );
				}
				s_keybindCaptureCmd.clear();
				Key_WaitKey( nullptr, nullptr );
				return false;
			}, nullptr );
#endif
		}
	}
	MUI_TrackControlStatus( ctx, ctx->last_rect, isCapturing ? altStatus : status );

	// Clicking Clear removes all bindings for this command.
	if ( mu_button_ex( ctx, "Clear", 0, MU_OPT_ALIGNCENTER ) != 0 ) {
		MUI_ClearKeybindingsForCommand( cmd );
		if ( isCapturing ) {
			s_keybindCaptureCmd.clear();
		}
	}
	MUI_TrackControlStatus( ctx, ctx->last_rect, "Clear all keys bound to this action" );

	mu_pop_id( ctx );
}

/**
*	@brief	Emit a `savegame` or `loadgame` slot button (`MTYPE_SAVEGAME` / `MTYPE_LOADGAME`).
*	@param	ctx			Active MicroUI context.
*	@param	slotDir		Save slot directory name (`save0`, `quick`, `save1`..`save15`).
*	@param	isLoadMenu	True for `loadgame` (disables empty slots), false for `savegame`.
*	@param	status		Optional `--status` tooltip string.
**/
static void MUI_Widget_SavegameSlot(
	mu_Context *ctx,
	const char *slotDir,
	const bool isLoadMenu,
	const char *status = nullptr )
{
	/**
	*	Push a unique ID scope for this save slot.
	**/
	mu_push_id( ctx, slotDir, static_cast< int32_t >( std::strlen( slotDir ) ) );

	/**
	*	Query slot description and determine whether the button is interactive.
	**/
	std::string slotLabel;
	bool isEmpty = true;
	MUI_GetSavegameSlotLabel( slotDir, slotLabel, isEmpty );

	const int32_t widths[ 1 ] = { -1 };
	mu_layout_row( ctx, 1, widths, MUI_GetRowHeight( ctx ) );

	const int32_t opt = ( isLoadMenu && isEmpty )
		? ( MU_OPT_ALIGNCENTER | MU_OPT_NOINTERACT )
		: MU_OPT_ALIGNCENTER;

	const int32_t res = mu_button_ex( ctx, slotLabel.c_str(), 0, opt );
	MUI_TrackControlStatus( ctx, ctx->last_rect, status );

	/**
	*	Dispatch `load "<slot>"` or `save "<slot>"; forcemenuoff` when activated.
	**/
	// Execute save or load command on click when allowed.
	if ( ( res & MU_RES_SUBMIT ) != 0 && !( isLoadMenu && isEmpty ) ) {
		char cmdBuf[ 128 ] = {};
		if ( isLoadMenu ) {
			std::snprintf( cmdBuf, sizeof( cmdBuf ), "load \"%s\"", slotDir );
			MUI_ForceMenuOff();
			MUI_ExecuteCommand( cmdBuf );
		} else {
			std::snprintf( cmdBuf, sizeof( cmdBuf ), "save \"%s\"; forcemenuoff", slotDir );
			MUI_ExecuteCommand( cmdBuf );
		}
	}

	mu_pop_id( ctx );
}



/**
*
*
*
*	All 29 Menu Implementations (`baseq2rtxp/q2rtxp.menu`):
*
*
*
**/

/**
*	@brief	1. `begin developer_menu` - "Developer (Test-)Maps" (lines 68-108 of `q2rtxp.menu`).
*	@param	ctx	Active MicroUI context.
**/
static void MUI_Menu_Developer( mu_Context *ctx ) {
	/**
	*	Begin the "Developer (Test-)Maps" window.
	**/
	if ( !MUI_BeginMenuWindow( ctx, "Developer (Test-)Maps", 28 ) ) {
		return;
	}

	/**
	*	Multiplayer & Signal I/O test maps.
	**/
	MUI_Widget_Blank( ctx );
	MUI_Widget_Action(
		ctx,
		"[MP] DeathMatch - It quacks like a duck thus..",
		"skill 0; gamemode 2; maxclients 8; map q2rtxp_dev_deathmatch force",
		true,
		"Deathmatch"
	);
	MUI_Widget_Action(
		ctx,
		"[MP] NavMesh - For testing testdummy navigation techniques",
		"skill 0; gamemode 1; maxclients 8; map q2rtxp_dev_navmesh force",
		true,
		"NavMesh"
	);
	MUI_Widget_Action(
		ctx,
		"[MP] Signal I/O - TargetRange",
		"skill 0; gamemode 2; maxclients 8; map q2rtxp_dev_sigio_targetrange force",
		true,
		"A simple moving target range level, utilizing Lua and Signal I/O"
	);

	/**
	*	CollisionModel material test maps.
	**/
	MUI_Widget_Blank( ctx );
	MUI_Widget_Action(
		ctx,
		"[SP] CollisionModel - Friction Materials",
		"skill 0; gamemode 0; maxclients 1; map q2rtxp_dev_cm_materials force",
		true,
		"A map with surfaces that have a different material friction applied to them."
	);
	MUI_Widget_Action(
		ctx,
		"[SP] CollisionModel - Audio Footstep Materials",
		"skill 0; gamemode 0; maxclients 1; map q2rtxp_dev_cm_mat_footsteps force",
		true,
		"A map with surfaces that have a distinct set of footstep audio effects assigned to them."
	);

	/**
	*	NPC test maps.
	**/
	MUI_Widget_Blank( ctx );
	MUI_Widget_Action(
		ctx,
		"[SP] NPC - monster_testdummy_puppet",
		"skill 0; gamemode 0; maxclients 1; map q2rtxp_dev_npc_puppet force",
		true,
		"A map with a basic few NPC's that can follow you around :-)"
	);

	/**
	*	Brush / func_* entity test maps.
	**/
	MUI_Widget_Blank( ctx );
	MUI_Widget_Action(
		ctx,
		"[SP] Entities - func_button",
		"skill 0; gamemode 0; maxclients 1; map q2rtxp_dev_func_buttons force",
		true,
		"func_button testbed, several func_buttons to engage triggering lights with in various way."
	);
	MUI_Widget_Action(
		ctx,
		"[SP] Entities - func_door & func_areaportal",
		"skill 0; gamemode 0; maxclients 1; map q2rtxp_dev_doors_areaportal force",
		true,
		"func_doors & areaportals testbed, tests doors with areaportals."
	);
	MUI_Widget_Action(
		ctx,
		"[SP] Entities - func_door/func_door_rotating",
		"skill 0; gamemode 0; maxclients 1; map q2rtxp_dev_func_doors force",
		true,
		"func_doors testbed, teamed doors, lockables, reversable, both directions, usetargets, and rotating!"
	);
	MUI_Widget_Action(
		ctx,
		"[SP] Entities - func_plat",
		"skill 0; gamemode 0; maxclients 1; map q2rtxp_dev_func_plat force",
		true,
		"func_plat tests, implements a simple elevator."
	);
	MUI_Widget_Action(
		ctx,
		"[SP] Entities - func_rotating",
		"skill 0; gamemode 0; maxclients 1; map q2rtxp_dev_func_rotating force",
		true,
		"func_rotating tests, various types of func_rotating doing their thing."
	);
	MUI_Widget_Action(
		ctx,
		"[SP] Entities - func_train",
		"skill 0; gamemode 0; maxclients 1; map q2rtxp_dev_func_train force",
		true,
		"func_train test."
	);
	MUI_Widget_Action(
		ctx,
		"[SP] Entities - func_wall",
		"skill 0; gamemode 0; maxclients 1; map q2rtxp_dev_func_wall force",
		true,
		"func_wall tests."
	);

	/**
	*	Info & Target entity test maps.
	**/
	MUI_Widget_Blank( ctx );
	MUI_Widget_Action(
		ctx,
		"[SP] Entities - info_intermission DM",
		"skill 0; gamemode 2; maxclients 4; map q2rtxp_dev_intermission force",
		true,
		"info_intermission entity testing."
	);
	MUI_Widget_Blank( ctx );
	MUI_Widget_Action(
		ctx,
		"[SP] Entities - target_* entities",
		"skill 0; gamemode 0; maxclients 1; map q2rtxp_dev_targets00 force",
		true,
		"target_* tests."
	);

	/**
	*	Graphics & Local Entities test maps.
	**/
	MUI_Widget_Blank( ctx );
	MUI_Widget_Action(
		ctx,
		"[SP] Gfx - Makkon Textures PBR (pt_nearest 1)",
		"skill 0; gamemode 0; maxclients 1; map q2rtxp_dev_graph_makkon force",
		true,
		"A map that utilizes various PBR Makkon Textures in pt_nearest mode, very modern retro :-)"
	);
	MUI_Widget_Action(
		ctx,
		"[SP] Gfx - Sprites(JSON)",
		"skill 0; gamemode 0; maxclients 1; map q2rtxp_dev_graph_sprites force",
		true,
		"Sprites done using .spj"
	);
	MUI_Widget_Action(
		ctx,
		"[SP] Gfx - SpotLights",
		"skill 0; gamemode 0; maxclients 1; map q2rtxp_dev_graph_spotlights force",
		true,
		"Is what it says really."
	);

	MUI_Widget_Blank( ctx );
	MUI_Widget_Action(
		ctx,
		"[SP] Local Entities - General",
		"skill 0; gamemode 0; maxclients 1; map q2rtxp_dev_localents_general force",
		true,
		"Showcases several local entities."
	);
	MUI_Widget_Action(
		ctx,
		"[SP] Local Entities - env_sound(EAX Reverb Areas)",
		"skill 0; gamemode 0; maxclients 1; map q2rtxp_dev_localents_env_sound force",
		true,
		"A map that consists of several sound environments that apply EAX reverb etc effects."
	);
	MUI_Widget_Action(
		ctx,
		"[SP] Local Entities - Local Client PVS",
		"skill 0; gamemode 0; maxclients 1; map q2rtxp_dev_localents_pvs force",
		true,
		"Testbed for svc_set_portalbit to notify local func_areaportals to open/close."
	);

	MUI_EndMenuWindow( ctx );
}

/**
*	@brief	2. `begin environment` - "Environment Settings" (lines 110-127 of `q2rtxp.menu`).
*	@param	ctx	Active MicroUI context.
**/
static void MUI_Menu_Environment( mu_Context *ctx ) {
	/**
	*	Begin the "Environment Settings" window.
	**/
	if ( !MUI_BeginMenuWindow( ctx, "Environment Settings", 12 ) ) {
		return;
	}

	/**
	*	Sky type, brightness, and time-of-day presets.
	**/
	static constexpr mui_pair_option_t skyTypePairs[] = {
		{ "original env.map", "0" },
		{ "earth",            "1" },
		{ "stroggos",         "2" }
	};
	MUI_Widget_Pairs( ctx, "sky type", "physical_sky", skyTypePairs, std::size( skyTypePairs ) );

	MUI_Widget_Range( ctx, "sun and sky brightness", "physical_sky_brightness", -10.0f, 2.0f, 0.1f, "%+.1f EV", false );

	static constexpr mui_pair_option_t sunPresetPairs[] = {
		{ "custom",           "0" },
		{ "current time",     "1" },
		{ "12x current time", "2" },
		{ "night",            "3" },
		{ "dawn",             "4" },
		{ "morning",          "5" },
		{ "noon",             "6" },
		{ "evening",          "7" },
		{ "dusk",             "8" }
	};
	MUI_Widget_Pairs( ctx, "time of day", "sun_preset", sunPresetPairs, std::size( sunPresetPairs ) );
	MUI_Widget_Bind( ctx, "next time of day key", "next_sun" );

	/**
	*	Custom sun orientation and gamepad controls.
	**/
	MUI_Widget_Blank( ctx );
	MUI_Widget_Range(
		ctx,
		"sun elevation",
		"sun_elevation",
		-20.0f,
		90.0f,
		1.0f,
		"%.0f deg",
		false,
		"only effective in custom time of day mode"
	);
	MUI_Widget_Range(
		ctx,
		"sun azimuth",
		"sun_azimuth",
		-20.0f,
		380.0f,
		5.0f,
		"%.0f deg",
		false,
		"only effective in custom time of day mode"
	);

	static constexpr mui_pair_option_t sunGamepadPairs[] = {
		{ "off",         "0" },
		{ "left stick",  "1" },
		{ "right stick", "2" }
	};
	MUI_Widget_Pairs(
		ctx,
		"control sun with gamepad",
		"sun_gamepad",
		sunGamepadPairs,
		std::size( sunGamepadPairs ),
		"only effective in custom time of day mode"
	);

	/**
	*	Clouds toggle and automatic sun latitude.
	**/
	MUI_Widget_Blank( ctx );
	MUI_Widget_Toggle( ctx, "clouds", "physical_sky_draw_clouds" );
	MUI_Widget_Blank( ctx );
	MUI_Widget_Range(
		ctx,
		"latitude",
		"sun_latitude",
		-90.0f,
		90.0f,
		2.0f,
		"%.1f deg",
		false,
		"latitude to compute the sun position in automatic mode"
	);

	MUI_EndMenuWindow( ctx );
}

/**
*	@brief	3. `begin resolution` - "Dynamic Resolution Scaling" (lines 129-141 of `q2rtxp.menu`).
*	@param	ctx	Active MicroUI context.
**/
static void MUI_Menu_Resolution( mu_Context *ctx ) {
	/**
	*	Begin the "Dynamic Resolution Scaling" window.
	**/
	if ( !MUI_BeginMenuWindow( ctx, "Dynamic Resolution Scaling", 7 ) ) {
		return;
	}

	/**
	*	Dynamic resolution scaling controls and fixed scale slider.
	**/
	MUI_Widget_Toggle( ctx, "dynamic resolution scaling", "drs_enable" );
	MUI_Widget_Blank( ctx );
	MUI_Widget_Range(
		ctx,
		"target frames per second",
		"drs_target",
		30.0f,
		250.0f,
		1.0f,
		"%.0f FPS",
		false,
		"only effective in dynamic mode"
	);
	MUI_Widget_Range(
		ctx,
		"minimum scale",
		"drs_minscale",
		25.0f,
		100.0f,
		5.0f,
		"%.0f%%",
		false,
		"only effective in dynamic mode"
	);
	MUI_Widget_Range(
		ctx,
		"maximum scale",
		"drs_maxscale",
		50.0f,
		150.0f,
		5.0f,
		"%.0f%%",
		false,
		"only effective in dynamic mode"
	);
	MUI_Widget_Blank( ctx );
	MUI_Widget_Range( ctx, "fixed resolution scale", "viewsize", 25.0f, 200.0f, 5.0f, "%.0f%%", false );

	MUI_EndMenuWindow( ctx );
}

/**
*	@brief	4. `begin hdr` - "HDR Settings" (lines 143-153 of `q2rtxp.menu`).
*	@param	ctx	Active MicroUI context.
**/
static void MUI_Menu_HDR( mu_Context *ctx ) {
	/**
	*	Begin the "HDR Settings" window.
	**/
	if ( !MUI_BeginMenuWindow( ctx, "HDR Settings", 5 ) ) {
		return;
	}

	/**
	*	HDR toggle, peak/UI nits, and saturation adjustment.
	**/
	MUI_Widget_Toggle( ctx, "Enable HDR", "vid_hdr" );
	MUI_Widget_Blank( ctx );
	MUI_Widget_Range( ctx, "peak brightness", "tm_hdr_peak_nits", 100.0f, 2000.0f, 10.0f, "%.0f nits", false );
	MUI_Widget_Range( ctx, "UI brightness", "ui_hdr_nits", 100.0f, 2000.0f, 10.0f, "%.0f nits", false );
	MUI_Widget_Range( ctx, "Saturation adjustment", "tm_hdr_saturation_scale", 0.0f, 200.0f, 5.0f, "%.0f%%", false );

	MUI_EndMenuWindow( ctx );
}

/**
*	@brief	5. `begin photomode` - "Photo Mode Settings" (lines 155-170 of `q2rtxp.menu`).
*	@param	ctx	Active MicroUI context.
**/
static void MUI_Menu_PhotoMode( mu_Context *ctx ) {
	/**
	*	Begin the "Photo Mode Settings" window.
	**/
	if ( !MUI_BeginMenuWindow( ctx, "Photo Mode Settings", 10 ) ) {
		return;
	}

	/**
	*	Accumulation rendering, activation keybind, and camera DOF controls.
	**/
	static constexpr mui_pair_option_t photoEnablePairs[] = {
		{ "no",                         "0" },
		{ "yes",                        "1" },
		{ "yes, hide GUI immediately",  "2" }
	};
	MUI_Widget_Pairs( ctx, "enable photo mode", "pt_accumulation_rendering", photoEnablePairs, std::size( photoEnablePairs ) );
	MUI_Widget_Bind( ctx, "activation key", "pause" );
	MUI_Widget_Toggle( ctx, "show pause plaque", "scr_showpause" );
	MUI_Widget_Blank( ctx );
	MUI_Widget_Range( ctx, "number of frames to accumulate", "pt_accumulation_rendering_framenum", 100.0f, 8000.0f, 100.0f, "%.0f", false );
	MUI_Widget_Blank( ctx );
	MUI_Widget_Toggle( ctx, "free camera", "pt_freecam" );
	MUI_Widget_Toggle( ctx, "depth of field", "pt_dof" );

	static constexpr mui_pair_option_t apertureShapePairs[] = {
		{ "circle",   "0" },
		{ "triangle", "3" },
		{ "square",   "4" },
		{ "pentagon", "5" },
		{ "hexagon",  "6" },
		{ "heptagon", "7" },
		{ "octagon",  "8" }
	};
	MUI_Widget_Pairs( ctx, "camera aperture shape", "pt_aperture_type", apertureShapePairs, std::size( apertureShapePairs ) );
	MUI_Widget_Range( ctx, "aperture rotation", "pt_aperture_angle", 0.0f, 1.0f, 0.1f, "%.0f%%", true );

	MUI_EndMenuWindow( ctx );
}

/**
*	@brief	6. `begin video` - "Video Setup" (lines 172-236 of `q2rtxp.menu`).
*	@param	ctx	Active MicroUI context.
**/
static void MUI_Menu_Video( mu_Context *ctx ) {
	/**
	*	Begin the "Video Setup" window.
	**/
	if ( !MUI_BeginMenuWindow( ctx, "Video Setup", 26 ) ) {
		return;
	}

	/**
	*	Common display and renderer controls.
	**/
	static constexpr mui_pair_option_t rendererPairs[] = {
		{ "opengl", "0" },
		{ "rtx",    "1" }
	};
	MUI_Widget_Pairs( ctx, "renderer", "vid_rtx", rendererPairs, std::size( rendererPairs ) );

	// Expand `values -s "use LEFT and RIGHT keys to select" "video mode" vid_fullscreen windowed $$vid_modelist`.
	{
		std::vector<std::string> modeTokens;
		MUI_ExpandTokens( "vid_modelist", modeTokens );
		std::vector<const char *> modePtrs;
		modePtrs.push_back( "windowed" );
		for ( const std::string &tok : modeTokens ) {
			modePtrs.push_back( tok.c_str() );
		}
		MUI_Widget_Values(
			ctx,
			"video mode",
			"vid_fullscreen",
			modePtrs.data(),
			static_cast< int32_t >( modePtrs.size() ),
			"use LEFT and RIGHT keys to select"
		);
	}

	static constexpr const char *aspectRatioValues[] = { "4:3", "auto" };
	MUI_Widget_Values( ctx, "aspect ratio", "cl_adjustfov", aspectRatioValues, std::size( aspectRatioValues ) );

	// Expand `pairs "full screen display" vid_display $vid_displaylist`.
	{
		std::vector<std::string> displayTokens;
		MUI_ExpandTokens( "vid_displaylist", displayTokens );
		std::vector<mui_pair_option_t> displayPairs;
		for ( size_t i = 0; i + 1 < displayTokens.size(); i += 2 ) {
			displayPairs.push_back( { displayTokens[ i ].c_str(), displayTokens[ i + 1 ].c_str() } );
		}
		if ( !displayPairs.empty() ) {
			MUI_Widget_Pairs(
				ctx,
				"full screen display",
				"vid_display",
				displayPairs.data(),
				static_cast< int32_t >( displayPairs.size() )
			);
		}
	}

	MUI_Widget_Toggle( ctx, "vertical sync", "vid_vsync" );

	static constexpr mui_pair_option_t fpsCounterPairs[] = {
		{ "off",                           "0" },
		{ "show FPS",                      "1" },
		{ "show FPS and resolution scale", "2" }
	};
	MUI_Widget_Pairs( ctx, "FPS counter", "scr_fps", fpsCounterPairs, std::size( fpsCounterPairs ) );
	MUI_Widget_Range( ctx, "field of view", "fov", 60.0f, 120.0f, 5.0f, "%.0f deg", false );
	MUI_Widget_Blank( ctx );

	/**
	*	Conditional `ifeq vid_rtx 1` block: Path-Tracing RTX video settings.
	**/
	// Check if RTX renderer is selected (`ifeq vid_rtx 1`).
	if ( MUI_CheckCondition( "vid_rtx", 1, true ) ) {
		MUI_Widget_Action( ctx, "resolution scaling options...", "pushmenu resolution" );
		MUI_Widget_Action( ctx, "HDR options...", "pushmenu hdr" );
		MUI_Widget_Blank( ctx );

		MUI_Widget_Range( ctx, "exposure bias (brightness)", "tm_exposure_bias", -5.0f, 0.0f, 0.1f, "%+.1f EV", false );
		MUI_Widget_Range( ctx, "contrast", "tm_reinhard", 0.0f, 1.0f, 0.1f, "%.0f%%", true );
		MUI_Widget_Range( ctx, "texture LOD bias", "pt_texture_lod_bias", -2.0f, 2.0f, 0.5f, nullptr, false );

		static constexpr mui_pair_option_t taaPairs[] = {
			{ "none",               "0" },
			{ "temporal AA",        "1" },
			{ "temporal upscaling", "2" }
		};
		MUI_Widget_Pairs( ctx, "anti-aliasing", "flt_taa", taaPairs, std::size( taaPairs ) );
		MUI_Widget_Toggle( ctx, "AMD FSR 1.0", "flt_fsr_enable" );
		MUI_Widget_Range( ctx, "AMD FSR 1.0 sharpness", "flt_fsr_sharpness", 0.0f, 2.0f, 0.01f, nullptr, false, "lower is sharper" );

		static constexpr mui_pair_option_t giBouncePairs[] = {
			{ "low",    "0.5" },
			{ "medium", "1" },
			{ "high",   "2" }
		};
		MUI_Widget_Pairs( ctx, "global illumination", "pt_num_bounce_rays", giBouncePairs, std::size( giBouncePairs ) );

		static constexpr mui_pair_option_t reflectRefractPairs[] = {
			{ "off", "0" },
			{ "1",   "1" },
			{ "2",   "2" },
			{ "4",   "4" },
			{ "8",   "8" }
		};
		MUI_Widget_Pairs( ctx, "reflection/refraction depth", "pt_reflect_refract", reflectRefractPairs, std::size( reflectRefractPairs ) );

		MUI_Widget_Toggle(
			ctx,
			"security cameras",
			"pt_cameras",
			false,
			-1,
			"turn some monitors in the game into security camera views"
		);
		MUI_Widget_Toggle( ctx, "caustics", "pt_caustics" );
		MUI_Widget_Toggle( ctx, "god rays", "gr_enable" );
		MUI_Widget_Toggle( ctx, "bloom", "bloom_enable" );
		MUI_Widget_Blank( ctx );

		MUI_Widget_Toggle( ctx, "denoiser", "flt_enable" );
		MUI_Widget_Toggle( ctx, "textures", "flt_fixed_albedo", true, 1 );

		static constexpr mui_pair_option_t ptNearestPairs[] = {
			{ "anisotropic",     "0" },
			{ "mixed",           "1" },
			{ "nearest (noisy)", "2" }
		};
		MUI_Widget_Pairs( ctx, "texture filtering", "pt_nearest", ptNearestPairs, std::size( ptNearestPairs ) );

		static constexpr mui_pair_option_t thickGlassPairs[] = {
			{ "disabled",        "0" },
			{ "photo mode only", "1" },
			{ "enabled",         "2" }
		};
		MUI_Widget_Pairs(
			ctx,
			"thick glass refraction",
			"pt_thick_glass",
			thickGlassPairs,
			std::size( thickGlassPairs ),
			"experimental feature - simulates reflection and refraction effects in thick glass"
		);

		static constexpr mui_pair_option_t projectionPairs[] = {
			{ "perspective", "0" },
			{ "cylindrical", "1" }
		};
		MUI_Widget_Pairs( ctx, "projection", "pt_projection", projectionPairs, std::size( projectionPairs ) );
		MUI_Widget_Blank( ctx );

		static constexpr mui_pair_option_t sliPairs[] = {
			{ "disabled",       "0" },
			{ "when available", "1" }
		};
		MUI_Widget_Pairs( ctx, "multi-gpu support", "sli", sliPairs, std::size( sliPairs ) );

		static constexpr mui_pair_option_t rtApiPairs[] = {
			{ "automatic selection",             "auto" },
			{ "prefer KHR_ray_query",            "query" },
			{ "prefer KHR_ray_tracing_pipeline", "pipeline" }
		};
		MUI_Widget_Pairs( ctx, "ray tracing API", "ray_tracing_api", rtApiPairs, std::size( rtApiPairs ) );
		MUI_Widget_Toggle( ctx, "GPU profiler", "profiler" );
	}

	/**
	*	Conditional `ifneq vid_rtx 1` block: OpenGL renderer settings.
	**/
	// Check if OpenGL renderer is selected (`ifneq vid_rtx 1`).
	if ( MUI_CheckCondition( "vid_rtx", 1, false ) ) {
		static constexpr mui_pair_option_t msaaPairs[] = {
			{ "no",      "0" },
			{ "2x MSAA", "2" },
			{ "4x MSAA", "4" },
			{ "8x MSAA", "8" }
		};
		MUI_Widget_Pairs( ctx, "anti-aliasing", "gl_multisamples", msaaPairs, std::size( msaaPairs ) );
		MUI_Widget_Range( ctx, "texture gamma", "vid_gamma", 1.3f, 0.3f );
		MUI_Widget_Toggle( ctx, "hardware gamma", "vid_hwgamma" );
		MUI_Widget_Range( ctx, "texture quality", "gl_picmip", 3.0f, 0.0f, -1.0f );

		static constexpr mui_pair_option_t glTexModePairs[] = {
			{ "nearest",   "GL_NEAREST" },
			{ "linear",    "GL_LINEAR" },
			{ "bilinear",  "GL_LINEAR_MIPMAP_NEAREST" },
			{ "trilinear", "GL_LINEAR_MIPMAP_LINEAR" }
		};
		MUI_Widget_Pairs( ctx, "texture filter", "gl_texturemode", glTexModePairs, std::size( glTexModePairs ) );

		static constexpr mui_pair_option_t anisoPairs[] = {
			{ "no",  "1" },
			{ "2x",  "2" },
			{ "4x",  "4" },
			{ "8x",  "8" },
			{ "16x", "16" }
		};
		MUI_Widget_Pairs( ctx, "anisotropic filter", "gl_anisotropy", anisoPairs, std::size( anisoPairs ) );
		MUI_Widget_Range( ctx, "texture saturation", "gl_saturation", 0.0f, 1.0f );

		static constexpr mui_pair_option_t intensityPairs[] = {
			{ "1x", "1" },
			{ "2x", "2" },
			{ "3x", "3" }
		};
		MUI_Widget_Pairs( ctx, "texture intensity", "intensity", intensityPairs, std::size( intensityPairs ) );
		MUI_Widget_Range( ctx, "lightmap saturation", "gl_coloredlightmaps", 0.0f, 1.0f );
		MUI_Widget_Range( ctx, "lightmap brightness", "gl_brightness", 0.0f, 0.3f );

		static constexpr const char *glShaderValues[] = { "legacy", "shaders" };
		MUI_Widget_Values( ctx, "rendering backend", "gl_shaders", glShaderValues, std::size( glShaderValues ) );
		MUI_Widget_Blank( ctx );
		MUI_Widget_Toggle( ctx, "use new textures and models", "gl_use_hd_assets" );
	}

	MUI_EndMenuWindow( ctx );
}

/**
*	@brief	7. `begin options` - "Options" (lines 238-263 of `q2rtxp.menu`).
*	@param	ctx	Active MicroUI context.
**/
static void MUI_Menu_Options( mu_Context *ctx ) {
	/**
	*	Begin the "Options" window.
	**/
	if ( !MUI_BeginMenuWindow( ctx, "Options", 16 ) ) {
		return;
	}

	/**
	*	Submenu navigation actions.
	**/
	// Only show "environment setup" when `vid_rtx` is 1.
	if ( MUI_CheckCondition( "vid_rtx", 1, true ) ) {
		MUI_Widget_Action( ctx, "environment setup", "pushmenu environment" );
	}
	MUI_Widget_Action( ctx, "graphics settings", "pushmenu video" );
	MUI_Widget_Action( ctx, "photo mode settings", "pushmenu photomode" );
	MUI_Widget_Action( ctx, "sound settings", "pushmenu sound" );
	MUI_Widget_Action( ctx, "player setup", "pushmenu players" );
	MUI_Widget_Blank( ctx );
	MUI_Widget_Action( ctx, "input setup", "pushmenu input" );
	MUI_Widget_Action( ctx, "developer key bindings", "pushmenu keys_developer" );
	MUI_Widget_Action( ctx, "action key bindings", "pushmenu keys" );
	MUI_Widget_Action( ctx, "weapon and item bindings", "pushmenu weapons" );
	MUI_Widget_Action( ctx, "function key bindings", "pushmenu keys_function" );
	MUI_Widget_Blank( ctx );
	MUI_Widget_Action( ctx, "effects setup", "pushmenu effects" );
	MUI_Widget_Action( ctx, "screen setup", "pushmenu screen" );
	MUI_Widget_Action( ctx, "download options", "pushmenu downloads" );
	MUI_Widget_Action( ctx, "address book", "pushmenu addressbook" );

	MUI_EndMenuWindow( ctx );
}

/**
*	@brief	8. `begin sound` - "Sound Setup" (lines 265-278 of `q2rtxp.menu`).
*	@param	ctx	Active MicroUI context.
**/
static void MUI_Menu_Sound( mu_Context *ctx ) {
	/**
	*	Begin the "Sound Setup" window.
	**/
	if ( !MUI_BeginMenuWindow( ctx, "Sound Setup", 9 ) ) {
		return;
	}

	/**
	*	Volume sliders, music toggles, ambient sounds, chat beep, and sound engine selection.
	**/
	MUI_Widget_Range( ctx, "master volume", "s_volume", 0.0f, 1.0f, 0.05f, "%.0f%%", true );
	MUI_Widget_Range( ctx, "music volume", "ogg_volume", 0.0f, 1.0f, 0.05f, "%.0f%%", true );
	MUI_Widget_Toggle( ctx, "enable music", "ogg_enable" );
	MUI_Widget_Toggle( ctx, "shuffle tracks", "ogg_shuffle" );
	MUI_Widget_Blank( ctx );

	static constexpr const char *ambientValues[] = { "no", "yes", "only player's own" };
	MUI_Widget_Values( ctx, "ambient sounds", "s_ambient", ambientValues, std::size( ambientValues ) );

	static constexpr const char *chatBeepValues[] = { "disabled", "default", "alternative" };
	MUI_Widget_Values( ctx, "chat beep", "cl_chat_sound", chatBeepValues, std::size( chatBeepValues ) );

	MUI_Widget_Blank( ctx );

	static constexpr const char *soundEngineValues[] = { "no sound", "software", "OpenAL" };
	MUI_Widget_Values( ctx, "sound engine", "s_enable", soundEngineValues, std::size( soundEngineValues ) );

	MUI_EndMenuWindow( ctx );
}

/**
*	@brief	9. `begin railtrail` - "Railgun Trail Setup" (lines 280-295 of `q2rtxp.menu`).
*	@param	ctx	Active MicroUI context.
**/
static void MUI_Menu_RailTrail( mu_Context *ctx ) {
	/**
	*	Begin the "Railgun Trail Setup" window.
	**/
	if ( !MUI_BeginMenuWindow( ctx, "Railgun Trail Setup", 6 ) ) {
		return;
	}

	/**
	*	Rail trail type, duration, widths, and colors.
	**/
	static constexpr const char *railTypeValues[] = { "default", "core only", "core and spiral" };
	MUI_Widget_Values( ctx, "rail trail type", "cl_railtrail_type", railTypeValues, std::size( railTypeValues ) );

	MUI_Widget_Range( ctx, "rail trail duration", "cl_railtrail_time", 0.1f, 3.0f, 0.1f, "%.1f s", false );
	MUI_Widget_Range( ctx, "core width", "cl_railcore_width", 1.0f, 6.0f, 1.0f );
	MUI_Widget_Range( ctx, "spiral radius", "cl_railspiral_radius", 1.0f, 6.0f, 1.0f );

	static constexpr mui_pair_option_t railColorPairs[] = {
		{ "black",   "black" },
		{ "red",     "red" },
		{ "green",   "green" },
		{ "yellow",  "yellow" },
		{ "blue",    "blue" },
		{ "cyan",    "cyan" },
		{ "magenta", "magenta" },
		{ "white",   "white" }
	};
	MUI_Widget_Pairs( ctx, "core color", "cl_railcore_color", railColorPairs, std::size( railColorPairs ) );
	MUI_Widget_Pairs( ctx, "spiral color", "cl_railspiral_color", railColorPairs, std::size( railColorPairs ) );

	MUI_EndMenuWindow( ctx );
}

/**
*	@brief	10. `begin effects` - "Effects Setup" (lines 297-317 of `q2rtxp.menu`).
*	@param	ctx	Active MicroUI context.
**/
static void MUI_Menu_Effects( mu_Context *ctx ) {
	/**
	*	Begin the "Effects Setup" window.
	**/
	if ( !MUI_BeginMenuWindow( ctx, "Effects Setup", 8 ) ) {
		return;
	}

	/**
	*	Conditional OpenGL vs RTX effects controls.
	**/
	// OpenGL effects (`ifneq vid_rtx 1`).
	if ( MUI_CheckCondition( "vid_rtx", 1, false ) ) {
		static constexpr const char *dynLightValues[] = { "no", "yes", "only switchable" };
		MUI_Widget_Values( ctx, "dynamic lighting", "gl_dynamic", dynLightValues, std::size( dynLightValues ) );

		static constexpr const char *celShadingValues[] = { "no", "1x", "2x", "3x" };
		MUI_Widget_Values( ctx, "entity cel-shading", "gl_celshading", celShadingValues, std::size( celShadingValues ) );

		MUI_Widget_Toggle( ctx, "ground shadows", "gl_shadows" );
		MUI_Widget_Toggle( ctx, "screen blending", "gl_polyblend" );
	}

	// RTX screen blending effects (`ifeq vid_rtx 1`).
	if ( MUI_CheckCondition( "vid_rtx", 1, true ) ) {
		MUI_Widget_Toggle( ctx, "screen blending", "tm_blend_enable" );
		MUI_Widget_Range( ctx, "screen blending center strength", "tm_blend_scale_center", 0.0f, 1.0f, 0.0f, "%.0f%%", true );
		MUI_Widget_Range( ctx, "screen blending border strength", "tm_blend_scale_border", 0.0f, 1.0f, 0.0f, "%.0f%%", true );
	}

	MUI_Widget_Toggle( ctx, "explosion sprites", "cl_explosion_sprites" );
	MUI_Widget_Blank( ctx );
	MUI_Widget_Action( ctx, "railgun trail setup...", "pushmenu railtrail", true );

	MUI_EndMenuWindow( ctx );
}

/**
*	@brief	11. `begin crosshair` - "Crosshair Setup" (lines 319-330 of `q2rtxp.menu`).
*	@param	ctx	Active MicroUI context.
**/
static void MUI_Menu_Crosshair( mu_Context *ctx ) {
	/**
	*	Begin the "Crosshair Setup" window.
	**/
	if ( !MUI_BeginMenuWindow( ctx, "Crosshair Setup", 7 ) ) {
		return;
	}

	/**
	*	Crosshair shape, scale, health coloring, and RGBA channel sliders.
	**/
	static constexpr const char *crosshairTypeValues[] = { "none", "cross", "dot", "angle" };
	MUI_Widget_Values( ctx, "crosshair type", "crosshair", crosshairTypeValues, std::size( crosshairTypeValues ) );

	static constexpr mui_pair_option_t chScalePairs[] = {
		{ "0.5x", "0.5" },
		{ "1x",   "1" },
		{ "2x",   "2" }
	};
	MUI_Widget_Pairs( ctx, "crosshair scale", "ch_scale", chScalePairs, std::size( chScalePairs ) );

	MUI_Widget_Toggle( ctx, "color by health", "ch_health" );
	MUI_Widget_Range( ctx, "red channel", "ch_red", 0.0f, 1.0f, 0.0f, "%.0f%%", true );
	MUI_Widget_Range( ctx, "green channel", "ch_green", 0.0f, 1.0f, 0.0f, "%.0f%%", true );
	MUI_Widget_Range( ctx, "blue channel", "ch_blue", 0.0f, 1.0f, 0.0f, "%.0f%%", true );
	MUI_Widget_Range( ctx, "alpha channel", "ch_alpha", 0.0f, 1.0f, 0.0f, "%.0f%%", true );

	MUI_EndMenuWindow( ctx );
}

/**
*	@brief	12. `begin screen` - "Screen Setup" (lines 332-348 of `q2rtxp.menu`).
*	@param	ctx	Active MicroUI context.
**/
static void MUI_Menu_Screen( mu_Context *ctx ) {
	/**
	*	Begin the "Screen Setup" window.
	**/
	if ( !MUI_BeginMenuWindow( ctx, "Screen Setup", 12 ) ) {
		return;
	}

	/**
	*	Cutscenes, HUD/console opacity and scaling, and crosshair setup link.
	**/
	MUI_Widget_Toggle( ctx, "cutscenes", "cl_cinematics" );
	MUI_Widget_Blank( ctx );

	static constexpr mui_pair_option_t showItemPairs[] = {
		{ "do not show",    "0" },
		{ "show on change", "1" },
		{ "always show",    "2" }
	};
	MUI_Widget_Pairs( ctx, "selected inventory item", "scr_showitemname", showItemPairs, std::size( showItemPairs ) );

	MUI_Widget_Toggle( ctx, "ping graph", "scr_lag_draw" );

	static constexpr const char *demoBarValues[] = { "no", "yes", "verbose" };
	MUI_Widget_Values( ctx, "demo bar", "scr_demobar", demoBarValues, std::size( demoBarValues ) );

	MUI_Widget_Range( ctx, "HUD opacity", "scr_alpha", 0.0f, 1.0f, 0.0f, "%.0f%%", true );
	MUI_Widget_Range( ctx, "console opacity", "con_alpha", 0.0f, 1.0f, 0.0f, "%.0f%%", true );

	static constexpr mui_pair_option_t scalePairs[] = {
		{ "auto", "0" },
		{ "1x",   "1" },
		{ "2x",   "2" },
		{ "4x",   "4" }
	};
	MUI_Widget_Pairs( ctx, "HUD scale", "scr_scale", scalePairs, std::size( scalePairs ) );
	MUI_Widget_Pairs( ctx, "console scale", "con_scale", scalePairs, std::size( scalePairs ) );
	MUI_Widget_Pairs( ctx, "menu scale", "ui_scale", scalePairs, std::size( scalePairs ) );

	MUI_Widget_Blank( ctx );
	MUI_Widget_Action( ctx, "crosshair setup...", "pushmenu crosshair", true );

	MUI_EndMenuWindow( ctx );
}

/**
*	@brief	13. `begin downloads` - "Download Options" (lines 350-363 of `q2rtxp.menu`).
*	@param	ctx	Active MicroUI context.
**/
static void MUI_Menu_Downloads( mu_Context *ctx ) {
	/**
	*	Begin the "Download Options" window.
	**/
	if ( !MUI_BeginMenuWindow( ctx, "Download Options", 9 ) ) {
		return;
	}

	/**
	*	Asset auto-download toggles and HTTP download toggle.
	**/
	MUI_Widget_Toggle( ctx, "allow downloading", "allow_download" );
	MUI_Widget_Toggle( ctx, "maps", "allow_download_maps" );
	MUI_Widget_Toggle( ctx, "player models/skins", "allow_download_players" );
	MUI_Widget_Toggle( ctx, "models", "allow_download_models" );
	MUI_Widget_Toggle( ctx, "sounds", "allow_download_sounds" );
	MUI_Widget_Toggle( ctx, "textures", "allow_download_textures" );
	MUI_Widget_Toggle( ctx, "pics", "allow_download_pics" );
	MUI_Widget_Blank( ctx );
	MUI_Widget_Toggle( ctx, "HTTP downloads", "cl_http_downloads" );

	MUI_EndMenuWindow( ctx );
}

/**
*	@brief	14. `begin gameflags` - "Deathmatch Flags" (lines 365-381 of `q2rtxp.menu`).
*	@param	ctx	Active MicroUI context.
**/
static void MUI_Menu_GameFlags( mu_Context *ctx ) {
	/**
	*	Begin the "Deathmatch Flags" window.
	**/
	if ( !MUI_BeginMenuWindow( ctx, "Deathmatch Flags", 12 ) ) {
		return;
	}

	/**
	*	Bitfield toggles on `dmflags`.
	**/
	MUI_Widget_Toggle( ctx, "falling damage", "dmflags", true, 3 );
	MUI_Widget_Toggle( ctx, "weapons stay", "dmflags", false, 2 );
	MUI_Widget_Toggle( ctx, "instant powerups", "dmflags", false, 4 );
	MUI_Widget_Toggle( ctx, "allow powerups", "dmflags", true, 1 );
	MUI_Widget_Toggle( ctx, "allow health", "dmflags", true, 0 );
	MUI_Widget_Toggle( ctx, "allow armor", "dmflags", true, 11 );
	MUI_Widget_Toggle( ctx, "spawn farthest", "dmflags", false, 9 );
	MUI_Widget_Toggle( ctx, "same map", "dmflags", false, 5 );
	MUI_Widget_Toggle( ctx, "force respawn", "dmflags", false, 10 );
	MUI_Widget_Toggle( ctx, "infinite ammo", "dmflags", false, 13 );
	MUI_Widget_Toggle( ctx, "fixed FOV", "dmflags", false, 15 );
	MUI_Widget_Toggle( ctx, "quad drop", "dmflags", false, 14 );

	MUI_EndMenuWindow( ctx );
}

/**
*	@brief	15. `begin startserver` - "Start Server" (lines 383-400 of `q2rtxp.menu`).
*	@param	ctx	Active MicroUI context.
**/
static void MUI_Menu_StartServer( mu_Context *ctx ) {
	/**
	*	Begin the "Start Server" window.
	**/
	if ( !MUI_BeginMenuWindow( ctx, "Start Server", 9 ) ) {
		return;
	}

	/**
	*	Expand `strings "initial map" _ui_nextserver $$com_maplist`.
	**/
	{
		std::vector<std::string> mapTokens;
		MUI_ExpandTokens( "com_maplist", mapTokens );
		std::vector<const char *> mapPtrs;
		for ( const std::string &tok : mapTokens ) {
			mapPtrs.push_back( tok.c_str() );
		}
		if ( !mapPtrs.empty() ) {
			MUI_Widget_Strings(
				ctx,
				"initial map",
				"_ui_nextserver",
				mapPtrs.data(),
				static_cast< int32_t >( mapPtrs.size() )
			);
		} else {
			MUI_Widget_Field( ctx, "initial map", "_ui_nextserver", 16, false, false );
		}
	}

	static constexpr mui_pair_option_t gameModePairs[] = {
		{ "Deathmatch",  "0" },
		{ "Cooperative", "1" }
	};
	MUI_Widget_Pairs( ctx, "game mode", "_ui_gamemode", gameModePairs, std::size( gameModePairs ) );

	MUI_Widget_Field( ctx, "time limit", "timelimit", 3, false, true, "0 - no time limit" );
	MUI_Widget_Field( ctx, "frag limit", "fraglimit", 3, false, true, "0 - no frag limit" );
	MUI_Widget_Field( ctx, "max players", "maxclients", 3, false, true );
	MUI_Widget_Field( ctx, "host name", "hostname", 16, false, false );

	MUI_Widget_Blank( ctx );
	MUI_Widget_Action( ctx, "deathmatch flags...", "pushmenu gameflags", true );
	MUI_Widget_Action(
		ctx,
		"begin game!",
		"forcemenuoff; if $_ui_gamemode == 0 then gamemode 1 else gamemode 2; map $_ui_nextserver force",
		true
	);

	MUI_EndMenuWindow( ctx );
}

/**
*	@brief	16. `begin multiplayer` - "Multiplayer" (lines 402-412 of `q2rtxp.menu`).
*	@param	ctx	Active MicroUI context.
**/
static void MUI_Menu_Multiplayer( mu_Context *ctx ) {
	/**
	*	Begin the "Multiplayer" window.
	**/
	if ( !MUI_BeginMenuWindow( ctx, "Multiplayer", 6 ) ) {
		return;
	}

	/**
	*	Server browser, demos, start server, player setup, and disconnect actions.
	**/
	MUI_Widget_Action( ctx, "browse q2servers.com", "pushmenu servers \"+http://q2servers.com/?raw=2\"" );
	MUI_Widget_Action( ctx, "browse address book", "pushmenu servers \"favorites://\" \"file:///servers.lst\" \"broadcast://\"" );
	MUI_Widget_Action( ctx, "browse demos", "pushmenu demos" );
	MUI_Widget_Action( ctx, "start server", "pushmenu startserver" );
	MUI_Widget_Action( ctx, "player setup", "pushmenu players" );
	MUI_Widget_Action( ctx, "disconnect", "disconnect" );

	MUI_EndMenuWindow( ctx );
}

/**
*	@brief	17. `begin singleplayer` - "Singleplayer" (lines 414-428 of `q2rtxp.menu`).
*	@param	ctx	Active MicroUI context.
**/
static void MUI_Menu_Singleplayer( mu_Context *ctx ) {
	/**
	*	Begin the "Singleplayer" window.
	**/
	if ( !MUI_BeginMenuWindow( ctx, "Singleplayer", 7 ) ) {
		return;
	}

	/**
	*	Difficulty skill actions and load/save game menu links.
	**/
	MUI_Widget_Action( ctx, "easy", "skill 0; gamemode 0; maxclients 1; newgame" );
	MUI_Widget_Action( ctx, "medium", "skill 1; gamemode 0; maxclients 1; newgame" );
	MUI_Widget_Action( ctx, "hard", "skill 2; gamemode 0; maxclients 1; newgame" );
	MUI_Widget_Action( ctx, "nightmare", "skill 3; gamemode 0; maxclients 1; newgame" );
	MUI_Widget_Blank( ctx );
	MUI_Widget_Action( ctx, "load game...", "pushmenu loadgame" );
	MUI_Widget_Action( ctx, "save game...", "pushmenu savegame" );

	MUI_EndMenuWindow( ctx );
}

/**
*	@brief	18. `begin loadgame` - "Load Game" (lines 430-453 of `q2rtxp.menu`).
*	@param	ctx	Active MicroUI context.
**/
static void MUI_Menu_LoadGame( mu_Context *ctx ) {
	/**
	*	Begin the "Load Game" window.
	**/
	if ( !MUI_BeginMenuWindow( ctx, "Load Game", 19 ) ) {
		return;
	}

	/**
	*	Auto-save (`save0`), quick-save (`quick`), and manual slots (`save1`..`save15`).
	**/
	MUI_Widget_SavegameSlot( ctx, "save0", true );
	MUI_Widget_Blank( ctx );
	MUI_Widget_SavegameSlot( ctx, "quick", true );
	MUI_Widget_Blank( ctx );

	static constexpr const char *loadSlots[] = {
		"save1", "save2", "save3", "save4", "save5",
		"save6", "save7", "save8", "save9", "save10",
		"save11", "save12", "save13", "save14", "save15"
	};
	// Emit a loadgame button for each manual save slot.
	for ( const char *slot : loadSlots ) {
		MUI_Widget_SavegameSlot( ctx, slot, true );
	}

	MUI_EndMenuWindow( ctx );
}

/**
*	@brief	19. `begin savegame` - "Save Game" (lines 455-473 of `q2rtxp.menu`).
*	@param	ctx	Active MicroUI context.
**/
static void MUI_Menu_SaveGame( mu_Context *ctx ) {
	/**
	*	Begin the "Save Game" window.
	**/
	if ( !MUI_BeginMenuWindow( ctx, "Save Game", 15 ) ) {
		return;
	}

	/**
	*	Manual save slots (`save1`..`save8`, `save10`..`save15` as listed in `q2rtxp.menu`).
	**/
	static constexpr const char *saveSlots[] = {
		"save1", "save2", "save3", "save4", "save5",
		"save6", "save7", "save8", "save10", "save11",
		"save12", "save13", "save14", "save15"
	};
	// Emit a savegame button for each slot.
	for ( const char *slot : saveSlots ) {
		MUI_Widget_SavegameSlot( ctx, slot, false );
	}

	MUI_EndMenuWindow( ctx );
}

/**
*	@brief	20. `begin quit` - "Quit Game?" (lines 475-482 of `q2rtxp.menu`).
*	@param	ctx	Active MicroUI context.
**/
static void MUI_Menu_Quit( mu_Context *ctx ) {
	/**
	*	Begin the "Quit Game?" confirmation window.
	**/
	if ( !MUI_BeginMenuWindow( ctx, "Quit Game?", 3 ) ) {
		return;
	}

	/**
	*	Quit confirmation and cancel actions.
	**/
	MUI_Widget_Action( ctx, "quit game", "quit" );
	MUI_Widget_Action( ctx, "cancel", "popmenu" );

	MUI_EndMenuWindow( ctx );
}

/**
*	@brief	21. `begin main` - "Q2RTXPerimental" (lines 484-504 of `q2rtxp.menu`).
*	@param	ctx	Active MicroUI context.
**/
static void MUI_Menu_Main( mu_Context *ctx ) {
	/**
	*	Begin the "Q2RTXPerimental" main menu window.
	**/
	if ( !MUI_BeginMenuWindow( ctx, "Q2RTXPerimental", 7 ) ) {
		return;
	}

	/**
	*	Root menu entries.
	**/
	MUI_Widget_Action( ctx, "developer", "pushmenu developer_menu" );
	MUI_Widget_Blank( ctx );
	MUI_Widget_Action( ctx, "singleplayer", "pushmenu singleplayer" );
	MUI_Widget_Action( ctx, "multiplayer", "pushmenu multiplayer" );
	MUI_Widget_Action( ctx, "options", "pushmenu options" );
	MUI_Widget_Action( ctx, "video settings", "pushmenu video" );
	MUI_Widget_Action( ctx, "quit", "pushmenu quit" );

	MUI_EndMenuWindow( ctx );
}

/**
*	@brief	22. `begin game` - "In-Game Menu" (lines 506-521 of `q2rtxp.menu`).
*	@param	ctx	Active MicroUI context.
**/
static void MUI_Menu_Game( mu_Context *ctx ) {
	/**
	*	Begin the "In-Game Menu" window.
	**/
	if ( !MUI_BeginMenuWindow( ctx, "In-Game Menu", 7 ) ) {
		return;
	}

	/**
	*	In-game root menu entries.
	**/
	MUI_Widget_Action( ctx, "developer stuff", "pushmenu developer_menu" );
	MUI_Widget_Blank( ctx );
	MUI_Widget_Action( ctx, "singleplayer", "pushmenu singleplayer" );
	MUI_Widget_Action( ctx, "multiplayer", "pushmenu multiplayer" );
	MUI_Widget_Action( ctx, "options", "pushmenu options" );
	MUI_Widget_Action( ctx, "video settings", "pushmenu video" );
	MUI_Widget_Action( ctx, "quit", "pushmenu quit" );

	MUI_EndMenuWindow( ctx );
}

/**
*	@brief	23. `begin addressbook` - "Address Book" (lines 523-544 of `q2rtxp.menu`).
*	@param	ctx	Active MicroUI context.
**/
static void MUI_Menu_AddressBook( mu_Context *ctx ) {
	/**
	*	Begin the "Address Book" window.
	**/
	if ( !MUI_BeginMenuWindow( ctx, "Address Book", 16 ) ) {
		return;
	}

	/**
	*	16 centered server address fields (`adr0`..`adr15`).
	**/
	static constexpr const char *addressBookCvars[] = {
		"adr0",  "adr1",  "adr2",  "adr3",
		"adr4",  "adr5",  "adr6",  "adr7",
		"adr8",  "adr9",  "adr10", "adr11",
		"adr12", "adr13", "adr14", "adr15"
	};
	// Emit a centered width-32 field for each address book cvar.
	for ( const char *adrCvar : addressBookCvars ) {
		MUI_Widget_Field( ctx, nullptr, adrCvar, 32, true, false );
	}

	MUI_EndMenuWindow( ctx );
}

/**
*	@brief	24. `begin input` - "Input Setup" (lines 546-557 of `q2rtxp.menu`).
*	@param	ctx	Active MicroUI context.
**/
static void MUI_Menu_Input( mu_Context *ctx ) {
	/**
	*	Begin the "Input Setup" window.
	**/
	if ( !MUI_BeginMenuWindow( ctx, "Input Setup", 7 ) ) {
		return;
	}

	/**
	*	Mouse sensitivity field and look/movement toggles.
	**/
	MUI_Widget_Field( ctx, "mouse sensitivity", "sensitivity", 8, false, true );
	MUI_Widget_Toggle( ctx, "invert mouse", "m_invert" );
	MUI_Widget_Toggle( ctx, "adjust sensitivity with FOV", "m_autosens" );
	MUI_Widget_Toggle( ctx, "filter mouse input", "m_filter" );
	MUI_Widget_Toggle( ctx, "free look", "freelook" );
	MUI_Widget_Blank( ctx );
	MUI_Widget_Toggle( ctx, "always run", "cl_run" );

	MUI_EndMenuWindow( ctx );
}

/**
*	@brief	25. `begin keys` - "Action Key Bindings" (lines 559-586 of `q2rtxp.menu`).
*	@param	ctx	Active MicroUI context.
**/
static void MUI_Menu_Keys( mu_Context *ctx ) {
	/**
	*	Begin the "Action Key Bindings" window.
	**/
	if ( !MUI_BeginMenuWindow( ctx, "Action Key Bindings", 24 ) ) {
		return;
	}

	/**
	*	Combat & interaction bindings.
	**/
	MUI_Widget_Bind( ctx, "Primary Fire", "+fire_prim" );
	MUI_Widget_Bind( ctx, "Secondary Fire", "+fire_sec" );
	MUI_Widget_Bind( ctx, "Reload Weapon", "+reload" );
	MUI_Widget_Bind( ctx, "Use Target", "+usetarget" );

	/**
	*	Movement bindings.
	**/
	MUI_Widget_Blank( ctx );
	MUI_Widget_Bind( ctx, "Walk Forward", "+forward" );
	MUI_Widget_Bind( ctx, "Walk Backward", "+back" );
	MUI_Widget_Bind( ctx, "Run", "+speed" );
	MUI_Widget_Bind( ctx, "Step Left", "+moveleft" );
	MUI_Widget_Bind( ctx, "Step Right", "+moveright" );
	MUI_Widget_Bind( ctx, "Up/Jump", "+moveup" );
	MUI_Widget_Bind( ctx, "Down/Crouch", "+movedown" );

	/**
	*	Weapon bar & inventory bindings.
	**/
	MUI_Widget_Blank( ctx );
	MUI_Widget_Bind( ctx, "Next Weapon", "+wpbar_next" );
	MUI_Widget_Bind( ctx, "Previous Weapon", "+wpbar_prev" );
	MUI_Widget_Blank( ctx );
	MUI_Widget_Bind( ctx, "Inventory", "inven" );
	MUI_Widget_Bind( ctx, "Inventory Use Selected Item", "invuse" );
	MUI_Widget_Bind( ctx, "Inventory Drop Selected Item", "invdrop" );
	MUI_Widget_Bind( ctx, "Inventory Prev Selected Item", "invprev" );
	MUI_Widget_Bind( ctx, "Inventory Next Selected Item", "invnext" );

	MUI_Widget_Blank( ctx );
	MUI_Widget_Action( ctx, "legacy keys...", "pushmenu keys_legacy" );

	MUI_EndMenuWindow( ctx );
}

/**
*	@brief	26. `begin keys_function` - "Function Key Bindings" (lines 588-609 of `q2rtxp.menu`).
*	@param	ctx	Active MicroUI context.
**/
static void MUI_Menu_KeysFunction( mu_Context *ctx ) {
	/**
	*	Begin the "Function Key Bindings" window.
	**/
	if ( !MUI_BeginMenuWindow( ctx, "Function Key Bindings", 16 ) ) {
		return;
	}

	/**
	*	Menu shortcut and utility function key bindings.
	**/
	MUI_Widget_Bind( ctx, "save game menu", "pushmenu savegame" );
	MUI_Widget_Bind( ctx, "load game menu", "pushmenu loadgame" );
	MUI_Widget_Bind( ctx, "options menu", "pushmenu options" );
	MUI_Widget_Bind( ctx, "graphics settings", "pushmenu video" );
	MUI_Widget_Bind( ctx, "quick save", "save quick" );
	MUI_Widget_Bind( ctx, "sound settings", "pushmenu sound" );
	MUI_Widget_Bind( ctx, "player setup", "pushmenu players" );
	MUI_Widget_Bind( ctx, "quick load", "load quick" );
	MUI_Widget_Bind( ctx, "quit", "pushmenu quit" );
	MUI_Widget_Bind( ctx, "photo mode settings", "pushmenu photomode" );
	MUI_Widget_Blank( ctx );
	MUI_Widget_Bind( ctx, "screenshot", "screenshot" );
	MUI_Widget_Bind( ctx, "pause game", "pause" );
	MUI_Widget_Bind( ctx, "next time of day", "next_sun" );
	MUI_Widget_Bind( ctx, "score table", "score" );
	MUI_Widget_Bind( ctx, "chat", "messagemode" );

	MUI_EndMenuWindow( ctx );
}

/**
*	@brief	27. `begin keys_legacy` - "Legacy Key Bindings" (lines 611-624 of `q2rtxp.menu`).
*	@param	ctx	Active MicroUI context.
**/
static void MUI_Menu_KeysLegacy( mu_Context *ctx ) {
	/**
	*	Begin the "Legacy Key Bindings" window.
	**/
	if ( !MUI_BeginMenuWindow( ctx, "Legacy Key Bindings", 8 ) ) {
		return;
	}

	/**
	*	Keyboard turn, strafe, and look bindings.
	**/
	MUI_Widget_Bind( ctx, "turn left", "+left" );
	MUI_Widget_Bind( ctx, "turn right", "+right" );
	MUI_Widget_Bind( ctx, "sidestep", "+strafe" );
	MUI_Widget_Bind( ctx, "look up", "+lookup" );
	MUI_Widget_Bind( ctx, "look down", "+lookdown" );
	MUI_Widget_Bind( ctx, "center view", "centerview" );
	MUI_Widget_Bind( ctx, "mouse look", "+mlook" );
	MUI_Widget_Bind( ctx, "keyboard look", "+klook" );

	MUI_EndMenuWindow( ctx );
}

/**
*	@brief	28. `begin keys_developer` - "Developer Key Bindings" (lines 626-634 of `q2rtxp.menu`).
*	@param	ctx	Active MicroUI context.
**/
static void MUI_Menu_KeysDeveloper( mu_Context *ctx ) {
	/**
	*	Begin the "Developer Key Bindings" window.
	**/
	if ( !MUI_BeginMenuWindow( ctx, "Developer Key Bindings", 4 ) ) {
		return;
	}

	/**
	*	Cheat / debugging key bindings.
	**/
	MUI_Widget_Bind( ctx, "Give ALl", "give all" );
	MUI_Widget_Bind( ctx, "GodMode", "god" );
	MUI_Widget_Bind( ctx, "NoClip", "noclip" );

	MUI_EndMenuWindow( ctx );
}

/**
*	@brief	29. `begin weapons` - "Weapon and Item Bindings" (lines 636-642 of `q2rtxp.menu`).
*	@param	ctx	Active MicroUI context.
**/
static void MUI_Menu_Weapons( mu_Context *ctx ) {
	/**
	*	Begin the "Weapon and Item Bindings" window.
	**/
	if ( !MUI_BeginMenuWindow( ctx, "Weapon and Item Bindings", 4 ) ) {
		return;
	}

	/**
	*	Direct weapon use bindings.
	**/
	MUI_Widget_Bind( ctx, "fists", "use Fists" );
	MUI_Widget_Bind( ctx, "pistol", "use Pistol" );

	MUI_EndMenuWindow( ctx );
}

/**
*	@brief	Built-in hardcoded menu hook (`servers`, `demos`, `players`) referenced by `pushmenu` in `q2rtxp.menu`.
*	@param	ctx	Active MicroUI context.
*	@note	Provides a template window for built-in C++ menus or forwards to the legacy engine UI menu.
**/
static void MUI_Menu_BuiltinPlaceholder( mu_Context *ctx ) {
	/**
	*	Determine which built-in menu is at the top of the stack.
	**/
	const mui_script_menu_id_t activeId = ( s_menuStackDepth > 0 )
		? s_menuStack[ s_menuStackDepth - 1 ]
		: mui_script_menu_id_t::NONE;
	const mui_menu_registry_entry_t *entry = MUI_FindMenuEntryById( activeId );
	const char *title = ( entry != nullptr && entry->title != nullptr ) ? entry->title : "Built-In Menu";

	/**
	*	Begin template window for the built-in menu.
	**/
	if ( !MUI_BeginMenuWindow( ctx, title, 5 ) ) {
		return;
	}

	const int32_t widths[ 1 ] = { -1 };
	mu_layout_row( ctx, 1, widths, MUI_GetRowHeight( ctx ) );
	mu_label_ex( ctx, "Built-in C++ menu placeholder template.", MU_COLOR_TEXT, MU_OPT_ALIGNCENTER );

	// Show any arguments passed to `pushmenu servers ...`.
	if ( activeId == mui_script_menu_id_t::BUILTIN_SERVERS && !s_serverBrowserArgs.empty() ) {
		mu_layout_row( ctx, 1, widths, MUI_GetRowHeight( ctx ) );
		mu_label_ex( ctx, s_serverBrowserArgs.c_str(), MU_COLOR_TEXT, MU_OPT_ALIGNCENTER );
	}

	MUI_EndMenuWindow( ctx );
}



/**
*
*
*
*	Public Drop-In Entry Points:
*
*
*
**/
/**
*	@brief	Open the root MicroUI menu by script menu name (defaults to `"game"` when in-game or `"main"`).
*	@param	menuName	Optional script menu name from `q2rtxp.menu` (e.g. `"main"`, `"game"`, `"options"`, `"video"`).
**/
void CLG_MUI_OpenRootMenu( const char *menuName ) {
	/**
	*	Clear existing menu layers and transient capture state.
	**/
	s_menuStackDepth = 0;
	s_keybindCaptureCmd.clear();
	s_currentStatusText = nullptr;

	/**
	*	Push the requested menu by name, or default to `"game"` / `"main"` based on connection state.
	**/
	// Try pushing the named menu if a non-empty name was provided.
	if ( menuName != nullptr && menuName[ 0 ] != '\0' ) {
		if ( MUI_PushMenuByName( menuName ) ) {
			return;
		}
	}

#if defined( CLGAME_INCLUDE )
	// Default to "game" when connected and active, otherwise "main".
	const bool inGame = ( clgi.GetConnectionState != nullptr ) && ( clgi.GetConnectionState() >= ca_active );
#else
	const bool inGame = ( cls.state >= ca_active );
#endif
	MUI_PushMenu( inGame ? mui_script_menu_id_t::GAME : mui_script_menu_id_t::MAIN );
}

/**
*	@brief	Open the root MicroUI menu (`main` or `game`), matching `UI_OpenMenu`.
*	@param	inGameMenu	True to open the `game` ("In-Game Menu"), false to open `main` ("Q2RTXPerimental").
**/
void CLG_MUI_OpenRootMenu( const bool inGameMenu ) {
	/**
	*	Clear existing menu layers and push the requested root menu.
	**/
	s_menuStackDepth = 0;
	s_keybindCaptureCmd.clear();
	s_currentStatusText = nullptr;
	MUI_PushMenu( inGameMenu ? mui_script_menu_id_t::GAME : mui_script_menu_id_t::MAIN );
}

/**
*	@brief	Push a Q2RTXP MicroUI menu onto the navigation stack by its `.menu` script name.
*	@param	menuName	Null-terminated script menu name (e.g. `"options"`, `"video"`, `"quit"`).
*	@return	True if the menu was found and pushed onto the stack.
**/
const bool CLG_MUI_PushMenuByName( const char *menuName ) {
	/**
	*	Delegate to MUI_PushMenuByName.
	**/
	return MUI_PushMenuByName( menuName );
}

/**
*	@brief	Pop the top-most Q2RTXP MicroUI menu from the stack, closing the menu system when empty.
**/
void CLG_MUI_PopMenu( void ) {
	/**
	*	If a keybind widget is currently capturing a key, cancel capture instead of popping the menu.
	**/
	// Cancel active keybind capture on Escape/pop first.
	if ( !s_keybindCaptureCmd.empty() ) {
		s_keybindCaptureCmd.clear();
		return;
	}

	/**
	*	Pop the top menu layer from the navigation stack.
	**/
	MUI_PopMenu();
}

/**
*	@brief	Close all open MicroUI script menus and clear the menu navigation stack.
**/
void CLG_MUI_CloseAllMenus( void ) {
	/**
	*	Clear stack depth and transient state without recursive close notifications.
	**/
	s_menuStackDepth = 0;
	s_keybindCaptureCmd.clear();
	s_currentStatusText = nullptr;
}

/**
*	@brief	Query whether any Q2RTXP MicroUI script menu is currently open on the stack.
*	@return	True if at least one menu is active on the stack.
**/
const bool CLG_MUI_IsMenuActive( void ) {
	/**
	*	Return true whenever at least one menu layer is present on the stack.
	**/
	return ( s_menuStackDepth > 0 );
}

/**
*	@brief	Handle key events for active Q2RTXP MicroUI menus (Escape back/close and `bind` key capture).
*	@param	key		Engine key code (`K_ESCAPE`, etc.).
*	@param	isDown	True when the key is pressed, false when released.
*	@return	True if the key event was consumed by the Q2RTXP menu stack.
**/
const bool CLG_MUI_HandleKeyEvent( const int32_t key, const bool isDown ) {
	/**
	*	Ignore key events when no Q2RTXP menu is open on the stack.
	**/
	// Return false if no menu is currently active.
	if ( s_menuStackDepth <= 0 ) {
		return false;
	}

	/**
	*	If a `bind` control is awaiting a key press, consume the event to cancel or complete capture.
	**/
	// Handle active keybind capture mode.
	if ( !s_keybindCaptureCmd.empty() ) {
		// Consume key release events while capturing.
		if ( !isDown ) {
			return true;
		}
		// Clear capture state once a key press arrives.
		s_keybindCaptureCmd.clear();
		return true;
	}

	return false;
}

/**
*	@brief	Process and emit MicroUI commands for the currently active menu on the stack.
*	@param	ctx	Active MicroUI context (must be called between `mu_begin( ctx )` and `mu_end( ctx )`).
*	@return	True if a menu is currently active and was processed this frame.
**/
const bool CLG_MUI_ProcessQ2RTXPMenus( mu_Context *ctx ) {
	/**
	*	Sanity check MicroUI context and ensure a menu is open on the stack.
	**/
	// Return false if context is null or no menu is currently open.
	if ( ctx == nullptr || s_menuStackDepth <= 0 ) {
		return false;
	}

	/**
	*	Resolve the top-most menu on the navigation stack and invoke its MicroUI builder.
	**/
	const mui_script_menu_id_t activeMenuId = s_menuStack[ s_menuStackDepth - 1 ];
	const mui_menu_registry_entry_t *entry = MUI_FindMenuEntryById( activeMenuId );

	// If the top menu entry is invalid, pop it and abort this frame.
	if ( entry == nullptr || entry->drawFunc == nullptr ) {
		MUI_PopMenu();
		return false;
	}

	// Build the active menu's MicroUI window and controls while guarding against mid-frame context re-init.
	s_isBuildingMenuFrame = true;
	entry->drawFunc( ctx );
	s_isBuildingMenuFrame = false;

	return ( s_menuStackDepth > 0 );
}

/**
*	@brief	Render the full-screen menu background (`conback.png`) behind the MicroUI menu windows
*			when not connected to an active game world (`ca_active`).
**/
void CLG_MUI_DrawBackground( void ) {
	/**
	*	Only draw the background when a menu is open on the stack.
	**/
	// Bail out if no menu is currently active.
	if ( s_menuStackDepth <= 0 ) {
		return;
	}

	const int32_t screenW = MUI_GetScreenWidth();
	const int32_t screenH = MUI_GetScreenHeight();

#if defined( CLGAME_INCLUDE )
	/**
	*	When disconnected (`!= ca_active`), render `conback.png` keeping aspect ratio across the screen
	*	(matching `Menu_Draw` in `src/client/ui/menu.cpp` and `background "conback.png"` in `q2rtxp.menu`).
	**/
	if ( clgi.GetConnectionState() != ca_active ) {
		const qhandle_t bgPic = clgi.R_RegisterPic( "conback.png" );
		if ( bgPic != 0 ) {
			clgi.R_ClearColor();
			clgi.R_ClearStyle();
			clgi.R_DrawKeepAspectPic( 0, 0, screenW, screenH, bgPic );
		}
	}
#else
	if ( cls.state != ca_active ) {
		const qhandle_t bgPic = R_RegisterPic( "conback.png" );
		if ( bgPic != 0 ) {
			R_ClearColor();
			R_ClearStyle();
			R_DrawKeepAspectPic( 0, 0, screenW, screenH, bgPic );
		}
	}
#endif
}
