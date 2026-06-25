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
void Nav_DebugDraw( void );
