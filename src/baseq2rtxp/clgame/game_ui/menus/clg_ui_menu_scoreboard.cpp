/********************************************************************
*
*
*	ClientGame: ScoreBoard GameUI Implementation:
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

	enum {
		ATLAS_WHITE = MU_ICON_MAX,
		ATLAS_FONT
	};

	//! The internal GameUI icon/font atlas.
	extern unsigned char clg_ui_atlas_texture[ ];
	//! Rectangle definitions for icons and font characters inside the UI atlas.
	extern mu_Rect clg_ui_atlas[ ];
}

// CLGame UI
#include "clgame/game_ui/clg_ui_main.h"
// SGame UI.
#include "sharedgame/sg_game_ui.h"
#include <array>
#include <algorithm>
#include <cstdio>
#include <string>
#include "shared/client/scoreboard.h"

// Scoreboard UI
#include "clgame/game_ui/menus/clg_ui_menu_scoreboard.h"

//! Toggle optional team grouping in the scoreboard UI.
//! Keep this disabled until server-side team metadata is implemented and transmitted.
#ifndef CLG_SCOREBOARD_ENABLE_TEAMS
#define CLG_SCOREBOARD_ENABLE_TEAMS 0
#endif

#if CLG_SCOREBOARD_ENABLE_TEAMS
//! Number of teams expected by the temporary grouped scoreboard layout.
static constexpr int32_t CLG_SCOREBOARD_TEAM_COUNT = 2;
#endif


/**
*	@brief	Quote a raw command argument so spaces and punctuation survive tokenization.
*	@param	arg	Raw argument text to quote.
*	@return	Quoted argument text suitable for CL_ClientCommand.
**/
static std::string CLG_Scoreboard_QuoteCommandArgument( const std::string &arg ) {
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
*	@brief	Build a CSV list of the currently selected scoreboard players.
*	@param	selectedClients	Client numbers for the selected players.
*	@param	selCount	Total number of selected players.
*	@param	entries	Scoreboard entry table.
*	@return	Comma-separated player list suitable for server commands.
**/
static std::string CLG_Scoreboard_BuildSelectedPlayerCsv( const int *selectedClients, const int selCount, const scoreboard_entry_t *entries ) {
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
*	@brief	Count live scoreboard entries and cache their indices.
*	@param	entries	Live scoreboard table received from the client import API.
*	@param	outIndices	[out] Indices of valid live entries.
*	@return	Total number of live clients currently present in the scoreboard snapshot.
**/
static int32_t CLG_Scoreboard_BuildLiveClientList( const scoreboard_entry_t *entries, std::array<int32_t, MAX_CLIENTS> &outIndices ) {
	int32_t liveCount = 0;

	/**
	*	Walk all slots and keep only currently valid live client entries.
	**/
	for ( int32_t i = 0; i < MAX_CLIENTS; i++ ) {
		if ( entries[ i ].isValidSlot ) {
			outIndices[ liveCount++ ] = i;
		}
	}

	return liveCount;
}


/**
*	@brief	Draw the standard four-column scoreboard header labels (checkbox, name, score, ping).
*	@param	ctx	MicroUI context.
*	@note	Assuming layout row is already configured by caller.
**/
static void CLG_Scoreboard_DrawHeaderRow( mu_Context *ctx, const int *rowWidths, const int rowHeight ) {
	mu_label( ctx, "" );
	mu_label( ctx, "Name" );
	mu_label_ex( ctx, "Score", MU_COLOR_TEXT, MU_OPT_ALIGNRIGHT );
	mu_label_ex( ctx, "Ping", MU_COLOR_TEXT, MU_OPT_ALIGNRIGHT );
}


/**
*	@brief	Draw one client row containing checkbox, Name, Score and Ping labels.
*	@param	ctx	MicroUI context.
*	@param	rowWidths	Column width array for checkbox, Name, Score and Ping (4 columns).
*	@param	rowHeight	Fixed row height for this scoreboard.
*	@param	entry	Live scoreboard entry to render.
*	@param	visualIndex	Visible row index used for alternating background colors.
*	@param	selected	[in/out] Pointer to checkbox state for this client.
*	@return	MicroUI change flag from checkbox interaction.
*	@note	Assuming layout row is already configured by caller.
**/
static int CLG_Scoreboard_DrawClientRow( mu_Context *ctx, const int *rowWidths, const int rowHeight, const scoreboard_entry_t &entry, const int32_t visualIndex, int *selected ) {
	char scoreText[ 16 ] = {};
	char pingText[ 16 ] = {};
	std::snprintf( scoreText, sizeof( scoreText ), "%d", entry.clientScore );
	std::snprintf( pingText, sizeof( pingText ), "%d", entry.clientPing );

	const int checkboxResult = mu_checkbox( ctx, "", selected );
	mu_label( ctx, entry.clientName );
	mu_label_ex( ctx, scoreText, MU_COLOR_TEXT, MU_OPT_ALIGNRIGHT );
	mu_label_ex( ctx, pingText, MU_COLOR_TEXT, MU_OPT_ALIGNRIGHT );
	return checkboxResult;
}


#if CLG_SCOREBOARD_ENABLE_TEAMS
/**
*	@brief	Resolve the team index for a scoreboard entry.
*	@param	entry	Live scoreboard entry.
*	@return	Team index in [0, CLG_SCOREBOARD_TEAM_COUNT) when known, otherwise -1.
*	@note	Maps server-emitted team ids (1..N) to local team array indices (0..N-1).
**/
static int32_t CLG_Scoreboard_GetClientTeamIndex( const scoreboard_entry_t &entry ) {
	if ( entry.clientTeamId <= 0 ) {
		return -1;
	}

	const int32_t teamIndex = entry.clientTeamId - 1;
	if ( teamIndex < 0 || teamIndex >= CLG_SCOREBOARD_TEAM_COUNT ) {
		return -1;
	}

	return teamIndex;
}


/**
*	@brief	Resolve a display label for a team section header.
*	@param	teamIndex	Team index to convert to a readable label.
*	@return	Static label text.
**/
static const char *CLG_Scoreboard_GetTeamLabel( const int32_t teamIndex ) {
	switch ( teamIndex ) {
		case 0:
			return "Team 1";
		case 1:
			return "Team 2";
		default:
			return "Team";
	}
}


/**
*	@brief	Draw a team section title row label.
*	@param	ctx	MicroUI context.
*	@param	rowWidths	Column width array (4 columns).
*	@param	rowHeight	Fixed row height for this scoreboard.
*	@param	teamLabel	Human-readable team label.
*	@param	selected	[in/out] Pointer to team-wide checkbox state.
*	@return	MicroUI change flag from checkbox interaction.
*	@note	Assuming layout row is already configured by caller.
**/
static int CLG_Scoreboard_DrawTeamHeaderRow( mu_Context *ctx, const int *rowWidths, const int rowHeight, const char *teamLabel, int *selected ) {
	const int checkboxResult = mu_checkbox( ctx, "", selected );
	mu_label( ctx, teamLabel );
	mu_label( ctx, "" );
	mu_label( ctx, "" );
	return checkboxResult;
}
#endif


/**
*	@brief	Draw the live scoreboard window for active clients and expose per-player actions.
*	@param	ctx	MicroUI context used to build and render the scoreboard controls.
*	@return	True when the scoreboard window was evaluated and, if visible, rendered.
*	@note	Preserves checkbox selection and mute state across frames so the user can
*			prepare multi-player actions before sending commands to the server.
**/
const bool CLG_MUI_ProcessScoreBoard( mu_Context *ctx ) {
	/**
	*	Persistent selection and mute state across frames.
	*	These arrays keep UI state stable even when the scoreboard is rebuilt every frame.
	**/
	static int playerSelected[ MAX_CLIENTS ] = {};
	static int playerMuted[ MAX_CLIENTS ] = {};
	static int focusedMuteClient = -1;

	const int checkboxWidth = 24;
	const int nameWidth = 216;
	const int scoreWidth = 52;
	const int pingWidth = 52;
	const int rowWidths[ 4 ] = { checkboxWidth, nameWidth, scoreWidth, pingWidth };
	const int rowHeight = 24;
	const int rowWidthTotal = checkboxWidth + nameWidth + scoreWidth + pingWidth;
	const int rowWidthVisual = rowWidthTotal + ( ctx->style->spacing * 3 );

	/**
	*	Fetch the current scoreboard snapshot from the client import layer.
	*	Abort early if the game has no valid scoreboard data to display yet.
	**/
	const scoreboard_entry_t *entries = clgi.GetScoreboardEntries();
	if ( !entries ) {
		return false;
	}

	/**
	*	Build a compact list of live client indices so the UI only iterates valid rows.
	**/
	std::array<int32_t, MAX_CLIENTS> liveClientIndices = {};
	const int32_t liveClientCount = CLG_Scoreboard_BuildLiveClientList( entries, liveClientIndices );

	/**
	*	Calculate window height accounting for the header, client rows, spacer, and action buttons.
	**/
	int32_t contentRows = 2;
	if ( liveClientCount <= 0 ) {
		/**
		*	Reserve one row for the empty-state message when no clients are active.
		**/
		contentRows += 1;
	} else {
		/**
		*	Reserve one row per visible client entry.
		**/
		contentRows += liveClientCount;
	}
	contentRows += 2;

	/**
	*	Convert the row count into a pixel height and clamp it to the screen viewport.
	**/
	const int32_t contentHeight = ( contentRows * rowHeight ) + ( ( contentRows - 1 ) * ctx->style->spacing );
	const int32_t scoreBoardWidth = rowWidthVisual + ( ctx->style->padding * 2 );
	const int32_t maxScoreBoardHeight = ( clgi.screen->screenHeight * 3 ) / 4;
	const int32_t scoreBoardHeight = std::min( maxScoreBoardHeight, contentHeight + ( ctx->style->padding * 2 ) );
	const int32_t scoreBoardX = ( clgi.screen->screenWidth - scoreBoardWidth ) / 2;
	const int32_t scoreBoardY = ( clgi.screen->screenHeight - scoreBoardHeight ) / 2;

	const int32_t opt = MU_OPT_ALIGNCENTER | MU_OPT_NOCLOSE | MU_OPT_HOLDFOCUS | MU_OPT_KEEPFOCUS | MU_OPT_NODRAG | MU_OPT_NORESIZE | MU_OPT_NOSCROLL;
	if ( mu_begin_window_ex( ctx, "Scoreboard", mu_rect( scoreBoardX, scoreBoardY, scoreBoardWidth, scoreBoardHeight ), opt ) ) {
		/**
		*	Render common scoreboard styling.
		*	These colors define the header, alternating row backgrounds, and separators.
		**/
		const mu_Color headerColor = mu_color( 255, 120, 0, 183 );
		const mu_Color rowColorDark = mu_color( 0x44, 0x44, 0x44, 196 );
		const mu_Color rowColorLight = mu_color( 0x55, 0x55, 0x55, 196 );
		const mu_Color separatorColor = mu_color( 255, 144, 90, 235 );

		/**
		*	Draw the header row background and its vertical separators.
		*	The visible labels themselves are emitted by the shared header helper.
		**/
		mu_layout_row( ctx, 4, rowWidths, rowHeight );
		mu_Rect headerRect = mu_layout_next( ctx );
		mu_layout_set_next( ctx, headerRect, 0 );
		headerRect.w = rowWidthVisual;
		mu_draw_rect( ctx, headerRect, headerColor );
		mu_draw_rect( ctx, mu_rect( headerRect.x + checkboxWidth, headerRect.y, 1, headerRect.h ), separatorColor );
		mu_draw_rect( ctx, mu_rect( headerRect.x + checkboxWidth + nameWidth, headerRect.y, 1, headerRect.h ), separatorColor );
		mu_draw_rect( ctx, mu_rect( headerRect.x + checkboxWidth + nameWidth + scoreWidth, headerRect.y, 1, headerRect.h ), separatorColor );

		CLG_Scoreboard_DrawHeaderRow( ctx, rowWidths, rowHeight );

		/**
		*	Draw all live client rows, or a fallback message if the scoreboard is empty.
		**/
		if ( liveClientCount <= 0 ) {
			/**
			*	Render the empty-state row so the window still has a balanced layout.
			**/
			mu_layout_row( ctx, 4, rowWidths, rowHeight );
			mu_Rect rowRect = mu_layout_next( ctx );
			mu_layout_set_next( ctx, rowRect, 0 );
			rowRect.w = rowWidthVisual;
			mu_draw_rect( ctx, rowRect, rowColorDark );
			mu_label( ctx, "" );
			mu_label( ctx, "No active clients." );
			mu_label( ctx, "" );
			mu_label( ctx, "" );
		} else {
			/**
			*	Render every active client row with a checkbox and alternating background color.
			**/
			for ( int32_t i = 0; i < liveClientCount; i++ ) {
				const int32_t clientIndex = liveClientIndices[ i ];
				const scoreboard_entry_t &entry = entries[ clientIndex ];

				/**
				*	Prepare the row layout and paint the row separators before labels are drawn.
				**/
				mu_layout_row( ctx, 4, rowWidths, rowHeight );
				mu_Rect rowRect = mu_layout_next( ctx );
				mu_layout_set_next( ctx, rowRect, 0 );
				rowRect.w = rowWidthVisual;
				mu_draw_rect( ctx, rowRect, ( i & 1 ) ? rowColorLight : rowColorDark );
				mu_draw_rect( ctx, mu_rect( rowRect.x + checkboxWidth, rowRect.y, 1, rowRect.h ), separatorColor );
				mu_draw_rect( ctx, mu_rect( rowRect.x + checkboxWidth + nameWidth, rowRect.y, 1, rowRect.h ), separatorColor );
				mu_draw_rect( ctx, mu_rect( rowRect.x + checkboxWidth + nameWidth + scoreWidth, rowRect.y, 1, rowRect.h ), separatorColor );

				/**
				*	Draw the interactive client row and remember whether the checkbox changed.
				**/
				const int checkboxChanged = CLG_Scoreboard_DrawClientRow( ctx, rowWidths, rowHeight, entry, i, &playerSelected[ clientIndex ] );

				/**
				*	Track the last client whose checkbox was toggled so mute actions can target it first.
				**/
				if ( ( checkboxChanged & MU_RES_CHANGE ) != 0 ) {
					if ( playerSelected[ clientIndex ] ) {
						focusedMuteClient = clientIndex;
					} else if ( focusedMuteClient == clientIndex ) {
						focusedMuteClient = -1;
					}
				}
			}
		}

		/**
		*	Add a small spacer row so the action buttons render below the player list.
		**/
		mu_layout_row( ctx, 4, rowWidths, ctx->style->padding );
		mu_label( ctx, "" );
		mu_label( ctx, "" );
		mu_label( ctx, "" );
		mu_label( ctx, "" );

		/**
		*	Collect all selected clients so the action buttons can operate on a stable snapshot.
		**/
		int selClients[ MAX_CLIENTS ];
		int selCount = 0;
		bool anyMutedSelected = false;

		/**
		*	Walk the visible client list and gather only the currently selected entries.
		*	This also determines whether the current selection already contains muted players.
		**/
		for ( int32_t i = 0; i < liveClientCount; i++ ) {
			const int32_t clientIndex = liveClientIndices[ i ];
			if ( playerSelected[ clientIndex ] ) {
				selClients[ selCount++ ] = clientIndex;
				if ( playerMuted[ clientIndex ] ) {
					anyMutedSelected = true;
				}
			}
		}

		/**
		*	Choose the mute label based on the current selection and the focused checkbox state.
		*	If a focused client exists, its mute state takes priority for a more predictable toggle.
		**/
		const char *muteLabel = "Mute";
		if ( selCount > 0 ) {
			const bool focusedClientValid =
				focusedMuteClient >= 0 && focusedMuteClient < MAX_CLIENTS &&
				playerSelected[ focusedMuteClient ] != 0;
			if ( focusedClientValid ) {
				muteLabel = playerMuted[ focusedMuteClient ] ? "Unmute" : "Mute";
			} else {
				muteLabel = anyMutedSelected ? "Unmute" : "Mute";
			}
		}

		/**
		*	Build the selected-player CSV used by vote commands.
		**/
		const std::string selectedCsv = CLG_Scoreboard_BuildSelectedPlayerCsv( selClients, selCount, entries );

		/**
		*	Measure button widths so the action row can be aligned precisely to the columns.
		**/
		const char *voteKickLabel = "Vote Kick";
		const char *voteBanLabel = "Vote Ban";
		const int muteButtonWidth = ctx->text_width( ctx->style->font, muteLabel, -1 ) + ( ctx->style->padding * 2 );
		const int voteKickButtonWidth = ctx->text_width( ctx->style->font, voteKickLabel, -1 ) + ( ctx->style->padding * 2 );
		const int voteBanButtonWidth = ctx->text_width( ctx->style->font, voteBanLabel, -1 ) + ( ctx->style->padding * 2 );

		/**
		*	Draw the action row: mute under the name column, vote buttons right-aligned.
		**/
		mu_layout_row( ctx, 4, rowWidths, rowHeight );

		/**
		*	Skip the checkbox column and place the mute button under the name column.
		**/
		mu_layout_next( ctx );
		mu_Rect nameCol = mu_layout_next( ctx );
		mu_Rect muteRect = nameCol;
		muteRect.w = muteButtonWidth;
		mu_layout_set_next( ctx, muteRect, 0 );
		if ( mu_button( ctx, muteLabel ) ) {
			if ( selCount > 0 ) {
				/**
				*	Toggle mute state for every selected player in one action.
				*	If any selected player is already muted, unmute the group; otherwise mute it.
				**/
				const int setMuted = anyMutedSelected ? 0 : 1;
				for ( int i = 0; i < selCount; i++ ) {
					playerMuted[ selClients[ i ] ] = setMuted;
				}
			}
		}

		/**
		*	Reserve the score and ping columns for the vote buttons and align them to the right edge.
		**/
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
				/**
				*	Forward a votekick command containing the quoted selected-player CSV.
				**/
				const std::string cmd = std::string( "votekick " ) + CLG_Scoreboard_QuoteCommandArgument( selectedCsv );
				clgi.CL_ClientCommand( cmd.c_str() );
			}
		}

		mu_Rect banRect = mu_rect( startX + voteKickButtonWidth + spacing, y, voteBanButtonWidth, h );
		mu_layout_set_next( ctx, banRect, 0 );
		if ( mu_button( ctx, voteBanLabel ) ) {
			if ( selCount > 0 ) {
				/**
				*	Forward a voteban command containing the quoted selected-player CSV.
				**/
				const std::string cmd = std::string( "voteban " ) + CLG_Scoreboard_QuoteCommandArgument( selectedCsv );
				clgi.CL_ClientCommand( cmd.c_str() );
			}
		}

		mu_end_window( ctx );
	}

	return true;
}
