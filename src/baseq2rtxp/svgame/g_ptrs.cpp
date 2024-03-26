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
extern mmove_t berserk_move_attack_club;
extern mmove_t berserk_move_attack_spike;
extern mmove_t berserk_move_death1;
extern mmove_t berserk_move_death2;
extern mmove_t berserk_move_pain1;
extern mmove_t berserk_move_pain2;
extern mmove_t berserk_move_run1;
extern mmove_t berserk_move_stand;
extern mmove_t berserk_move_stand_fidget;
extern mmove_t berserk_move_walk;
extern mmove_t boss2_move_attack_mg;
extern mmove_t boss2_move_attack_post_mg;
extern mmove_t boss2_move_attack_pre_mg;
extern mmove_t boss2_move_attack_rocket;
extern mmove_t boss2_move_death;
extern mmove_t boss2_move_pain_heavy;
extern mmove_t boss2_move_pain_light;
extern mmove_t boss2_move_run;
extern mmove_t boss2_move_stand;
extern mmove_t boss2_move_walk;
extern mmove_t brain_move_attack1;
extern mmove_t brain_move_attack2;
extern mmove_t brain_move_death1;
extern mmove_t brain_move_death2;
extern mmove_t brain_move_duck;
extern mmove_t brain_move_idle;
extern mmove_t brain_move_pain1;
extern mmove_t brain_move_pain2;
extern mmove_t brain_move_pain3;
extern mmove_t brain_move_run;
extern mmove_t brain_move_stand;
extern mmove_t brain_move_walk1;
extern mmove_t chick_move_attack1;
extern mmove_t chick_move_death1;
extern mmove_t chick_move_death2;
extern mmove_t chick_move_duck;
extern mmove_t chick_move_end_attack1;
extern mmove_t chick_move_end_slash;
extern mmove_t chick_move_fidget;
extern mmove_t chick_move_pain1;
extern mmove_t chick_move_pain2;
extern mmove_t chick_move_pain3;
extern mmove_t chick_move_run;
extern mmove_t chick_move_slash;
extern mmove_t chick_move_stand;
extern mmove_t chick_move_start_attack1;
extern mmove_t chick_move_start_run;
extern mmove_t chick_move_start_slash;
extern mmove_t chick_move_walk;
extern mmove_t flipper_move_attack;
extern mmove_t flipper_move_death;
extern mmove_t flipper_move_pain1;
extern mmove_t flipper_move_pain2;
extern mmove_t flipper_move_run_loop;
extern mmove_t flipper_move_run_start;
extern mmove_t flipper_move_stand;
extern mmove_t flipper_move_start_run;
extern mmove_t flipper_move_walk;
extern mmove_t floater_move_attack1;
extern mmove_t floater_move_attack2;
extern mmove_t floater_move_attack3;
extern mmove_t floater_move_pain1;
extern mmove_t floater_move_pain2;
extern mmove_t floater_move_run;
extern mmove_t floater_move_stand1;
extern mmove_t floater_move_stand2;
extern mmove_t floater_move_walk;
extern mmove_t flyer_move_attack2;
extern mmove_t flyer_move_end_melee;
extern mmove_t flyer_move_loop_melee;
extern mmove_t flyer_move_pain1;
extern mmove_t flyer_move_pain2;
extern mmove_t flyer_move_pain3;
extern mmove_t flyer_move_run;
extern mmove_t flyer_move_stand;
extern mmove_t flyer_move_start;
extern mmove_t flyer_move_start_melee;
extern mmove_t flyer_move_stop;
extern mmove_t flyer_move_walk;
extern mmove_t gladiator_move_attack_gun;
extern mmove_t gladiator_move_attack_melee;
extern mmove_t gladiator_move_death;
extern mmove_t gladiator_move_pain;
extern mmove_t gladiator_move_pain_air;
extern mmove_t gladiator_move_run;
extern mmove_t gladiator_move_stand;
extern mmove_t gladiator_move_walk;
extern mmove_t gunner_move_attack_chain;
extern mmove_t gunner_move_attack_grenade;
extern mmove_t gunner_move_death;
extern mmove_t gunner_move_duck;
extern mmove_t gunner_move_endfire_chain;
extern mmove_t gunner_move_fidget;
extern mmove_t gunner_move_fire_chain;
extern mmove_t gunner_move_pain1;
extern mmove_t gunner_move_pain2;
extern mmove_t gunner_move_pain3;
extern mmove_t gunner_move_run;
extern mmove_t gunner_move_runandshoot;
extern mmove_t gunner_move_stand;
extern mmove_t gunner_move_walk;
extern mmove_t hover_move_attack1;
extern mmove_t hover_move_death1;
extern mmove_t hover_move_end_attack;
extern mmove_t hover_move_pain1;
extern mmove_t hover_move_pain2;
extern mmove_t hover_move_pain3;
extern mmove_t hover_move_run;
extern mmove_t hover_move_stand;
extern mmove_t hover_move_start_attack;
extern mmove_t hover_move_walk;
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
extern mmove_t insane_move_crawl;
extern mmove_t insane_move_crawl_death;
extern mmove_t insane_move_crawl_pain;
extern mmove_t insane_move_cross;
extern mmove_t insane_move_down;
extern mmove_t insane_move_downtoup;
extern mmove_t insane_move_jumpdown;
extern mmove_t insane_move_run_insane;
extern mmove_t insane_move_run_normal;
extern mmove_t insane_move_runcrawl;
extern mmove_t insane_move_stand_death;
extern mmove_t insane_move_stand_insane;
extern mmove_t insane_move_stand_normal;
extern mmove_t insane_move_stand_pain;
extern mmove_t insane_move_struggle_cross;
extern mmove_t insane_move_uptodown;
extern mmove_t insane_move_walk_insane;
extern mmove_t insane_move_walk_normal;
extern mmove_t jorg_move_attack1;
extern mmove_t jorg_move_attack2;
extern mmove_t jorg_move_death;
extern mmove_t jorg_move_end_attack1;
extern mmove_t jorg_move_pain1;
extern mmove_t jorg_move_pain2;
extern mmove_t jorg_move_pain3;
extern mmove_t jorg_move_run;
extern mmove_t jorg_move_stand;
extern mmove_t jorg_move_start_attack1;
extern mmove_t jorg_move_walk;
extern mmove_t makron_move_attack3;
extern mmove_t makron_move_attack4;
extern mmove_t makron_move_attack5;
extern mmove_t makron_move_death2;
extern mmove_t makron_move_pain4;
extern mmove_t makron_move_pain5;
extern mmove_t makron_move_pain6;
extern mmove_t makron_move_run;
extern mmove_t makron_move_sight;
extern mmove_t makron_move_stand;
extern mmove_t makron_move_walk;
extern mmove_t medic_move_attackBlaster;
extern mmove_t medic_move_attackCable;
extern mmove_t medic_move_attackHyperBlaster;
extern mmove_t medic_move_death;
extern mmove_t medic_move_duck;
extern mmove_t medic_move_pain1;
extern mmove_t medic_move_pain2;
extern mmove_t medic_move_run;
extern mmove_t medic_move_stand;
extern mmove_t medic_move_walk;
extern mmove_t mutant_move_attack;
extern mmove_t mutant_move_death1;
extern mmove_t mutant_move_death2;
extern mmove_t mutant_move_idle;
extern mmove_t mutant_move_jump;
extern mmove_t mutant_move_pain1;
extern mmove_t mutant_move_pain2;
extern mmove_t mutant_move_pain3;
extern mmove_t mutant_move_run;
extern mmove_t mutant_move_stand;
extern mmove_t mutant_move_start_walk;
extern mmove_t mutant_move_walk;
extern mmove_t parasite_move_death;
extern mmove_t parasite_move_drain;
extern mmove_t parasite_move_end_fidget;
extern mmove_t parasite_move_fidget;
extern mmove_t parasite_move_pain1;
extern mmove_t parasite_move_run;
extern mmove_t parasite_move_stand;
extern mmove_t parasite_move_start_fidget;
extern mmove_t parasite_move_start_run;
extern mmove_t parasite_move_start_walk;
extern mmove_t parasite_move_walk;
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
extern mmove_t supertank_move_attack1;
extern mmove_t supertank_move_attack2;
extern mmove_t supertank_move_death;
extern mmove_t supertank_move_end_attack1;
extern mmove_t supertank_move_forward;
extern mmove_t supertank_move_pain1;
extern mmove_t supertank_move_pain2;
extern mmove_t supertank_move_pain3;
extern mmove_t supertank_move_run;
extern mmove_t supertank_move_stand;
extern mmove_t tank_move_attack_blast;
extern mmove_t tank_move_attack_chain;
extern mmove_t tank_move_attack_fire_rocket;
extern mmove_t tank_move_attack_post_blast;
extern mmove_t tank_move_attack_post_rocket;
extern mmove_t tank_move_attack_pre_rocket;
extern mmove_t tank_move_attack_strike;
extern mmove_t tank_move_death;
extern mmove_t tank_move_pain1;
extern mmove_t tank_move_pain2;
extern mmove_t tank_move_pain3;
extern mmove_t tank_move_reattack_blast;
extern mmove_t tank_move_run;
extern mmove_t tank_move_stand;
extern mmove_t tank_move_start_run;
extern mmove_t tank_move_walk;
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
extern void berserk_die( edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point );
extern void berserk_melee( edict_t *self );
extern void berserk_pain( edict_t *self, edict_t *other, float kick, int damage );
extern void berserk_run( edict_t *self );
extern void berserk_search( edict_t *self );
extern void berserk_sight( edict_t *self, edict_t *other );
extern void berserk_stand( edict_t *self );
extern void berserk_walk( edict_t *self );
extern void bfg_explode( edict_t *self );
extern void bfg_think( edict_t *self );
extern void bfg_touch( edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf );
extern void blaster_touch( edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf );
extern void body_die( edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point );
extern void boss2_attack( edict_t *self );
extern bool Boss2_CheckAttack( edict_t *self );
extern void boss2_die( edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point );
extern void boss2_pain( edict_t *self, edict_t *other, float kick, int damage );
extern void boss2_run( edict_t *self );
extern void boss2_search( edict_t *self );
extern void boss2_stand( edict_t *self );
extern void boss2_walk( edict_t *self );
extern void BossExplode( edict_t *self );
extern void brain_die( edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point );
extern void brain_dodge( edict_t *self, edict_t *attacker, float eta );
extern void brain_idle( edict_t *self );
extern void brain_melee( edict_t *self );
extern void brain_pain( edict_t *self, edict_t *other, float kick, int damage );
extern void brain_run( edict_t *self );
extern void brain_search( edict_t *self );
extern void brain_sight( edict_t *self, edict_t *other );
extern void brain_stand( edict_t *self );
extern void brain_walk( edict_t *self );
extern void button_done( edict_t *self );
extern void button_killed( edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point );
extern void button_return( edict_t *self );
extern void button_touch( edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf );
extern void button_use( edict_t *self, edict_t *other, edict_t *activator );
extern void button_wait( edict_t *self );
extern void chick_attack( edict_t *self );
extern void chick_die( edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point );
extern void chick_dodge( edict_t *self, edict_t *attacker, float eta );
extern void chick_melee( edict_t *self );
extern void chick_pain( edict_t *self, edict_t *other, float kick, int damage );
extern void chick_run( edict_t *self );
extern void chick_sight( edict_t *self, edict_t *other );
extern void chick_stand( edict_t *self );
extern void chick_walk( edict_t *self );
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
extern void flare_think( edict_t *self );
extern void flare_touch( edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf );
extern void flipper_die( edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point );
extern void flipper_melee( edict_t *self );
extern void flipper_pain( edict_t *self, edict_t *other, float kick, int damage );
extern void flipper_sight( edict_t *self, edict_t *other );
extern void flipper_stand( edict_t *self );
extern void flipper_start_run( edict_t *self );
extern void flipper_walk( edict_t *self );
extern void floater_attack( edict_t *self );
extern void floater_die( edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point );
extern void floater_idle( edict_t *self );
extern void floater_melee( edict_t *self );
extern void floater_pain( edict_t *self, edict_t *other, float kick, int damage );
extern void floater_run( edict_t *self );
extern void floater_sight( edict_t *self, edict_t *other );
extern void floater_stand( edict_t *self );
extern void floater_walk( edict_t *self );
extern void flyer_attack( edict_t *self );
extern void flyer_die( edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point );
extern void flyer_idle( edict_t *self );
extern void flyer_melee( edict_t *self );
extern void flyer_pain( edict_t *self, edict_t *other, float kick, int damage );
extern void flyer_run( edict_t *self );
extern void flyer_sight( edict_t *self, edict_t *other );
extern void flyer_stand( edict_t *self );
extern void flyer_walk( edict_t *self );
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
extern void gladiator_attack( edict_t *self );
extern void gladiator_die( edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point );
extern void gladiator_idle( edict_t *self );
extern void gladiator_melee( edict_t *self );
extern void gladiator_pain( edict_t *self, edict_t *other, float kick, int damage );
extern void gladiator_run( edict_t *self );
extern void gladiator_search( edict_t *self );
extern void gladiator_sight( edict_t *self, edict_t *other );
extern void gladiator_stand( edict_t *self );
extern void gladiator_walk( edict_t *self );
extern void Grenade_Explode( edict_t *ent );
extern void Grenade_Touch( edict_t *ent, edict_t *other, cplane_t *plane, csurface_t *surf );
extern void gunner_attack( edict_t *self );
extern void gunner_die( edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point );
extern void gunner_dodge( edict_t *self, edict_t *attacker, float eta );
extern void gunner_pain( edict_t *self, edict_t *other, float kick, int damage );
extern void gunner_run( edict_t *self );
extern void gunner_search( edict_t *self );
extern void gunner_sight( edict_t *self, edict_t *other );
extern void gunner_stand( edict_t *self );
extern void gunner_walk( edict_t *self );
extern void hover_deadthink( edict_t *self );
extern void hover_die( edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point );
extern void hover_pain( edict_t *self, edict_t *other, float kick, int damage );
extern void hover_run( edict_t *self );
extern void hover_search( edict_t *self );
extern void hover_sight( edict_t *self, edict_t *other );
extern void hover_stand( edict_t *self );
extern void hover_start_attack( edict_t *self );
extern void hover_walk( edict_t *self );
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
extern void insane_die( edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point );
extern void insane_pain( edict_t *self, edict_t *other, float kick, int damage );
extern void insane_run( edict_t *self );
extern void insane_stand( edict_t *self );
extern void insane_walk( edict_t *self );
extern void jorg_attack( edict_t *self );
extern bool Jorg_CheckAttack( edict_t *self );
extern void jorg_die( edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point );
extern void jorg_pain( edict_t *self, edict_t *other, float kick, int damage );
extern void jorg_run( edict_t *self );
extern void jorg_search( edict_t *self );
extern void jorg_stand( edict_t *self );
extern void jorg_walk( edict_t *self );
extern void spotlight_think( edict_t *self );
extern void spotlight_use( edict_t *self, edict_t *other, edict_t *activator );
extern void light_use( edict_t *self, edict_t *other, edict_t *activator );
extern bool M_CheckAttack( edict_t *self );
extern void M_droptofloor( edict_t *self );
extern void M_FliesOff( edict_t *self );
extern void M_FliesOn( edict_t *self );
extern void makron_attack( edict_t *self );
extern bool Makron_CheckAttack( edict_t *self );
extern void makron_die( edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point );
extern void makron_pain( edict_t *self, edict_t *other, float kick, int damage );
extern void makron_run( edict_t *self );
extern void makron_sight( edict_t *self, edict_t *other );
extern void makron_stand( edict_t *self );
extern void makron_torso_think( edict_t *self );
extern void makron_walk( edict_t *self );
extern void MakronSpawn( edict_t *self );
extern void medic_attack( edict_t *self );
extern bool medic_checkattack( edict_t *self );
extern void medic_die( edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point );
extern void medic_dodge( edict_t *self, edict_t *attacker, float eta );
extern void medic_idle( edict_t *self );
extern void medic_pain( edict_t *self, edict_t *other, float kick, int damage );
extern void medic_run( edict_t *self );
extern void medic_search( edict_t *self );
extern void medic_sight( edict_t *self, edict_t *other );
extern void medic_stand( edict_t *self );
extern void medic_walk( edict_t *self );
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
extern bool mutant_checkattack( edict_t *self );
extern void mutant_die( edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point );
extern void mutant_idle( edict_t *self );
extern void mutant_jump( edict_t *self );
extern void mutant_jump_touch( edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf );
extern void mutant_melee( edict_t *self );
extern void mutant_pain( edict_t *self, edict_t *other, float kick, int damage );
extern void mutant_run( edict_t *self );
extern void mutant_search( edict_t *self );
extern void mutant_sight( edict_t *self, edict_t *other );
extern void mutant_stand( edict_t *self );
extern void mutant_walk( edict_t *self );
extern void parasite_attack( edict_t *self );
extern void parasite_die( edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point );
extern void parasite_idle( edict_t *self );
extern void parasite_pain( edict_t *self, edict_t *other, float kick, int damage );
extern void parasite_sight( edict_t *self, edict_t *other );
extern void parasite_stand( edict_t *self );
extern void parasite_start_run( edict_t *self );
extern void parasite_start_walk( edict_t *self );
extern void path_corner_touch( edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf );
extern void plat_blocked( edict_t *self, edict_t *other );
extern void plat_go_down( edict_t *self );
extern void plat_hit_bottom( edict_t *self );
extern void plat_hit_top( edict_t *self );
extern void player_die( edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point );
extern void player_pain( edict_t *self, edict_t *other, float kick, int damage );
extern void point_combat_touch( edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf );
extern void rocket_touch( edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf );
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
extern void supertank_attack( edict_t *self );
extern void supertank_die( edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point );
extern void supertank_pain( edict_t *self, edict_t *other, float kick, int damage );
extern void supertank_run( edict_t *self );
extern void supertank_search( edict_t *self );
extern void supertank_stand( edict_t *self );
extern void supertank_walk( edict_t *self );
extern void swimmonster_start_go( edict_t *self );
extern void tank_attack( edict_t *self );
extern void tank_die( edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point );
extern void tank_idle( edict_t *self );
extern void tank_pain( edict_t *self, edict_t *other, float kick, int damage );
extern void tank_run( edict_t *self );
extern void tank_sight( edict_t *self, edict_t *other );
extern void tank_stand( edict_t *self );
extern void tank_walk( edict_t *self );
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
extern void Think_Boss3Stand( edict_t *self );
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
extern void Use_Boss3( edict_t *ent, edict_t *other, edict_t *activator );
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
{ P_think,  (void*)bfg_explode },
{ P_think,  (void*)bfg_think },
{ P_think,  (void*)BossExplode },
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
{ P_think,  (void*)flare_think },
{ P_think,  (void*)flymonster_start_go },
{ P_think,  (void*)func_clock_think },
{ P_think,  (void*)func_object_release },
{ P_think,  (void*)func_timer_think },
{ P_think,  (void*)func_train_find },
{ P_think,  (void*)G_FreeEdict },
{ P_think,  (void*)gib_think },
{ P_think,  (void*)Grenade_Explode },
{ P_think,  (void*)hover_deadthink },
{ P_think,  (void*)M_droptofloor },
{ P_think,  (void*)M_FliesOff },
{ P_think,  (void*)M_FliesOn },
{ P_think,  (void*)makron_torso_think },
{ P_think,  (void*)MakronSpawn },
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
{ P_think,  (void*)Think_Boss3Stand },
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
{ P_touch, (void*)bfg_touch },
{ P_touch, (void*)blaster_touch },
{ P_touch, (void*)button_touch },
{ P_touch, (void*)door_touch },
{ P_touch, (void*)drop_temp_touch },
{ P_touch, (void*)flare_touch },
{ P_touch, (void*)func_object_touch },
{ P_touch, (void*)gib_touch },
{ P_touch, (void*)Grenade_Touch },
{ P_touch, (void*)hurt_touch },
{ P_touch, (void*)misc_viper_bomb_touch },
{ P_touch, (void*)mutant_jump_touch },
{ P_touch, (void*)path_corner_touch },
{ P_touch, (void*)point_combat_touch },
{ P_touch, (void*)rocket_touch },
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
{ P_use, (void*)Use_Boss3 },
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
{ P_pain, (void*)berserk_pain },
{ P_pain, (void*)boss2_pain },
{ P_pain, (void*)brain_pain },
{ P_pain, (void*)chick_pain },
{ P_pain, (void*)flipper_pain },
{ P_pain, (void*)floater_pain },
{ P_pain, (void*)flyer_pain },
{ P_pain, (void*)gladiator_pain },
{ P_pain, (void*)gunner_pain },
{ P_pain, (void*)hover_pain },
{ P_pain, (void*)infantry_pain },
{ P_pain, (void*)insane_pain },
{ P_pain, (void*)jorg_pain },
{ P_pain, (void*)makron_pain },
{ P_pain, (void*)medic_pain },
{ P_pain, (void*)mutant_pain },
{ P_pain, (void*)parasite_pain },
{ P_pain, (void*)player_pain },
{ P_pain, (void*)soldier_pain },
{ P_pain, (void*)supertank_pain },
{ P_pain, (void*)tank_pain },
{ P_die, (void*)actor_die },
{ P_die, (void*)barrel_delay },
{ P_die, (void*)berserk_die },
{ P_die, (void*)body_die },
{ P_die, (void*)boss2_die },
{ P_die, (void*)brain_die },
{ P_die, (void*)button_killed },
{ P_die, (void*)chick_die },
{ P_die, (void*)debris_die },
{ P_die, (void*)door_killed },
{ P_die, (void*)door_secret_die },
{ P_die, (void*)flipper_die },
{ P_die, (void*)floater_die },
{ P_die, (void*)flyer_die },
{ P_die, (void*)func_explosive_explode },
{ P_die, (void*)gib_die },
{ P_die, (void*)gladiator_die },
{ P_die, (void*)gunner_die },
{ P_die, (void*)hover_die },
{ P_die, (void*)infantry_die },
{ P_die, (void*)insane_die },
{ P_die, (void*)jorg_die },
{ P_die, (void*)makron_die },
{ P_die, (void*)medic_die },
{ P_die, (void*)misc_deadsoldier_die },
{ P_die, (void*)mutant_die },
{ P_die, (void*)parasite_die },
{ P_die, (void*)player_die },
{ P_die, (void*)soldier_die },
{ P_die, (void*)supertank_die },
{ P_die, (void*)tank_die },
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
{ P_monsterinfo_currentmove, &berserk_move_attack_club },
{ P_monsterinfo_currentmove, &berserk_move_attack_spike },
{ P_monsterinfo_currentmove, &berserk_move_death1 },
{ P_monsterinfo_currentmove, &berserk_move_death2 },
{ P_monsterinfo_currentmove, &berserk_move_pain1 },
{ P_monsterinfo_currentmove, &berserk_move_pain2 },
{ P_monsterinfo_currentmove, &berserk_move_run1 },
{ P_monsterinfo_currentmove, &berserk_move_stand },
{ P_monsterinfo_currentmove, &berserk_move_stand_fidget },
{ P_monsterinfo_currentmove, &berserk_move_walk },
{ P_monsterinfo_currentmove, &boss2_move_attack_mg },
{ P_monsterinfo_currentmove, &boss2_move_attack_post_mg },
{ P_monsterinfo_currentmove, &boss2_move_attack_pre_mg },
{ P_monsterinfo_currentmove, &boss2_move_attack_rocket },
{ P_monsterinfo_currentmove, &boss2_move_death },
{ P_monsterinfo_currentmove, &boss2_move_pain_heavy },
{ P_monsterinfo_currentmove, &boss2_move_pain_light },
{ P_monsterinfo_currentmove, &boss2_move_run },
{ P_monsterinfo_currentmove, &boss2_move_stand },
{ P_monsterinfo_currentmove, &boss2_move_walk },
{ P_monsterinfo_currentmove, &brain_move_attack1 },
{ P_monsterinfo_currentmove, &brain_move_attack2 },
{ P_monsterinfo_currentmove, &brain_move_death1 },
{ P_monsterinfo_currentmove, &brain_move_death2 },
{ P_monsterinfo_currentmove, &brain_move_duck },
{ P_monsterinfo_currentmove, &brain_move_idle },
{ P_monsterinfo_currentmove, &brain_move_pain1 },
{ P_monsterinfo_currentmove, &brain_move_pain2 },
{ P_monsterinfo_currentmove, &brain_move_pain3 },
{ P_monsterinfo_currentmove, &brain_move_run },
{ P_monsterinfo_currentmove, &brain_move_stand },
{ P_monsterinfo_currentmove, &brain_move_walk1 },
{ P_monsterinfo_currentmove, &chick_move_attack1 },
{ P_monsterinfo_currentmove, &chick_move_death1 },
{ P_monsterinfo_currentmove, &chick_move_death2 },
{ P_monsterinfo_currentmove, &chick_move_duck },
{ P_monsterinfo_currentmove, &chick_move_end_attack1 },
{ P_monsterinfo_currentmove, &chick_move_end_slash },
{ P_monsterinfo_currentmove, &chick_move_fidget },
{ P_monsterinfo_currentmove, &chick_move_pain1 },
{ P_monsterinfo_currentmove, &chick_move_pain2 },
{ P_monsterinfo_currentmove, &chick_move_pain3 },
{ P_monsterinfo_currentmove, &chick_move_run },
{ P_monsterinfo_currentmove, &chick_move_slash },
{ P_monsterinfo_currentmove, &chick_move_stand },
{ P_monsterinfo_currentmove, &chick_move_start_attack1 },
{ P_monsterinfo_currentmove, &chick_move_start_run },
{ P_monsterinfo_currentmove, &chick_move_start_slash },
{ P_monsterinfo_currentmove, &chick_move_walk },
{ P_monsterinfo_currentmove, &flipper_move_attack },
{ P_monsterinfo_currentmove, &flipper_move_death },
{ P_monsterinfo_currentmove, &flipper_move_pain1 },
{ P_monsterinfo_currentmove, &flipper_move_pain2 },
{ P_monsterinfo_currentmove, &flipper_move_run_loop },
{ P_monsterinfo_currentmove, &flipper_move_run_start },
{ P_monsterinfo_currentmove, &flipper_move_stand },
{ P_monsterinfo_currentmove, &flipper_move_start_run },
{ P_monsterinfo_currentmove, &flipper_move_walk },
{ P_monsterinfo_currentmove, &floater_move_attack1 },
{ P_monsterinfo_currentmove, &floater_move_attack2 },
{ P_monsterinfo_currentmove, &floater_move_attack3 },
{ P_monsterinfo_currentmove, &floater_move_pain1 },
{ P_monsterinfo_currentmove, &floater_move_pain2 },
{ P_monsterinfo_currentmove, &floater_move_run },
{ P_monsterinfo_currentmove, &floater_move_stand1 },
{ P_monsterinfo_currentmove, &floater_move_stand2 },
{ P_monsterinfo_currentmove, &floater_move_walk },
{ P_monsterinfo_currentmove, &flyer_move_attack2 },
{ P_monsterinfo_currentmove, &flyer_move_end_melee },
{ P_monsterinfo_currentmove, &flyer_move_loop_melee },
{ P_monsterinfo_currentmove, &flyer_move_pain1 },
{ P_monsterinfo_currentmove, &flyer_move_pain2 },
{ P_monsterinfo_currentmove, &flyer_move_pain3 },
{ P_monsterinfo_currentmove, &flyer_move_run },
{ P_monsterinfo_currentmove, &flyer_move_stand },
{ P_monsterinfo_currentmove, &flyer_move_start },
{ P_monsterinfo_currentmove, &flyer_move_start_melee },
{ P_monsterinfo_currentmove, &flyer_move_stop },
{ P_monsterinfo_currentmove, &flyer_move_walk },
{ P_monsterinfo_currentmove, &gladiator_move_attack_gun },
{ P_monsterinfo_currentmove, &gladiator_move_attack_melee },
{ P_monsterinfo_currentmove, &gladiator_move_death },
{ P_monsterinfo_currentmove, &gladiator_move_pain },
{ P_monsterinfo_currentmove, &gladiator_move_pain_air },
{ P_monsterinfo_currentmove, &gladiator_move_run },
{ P_monsterinfo_currentmove, &gladiator_move_stand },
{ P_monsterinfo_currentmove, &gladiator_move_walk },
{ P_monsterinfo_currentmove, &gunner_move_attack_chain },
{ P_monsterinfo_currentmove, &gunner_move_attack_grenade },
{ P_monsterinfo_currentmove, &gunner_move_death },
{ P_monsterinfo_currentmove, &gunner_move_duck },
{ P_monsterinfo_currentmove, &gunner_move_endfire_chain },
{ P_monsterinfo_currentmove, &gunner_move_fidget },
{ P_monsterinfo_currentmove, &gunner_move_fire_chain },
{ P_monsterinfo_currentmove, &gunner_move_pain1 },
{ P_monsterinfo_currentmove, &gunner_move_pain2 },
{ P_monsterinfo_currentmove, &gunner_move_pain3 },
{ P_monsterinfo_currentmove, &gunner_move_run },
{ P_monsterinfo_currentmove, &gunner_move_runandshoot },
{ P_monsterinfo_currentmove, &gunner_move_stand },
{ P_monsterinfo_currentmove, &gunner_move_walk },
{ P_monsterinfo_currentmove, &hover_move_attack1 },
{ P_monsterinfo_currentmove, &hover_move_death1 },
{ P_monsterinfo_currentmove, &hover_move_end_attack },
{ P_monsterinfo_currentmove, &hover_move_pain1 },
{ P_monsterinfo_currentmove, &hover_move_pain2 },
{ P_monsterinfo_currentmove, &hover_move_pain3 },
{ P_monsterinfo_currentmove, &hover_move_run },
{ P_monsterinfo_currentmove, &hover_move_stand },
{ P_monsterinfo_currentmove, &hover_move_start_attack },
{ P_monsterinfo_currentmove, &hover_move_walk },
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
{ P_monsterinfo_currentmove, &insane_move_crawl },
{ P_monsterinfo_currentmove, &insane_move_crawl_death },
{ P_monsterinfo_currentmove, &insane_move_crawl_pain },
{ P_monsterinfo_currentmove, &insane_move_cross },
{ P_monsterinfo_currentmove, &insane_move_down },
{ P_monsterinfo_currentmove, &insane_move_downtoup },
{ P_monsterinfo_currentmove, &insane_move_jumpdown },
{ P_monsterinfo_currentmove, &insane_move_run_insane },
{ P_monsterinfo_currentmove, &insane_move_run_normal },
{ P_monsterinfo_currentmove, &insane_move_runcrawl },
{ P_monsterinfo_currentmove, &insane_move_stand_death },
{ P_monsterinfo_currentmove, &insane_move_stand_insane },
{ P_monsterinfo_currentmove, &insane_move_stand_normal },
{ P_monsterinfo_currentmove, &insane_move_stand_pain },
{ P_monsterinfo_currentmove, &insane_move_struggle_cross },
{ P_monsterinfo_currentmove, &insane_move_uptodown },
{ P_monsterinfo_currentmove, &insane_move_walk_insane },
{ P_monsterinfo_currentmove, &insane_move_walk_normal },
{ P_monsterinfo_currentmove, &jorg_move_attack1 },
{ P_monsterinfo_currentmove, &jorg_move_attack2 },
{ P_monsterinfo_currentmove, &jorg_move_death },
{ P_monsterinfo_currentmove, &jorg_move_end_attack1 },
{ P_monsterinfo_currentmove, &jorg_move_pain1 },
{ P_monsterinfo_currentmove, &jorg_move_pain2 },
{ P_monsterinfo_currentmove, &jorg_move_pain3 },
{ P_monsterinfo_currentmove, &jorg_move_run },
{ P_monsterinfo_currentmove, &jorg_move_stand },
{ P_monsterinfo_currentmove, &jorg_move_start_attack1 },
{ P_monsterinfo_currentmove, &jorg_move_walk },
{ P_monsterinfo_currentmove, &makron_move_attack3 },
{ P_monsterinfo_currentmove, &makron_move_attack4 },
{ P_monsterinfo_currentmove, &makron_move_attack5 },
{ P_monsterinfo_currentmove, &makron_move_death2 },
{ P_monsterinfo_currentmove, &makron_move_pain4 },
{ P_monsterinfo_currentmove, &makron_move_pain5 },
{ P_monsterinfo_currentmove, &makron_move_pain6 },
{ P_monsterinfo_currentmove, &makron_move_run },
{ P_monsterinfo_currentmove, &makron_move_sight },
{ P_monsterinfo_currentmove, &makron_move_stand },
{ P_monsterinfo_currentmove, &makron_move_walk },
{ P_monsterinfo_currentmove, &medic_move_attackBlaster },
{ P_monsterinfo_currentmove, &medic_move_attackCable },
{ P_monsterinfo_currentmove, &medic_move_attackHyperBlaster },
{ P_monsterinfo_currentmove, &medic_move_death },
{ P_monsterinfo_currentmove, &medic_move_duck },
{ P_monsterinfo_currentmove, &medic_move_pain1 },
{ P_monsterinfo_currentmove, &medic_move_pain2 },
{ P_monsterinfo_currentmove, &medic_move_run },
{ P_monsterinfo_currentmove, &medic_move_stand },
{ P_monsterinfo_currentmove, &medic_move_walk },
{ P_monsterinfo_currentmove, &mutant_move_attack },
{ P_monsterinfo_currentmove, &mutant_move_death1 },
{ P_monsterinfo_currentmove, &mutant_move_death2 },
{ P_monsterinfo_currentmove, &mutant_move_idle },
{ P_monsterinfo_currentmove, &mutant_move_jump },
{ P_monsterinfo_currentmove, &mutant_move_pain1 },
{ P_monsterinfo_currentmove, &mutant_move_pain2 },
{ P_monsterinfo_currentmove, &mutant_move_pain3 },
{ P_monsterinfo_currentmove, &mutant_move_run },
{ P_monsterinfo_currentmove, &mutant_move_stand },
{ P_monsterinfo_currentmove, &mutant_move_start_walk },
{ P_monsterinfo_currentmove, &mutant_move_walk },
{ P_monsterinfo_currentmove, &parasite_move_death },
{ P_monsterinfo_currentmove, &parasite_move_drain },
{ P_monsterinfo_currentmove, &parasite_move_end_fidget },
{ P_monsterinfo_currentmove, &parasite_move_fidget },
{ P_monsterinfo_currentmove, &parasite_move_pain1 },
{ P_monsterinfo_currentmove, &parasite_move_run },
{ P_monsterinfo_currentmove, &parasite_move_stand },
{ P_monsterinfo_currentmove, &parasite_move_start_fidget },
{ P_monsterinfo_currentmove, &parasite_move_start_run },
{ P_monsterinfo_currentmove, &parasite_move_start_walk },
{ P_monsterinfo_currentmove, &parasite_move_walk },
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
{ P_monsterinfo_currentmove, &supertank_move_attack1 },
{ P_monsterinfo_currentmove, &supertank_move_attack2 },
{ P_monsterinfo_currentmove, &supertank_move_death },
{ P_monsterinfo_currentmove, &supertank_move_end_attack1 },
{ P_monsterinfo_currentmove, &supertank_move_forward },
{ P_monsterinfo_currentmove, &supertank_move_pain1 },
{ P_monsterinfo_currentmove, &supertank_move_pain2 },
{ P_monsterinfo_currentmove, &supertank_move_pain3 },
{ P_monsterinfo_currentmove, &supertank_move_run },
{ P_monsterinfo_currentmove, &supertank_move_stand },
{ P_monsterinfo_currentmove, &tank_move_attack_blast },
{ P_monsterinfo_currentmove, &tank_move_attack_chain },
{ P_monsterinfo_currentmove, &tank_move_attack_fire_rocket },
{ P_monsterinfo_currentmove, &tank_move_attack_post_blast },
{ P_monsterinfo_currentmove, &tank_move_attack_post_rocket },
{ P_monsterinfo_currentmove, &tank_move_attack_pre_rocket },
{ P_monsterinfo_currentmove, &tank_move_attack_strike },
{ P_monsterinfo_currentmove, &tank_move_death },
{ P_monsterinfo_currentmove, &tank_move_pain1 },
{ P_monsterinfo_currentmove, &tank_move_pain2 },
{ P_monsterinfo_currentmove, &tank_move_pain3 },
{ P_monsterinfo_currentmove, &tank_move_reattack_blast },
{ P_monsterinfo_currentmove, &tank_move_run },
{ P_monsterinfo_currentmove, &tank_move_stand },
{ P_monsterinfo_currentmove, &tank_move_start_run },
{ P_monsterinfo_currentmove, &tank_move_walk },

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
{ P_monsterinfo_nextmove, &berserk_move_attack_club },
{ P_monsterinfo_nextmove, &berserk_move_attack_spike },
{ P_monsterinfo_nextmove, &berserk_move_death1 },
{ P_monsterinfo_nextmove, &berserk_move_death2 },
{ P_monsterinfo_nextmove, &berserk_move_pain1 },
{ P_monsterinfo_nextmove, &berserk_move_pain2 },
{ P_monsterinfo_nextmove, &berserk_move_run1 },
{ P_monsterinfo_nextmove, &berserk_move_stand },
{ P_monsterinfo_nextmove, &berserk_move_stand_fidget },
{ P_monsterinfo_nextmove, &berserk_move_walk },
{ P_monsterinfo_nextmove, &boss2_move_attack_mg },
{ P_monsterinfo_nextmove, &boss2_move_attack_post_mg },
{ P_monsterinfo_nextmove, &boss2_move_attack_pre_mg },
{ P_monsterinfo_nextmove, &boss2_move_attack_rocket },
{ P_monsterinfo_nextmove, &boss2_move_death },
{ P_monsterinfo_nextmove, &boss2_move_pain_heavy },
{ P_monsterinfo_nextmove, &boss2_move_pain_light },
{ P_monsterinfo_nextmove, &boss2_move_run },
{ P_monsterinfo_nextmove, &boss2_move_stand },
{ P_monsterinfo_nextmove, &boss2_move_walk },
{ P_monsterinfo_nextmove, &brain_move_attack1 },
{ P_monsterinfo_nextmove, &brain_move_attack2 },
{ P_monsterinfo_nextmove, &brain_move_death1 },
{ P_monsterinfo_nextmove, &brain_move_death2 },
{ P_monsterinfo_nextmove, &brain_move_duck },
{ P_monsterinfo_nextmove, &brain_move_idle },
{ P_monsterinfo_nextmove, &brain_move_pain1 },
{ P_monsterinfo_nextmove, &brain_move_pain2 },
{ P_monsterinfo_nextmove, &brain_move_pain3 },
{ P_monsterinfo_nextmove, &brain_move_run },
{ P_monsterinfo_nextmove, &brain_move_stand },
{ P_monsterinfo_nextmove, &brain_move_walk1 },
{ P_monsterinfo_nextmove, &chick_move_attack1 },
{ P_monsterinfo_nextmove, &chick_move_death1 },
{ P_monsterinfo_nextmove, &chick_move_death2 },
{ P_monsterinfo_nextmove, &chick_move_duck },
{ P_monsterinfo_nextmove, &chick_move_end_attack1 },
{ P_monsterinfo_nextmove, &chick_move_end_slash },
{ P_monsterinfo_nextmove, &chick_move_fidget },
{ P_monsterinfo_nextmove, &chick_move_pain1 },
{ P_monsterinfo_nextmove, &chick_move_pain2 },
{ P_monsterinfo_nextmove, &chick_move_pain3 },
{ P_monsterinfo_nextmove, &chick_move_run },
{ P_monsterinfo_nextmove, &chick_move_slash },
{ P_monsterinfo_nextmove, &chick_move_stand },
{ P_monsterinfo_nextmove, &chick_move_start_attack1 },
{ P_monsterinfo_nextmove, &chick_move_start_run },
{ P_monsterinfo_nextmove, &chick_move_start_slash },
{ P_monsterinfo_nextmove, &chick_move_walk },
{ P_monsterinfo_nextmove, &flipper_move_attack },
{ P_monsterinfo_nextmove, &flipper_move_death },
{ P_monsterinfo_nextmove, &flipper_move_pain1 },
{ P_monsterinfo_nextmove, &flipper_move_pain2 },
{ P_monsterinfo_nextmove, &flipper_move_run_loop },
{ P_monsterinfo_nextmove, &flipper_move_run_start },
{ P_monsterinfo_nextmove, &flipper_move_stand },
{ P_monsterinfo_nextmove, &flipper_move_start_run },
{ P_monsterinfo_nextmove, &flipper_move_walk },
{ P_monsterinfo_nextmove, &floater_move_attack1 },
{ P_monsterinfo_nextmove, &floater_move_attack2 },
{ P_monsterinfo_nextmove, &floater_move_attack3 },
{ P_monsterinfo_nextmove, &floater_move_pain1 },
{ P_monsterinfo_nextmove, &floater_move_pain2 },
{ P_monsterinfo_nextmove, &floater_move_run },
{ P_monsterinfo_nextmove, &floater_move_stand1 },
{ P_monsterinfo_nextmove, &floater_move_stand2 },
{ P_monsterinfo_nextmove, &floater_move_walk },
{ P_monsterinfo_nextmove, &flyer_move_attack2 },
{ P_monsterinfo_nextmove, &flyer_move_end_melee },
{ P_monsterinfo_nextmove, &flyer_move_loop_melee },
{ P_monsterinfo_nextmove, &flyer_move_pain1 },
{ P_monsterinfo_nextmove, &flyer_move_pain2 },
{ P_monsterinfo_nextmove, &flyer_move_pain3 },
{ P_monsterinfo_nextmove, &flyer_move_run },
{ P_monsterinfo_nextmove, &flyer_move_stand },
{ P_monsterinfo_nextmove, &flyer_move_start },
{ P_monsterinfo_nextmove, &flyer_move_start_melee },
{ P_monsterinfo_nextmove, &flyer_move_stop },
{ P_monsterinfo_nextmove, &flyer_move_walk },
{ P_monsterinfo_nextmove, &gladiator_move_attack_gun },
{ P_monsterinfo_nextmove, &gladiator_move_attack_melee },
{ P_monsterinfo_nextmove, &gladiator_move_death },
{ P_monsterinfo_nextmove, &gladiator_move_pain },
{ P_monsterinfo_nextmove, &gladiator_move_pain_air },
{ P_monsterinfo_nextmove, &gladiator_move_run },
{ P_monsterinfo_nextmove, &gladiator_move_stand },
{ P_monsterinfo_nextmove, &gladiator_move_walk },
{ P_monsterinfo_nextmove, &gunner_move_attack_chain },
{ P_monsterinfo_nextmove, &gunner_move_attack_grenade },
{ P_monsterinfo_nextmove, &gunner_move_death },
{ P_monsterinfo_nextmove, &gunner_move_duck },
{ P_monsterinfo_nextmove, &gunner_move_endfire_chain },
{ P_monsterinfo_nextmove, &gunner_move_fidget },
{ P_monsterinfo_nextmove, &gunner_move_fire_chain },
{ P_monsterinfo_nextmove, &gunner_move_pain1 },
{ P_monsterinfo_nextmove, &gunner_move_pain2 },
{ P_monsterinfo_nextmove, &gunner_move_pain3 },
{ P_monsterinfo_nextmove, &gunner_move_run },
{ P_monsterinfo_nextmove, &gunner_move_runandshoot },
{ P_monsterinfo_nextmove, &gunner_move_stand },
{ P_monsterinfo_nextmove, &gunner_move_walk },
{ P_monsterinfo_nextmove, &hover_move_attack1 },
{ P_monsterinfo_nextmove, &hover_move_death1 },
{ P_monsterinfo_nextmove, &hover_move_end_attack },
{ P_monsterinfo_nextmove, &hover_move_pain1 },
{ P_monsterinfo_nextmove, &hover_move_pain2 },
{ P_monsterinfo_nextmove, &hover_move_pain3 },
{ P_monsterinfo_nextmove, &hover_move_run },
{ P_monsterinfo_nextmove, &hover_move_stand },
{ P_monsterinfo_nextmove, &hover_move_start_attack },
{ P_monsterinfo_nextmove, &hover_move_walk },
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
{ P_monsterinfo_nextmove, &insane_move_crawl },
{ P_monsterinfo_nextmove, &insane_move_crawl_death },
{ P_monsterinfo_nextmove, &insane_move_crawl_pain },
{ P_monsterinfo_nextmove, &insane_move_cross },
{ P_monsterinfo_nextmove, &insane_move_down },
{ P_monsterinfo_nextmove, &insane_move_downtoup },
{ P_monsterinfo_nextmove, &insane_move_jumpdown },
{ P_monsterinfo_nextmove, &insane_move_run_insane },
{ P_monsterinfo_nextmove, &insane_move_run_normal },
{ P_monsterinfo_nextmove, &insane_move_runcrawl },
{ P_monsterinfo_nextmove, &insane_move_stand_death },
{ P_monsterinfo_nextmove, &insane_move_stand_insane },
{ P_monsterinfo_nextmove, &insane_move_stand_normal },
{ P_monsterinfo_nextmove, &insane_move_stand_pain },
{ P_monsterinfo_nextmove, &insane_move_struggle_cross },
{ P_monsterinfo_nextmove, &insane_move_uptodown },
{ P_monsterinfo_nextmove, &insane_move_walk_insane },
{ P_monsterinfo_nextmove, &insane_move_walk_normal },
{ P_monsterinfo_nextmove, &jorg_move_attack1 },
{ P_monsterinfo_nextmove, &jorg_move_attack2 },
{ P_monsterinfo_nextmove, &jorg_move_death },
{ P_monsterinfo_nextmove, &jorg_move_end_attack1 },
{ P_monsterinfo_nextmove, &jorg_move_pain1 },
{ P_monsterinfo_nextmove, &jorg_move_pain2 },
{ P_monsterinfo_nextmove, &jorg_move_pain3 },
{ P_monsterinfo_nextmove, &jorg_move_run },
{ P_monsterinfo_nextmove, &jorg_move_stand },
{ P_monsterinfo_nextmove, &jorg_move_start_attack1 },
{ P_monsterinfo_nextmove, &jorg_move_walk },
{ P_monsterinfo_nextmove, &makron_move_attack3 },
{ P_monsterinfo_nextmove, &makron_move_attack4 },
{ P_monsterinfo_nextmove, &makron_move_attack5 },
{ P_monsterinfo_nextmove, &makron_move_death2 },
{ P_monsterinfo_nextmove, &makron_move_pain4 },
{ P_monsterinfo_nextmove, &makron_move_pain5 },
{ P_monsterinfo_nextmove, &makron_move_pain6 },
{ P_monsterinfo_nextmove, &makron_move_run },
{ P_monsterinfo_nextmove, &makron_move_sight },
{ P_monsterinfo_nextmove, &makron_move_stand },
{ P_monsterinfo_nextmove, &makron_move_walk },
{ P_monsterinfo_nextmove, &medic_move_attackBlaster },
{ P_monsterinfo_nextmove, &medic_move_attackCable },
{ P_monsterinfo_nextmove, &medic_move_attackHyperBlaster },
{ P_monsterinfo_nextmove, &medic_move_death },
{ P_monsterinfo_nextmove, &medic_move_duck },
{ P_monsterinfo_nextmove, &medic_move_pain1 },
{ P_monsterinfo_nextmove, &medic_move_pain2 },
{ P_monsterinfo_nextmove, &medic_move_run },
{ P_monsterinfo_nextmove, &medic_move_stand },
{ P_monsterinfo_nextmove, &medic_move_walk },
{ P_monsterinfo_nextmove, &mutant_move_attack },
{ P_monsterinfo_nextmove, &mutant_move_death1 },
{ P_monsterinfo_nextmove, &mutant_move_death2 },
{ P_monsterinfo_nextmove, &mutant_move_idle },
{ P_monsterinfo_nextmove, &mutant_move_jump },
{ P_monsterinfo_nextmove, &mutant_move_pain1 },
{ P_monsterinfo_nextmove, &mutant_move_pain2 },
{ P_monsterinfo_nextmove, &mutant_move_pain3 },
{ P_monsterinfo_nextmove, &mutant_move_run },
{ P_monsterinfo_nextmove, &mutant_move_stand },
{ P_monsterinfo_nextmove, &mutant_move_start_walk },
{ P_monsterinfo_nextmove, &mutant_move_walk },
{ P_monsterinfo_nextmove, &parasite_move_death },
{ P_monsterinfo_nextmove, &parasite_move_drain },
{ P_monsterinfo_nextmove, &parasite_move_end_fidget },
{ P_monsterinfo_nextmove, &parasite_move_fidget },
{ P_monsterinfo_nextmove, &parasite_move_pain1 },
{ P_monsterinfo_nextmove, &parasite_move_run },
{ P_monsterinfo_nextmove, &parasite_move_stand },
{ P_monsterinfo_nextmove, &parasite_move_start_fidget },
{ P_monsterinfo_nextmove, &parasite_move_start_run },
{ P_monsterinfo_nextmove, &parasite_move_start_walk },
{ P_monsterinfo_nextmove, &parasite_move_walk },
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
{ P_monsterinfo_nextmove, &supertank_move_attack1 },
{ P_monsterinfo_nextmove, &supertank_move_attack2 },
{ P_monsterinfo_nextmove, &supertank_move_death },
{ P_monsterinfo_nextmove, &supertank_move_end_attack1 },
{ P_monsterinfo_nextmove, &supertank_move_forward },
{ P_monsterinfo_nextmove, &supertank_move_pain1 },
{ P_monsterinfo_nextmove, &supertank_move_pain2 },
{ P_monsterinfo_nextmove, &supertank_move_pain3 },
{ P_monsterinfo_nextmove, &supertank_move_run },
{ P_monsterinfo_nextmove, &supertank_move_stand },
{ P_monsterinfo_nextmove, &tank_move_attack_blast },
{ P_monsterinfo_nextmove, &tank_move_attack_chain },
{ P_monsterinfo_nextmove, &tank_move_attack_fire_rocket },
{ P_monsterinfo_nextmove, &tank_move_attack_post_blast },
{ P_monsterinfo_nextmove, &tank_move_attack_post_rocket },
{ P_monsterinfo_nextmove, &tank_move_attack_pre_rocket },
{ P_monsterinfo_nextmove, &tank_move_attack_strike },
{ P_monsterinfo_nextmove, &tank_move_death },
{ P_monsterinfo_nextmove, &tank_move_pain1 },
{ P_monsterinfo_nextmove, &tank_move_pain2 },
{ P_monsterinfo_nextmove, &tank_move_pain3 },
{ P_monsterinfo_nextmove, &tank_move_reattack_blast },
{ P_monsterinfo_nextmove, &tank_move_run },
{ P_monsterinfo_nextmove, &tank_move_stand },
{ P_monsterinfo_nextmove, &tank_move_start_run },
{ P_monsterinfo_nextmove, &tank_move_walk },

//
// MonsterInfo_Stand.
//
{ P_monsterinfo_stand, (void*)actor_stand },
{ P_monsterinfo_stand, (void*)berserk_stand },
{ P_monsterinfo_stand, (void*)boss2_stand },
{ P_monsterinfo_stand, (void*)brain_stand },
{ P_monsterinfo_stand, (void*)chick_stand },
{ P_monsterinfo_stand, (void*)flipper_stand },
{ P_monsterinfo_stand, (void*)floater_stand },
{ P_monsterinfo_stand, (void*)flyer_stand },
{ P_monsterinfo_stand, (void*)gladiator_stand },
{ P_monsterinfo_stand, (void*)gunner_stand },
{ P_monsterinfo_stand, (void*)hover_stand },
{ P_monsterinfo_stand, (void*)infantry_stand },
{ P_monsterinfo_stand, (void*)insane_stand },
{ P_monsterinfo_stand, (void*)jorg_stand },
{ P_monsterinfo_stand, (void*)makron_stand },
{ P_monsterinfo_stand, (void*)medic_stand },
{ P_monsterinfo_stand, (void*)mutant_stand },
{ P_monsterinfo_stand, (void*)parasite_stand },
{ P_monsterinfo_stand, (void*)soldier_stand },
{ P_monsterinfo_stand, (void*)supertank_stand },
{ P_monsterinfo_stand, (void*)tank_stand },
{ P_monsterinfo_idle, (void*)brain_idle },
{ P_monsterinfo_idle, (void*)floater_idle },
{ P_monsterinfo_idle, (void*)flyer_idle },
{ P_monsterinfo_idle, (void*)gladiator_idle },
{ P_monsterinfo_idle, (void*)infantry_fidget },
{ P_monsterinfo_idle, (void*)medic_idle },
{ P_monsterinfo_idle, (void*)mutant_idle },
{ P_monsterinfo_idle, (void*)parasite_idle },
{ P_monsterinfo_idle, (void*)tank_idle },
{ P_monsterinfo_search, (void*)berserk_search },
{ P_monsterinfo_search, (void*)boss2_search },
{ P_monsterinfo_search, (void*)brain_search },
{ P_monsterinfo_search, (void*)gladiator_search },
{ P_monsterinfo_search, (void*)gunner_search },
{ P_monsterinfo_search, (void*)hover_search },
{ P_monsterinfo_search, (void*)jorg_search },
{ P_monsterinfo_search, (void*)medic_search },
{ P_monsterinfo_search, (void*)mutant_search },
{ P_monsterinfo_search, (void*)supertank_search },
{ P_monsterinfo_walk, (void*)actor_walk },
{ P_monsterinfo_walk, (void*)berserk_walk },
{ P_monsterinfo_walk, (void*)boss2_walk },
{ P_monsterinfo_walk, (void*)brain_walk },
{ P_monsterinfo_walk, (void*)chick_walk },
{ P_monsterinfo_walk, (void*)flipper_walk },
{ P_monsterinfo_walk, (void*)floater_walk },
{ P_monsterinfo_walk, (void*)flyer_walk },
{ P_monsterinfo_walk, (void*)gladiator_walk },
{ P_monsterinfo_walk, (void*)gunner_walk },
{ P_monsterinfo_walk, (void*)hover_walk },
{ P_monsterinfo_walk, (void*)infantry_walk },
{ P_monsterinfo_walk, (void*)insane_walk },
{ P_monsterinfo_walk, (void*)jorg_walk },
{ P_monsterinfo_walk, (void*)makron_walk },
{ P_monsterinfo_walk, (void*)medic_walk },
{ P_monsterinfo_walk, (void*)mutant_walk },
{ P_monsterinfo_walk, (void*)parasite_start_walk },
{ P_monsterinfo_walk, (void*)soldier_walk },
{ P_monsterinfo_walk, (void*)supertank_walk },
{ P_monsterinfo_walk, (void*)tank_walk },
{ P_monsterinfo_run, (void*)actor_run },
{ P_monsterinfo_run, (void*)berserk_run },
{ P_monsterinfo_run, (void*)boss2_run },
{ P_monsterinfo_run, (void*)brain_run },
{ P_monsterinfo_run, (void*)chick_run },
{ P_monsterinfo_run, (void*)flipper_start_run },
{ P_monsterinfo_run, (void*)floater_run },
{ P_monsterinfo_run, (void*)flyer_run },
{ P_monsterinfo_run, (void*)gladiator_run },
{ P_monsterinfo_run, (void*)gunner_run },
{ P_monsterinfo_run, (void*)hover_run },
{ P_monsterinfo_run, (void*)infantry_run },
{ P_monsterinfo_run, (void*)insane_run },
{ P_monsterinfo_run, (void*)jorg_run },
{ P_monsterinfo_run, (void*)makron_run },
{ P_monsterinfo_run, (void*)medic_run },
{ P_monsterinfo_run, (void*)mutant_run },
{ P_monsterinfo_run, (void*)parasite_start_run },
{ P_monsterinfo_run, (void*)soldier_run },
{ P_monsterinfo_run, (void*)supertank_run },
{ P_monsterinfo_run, (void*)tank_run },
{ P_monsterinfo_dodge, (void*)brain_dodge },
{ P_monsterinfo_dodge, (void*)chick_dodge },
{ P_monsterinfo_dodge, (void*)gunner_dodge },
{ P_monsterinfo_dodge, (void*)infantry_dodge },
{ P_monsterinfo_dodge, (void*)medic_dodge },
{ P_monsterinfo_dodge, (void*)soldier_dodge },
{ P_monsterinfo_attack, (void*)actor_attack },
{ P_monsterinfo_attack, (void*)boss2_attack },
{ P_monsterinfo_attack, (void*)chick_attack },
{ P_monsterinfo_attack, (void*)floater_attack },
{ P_monsterinfo_attack, (void*)flyer_attack },
{ P_monsterinfo_attack, (void*)gladiator_attack },
{ P_monsterinfo_attack, (void*)gunner_attack },
{ P_monsterinfo_attack, (void*)hover_start_attack },
{ P_monsterinfo_attack, (void*)infantry_attack },
{ P_monsterinfo_attack, (void*)jorg_attack },
{ P_monsterinfo_attack, (void*)makron_attack },
{ P_monsterinfo_attack, (void*)medic_attack },
{ P_monsterinfo_attack, (void*)mutant_jump },
{ P_monsterinfo_attack, (void*)parasite_attack },
{ P_monsterinfo_attack, (void*)soldier_attack },
{ P_monsterinfo_attack, (void*)supertank_attack },
{ P_monsterinfo_attack, (void*)tank_attack },
{ P_monsterinfo_melee, (void*)berserk_melee },
{ P_monsterinfo_melee, (void*)brain_melee },
{ P_monsterinfo_melee, (void*)chick_melee },
{ P_monsterinfo_melee, (void*)flipper_melee },
{ P_monsterinfo_melee, (void*)floater_melee },
{ P_monsterinfo_melee, (void*)flyer_melee },
{ P_monsterinfo_melee, (void*)gladiator_melee },
{ P_monsterinfo_melee, (void*)mutant_melee },
{ P_monsterinfo_sight, (void*)berserk_sight },
{ P_monsterinfo_sight, (void*)brain_sight },
{ P_monsterinfo_sight, (void*)chick_sight },
{ P_monsterinfo_sight, (void*)flipper_sight },
{ P_monsterinfo_sight, (void*)floater_sight },
{ P_monsterinfo_sight, (void*)flyer_sight },
{ P_monsterinfo_sight, (void*)gladiator_sight },
{ P_monsterinfo_sight, (void*)gunner_sight },
{ P_monsterinfo_sight, (void*)hover_sight },
{ P_monsterinfo_sight, (void*)infantry_sight },
{ P_monsterinfo_sight, (void*)makron_sight },
{ P_monsterinfo_sight, (void*)medic_sight },
{ P_monsterinfo_sight, (void*)mutant_sight },
{ P_monsterinfo_sight, (void*)parasite_sight },
{ P_monsterinfo_sight, (void*)soldier_sight },
{ P_monsterinfo_sight, (void*)tank_sight },
{ P_monsterinfo_checkattack, (void*)Boss2_CheckAttack },
{ P_monsterinfo_checkattack, (void*)Jorg_CheckAttack },
{ P_monsterinfo_checkattack, (void*)M_CheckAttack },
{ P_monsterinfo_checkattack, (void*)Makron_CheckAttack },
{ P_monsterinfo_checkattack, (void*)medic_checkattack },
{ P_monsterinfo_checkattack, (void*)mutant_checkattack },
};
const int num_save_ptrs = sizeof( save_ptrs ) / sizeof( save_ptrs[ 0 ] );
