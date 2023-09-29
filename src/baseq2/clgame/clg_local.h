// clg_local.h -- local definitions for Client Game module

#include "shared/shared.h"
#include "shared/list.h"

// define SVGAME_INCLUDE so that game.h does not define the
// short, server-visible gclient_t and edict_t structures,
// because we define the full size ones in this file
#define CLGAME_INCLUDE
#include "shared/clgame.h"