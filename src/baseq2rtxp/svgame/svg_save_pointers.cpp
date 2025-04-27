// Game related types.
#include "svgame/svg_local.h"

// Save related types.
#include "svg_save.h"

#include "svgame/entities/info/svg_info_notnull.h"
#include "svgame/entities/info/svg_info_null.h"

#include "svgame/entities/info/svg_info_player_start.h"

#include "svgame/entities/monster/svg_monster_testdummy.h"

#include "svgame/entities/svg_ed_player.h"
#include "svgame/entities/svg_ed_worldspawn.h"

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

extern void player_die( svg_base_edict_t *self, svg_base_edict_t *inflictor, svg_base_edict_t *attacker, int damage, vec3_t point );
extern void player_pain( svg_base_edict_t *self, svg_base_edict_t *other, float kick, int damage );

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

const svg_save_descriptor_funcptr_t save_ptrs[] = {

//
//	Disabled entity... Too Q2-ish specific.
//
#if 0
{ FPTR_CALLBACK_PUSHER_MOVEINFO_ENDMOVECALLBACK, (void *)door_secret_done },
{ FPTR_CALLBACK_PUSHER_MOVEINFO_ENDMOVECALLBACK, (void *)door_secret_move1 },
{ FPTR_CALLBACK_PUSHER_MOVEINFO_ENDMOVECALLBACK, (void *)door_secret_move3 },
{ FPTR_CALLBACK_PUSHER_MOVEINFO_ENDMOVECALLBACK, (void *)door_secret_move5 },
{ FPTR_CALLBACK_DIE, (void *)door_secret_die },
{ FPTR_CALLBACK_THINK,  (void *)door_secret_move2 },
{ FPTR_CALLBACK_THINK,  (void *)door_secret_move4 },
{ FPTR_CALLBACK_THINK,  (void *)door_secret_move6 },
{ FPTR_CALLBACK_BLOCKED, (void *)door_secret_blocked },
{ FPTR_CALLBACK_USE, (void *)door_secret_use },
#endif

// <Q2RTXP>
{ FPTR_CALLBACK_SPAWN, (void *)svg_info_null_t::info_null_spawn },
{ FPTR_CALLBACK_SPAWN, (void *)svg_info_notnull_t::info_notnull_spawn },

{ FPTR_CALLBACK_SPAWN, (void *)svg_info_player_coop_t::info_player_coop_spawn },
{ FPTR_CALLBACK_SPAWN, (void *)svg_info_player_deathmatch_t::info_player_deathmatch_spawn },
{ FPTR_CALLBACK_SPAWN, (void *)svg_info_player_intermission_t::info_player_intermission_spawn },
{ FPTR_CALLBACK_SPAWN, (void *)svg_info_player_start_t::info_player_start_spawn },

{ FPTR_CALLBACK_SPAWN, (void *)svg_player_edict_t::player_edict_spawn },
{ FPTR_CALLBACK_SPAWN, (void *)svg_worldspawn_edict_t::ed_worldspawn_spawn },

{ FPTR_CALLBACK_ONSIGNALIN, ( void * )button_onsignalin },

{ FPTR_CALLBACK_ONSIGNALIN, (void *)door_onsignalin },

{ FPTR_CALLBACK_ONSIGNALIN, (void *)rotating_onsignalin },

{ FPTR_CALLBACK_ONSIGNALIN, (void *)func_wall_onsignalin },

{ FPTR_CALLBACK_USE, (void *)func_breakable_spawn_on_trigger },
{ FPTR_CALLBACK_DIE, (void *)func_breakable_explode },
{ FPTR_CALLBACK_PAIN, (void *)func_breakable_pain },
{ FPTR_CALLBACK_ONSIGNALIN, (void *)func_breakable_onsignalin },
{ FPTR_CALLBACK_USE, (void *)func_breakable_use },

{ FPTR_CALLBACK_SPAWN, (void *)svg_monster_testdummy_t::monster_testdummy_puppet_spawn },
{ FPTR_CALLBACK_POSTSPAWN, (void *)svg_monster_testdummy_t::monster_testdummy_puppet_postspawn },
{ FPTR_CALLBACK_THINK, (void *)svg_monster_testdummy_t::monster_testdummy_puppet_think },
{ FPTR_CALLBACK_TOUCH, (void *)svg_monster_testdummy_t::monster_testdummy_puppet_touch },
{ FPTR_CALLBACK_USE, (void *)svg_monster_testdummy_t::monster_testdummy_puppet_use },
{ FPTR_CALLBACK_DIE, (void *)svg_monster_testdummy_t::monster_testdummy_puppet_die },

// </Q2RTXP>

//
//	Post Spawn Callback Pointers:
//
{ FPTR_CALLBACK_POSTSPAWN, (void *)door_postspawn },

//
//	Think Callback Pointers:
//
{ FPTR_CALLBACK_THINK,  (void *)drop_make_touchable },
{ FPTR_CALLBACK_THINK,  (void *)droptofloor },
{ FPTR_CALLBACK_THINK,  (void *)M_droptofloor },

{ FPTR_CALLBACK_THINK,  (void *)DoRespawn },
{ FPTR_CALLBACK_THINK,  (void *)gib_think },
{ FPTR_CALLBACK_THINK,  (void *)MegaHealth_think },

{ FPTR_CALLBACK_THINK,  (void *)SVG_FreeEdict },

{ FPTR_CALLBACK_THINK,  (void *)SVG_PushMove_MoveBegin },
{ FPTR_CALLBACK_THINK,  (void *)SVG_PushMove_MoveDone },
{ FPTR_CALLBACK_THINK,  (void *)SVG_PushMove_MoveFinal },
{ FPTR_CALLBACK_THINK,  (void *)SVG_PushMove_AngleMoveBegin },
{ FPTR_CALLBACK_THINK,  (void *)SVG_PushMove_AngleMoveDone },
{ FPTR_CALLBACK_THINK,  (void *)SVG_PushMove_AngleMoveFinal },

{ FPTR_CALLBACK_THINK,  (void *)SVG_PushMove_Think_AccelerateMove },
{ FPTR_CALLBACK_THINK,	(void *)SVG_PushMove_Think_AccelerateMoveNew },
{ FPTR_CALLBACK_THINK,  (void *)SVG_PushMove_Think_CalculateMoveSpeed },

{ FPTR_CALLBACK_THINK,  (void *)barrel_explode },
#if 0
{ FPTR_CALLBACK_THINK,  (void *)button_return_to_unpressed_state },
#else
{ FPTR_CALLBACK_THINK,  (void *)button_think_return },
#endif
{ FPTR_CALLBACK_THINK,  (void *)door_close_move },
{ FPTR_CALLBACK_THINK,  (void *)door_open_move },

//{ FPTR_CALLBACK_THINK,  (void *)func_clock_think },
{ FPTR_CALLBACK_THINK,  (void *)func_object_release },
{ FPTR_CALLBACK_THINK,  (void *)func_timer_think },

//{ FPTR_CALLBACK_THINK,  (void *)rotating_start },
//{ FPTR_CALLBACK_THINK,  (void *)rotating_end},

{ FPTR_CALLBACK_THINK,  (void *)rotating_accelerate },
{ FPTR_CALLBACK_THINK,  (void *)rotating_decelerate},

{ FPTR_CALLBACK_THINK,  (void *)func_train_find },

{ FPTR_CALLBACK_THINK,  (void *)multi_wait },

{ FPTR_CALLBACK_THINK,  (void *)plat_go_down },

{ FPTR_CALLBACK_THINK,  (void *)target_crosslevel_target_think },
{ FPTR_CALLBACK_THINK,  (void *)target_earthquake_think },
{ FPTR_CALLBACK_THINK,  (void *)target_explosion_explode },
{ FPTR_CALLBACK_THINK,  (void *)target_laser_start },
{ FPTR_CALLBACK_THINK,  (void *)target_laser_think },
{ FPTR_CALLBACK_THINK,  (void *)target_lightramp_think },

//{ FPTR_CALLBACK_THINK,  (void *)TH_viewthing },
{ FPTR_CALLBACK_THINK,  (void *)Think_UseTargetsDelay },
{ FPTR_CALLBACK_THINK,  (void *)Think_SignalOutDelay },
{ FPTR_CALLBACK_THINK,  (void *)Think_SpawnDoorTrigger },

{ FPTR_CALLBACK_THINK,  (void *)LUA_Think_UseTargetDelay },
{ FPTR_CALLBACK_THINK,  (void *)LUA_Think_SignalOutDelay },

{ FPTR_CALLBACK_THINK,  (void*)train_next },

{ FPTR_CALLBACK_THINK,  (void*)trigger_elevator_init },

{ FPTR_CALLBACK_THINK,	(void*)spotlight_think },

//
//	Blocked Callback Pointers.
//
{ FPTR_CALLBACK_BLOCKED, (void*)door_blocked },
{ FPTR_CALLBACK_BLOCKED, (void*)plat_blocked },
{ FPTR_CALLBACK_BLOCKED, (void*)rotating_blocked },
{ FPTR_CALLBACK_BLOCKED, (void*)train_blocked },

//
//	Touch Callback Pointers.
// 	
{ FPTR_CALLBACK_TOUCH, (void*)barrel_touch },
{ FPTR_CALLBACK_TOUCH, (void *)button_touch },
{ FPTR_CALLBACK_TOUCH, (void*)door_touch },
{ FPTR_CALLBACK_TOUCH, (void*)drop_temp_touch },
{ FPTR_CALLBACK_TOUCH, (void*)func_object_touch },
{ FPTR_CALLBACK_TOUCH, (void*)gib_touch },
{ FPTR_CALLBACK_TOUCH, (void*)hurt_touch },
{ FPTR_CALLBACK_TOUCH, (void*)path_corner_touch },
//{ FPTR_CALLBACK_TOUCH, (void*)point_combat_touch },
{ FPTR_CALLBACK_TOUCH, (void*)rotating_touch },
{ FPTR_CALLBACK_TOUCH, (void*)teleporter_touch },
{ FPTR_CALLBACK_TOUCH, (void*)Touch_DoorTrigger },
{ FPTR_CALLBACK_TOUCH, (void*)Touch_Item },
{ FPTR_CALLBACK_TOUCH, (void*)Touch_Multi },
{ FPTR_CALLBACK_TOUCH, (void*)Touch_Plat_Center },
{ FPTR_CALLBACK_TOUCH, (void*)trigger_gravity_touch },
{ FPTR_CALLBACK_TOUCH, (void*)trigger_push_touch },

//
//	Use Callback Pointers:
//
{ FPTR_CALLBACK_USE, (void*)button_use },
{ FPTR_CALLBACK_USE, (void *)button_usetarget_press },
{ FPTR_CALLBACK_USE, (void *)button_usetarget_toggle },
{ FPTR_CALLBACK_USE, (void *)button_usetarget_continuous_press },

{ FPTR_CALLBACK_USE, (void*)door_use },

//{ FPTR_CALLBACK_USE, (void*)func_clock_use },
{ FPTR_CALLBACK_USE, (void*)func_conveyor_use },
//{ FPTR_CALLBACK_USE, (void*)func_breakable_spawn },
//{ FPTR_CALLBACK_USE, (void*)func_breakable_use },
{ FPTR_CALLBACK_USE, (void*)func_object_use },
{ FPTR_CALLBACK_USE, (void*)func_timer_use },
{ FPTR_CALLBACK_USE, (void*)func_wall_use },

{ FPTR_CALLBACK_USE, (void*)hurt_use },

{ FPTR_CALLBACK_USE, (void*)spotlight_use },
{ FPTR_CALLBACK_USE, (void*)light_use },

{ FPTR_CALLBACK_USE, (void*)rotating_use },

{ FPTR_CALLBACK_USE, (void*)target_earthquake_use },
{ FPTR_CALLBACK_USE, (void*)target_laser_use },
{ FPTR_CALLBACK_USE, (void*)target_lightramp_use },
//{ FPTR_CALLBACK_USE, (void*)target_string_use },

{ FPTR_CALLBACK_USE, (void*)train_use },

{ FPTR_CALLBACK_USE, (void*)trigger_counter_use },
{ FPTR_CALLBACK_USE, (void*)trigger_crosslevel_trigger_use },
{ FPTR_CALLBACK_USE, (void*)trigger_elevator_use },
{ FPTR_CALLBACK_USE, (void*)trigger_enable },
{ FPTR_CALLBACK_USE, (void*)trigger_relay_use },

{ FPTR_CALLBACK_USE, (void*)Use_Areaportal },
{ FPTR_CALLBACK_USE, (void*)Use_Item },
{ FPTR_CALLBACK_USE, (void*)use_killbox },
{ FPTR_CALLBACK_USE, (void*)Use_Multi },
{ FPTR_CALLBACK_USE, (void*)Use_Plat },
{ FPTR_CALLBACK_USE, (void *)Use_Target_Tent },

{ FPTR_CALLBACK_USE, (void*)use_target_blaster },
{ FPTR_CALLBACK_USE, (void*)use_target_changelevel },
{ FPTR_CALLBACK_USE, (void*)use_target_explosion },
{ FPTR_CALLBACK_USE, (void*)use_target_goal },
{ FPTR_CALLBACK_USE, (void*)use_target_secret },
{ FPTR_CALLBACK_USE, (void*)use_target_spawner },
{ FPTR_CALLBACK_USE, (void*)Use_Target_Speaker },
{ FPTR_CALLBACK_USE, (void*)use_target_splash },

//
//	Pain Callback Pointers:
//
{ FPTR_CALLBACK_PAIN, (void *)door_pain },
{ FPTR_CALLBACK_PAIN, (void*)player_pain },

//
//	Die Callback Pointers:
//
{ FPTR_CALLBACK_DIE, (void*)barrel_delay },
{ FPTR_CALLBACK_DIE, (void*)body_die },
{ FPTR_CALLBACK_DIE, (void *)button_killed },
{ FPTR_CALLBACK_DIE, (void*)debris_die },
{ FPTR_CALLBACK_DIE, (void*)door_killed },
//{ FPTR_CALLBACK_DIE, (void*)func_breakable_explode },
{ FPTR_CALLBACK_DIE, (void*)gib_die },
{ FPTR_CALLBACK_DIE, (void*)player_die },

//
// MoveInfo_EndFunc	Callback Pointers:
//
#if 0
{ FPTR_CALLBACK_PUSHER_MOVEINFO_ENDMOVECALLBACK, (void*)button_reached_pressed_state },
{ FPTR_CALLBACK_PUSHER_MOVEINFO_ENDMOVECALLBACK, (void*)button_trigger_and_wait },
#else
{ FPTR_CALLBACK_PUSHER_MOVEINFO_ENDMOVECALLBACK, (void *)button_unpress_move_done },
{ FPTR_CALLBACK_PUSHER_MOVEINFO_ENDMOVECALLBACK, (void *)button_press_move_done },
#endif
{ FPTR_CALLBACK_PUSHER_MOVEINFO_ENDMOVECALLBACK, (void*)door_close_move_done },
{ FPTR_CALLBACK_PUSHER_MOVEINFO_ENDMOVECALLBACK, (void*)door_open_move_done },
{ FPTR_CALLBACK_PUSHER_MOVEINFO_ENDMOVECALLBACK, (void*)plat_hit_bottom },
{ FPTR_CALLBACK_PUSHER_MOVEINFO_ENDMOVECALLBACK, (void*)plat_hit_top },
{ FPTR_CALLBACK_PUSHER_MOVEINFO_ENDMOVECALLBACK, (void*)train_wait },

//
// MonsterInfo_CurrentMove.
//


//
// MonsterInfo_CurrentMove.
//


};
//! Total number of save pointers.
const int num_save_ptrs = sizeof( save_ptrs ) / sizeof( save_ptrs[ 0 ] );

/**
*	@brief	Checks whether the passed (save-) callback function pointer exists within
* 			the registered save pointer table, and has a matching type set.
**/
const svg_save_descriptor_funcptr_error_t SVG_Save_DebugValidateCallbackFuncPtr( svg_base_edict_t *edict, void *p, svg_save_descriptor_funcptr_type_t type, const std::string &functionName ) {
    // For registering matching type found or not.
    bool matchingType = false;
    // For registering matching ptr found or not.
    bool matchingPtr = false;
    // For registering the found type and ptr type.
    
    // Iterate the registered save ptr table.
    for ( int32_t i = 0; i < num_save_ptrs; i++ ) {
        // Get registered ptr data.
        const svg_save_descriptor_funcptr_t *registeredPtr = &save_ptrs[ i ];

		// We had a matching type.
        if ( registeredPtr->type == type ) {
            matchingType = true;
        }
        // We had a matching pointer.
        if ( registeredPtr->ptr == p || p == nullptr ) {
            matchingPtr = true;
        }

        // Break out by returning success if we found it.
		if ( matchingType && matchingPtr ) {
            return svg_save_descriptor_funcptr_error_t::FPTR_ERROR_SUCCESS;
		}
    }

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
