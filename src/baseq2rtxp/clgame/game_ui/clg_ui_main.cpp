/********************************************************************
*
*
*	ClientGame: Core GameUI Implementation:
*
*
********************************************************************/
// Local ClientGame
#include "clgame/clg_local.h"
#include "clgame/clg_precache.h"
#include "clgame/clg_screen.h"

// MicroUI utility library.
extern "C" {
	#include "clgame/game_ui/microui-2.02/src/microui.h"

		// Track the last touched player checkbox so the mute label follows the player's state the user just selected.
		static int focusedMuteTeam = -1;
		static int focusedMutePlayer = -1;
	//! @brief	Enumerator for the icons in the UI atlas. These are used to identify which icon to draw when rendering UI elements that use icons, such as buttons or checkboxes. The ATLAS_WHITE icon is a simple white square that can be tinted with different colors when drawn, and the ATLAS_FONT icon is a placeholder for font characters that will be rendered using a bitmap font system. The actual texture coordinates for these icons will be defined in the UI rendering code, based on their position in the atlas texture.
	enum { ATLAS_WHITE = MU_ICON_MAX, ATLAS_FONT };

	//! The internal GameUI icon/font atlas. 
	extern unsigned char clg_ui_atlas_texture[ ];
	//! The rectangle definitions for each icon and font character in the atlas, indexed by the icon/font ID. These rectangles define the position and size of each icon/character in the atlas texture, and they are used when rendering UI elements that use these icons/characters to determine which part of the atlas texture to sample from.
	extern mu_Rect clg_ui_atlas[ ];
}


// CLGame UI
#include "clgame/game_ui/clg_ui_main.h"
// SGame UI.
#include "sharedgame/sg_game_ui.h"
// C++ helpers for safer string handling and containers used in UI callbacks.
#include <string>
#include <array>

// Scoreboard UI
#include "clgame/game_ui/menus/clg_ui_menu_scoreboard.h"

//! Define to use R_DrawString font rendering instead of the atlas font.
//#define USE_R_DRAW_STRING_FONT 1

//! Atlas dimensions for the embedded GameUI atlas texture copy passed to the renderer.
static constexpr int32_t CLG_UI_ATLAS_WIDTH = 128;
//! Atlas dimensions for the embedded GameUI atlas texture copy passed to the renderer.
static constexpr int32_t CLG_UI_ATLAS_HEIGHT = 128;
//! RGBA8 atlas format channel count.
static constexpr int32_t CLG_UI_ATLAS_CHANNELS = 4;
//! Total byte size for the atlas texture upload buffer.
static constexpr size_t CLG_UI_ATLAS_BYTE_COUNT =
	( CLG_UI_ATLAS_WIDTH * CLG_UI_ATLAS_HEIGHT * CLG_UI_ATLAS_CHANNELS );

// Forward declarations for internal GameUI functions, these are used to implement the different GameUI windows and the main frame processing function.
static void __process_frame( mu_Context *ctx );
static const bool __style_window( mu_Context *ctx );
static const bool __log_window( mu_Context *ctx );
static const bool __test_window( mu_Context *ctx );
static void write_log( const char *text );
static void MicroUI_SetTextMeasureCallbacks( mu_Context *ctx );

static std::string logbuf;
static bool logbuf_updated = false;
static float bg[ 3 ] = { 90, 95, 100 };

static void write_log( const char *text ) {
	if ( !logbuf.empty() ) {
		logbuf.push_back( '\n' );
	}
	logbuf += ( text ? text : "" );
	logbuf_updated = true;
}

/**
* 	@brief	Quote a raw command argument so spaces and punctuation survive tokenization.
* 	@param	arg	Raw argument text to quote.
* 	@return	Quoted argument text suitable for CL_ClientCommand.
**/
static std::string QuoteCommandArgument( const std::string &arg ) {
	std::string quoted;
	quoted.reserve( arg.size() + 2 );
	quoted.push_back( '"' );
	for ( const char ch : arg ) {
		if ( ch == '"' || ch == '\\' ) {
			quoted.push_back( '\\' );
		}
		quoted.push_back( ch );
	}
	quoted.push_back( '"' );
	return quoted;
}

/**
* 	@brief	Build a CSV list of the currently selected scoreboard players.
* 	@param	selectedClients	Client numbers for the selected players.
* 	@param	selCount	Total number of selected players.
* 	@param	entries	Scoreboard entry table.
* 	@return	Comma-separated player list suitable for server commands.
**/
static std::string BuildSelectedPlayerCsv( const int *selectedClients, const int selCount, const scoreboard_entry_t *entries ) {
	std::string csv;
	for ( int i = 0; i < selCount; i++ ) {
		if ( i > 0 ) {
			csv.push_back( ',' );
		}
		csv += entries[ selectedClients[ i ] ].clientName;
	}
	return csv;
}

/**
*
*
*
*	GameUI Core:
*
*
*
**/
// Global GameUI context, this is allocated when the client game initializes and freed when it shuts down, it's used to manage the state of the GameUI and handle user input events.
static struct clg_gameui_ctx {
	//! The GameUI context for MicroUI, this is allocated when the client game initializes and freed when it shuts down, it's used to manage the state of the UI and handle user input events.
	mu_Context *mu_ctx = nullptr;

	//! The currently active menu, this is set when the client receives a command from 
	//! the server to open a menu, and it's used to determine which GameUI window to render 
	//! and process input for in the main frame processing function.
	//! 
	//! Set to sg_game_ui_menu_id::NONE to indicate that no menu is active and the client 
	//! should return input focus to gameplay.
	sg_game_ui_menu_id activeMenuID = sg_game_ui_menu_id::NONE;

	//! Forces the next opened menu to build once before the visible frame so layout-dependent controls settle.
	bool layoutWarmupPending = false;

	struct {
		//! Handle for the GameUI atlas texture, this is loaded when the client game initializes and it's used to render all the icons and font glyphs in the GameUI.
		qhandle_t imageHandle = 0;
		//! Stores the byte data for the GameUI atlas texture, this is used to upload the atlas texture to the renderer when the client game initializes. We keep a copy of the atlas data in memory so we can re-upload it if needed, such as when the renderer is re-initialized or when the client game reloads.
		byte *textureCopy = nullptr;
	} atlas = {};
} s_gameui_ctx;

#ifdef USE_R_DRAW_STRING_FONT
	/**
	*	@brief	Calculate the width of the given text using the specified font. This is used by the UI context to layout text elements correctly.
	+	param font	The font to use for measuring the text. Monospaced font so.
	+	@param text	The text to measure the width of.
	+	@param len	The length of the text. If -1, the function will calculate the length using strlen.
	+	@return
	**/
	static int UI_GetDrawPicTextWidth( mu_Font font, const char *text, int len ) {
	if ( len == -1 ) { 
		len = strlen( text );
	}
	int x = 0;
	while ( len-- && *text ) {
		byte c = *text++;
		//draw_char( x, y, flags, c, font );
		if ( c != '\0' ) {
			x += CHAR_WIDTH;
		//}
		} else {
			x += CHAR_WIDTH / 2;
		}
	}

	return x;
	//return len * CHAR_WIDTH;//r_get_text_width( text, len );
	}
	/**
	*	@brief 
	*	@param	font 
	*	@return 
	**/
	static int UI_GetDrawPicTextHeight( mu_Font font ) {
		return CHAR_HEIGHT;
	}
#else
	/**
	*	@brief	Calculate the width of the given text using the specified font. This is used by the UI context to layout text elements correctly.
	+	param font	The font to use for measuring the text. Monospaced font so.
	+	@param text	The text to measure the width of.
	+	@param len	The length of the text. If -1, the function will calculate the length using strlen.
	+	@return
	**/
	static int UI_GetAtlasTextWidth( mu_Font font, const char *text, int len ) {
		int res = 0;
		for ( const char *p = text; *p && len--; p++ ) {
			res += clg_ui_atlas[ ATLAS_FONT + ( unsigned char )*p ].w;
		}
		return res;
	}
	/**
	*	@brief
	*	@param	font
	*	@return
	**/
	static int UI_GetAtlasTextHeight( mu_Font font ) {
		return 18;
	}
#endif

/**
*	@brief	Bind text measurement callbacks for the active font backend.
*	@param	ctx	Target MicroUI context.
**/
static void MicroUI_SetTextMeasureCallbacks( mu_Context *ctx ) {
#ifdef USE_R_DRAW_STRING_FONT
	ctx->text_width = UI_GetDrawPicTextWidth;
	ctx->text_height = UI_GetDrawPicTextHeight;
#else
	ctx->text_width = UI_GetAtlasTextWidth;
	ctx->text_height = UI_GetAtlasTextHeight;
#endif
}

/**
*	@brief	Allocate the client's UI context, called when the client begins and after loading plague has ended.
**/
void CLG_UI_AllocateContext() {
	// Allocate the MicroUI context in TAG_CLGAME_GAME_MICRO_UI.
	if ( !s_gameui_ctx.mu_ctx ) {
		s_gameui_ctx.mu_ctx = ( mu_Context * )clgi.TagMallocz( sizeof( mu_Context ), TAG_CLGAME_GAME_MICRO_UI );
	}
	// Register the atlas only once and pass renderer-owned memory because raw image backends free the provided pixel buffer.
	if ( s_gameui_ctx.atlas.textureCopy == nullptr ) {
		s_gameui_ctx.atlas.textureCopy = ( byte * )clgi.Z_Malloc( CLG_UI_ATLAS_BYTE_COUNT );
		// Copy the embedded atlas texture data to the renderer-owned buffer, so it can be safely passed to the renderer for uploading without risking use-after-free bugs if the renderer frees the buffer after upload.
		memcpy( s_gameui_ctx.atlas.textureCopy, clg_ui_atlas_texture, CLG_UI_ATLAS_BYTE_COUNT );
	}

	// Check if the atlas texture has already been registered with the renderer, if not, register it now. This allows us to avoid re-registering the texture and uploading it multiple times if the client game reloads or if the renderer is re-initialized, since we keep a copy of the atlas data in memory and can re-upload it as needed.
	if ( s_gameui_ctx.atlas.imageHandle == 0 ) {
		// Upload the render owned atlas copy.
		s_gameui_ctx.atlas.imageHandle = clgi.R_RegisterRawImage( "clg_ui_atlas", CLG_UI_ATLAS_WIDTH, CLG_UI_ATLAS_HEIGHT, s_gameui_ctx.atlas.textureCopy, IT_PIC, IF_PERMANENT | IF_SCRAP | IF_SRGB );
		// IMG_Load_RTX stores the pointer as image-owned pixel data, so transfer ownership here.
		s_gameui_ctx.atlas.textureCopy = nullptr;
	}
	// Initialize the UI context defaults before binding callbacks because mu_init clears the function pointers.
	mu_init( s_gameui_ctx.mu_ctx );
	// Bind the text metric callbacks immediately so the first menu frame has valid sizing data.
	MicroUI_SetTextMeasureCallbacks( s_gameui_ctx.mu_ctx );

}
/**
*	Free the client's UI context, called when the client disconnects and before clearing its state.
**/
void CLG_UI_FreeContext() {
	// Free the MicroUI context.
	if ( s_gameui_ctx.mu_ctx != nullptr ) {
		// Free the microui context.
		clgi.TagFree( s_gameui_ctx.mu_ctx );
		// Clear the pointer to the UI context to avoid dangling pointers and potential use-after-free bugs.
		s_gameui_ctx.mu_ctx = nullptr;
	}
	// Clear UI state so reconnect/disconnect cannot leave a stale menu active.
	s_gameui_ctx.activeMenuID = sg_game_ui_menu_id::NONE;
	s_gameui_ctx.layoutWarmupPending = false;
	// Release the registered atlas handle so a future UI allocation recreates it cleanly.
	if ( s_gameui_ctx.atlas.imageHandle != 0 ) {
		// Unregister the atlas texture from the renderer, this will free the texture and any associated resources on the renderer side, and it allows us to clean up properly when the client game shuts down or reloads.
		//clgi.R_UnregisterImage( s_gameui_ctx.atlas.imageHandle );
		// Clear the atlas image handle to indicate that it is no longer registered, this allows future UI allocations to know that they need to re-register the atlas texture if needed.
		s_gameui_ctx.atlas.imageHandle = 0;
	}
	// Determine the key event destination, if it's currently set to GameUI, switch it back to Game so that input is not left in a state where it is focused on a nonexistent UI after the context is freed.
	if ( ( clgi.GetKeyEventDestination() & KEY_GAME_UI ) != 0 ) {
		clgi.SetKeyEventDestination( KEY_GAME );
	}
	
	//clgi.FreeTags( TAG_CLGAME_UI );
	//clgi.FreeTags( TAG_CLGAME_GAME_MICRO_UI );
}

/**
*	@brief	Called each frame to update the client's UI state,
*			it runs at the same framerate as the client does so it remains responsive.
**/
void CLG_UI_ProcessFrame() {
	/**
	* 	We will only process the UI context if there is an active menu, this is determined by whether the activeMenuID 
	* 	is set to a valid menu ID or sg_game_ui_menu_id::NONE.This allows us to avoid unnecessary processing when no 
	* 	UI is active and return input focus to gameplay.
	**/
	#if 1
	if ( s_gameui_ctx.activeMenuID == sg_game_ui_menu_id::NONE ) {
		// Return input focus to gameplay if there is no active menu, this can happen if the server closes the menu by sending a command with sg_game_ui_menu_id::NONE, or if the client opened the menu but then closed it without returning focus to gameplay for some reason.
		if ( ( clgi.GetKeyEventDestination() & KEY_GAME_UI ) != 0 ) {
			clgi.SetKeyEventDestination( KEY_GAME );
		} else {
			// Input is already focused on gameplay, nothing to do.
		}
		return;
	}
	#endif
	/**
	*	Keep input routed to GameUI whenever a menu is active so UI widgets receive keyboard and mouse events.
	**/
	if ( ( clgi.GetKeyEventDestination() & KEY_GAME_UI ) == 0 ) {
		clgi.SetKeyEventDestination( KEY_GAME_UI );
	}
	/**
	* 	If there is no UI context, return early:
	**/
	if ( !s_gameui_ctx.mu_ctx ) {
		s_gameui_ctx.activeMenuID = sg_game_ui_menu_id::NONE;
		s_gameui_ctx.layoutWarmupPending = false;
		if ( ( clgi.GetKeyEventDestination() & KEY_GAME_UI ) != 0 ) {
			clgi.SetKeyEventDestination( KEY_GAME );
		}
		return;
	}
	/**
	* 	Process the UI context for this frame since there is an active menu. 
	* 	The __process_frame function will handle rendering the correct GameUI window based on the activeMenuID, 
	* 	and it will also handle any input events that were captured by the UI context since the last frame.
	**/
	if ( s_gameui_ctx.activeMenuID != sg_game_ui_menu_id::NONE ) {
		__process_frame( s_gameui_ctx.mu_ctx );
	}
	// The first frame after opening can change scrollbar/body layout once the content size becomes known.
	// Run one extra frame, so the visible frame starts with the settled geometry instead of stepping over
	// a few frames as scrollbars appear and layout widths recalculate.
	if ( s_gameui_ctx.layoutWarmupPending && s_gameui_ctx.activeMenuID != sg_game_ui_menu_id::NONE ) {
		s_gameui_ctx.layoutWarmupPending = false;
		__process_frame( s_gameui_ctx.mu_ctx );
	}
}

/**
*	@brief	Close the active GameUI menu and restore gameplay input focus.
**/
void CLG_UI_CloseMenu() {
	// Clear active menu so GameUI stops processing.
	s_gameui_ctx.activeMenuID = sg_game_ui_menu_id::NONE;
	s_gameui_ctx.layoutWarmupPending = false;

	// Reset MicroUI state so stale captured widget/focus state does not persist across menu re-opens.
	if ( s_gameui_ctx.mu_ctx ) {
		// Re-initialize the UI context to clear any captured state and prepare it for the next time a menu is opened.
		mu_init( s_gameui_ctx.mu_ctx );
		// Rebind the text measurement callbacks after the reset so the next open frame still has valid metrics.
		MicroUI_SetTextMeasureCallbacks( s_gameui_ctx.mu_ctx );
	}

	// Return input focus to gameplay if there is no active menu, this can happen if the server closes the menu by sending a command with sg_game_ui_menu_id::NONE, or if the client opened the menu but then closed it without returning focus to gameplay for some reason.
	if ( ( clgi.GetKeyEventDestination() & KEY_GAME_UI ) == 0 ) {
		clgi.SetKeyEventDestination( KEY_GAME );
	}
}

/**
*	@brief	Will open the specified menu, based on the menuID received from the server.
**/
void CLG_UI_OpenMenu( const sg_game_ui_menu_id menuID ) {
	// Reject invalid menu IDs so we do not switch focus to a nonexistent GameUI screen.
	if ( menuID <= sg_game_ui_menu_id::NONE || menuID >= sg_game_ui_menu_id::MAX_ID ) {
		return;
	}

	// If the menu is already open, keep things as is.
	if ( menuID != sg_game_ui_menu_id::NONE && menuID == s_gameui_ctx.activeMenuID ) {
		return;
	}

	// Close the console first so GameUI input can become the active destination without console interference.
	if ( clgi.Con_Close ) {
		clgi.Con_Close( true );
	}

	// Switch input focus to GameUI now that a valid menu exists.
	if ( ( clgi.GetKeyEventDestination() & KEY_GAME_UI ) == 0 ) {
		clgi.SetKeyEventDestination( KEY_GAME_UI );
	}

	// Remember the active menu so the frame/update path renders the correct GameUI window.
	s_gameui_ctx.activeMenuID = menuID;
	// Warm up the layout on the first visible frame so scrollbar-dependent sizing stabilizes before the user sees it.
	s_gameui_ctx.layoutWarmupPending = true;
}




/**
*
*
*
*	Game UI - User Input:
*
*	Fired by user input events when the engine's event destination is set to keyEventDest_t::GAME_UI.
*	It gives the client game a chance to handle user input events that are meant for the UI, such as when
*	user is navigating the in-game menu, etc.
*
*
*
**/
//! Called when the client receives a mouse move event, gives the client game a chance to handle it 
//! when the client's current input(key) event destination is set to keyEventDest_t::GAME_UI.
void CLG_UI_MouseMoveEvent( const int32_t x, const int32_t y ) {
	if ( !s_gameui_ctx.mu_ctx ) {
		return;
	}
	mu_input_mousemove( s_gameui_ctx.mu_ctx, x, y );
}
//! Called when the client receives a mouse button event, gives the client game a chance to handle it 
//! when the client's current input(key) event destination is set to keyEventDest_t::GAME_UI.
void CLG_UI_MouseButtonEvent( const int32_t button, const bool isDown, const int32_t x, const int32_t y ) {
	if ( !s_gameui_ctx.mu_ctx ) {
		return;
	}
	if ( isDown == true ) {
		mu_input_mousedown( s_gameui_ctx.mu_ctx, x, y, button );
	} else {
		mu_input_mouseup( s_gameui_ctx.mu_ctx, x, y, button );
	}
}
//! Called when the client receives a mouse scroll event, gives the client game a chance to handle it 
//! when the client's current input(key) event destination is set to keyEventDest_t::GAME_UI.
void CLG_UI_MouseScrollEvent( /*const int32_t scrollX*/ const int32_t scrollY ) {
	if ( !s_gameui_ctx.mu_ctx ) {
		return;
	}
	mu_input_scroll( s_gameui_ctx.mu_ctx, 0 /* scrollX */, scrollY);
}

//! Called when the client receives a key event, gives the client game a chance to handle it 
//! when the client's current key event destination is set to keyEventDest_t::GAME_UI.
void CLG_UI_KeyEvent( const int32_t key, const bool isDown ) {
	if ( !s_gameui_ctx.mu_ctx ) {
		return;
	}
	if ( key && isDown ) {
		mu_input_keydown( s_gameui_ctx.mu_ctx, key );
	} else if ( key && !isDown ) {
		mu_input_keyup( s_gameui_ctx.mu_ctx, key );
	}
}

//! Called when the client receives a character event, gives the client game a chance to handle it 
//! when the client's current key event destination is set to keyEventDest_t::GAME_UI.
void CLG_UI_TextInputEvent( const char *str ) {
	if ( !s_gameui_ctx.mu_ctx ) {
		return;
	}
	// Pass the input text to the UI context.
	mu_input_text( s_gameui_ctx.mu_ctx, str );
}



/**
*
*
*
*	Game UI - Drawing:
*
*
*
**/
/**
*	@brief	Iterate the mu drawing commands and render them using the client's rendering functions.
**/
void CLG_UI_DrawRenderCommands() {
	if ( !s_gameui_ctx.mu_ctx ) {
		return;
	}

	// The active drawing command, used for iterating the command list.
	mu_Command *muCmd = nullptr;
	// Iterate on to the next rendering command generated by the UI context, until there are no more commands to process.
	while ( mu_next_command( s_gameui_ctx.mu_ctx, &muCmd ) ) {
		// Process the command based on its type.
		switch ( muCmd->type ) {
			// Simple draw text command, render the text at the specified position with requested color.
			case MU_COMMAND_TEXT: {
				// Set the color for drawing the text.
				clgi.R_SetColor(
					MakeColor(
						muCmd->text.color.r,
						muCmd->text.color.g,
						muCmd->text.color.b,
						muCmd->text.color.a
					) 
				);
				
				mu_Rect dst = { muCmd->text.pos.x, muCmd->text.pos.y, 0, 0 };
				for ( const char *p = muCmd->text.str; *p; p++ ) {
					if ( ( *p & 0xc0 ) == 0x80 ) { continue; }
					int chr = mu_min( ( unsigned char )*p, 127 );
					mu_Rect src = clg_ui_atlas[ ATLAS_FONT + chr ];
					dst.w = src.w;
					dst.h = src.h;
					clgi.R_DrawPicEx( dst.x, dst.y, dst.w, dst.h, s_gameui_ctx.atlas.imageHandle, src.x, src.y, src.w, src.h );
					dst.x += dst.w;
				}
				//// Draw the text using the client's rendering function.
				//static constexpr int32_t yOffset = 2; // The extra pixel offset to apply to the y position when drawing text, to accommodate for the extra size added to the text's height in the font texture.
				//SCR_DrawStringMultiEx(
				//	muCmd->text.pos.x,
				//	muCmd->text.pos.y + yOffset, // Offset it 1 pixel unit, to accommodate for its (CHAR_HEIGHT + 2) extra size offset.
				//	0, // Flags, not used.
				//	MAX_STRING_CHARS, // Max text length, should be enough for any reasonable text command.
				//	( const char * )&muCmd->text.str,
				//	precache.screen.font_pic
				//);
				// Clear the color after drawing the text, to avoid affecting subsequent draw calls that don't specify a color.
				clgi.R_ClearColor( );
				break;
			}
			case MU_COMMAND_RECT: {
				// Clear the color before drawing the rectangle, to avoid affecting subsequent draw calls that don't specify a color.
				clgi.R_ClearColor( );
				//r_draw_rect( cmd->rect.rect, cmd->rect.color );
				clgi.R_DrawFill32( muCmd->rect.rect.x, muCmd->rect.rect.y, muCmd->rect.rect.w, muCmd->rect.rect.h,
					MakeColor(
						muCmd->rect.color.r,
						muCmd->rect.color.g,
						muCmd->rect.color.b,
						muCmd->rect.color.a
					)
				);
				// Clear the color before drawing the rectangle, to avoid affecting subsequent draw calls that don't specify a color.
				clgi.R_ClearColor();
				break;
			}
			case MU_COMMAND_ICON: {
				// Get the source rectangle for the icon from the atlas, and calculate the destination rectangle based on the command's specified position and size.
				mu_Rect rect = muCmd->icon.rect;
				mu_Rect src = clg_ui_atlas[ muCmd->icon.id ];
				// Calculate the destination position to draw the icon, centering it within the command's specified rectangle.
				const int32_t destinationX = rect.x + ( rect.w - src.w ) / 2;
				const int32_t destinationY = rect.y + ( rect.h - src.h ) / 2;
				// Apply color.
				clgi.R_SetColor(
					MakeColor(
						muCmd->icon.color.r,
						muCmd->icon.color.g,
						muCmd->icon.color.b,
						muCmd->icon.color.a
					)
				);
				// Draw the icon.
				clgi.R_DrawPicEx( destinationX, destinationY, src.w, src.h, s_gameui_ctx.atlas.imageHandle, src.x, src.y, src.w, src.h );
				// Clear the color after drawing the icon, to avoid affecting subsequent draw calls that don't specify a color.
				clgi.R_ClearColor();
				break;
			}
			case MU_COMMAND_CLIP: {
				// When the clip rect is the default max size, we can just disable clipping instead of setting a huge clip rect.
				if ( muCmd->clip.rect.x == 0 && muCmd->clip.rect.y == 0
					&& muCmd->clip.rect.w == 0x1000000 && muCmd->clip.rect.h == 0x1000000 )
				{
					clgi.R_SetClipRect( nullptr );
					break;
				}
				// Convert it to the client's expected input for VKPT setting its active clip rect.
				const clipRect_t clipRect = {
					muCmd->clip.rect.x,
					muCmd->clip.rect.x + muCmd->clip.rect.w,
					muCmd->clip.rect.y,
					muCmd->clip.rect.y + muCmd->clip.rect.h
				};
				// Apply.
				clgi.R_SetClipRect( &clipRect );
				break;
			}
			// In any other unknown command, we just reset the clip rect and clear the color to avoid affecting other rendering code outside of the UI that might rely on those states.
			default: {
				clgi.R_SetClipRect( nullptr );
				clgi.R_ClearColor();
				break;
			}
		}
	}
	// After processing all the commands for this frame, reset the clip rect to NULL to avoid affecting other rendering code outside of the UI.
	clgi.R_SetClipRect( nullptr );
}

/**
*
*
*
*	Temporary Shit:
*
*
*
**/


static const bool __test_window( mu_Context *ctx ) {
  /* do window */
	if ( mu_begin_window( ctx, "Demo Window", mu_rect( 40, 40, 520, 450 ) ) ) {
		mu_Container *win = mu_get_current_container( ctx );
		win->rect.w = mu_max( win->rect.w, 240 );
		win->rect.h = mu_max( win->rect.h, 300 );

		/* window info */
		if ( mu_header( ctx, "Window Info" ) ) {
			mu_Container *win = mu_get_current_container( ctx );
			char buf[ 64 ];
			int widths[ ] = { 108, -1 };
			mu_layout_row( ctx, 2, widths, 0 );
			mu_label( ctx, "Position:" );
			sprintf( buf, "%d, %d", win->rect.x, win->rect.y ); mu_label( ctx, buf );
			mu_label( ctx, "Size:" );
			sprintf( buf, "%d, %d", win->rect.w, win->rect.h ); mu_label( ctx, buf );
		}

		/* labels + buttons */
		if ( mu_header_ex( ctx, "Test Buttons", MU_OPT_EXPANDED ) ) {
			int widths[ ] = { 210, -110, -1 };
			mu_layout_row( ctx, 3, widths, 0 );
			mu_label( ctx, "Test buttons 1:" );
			if ( mu_button( ctx, "Button 1" ) ) { write_log( "Pressed button 1" ); }
			if ( mu_button( ctx, "Button 2" ) ) { write_log( "Pressed button 2" ); }
			mu_label( ctx, "Test buttons 2:" );
			if ( mu_button( ctx, "Button 3" ) ) { write_log( "Pressed button 3" ); }
			if ( mu_button( ctx, "Popup" ) ) { mu_open_popup( ctx, "Test Popup" ); }
			if ( mu_begin_popup( ctx, "Test Popup" ) ) {
				mu_button( ctx, "Hello" );
				mu_button( ctx, "World" );
				mu_end_popup( ctx );
			}
		}

		/* tree */
		if ( mu_header_ex( ctx, "Tree and Text", MU_OPT_EXPANDED ) ) {
			int widths[ ] = { 140, -1 };
			mu_layout_row( ctx, 2, widths, 0 );
			mu_layout_begin_column( ctx );
			if ( mu_begin_treenode( ctx, "Test 1" ) ) {
				if ( mu_begin_treenode( ctx, "Test 1a" ) ) {
					mu_label( ctx, "Hello" );
					mu_label( ctx, "world" );
					mu_end_treenode( ctx );
				}
				if ( mu_begin_treenode( ctx, "Test 1b" ) ) {
					if ( mu_button( ctx, "Button 1" ) ) { write_log( "Pressed button 1" ); }
					if ( mu_button( ctx, "Button 2" ) ) { write_log( "Pressed button 2" ); }
					mu_end_treenode( ctx );
				}
				mu_end_treenode( ctx );
			}
			if ( mu_begin_treenode( ctx, "Test 2" ) ) {
				int widths[ ] = { 54, 54 };
				mu_layout_row( ctx, 2, widths, 0 );
				if ( mu_button( ctx, "Button 3" ) ) { write_log( "Pressed button 3" ); }
				if ( mu_button( ctx, "Button 4" ) ) { write_log( "Pressed button 4" ); }
				if ( mu_button( ctx, "Button 5" ) ) { write_log( "Pressed button 5" ); }
				if ( mu_button( ctx, "Button 6" ) ) { write_log( "Pressed button 6" ); }
				mu_end_treenode( ctx );
			}
			if ( mu_begin_treenode( ctx, "Test 3" ) ) {
				static int checks[ 3 ] = { 1, 0, 1 };
				mu_checkbox( ctx, "Checkbox 1", &checks[ 0 ] );
				mu_checkbox( ctx, "Checkbox 2", &checks[ 1 ] );
				mu_checkbox( ctx, "Checkbox 3", &checks[ 2 ] );
				mu_end_treenode( ctx );
			}
			mu_layout_end_column( ctx );

			mu_layout_begin_column( ctx );
			{
				int widths[ ] = { -1 };
				mu_layout_row( ctx, 1, widths, 0 );
				mu_text( ctx, "Lorem ipsum dolor sit amet, consectetur adipiscing "
					"elit. Maecenas lacinia, sem eu lacinia molestie, mi risus faucibus "
					"ipsum, eu varius magna felis a nulla." );
			}
			mu_layout_end_column( ctx );
		}

		/* background color sliders */
		if ( mu_header_ex( ctx, "Background Color", MU_OPT_EXPANDED ) ) {
			int widthsRowA[ ] = { -78, -1 };
			mu_layout_row( ctx, 2, widthsRowA, 84 );
			/* sliders */
			mu_layout_begin_column( ctx );
			int widthsRowB[ ] = { 46, -1 };
			mu_layout_row( ctx, 2, widthsRowB, 0 );
			mu_label( ctx, "Red:" );   mu_slider( ctx, &bg[ 0 ], 0, 255 );
			mu_label( ctx, "Green:" ); mu_slider( ctx, &bg[ 1 ], 0, 255 );
			mu_label( ctx, "Blue:" );  mu_slider( ctx, &bg[ 2 ], 0, 255 );
			mu_layout_end_column( ctx );
			/* color preview */
			mu_Rect r = mu_layout_next( ctx );
			mu_draw_rect( ctx, r, mu_color( bg[ 0 ], bg[ 1 ], bg[ 2 ], 255 ) );
			char buf[ 32 ];
			sprintf( buf, "#%02X%02X%02X", ( int )bg[ 0 ], ( int )bg[ 1 ], ( int )bg[ 2 ] );
			mu_draw_control_text( ctx, buf, r, MU_COLOR_TEXT, MU_OPT_ALIGNCENTER );
		}

		mu_end_window( ctx );
		// Visible.
		return true;
	}
	// InVisible.
	return false;
}


static const bool __log_window( mu_Context *ctx ) {
	if ( mu_begin_window( ctx, "Log Window", mu_rect( 570, 40, 450, 200 ) ) ) {
	  /* output text panel */
		int widths[ ] = { -1 };
		mu_layout_row( ctx, 1, widths, -25 );
		mu_begin_panel( ctx, "Log Output" );
		mu_Container *panel = mu_get_current_container( ctx );
		mu_layout_row( ctx, 1, widths, -1 );
		mu_text( ctx, logbuf.c_str() );
		mu_end_panel( ctx );
		if ( logbuf_updated ) {
			panel->scroll.y = panel->content_size.y;
			logbuf_updated = false;
		}

		/* input textbox + submit button */
		static char buf[ 128 ];
		int submitted = 0;
		{
			int widths[ ] = { -90, -1 };
			mu_layout_row( ctx, 2, widths, 0 );
			// Treat Enter on the textbox as a submit action so the log update still happens.
			if ( mu_textbox( ctx, buf, sizeof( buf ) ) & MU_RES_SUBMIT ) {
				submitted = 1;
			}
			if ( mu_button( ctx, "Submit" ) ) { submitted = 1; }
		}
		if ( submitted ) {
			write_log( buf );
			buf[ 0 ] = '\0';
		}

		mu_end_window( ctx );

		// Visible.
		return true;
	}
	// InVisible.
	return false;
}


static int uint8_slider( mu_Context *ctx, unsigned char *value, int low, int high ) {
	static float tmp;
	mu_push_id( ctx, &value, sizeof( value ) );
	tmp = *value;
	int res = mu_slider_ex( ctx, &tmp, low, high, 0, "%.0f", MU_OPT_ALIGNCENTER );
	*value = tmp;
	mu_pop_id( ctx );
	return res;
}


static const bool __style_window( mu_Context *ctx ) {
	static struct { const char *label; int idx; } colors[ ] = {
		{ "text:",         MU_COLOR_TEXT        },
		{ "border:",       MU_COLOR_BORDER      },
		{ "windowbg:",     MU_COLOR_WINDOWBG    },
		{ "titlebgactive:",      MU_COLOR_TITLEBG_ACTIVE   },
		{ "titlebginactive:",    MU_COLOR_TITLEBG_INACTIVE },
		{ "titletext:",    MU_COLOR_TITLETEXT   },
		{ "panelbg:",      MU_COLOR_PANELBG     },
		{ "button:",       MU_COLOR_BUTTON      },
		{ "buttonhover:",  MU_COLOR_BUTTONHOVER },
		{ "buttonfocus:",  MU_COLOR_BUTTONFOCUS },
		{ "base:",         MU_COLOR_BASE        },
		{ "basehover:",    MU_COLOR_BASEHOVER   },
		{ "basefocus:",    MU_COLOR_BASEFOCUS   },
		{ "scrollbase:",   MU_COLOR_SCROLLBASE  },
		{ "scrollthumb:",  MU_COLOR_SCROLLTHUMB },
		{ nullptr }
	};

	if ( mu_begin_window( ctx, "Style Editor", mu_rect( 650, 320, 420, 320 ) ) ) {
		int sw = mu_get_current_container( ctx )->body.w * 0.14;
		int widths[ ] = { 88, sw, sw, sw, sw, -1 };
		mu_layout_row( ctx, 6, widths, 0 );
		for ( int i = 0; colors[ i ].label; i++ ) {
			mu_label( ctx, colors[ i ].label );
			uint8_slider( ctx, &ctx->style->colors[ i ].r, 0, 255 );
			uint8_slider( ctx, &ctx->style->colors[ i ].g, 0, 255 );
			uint8_slider( ctx, &ctx->style->colors[ i ].b, 0, 255 );
			uint8_slider( ctx, &ctx->style->colors[ i ].a, 0, 255 );
			mu_draw_rect( ctx, mu_layout_next( ctx ), ctx->style->colors[ i ] );
		}
		mu_end_window( ctx );
		// Visible.
		return true;
	}
	// InVisible.
	return false;
}

#if 0
/**
*	@brief	Draw the temporary team-selection scoreboard mockup.
* 	@note	Implements the requested two-team layout with per-player voting checkboxes,
* 			team-wide select toggles, alternating row colors, and fixed-width columns.
**/
static const bool __fake_scoreboard( mu_Context *ctx ) {
	// The number of players, for determining layout height.
	const int numberOfPlayers = 4;
	const int teamCount = 2;
	const int spacerRows = 1;
	const int footerSpacerRows = 1;
	const int actionRows = 1;

	// Keep the window sized to the fixed table geometry plus the window padding.
	const int checkboxWidth = 24;
	const int nameWidth = 216;
	const int scoreWidth = 52;
	const int pingWidth = 52;
	const int rowWidthTotal = checkboxWidth + nameWidth + scoreWidth + pingWidth;
	// Include the three inter-column gaps so the drawn bars reach the same visual width as the layout.
	const int rowWidthVisual = rowWidthTotal + ( ctx->style->spacing * 3 );
	const int scoreBoardWidth = rowWidthVisual + ( ctx->style->padding * 2 );
	const int rowHeight = 24;
	const int totalRows = ( teamCount * ( numberOfPlayers + 1 ) ) + spacerRows + footerSpacerRows + actionRows;
	const int contentHeight = ( totalRows * rowHeight ) + ( ( totalRows - 1 ) * ctx->style->spacing );
	const int scoreBoardHeight = contentHeight + ( ctx->style->padding * 2 ) - ctx->style->spacing;
	const int scoreBoardX = ( clgi.screen->screenWidth - scoreBoardWidth ) / 2;
	const int scoreBoardY = ( clgi.screen->screenHeight - scoreBoardHeight ) / 2;
	
	/* No title. */
	const int opt = MU_OPT_AUTOSIZE | MU_OPT_ALIGNCENTER | MU_OPT_NOCLOSE | MU_OPT_HOLDFOCUS |
		MU_OPT_KEEPFOCUS | MU_OPT_NODRAG |
		/* No resizing or scrolling.*/
		MU_OPT_NORESIZE | MU_OPT_NOSCROLL;
	
	const char *name = "Hostage Reacquisition:"; // Should be gamemode name.
	if ( mu_begin_window_ex( ctx, name, mu_rect( scoreBoardX, scoreBoardY, 0, scoreBoardHeight ), opt ) ) {
		//mu_header_ex( ctx, "Scoreboard:", MU_OPT_ALIGNCENTER );
		/**
		* 	Keep the scoreboard state persistent so checkbox interactions survive frame updates.
		**/
		static int teamSelections[ 2 ] = { 0, 0 };
		static int playerSelections[ 2 ][ 4 ] = {
			{ 0, 0, 0, 0 },
			{ 0, 0, 0, 0 }
		};
		static const char *teamNames[ 2 ] = {
			"Team:    Outlaws",
			"Team:    ScummieGummies"
		};
		static const char *playerNames[ 2 ][ 4 ] = {
			{ "PlayerWithANose", "Hangman_Noose69", "EpicLulzBro", "Muffinator" },
			{ "Spoike", "WatIsDeze", "Rambo", "Steven Seagull" }
		};
		const int rowWidths[ 4 ] = { checkboxWidth, nameWidth, scoreWidth, pingWidth };
		const mu_Color rowColorDark = mu_color( 0x44, 0x44, 0x44, 196 );
		const mu_Color rowColorLight = mu_color( 0x55, 0x55, 0x55, 196 );
		const mu_Color headerColorA = mu_color( 255, 120, 0, 183 );
		const mu_Color headerColorB = mu_color( 255, 100, 0, 128 );
		const mu_Color separatorColor = mu_color( 255, 144, 90, 235 );
		const char *scoreText[ ] = {
			"1337",
			"12",
			"8",
			"0"
		};
		const char *pingText[ ] = {
			"50ms",
			"120ms",
			"200ms",
			"999ms"
		};

		/**
		* 	Build both team sections with identical column layout and placeholder data.
		**/
		for ( int team = 0; team < 2; team++ ) {
			/**
			* 	Synchronize the team-wide checkbox with the current player selections before drawing.
			**/
			int allSelected = 1;
			for ( int player = 0; player < numberOfPlayers; player++ ) {
				if ( playerSelections[ team ][ player ] == 0 ) {
					allSelected = 0;
					break;
				}
			}
			teamSelections[ team ] = allSelected;

			/**
			* 	Draw the team header row and allow it to toggle every player in that team.
			**/
			mu_layout_row( ctx, 4, rowWidths, rowHeight );
			mu_Rect headerRect = mu_layout_next( ctx );
			mu_layout_set_next( ctx, headerRect, 0 );
			headerRect.w = rowWidthVisual;
			mu_draw_rect( ctx, headerRect, ( team == 0 ? headerColorA : headerColorB ) );
			//mu_draw_rect( ctx, mu_rect( headerRect.x + checkboxWidth + ctx->style->spacing, headerRect.y, 1, headerRect.h ), separatorColor );
			mu_draw_rect( ctx, mu_rect( headerRect.x + checkboxWidth + nameWidth, headerRect.y, 1, headerRect.h ), separatorColor );
			mu_draw_rect( ctx, mu_rect( headerRect.x + checkboxWidth + nameWidth + scoreWidth, headerRect.y, 1, headerRect.h ), separatorColor );

			int teamSelectionChanged = mu_checkbox( ctx, "", &teamSelections[ team ] );
			mu_label( ctx, teamNames[ team ] );
			mu_label_ex( ctx, "Score:", MU_COLOR_TEXT, MU_OPT_ALIGNRIGHT );
			mu_label_ex( ctx, "Ping:", MU_COLOR_TEXT, MU_OPT_ALIGNRIGHT );

			/**
			* 	If the team checkbox changed, propagate the new value to every player row.
			*	This will allow the user to select/deselect all players in a team with one 
			*	click, and it will also keep the team checkbox state in sync if the user 
			*	manually toggles every player row checkbox.
			**/
			if ( ( teamSelectionChanged & MU_RES_CHANGE ) != 0 ) {
				// Team-wide selection changes affect multiple players, so clear any single-player focus.
				focusedMuteTeam = -1;
				focusedMutePlayer = -1;
				for ( int player = 0; player < numberOfPlayers; player++ ) {
					playerSelections[ team ][ player ] = teamSelections[ team ];
				}
			}

			/**
			* 	Emit the four placeholder player rows, alternating the fill color each time.
			**/
			for ( int player = 0; player < numberOfPlayers; player++ ) {
				mu_layout_row( ctx, 4, rowWidths, rowHeight );
				mu_Rect rowRect = mu_layout_next( ctx );
				mu_layout_set_next( ctx, rowRect, 0 );
				rowRect.w = rowWidthVisual;
				mu_draw_rect( ctx, rowRect, ( player & 1 ) ? rowColorLight : rowColorDark );
				//mu_draw_rect( ctx, mu_rect( rowRect.x + checkboxWidth, rowRect.y, 1, rowRect.h ), separatorColor );
				mu_draw_rect( ctx, mu_rect( rowRect.x + checkboxWidth + nameWidth, rowRect.y, 1, rowRect.h ), separatorColor );
				mu_draw_rect( ctx, mu_rect( rowRect.x + checkboxWidth + nameWidth + scoreWidth, rowRect.y, 1, rowRect.h ), separatorColor );

				const int playerSelectionChanged = mu_checkbox( ctx, "", &playerSelections[ team ][ player ] );
				// Remember the last player checkbox the user touched so the footer label can follow a single-player selection.
				if ( ( playerSelectionChanged & MU_RES_CHANGE ) != 0 ) {
					if ( playerSelections[ team ][ player ] ) {
						focusedMuteTeam = team;
						focusedMutePlayer = player;
					} else if ( focusedMuteTeam == team && focusedMutePlayer == player ) {
						focusedMuteTeam = -1;
						focusedMutePlayer = -1;
					}
				}
				mu_label( ctx, playerNames[ team ][ player ] );
				mu_label_ex( ctx, scoreText[ player ], MU_COLOR_TEXT, MU_OPT_ALIGNRIGHT );
				mu_label_ex( ctx, pingText[ player ], MU_COLOR_TEXT, MU_OPT_ALIGNRIGHT );
			}

			/**
			* 	Insert the requested blank separator row between the two teams.
			**/
			if ( team == 0 ) {
				mu_layout_row( ctx, 4, rowWidths, ctx->style->padding );
				mu_label( ctx, "" );
				mu_label( ctx, "" );
				mu_label( ctx, "" );
				mu_label( ctx, "" );
			}

			/**
			* 	Refresh the team header state so it reflects the final per-player selection state.
			**/
			teamSelections[ team ] = 1;
			for ( int player = 0; player < 4; player++ ) {
				if ( playerSelections[ team ][ player ] == 0 ) {
					teamSelections[ team ] = 0;
					break;
				}
			}
		}

		/**
		* 	Add a small footer spacer so the action buttons sit a bit farther below the player list.
		**/
		mu_layout_row( ctx, 4, rowWidths, ctx->style->padding );
		mu_label( ctx, "" );
		mu_label( ctx, "" );
		mu_label( ctx, "" );
		mu_label( ctx, "" );

		/**
		* 	Action row: Mute/Unmute (left aligned under the name column), Vote Kick, Vote Ban (right aligned across the score+ping columns).
		* 	Buttons operate on all currently selected players. Basic per-player muted state is tracked locally and actions are logged.
		**/
		/**
		*  Track per-player muted state locally in the UI using a small
		*  fixed-size C++ container for clarity and type-safety.
		**/
		static std::array<std::array<int, 4>, 2> playerMuted = {};

		// Collect selected players and determine if any selected are muted.
		int selTeam[ 8 ];
		int selIndex[ 8 ];
		int selCount = 0;
		bool anyMutedSelected = false;
		for ( int t = 0; t < 2; t++ ) {
			for ( int p = 0; p < numberOfPlayers; p++ ) {
				if ( playerSelections[ t ][ p ] ) {
					selTeam[ selCount ] = t;
					selIndex[ selCount ] = p;
					selCount++;
					if ( playerMuted[ t ][ p ] ) { anyMutedSelected = true; }
				}
			}
		}

		const char *voteKickLabel = "Vote Kick";
		const char *voteBanLabel = "Vote Ban";
		const bool focusedMutePlayerValid =
			focusedMuteTeam >= 0 && focusedMuteTeam < 2 &&
			focusedMutePlayer >= 0 && focusedMutePlayer < 4 &&
			playerSelections[ focusedMuteTeam ][ focusedMutePlayer ] != 0;
		const char *muteLabel = "Mute";
		if ( selCount > 0 ) {
			// Let the footer label follow the most recently touched selected player when we have one.
			if ( focusedMutePlayerValid ) {
				muteLabel = playerMuted[ focusedMuteTeam ][ focusedMutePlayer ] ? "Unmute" : "Mute";
			} else {
				muteLabel = anyMutedSelected ? "Unmute" : "Mute";
			}
		}
		const std::string selectedCsv = BuildSelectedPlayerCsv( selTeam, selCount, playerNames );

		// Measure button widths based on the current font metrics so layout is precise.
		const int muteButtonWidth = ctx->text_width( ctx->style->font, muteLabel, -1 ) + ( ctx->style->padding * 2 );
		const int voteKickButtonWidth = ctx->text_width( ctx->style->font, voteKickLabel, -1 ) + ( ctx->style->padding * 2 );
		const int voteBanButtonWidth = ctx->text_width( ctx->style->font, voteBanLabel, -1 ) + ( ctx->style->padding * 2 );

		// Use the same 4-column layout as the players.
		mu_layout_row( ctx, 4, rowWidths, rowHeight );

		// Skip the checkbox column so the mute button sits under the name column.
		mu_layout_next( ctx ); // checkbox column
		mu_Rect nameCol = mu_layout_next( ctx ); // name column rect
		mu_Rect muteRect = nameCol;
		muteRect.w = muteButtonWidth;
		mu_layout_set_next( ctx, muteRect, 0 );
		if ( mu_button( ctx, muteLabel ) ) {
			if ( selCount > 0 ) {
				// Toggle: if any were muted, unmute all; otherwise mute all.
				const int setMuted = anyMutedSelected ? 0 : 1;
				for ( int i = 0; i < selCount; i++ ) {
					int tt = selTeam[ i ];
					int pp = selIndex[ i ];
					playerMuted[ tt ][ pp ] = setMuted;
				}
				const std::string msg = ( setMuted ? "Muted: " : "Unmuted: " ) + selectedCsv;
				write_log( msg.c_str() );
			}
		}

		// Reserve the score+ping columns for the two right-aligned action buttons.
		mu_Rect scoreCol = mu_layout_next( ctx );
		mu_Rect pingCol = mu_layout_next( ctx );
		mu_Rect combined = scoreCol;
		combined.w = ( pingCol.x + pingCol.w ) - scoreCol.x;

		const int spacing = ctx->style->spacing;
		const int totalButtonsW = voteKickButtonWidth + spacing + voteBanButtonWidth;
		int startX = combined.x + combined.w - totalButtonsW;
		int y = combined.y;
		int h = combined.h;

		mu_Rect kickRect = mu_rect( startX, y, voteKickButtonWidth, h );
		mu_layout_set_next( ctx, kickRect, 0 );
		if ( mu_button( ctx, voteKickLabel ) ) {
			if ( selCount > 0 ) {
				// Log locally and forward the votekick command to the server as a single quoted CSV argument.
				const std::string logmsg = std::string( "VoteKick: " ) + selectedCsv;
				write_log( logmsg.c_str() );
				const std::string cmd = std::string( "votekick " ) + QuoteCommandArgument( selectedCsv );
				clgi.CL_ClientCommand( cmd.c_str() );
			}
		}

		mu_Rect banRect = mu_rect( startX + voteKickButtonWidth + spacing, y, voteBanButtonWidth, h );
		mu_layout_set_next( ctx, banRect, 0 );
		if ( mu_button( ctx, voteBanLabel ) ) {
			if ( selCount > 0 ) {
				// Log locally and forward the voteban command to the server as a single quoted CSV argument.
				const std::string logmsg = std::string( "VoteBan: " ) + selectedCsv;
				write_log( logmsg.c_str() );
				const std::string cmd = std::string( "voteban " ) + QuoteCommandArgument( selectedCsv );
				clgi.CL_ClientCommand( cmd.c_str() );
			}
		}
		mu_end_window( ctx );
		// Visible.
		return true;
	}
	// InVisible.
	return false;
}
#endif

static void __process_frame( mu_Context *ctx ) {
	// Used for automatically returning control to whichever state it was.
	int32_t windowsVisible = 0;

	// Begin actual menu building.
	mu_begin( ctx );
	{
		//windowsVisible += __style_window( ctx );
		//windowsVisible += __log_window( ctx );
		//windowsVisible += __test_window( ctx );
		windowsVisible += CLG_MUI_ProcessScoreBoard( ctx );
	}
	// Done building menus.
	mu_end( ctx );

	// When no windows are visible:
	if ( !windowsVisible ) {
		// None are visible so resort to closing the actual menu.
		CLG_UI_CloseMenu();
	}
}