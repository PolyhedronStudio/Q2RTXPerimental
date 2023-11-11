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
{ P_prethink, misc_viper_bomb_prethink },
{ P_think, AngleMove_Begin },
{ P_think, AngleMove_Done },
{ P_think, AngleMove_Final },
{ P_think, barrel_explode },
{ P_think, bfg_explode },
{ P_think, bfg_think },
{ P_think, BossExplode },
{ P_think, button_return },
{ P_think, commander_body_drop },
{ P_think, commander_body_think },
{ P_think, door_go_down },
{ P_think, door_secret_move2 },
{ P_think, door_secret_move4 },
{ P_think, door_secret_move6 },
{ P_think, DoRespawn },
{ P_think, drop_make_touchable },
{ P_think, droptofloor },
{ P_think, flare_think },
{ P_think, flymonster_start_go },
{ P_think, func_clock_think },
{ P_think, func_object_release },
{ P_think, func_timer_think },
{ P_think, func_train_find },
{ P_think, G_FreeEdict },
{ P_think, gib_think },
{ P_think, Grenade_Explode },
{ P_think, hover_deadthink },
{ P_think, M_droptofloor },
{ P_think, M_FliesOff },
{ P_think, M_FliesOn },
{ P_think, makron_torso_think },
{ P_think, MakronSpawn },
{ P_think, MegaHealth_think },
{ P_think, misc_banner_think },
{ P_think, misc_blackhole_think },
{ P_think, misc_easterchick2_think },
{ P_think, misc_easterchick_think },
{ P_think, misc_eastertank_think },
{ P_think, misc_satellite_dish_think },
{ P_think, monster_think },
{ P_think, monster_triggered_spawn },
{ P_think, Move_Begin },
{ P_think, Move_Done },
{ P_think, Move_Final },
{ P_think, multi_wait },
{ P_think, plat_go_down },
{ P_think, SP_CreateCoopSpots },
{ P_think, SP_FixCoopSpots },
{ P_think, swimmonster_start_go },
{ P_think, target_crosslevel_target_think },
{ P_think, target_earthquake_think },
{ P_think, target_explosion_explode },
{ P_think, target_laser_start },
{ P_think, target_laser_think },
{ P_think, target_lightramp_think },
{ P_think, TH_viewthing },
{ P_think, Think_AccelMove },
{ P_think, Think_Boss3Stand },
{ P_think, Think_CalcMoveSpeed },
{ P_think, Think_Delay },
{ P_think, Think_SpawnDoorTrigger },
{ P_think, train_next },
{ P_think, trigger_elevator_init },
{ P_think, turret_breach_finish_init },
{ P_think, turret_breach_think },
{ P_think, turret_driver_link },
{ P_think, turret_driver_think },
{ P_think, walkmonster_start_go },
{ P_blocked, door_blocked },
{ P_blocked, door_secret_blocked },
{ P_blocked, plat_blocked },
{ P_blocked, rotating_blocked },
{ P_blocked, train_blocked },
{ P_blocked, turret_blocked },
{ P_touch, barrel_touch },
{ P_touch, bfg_touch },
{ P_touch, blaster_touch },
{ P_touch, button_touch },
{ P_touch, door_touch },
{ P_touch, drop_temp_touch },
{ P_touch, flare_touch },
{ P_touch, func_object_touch },
{ P_touch, gib_touch },
{ P_touch, Grenade_Touch },
{ P_touch, hurt_touch },
{ P_touch, misc_viper_bomb_touch },
{ P_touch, mutant_jump_touch },
{ P_touch, path_corner_touch },
{ P_touch, point_combat_touch },
{ P_touch, rocket_touch },
{ P_touch, rotating_touch },
{ P_touch, target_actor_touch },
{ P_touch, teleporter_touch },
{ P_touch, Touch_DoorTrigger },
{ P_touch, Touch_Item },
{ P_touch, Touch_Multi },
{ P_touch, Touch_Plat_Center },
{ P_touch, trigger_gravity_touch },
{ P_touch, trigger_monsterjump_touch },
{ P_touch, trigger_push_touch },
{ P_use, actor_use },
{ P_use, button_use },
{ P_use, commander_body_use },
{ P_use, door_secret_use },
{ P_use, door_use },
{ P_use, func_clock_use },
{ P_use, func_conveyor_use },
{ P_use, func_explosive_spawn },
{ P_use, func_explosive_use },
{ P_use, func_object_use },
{ P_use, func_timer_use },
{ P_use, func_wall_use },
{ P_use, hurt_use },
{ P_use, light_use },
{ P_use, misc_blackhole_use },
{ P_use, misc_satellite_dish_use },
{ P_use, misc_strogg_ship_use },
{ P_use, misc_viper_bomb_use },
{ P_use, misc_viper_use },
{ P_use, monster_triggered_spawn_use },
{ P_use, monster_use },
{ P_use, rotating_use },
{ P_use, target_earthquake_use },
{ P_use, target_laser_use },
{ P_use, target_lightramp_use },
{ P_use, target_string_use },
{ P_use, train_use },
{ P_use, trigger_counter_use },
{ P_use, trigger_crosslevel_trigger_use },
{ P_use, trigger_elevator_use },
{ P_use, trigger_enable },
{ P_use, trigger_key_use },
{ P_use, trigger_relay_use },
{ P_use, Use_Areaportal },
{ P_use, Use_Boss3 },
{ P_use, Use_Item },
{ P_use, use_killbox },
{ P_use, Use_Multi },
{ P_use, Use_Plat },
{ P_use, use_target_blaster },
{ P_use, use_target_changelevel },
{ P_use, use_target_explosion },
{ P_use, use_target_goal },
{ P_use, Use_Target_Help },
{ P_use, use_target_secret },
{ P_use, use_target_spawner },
{ P_use, Use_Target_Speaker },
{ P_use, use_target_splash },
{ P_use, Use_Target_Tent },
{ P_pain, actor_pain },
{ P_pain, berserk_pain },
{ P_pain, boss2_pain },
{ P_pain, brain_pain },
{ P_pain, chick_pain },
{ P_pain, flipper_pain },
{ P_pain, floater_pain },
{ P_pain, flyer_pain },
{ P_pain, gladiator_pain },
{ P_pain, gunner_pain },
{ P_pain, hover_pain },
{ P_pain, infantry_pain },
{ P_pain, insane_pain },
{ P_pain, jorg_pain },
{ P_pain, makron_pain },
{ P_pain, medic_pain },
{ P_pain, mutant_pain },
{ P_pain, parasite_pain },
{ P_pain, player_pain },
{ P_pain, soldier_pain },
{ P_pain, supertank_pain },
{ P_pain, tank_pain },
{ P_die, actor_die },
{ P_die, barrel_delay },
{ P_die, berserk_die },
{ P_die, body_die },
{ P_die, boss2_die },
{ P_die, brain_die },
{ P_die, button_killed },
{ P_die, chick_die },
{ P_die, debris_die },
{ P_die, door_killed },
{ P_die, door_secret_die },
{ P_die, flipper_die },
{ P_die, floater_die },
{ P_die, flyer_die },
{ P_die, func_explosive_explode },
{ P_die, gib_die },
{ P_die, gladiator_die },
{ P_die, gunner_die },
{ P_die, hover_die },
{ P_die, infantry_die },
{ P_die, insane_die },
{ P_die, jorg_die },
{ P_die, makron_die },
{ P_die, medic_die },
{ P_die, misc_deadsoldier_die },
{ P_die, mutant_die },
{ P_die, parasite_die },
{ P_die, player_die },
{ P_die, soldier_die },
{ P_die, supertank_die },
{ P_die, tank_die },
{ P_die, turret_driver_die },

//
// MoveInfo_EndFunc
//
{ P_moveinfo_endfunc, button_done },
{ P_moveinfo_endfunc, button_wait },
{ P_moveinfo_endfunc, door_hit_bottom },
{ P_moveinfo_endfunc, door_hit_top },
{ P_moveinfo_endfunc, door_secret_done },
{ P_moveinfo_endfunc, door_secret_move1 },
{ P_moveinfo_endfunc, door_secret_move3 },
{ P_moveinfo_endfunc, door_secret_move5 },
{ P_moveinfo_endfunc, plat_hit_bottom },
{ P_moveinfo_endfunc, plat_hit_top },
{ P_moveinfo_endfunc, train_wait },

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
{ P_monsterinfo_stand, actor_stand },
{ P_monsterinfo_stand, berserk_stand },
{ P_monsterinfo_stand, boss2_stand },
{ P_monsterinfo_stand, brain_stand },
{ P_monsterinfo_stand, chick_stand },
{ P_monsterinfo_stand, flipper_stand },
{ P_monsterinfo_stand, floater_stand },
{ P_monsterinfo_stand, flyer_stand },
{ P_monsterinfo_stand, gladiator_stand },
{ P_monsterinfo_stand, gunner_stand },
{ P_monsterinfo_stand, hover_stand },
{ P_monsterinfo_stand, infantry_stand },
{ P_monsterinfo_stand, insane_stand },
{ P_monsterinfo_stand, jorg_stand },
{ P_monsterinfo_stand, makron_stand },
{ P_monsterinfo_stand, medic_stand },
{ P_monsterinfo_stand, mutant_stand },
{ P_monsterinfo_stand, parasite_stand },
{ P_monsterinfo_stand, soldier_stand },
{ P_monsterinfo_stand, supertank_stand },
{ P_monsterinfo_stand, tank_stand },
{ P_monsterinfo_idle, brain_idle },
{ P_monsterinfo_idle, floater_idle },
{ P_monsterinfo_idle, flyer_idle },
{ P_monsterinfo_idle, gladiator_idle },
{ P_monsterinfo_idle, infantry_fidget },
{ P_monsterinfo_idle, medic_idle },
{ P_monsterinfo_idle, mutant_idle },
{ P_monsterinfo_idle, parasite_idle },
{ P_monsterinfo_idle, tank_idle },
{ P_monsterinfo_search, berserk_search },
{ P_monsterinfo_search, boss2_search },
{ P_monsterinfo_search, brain_search },
{ P_monsterinfo_search, gladiator_search },
{ P_monsterinfo_search, gunner_search },
{ P_monsterinfo_search, hover_search },
{ P_monsterinfo_search, jorg_search },
{ P_monsterinfo_search, medic_search },
{ P_monsterinfo_search, mutant_search },
{ P_monsterinfo_search, supertank_search },
{ P_monsterinfo_walk, actor_walk },
{ P_monsterinfo_walk, berserk_walk },
{ P_monsterinfo_walk, boss2_walk },
{ P_monsterinfo_walk, brain_walk },
{ P_monsterinfo_walk, chick_walk },
{ P_monsterinfo_walk, flipper_walk },
{ P_monsterinfo_walk, floater_walk },
{ P_monsterinfo_walk, flyer_walk },
{ P_monsterinfo_walk, gladiator_walk },
{ P_monsterinfo_walk, gunner_walk },
{ P_monsterinfo_walk, hover_walk },
{ P_monsterinfo_walk, infantry_walk },
{ P_monsterinfo_walk, insane_walk },
{ P_monsterinfo_walk, jorg_walk },
{ P_monsterinfo_walk, makron_walk },
{ P_monsterinfo_walk, medic_walk },
{ P_monsterinfo_walk, mutant_walk },
{ P_monsterinfo_walk, parasite_start_walk },
{ P_monsterinfo_walk, soldier_walk },
{ P_monsterinfo_walk, supertank_walk },
{ P_monsterinfo_walk, tank_walk },
{ P_monsterinfo_run, actor_run },
{ P_monsterinfo_run, berserk_run },
{ P_monsterinfo_run, boss2_run },
{ P_monsterinfo_run, brain_run },
{ P_monsterinfo_run, chick_run },
{ P_monsterinfo_run, flipper_start_run },
{ P_monsterinfo_run, floater_run },
{ P_monsterinfo_run, flyer_run },
{ P_monsterinfo_run, gladiator_run },
{ P_monsterinfo_run, gunner_run },
{ P_monsterinfo_run, hover_run },
{ P_monsterinfo_run, infantry_run },
{ P_monsterinfo_run, insane_run },
{ P_monsterinfo_run, jorg_run },
{ P_monsterinfo_run, makron_run },
{ P_monsterinfo_run, medic_run },
{ P_monsterinfo_run, mutant_run },
{ P_monsterinfo_run, parasite_start_run },
{ P_monsterinfo_run, soldier_run },
{ P_monsterinfo_run, supertank_run },
{ P_monsterinfo_run, tank_run },
{ P_monsterinfo_dodge, brain_dodge },
{ P_monsterinfo_dodge, chick_dodge },
{ P_monsterinfo_dodge, gunner_dodge },
{ P_monsterinfo_dodge, infantry_dodge },
{ P_monsterinfo_dodge, medic_dodge },
{ P_monsterinfo_dodge, soldier_dodge },
{ P_monsterinfo_attack, actor_attack },
{ P_monsterinfo_attack, boss2_attack },
{ P_monsterinfo_attack, chick_attack },
{ P_monsterinfo_attack, floater_attack },
{ P_monsterinfo_attack, flyer_attack },
{ P_monsterinfo_attack, gladiator_attack },
{ P_monsterinfo_attack, gunner_attack },
{ P_monsterinfo_attack, hover_start_attack },
{ P_monsterinfo_attack, infantry_attack },
{ P_monsterinfo_attack, jorg_attack },
{ P_monsterinfo_attack, makron_attack },
{ P_monsterinfo_attack, medic_attack },
{ P_monsterinfo_attack, mutant_jump },
{ P_monsterinfo_attack, parasite_attack },
{ P_monsterinfo_attack, soldier_attack },
{ P_monsterinfo_attack, supertank_attack },
{ P_monsterinfo_attack, tank_attack },
{ P_monsterinfo_melee, berserk_melee },
{ P_monsterinfo_melee, brain_melee },
{ P_monsterinfo_melee, chick_melee },
{ P_monsterinfo_melee, flipper_melee },
{ P_monsterinfo_melee, floater_melee },
{ P_monsterinfo_melee, flyer_melee },
{ P_monsterinfo_melee, gladiator_melee },
{ P_monsterinfo_melee, mutant_melee },
{ P_monsterinfo_sight, berserk_sight },
{ P_monsterinfo_sight, brain_sight },
{ P_monsterinfo_sight, chick_sight },
{ P_monsterinfo_sight, flipper_sight },
{ P_monsterinfo_sight, floater_sight },
{ P_monsterinfo_sight, flyer_sight },
{ P_monsterinfo_sight, gladiator_sight },
{ P_monsterinfo_sight, gunner_sight },
{ P_monsterinfo_sight, hover_sight },
{ P_monsterinfo_sight, infantry_sight },
{ P_monsterinfo_sight, makron_sight },
{ P_monsterinfo_sight, medic_sight },
{ P_monsterinfo_sight, mutant_sight },
{ P_monsterinfo_sight, parasite_sight },
{ P_monsterinfo_sight, soldier_sight },
{ P_monsterinfo_sight, tank_sight },
{ P_monsterinfo_checkattack, Boss2_CheckAttack },
{ P_monsterinfo_checkattack, Jorg_CheckAttack },
{ P_monsterinfo_checkattack, M_CheckAttack },
{ P_monsterinfo_checkattack, Makron_CheckAttack },
{ P_monsterinfo_checkattack, medic_checkattack },
{ P_monsterinfo_checkattack, mutant_checkattack },
};
const int num_save_ptrs = sizeof( save_ptrs ) / sizeof( save_ptrs[ 0 ] );