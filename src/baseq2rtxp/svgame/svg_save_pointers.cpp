// Game related types.
#include "svgame/svg_local.h"

// Save related types.

#include "svgame/svg_save.h"
#include "svgame/svg_signalio.h"

#include "svgame/entities/info/svg_info_notnull.h"
#include "svgame/entities/info/svg_info_null.h"

#include "svgame/entities/info/svg_info_player_start.h"

#include "svgame/entities/monster/svg_monster_testdummy.h"

#include "svgame/entities/svg_player_edict.h"
#include "svgame/entities/svg_worldspawn_edict.h"


#if 0
extern mmove_t actor_move_attack;
extern mmove_t actor_move_death1;
extern mmove_t actor_move_death2;
extern mmove_t actor_move_flipoff;
extern mmove_t actor_move_pain1;
extern mmove_t actor_move_pain2;
extern mmove_t actor_move_pain3;
extern mmove_t actor_move_run;
extern mmove_t actor_move_stand;
extern mmove_t actor_move_taunt;
extern mmove_t actor_move_walk;

extern mmove_t infantry_move_attack1;
extern mmove_t infantry_move_attack2;
extern mmove_t infantry_move_death1;
extern mmove_t infantry_move_death2;
extern mmove_t infantry_move_death3;
extern mmove_t infantry_move_duck;
extern mmove_t infantry_move_fidget;
extern mmove_t infantry_move_pain1;
extern mmove_t infantry_move_pain2;
extern mmove_t infantry_move_run;
extern mmove_t infantry_move_stand;
extern mmove_t infantry_move_walk;

extern mmove_t soldier_move_attack1;
extern mmove_t soldier_move_attack2;
extern mmove_t soldier_move_attack3;
extern mmove_t soldier_move_attack4;
extern mmove_t soldier_move_attack6;
extern mmove_t soldier_move_death1;
extern mmove_t soldier_move_death2;
extern mmove_t soldier_move_death3;
extern mmove_t soldier_move_death4;
extern mmove_t soldier_move_death5;
extern mmove_t soldier_move_death6;
extern mmove_t soldier_move_duck;
extern mmove_t soldier_move_pain1;
extern mmove_t soldier_move_pain2;
extern mmove_t soldier_move_pain3;
extern mmove_t soldier_move_pain4;
extern mmove_t soldier_move_run;
extern mmove_t soldier_move_stand1;
extern mmove_t soldier_move_stand3;
extern mmove_t soldier_move_start_run;
extern mmove_t soldier_move_walk1;
extern mmove_t soldier_move_walk2;
#endif

// <Q2RTXP>:
// Lua. (Certain things as Thinks for DelayedUse)
void LUA_Think_UseTargetDelay( svg_base_edict_t *entity );
void LUA_Think_SignalOutDelay( svg_base_edict_t *entity );

void button_onsignalin( svg_base_edict_t *self, svg_base_edict_t *other, svg_base_edict_t *activator, const char *signalName, const svg_signal_argument_array_t &signalArguments );
void door_onsignalin( svg_base_edict_t *self, svg_base_edict_t *other, svg_base_edict_t *activator, const char *signalName, const svg_signal_argument_array_t &signalArguments );
void rotating_onsignalin( svg_base_edict_t *self, svg_base_edict_t *other, svg_base_edict_t *activator, const char *signalName, const svg_signal_argument_array_t &signalArguments );
void func_wall_onsignalin( svg_base_edict_t *self, svg_base_edict_t *other, svg_base_edict_t *activator, const char *signalName, const svg_signal_argument_array_t &signalArguments );
void func_breakable_onsignalin( svg_base_edict_t *self, svg_base_edict_t *other, svg_base_edict_t *activator, const char *signalName, const svg_signal_argument_array_t &signalArguments );
//void monster_testdummy_puppet_use( svg_base_edict_t *self, svg_base_edict_t *other, svg_base_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );

//extern void SP_monster_testdummy_puppet( svg_base_edict_t *self );
//extern void monster_testdummy_puppet_die( svg_base_edict_t *self, svg_base_edict_t *inflictor, svg_base_edict_t *attacker, int damage, vec3_t point );
//void svg_monster_testdummy_t::monster_testdummy_puppet_die( svg_base_edict_t *self, svg_base_edict_t *inflictor, svg_base_edict_t *attacker, int damage, vec3_t point );
//extern void monster_testdummy_puppet_think( svg_base_edict_t *self );
//extern void monster_testdummy_puppet_touch( svg_base_edict_t *self, svg_base_edict_t *other, const cm_plane_t *plane, cm_surface_t *surf );
//extern void monster_testdummy_puppet_post_spawn( svg_base_edict_t *self );
// </Q2RTXP>




extern void SVG_PushMove_Think_CalculateMoveSpeed( svg_base_edict_t *self );

extern void SVG_PushMove_AngleMoveBegin( svg_base_edict_t *ent );
extern void SVG_PushMove_AngleMoveDone( svg_base_edict_t *ent );
extern void SVG_PushMove_AngleMoveFinal( svg_base_edict_t *ent );

extern void barrel_delay( svg_base_edict_t *self, svg_base_edict_t *inflictor, svg_base_edict_t *attacker, int damage, vec3_t point );
extern void barrel_explode( svg_base_edict_t *self );
extern void barrel_touch( svg_base_edict_t *self, svg_base_edict_t *other, const cm_plane_t *plane, cm_surface_t *surf );
extern void body_die( svg_base_edict_t *self, svg_base_edict_t *inflictor, svg_base_edict_t *attacker, int damage, vec3_t point );

extern void button_unpress_move_done( svg_base_edict_t *self );
extern void button_think_return( svg_base_edict_t *self );
extern void button_press_move_done( svg_base_edict_t *self );
extern void button_killed( svg_base_edict_t *self, svg_base_edict_t *inflictor, svg_base_edict_t *attacker, int damage, vec3_t point );
extern void button_touch( svg_base_edict_t *self, svg_base_edict_t *other, const cm_plane_t *plane, cm_surface_t *surf );
extern void button_use( svg_base_edict_t *self, svg_base_edict_t *other, svg_base_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );
extern void button_usetarget_press( svg_base_edict_t *self, svg_base_edict_t *other, svg_base_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );
extern void button_usetarget_toggle( svg_base_edict_t *self, svg_base_edict_t *other, svg_base_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );
extern void button_usetarget_continuous_press( svg_base_edict_t *self, svg_base_edict_t *other, svg_base_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );

extern void debris_die( svg_base_edict_t *self, svg_base_edict_t *inflictor, svg_base_edict_t *attacker, int damage, vec3_t point );

extern void door_blocked( svg_base_edict_t *self, svg_base_edict_t *other );
extern void door_close_move( svg_base_edict_t *self );
extern void door_open_move( svg_base_edict_t *self/*, svg_base_edict_t *activator */);
extern void door_close_move_done( svg_base_edict_t *self );
extern void door_open_move_done( svg_base_edict_t *self );
extern void door_killed( svg_base_edict_t *self, svg_base_edict_t *inflictor, svg_base_edict_t *attacker, int damage, vec3_t point );
extern void door_postspawn( svg_base_edict_t *self );
extern void door_pain( svg_base_edict_t *self, svg_base_edict_t *other, float kick, int damage );
extern void door_touch( svg_base_edict_t *self, svg_base_edict_t *other, const cm_plane_t *plane, cm_surface_t *surf );
extern void door_use( svg_base_edict_t *self, svg_base_edict_t *other, svg_base_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );
#if 0
extern void door_secret_blocked( svg_base_edict_t *self, svg_base_edict_t *other );
extern void door_secret_die( svg_base_edict_t *self, svg_base_edict_t *inflictor, svg_base_edict_t *attacker, int damage, vec3_t point );
extern void door_secret_done( svg_base_edict_t *self );
extern void door_secret_move1( svg_base_edict_t *self );
extern void door_secret_move2( svg_base_edict_t *self );
extern void door_secret_move3( svg_base_edict_t *self );
extern void door_secret_move4( svg_base_edict_t *self );
extern void door_secret_move5( svg_base_edict_t *self );
extern void door_secret_move6( svg_base_edict_t *self );
extern void door_secret_use( svg_base_edict_t *self, svg_base_edict_t *other, svg_base_edict_t *activator );
#endif

extern void DoRespawn( svg_base_edict_t *self );

extern void drop_make_touchable( svg_base_edict_t *self );
extern void drop_temp_touch( svg_base_edict_t *self, svg_base_edict_t *other, const cm_plane_t *plane, cm_surface_t *surf );
extern void droptofloor( svg_base_edict_t *self );

//extern void func_clock_think( svg_base_edict_t *self );
//extern void func_clock_use( svg_base_edict_t *self, svg_base_edict_t *other, svg_base_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );

extern void func_conveyor_use( svg_base_edict_t *self, svg_base_edict_t *other, svg_base_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );

extern void func_breakable_explode( svg_base_edict_t *self, svg_base_edict_t *inflictor, svg_base_edict_t *attacker, int damage, vec3_t point );
extern void func_breakable_pain( svg_base_edict_t *self, svg_base_edict_t *other, float kick, int damage );
extern void func_breakable_spawn_on_trigger( svg_base_edict_t *self, svg_base_edict_t *other, svg_base_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );
extern void func_breakable_use( svg_base_edict_t *self, svg_base_edict_t *other, svg_base_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );

extern void func_object_release( svg_base_edict_t *self );
extern void func_object_touch( svg_base_edict_t *self, svg_base_edict_t *other, const cm_plane_t *plane, cm_surface_t *surf );
extern void func_object_use( svg_base_edict_t *self, svg_base_edict_t *other, svg_base_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );

extern void func_timer_think( svg_base_edict_t *self );
extern void func_timer_use( svg_base_edict_t *self, svg_base_edict_t *other, svg_base_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );

extern void func_train_find( svg_base_edict_t *self );

extern void func_wall_use( svg_base_edict_t *self, svg_base_edict_t *other, svg_base_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );

extern void SVG_FreeEdict( svg_base_edict_t *self );

extern void gib_die( svg_base_edict_t *self, svg_base_edict_t *inflictor, svg_base_edict_t *attacker, int damage, vec3_t point );
extern void gib_think( svg_base_edict_t *self );
extern void gib_touch( svg_base_edict_t *self, svg_base_edict_t *other, const cm_plane_t *plane, cm_surface_t *surf );

extern void hurt_touch( svg_base_edict_t *self, svg_base_edict_t *other, const cm_plane_t *plane, cm_surface_t *surf );
extern void hurt_use( svg_base_edict_t *self, svg_base_edict_t *other, svg_base_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );

extern void spotlight_think( svg_base_edict_t *self );
extern void spotlight_use( svg_base_edict_t *self, svg_base_edict_t *other, svg_base_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );
extern void light_use( svg_base_edict_t *self, svg_base_edict_t *other, svg_base_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );

extern void M_droptofloor( svg_base_edict_t *self );

extern void MegaHealth_think( svg_base_edict_t *self );

extern void SVG_PushMove_MoveBegin( svg_base_edict_t *ent );
extern void SVG_PushMove_MoveDone( svg_base_edict_t *ent );
extern void SVG_PushMove_MoveFinal( svg_base_edict_t *ent );
extern void SVG_PushMove_Think_AccelerateMove( svg_base_edict_t *self );
extern void SVG_PushMove_Think_AccelerateMoveNew( svg_base_edict_t *self );

extern void multi_wait( svg_base_edict_t *ent );

extern void path_corner_touch( svg_base_edict_t *self, svg_base_edict_t *other, const cm_plane_t *plane, cm_surface_t *surf );

extern void plat_blocked( svg_base_edict_t *self, svg_base_edict_t *other );
extern void plat_go_down( svg_base_edict_t *self );
extern void plat_hit_bottom( svg_base_edict_t *self );
extern void plat_hit_top( svg_base_edict_t *self );

//extern void player_die( svg_base_edict_t *self, svg_base_edict_t *inflictor, svg_base_edict_t *attacker, int damage, vec3_t point );
//extern void player_pain( svg_base_edict_t *self, svg_base_edict_t *other, float kick, int damage );

//extern void point_combat_touch( svg_base_edict_t *self, svg_base_edict_t *other, const cm_plane_t *plane, cm_surface_t *surf );

//extern void rotating_start( svg_base_edict_t *self );
//extern void rotating_end( svg_base_edict_t *self );
extern void rotating_accelerate( svg_base_edict_t *self );
extern void rotating_decelerate( svg_base_edict_t *self );
extern void rotating_blocked( svg_base_edict_t *self, svg_base_edict_t *other );
extern void rotating_touch( svg_base_edict_t *self, svg_base_edict_t *other, const cm_plane_t *plane, cm_surface_t *surf );
extern void rotating_use( svg_base_edict_t *self, svg_base_edict_t *other, svg_base_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );

extern void target_crosslevel_target_think( svg_base_edict_t *self );

extern void target_earthquake_think( svg_base_edict_t *self );
extern void target_earthquake_use( svg_base_edict_t *self, svg_base_edict_t *other, svg_base_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );

extern void target_explosion_explode( svg_base_edict_t *self );

extern void target_laser_start( svg_base_edict_t *self );
extern void target_laser_think( svg_base_edict_t *self );
extern void target_laser_use( svg_base_edict_t *self, svg_base_edict_t *other, svg_base_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );

extern void target_lightramp_think( svg_base_edict_t *self );
extern void target_lightramp_use( svg_base_edict_t *self, svg_base_edict_t *other, svg_base_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );

//extern void target_string_use( svg_base_edict_t *self, svg_base_edict_t *other, svg_base_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );

extern void teleporter_touch( svg_base_edict_t *self, svg_base_edict_t *other, const cm_plane_t *plane, cm_surface_t *surf );

//extern void TH_viewthing( svg_base_edict_t *ent );


extern void Think_CalcMoveSpeed( svg_base_edict_t *self );
extern void Think_UseTargetsDelay( svg_base_edict_t *self );
extern void Think_SignalOutDelay( svg_base_edict_t *self );
extern void Think_SpawnDoorTrigger( svg_base_edict_t *ent );

extern void Touch_DoorTrigger( svg_base_edict_t *self, svg_base_edict_t *other, const cm_plane_t *plane, cm_surface_t *surf );
extern void Touch_Item( svg_base_edict_t *ent, svg_base_edict_t *other, const cm_plane_t *plane, cm_surface_t *surf );
extern void Touch_Multi( svg_base_edict_t *ent, svg_base_edict_t *other, const cm_plane_t *plane, cm_surface_t *surf );
extern void Touch_Plat_Center( svg_base_edict_t *ent, svg_base_edict_t *other, const cm_plane_t *plane, cm_surface_t *surf );

extern void train_blocked( svg_base_edict_t *self, svg_base_edict_t *other );
extern void train_next( svg_base_edict_t *self );
extern void train_use( svg_base_edict_t *self, svg_base_edict_t *other, svg_base_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );
extern void train_wait( svg_base_edict_t *self );

extern void trigger_counter_use( svg_base_edict_t *self, svg_base_edict_t *other, svg_base_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );

extern void trigger_crosslevel_trigger_use( svg_base_edict_t *self, svg_base_edict_t *other, svg_base_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );

extern void trigger_elevator_init( svg_base_edict_t *self );
extern void trigger_elevator_use( svg_base_edict_t *self, svg_base_edict_t *other, svg_base_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );

extern void trigger_enable( svg_base_edict_t *self, svg_base_edict_t *other, svg_base_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );

extern void trigger_gravity_touch( svg_base_edict_t *self, svg_base_edict_t *other, const cm_plane_t *plane, cm_surface_t *surf );

extern void trigger_push_touch( svg_base_edict_t *self, svg_base_edict_t *other, const cm_plane_t *plane, cm_surface_t *surf );

extern void trigger_relay_use( svg_base_edict_t *self, svg_base_edict_t *other, svg_base_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );

extern void Use_Areaportal( svg_base_edict_t *ent, svg_base_edict_t *other, svg_base_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );
extern void Use_Item( svg_base_edict_t *ent, svg_base_edict_t *other, svg_base_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );
extern void use_killbox( svg_base_edict_t *self, svg_base_edict_t *other, svg_base_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );
extern void Use_Multi( svg_base_edict_t *self, svg_base_edict_t *other, svg_base_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );
extern void Use_Plat( svg_base_edict_t *self, svg_base_edict_t *other, svg_base_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );
extern void Use_Target_Tent( svg_base_edict_t *self, svg_base_edict_t *other, svg_base_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );

extern void use_target_blaster( svg_base_edict_t *self, svg_base_edict_t *other, svg_base_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );
extern void use_target_changelevel( svg_base_edict_t *self, svg_base_edict_t *other, svg_base_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );
extern void use_target_explosion( svg_base_edict_t *self, svg_base_edict_t *other, svg_base_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );
extern void use_target_goal( svg_base_edict_t *self, svg_base_edict_t *other, svg_base_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );
extern void use_target_secret( svg_base_edict_t *self, svg_base_edict_t *other, svg_base_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );
extern void use_target_spawner( svg_base_edict_t *self, svg_base_edict_t *other, svg_base_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );
extern void Use_Target_Speaker( svg_base_edict_t *self, svg_base_edict_t *other, svg_base_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );
extern void use_target_splash( svg_base_edict_t *self, svg_base_edict_t *other, svg_base_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );

/**
*	@brief	Checks whether the passed (save-) callback function pointer exists within
* 			the registered save pointer table, and has a matching type set.
**/
const svg_save_descriptor_funcptr_error_t SVG_Save_DebugValidateCallbackFuncPtr( svg_base_edict_t *edict, void *p, svg_save_funcptr_type_t type, const std::string &functionName ) {
    // For registering matching type found or not.
    bool matchingType = false;
    // For registering matching ptr found or not.
    bool matchingPtr = false;
    // For registering the found type and ptr type.
    
    // If the pointer is null, we can return success, since it means we 'unset' the callback.
    if ( p == nullptr ) {
		return svg_save_descriptor_funcptr_error_t::FPTR_ERROR_SUCCESS;
    }

    // Iterate the registered save ptr table.
    svg_save_funcptr_instance_t *saveAbleInstance = svg_save_funcptr_instance_t::GetForPointer( p );
    if ( saveAbleInstance && saveAbleInstance->ptr == p ) {
        matchingPtr = true;
    }
    if ( saveAbleInstance && saveAbleInstance->saveAbleType == type ) {
		matchingType = true;
    }
    if ( matchingType && matchingPtr ) {
        return svg_save_descriptor_funcptr_error_t::FPTR_ERROR_SUCCESS;
    }

    // <NOTE>:
    // Breakpoint at errorStr to figure out which function pointer is not registered
    // in the save list.
    // </NOTE>:
	// Failure to find, display appropriate error matching.
    std::string errorStr = functionName + ": entity(#" + std::to_string(edict->s.number) + ", \"" + edict->classname.ptr + "\")";
    svg_save_descriptor_funcptr_error_t errors;
    if ( matchingType == false ) {
        errorStr += " - No matching type found";
        errors |= svg_save_descriptor_funcptr_error_t::FPTR_ERROR_TYPE_MISMATCH;
    }
    if ( matchingPtr == false ) {
        errorStr += " - No matching Ptr found";
        errors |= svg_save_descriptor_funcptr_error_t::FPTR_ERROR_ADDRESS_MISMATCH;
    }

    // Error out.
	gi.error( "%s", errorStr.c_str() );
    return errors;
}
