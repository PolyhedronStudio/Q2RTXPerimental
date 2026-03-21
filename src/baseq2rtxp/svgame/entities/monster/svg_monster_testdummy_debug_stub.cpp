/********************************************************************
*
*	ServerGame: TestDummy Debug Monster Spawn Stub
*
*	Provides the missing onSpawn symbol without pulling the full debug TU into the build.
*
********************************************************************/
#include "svgame/entities/monster/svg_monster_testdummy_debug.h"

/**
*\t@brief	Minimal spawn stub for the debug testdummy symbol.
*\t@note	The full debug implementation remains excluded from the build until its compile issues are resolved.
**/
DEFINE_MEMBER_CALLBACK_SPAWN( svg_monster_testdummy_debug_t, onSpawn )( svg_monster_testdummy_debug_t *self ) -> void {
    // Keep the stub intentionally empty so the linker can resolve the symbol safely.
    (void)self;
}
