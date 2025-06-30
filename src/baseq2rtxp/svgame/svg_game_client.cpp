/********************************************************************
*
*
*	SVGame: Local "Client" Data Structure Implementation(s).
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/svg_game_client.h"



/**
*   Save field descriptors for restoring/writing the svg_client_t data to save games:
**/
SAVE_DESCRIPTOR_FIELDS_BEGIN( svg_client_t )
	SAVE_DESCRIPTOR_DEFINE_FIELD( svg_client_t, ps.pmove.pm_type, SD_FIELD_TYPE_INT32 ),
	SAVE_DESCRIPTOR_DEFINE_FIELD( svg_client_t, ps.pmove.pm_flags, SD_FIELD_TYPE_INT16 ),
	SAVE_DESCRIPTOR_DEFINE_FIELD( svg_client_t, ps.pmove.pm_time, SD_FIELD_TYPE_INT16 ),
	SAVE_DESCRIPTOR_DEFINE_FIELD( svg_client_t, ps.pmove.gravity, SD_FIELD_TYPE_INT16 ),
	SAVE_DESCRIPTOR_DEFINE_FIELD_ARRAY( svg_client_t, ps.pmove.origin, SD_FIELD_TYPE_VECTOR3, 1 ),
	SAVE_DESCRIPTOR_DEFINE_FIELD_ARRAY( svg_client_t, ps.pmove.delta_angles, SD_FIELD_TYPE_VECTOR3, 1 ),
	SAVE_DESCRIPTOR_DEFINE_FIELD_ARRAY( svg_client_t, ps.pmove.velocity, SD_FIELD_TYPE_VECTOR3, 1 ),
	SAVE_DESCRIPTOR_DEFINE_FIELD( svg_client_t, ps.pmove.viewheight, SD_FIELD_TYPE_INT8 ),
	SAVE_DESCRIPTOR_DEFINE_FIELD_ARRAY( svg_client_t, ps.viewangles, SD_FIELD_TYPE_VECTOR3, 1 ),
	SAVE_DESCRIPTOR_DEFINE_FIELD_ARRAY( svg_client_t, ps.viewoffset, SD_FIELD_TYPE_VECTOR3, 1 ),
	SAVE_DESCRIPTOR_DEFINE_FIELD_ARRAY( svg_client_t, ps.kick_angles, SD_FIELD_TYPE_VECTOR3, 1 ),
	SAVE_DESCRIPTOR_DEFINE_FIELD( svg_client_t, ps.gun.modelIndex, SD_FIELD_TYPE_INT32 ),
	SAVE_DESCRIPTOR_DEFINE_FIELD( svg_client_t, ps.gun.animationID, SD_FIELD_TYPE_INT32 ),
	SAVE_DESCRIPTOR_DEFINE_FIELD_ARRAY( svg_client_t, ps.screen_blend, SD_FIELD_TYPE_VECTOR4, 1 ),
	SAVE_DESCRIPTOR_DEFINE_FIELD( svg_client_t, ps.fov, SD_FIELD_TYPE_FLOAT ),
	SAVE_DESCRIPTOR_DEFINE_FIELD( svg_client_t, ps.rdflags, SD_FIELD_TYPE_INT32 ),
	SAVE_DESCRIPTOR_DEFINE_FIELD( svg_client_t, ps.bobCycle, SD_FIELD_TYPE_INT32 ),
	SAVE_DESCRIPTOR_DEFINE_FIELD_ARRAY( svg_client_t, ps.stats, SD_FIELD_TYPE_INT64, MAX_STATS ),

	SAVE_DESCRIPTOR_DEFINE_FIELD( svg_client_t, clientNum, SD_FIELD_TYPE_INT32 ),

	SAVE_DESCRIPTOR_DEFINE_FIELD_ARRAY( svg_client_t, pers.userinfo, SD_FIELD_TYPE_ZSTRING, MAX_INFO_STRING ),
	SAVE_DESCRIPTOR_DEFINE_FIELD_ARRAY( svg_client_t, pers.netname, SD_FIELD_TYPE_ZSTRING, 16 ),
	SAVE_DESCRIPTOR_DEFINE_FIELD( svg_client_t, pers.hand, SD_FIELD_TYPE_INT32 ),

	SAVE_DESCRIPTOR_DEFINE_FIELD( svg_client_t, pers.connected, SD_FIELD_TYPE_BOOL ),
	SAVE_DESCRIPTOR_DEFINE_FIELD( svg_client_t, pers.spawned, SD_FIELD_TYPE_BOOL ),

	SAVE_DESCRIPTOR_DEFINE_FIELD( svg_client_t, pers.health, SD_FIELD_TYPE_INT32 ),
	SAVE_DESCRIPTOR_DEFINE_FIELD( svg_client_t, pers.max_health, SD_FIELD_TYPE_INT32 ),
	SAVE_DESCRIPTOR_DEFINE_FIELD( svg_client_t, pers.savedFlags, SD_FIELD_TYPE_INT32 ),

	SAVE_DESCRIPTOR_DEFINE_FIELD( svg_client_t, pers.selected_item, SD_FIELD_TYPE_INT32 ),
	SAVE_DESCRIPTOR_DEFINE_FIELD_ARRAY( svg_client_t, pers.inventory, SD_FIELD_TYPE_INT32, MAX_ITEMS ),

	SAVE_DESCRIPTOR_DEFINE_FIELD( svg_client_t, pers.weapon, SD_FIELD_TYPE_ITEM ),
	SAVE_DESCRIPTOR_DEFINE_FIELD( svg_client_t, pers.lastweapon, SD_FIELD_TYPE_ITEM ),

	SAVE_DESCRIPTOR_DEFINE_FIELD_ARRAY( svg_client_t, pers.weapon_clip_ammo, SD_FIELD_TYPE_INT32, MAX_ITEMS ),

	SAVE_DESCRIPTOR_DEFINE_FIELD( svg_client_t, pers.ammoCapacities.pistol, SD_FIELD_TYPE_INT32 ),
	SAVE_DESCRIPTOR_DEFINE_FIELD( svg_client_t, pers.ammoCapacities.rifle, SD_FIELD_TYPE_INT32 ),
	SAVE_DESCRIPTOR_DEFINE_FIELD( svg_client_t, pers.ammoCapacities.smg, SD_FIELD_TYPE_INT32 ),
	SAVE_DESCRIPTOR_DEFINE_FIELD( svg_client_t, pers.ammoCapacities.sniper, SD_FIELD_TYPE_INT32 ),
	SAVE_DESCRIPTOR_DEFINE_FIELD( svg_client_t, pers.ammoCapacities.shotgun, SD_FIELD_TYPE_INT32 ),

	SAVE_DESCRIPTOR_DEFINE_FIELD( svg_client_t, pers.score, SD_FIELD_TYPE_INT32 ),
	SAVE_DESCRIPTOR_DEFINE_FIELD( svg_client_t, pers.spectator, SD_FIELD_TYPE_BOOL ),

	SAVE_DESCRIPTOR_DEFINE_FIELD( svg_client_t, showscores, SD_FIELD_TYPE_BOOL ),
	SAVE_DESCRIPTOR_DEFINE_FIELD( svg_client_t, showinventory, SD_FIELD_TYPE_BOOL ),
	SAVE_DESCRIPTOR_DEFINE_FIELD( svg_client_t, showhelp, SD_FIELD_TYPE_BOOL ),
	SAVE_DESCRIPTOR_DEFINE_FIELD( svg_client_t, showhelpicon, SD_FIELD_TYPE_BOOL ),

	SAVE_DESCRIPTOR_DEFINE_FIELD( svg_client_t, ammo_index, SD_FIELD_TYPE_INT32 ),

	SAVE_DESCRIPTOR_DEFINE_FIELD( svg_client_t, newweapon, SD_FIELD_TYPE_ITEM ),
	SAVE_DESCRIPTOR_DEFINE_FIELD( svg_client_t, weapon_thunk, SD_FIELD_TYPE_BOOL ),

	SAVE_DESCRIPTOR_DEFINE_FIELD( svg_client_t, grenade_blew_up, SD_FIELD_TYPE_BOOL ),
	SAVE_DESCRIPTOR_DEFINE_FIELD( svg_client_t, grenade_time, SD_FIELD_TYPE_INT64 ),
	SAVE_DESCRIPTOR_DEFINE_FIELD( svg_client_t, grenade_finished_time, SD_FIELD_TYPE_INT64 ),

	SAVE_DESCRIPTOR_DEFINE_FIELD( svg_client_t, frameDamage.armor, SD_FIELD_TYPE_INT32 ),
	SAVE_DESCRIPTOR_DEFINE_FIELD( svg_client_t, frameDamage.blood, SD_FIELD_TYPE_INT32 ),
	SAVE_DESCRIPTOR_DEFINE_FIELD( svg_client_t, frameDamage.knockBack, SD_FIELD_TYPE_INT32 ),
	SAVE_DESCRIPTOR_DEFINE_FIELD_ARRAY( svg_client_t, frameDamage.from, SD_FIELD_TYPE_VECTOR3, 1 ),

	SAVE_DESCRIPTOR_DEFINE_FIELD( svg_client_t, killer_yaw, SD_FIELD_TYPE_FLOAT ),

	SAVE_DESCRIPTOR_DEFINE_FIELD( svg_client_t, weaponState.mode, SD_FIELD_TYPE_INT32 ),

	SAVE_DESCRIPTOR_DEFINE_FIELD( svg_client_t, weaponState.canChangeMode, SD_FIELD_TYPE_INT32 ),

	SAVE_DESCRIPTOR_DEFINE_FIELD( svg_client_t, weaponState.aimState.isAiming, SD_FIELD_TYPE_INT32 ),

	SAVE_DESCRIPTOR_DEFINE_FIELD( svg_client_t, weaponState.animation.currentFrame, SD_FIELD_TYPE_INT32 ),
	SAVE_DESCRIPTOR_DEFINE_FIELD( svg_client_t, weaponState.animation.startFrame, SD_FIELD_TYPE_INT32 ),
	SAVE_DESCRIPTOR_DEFINE_FIELD( svg_client_t, weaponState.animation.endFrame, SD_FIELD_TYPE_INT32 ),

	SAVE_DESCRIPTOR_DEFINE_FIELD( svg_client_t, weaponState.timers.lastEmptyWeaponClick, SD_FIELD_TYPE_INT64 ),
	SAVE_DESCRIPTOR_DEFINE_FIELD( svg_client_t, weaponState.timers.lastPrimaryFire, SD_FIELD_TYPE_INT64 ),
	SAVE_DESCRIPTOR_DEFINE_FIELD( svg_client_t, weaponState.timers.lastAimedFire, SD_FIELD_TYPE_INT64 ),
	SAVE_DESCRIPTOR_DEFINE_FIELD( svg_client_t, weaponState.timers.lastDrawn, SD_FIELD_TYPE_INT64 ),
	SAVE_DESCRIPTOR_DEFINE_FIELD( svg_client_t, weaponState.timers.lastHolster, SD_FIELD_TYPE_INT64 ),

	SAVE_DESCRIPTOR_DEFINE_FIELD_ARRAY( svg_client_t, weaponKicks.offsetAngles, SD_FIELD_TYPE_VECTOR3, 1 ),
	SAVE_DESCRIPTOR_DEFINE_FIELD_ARRAY( svg_client_t, weaponKicks.offsetOrigin, SD_FIELD_TYPE_VECTOR3, 1 ),

	SAVE_DESCRIPTOR_DEFINE_FIELD_ARRAY( svg_client_t, viewMove.viewAngles, SD_FIELD_TYPE_VECTOR3, 1 ),
	SAVE_DESCRIPTOR_DEFINE_FIELD_ARRAY( svg_client_t, viewMove.viewForward, SD_FIELD_TYPE_VECTOR3, 1 ),
	SAVE_DESCRIPTOR_DEFINE_FIELD( svg_client_t, viewMove.fallTime, SD_FIELD_TYPE_INT64 ),
	SAVE_DESCRIPTOR_DEFINE_FIELD( svg_client_t, viewMove.quakeTime, SD_FIELD_TYPE_INT64 ),

	SAVE_DESCRIPTOR_DEFINE_FIELD( svg_client_t, viewMove.damageRoll, SD_FIELD_TYPE_FLOAT ),
	SAVE_DESCRIPTOR_DEFINE_FIELD( svg_client_t, viewMove.damagePitch, SD_FIELD_TYPE_FLOAT ),
	SAVE_DESCRIPTOR_DEFINE_FIELD( svg_client_t, viewMove.fallValue, SD_FIELD_TYPE_FLOAT ),

	SAVE_DESCRIPTOR_DEFINE_FIELD( svg_client_t, damage_alpha, SD_FIELD_TYPE_FLOAT ),
	SAVE_DESCRIPTOR_DEFINE_FIELD( svg_client_t, bonus_alpha, SD_FIELD_TYPE_FLOAT ),
	SAVE_DESCRIPTOR_DEFINE_FIELD_ARRAY( svg_client_t, damage_blend, SD_FIELD_TYPE_VECTOR3, 1 ),
	SAVE_DESCRIPTOR_DEFINE_FIELD( svg_client_t, bobCycle, SD_FIELD_TYPE_INT64 ),
	SAVE_DESCRIPTOR_DEFINE_FIELD( svg_client_t, oldBobCycle , SD_FIELD_TYPE_INT64 ),
	SAVE_DESCRIPTOR_DEFINE_FIELD( svg_client_t, bobFracSin, SD_FIELD_TYPE_DOUBLE ),
	SAVE_DESCRIPTOR_DEFINE_FIELD( svg_client_t, last_stair_step_frame, SD_FIELD_TYPE_INT64 ),
	SAVE_DESCRIPTOR_DEFINE_FIELD_ARRAY( svg_client_t, last_ladder_pos, SD_FIELD_TYPE_VECTOR3, 1 ),
	SAVE_DESCRIPTOR_DEFINE_FIELD( svg_client_t, last_ladder_sound, SD_FIELD_TYPE_INT64 ),

	SAVE_DESCRIPTOR_DEFINE_FIELD_ARRAY( svg_client_t, oldviewangles, SD_FIELD_TYPE_VECTOR3, 1 ),
	SAVE_DESCRIPTOR_DEFINE_FIELD_ARRAY( svg_client_t, oldvelocity, SD_FIELD_TYPE_VECTOR3, 1 ),

	SAVE_DESCRIPTOR_DEFINE_FIELD( svg_client_t, oldgroundentity, SD_FIELD_TYPE_EDICT ),

	SAVE_DESCRIPTOR_DEFINE_FIELD( svg_client_t, old_waterlevel, SD_FIELD_TYPE_INT32 ),
	SAVE_DESCRIPTOR_DEFINE_FIELD( svg_client_t, next_drown_time, SD_FIELD_TYPE_INT64 ),
	SAVE_DESCRIPTOR_DEFINE_FIELD( svg_client_t, pickup_msg_time, SD_FIELD_TYPE_INT64 ),
SAVE_DESCRIPTOR_FIELDS_END();