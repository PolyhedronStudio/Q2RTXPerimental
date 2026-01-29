/********************************************************************
*
*
*	SVGame: Navigation Path Movement
*
*	Reusable, per-entity path follow state with throttled rebuild + failure backoff.
*
********************************************************************/

// Includes: local and navigation headers.
#include "svgame/svg_local.h"
#include "svgame/nav/svg_nav.h"
#include "svgame/nav/svg_nav_path_process.h"
#include "svgame/nav/svg_nav_path_movement.h"

