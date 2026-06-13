#pragma once
#include "nav_core.h"
#include "nav_types.h"

// Initializes the nav debug cvars. Call from SVG_InitGame.
void Nav_DebugInit();

// Draws the KD-Tree nodes and polygons. Call from SVG_RunFrame.
void Nav_DebugDraw();
