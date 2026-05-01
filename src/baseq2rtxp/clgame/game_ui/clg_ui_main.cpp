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
// UI
extern "C" {
	#include "clgame/game_ui/microui-2.02/src/microui.h"
}

// Local UI
#include "clgame/game_ui/clg_ui_main.h"

#include "sharedgame/sg_game_ui.h"

/**
*
*
*
*	Temporary Shit:
*
*
*
**/
static  char logbuf[ 64000 ];
static   int logbuf_updated = 0;
static float bg[ 3 ] = { 90, 95, 100 };


static void write_log( const char *text ) {
	if ( logbuf[ 0 ] ) { strcat( logbuf, "\n" ); }
	strcat( logbuf, text );
	logbuf_updated = 1;
}


static void __test_window( mu_Context *ctx ) {
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
			int widths[ ] = { 188, -110, -1 };
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
			int widths[] = { 140, -1 };
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
				{
					int widths[ ] = { 144, -110, -1 };
					mu_layout_row( ctx, 2, widths, 0 );
					if ( mu_button( ctx, "Button 3" ) ) { write_log( "Pressed button 3" ); }
					if ( mu_button( ctx, "Button 4" ) ) { write_log( "Pressed button 4" ); }
					if ( mu_button( ctx, "Button 5" ) ) { write_log( "Pressed button 5" ); }
					if ( mu_button( ctx, "Button 6" ) ) { write_log( "Pressed button 6" ); }
					mu_end_treenode( ctx );
				}
				{
					int widths[ ] = { 144, -110, -1 };
					mu_layout_row( ctx, 2, widths, 0 );
					if ( mu_button( ctx, "Button 7" ) ) { write_log( "Pressed button 7" ); }
					if ( mu_button( ctx, "Button 8" ) ) { write_log( "Pressed button 8" ); }
					if ( mu_button( ctx, "Button 9" ) ) { write_log( "Pressed button 9" ); }
					if ( mu_button( ctx, "Button 10" ) ) { write_log( "Pressed button 10" ); }
				}
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
			mu_layout_row( ctx, 2, widthsRowA, 74 );
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
	}
}


static void __log_window( mu_Context *ctx ) {
	if ( mu_begin_window( ctx, "Log Window", mu_rect( 570, 40, 450, 200 ) ) ) {
	  /* output text panel */
		int widths[ ] = { -1 };
		mu_layout_row( ctx, 1, widths, -25 );
		mu_begin_panel( ctx, "Log Output" );
		mu_Container *panel = mu_get_current_container( ctx );
		mu_layout_row( ctx, 1, widths, -1 );
		mu_text( ctx, logbuf );
		mu_end_panel( ctx );
		if ( logbuf_updated ) {
			panel->scroll.y = panel->content_size.y;
			logbuf_updated = 0;
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
	}
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


static void __style_window( mu_Context *ctx ) {
	static struct { const char *label; int idx; } colors[ ] = {
	  { "text:",         MU_COLOR_TEXT        },
	  { "border:",       MU_COLOR_BORDER      },
	  { "windowbg:",     MU_COLOR_WINDOWBG    },
	  { "titlebg:",      MU_COLOR_TITLEBG     },
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
	  { NULL }
	};

	if ( mu_begin_window( ctx, "Style Editor", mu_rect( 650, 320, 420, 320 ) ) ) {
		int sw = mu_get_current_container( ctx )->body.w * 0.14;
		int widths[] = { 88, sw, sw, sw, sw, -1 };
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
	}
}

static void __process_frame( mu_Context *ctx ) {
	mu_begin( ctx );
	{
		__style_window( ctx );
		__log_window( ctx );
		__test_window( ctx );
	}
	mu_end( ctx );
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
static struct {
	game_ui_menu_id activeMenuID;
} s_gameui_ctx;
// MicroUI context, this is allocated when the client game initializes and freed when it shuts down, it's used to manage the state of the UI and handle user input events.
static mu_Context *s_ui_ctx = nullptr;

/**
*	@brief	Calculate the width of the given text using the specified font. This is used by the UI context to layout text elements correctly.
+	param font	The font to use for measuring the text. In this example, we ignore the font parameter and assume a fixed width for simplicity.
+	@param text	The text to measure the width of.
+	@param len	The length of the text. If -1, the function will calculate the length using strlen.
+	@return
**/
static int UI_GetTextWidth( mu_Font font, const char *text, int len ) {
	if ( len == -1 ) { 
		len = strlen( text );
	}
	int x = 0;
	while ( len-- && *text ) {
		//byte c = *text++;
		//draw_char( x, y, flags, c, font );
		//if ( c != '\0' ) {
			x += CHAR_WIDTH;
		//}
		//} else {
		//	x += CHAR_WIDTH / 2;
		//}
	}

	return x;
	//return len * CHAR_WIDTH;//r_get_text_width( text, len );
}
/**
 * @brief 
 * @param font 
 * @return 
 */
static int UI_GetTextHeight( mu_Font font ) {
	return CHAR_HEIGHT;
}

/**
*	@brief	Allocate the client's UI context, called when the client begins and after loading plague has ended.
**/
void CLG_UI_AllocateContext() {
	// Allocate a UI context.
	s_ui_ctx = ( mu_Context*)clgi.TagMallocz( sizeof( mu_Context ), TAG_CLGAME );

	// Init the UI context.
	mu_init( s_ui_ctx );

	// Setup the UI context's text measurement callbacks, these are used by the UI context to layout text elements correctly.
	s_ui_ctx->text_width = UI_GetTextWidth;
	s_ui_ctx->text_height = UI_GetTextHeight;
}
/**
*	Free the client's UI context, called when the client disconnects and before clearing its state.
**/
void CLG_UI_FreeContext() {
	// Free the UI context.
	clgi.TagFree( s_ui_ctx );
}

/**
*	@brief	Called each frame to update the client's UI state,
*			it runs at the same framerate as the client does so it remains responsive.
**/
void CLG_UI_ProcessFrame() {
	// If there is no UI context, return early.
	if ( !s_ui_ctx ) {
		return;
	}
	// Process the UI context for this frame.
	if ( s_gameui_ctx.activeMenuID == game_ui_menu_id::TEAM ) {
		__process_frame( s_ui_ctx );
	}
}

/**
*	@brief	Close the active GameUI menu and restore gameplay input focus.
**/
void CLG_UI_CloseMenu() {
	// Clear active menu so GameUI stops processing.
	s_gameui_ctx.activeMenuID = game_ui_menu_id::NONE;
	// Reset MicroUI state so stale captured widget/focus state does not persist across menu re-opens.
	if ( s_ui_ctx ) {
		mu_init( s_ui_ctx );
		s_ui_ctx->text_width = UI_GetTextWidth;
		s_ui_ctx->text_height = UI_GetTextHeight;
	}

	// Return input focus to gameplay.
	clgi.SetKeyEventDestination( KEY_GAME );
}

/**
*	@brief	Will open the specified menu, based on the menuID received from the server.
**/
void CLG_UI_OpenMenu( const game_ui_menu_id menuID ) {
	// Reject invalid menu IDs so we do not switch focus to a nonexistent GameUI screen.
	if ( menuID <= game_ui_menu_id::NONE || menuID >= game_ui_menu_id::MAX_ID ) {
		return;
	}

	// Close the console first so GameUI input can become the active destination without console interference.
	if ( clgi.Con_Close ) {
		clgi.Con_Close( true );
	}

	// Switch input focus to GameUI now that a valid menu exists.
	clgi.SetKeyEventDestination( KEY_GAME_UI );

	// Remember the active menu so the frame/update path renders the correct GameUI window.
	s_gameui_ctx.activeMenuID = menuID;
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
	// Pass the mouse move event to the UI context.
	mu_input_mousemove( s_ui_ctx, x, y );
}
//! Called when the client receives a mouse button event, gives the client game a chance to handle it 
//! when the client's current input(key) event destination is set to keyEventDest_t::GAME_UI.
void CLG_UI_MouseButtonEvent( const int32_t button, const bool isDown, const int32_t x, const int32_t y ) {
	// Pass the mouse button event to the UI context.
	if ( isDown == true ) {
		mu_input_mousedown( s_ui_ctx, x, y, button );
	} else {
		mu_input_mouseup( s_ui_ctx, x, y, button );
	}
}
//! Called when the client receives a mouse scroll event, gives the client game a chance to handle it 
//! when the client's current input(key) event destination is set to keyEventDest_t::GAME_UI.
void CLG_UI_MouseScrollEvent( /*const int32_t scrollX*/ const int32_t scrollY ) {
	mu_input_scroll( s_ui_ctx, 0 /* scrollX */, scrollY);
}

//! Called when the client receives a key event, gives the client game a chance to handle it 
//! when the client's current key event destination is set to keyEventDest_t::GAME_UI.
void CLG_UI_KeyEvent( const int32_t key, const bool isDown ) {
	if ( key && isDown ) {
		mu_input_keydown( s_ui_ctx, key );
	} else if ( key && !isDown ) {
		mu_input_keyup( s_ui_ctx, key );
	}
}

//! Called when the client receives a character event, gives the client game a chance to handle it 
//! when the client's current key event destination is set to keyEventDest_t::GAME_UI.
void CLG_UI_TextInputEvent( const char *str ) {
	// Pass the input text to the UI context.
	mu_input_text( s_ui_ctx, str );
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
	// The active drawing command, used for iterating the command list.
	mu_Command *muCmd = nullptr;
	// Iterate on to the next rendering command generated by the UI context, until there are no more commands to process.
	while ( mu_next_command( s_ui_ctx, &muCmd ) ) {
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
				// Draw the text using the client's rendering function.
				static constexpr int32_t yOffset = 2; // The extra pixel offset to apply to the y position when drawing text, to accommodate for the extra size added to the text's height in the font texture.
				SCR_DrawStringMultiEx(
					muCmd->text.pos.x,
					muCmd->text.pos.y + yOffset, // Offset it 1 pixel unit, to accommodate for its (CHAR_HEIGHT + 2) extra size offset.
					0, // Flags, not used.
					MAX_STRING_CHARS, // Max text length, should be enough for any reasonable text command.
					( const char * )&muCmd->text.str,
					precache.screen.font_pic
				);
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
				//r_draw_icon( cmd->icon.id, cmd->icon.rect, cmd->icon.color );
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


#if 0
/**
*	@brief	Called when the client receives a UI input event, it updates the UI context's input state accordingly.
**/
const bool CLG_UI_HandleInputEvent( const clgame_input_event_t *event ) {
	// Handle the input event for the UI context.
	switch ( event->type ) {
	case CLGAME_INPUT_EVENT_MOUSEMOVE:
		s_ui_ctx->mouse_pos.x = event->data.mousemove.x;
		s_ui_ctx->mouse_pos.y = event->data.mousemove.y;
		return true;
	case CLGAME_INPUT_EVENT_MOUSEBUTTON:
		if ( event->data.mousebutton.down ) {
			s_ui_ctx->mouse_down |= 1 << event->data.mousebutton.button;
			s_ui_ctx->mouse_pressed |= 1 << event->data.mousebutton.button;
		} else {
			s_ui_ctx->mouse_down &= ~( 1 << event->data.mousebutton.button );
		}
		return true;
	case CLGAME_INPUT_EVENT_KEY:
		if ( event->data.key.down ) {
			s_ui_ctx->key_down |= 1 << event->data.key.key;
			s_ui_ctx->key_pressed |= 1 << event->data.key.key;
		} else {
			s_ui_ctx->key_down &= ~( 1 << event->data.key.key );
		}
		return true;
	case CLGAME_INPUT_EVENT_TEXT:
		strncpy( s_ui_ctx->input_text, event->data.text.text, sizeof( s_ui_ctx->input_text ) - 1 );
		return true;
	default:
		return false;
	}
}
#endif