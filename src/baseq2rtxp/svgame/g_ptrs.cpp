// Game related types.
#include "g_local.h"
// Save related types.
#include "g_save.h"

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

extern void actor_attack( edict_t *self );
extern void actor_die( edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point );
extern void actor_pain( edict_t *self, edict_t *other, float kick, int damage );
extern void actor_run( edict_t *self );
extern void actor_stand( edict_t *self );
extern void actor_use( edict_t *self, edict_t *other, edict_t *activator );
extern void actor_walk( edict_t *self );
extern void AngleMove_Begin( edict_t *ent );
extern void AngleMove_Done( edict_t *ent );
extern void AngleMove_Final( edict_t *ent );
extern void barrel_delay( edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point );
extern void barrel_explode( edict_t *self );
extern void barrel_touch( edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf );
extern void body_die( edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point );
extern void button_done( edict_t *self );
extern void button_killed( edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point );
extern void button_return( edict_t *self );
extern void button_touch( edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf );
extern void button_use( edict_t *self, edict_t *other, edict_t *activator );
extern void button_wait( edict_t *self );
extern void commander_body_drop( edict_t *self );
extern void commander_body_think( edict_t *self );
extern void commander_body_use( edict_t *self, edict_t *other, edict_t *activator );
extern void debris_die( edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point );
extern void door_blocked( edict_t *self, edict_t *other );
extern void door_go_down( edict_t *self );
extern void door_hit_bottom( edict_t *self );
extern void door_hit_top( edict_t *self );
extern void door_killed( edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point );
extern void door_postspawn( edict_t *self );
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
extern void door_touch( edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf );
extern void door_use( edict_t *self, edict_t *other, edict_t *activator );
extern void DoRespawn( edict_t *self );
extern void drop_make_touchable( edict_t *self );
extern void drop_temp_touch( edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf );
extern void droptofloor( edict_t *self );
extern void flymonster_start_go( edict_t *self );
extern void func_clock_think( edict_t *self );
extern void func_clock_use( edict_t *self, edict_t *other, edict_t *activator );
extern void func_conveyor_use( edict_t *self, edict_t *other, edict_t *activator );
extern void func_explosive_explode( edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point );
extern void func_explosive_spawn( edict_t *self, edict_t *other, edict_t *activator );
extern void func_explosive_use( edict_t *self, edict_t *other, edict_t *activator );
extern void func_object_release( edict_t *self );
extern void func_object_touch( edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf );
extern void func_object_use( edict_t *self, edict_t *other, edict_t *activator );
extern void func_timer_think( edict_t *self );
extern void func_timer_use( edict_t *self, edict_t *other, edict_t *activator );
extern void func_train_find( edict_t *self );
extern void func_wall_use( edict_t *self, edict_t *other, edict_t *activator );
extern void G_FreeEdict( edict_t *self );
extern void gib_die( edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point );
extern void gib_think( edict_t *self );
extern void gib_touch( edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf );
extern void hurt_touch( edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf );
extern void hurt_use( edict_t *self, edict_t *other, edict_t *activator );
extern void infantry_attack( edict_t *self );
extern void infantry_die( edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point );
extern void infantry_dodge( edict_t *self, edict_t *attacker, float eta );
extern void infantry_fidget( edict_t *self );
extern void infantry_pain( edict_t *self, edict_t *other, float kick, int damage );
extern void infantry_run( edict_t *self );
extern void infantry_sight( edict_t *self, edict_t *other );
extern void infantry_stand( edict_t *self );
extern void infantry_walk( edict_t *self );
extern void spotlight_think( edict_t *self );
extern void spotlight_use( edict_t *self, edict_t *other, edict_t *activator );
extern void light_use( edict_t *self, edict_t *other, edict_t *activator );
extern bool M_CheckAttack( edict_t *self );
extern void M_droptofloor( edict_t *self );
extern void M_FliesOff( edict_t *self );
extern void M_FliesOn( edict_t *self );
extern void MegaHealth_think( edict_t *self );
extern void misc_banner_think( edict_t *self );
extern void misc_blackhole_think( edict_t *self );
extern void misc_blackhole_use( edict_t *self, edict_t *other, edict_t *activator );
extern void misc_deadsoldier_die( edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point );
extern void misc_easterchick2_think( edict_t *self );
extern void misc_easterchick_think( edict_t *self );
extern void misc_eastertank_think( edict_t *self );
extern void misc_satellite_dish_think( edict_t *self );
extern void misc_satellite_dish_use( edict_t *self, edict_t *other, edict_t *activator );
extern void misc_strogg_ship_use( edict_t *self, edict_t *other, edict_t *activator );
extern void misc_viper_bomb_prethink( edict_t *self );
extern void misc_viper_bomb_touch( edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf );
extern void misc_viper_bomb_use( edict_t *self, edict_t *other, edict_t *activator );
extern void misc_viper_use( edict_t *self, edict_t *other, edict_t *activator );
extern void monster_think( edict_t *self );
extern void monster_triggered_spawn( edict_t *self );
extern void monster_triggered_spawn_use( edict_t *self, edict_t *other, edict_t *activator );
extern void monster_use( edict_t *self, edict_t *other, edict_t *activator );
extern void Move_Begin( edict_t *ent );
extern void Move_Done( edict_t *ent );
extern void Move_Final( edict_t *ent );
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
extern void rotating_use( edict_t *self, edict_t *other, edict_t *activator );
extern void soldier_attack( edict_t *self );
extern void soldier_die( edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point );
extern void soldier_dodge( edict_t *self, edict_t *attacker, float eta );
extern void soldier_pain( edict_t *self, edict_t *other, float kick, int damage );
extern void soldier_run( edict_t *self );
extern void soldier_sight( edict_t *self, edict_t *other );
extern void soldier_stand( edict_t *self );
extern void soldier_walk( edict_t *self );
extern void SP_CreateCoopSpots( edict_t *self );
extern void SP_FixCoopSpots( edict_t *self );
extern void swimmonster_start_go( edict_t *self );
extern void target_actor_touch( edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf );
extern void target_crosslevel_target_think( edict_t *self );
extern void target_earthquake_think( edict_t *self );
extern void target_earthquake_use( edict_t *self, edict_t *other, edict_t *activator );
extern void target_explosion_explode( edict_t *self );
extern void target_laser_start( edict_t *self );
extern void target_laser_think( edict_t *self );
extern void target_laser_use( edict_t *self, edict_t *other, edict_t *activator );
extern void target_lightramp_think( edict_t *self );
extern void target_lightramp_use( edict_t *self, edict_t *other, edict_t *activator );
extern void target_string_use( edict_t *self, edict_t *other, edict_t *activator );
extern void teleporter_touch( edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf );
extern void TH_viewthing( edict_t *ent );
extern void Think_AccelMove( edict_t *self );
extern void Think_CalcMoveSpeed( edict_t *self );
extern void Think_Delay( edict_t *self );
extern void Think_SpawnDoorTrigger( edict_t *ent );
extern void Touch_DoorTrigger( edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf );
extern void Touch_Item( edict_t *ent, edict_t *other, cplane_t *plane, csurface_t *surf );
extern void Touch_Multi( edict_t *ent, edict_t *other, cplane_t *plane, csurface_t *surf );
extern void Touch_Plat_Center( edict_t *ent, edict_t *other, cplane_t *plane, csurface_t *surf );
extern void train_blocked( edict_t *self, edict_t *other );
extern void train_next( edict_t *self );
extern void train_use( edict_t *self, edict_t *other, edict_t *activator );
extern void train_wait( edict_t *self );
extern void trigger_counter_use( edict_t *self, edict_t *other, edict_t *activator );
extern void trigger_crosslevel_trigger_use( edict_t *self, edict_t *other, edict_t *activator );
extern void trigger_elevator_init( edict_t *self );
extern void trigger_elevator_use( edict_t *self, edict_t *other, edict_t *activator );
extern void trigger_enable( edict_t *self, edict_t *other, edict_t *activator );
extern void trigger_gravity_touch( edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf );
extern void trigger_key_use( edict_t *self, edict_t *other, edict_t *activator );
extern void trigger_monsterjump_touch( edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf );
extern void trigger_push_touch( edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf );
extern void trigger_relay_use( edict_t *self, edict_t *other, edict_t *activator );
extern void turret_blocked( edict_t *self, edict_t *other );
extern void turret_breach_finish_init( edict_t *self );
extern void turret_breach_think( edict_t *self );
extern void turret_driver_die( edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point );
extern void turret_driver_link( edict_t *self );
extern void turret_driver_think( edict_t *self );
extern void Use_Areaportal( edict_t *ent, edict_t *other, edict_t *activator );
extern void Use_Item( edict_t *ent, edict_t *other, edict_t *activator );
extern void use_killbox( edict_t *self, edict_t *other, edict_t *activator );
extern void Use_Multi( edict_t *self, edict_t *other, edict_t *activator );
extern void Use_Plat( edict_t *self, edict_t *other, edict_t *activator );
extern void use_target_blaster( edict_t *self, edict_t *other, edict_t *activator );
extern void use_target_changelevel( edict_t *self, edict_t *other, edict_t *activator );
extern void use_target_explosion( edict_t *self, edict_t *other, edict_t *activator );
extern void use_target_goal( edict_t *self, edict_t *other, edict_t *activator );
extern void Use_Target_Help( edict_t *self, edict_t *other, edict_t *activator );
extern void use_target_secret( edict_t *self, edict_t *other, edict_t *activator );
extern void use_target_spawner( edict_t *self, edict_t *other, edict_t *activator );
extern void Use_Target_Speaker( edict_t *self, edict_t *other, edict_t *activator );
extern void use_target_splash( edict_t *self, edict_t *other, edict_t *activator );
extern void Use_Target_Tent( edict_t *self, edict_t *other, edict_t *activator );
extern void walkmonster_start_go( edict_t *self );
const save_ptr_t save_ptrs[] = {
{ P_postspawn, (void *)door_postspawn },
{ P_prethink, (void*)misc_viper_bomb_prethink },
{ P_think,  (void*)AngleMove_Begin },
{ P_think,  (void*)AngleMove_Done },
{ P_think,  (void*)AngleMove_Final },
{ P_think,  (void*)barrel_explode },
{ P_think,  (void*)button_return },
{ P_think,  (void*)commander_body_drop },
{ P_think,  (void*)commander_body_think },
{ P_think,  (void*)door_go_down },
{ P_think,  (void*)door_secret_move2 },
{ P_think,  (void*)door_secret_move4 },
{ P_think,  (void*)door_secret_move6 },
{ P_think,  (void*)DoRespawn },
{ P_think,  (void*)drop_make_touchable },
{ P_think,  (void*)droptofloor },
{ P_think,  (void*)flymonster_start_go },
{ P_think,  (void*)func_clock_think },
{ P_think,  (void*)func_object_release },
{ P_think,  (void*)func_timer_think },
{ P_think,  (void*)func_train_find },
{ P_think,  (void*)G_FreeEdict },
{ P_think,  (void*)gib_think },
{ P_think,  (void*)M_droptofloor },
{ P_think,  (void*)M_FliesOff },
{ P_think,  (void*)M_FliesOn },
{ P_think,  (void*)MegaHealth_think },
{ P_think,  (void*)misc_banner_think },
{ P_think,  (void*)misc_blackhole_think },
{ P_think,  (void*)misc_easterchick2_think },
{ P_think,  (void*)misc_easterchick_think },
{ P_think,  (void*)misc_eastertank_think },
{ P_think,  (void*)misc_satellite_dish_think },
{ P_think,  (void*)monster_think },
{ P_think,  (void*)monster_triggered_spawn },
{ P_think,  (void*)Move_Begin },
{ P_think,  (void*)Move_Done },
{ P_think,  (void*)Move_Final },
{ P_think,  (void*)multi_wait },
{ P_think,  (void*)plat_go_down },
{ P_think,  (void*)SP_CreateCoopSpots },
{ P_think,  (void*)SP_FixCoopSpots },
{ P_think,  (void*)swimmonster_start_go },
{ P_think,  (void*)target_crosslevel_target_think },
{ P_think,  (void*)target_earthquake_think },
{ P_think,  (void*)target_explosion_explode },
{ P_think,  (void*)target_laser_start },
{ P_think,  (void*)target_laser_think },
{ P_think,  (void*)target_lightramp_think },
{ P_think,  (void*)TH_viewthing },
{ P_think,  (void*)Think_AccelMove },
{ P_think,  (void*)Think_CalcMoveSpeed },
{ P_think,  (void*)Think_Delay },
{ P_think,  (void*)Think_SpawnDoorTrigger },
{ P_think,  (void*)train_next },
{ P_think,  (void*)trigger_elevator_init },
{ P_think,  (void*)turret_breach_finish_init },
{ P_think,  (void*)turret_breach_think },
{ P_think,  (void*)turret_driver_link },
{ P_think,  (void*)turret_driver_think },
{ P_think,	(void*)spotlight_think },
{ P_think,  (void*)walkmonster_start_go },
{ P_blocked, (void*)door_blocked },
{ P_blocked, (void*)door_secret_blocked },
{ P_blocked, (void*)plat_blocked },
{ P_blocked, (void*)rotating_blocked },
{ P_blocked, (void*)train_blocked },
{ P_blocked, (void*)turret_blocked },
{ P_touch, (void*)barrel_touch },
{ P_touch, (void*)button_touch },
{ P_touch, (void*)door_touch },
{ P_touch, (void*)drop_temp_touch },
{ P_touch, (void*)func_object_touch },
{ P_touch, (void*)gib_touch },
{ P_touch, (void*)hurt_touch },
{ P_touch, (void*)misc_viper_bomb_touch },
{ P_touch, (void*)path_corner_touch },
{ P_touch, (void*)point_combat_touch },
{ P_touch, (void*)rotating_touch },
{ P_touch, (void*)target_actor_touch },
{ P_touch, (void*)teleporter_touch },
{ P_touch, (void*)Touch_DoorTrigger },
{ P_touch, (void*)Touch_Item },
{ P_touch, (void*)Touch_Multi },
{ P_touch, (void*)Touch_Plat_Center },
{ P_touch, (void*)trigger_gravity_touch },
{ P_touch, (void*)trigger_monsterjump_touch },
{ P_touch, (void*)trigger_push_touch },
{ P_use, (void*)actor_use },
{ P_use, (void*)button_use },
{ P_use, (void*)commander_body_use },
{ P_use, (void*)door_secret_use },
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
{ P_use, (void*)misc_blackhole_use },
{ P_use, (void*)misc_satellite_dish_use },
{ P_use, (void*)misc_strogg_ship_use },
{ P_use, (void*)misc_viper_bomb_use },
{ P_use, (void*)misc_viper_use },
{ P_use, (void*)monster_triggered_spawn_use },
{ P_use, (void*)monster_use },
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
{ P_use, (void*)trigger_key_use },
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
{ P_use, (void*)Use_Target_Help },
{ P_use, (void*)use_target_secret },
{ P_use, (void*)use_target_spawner },
{ P_use, (void*)Use_Target_Speaker },
{ P_use, (void*)use_target_splash },
{ P_use, (void*)Use_Target_Tent },
{ P_pain, (void*)actor_pain },
{ P_pain, (void*)infantry_pain },
{ P_pain, (void*)player_pain },
{ P_pain, (void*)soldier_pain },
{ P_die, (void*)actor_die },
{ P_die, (void*)barrel_delay },
{ P_die, (void*)body_die },
{ P_die, (void*)button_killed },
{ P_die, (void*)debris_die },
{ P_die, (void*)door_killed },
{ P_die, (void*)door_secret_die },
{ P_die, (void*)func_explosive_explode },
{ P_die, (void*)gib_die },
{ P_die, (void*)infantry_die },
{ P_die, (void*)misc_deadsoldier_die },
{ P_die, (void*)player_die },
{ P_die, (void*)soldier_die },
{ P_die, (void*)turret_driver_die },

//
// MoveInfo_EndFunc
//
{ P_moveinfo_endfunc, (void*)button_done },
{ P_moveinfo_endfunc, (void*)button_wait },
{ P_moveinfo_endfunc, (void*)door_hit_bottom },
{ P_moveinfo_endfunc, (void*)door_hit_top },
{ P_moveinfo_endfunc, (void*)door_secret_done },
{ P_moveinfo_endfunc, (void*)door_secret_move1 },
{ P_moveinfo_endfunc, (void*)door_secret_move3 },
{ P_moveinfo_endfunc, (void*)door_secret_move5 },
{ P_moveinfo_endfunc, (void*)plat_hit_bottom },
{ P_moveinfo_endfunc, (void*)plat_hit_top },
{ P_moveinfo_endfunc, (void*)train_wait },

//
// MonsterInfo_CurrentMove.
//
{ P_monsterinfo_currentmove, &actor_move_attack },
{ P_monsterinfo_currentmove, &actor_move_death1 },
{ P_monsterinfo_currentmove, &actor_move_death2 },
{ P_monsterinfo_currentmove, &actor_move_flipoff },
{ P_monsterinfo_currentmove, &actor_move_pain1 },
{ P_monsterinfo_currentmove, &actor_move_pain2 },
{ P_monsterinfo_currentmove, &actor_move_pain3 },
{ P_monsterinfo_currentmove, &actor_move_run },
{ P_monsterinfo_currentmove, &actor_move_stand },
{ P_monsterinfo_currentmove, &actor_move_taunt },
{ P_monsterinfo_currentmove, &actor_move_walk },

{ P_monsterinfo_currentmove, &infantry_move_attack1 },
{ P_monsterinfo_currentmove, &infantry_move_attack2 },
{ P_monsterinfo_currentmove, &infantry_move_death1 },
{ P_monsterinfo_currentmove, &infantry_move_death2 },
{ P_monsterinfo_currentmove, &infantry_move_death3 },
{ P_monsterinfo_currentmove, &infantry_move_duck },
{ P_monsterinfo_currentmove, &infantry_move_fidget },
{ P_monsterinfo_currentmove, &infantry_move_pain1 },
{ P_monsterinfo_currentmove, &infantry_move_pain2 },
{ P_monsterinfo_currentmove, &infantry_move_run },
{ P_monsterinfo_currentmove, &infantry_move_stand },
{ P_monsterinfo_currentmove, &infantry_move_walk },

{ P_monsterinfo_currentmove, &soldier_move_attack1 },
{ P_monsterinfo_currentmove, &soldier_move_attack2 },
{ P_monsterinfo_currentmove, &soldier_move_attack3 },
{ P_monsterinfo_currentmove, &soldier_move_attack4 },
{ P_monsterinfo_currentmove, &soldier_move_attack6 },
{ P_monsterinfo_currentmove, &soldier_move_death1 },
{ P_monsterinfo_currentmove, &soldier_move_death2 },
{ P_monsterinfo_currentmove, &soldier_move_death3 },
{ P_monsterinfo_currentmove, &soldier_move_death4 },
{ P_monsterinfo_currentmove, &soldier_move_death5 },
{ P_monsterinfo_currentmove, &soldier_move_death6 },
{ P_monsterinfo_currentmove, &soldier_move_duck },
{ P_monsterinfo_currentmove, &soldier_move_pain1 },
{ P_monsterinfo_currentmove, &soldier_move_pain2 },
{ P_monsterinfo_currentmove, &soldier_move_pain3 },
{ P_monsterinfo_currentmove, &soldier_move_pain4 },
{ P_monsterinfo_currentmove, &soldier_move_run },
{ P_monsterinfo_currentmove, &soldier_move_stand1 },
{ P_monsterinfo_currentmove, &soldier_move_stand3 },
{ P_monsterinfo_currentmove, &soldier_move_start_run },
{ P_monsterinfo_currentmove, &soldier_move_walk1 },
{ P_monsterinfo_currentmove, &soldier_move_walk2 },

//
// MonsterInfo_CurrentMove.
//
{ P_monsterinfo_nextmove, &actor_move_attack },
{ P_monsterinfo_nextmove, &actor_move_death1 },
{ P_monsterinfo_nextmove, &actor_move_death2 },
{ P_monsterinfo_nextmove, &actor_move_flipoff },
{ P_monsterinfo_nextmove, &actor_move_pain1 },
{ P_monsterinfo_nextmove, &actor_move_pain2 },
{ P_monsterinfo_nextmove, &actor_move_pain3 },
{ P_monsterinfo_nextmove, &actor_move_run },
{ P_monsterinfo_nextmove, &actor_move_stand },
{ P_monsterinfo_nextmove, &actor_move_taunt },
{ P_monsterinfo_nextmove, &actor_move_walk },

{ P_monsterinfo_nextmove, &infantry_move_attack1 },
{ P_monsterinfo_nextmove, &infantry_move_attack2 },
{ P_monsterinfo_nextmove, &infantry_move_death1 },
{ P_monsterinfo_nextmove, &infantry_move_death2 },
{ P_monsterinfo_nextmove, &infantry_move_death3 },
{ P_monsterinfo_nextmove, &infantry_move_duck },
{ P_monsterinfo_nextmove, &infantry_move_fidget },
{ P_monsterinfo_nextmove, &infantry_move_pain1 },
{ P_monsterinfo_nextmove, &infantry_move_pain2 },
{ P_monsterinfo_nextmove, &infantry_move_run },
{ P_monsterinfo_nextmove, &infantry_move_stand },
{ P_monsterinfo_nextmove, &infantry_move_walk },

{ P_monsterinfo_nextmove, &soldier_move_attack1 },
{ P_monsterinfo_nextmove, &soldier_move_attack2 },
{ P_monsterinfo_nextmove, &soldier_move_attack3 },
{ P_monsterinfo_nextmove, &soldier_move_attack4 },
{ P_monsterinfo_nextmove, &soldier_move_attack6 },
{ P_monsterinfo_nextmove, &soldier_move_death1 },
{ P_monsterinfo_nextmove, &soldier_move_death2 },
{ P_monsterinfo_nextmove, &soldier_move_death3 },
{ P_monsterinfo_nextmove, &soldier_move_death4 },
{ P_monsterinfo_nextmove, &soldier_move_death5 },
{ P_monsterinfo_nextmove, &soldier_move_death6 },
{ P_monsterinfo_nextmove, &soldier_move_duck },
{ P_monsterinfo_nextmove, &soldier_move_pain1 },
{ P_monsterinfo_nextmove, &soldier_move_pain2 },
{ P_monsterinfo_nextmove, &soldier_move_pain3 },
{ P_monsterinfo_nextmove, &soldier_move_pain4 },
{ P_monsterinfo_nextmove, &soldier_move_run },
{ P_monsterinfo_nextmove, &soldier_move_stand1 },
{ P_monsterinfo_nextmove, &soldier_move_stand3 },
{ P_monsterinfo_nextmove, &soldier_move_start_run },
{ P_monsterinfo_nextmove, &soldier_move_walk1 },
{ P_monsterinfo_nextmove, &soldier_move_walk2 },

//
// MonsterInfo_Stand.
//
{ P_monsterinfo_stand, (void*)actor_stand },
{ P_monsterinfo_stand, (void*)infantry_stand },
{ P_monsterinfo_stand, (void*)soldier_stand },
{ P_monsterinfo_idle, (void*)infantry_fidget },
{ P_monsterinfo_walk, (void*)actor_walk },
{ P_monsterinfo_walk, (void*)infantry_walk },
{ P_monsterinfo_walk, (void*)soldier_walk },
{ P_monsterinfo_run, (void*)actor_run },
{ P_monsterinfo_run, (void*)infantry_run },
{ P_monsterinfo_run, (void*)soldier_run },
{ P_monsterinfo_dodge, (void*)infantry_dodge },
{ P_monsterinfo_dodge, (void*)soldier_dodge },
{ P_monsterinfo_attack, (void*)actor_attack },
{ P_monsterinfo_attack, (void*)infantry_attack },
{ P_monsterinfo_attack, (void*)soldier_attack },
{ P_monsterinfo_sight, (void*)infantry_sight },
{ P_monsterinfo_sight, (void*)soldier_sight },
{ P_monsterinfo_checkattack, (void*)M_CheckAttack },
};
const int num_save_ptrs = sizeof( save_ptrs ) / sizeof( save_ptrs[ 0 ] );
