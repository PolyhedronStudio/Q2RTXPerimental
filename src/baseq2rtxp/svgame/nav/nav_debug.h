#pragma once

#include "nav_core.h"
#include "nav_types.h"

/**
* @brief Register nav debug cvars and cached debug state.
* @note Call during game initialization before any debug drawing is requested.
**/
void Nav_DebugInit( void );

/**
* @brief Draw the nav KD-tree and polygon overlays for the current frame.
* @note This is intended for server-frame debug visualization.
**/
void SVG_Nav_DebugDraw( void );

/**
* @brief Set nav debug test goal A from current player location.
**/
void Nav_DebugSetGoalACommand( void );

/**
* @brief Set nav debug test goal B from current player location.
**/
void Nav_DebugSetGoalBCommand( void );

/**
* @brief Build one test path from goal A to goal B and cache it for rendering.
**/
void Nav_DebugTestPathCommand( void );
