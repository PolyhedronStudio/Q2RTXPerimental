// Minimal stub implementations to satisfy linker when full monster puppet TU
// is not compiled into the target. These are intentionally simple and
// conservative: they provide basic defaults so other translation units
// can link successfully during incremental builds.

#include "svgame/svg_local.h"
#include "svgame/nav/svg_nav.h"

#include "svgame/svg_local.h"
#include "svgame/svg_entity_events.h"
#include "svgame/svg_misc.h"
#include "svgame/svg_trigger.h"
#include "svgame/svg_utils.h"

// TODO: Move elsewhere.. ?
#include "refresh/shared_types.h"

// Entities
#include "sharedgame/sg_entities.h"

// SharedGame UseTargetHints.
#include "sharedgame/sg_usetarget_hints.h"

// Monster Move
#include "svgame/monsters/svg_mmove.h"
#include "svgame/monsters/svg_mmove_slidemove.h"

// Player trail (Q2/Q2RTX pursuit trail)
#include "svgame/player/svg_player_trail.h"

// TestDummy Monster
#include "svgame/entities/monster/svg_monster_testdummy_debug.h"

// Navigation cluster routing (coarse tile routing pre-pass).
#include "svgame/nav/svg_nav_clusters.h"
// Async navigation queue helpers.
#include "svgame/nav/svg_nav_request.h"
// Traversal helpers required for path invalidation.
#include "svgame/nav/svg_nav_traversal.h"

// Provide nav-agent bbox helper (fallback used when puppet TU not linked).
//void SVG_Monster_GetNavigationAgentBounds( const svg_monster_testdummy_debug_t *ent, Vector3 *out_mins, Vector3 *out_maxs ) {
//    if ( !out_mins || !out_maxs ) {
//        return;
//    }
//    // If we have a live navmesh, prefer its agent bbox.
//    const nav_mesh_t *mesh = g_nav_mesh.get();
//    if ( mesh && ( mesh->agent_maxs.z > mesh->agent_mins.z ) ) {
//        *out_mins = mesh->agent_mins;
//        *out_maxs = mesh->agent_maxs;
//        return;
//    }
//    // Fallback to the entity collision bounds when available.
//    if ( ent ) {
//        *out_mins = ent->mins;
//        *out_maxs = ent->maxs;
//        return;
//    }
//    // Last-resort: reasonable default box.
//    *out_mins = Vector3{ -16.f, -16.f, -36.f };
//    *out_maxs = Vector3{ 16.f, 16.f, 36.f };
//}

bool SVG_Monster_TryQueueNavigationRebuild( svg_monster_testdummy_debug_t *self, const Vector3 &start_origin,
    const Vector3 &goal_origin, const svg_nav_path_policy_t &policy, const Vector3 &agent_mins,
    const Vector3 &agent_maxs, const bool force ) {
    (void)self; (void)start_origin; (void)goal_origin; (void)policy; (void)agent_mins; (void)agent_maxs; (void)force;
    return false;
}

void SVG_Monster_ResetNavigationPath( svg_monster_testdummy_debug_t *self ) {
    if ( !self ) return;
    SVG_Nav_FreeTraversalPath( &self->navigationState.pathProcess.path );
    self->navigationState.pathProcess.path_index = 0;
    self->navigationState.pathProcess.pending_request_handle = 0;
    self->navigationState.pathProcess.rebuild_in_progress = false;
}
#if 0
// Static callback stubs
void svg_monster_testdummy_t::onSpawn( svg_monster_testdummy_t *self ) {
    if ( !self ) return;
    // Minimal spawn behaviour: set entity type and link.
    self->s.entityType = ET_MONSTER;
    self->solid = SOLID_BOUNDS_BOX;
    self->movetype = MOVETYPE_WALK;
    self->nextthink = level.time + 20_hz;
    gi.linkentity( self );
}

void svg_monster_testdummy_t::onPostSpawn( svg_monster_testdummy_t *self ) {
    (void)self;
}

void svg_monster_testdummy_t::onPain( svg_monster_testdummy_t *self, svg_base_edict_t *other, float kick, int32_t damage, entity_damageflags_t damageFlags ) {
    (void)self; (void)other; (void)kick; (void)damage; (void)damageFlags;
}

void svg_monster_testdummy_t::onDie( svg_monster_testdummy_t *self, svg_base_edict_t *inflictor, svg_base_edict_t *attacker, int32_t damage, Vector3 *point ) {
    (void)self; (void)inflictor; (void)attacker; (void)damage; (void)point;
}

void svg_monster_testdummy_t::onTouch( svg_monster_testdummy_t *self, svg_base_edict_t *other, const cm_plane_t *plane, cm_surface_t *surf ) {
    (void)self; (void)other; (void)plane; (void)surf;
}

// Core virtual methods (simple implementations)
void svg_monster_testdummy_t::Reset( const bool retainDictionary ) {
    Super::Reset( retainDictionary );
    testVar = 1337;
}

void svg_monster_testdummy_t::Save( struct game_write_context_t *ctx ) {
    Super::Save( ctx );
}

void svg_monster_testdummy_t::Restore( struct game_read_context_t *ctx ) {
    Super::Restore( ctx );
}

const bool svg_monster_testdummy_t::KeyValue( const cm_entity_t *keyValuePair, std::string &errorStr ) {
    return Super::KeyValue( keyValuePair, errorStr );
}

int32_t svg_monster_testdummy_t::GetSaveDescriptorFieldsCount() {
    return 0;
}

svg_save_descriptor_field_t *svg_monster_testdummy_t::GetSaveDescriptorField( const char * ) {
    return nullptr;
}

svg_save_descriptor_field_t *svg_monster_testdummy_t::GetSaveDescriptorFields() {
    return nullptr;
}

// Provide an out-of-line (non-inline) destructor to ensure the vtable for
// `svg_monster_testdummy_t` is emitted in this translation unit. This
// resolves linker errors for the class' virtual methods on MSVC/Windows.
svg_monster_testdummy_t::~svg_monster_testdummy_t() {
    // Intentionally empty - only exists to force vtable emission.
}
#endif