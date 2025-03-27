// Game related types.
#include "svgame/svg_local.h"

// Save related types.
#include "svg_save.h"

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
void LUA_Think_UseTargetDelay( svg_edict_t *entity );
void LUA_Think_SignalOutDelay( svg_edict_t *entity );

void button_onsignalin( svg_edict_t *self, svg_edict_t *other, svg_edict_t *activator, const char *signalName, const svg_signal_argument_array_t &signalArguments );
void door_onsignalin( svg_edict_t *self, svg_edict_t *other, svg_edict_t *activator, const char *signalName, const svg_signal_argument_array_t &signalArguments );
void rotating_onsignalin( svg_edict_t *self, svg_edict_t *other, svg_edict_t *activator, const char *signalName, const svg_signal_argument_array_t &signalArguments );
void func_wall_onsignalin( svg_edict_t *self, svg_edict_t *other, svg_edict_t *activator, const char *signalName, const svg_signal_argument_array_t &signalArguments );
void func_breakable_onsignalin( svg_edict_t *self, svg_edict_t *other, svg_edict_t *activator, const char *signalName, const svg_signal_argument_array_t &signalArguments );
void monster_testdummy_puppet_use( svg_edict_t *self, svg_edict_t *other, svg_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );

extern void SP_monster_testdummy_puppet( svg_edict_t *self );
extern void monster_testdummy_puppet_die( svg_edict_t *self, svg_edict_t *inflictor, svg_edict_t *attacker, int damage, vec3_t point );
extern void monster_testdummy_puppet_think( svg_edict_t *self );
extern void monster_testdummy_puppet_touch( svg_edict_t *self, svg_edict_t *other, cplane_t *plane, csurface_t *surf );
extern void monster_testdummy_puppet_post_spawn( svg_edict_t *self );
// </Q2RTXP>




extern void SVG_PushMove_Think_CalculateMoveSpeed( svg_edict_t *self );

extern void SVG_PushMove_AngleMoveBegin( svg_edict_t *ent );
extern void SVG_PushMove_AngleMoveDone( svg_edict_t *ent );
extern void SVG_PushMove_AngleMoveFinal( svg_edict_t *ent );

extern void barrel_delay( svg_edict_t *self, svg_edict_t *inflictor, svg_edict_t *attacker, int damage, vec3_t point );
extern void barrel_explode( svg_edict_t *self );
extern void barrel_touch( svg_edict_t *self, svg_edict_t *other, cplane_t *plane, csurface_t *surf );
extern void body_die( svg_edict_t *self, svg_edict_t *inflictor, svg_edict_t *attacker, int damage, vec3_t point );

extern void button_unpress_move_done( svg_edict_t *self );
extern void button_think_return( svg_edict_t *self );
extern void button_press_move_done( svg_edict_t *self );
extern void button_killed( svg_edict_t *self, svg_edict_t *inflictor, svg_edict_t *attacker, int damage, vec3_t point );
extern void button_touch( svg_edict_t *self, svg_edict_t *other, cplane_t *plane, csurface_t *surf );
extern void button_use( svg_edict_t *self, svg_edict_t *other, svg_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );
extern void button_usetarget_press( svg_edict_t *self, svg_edict_t *other, svg_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );
extern void button_usetarget_toggle( svg_edict_t *self, svg_edict_t *other, svg_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );
extern void button_usetarget_continuous_press( svg_edict_t *self, svg_edict_t *other, svg_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );

extern void debris_die( svg_edict_t *self, svg_edict_t *inflictor, svg_edict_t *attacker, int damage, vec3_t point );

extern void door_blocked( svg_edict_t *self, svg_edict_t *other );
extern void door_close_move( svg_edict_t *self );
extern void door_open_move( svg_edict_t *self/*, svg_edict_t *activator */);
extern void door_close_move_done( svg_edict_t *self );
extern void door_open_move_done( svg_edict_t *self );
extern void door_killed( svg_edict_t *self, svg_edict_t *inflictor, svg_edict_t *attacker, int damage, vec3_t point );
extern void door_postspawn( svg_edict_t *self );
extern void door_pain( svg_edict_t *self, svg_edict_t *other, float kick, int damage );
extern void door_touch( svg_edict_t *self, svg_edict_t *other, cplane_t *plane, csurface_t *surf );
extern void door_use( svg_edict_t *self, svg_edict_t *other, svg_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );
#if 0
extern void door_secret_blocked( svg_edict_t *self, svg_edict_t *other );
extern void door_secret_die( svg_edict_t *self, svg_edict_t *inflictor, svg_edict_t *attacker, int damage, vec3_t point );
extern void door_secret_done( svg_edict_t *self );
extern void door_secret_move1( svg_edict_t *self );
extern void door_secret_move2( svg_edict_t *self );
extern void door_secret_move3( svg_edict_t *self );
extern void door_secret_move4( svg_edict_t *self );
extern void door_secret_move5( svg_edict_t *self );
extern void door_secret_move6( svg_edict_t *self );
extern void door_secret_use( svg_edict_t *self, svg_edict_t *other, svg_edict_t *activator );
#endif

extern void DoRespawn( svg_edict_t *self );

extern void drop_make_touchable( svg_edict_t *self );
extern void drop_temp_touch( svg_edict_t *self, svg_edict_t *other, cplane_t *plane, csurface_t *surf );
extern void droptofloor( svg_edict_t *self );

//extern void func_clock_think( svg_edict_t *self );
//extern void func_clock_use( svg_edict_t *self, svg_edict_t *other, svg_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );

extern void func_conveyor_use( svg_edict_t *self, svg_edict_t *other, svg_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );

extern void func_breakable_explode( svg_edict_t *self, svg_edict_t *inflictor, svg_edict_t *attacker, int damage, vec3_t point );
extern void func_breakable_pain( svg_edict_t *self, svg_edict_t *other, float kick, int damage );
extern void func_breakable_spawn_on_trigger( svg_edict_t *self, svg_edict_t *other, svg_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );
extern void func_breakable_use( svg_edict_t *self, svg_edict_t *other, svg_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );

extern void func_object_release( svg_edict_t *self );
extern void func_object_touch( svg_edict_t *self, svg_edict_t *other, cplane_t *plane, csurface_t *surf );
extern void func_object_use( svg_edict_t *self, svg_edict_t *other, svg_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );

extern void func_timer_think( svg_edict_t *self );
extern void func_timer_use( svg_edict_t *self, svg_edict_t *other, svg_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );

extern void func_train_find( svg_edict_t *self );

extern void func_wall_use( svg_edict_t *self, svg_edict_t *other, svg_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );

extern void SVG_FreeEdict( svg_edict_t *self );

extern void gib_die( svg_edict_t *self, svg_edict_t *inflictor, svg_edict_t *attacker, int damage, vec3_t point );
extern void gib_think( svg_edict_t *self );
extern void gib_touch( svg_edict_t *self, svg_edict_t *other, cplane_t *plane, csurface_t *surf );

extern void hurt_touch( svg_edict_t *self, svg_edict_t *other, cplane_t *plane, csurface_t *surf );
extern void hurt_use( svg_edict_t *self, svg_edict_t *other, svg_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );

extern void spotlight_think( svg_edict_t *self );
extern void spotlight_use( svg_edict_t *self, svg_edict_t *other, svg_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );
extern void light_use( svg_edict_t *self, svg_edict_t *other, svg_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );

extern void M_droptofloor( svg_edict_t *self );

extern void MegaHealth_think( svg_edict_t *self );

extern void SVG_PushMove_MoveBegin( svg_edict_t *ent );
extern void SVG_PushMove_MoveDone( svg_edict_t *ent );
extern void SVG_PushMove_MoveFinal( svg_edict_t *ent );
extern void SVG_PushMove_Think_AccelerateMove( svg_edict_t *self );
extern void SVG_PushMove_Think_AccelerateMoveNew( svg_edict_t *self );

extern void multi_wait( svg_edict_t *ent );

extern void path_corner_touch( svg_edict_t *self, svg_edict_t *other, cplane_t *plane, csurface_t *surf );

extern void plat_blocked( svg_edict_t *self, svg_edict_t *other );
extern void plat_go_down( svg_edict_t *self );
extern void plat_hit_bottom( svg_edict_t *self );
extern void plat_hit_top( svg_edict_t *self );

extern void player_die( svg_edict_t *self, svg_edict_t *inflictor, svg_edict_t *attacker, int damage, vec3_t point );
extern void player_pain( svg_edict_t *self, svg_edict_t *other, float kick, int damage );

//extern void point_combat_touch( svg_edict_t *self, svg_edict_t *other, cplane_t *plane, csurface_t *surf );

//extern void rotating_start( svg_edict_t *self );
//extern void rotating_end( svg_edict_t *self );
extern void rotating_accelerate( svg_edict_t *self );
extern void rotating_decelerate( svg_edict_t *self );
extern void rotating_blocked( svg_edict_t *self, svg_edict_t *other );
extern void rotating_touch( svg_edict_t *self, svg_edict_t *other, cplane_t *plane, csurface_t *surf );
extern void rotating_use( svg_edict_t *self, svg_edict_t *other, svg_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );

extern void target_crosslevel_target_think( svg_edict_t *self );

extern void target_earthquake_think( svg_edict_t *self );
extern void target_earthquake_use( svg_edict_t *self, svg_edict_t *other, svg_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );

extern void target_explosion_explode( svg_edict_t *self );

extern void target_laser_start( svg_edict_t *self );
extern void target_laser_think( svg_edict_t *self );
extern void target_laser_use( svg_edict_t *self, svg_edict_t *other, svg_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );

extern void target_lightramp_think( svg_edict_t *self );
extern void target_lightramp_use( svg_edict_t *self, svg_edict_t *other, svg_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );

//extern void target_string_use( svg_edict_t *self, svg_edict_t *other, svg_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );

extern void teleporter_touch( svg_edict_t *self, svg_edict_t *other, cplane_t *plane, csurface_t *surf );

//extern void TH_viewthing( svg_edict_t *ent );


extern void Think_CalcMoveSpeed( svg_edict_t *self );
extern void Think_UseTargetsDelay( svg_edict_t *self );
extern void Think_SignalOutDelay( svg_edict_t *self );
extern void Think_SpawnDoorTrigger( svg_edict_t *ent );

extern void Touch_DoorTrigger( svg_edict_t *self, svg_edict_t *other, cplane_t *plane, csurface_t *surf );
extern void Touch_Item( svg_edict_t *ent, svg_edict_t *other, cplane_t *plane, csurface_t *surf );
extern void Touch_Multi( svg_edict_t *ent, svg_edict_t *other, cplane_t *plane, csurface_t *surf );
extern void Touch_Plat_Center( svg_edict_t *ent, svg_edict_t *other, cplane_t *plane, csurface_t *surf );

extern void train_blocked( svg_edict_t *self, svg_edict_t *other );
extern void train_next( svg_edict_t *self );
extern void train_use( svg_edict_t *self, svg_edict_t *other, svg_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );
extern void train_wait( svg_edict_t *self );

extern void trigger_counter_use( svg_edict_t *self, svg_edict_t *other, svg_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );

extern void trigger_crosslevel_trigger_use( svg_edict_t *self, svg_edict_t *other, svg_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );

extern void trigger_elevator_init( svg_edict_t *self );
extern void trigger_elevator_use( svg_edict_t *self, svg_edict_t *other, svg_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );

extern void trigger_enable( svg_edict_t *self, svg_edict_t *other, svg_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );

extern void trigger_gravity_touch( svg_edict_t *self, svg_edict_t *other, cplane_t *plane, csurface_t *surf );

extern void trigger_push_touch( svg_edict_t *self, svg_edict_t *other, cplane_t *plane, csurface_t *surf );

extern void trigger_relay_use( svg_edict_t *self, svg_edict_t *other, svg_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );

extern void Use_Areaportal( svg_edict_t *ent, svg_edict_t *other, svg_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );
extern void Use_Item( svg_edict_t *ent, svg_edict_t *other, svg_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );
extern void use_killbox( svg_edict_t *self, svg_edict_t *other, svg_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );
extern void Use_Multi( svg_edict_t *self, svg_edict_t *other, svg_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );
extern void Use_Plat( svg_edict_t *self, svg_edict_t *other, svg_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );
extern void Use_Target_Tent( svg_edict_t *self, svg_edict_t *other, svg_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );

extern void use_target_blaster( svg_edict_t *self, svg_edict_t *other, svg_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );
extern void use_target_changelevel( svg_edict_t *self, svg_edict_t *other, svg_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );
extern void use_target_explosion( svg_edict_t *self, svg_edict_t *other, svg_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );
extern void use_target_goal( svg_edict_t *self, svg_edict_t *other, svg_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );
extern void use_target_secret( svg_edict_t *self, svg_edict_t *other, svg_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );
extern void use_target_spawner( svg_edict_t *self, svg_edict_t *other, svg_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );
extern void Use_Target_Speaker( svg_edict_t *self, svg_edict_t *other, svg_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );
extern void use_target_splash( svg_edict_t *self, svg_edict_t *other, svg_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );

const save_ptr_t save_ptrs[] = {

//
//	Disabled entity... Too Q2-ish specific.
//
#if 0
{ P_pusher_moveinfo_endmovecallback, (void *)door_secret_done },
{ P_pusher_moveinfo_endmovecallback, (void *)door_secret_move1 },
{ P_pusher_moveinfo_endmovecallback, (void *)door_secret_move3 },
{ P_pusher_moveinfo_endmovecallback, (void *)door_secret_move5 },
{ P_die, (void *)door_secret_die },
{ P_think,  (void *)door_secret_move2 },
{ P_think,  (void *)door_secret_move4 },
{ P_think,  (void *)door_secret_move6 },
{ P_blocked, (void *)door_secret_blocked },
{ P_use, (void *)door_secret_use },
#endif

// <Q2RTXP>
// OnSignalIn
{ P_onsignalin, ( void * )button_onsignalin },
{ P_onsignalin, (void *)door_onsignalin },
{ P_onsignalin, (void *)rotating_onsignalin },
{ P_onsignalin, (void *)func_wall_onsignalin },
{ P_onsignalin, (void *)func_breakable_onsignalin },

{ P_use, (void *)func_breakable_spawn_on_trigger },
{ P_die, (void *)func_breakable_explode },
{ P_pain, (void *)func_breakable_pain },
{P_use, (void *)monster_testdummy_puppet_use },
{ P_use, (void *)func_breakable_use },

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
{ P_think,  (void *)drop_make_touchable },
{ P_think,  (void *)droptofloor },
{ P_think,  (void *)M_droptofloor },

{ P_think,  (void *)DoRespawn },
{ P_think,  (void *)gib_think },
{ P_think,  (void *)MegaHealth_think },

{ P_think,  (void *)SVG_FreeEdict },

{ P_think,  (void *)SVG_PushMove_MoveBegin },
{ P_think,  (void *)SVG_PushMove_MoveDone },
{ P_think,  (void *)SVG_PushMove_MoveFinal },
{ P_think,  (void *)SVG_PushMove_AngleMoveBegin },
{ P_think,  (void *)SVG_PushMove_AngleMoveDone },
{ P_think,  (void *)SVG_PushMove_AngleMoveFinal },

{ P_think,  (void *)SVG_PushMove_Think_AccelerateMove },
{ P_think,	(void *)SVG_PushMove_Think_AccelerateMoveNew },
{ P_think,  (void *)SVG_PushMove_Think_CalculateMoveSpeed },

{ P_think,  (void *)barrel_explode },
#if 0
{ P_think,  (void *)button_return_to_unpressed_state },
#else
{ P_think,  (void *)button_think_return },
#endif
{ P_think,  (void *)door_close_move },
{ P_think,  (void *)door_open_move },

//{ P_think,  (void *)func_clock_think },
{ P_think,  (void *)func_object_release },
{ P_think,  (void *)func_timer_think },

//{ P_think,  (void *)rotating_start },
//{ P_think,  (void *)rotating_end},

{ P_think,  (void *)rotating_accelerate },
{ P_think,  (void *)rotating_decelerate},

{ P_think,  (void *)func_train_find },

{ P_think,  (void *)multi_wait },

{ P_think,  (void *)plat_go_down },

{ P_think,  (void *)target_crosslevel_target_think },
{ P_think,  (void *)target_earthquake_think },
{ P_think,  (void *)target_explosion_explode },
{ P_think,  (void *)target_laser_start },
{ P_think,  (void *)target_laser_think },
{ P_think,  (void *)target_lightramp_think },

//{ P_think,  (void *)TH_viewthing },
{ P_think,  (void *)Think_UseTargetsDelay },
{ P_think,  (void *)Think_SignalOutDelay },
{ P_think,  (void *)Think_SpawnDoorTrigger },

{ P_think,  (void *)LUA_Think_UseTargetDelay },
{ P_think,  (void *)LUA_Think_SignalOutDelay },

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
//{ P_touch, (void*)point_combat_touch },
{ P_touch, (void*)rotating_touch },
{ P_touch, (void*)teleporter_touch },
{ P_touch, (void*)Touch_DoorTrigger },
{ P_touch, (void*)Touch_Item },
{ P_touch, (void*)Touch_Multi },
{ P_touch, (void*)Touch_Plat_Center },
{ P_touch, (void*)trigger_gravity_touch },
{ P_touch, (void*)trigger_push_touch },

//
//	Use Callback Pointers:
//
{ P_use, (void*)button_use },
{ P_use, (void *)button_usetarget_press },
{ P_use, (void *)button_usetarget_toggle },
{ P_use, (void *)button_usetarget_continuous_press },

{ P_use, (void*)door_use },

//{ P_use, (void*)func_clock_use },
{ P_use, (void*)func_conveyor_use },
//{ P_use, (void*)func_breakable_spawn },
//{ P_use, (void*)func_breakable_use },
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
//{ P_use, (void*)target_string_use },

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
{ P_use, (void *)Use_Target_Tent },

{ P_use, (void*)use_target_blaster },
{ P_use, (void*)use_target_changelevel },
{ P_use, (void*)use_target_explosion },
{ P_use, (void*)use_target_goal },
{ P_use, (void*)use_target_secret },
{ P_use, (void*)use_target_spawner },
{ P_use, (void*)Use_Target_Speaker },
{ P_use, (void*)use_target_splash },

//
//	Pain Callback Pointers:
//
{ P_pain, (void *)door_pain },
{ P_pain, (void*)player_pain },

//
//	Die Callback Pointers:
//
{ P_die, (void*)barrel_delay },
{ P_die, (void*)body_die },
{ P_die, (void *)button_killed },
{ P_die, (void*)debris_die },
{ P_die, (void*)door_killed },
//{ P_die, (void*)func_breakable_explode },
{ P_die, (void*)gib_die },
{ P_die, (void*)player_die },

//
// MoveInfo_EndFunc	Callback Pointers:
//
#if 0
{ P_pusher_moveinfo_endmovecallback, (void*)button_reached_pressed_state },
{ P_pusher_moveinfo_endmovecallback, (void*)button_trigger_and_wait },
#else
{ P_pusher_moveinfo_endmovecallback, (void *)button_unpress_move_done },
{ P_pusher_moveinfo_endmovecallback, (void *)button_press_move_done },
#endif
{ P_pusher_moveinfo_endmovecallback, (void*)door_close_move_done },
{ P_pusher_moveinfo_endmovecallback, (void*)door_open_move_done },
{ P_pusher_moveinfo_endmovecallback, (void*)plat_hit_bottom },
{ P_pusher_moveinfo_endmovecallback, (void*)plat_hit_top },
{ P_pusher_moveinfo_endmovecallback, (void*)train_wait },

//
// MonsterInfo_CurrentMove.
//


//
// MonsterInfo_CurrentMove.
//


};
const int num_save_ptrs = sizeof( save_ptrs ) / sizeof( save_ptrs[ 0 ] );
