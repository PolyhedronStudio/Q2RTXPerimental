// Game related types.
#include "svg_local.h"

// Save related types.
#include "svg_save.h"

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

// <Q2RTXP>:
// Lua. (Certain things as Thinks for DelayedUse)
void LUA_Think_UseTargetDelay( edict_t *entity );


extern void SP_monster_testdummy_puppet( edict_t *self );
extern void monster_testdummy_puppet_die( edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point );
extern void monster_testdummy_puppet_think( edict_t *self );
extern void monster_testdummy_puppet_touch( edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf );
extern void monster_testdummy_puppet_post_spawn( edict_t *self );
// </Q2RTXP>

extern void SVG_PushMove_AngleMoveBegin( edict_t *ent );
extern void SVG_PushMove_AngleMoveDone( edict_t *ent );
extern void SVG_PushMove_AngleMoveFinal( edict_t *ent );
extern void barrel_delay( edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point );
extern void barrel_explode( edict_t *self );
extern void barrel_touch( edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf );
extern void body_die( edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point );

#if 0
extern void button_reached_pressed_state( edict_t *self );
extern void button_return_to_unpressed_state( edict_t *self );
extern void button_trigger_and_wait( edict_t *self );
#else
extern void button_unpress_move_done( edict_t *self );
extern void button_think_return( edict_t *self );
extern void button_press_move_done( edict_t *self );
#endif
extern void button_killed( edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point );
extern void button_touch( edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf );
extern void button_use( edict_t *self, edict_t *other, edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );

extern void debris_die( edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point );

extern void door_blocked( edict_t *self, edict_t *other );
extern void door_close_move( edict_t *self );
extern void door_open_move( edict_t *self/*, edict_t *activator */);
extern void door_close_move_done( edict_t *self );
extern void door_open_move_done( edict_t *self );
extern void door_killed( edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point );
extern void door_postspawn( edict_t *self );
extern void door_touch( edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf );
extern void door_use( edict_t *self, edict_t *other, edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );
#if 0
extern void door_secret_blocked( edict_t *self, edict_t *other );
extern void door_secret_die( edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point );
extern void door_secret_done( edict_t *self );
extern void door_secret_move1( edict_t *self );
extern void door_secret_move2( edict_t *self );
extern void door_secret_move3( edict_t *self );
extern void door_secret_move4( edict_t *self );
extern void door_secret_move5( edict_t *self );
extern void door_secret_move6( edict_t *self );
extern void door_secret_use( edict_t *self, edict_t *other, edict_t *activator );
#endif

extern void DoRespawn( edict_t *self );

extern void drop_make_touchable( edict_t *self );
extern void drop_temp_touch( edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf );
extern void droptofloor( edict_t *self );

extern void func_clock_think( edict_t *self );
extern void func_clock_use( edict_t *self, edict_t *other, edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );

extern void func_conveyor_use( edict_t *self, edict_t *other, edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );

extern void func_explosive_explode( edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point );
extern void func_explosive_spawn( edict_t *self, edict_t *other, edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );
extern void func_explosive_use( edict_t *self, edict_t *other, edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );

extern void func_object_release( edict_t *self );
extern void func_object_touch( edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf );
extern void func_object_use( edict_t *self, edict_t *other, edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );

extern void func_timer_think( edict_t *self );
extern void func_timer_use( edict_t *self, edict_t *other, edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );

extern void func_train_find( edict_t *self );

extern void func_wall_use( edict_t *self, edict_t *other, edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );

extern void SVG_FreeEdict( edict_t *self );
extern void gib_die( edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point );
extern void gib_think( edict_t *self );
extern void gib_touch( edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf );
extern void hurt_touch( edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf );
extern void hurt_use( edict_t *self, edict_t *other, edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );
extern void spotlight_think( edict_t *self );
extern void spotlight_use( edict_t *self, edict_t *other, edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );
extern void light_use( edict_t *self, edict_t *other, edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );
extern void M_droptofloor( edict_t *self );
extern void MegaHealth_think( edict_t *self );
extern void SVG_PushMove_MoveBegin( edict_t *ent );
extern void SVG_PushMove_MoveDone( edict_t *ent );
extern void SVG_PushMove_MoveFinal( edict_t *ent );
extern void multi_wait( edict_t *ent );
extern void path_corner_touch( edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf );
extern void plat_blocked( edict_t *self, edict_t *other );
extern void plat_go_down( edict_t *self );
extern void plat_hit_bottom( edict_t *self );
extern void plat_hit_top( edict_t *self );
extern void player_die( edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point );
extern void player_pain( edict_t *self, edict_t *other, float kick, int damage );
extern void point_combat_touch( edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf );
extern void rotating_blocked( edict_t *self, edict_t *other );
extern void rotating_touch( edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf );
extern void rotating_use( edict_t *self, edict_t *other, edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );
extern void SP_CreateCoopSpots( edict_t *self  );
extern void SP_FixCoopSpots( edict_t *self );
extern void target_crosslevel_target_think( edict_t *self );
extern void target_earthquake_think( edict_t *self );
extern void target_earthquake_use( edict_t *self, edict_t *other, edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );
extern void target_explosion_explode( edict_t *self );
extern void target_laser_start( edict_t *self );
extern void target_laser_think( edict_t *self );
extern void target_laser_use( edict_t *self, edict_t *other, edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );
extern void target_lightramp_think( edict_t *self );
extern void target_lightramp_use( edict_t *self, edict_t *other, edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );
extern void target_string_use( edict_t *self, edict_t *other, edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );
extern void teleporter_touch( edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf );
extern void TH_viewthing( edict_t *ent );
extern void SVG_PushMove_Think_AccelerateMove( edict_t *self );
extern void Think_CalcMoveSpeed( edict_t *self );
extern void Think_Delay( edict_t *self );
extern void Think_SpawnDoorTrigger( edict_t *ent );
extern void Touch_DoorTrigger( edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf );
extern void Touch_Item( edict_t *ent, edict_t *other, cplane_t *plane, csurface_t *surf );
extern void Touch_Multi( edict_t *ent, edict_t *other, cplane_t *plane, csurface_t *surf );
extern void Touch_Plat_Center( edict_t *ent, edict_t *other, cplane_t *plane, csurface_t *surf );
extern void train_blocked( edict_t *self, edict_t *other );
extern void train_next( edict_t *self );
extern void train_use( edict_t *self, edict_t *other, edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );
extern void train_wait( edict_t *self );
extern void trigger_counter_use( edict_t *self, edict_t *other, edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );
extern void trigger_crosslevel_trigger_use( edict_t *self, edict_t *other, edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );
extern void trigger_elevator_init( edict_t *self );
extern void trigger_elevator_use( edict_t *self, edict_t *other, edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );
extern void trigger_enable( edict_t *self, edict_t *other, edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );
extern void trigger_gravity_touch( edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf );
extern void trigger_monsterjump_touch( edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf );
extern void trigger_push_touch( edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf );
extern void trigger_relay_use( edict_t *self, edict_t *other, edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );
extern void Use_Areaportal( edict_t *ent, edict_t *other, edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );
extern void Use_Item( edict_t *ent, edict_t *other, edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );
extern void use_killbox( edict_t *self, edict_t *other, edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );
extern void Use_Multi( edict_t *self, edict_t *other, edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );
extern void Use_Plat( edict_t *self, edict_t *other, edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );
extern void use_target_blaster( edict_t *self, edict_t *other, edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );
extern void use_target_changelevel( edict_t *self, edict_t *other, edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );
extern void use_target_explosion( edict_t *self, edict_t *other, edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );
extern void use_target_goal( edict_t *self, edict_t *other, edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );
extern void use_target_secret( edict_t *self, edict_t *other, edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );
extern void use_target_spawner( edict_t *self, edict_t *other, edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );
extern void Use_Target_Speaker( edict_t *self, edict_t *other, edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );
extern void use_target_splash( edict_t *self, edict_t *other, edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );
extern void Use_Target_Tent( edict_t *self, edict_t *other, edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );

const save_ptr_t save_ptrs[] = {

//
//	Disabled entity... Too Q2-ish specific.
//
#if 0
{ P_pusher_moveinfo_endfunc, (void *)door_secret_done },
{ P_pusher_moveinfo_endfunc, (void *)door_secret_move1 },
{ P_pusher_moveinfo_endfunc, (void *)door_secret_move3 },
{ P_pusher_moveinfo_endfunc, (void *)door_secret_move5 },
{ P_die, (void *)door_secret_die },
{ P_think,  (void *)door_secret_move2 },
{ P_think,  (void *)door_secret_move4 },
{ P_think,  (void *)door_secret_move6 },
{ P_blocked, (void *)door_secret_blocked },
{ P_use, (void *)door_secret_use },
#endif

// <Q2RTXP>
{ P_die, (void *)monster_testdummy_puppet_die },
{ P_think, (void *)monster_testdummy_puppet_think },
{ P_touch, (void *)monster_testdummy_puppet_touch },
{ P_postspawn, (void *)monster_testdummy_puppet_post_spawn },
// </Q2RTXP>

//
//	Post Spawn Callback Pointers:
//
{ P_postspawn, (void *)door_postspawn },

//
//	Think Callback Pointers:
//
{ P_think,  (void *)SVG_PushMove_AngleMoveBegin },
{ P_think,  (void *)SVG_PushMove_AngleMoveDone },
{ P_think,  (void *)SVG_PushMove_AngleMoveFinal },
{ P_think,  (void *)barrel_explode },
#if 0
{ P_think,  (void *)button_return_to_unpressed_state },
#else
{ P_think,  (void *)button_think_return },
#endif
{ P_think,  (void *)door_close_move },
{ P_think,  (void *)door_open_move },

{ P_think,  (void *)DoRespawn },
{ P_think,  (void *)drop_make_touchable },
{ P_think,  (void *)droptofloor },
{ P_think,  (void *)func_clock_think },
{ P_think,  (void *)func_object_release },
{ P_think,  (void *)func_timer_think },
{ P_think,  (void *)func_train_find },
{ P_think,  (void *)SVG_FreeEdict },
{ P_think,  (void *)gib_think },
{ P_think,  (void *)M_droptofloor },
{ P_think,  (void *)MegaHealth_think },
{ P_think,  (void *)SVG_PushMove_MoveBegin },
{ P_think,  (void *)SVG_PushMove_MoveDone },
{ P_think,  (void *)SVG_PushMove_MoveFinal },
{ P_think,  (void *)multi_wait },
{ P_think,  (void *)plat_go_down },
{ P_think,  (void *)SP_CreateCoopSpots },
{ P_think,  (void *)SP_FixCoopSpots },
{ P_think,  (void *)target_crosslevel_target_think },
{ P_think,  (void *)target_earthquake_think },
{ P_think,  (void *)target_explosion_explode },
{ P_think,  (void *)target_laser_start },
{ P_think,  (void *)target_laser_think },
{ P_think,  (void *)target_lightramp_think },
{ P_think,  (void *)TH_viewthing },
{ P_think,  (void *)SVG_PushMove_Think_AccelerateMove },
{ P_think,  (void *)Think_CalcMoveSpeed },
{ P_think,  (void *)Think_Delay },
{ P_think,  (void *)LUA_Think_UseTargetDelay },
{ P_think,  (void*)Think_SpawnDoorTrigger },
{ P_think,  (void*)train_next },
{ P_think,  (void*)trigger_elevator_init },
{ P_think,	(void*)spotlight_think },

//
//	Blocked Callback Pointers.
//
{ P_blocked, (void*)door_blocked },
{ P_blocked, (void*)plat_blocked },
{ P_blocked, (void*)rotating_blocked },
{ P_blocked, (void*)train_blocked },

//
//	Touch Callback Pointers.
// 	
{ P_touch, (void*)barrel_touch },
{ P_touch, (void *)button_touch },
{ P_touch, (void*)door_touch },
{ P_touch, (void*)drop_temp_touch },
{ P_touch, (void*)func_object_touch },
{ P_touch, (void*)gib_touch },
{ P_touch, (void*)hurt_touch },
{ P_touch, (void*)path_corner_touch },
{ P_touch, (void*)point_combat_touch },
{ P_touch, (void*)rotating_touch },
{ P_touch, (void*)teleporter_touch },
{ P_touch, (void*)Touch_DoorTrigger },
{ P_touch, (void*)Touch_Item },
{ P_touch, (void*)Touch_Multi },
{ P_touch, (void*)Touch_Plat_Center },
{ P_touch, (void*)trigger_gravity_touch },
{ P_touch, (void*)trigger_monsterjump_touch },
{ P_touch, (void*)trigger_push_touch },

//
//	Use Callback Pointers:
//
{ P_use, (void*)button_use },
{ P_use, (void*)door_use },
{ P_use, (void*)func_clock_use },
{ P_use, (void*)func_conveyor_use },
{ P_use, (void*)func_explosive_spawn },
{ P_use, (void*)func_explosive_use },
{ P_use, (void*)func_object_use },
{ P_use, (void*)func_timer_use },
{ P_use, (void*)func_wall_use },
{ P_use, (void*)hurt_use },
{ P_use, (void*)spotlight_use },
{ P_use, (void*)light_use },
{ P_use, (void*)rotating_use },
{ P_use, (void*)target_earthquake_use },
{ P_use, (void*)target_laser_use },
{ P_use, (void*)target_lightramp_use },
{ P_use, (void*)target_string_use },
{ P_use, (void*)train_use },
{ P_use, (void*)trigger_counter_use },
{ P_use, (void*)trigger_crosslevel_trigger_use },
{ P_use, (void*)trigger_elevator_use },
{ P_use, (void*)trigger_enable },
{ P_use, (void*)trigger_relay_use },
{ P_use, (void*)Use_Areaportal },
{ P_use, (void*)Use_Item },
{ P_use, (void*)use_killbox },
{ P_use, (void*)Use_Multi },
{ P_use, (void*)Use_Plat },
{ P_use, (void*)use_target_blaster },
{ P_use, (void*)use_target_changelevel },
{ P_use, (void*)use_target_explosion },
{ P_use, (void*)use_target_goal },
{ P_use, (void*)use_target_secret },
{ P_use, (void*)use_target_spawner },
{ P_use, (void*)Use_Target_Speaker },
{ P_use, (void*)use_target_splash },
{ P_use, (void*)Use_Target_Tent },

//
//	Pain Callback Pointers:
//
{ P_pain, (void*)player_pain },

//
//	Die Callback Pointers:
//
{ P_die, (void*)barrel_delay },
{ P_die, (void*)body_die },
{ P_die, (void *)button_killed },
{ P_die, (void*)debris_die },
{ P_die, (void*)door_killed },
{ P_die, (void*)func_explosive_explode },
{ P_die, (void*)gib_die },
{ P_die, (void*)player_die },

//
// MoveInfo_EndFunc	Callback Pointers:
//
#if 0
{ P_pusher_moveinfo_endfunc, (void*)button_reached_pressed_state },
{ P_pusher_moveinfo_endfunc, (void*)button_trigger_and_wait },
#else
{ P_pusher_moveinfo_endfunc, (void *)button_unpress_move_done },
{ P_pusher_moveinfo_endfunc, (void *)button_press_move_done },
#endif
{ P_pusher_moveinfo_endfunc, (void*)door_close_move_done },
{ P_pusher_moveinfo_endfunc, (void*)door_open_move_done },
{ P_pusher_moveinfo_endfunc, (void*)plat_hit_bottom },
{ P_pusher_moveinfo_endfunc, (void*)plat_hit_top },
{ P_pusher_moveinfo_endfunc, (void*)train_wait },

//
// MonsterInfo_CurrentMove.
//


//
// MonsterInfo_CurrentMove.
//


};
const int num_save_ptrs = sizeof( save_ptrs ) / sizeof( save_ptrs[ 0 ] );
