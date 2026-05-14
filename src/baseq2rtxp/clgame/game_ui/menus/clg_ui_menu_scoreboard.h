/********************************************************************
*
*
*	ClientGame: ScoreBoard GameUI Implementation:
*
*
********************************************************************/
#pragma once

extern "C" {
	#include "clgame/game_ui/microui-2.02/src/microui.h"
}


/**
*	@brief	Draw the live scoreboard UI for all active client players with checkboxes and action buttons.
*	@param	ctx	MicroUI rendering context.
*	@return	True if the scoreboard is visible, False otherwise.
*	@note	Provides client selection via checkboxes, mute/unmute toggle, and vote kick/ban buttons.
**/
const bool CLG_MUI_ProcessScoreBoard( mu_Context *ctx );
