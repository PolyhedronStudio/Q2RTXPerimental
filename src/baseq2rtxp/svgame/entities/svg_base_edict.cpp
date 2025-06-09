/********************************************************************
*
*
*	SVGame: Edicts Functionalities:
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/svg_game_items.h"

#include "sharedgame/sg_means_of_death.h"

// Needed.
#include "svgame/entities/svg_base_edict.h"


/**
*   @brief  Save descriptor array definition for all the members of svg_base_edict_t.
**/
SAVE_DESCRIPTOR_FIELDS_BEGIN( svg_base_edict_t )
    /**
    *   Server Edict Entity State Data:
    **/
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, s.number, SD_FIELD_TYPE_INT32 ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, s.entityType, SD_FIELD_TYPE_INT32 ),

    SAVE_DESCRIPTOR_DEFINE_FIELD_ARRAY( svg_base_edict_t, s.origin, SD_FIELD_TYPE_VECTOR3, 1 ),
    SAVE_DESCRIPTOR_DEFINE_FIELD_ARRAY( svg_base_edict_t, s.angles, SD_FIELD_TYPE_VECTOR3, 1 ),
    SAVE_DESCRIPTOR_DEFINE_FIELD_ARRAY( svg_base_edict_t, s.old_origin, SD_FIELD_TYPE_VECTOR3, 1 ),

    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, s.solid, SD_FIELD_TYPE_INT32 ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, s.clipmask, SD_FIELD_TYPE_INT32 ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, s.hullContents, SD_FIELD_TYPE_INT32 ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, s.ownerNumber, SD_FIELD_TYPE_INT32 ),

    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, s.modelindex, SD_FIELD_TYPE_INT32 ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, s.modelindex2, SD_FIELD_TYPE_INT32 ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, s.modelindex3, SD_FIELD_TYPE_INT32 ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, s.modelindex4, SD_FIELD_TYPE_INT32 ),

    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, s.skinnum, SD_FIELD_TYPE_INT32 ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, s.effects, SD_FIELD_TYPE_INT32 ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, s.renderfx, SD_FIELD_TYPE_INT32 ),

    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, s.frame, SD_FIELD_TYPE_INT32 ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, s.old_frame, SD_FIELD_TYPE_INT32 ),

    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, s.sound, SD_FIELD_TYPE_INT32 ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, s.event, SD_FIELD_TYPE_INT32 ),

    SAVE_DESCRIPTOR_DEFINE_FIELD_ARRAY( svg_base_edict_t, s.spotlight.rgb, SD_FIELD_TYPE_VECTOR3, 1 ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, s.spotlight.intensity, SD_FIELD_TYPE_FLOAT ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, s.spotlight.angle_width, SD_FIELD_TYPE_FLOAT ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, s.spotlight.angle_falloff, SD_FIELD_TYPE_FLOAT ),

    /**
    *   Server Edict Data:
    **/
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, svflags, SD_FIELD_TYPE_INT32 ),
    SAVE_DESCRIPTOR_DEFINE_FIELD_ARRAY( svg_base_edict_t, mins, SD_FIELD_TYPE_VECTOR3, 1 ),
    SAVE_DESCRIPTOR_DEFINE_FIELD_ARRAY( svg_base_edict_t, maxs, SD_FIELD_TYPE_VECTOR3, 1 ),
    SAVE_DESCRIPTOR_DEFINE_FIELD_ARRAY( svg_base_edict_t, absmin, SD_FIELD_TYPE_VECTOR3, 1 ),
    SAVE_DESCRIPTOR_DEFINE_FIELD_ARRAY( svg_base_edict_t, absmax, SD_FIELD_TYPE_VECTOR3, 1 ),
    SAVE_DESCRIPTOR_DEFINE_FIELD_ARRAY( svg_base_edict_t, size, SD_FIELD_TYPE_VECTOR3, 1 ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, solid, SD_FIELD_TYPE_INT32 ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, clipmask, SD_FIELD_TYPE_INT32 ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, hullContents, SD_FIELD_TYPE_INT32 ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, owner, SD_FIELD_TYPE_EDICT ),

    /**
    *   Start of Game Edict data:
    **/
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, spawn_count, SD_FIELD_TYPE_INT32 ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, freetime, SD_FIELD_TYPE_INT64 ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, timestamp, SD_FIELD_TYPE_INT64 ),

    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, classname, SD_FIELD_TYPE_LEVEL_QSTRING ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, model, SD_FIELD_TYPE_LSTRING ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, angle, SD_FIELD_TYPE_FLOAT ),

    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, spawnflags, SD_FIELD_TYPE_INT32 ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, flags, SD_FIELD_TYPE_INT32 ),

    /**
    *   Health/Body Status Conditions:
    **/
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, health, SD_FIELD_TYPE_INT32 ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, max_health, SD_FIELD_TYPE_INT32 ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, gib_health, SD_FIELD_TYPE_INT32 ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, lifeStatus, SD_FIELD_TYPE_INT32 ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, takedamage, SD_FIELD_TYPE_INT32 ),

    /**
    *   UseTarget Properties and State:
    **/
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, useTarget.flags, SD_FIELD_TYPE_INT32 ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, useTarget.state, SD_FIELD_TYPE_INT32 ),

    /**
    *   Target Name Fields:
    **/
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, targetname, SD_FIELD_TYPE_LEVEL_QSTRING ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, targetNames.target, SD_FIELD_TYPE_LEVEL_QSTRING ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, targetNames.kill, SD_FIELD_TYPE_LEVEL_QSTRING ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, targetNames.team, SD_FIELD_TYPE_LEVEL_QSTRING ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, targetNames.path, SD_FIELD_TYPE_LEVEL_QSTRING ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, targetNames.death, SD_FIELD_TYPE_LEVEL_QSTRING ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, targetNames.movewith, SD_FIELD_TYPE_LEVEL_QSTRING ),

    /**
    *   Target Entities:
    **/
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, targetEntities.target, SD_FIELD_TYPE_EDICT ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, targetEntities.movewith, SD_FIELD_TYPE_EDICT ),

    /**
    *   Lua Properties:
    **/
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, luaProperties.luaName, SD_FIELD_TYPE_LEVEL_QSTRING ),

    /**
    *   "Delay" entities:
    **/
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, delayed.useTarget.creatorEntity, SD_FIELD_TYPE_EDICT ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, delayed.useTarget.useType, SD_FIELD_TYPE_INT32 ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, delayed.useTarget.useValue, SD_FIELD_TYPE_INT32 ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, delayed.signalOut.creatorEntity, SD_FIELD_TYPE_EDICT ),
    SAVE_DESCRIPTOR_DEFINE_FIELD_SIZE( svg_base_edict_t, delayed.signalOut.name, SD_FIELD_TYPE_ZSTRING, 256 ),

    /**
    *   Physics Related:
    **/
    SAVE_DESCRIPTOR_DEFINE_FIELD_ARRAY( svg_base_edict_t, moveWith.absoluteOrigin, SD_FIELD_TYPE_VECTOR3, 1 ),
    SAVE_DESCRIPTOR_DEFINE_FIELD_ARRAY( svg_base_edict_t, moveWith.originOffset, SD_FIELD_TYPE_VECTOR3, 1 ),
    SAVE_DESCRIPTOR_DEFINE_FIELD_ARRAY( svg_base_edict_t, moveWith.relativeDeltaOffset, SD_FIELD_TYPE_VECTOR3, 1 ),
    SAVE_DESCRIPTOR_DEFINE_FIELD_ARRAY( svg_base_edict_t, moveWith.spawnDeltaAngles, SD_FIELD_TYPE_VECTOR3, 1 ),
    SAVE_DESCRIPTOR_DEFINE_FIELD_ARRAY( svg_base_edict_t, moveWith.spawnParentAttachAngles, SD_FIELD_TYPE_VECTOR3, 1 ),
    SAVE_DESCRIPTOR_DEFINE_FIELD_ARRAY( svg_base_edict_t, moveWith.totalVelocity, SD_FIELD_TYPE_VECTOR3, 1 ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, moveWith.parentMoveEntity, SD_FIELD_TYPE_EDICT ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, moveWith.moveNextEntity, SD_FIELD_TYPE_EDICT ),

    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, movetype, SD_FIELD_TYPE_INT32 ),
    SAVE_DESCRIPTOR_DEFINE_FIELD_ARRAY( svg_base_edict_t, velocity, SD_FIELD_TYPE_VECTOR3, 1 ),
    SAVE_DESCRIPTOR_DEFINE_FIELD_ARRAY( svg_base_edict_t, avelocity, SD_FIELD_TYPE_VECTOR3, 1 ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, viewheight, SD_FIELD_TYPE_INT32 ),

    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, mass, SD_FIELD_TYPE_INT32 ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, gravity, SD_FIELD_TYPE_FLOAT ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, pausetime, SD_FIELD_TYPE_INT64 ),

    /**
    *   Pushers(MOVETYPE_PUSH/MOVETYPE_STOP) Physics:
    **/
    // Start/End Data:
    SAVE_DESCRIPTOR_DEFINE_FIELD_ARRAY( svg_base_edict_t, pushMoveInfo.startOrigin, SD_FIELD_TYPE_VECTOR3, 1 ),
    SAVE_DESCRIPTOR_DEFINE_FIELD_ARRAY( svg_base_edict_t, pushMoveInfo.startAngles, SD_FIELD_TYPE_VECTOR3, 1 ),
    SAVE_DESCRIPTOR_DEFINE_FIELD_ARRAY( svg_base_edict_t, pushMoveInfo.endOrigin, SD_FIELD_TYPE_VECTOR3, 1 ),
    SAVE_DESCRIPTOR_DEFINE_FIELD_ARRAY( svg_base_edict_t, pushMoveInfo.endAngles, SD_FIELD_TYPE_VECTOR3, 1 ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, pushMoveInfo.startFrame, SD_FIELD_TYPE_INT32 ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, pushMoveInfo.endFrame, SD_FIELD_TYPE_INT32 ),
    // Dynamic State Data:
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, pushMoveInfo.state, SD_FIELD_TYPE_INT32 ),
    SAVE_DESCRIPTOR_DEFINE_FIELD_ARRAY( svg_base_edict_t, pushMoveInfo.dir, SD_FIELD_TYPE_VECTOR3, 1 ),
    SAVE_DESCRIPTOR_DEFINE_FIELD_ARRAY( svg_base_edict_t, pushMoveInfo.dest, SD_FIELD_TYPE_VECTOR3, 1 ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, pushMoveInfo.in_motion, SD_FIELD_TYPE_BOOL ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, pushMoveInfo.current_speed, SD_FIELD_TYPE_DOUBLE ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, pushMoveInfo.move_speed, SD_FIELD_TYPE_DOUBLE ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, pushMoveInfo.next_speed, SD_FIELD_TYPE_DOUBLE ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, pushMoveInfo.remaining_distance, SD_FIELD_TYPE_DOUBLE ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, pushMoveInfo.decel_distance, SD_FIELD_TYPE_DOUBLE ),
    // Acceleration Data:
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, pushMoveInfo.accel, SD_FIELD_TYPE_FLOAT ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, pushMoveInfo.speed, SD_FIELD_TYPE_FLOAT ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, pushMoveInfo.decel, SD_FIELD_TYPE_FLOAT ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, pushMoveInfo.distance, SD_FIELD_TYPE_FLOAT ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, pushMoveInfo.wait, SD_FIELD_TYPE_FLOAT ),
    // Curve:
    SAVE_DESCRIPTOR_DEFINE_FIELD_ARRAY( svg_base_edict_t, pushMoveInfo.curve.referenceOrigin, SD_FIELD_TYPE_VECTOR3, 1 ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, pushMoveInfo.curve.positions, SD_FIELD_TYPE_LEVEL_QTAG_MEMORY ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, pushMoveInfo.curve.frame, SD_FIELD_TYPE_INT64 ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, pushMoveInfo.curve.subFrame, SD_FIELD_TYPE_INT64 ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, pushMoveInfo.curve.numberSubFrames, SD_FIELD_TYPE_INT64 ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, pushMoveInfo.curve.numberFramesDone, SD_FIELD_TYPE_INT64 ),
    // LockState:
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, pushMoveInfo.lockState.isLocked, SD_FIELD_TYPE_BOOL ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, pushMoveInfo.lockState.lockedSound, SD_FIELD_TYPE_INT32 ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, pushMoveInfo.lockState.lockingSound, SD_FIELD_TYPE_INT32 ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, pushMoveInfo.lockState.unlockingSound, SD_FIELD_TYPE_INT32 ),
    // Sounds:
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, pushMoveInfo.sounds.start, SD_FIELD_TYPE_INT32 ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, pushMoveInfo.sounds.middle, SD_FIELD_TYPE_INT32 ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, pushMoveInfo.sounds.end, SD_FIELD_TYPE_INT32 ),
    // Callback:
    SAVE_DESCRIPTOR_DEFINE_FUNCPTR( svg_base_edict_t, pushMoveInfo.endMoveCallback, SD_FIELD_TYPE_FUNCTION, FPTR_SAVE_TYPE_PUSHER_MOVEINFO_ENDMOVECALLBACK ),
    // Movewith:
    SAVE_DESCRIPTOR_DEFINE_FIELD_ARRAY( svg_base_edict_t, pushMoveInfo.lastVelocity, SD_FIELD_TYPE_VECTOR3, 1 ),
    
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, lip, SD_FIELD_TYPE_FLOAT ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, distance, SD_FIELD_TYPE_FLOAT ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, height, SD_FIELD_TYPE_FLOAT ),
    
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, speed, SD_FIELD_TYPE_FLOAT ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, accel, SD_FIELD_TYPE_FLOAT ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, decel, SD_FIELD_TYPE_FLOAT ),
    SAVE_DESCRIPTOR_DEFINE_FIELD_ARRAY( svg_base_edict_t, gravityVector, SD_FIELD_TYPE_VECTOR3, 1 ),

    // WID: Are these actually needed? Would they not be recalculated the first frame around?
    // WID: TODO: PushmoveInfo
    SAVE_DESCRIPTOR_DEFINE_FIELD_ARRAY( svg_base_edict_t, movedir, SD_FIELD_TYPE_VECTOR3, 1 ),
    SAVE_DESCRIPTOR_DEFINE_FIELD_ARRAY( svg_base_edict_t, pos1, SD_FIELD_TYPE_VECTOR3, 1 ),
    SAVE_DESCRIPTOR_DEFINE_FIELD_ARRAY( svg_base_edict_t, angles1, SD_FIELD_TYPE_VECTOR3, 1 ),
    SAVE_DESCRIPTOR_DEFINE_FIELD_ARRAY( svg_base_edict_t, pos2, SD_FIELD_TYPE_VECTOR3, 1 ),
    SAVE_DESCRIPTOR_DEFINE_FIELD_ARRAY( svg_base_edict_t, angles2, SD_FIELD_TYPE_VECTOR3, 1 ),
    SAVE_DESCRIPTOR_DEFINE_FIELD_ARRAY( svg_base_edict_t, lastOrigin, SD_FIELD_TYPE_VECTOR3, 1 ),
    SAVE_DESCRIPTOR_DEFINE_FIELD_ARRAY( svg_base_edict_t, lastAngles, SD_FIELD_TYPE_VECTOR3, 1 ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, movetarget, SD_FIELD_TYPE_EDICT ),

    /**
    *   NextThink AND Entity Callbacks:
    **/
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, nextthink, SD_FIELD_TYPE_INT64 ),
	
    SAVE_DESCRIPTOR_DEFINE_FUNCPTR( svg_base_edict_t, spawnCallbackFuncPtr, SD_FIELD_TYPE_FUNCTION, FPTR_SAVE_TYPE_SPAWN ),
    SAVE_DESCRIPTOR_DEFINE_FUNCPTR( svg_base_edict_t, postSpawnCallbackFuncPtr, SD_FIELD_TYPE_FUNCTION, FPTR_SAVE_TYPE_POSTSPAWN ),
    SAVE_DESCRIPTOR_DEFINE_FUNCPTR( svg_base_edict_t, preThinkCallbackFuncPtr, SD_FIELD_TYPE_FUNCTION, FPTR_SAVE_TYPE_PRETHINK ),
    SAVE_DESCRIPTOR_DEFINE_FUNCPTR( svg_base_edict_t, thinkCallbackFuncPtr, SD_FIELD_TYPE_FUNCTION, FPTR_SAVE_TYPE_THINK ),
    SAVE_DESCRIPTOR_DEFINE_FUNCPTR( svg_base_edict_t, postThinkCallbackFuncPtr, SD_FIELD_TYPE_FUNCTION, FPTR_SAVE_TYPE_POSTTHINK ),
    SAVE_DESCRIPTOR_DEFINE_FUNCPTR( svg_base_edict_t, blockedCallbackFuncPtr, SD_FIELD_TYPE_FUNCTION, FPTR_SAVE_TYPE_BLOCKED ),
    SAVE_DESCRIPTOR_DEFINE_FUNCPTR( svg_base_edict_t, touchCallbackFuncPtr, SD_FIELD_TYPE_FUNCTION, FPTR_SAVE_TYPE_TOUCH ),
    SAVE_DESCRIPTOR_DEFINE_FUNCPTR( svg_base_edict_t, useCallbackFuncPtr, SD_FIELD_TYPE_FUNCTION, FPTR_SAVE_TYPE_USE ),
    SAVE_DESCRIPTOR_DEFINE_FUNCPTR( svg_base_edict_t, onSignalInCallbackFuncPtr, SD_FIELD_TYPE_FUNCTION, FPTR_SAVE_TYPE_ONSIGNALIN ),
    SAVE_DESCRIPTOR_DEFINE_FUNCPTR( svg_base_edict_t, painCallbackFuncPtr, SD_FIELD_TYPE_FUNCTION, FPTR_SAVE_TYPE_PAIN ),
    SAVE_DESCRIPTOR_DEFINE_FUNCPTR( svg_base_edict_t, dieCallbackFuncPtr, SD_FIELD_TYPE_FUNCTION, FPTR_SAVE_TYPE_DIE ),
    // TODO:
    //SAVE_DESCRIPTOR_DEFINE_CALLBACK_PTR( svg_base_edict_t, think, SD_FIELD_TYPE_FUNCPTR, P_think ),

    /**
    *   Entity Pointers:
    **/
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, enemy, SD_FIELD_TYPE_EDICT ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, oldenemy, SD_FIELD_TYPE_EDICT ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, goalentity, SD_FIELD_TYPE_EDICT ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, chain, SD_FIELD_TYPE_EDICT ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, teamchain, SD_FIELD_TYPE_EDICT ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, teammaster, SD_FIELD_TYPE_EDICT ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, activator, SD_FIELD_TYPE_EDICT ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, other, SD_FIELD_TYPE_EDICT ),

    /**
    *   Light Data:
    **/
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, style, SD_FIELD_TYPE_INT32 ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, customLightStyle, SD_FIELD_TYPE_LSTRING ),

    /**
    *   Item Data:
    **/
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, item, SD_FIELD_TYPE_ITEM ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, itemName, SD_FIELD_TYPE_LEVEL_QSTRING ),

    /**
    *   Monster Data:
    **/
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, yaw_speed, SD_FIELD_TYPE_FLOAT ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, ideal_yaw, SD_FIELD_TYPE_FLOAT ),

    /**
    *   Player Noise/Trail:
    **/
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, mynoise, SD_FIELD_TYPE_EDICT ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, mynoise2, SD_FIELD_TYPE_EDICT ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, noise_index, SD_FIELD_TYPE_INT32 ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, noise_index2, SD_FIELD_TYPE_INT32 ),
    /**
    *   Sound Data:
    **/
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, volume, SD_FIELD_TYPE_FLOAT ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, attenuation, SD_FIELD_TYPE_FLOAT ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, last_sound_time, SD_FIELD_TYPE_INT64 ),
    /**
    *   Trigger(s) Data:
    **/
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, noisePath, SD_FIELD_TYPE_LEVEL_QSTRING ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, message, SD_FIELD_TYPE_LSTRING ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, wait, SD_FIELD_TYPE_FLOAT ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, delay, SD_FIELD_TYPE_FLOAT ),
    #undef random
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, random, SD_FIELD_TYPE_FLOAT ),
    /**
    *   Timers Data:
    **/
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, air_finished_time, SD_FIELD_TYPE_INT64 ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, damage_debounce_time, SD_FIELD_TYPE_INT64 ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, fly_sound_debounce_time, SD_FIELD_TYPE_INT64 ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, last_move_time, SD_FIELD_TYPE_INT64 ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, touch_debounce_time, SD_FIELD_TYPE_INT64 ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, pain_debounce_time, SD_FIELD_TYPE_INT64 ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, show_hostile_time, SD_FIELD_TYPE_INT64 ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, death_time, SD_FIELD_TYPE_INT64 ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, trail_time, SD_FIELD_TYPE_INT64 ),

    /**
    *   Various Data:
    **/
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, meansOfDeath, SD_FIELD_TYPE_INT32 ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, map, SD_FIELD_TYPE_LSTRING ),

    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, dmg, SD_FIELD_TYPE_INT32 ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, radius_dmg, SD_FIELD_TYPE_INT32 ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, dmg_radius, SD_FIELD_TYPE_FLOAT ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, light, SD_FIELD_TYPE_FLOAT ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, sounds, SD_FIELD_TYPE_INT32 ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_base_edict_t, count, SD_FIELD_TYPE_INT32 ),

    /**
    *   Only used for g_turret.cpp - WID: Remove?:
    **/
    SAVE_DESCRIPTOR_DEFINE_FIELD_ARRAY( svg_base_edict_t, move_origin, SD_FIELD_TYPE_VECTOR3, 1 ),
    SAVE_DESCRIPTOR_DEFINE_FIELD_ARRAY( svg_base_edict_t, move_angles, SD_FIELD_TYPE_VECTOR3, 1 ),
SAVE_DESCRIPTOR_FIELDS_END();


/**
*
*
*
*   Save Game Mechanism:
*
*
*
**/
/**
*   @brief Retrieves a pointer to the save descriptor fields.
*   @return A pointer to a structure of type `svg_save_descriptor_field_t` representing the save descriptor fields.
**/
svg_save_descriptor_field_t *svg_base_edict_t::GetSaveDescriptorFields() {
    return svg_base_edict_t::saveDescriptorFields;
}
/**
*   @return The number of save descriptor fields.
**/
int32_t svg_base_edict_t::GetSaveDescriptorFieldsCount() {
    return sizeof( svg_base_edict_t::saveDescriptorFields ) / sizeof( svg_base_edict_t::saveDescriptorFields[ 0 ] );
}
/**
*   @return A pointer to the save descriptor field with the given name.
**/
svg_save_descriptor_field_t *svg_base_edict_t::GetSaveDescriptorField( const char *name ) {
    // Check if the name is valid.
    if ( !name ) {
        return nullptr;
    }
    // Check if the name is empty.
    if ( *name == '\0' ) {
        return nullptr;
    }

    #if 0 // We don't want to lock us in recursion!
    // Check if parent type has a save descriptor field.
    if ( svg_base_edict_t::GetSaveDescriptorField( name ) ) {
        return svg_base_edict_t::GetSaveDescriptorField( name );
    }
    #endif // #if 0

    // Get pointer to the type's save descriptor fields.
    svg_save_descriptor_field_t *fields = GetSaveDescriptorFields();
    if ( !fields ) {
        // TODO: Warn?
        return nullptr;
    }

    // Iterate this edict (derived-) type's save descriptor fields and return a pointer to it if found.
    for ( int32_t i = 0; i < GetSaveDescriptorFieldsCount(); i++ ) {
        if ( strcmp( name, fields[ i ].name ) == 0 ) {
            return &svg_base_edict_t::saveDescriptorFields[ i ];
        }
    }

    // Unable to find.
    return nullptr;
}



/**
*
*
*   Callback Dispatching:
*
*
**/
/**
*   @brief  Calls the 'spawn' callback that is configured for this entity.
**/
void svg_base_edict_t::DispatchSpawnCallback() {
    if ( spawnCallbackFuncPtr ) {
        spawnCallbackFuncPtr( this );
    }
}
/**
*   @brief  Calls the 'postspawn' callback that is configured for this entity.
**/
void svg_base_edict_t::DispatchPostSpawnCallback() {
    if ( postSpawnCallbackFuncPtr ) {
        postSpawnCallbackFuncPtr( this );
    }
}

/**
*   @brief  Calls the 'prethink' callback that is configured for this entity.
**/
void svg_base_edict_t::DispatchPreThinkCallback() {
    if ( preThinkCallbackFuncPtr ) {
        preThinkCallbackFuncPtr( this );
    }
}
/**
*   @brief  Calls the 'think' callback that is configured for this entity.
**/
void svg_base_edict_t::DispatchThinkCallback() {
    if ( thinkCallbackFuncPtr ) {
        thinkCallbackFuncPtr( this );
    }
}
/**
*   @brief  Calls the 'postthink' callback that is configured for this entity.
**/
void svg_base_edict_t::DispatchPostThinkCallback() {
    if ( postThinkCallbackFuncPtr ) {
        postThinkCallbackFuncPtr( this );
    }
}
/**
*   @brief  Calls the 'blocked' callback that is configured for this entity.
**/
void svg_base_edict_t::DispatchBlockedCallback( svg_base_edict_t *other ) {
    if ( blockedCallbackFuncPtr ) {
        blockedCallbackFuncPtr( this, other );
    }
}
/**
*   @brief  Calls the 'touch' callback that is configured for this entity.
**/
void svg_base_edict_t::DispatchTouchCallback( svg_base_edict_t *other, const cm_plane_t *plane, cm_surface_t *surf ) {
    if ( touchCallbackFuncPtr ) {
        touchCallbackFuncPtr( this, other, plane, surf );
    }
}
/**
*   @brief  Calls the 'use' callback that is configured for this entity.
**/
void svg_base_edict_t::DispatchUseCallback( svg_base_edict_t *other, svg_base_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue ) {
    if ( useCallbackFuncPtr ) {
        useCallbackFuncPtr( this, other, activator, useType, useValue );
    }
}
/**
*   @brief  Calls the 'onsignalin' callback that is configured for this entity.
**/
void svg_base_edict_t::DispatchOnSignalInCallback( svg_base_edict_t *other, svg_base_edict_t *activator, const char *signalName, const svg_signal_argument_array_t &signalArguments ) {
    if ( onSignalInCallbackFuncPtr ) {
        onSignalInCallbackFuncPtr( this, other, activator, signalName, signalArguments );
    }
}
/**
*   @brief  Calls the 'pain' callback that is configured for this entity.
**/
void svg_base_edict_t::DispatchPainCallback( svg_base_edict_t *other, const float kick, const int32_t damage ) {
    if ( painCallbackFuncPtr != nullptr ) {
        painCallbackFuncPtr( this, other, kick, damage );
    }
}
/**
*   @brief  Calls the 'die' callback that is configured for this entity.
**/
void svg_base_edict_t::DispatchDieCallback( svg_base_edict_t *inflictor, svg_base_edict_t *attacker, const int32_t damage, vec_t *point ) {
    if ( dieCallbackFuncPtr != nullptr ) {
        dieCallbackFuncPtr( this, inflictor, attacker, damage, point );
    }
}



/**
*
*
*   Core:
*
*
**/
/**
*   Reconstructs the object, optionally retaining the entityDictionary.
**/
void svg_base_edict_t::Reset( const bool retainDictionary ) {
    // Call upon the base class.
    sv_shared_edict_t<svg_base_edict_t, svg_client_t>::Reset( retainDictionary );

    // Reset all member variables to defaults. (Avoid using memset because it corrupts vtable.)
    spawn_count = 0;
    freetime = 0_ms;
    timestamp = 0_ms;
    classname = svg_level_qstring_t::from_char_str( "svg_base_edict_t" );
    model = nullptr;
    angle = 0.0f;
    spawnflags = 0;
    flags = entity_flags_t::FL_NONE;
    health = 0;
    max_health = 0;
    gib_health = 0;
    lifeStatus = entity_lifestatus_t::LIFESTATUS_ALIVE;
    takedamage = entity_takedamage_t::DAMAGE_NO;
    useTarget = {};
    targetname = {};
    targetNames = {};
    targetEntities = {};
    delayed.signalOut.arguments.clear();
    delayed = {};
    moveWith = {};
    movetype = 0;
    velocity = QM_Vector3Zero();
    avelocity = QM_Vector3Zero();
    viewheight = 0;
    liquidInfo = {};
    groundInfo = {};
    gravityVector = { 0.f, 0.f, -1.f };
    mass = 0;
    gravity = 0.f;
    pushMoveInfo = {};
    speed = 0.f;
    accel = 0.f;
    decel = 0.f;
    distance = 0.f;
    lip = 0.f;
    height = 0.f;
    movedir = QM_Vector3Zero();
    pos1 = QM_Vector3Zero();
    angles1 = QM_Vector3Zero();
    pos2 = QM_Vector3Zero();
    angles2 = QM_Vector3Zero();
    lastOrigin = QM_Vector3Zero();
    lastAngles = QM_Vector3Zero();
    movetarget = nullptr;

    nextthink = 0_ms;
    postSpawnCallbackFuncPtr = nullptr;
    preThinkCallbackFuncPtr = nullptr;
    thinkCallbackFuncPtr = nullptr;
    postThinkCallbackFuncPtr = nullptr;
    blockedCallbackFuncPtr = nullptr;
    touchCallbackFuncPtr = nullptr;
    useCallbackFuncPtr = nullptr;
    onSignalInCallbackFuncPtr = nullptr;
    painCallbackFuncPtr = nullptr;
    dieCallbackFuncPtr = nullptr;

    enemy = nullptr;
    oldenemy = nullptr;
    goalentity = nullptr;

    chain = nullptr;
    teamchain = nullptr;
    teammaster = nullptr;

    activator = nullptr;
    other = nullptr;

    style = 0;

    customLightStyle = nullptr;

    item = nullptr;
    itemName = nullptr;

    yaw_speed = 0.f;
    ideal_yaw = 0.f;
    noisePath = nullptr;
    mynoise = nullptr;
    mynoise2 = nullptr;
    noise_index = 0;
    noise_index2 = 0;

    volume = 0.0f;
    attenuation = 0.0f;
    last_sound_time = 0_ms;

    message = nullptr;
    wait = 0.f;
    delay = 0.f;
    random = 0.f;

    air_finished_time = 0_ms;
    damage_debounce_time = 0_ms;
    fly_sound_debounce_time = 0_ms;
    last_move_time = 0_ms;
    touch_debounce_time = 0_ms;
    pain_debounce_time = 0_ms;
    show_hostile_time = 0_ms;
    trail_time = 0_ms;

    meansOfDeath = sg_means_of_death_e::MEANS_OF_DEATH_UNKNOWN;
    map = nullptr;

    dmg = 0;
    radius_dmg = 0;
    light = 0.f;
    sounds = 0;
    count = 0;

    move_origin = QM_Vector3Zero();
    move_angles = QM_Vector3Zero();
}
/**
*   @brief  Used for savegaming the entity. Each derived entity type
*           that needs to be saved should implement this function.
*
*   @note   Make sure to call the base parent class' Save() function.
**/
void svg_base_edict_t::Save( struct game_write_context_t *ctx ) {
	// Call upon the base class.
	//sv_shared_edict_t<svg_base_edict_t, svg_client_t>::Save( ctx );
	// Save all the members of this entity type.
	ctx->write_fields( svg_base_edict_t::saveDescriptorFields, this );
}
/**
*   @brief  Used for loadgaming the entity. Each derived entity type
*           that needs to be loaded should implement this function.
*
*   @note   Make sure to call the base parent class' Restore() function.
**/
void svg_base_edict_t::Restore( struct game_read_context_t *ctx ) {
    // Call upon the base class.
	//sv_shared_edict_t<svg_base_edict_t, svg_client_t>::Save( ctx );
    // Read all the members of this entity type.
    ctx->read_fields( svg_base_edict_t::saveDescriptorFields, this );
}

/**
*   @brief  Called for each cm_entity_t key/value pair for this entity.
*           If not handled, or unable to be handled by the derived entity type, it will return
*           set errorStr and return false. True otherwise.
**/
const bool svg_base_edict_t::KeyValue( const cm_entity_t *keyValuePair, std::string &errorStr ) {
    // Ease of use.
    const std::string keyStr = keyValuePair->key;

    // Match: classname
    if ( keyStr == "classname" && keyValuePair->parsed_type & cm_entity_parsed_type_t::ENTITY_PARSED_TYPE_STRING ) {
        // It is already set before during construction of this entity.
        // We return true however since we don't want it complaining.
        classname = svg_level_qstring_t::from_char_str( keyValuePair->string );
        return true;
    }
    // Match: model
    else if ( keyStr == "model" && keyValuePair->parsed_type & cm_entity_parsed_type_t::ENTITY_PARSED_TYPE_STRING ) {
        model = ED_NewString( keyValuePair->string );
        return true;
    }
    // Match: spawnflags
    else if ( keyStr == "spawnflags" && keyValuePair->parsed_type & cm_entity_parsed_type_t::ENTITY_PARSED_TYPE_INTEGER ) {
        spawnflags = keyValuePair->integer;
        return true;
    }
    // Match: speed
    else if ( keyStr == "speed" && keyValuePair->parsed_type & cm_entity_parsed_type_t::ENTITY_PARSED_TYPE_FLOAT ) {
        speed = keyValuePair->value;
        return true;
    }
    // Match: accel
    else if ( keyStr == "accel" && keyValuePair->parsed_type & cm_entity_parsed_type_t::ENTITY_PARSED_TYPE_FLOAT ) {
        accel = keyValuePair->value;
        return true;
    }
    // Match: speed
    else if ( keyStr == "decel" && keyValuePair->parsed_type & cm_entity_parsed_type_t::ENTITY_PARSED_TYPE_FLOAT ) {
        decel = keyValuePair->value;
        return true;
    }
    // Match: target
    else if ( keyStr == "target" && keyValuePair->parsed_type & cm_entity_parsed_type_t::ENTITY_PARSED_TYPE_STRING ) {
        targetNames.target = svg_level_qstring_t::from_char_str( keyValuePair->string );
        return true;
    }
    // Match: targetname
    else if ( keyStr == "targetname" && keyValuePair->parsed_type & cm_entity_parsed_type_t::ENTITY_PARSED_TYPE_STRING ) {
        targetname = svg_level_qstring_t::from_char_str( keyValuePair->string );
        return true;
    }
    // Match: path
    else if ( keyStr == "pathtarget" && keyValuePair->parsed_type & cm_entity_parsed_type_t::ENTITY_PARSED_TYPE_STRING ) {
        targetNames.path = svg_level_qstring_t::from_char_str( keyValuePair->string );
        return true;
    }
    //// Match: death
    else if ( keyStr == "deathtarget" && keyValuePair->parsed_type & cm_entity_parsed_type_t::ENTITY_PARSED_TYPE_STRING ) {
        targetNames.target = svg_level_qstring_t::from_char_str( keyValuePair->string );
        return true;
    }
    // Match: kill
    else if ( keyStr == "killtarget" && keyValuePair->parsed_type & cm_entity_parsed_type_t::ENTITY_PARSED_TYPE_STRING ) {
        targetNames.kill = svg_level_qstring_t::from_char_str( keyValuePair->string );
        return true;
    }
    // Match: message
    else if ( keyStr == "message" && keyValuePair->parsed_type & cm_entity_parsed_type_t::ENTITY_PARSED_TYPE_STRING ) {
        message = ED_NewString( keyValuePair->string );
        return true;
    }
    // Match: team
    else if ( keyStr == "team" && keyValuePair->parsed_type & cm_entity_parsed_type_t::ENTITY_PARSED_TYPE_STRING ) {
        targetNames.team = svg_level_qstring_t::from_char_str( keyValuePair->string );
        return true;
    }
    // Match: luaName
    else if ( keyStr == "luaName" && keyValuePair->parsed_type & cm_entity_parsed_type_t::ENTITY_PARSED_TYPE_STRING ) {
        // It is already set before during construction of this entity.
        // We return true however since we don't want it complaining.
        luaProperties.luaName = svg_level_qstring_t::from_char_str( keyValuePair->string );
        return true;
    }
    // Match: wait
    else if ( keyStr == "wait" && keyValuePair->parsed_type & cm_entity_parsed_type_t::ENTITY_PARSED_TYPE_FLOAT ) {
        wait = keyValuePair->value;
        return true;
    }
    // Match: delay
    else if ( keyStr == "delay" && keyValuePair->parsed_type & cm_entity_parsed_type_t::ENTITY_PARSED_TYPE_FLOAT ) {
        delay = keyValuePair->value;
        return true;
    }
    // Match: random
    else if ( keyStr == "random" && keyValuePair->parsed_type & cm_entity_parsed_type_t::ENTITY_PARSED_TYPE_FLOAT ) {
        random = keyValuePair->value;
        return true;
    }
    // Match: movewith
    else if ( keyStr == "movewith" && keyValuePair->parsed_type & cm_entity_parsed_type_t::ENTITY_PARSED_TYPE_STRING ) {
        targetNames.movewith = svg_level_qstring_t::from_char_str( keyValuePair->string );
        return true;
    // Match: move_origin
    } else if ( keyStr == "move_origin" && keyValuePair->parsed_type & cm_entity_parsed_type_t::ENTITY_PARSED_TYPE_VECTOR3 ) {
        move_origin = keyValuePair->vec3;
        return true;
    }
    // Match: move_angles
    else if ( keyStr == "move_angles" && keyValuePair->parsed_type & cm_entity_parsed_type_t::ENTITY_PARSED_TYPE_VECTOR3 ) {
        move_angles = keyValuePair->vec3;
        return true;
    }
    // Match: style
    else if ( keyStr == "style" && keyValuePair->parsed_type & cm_entity_parsed_type_t::ENTITY_PARSED_TYPE_INTEGER ) {
        style = keyValuePair->integer;
        return true;
    }
    // Match: count
    else if ( keyStr == "count" && keyValuePair->parsed_type & cm_entity_parsed_type_t::ENTITY_PARSED_TYPE_INTEGER ) {
        count = keyValuePair->integer;
        return true;
    }
    // Match: maxhealth
    else if ( keyStr == "maxhealth" && keyValuePair->parsed_type & cm_entity_parsed_type_t::ENTITY_PARSED_TYPE_INTEGER ) {
        max_health = keyValuePair->integer;
        return true;
    }
    // Match: health
    else if ( keyStr == "health" && keyValuePair->parsed_type & cm_entity_parsed_type_t::ENTITY_PARSED_TYPE_INTEGER ) {
        health = keyValuePair->integer;
        return true;
    }
    // Match: sounds
    else if ( keyStr == "sounds" && keyValuePair->parsed_type & cm_entity_parsed_type_t::ENTITY_PARSED_TYPE_INTEGER ) {
        sounds = keyValuePair->integer;
        return true;
    }
    // Match: light
    else if ( keyStr == "light" && keyValuePair->parsed_type & cm_entity_parsed_type_t::ENTITY_PARSED_TYPE_FLOAT ) {
        light = keyValuePair->value;
        return true;
    }
    // Match: dmg
    else if ( keyStr == "dmg" && keyValuePair->parsed_type & cm_entity_parsed_type_t::ENTITY_PARSED_TYPE_INTEGER ) {
        dmg = keyValuePair->integer;
        return true;
    }
    // Match: mass
    else if ( keyStr == "mass" && keyValuePair->parsed_type & cm_entity_parsed_type_t::ENTITY_PARSED_TYPE_INTEGER ) {
        mass = keyValuePair->integer;
        return true;
    }
    // Match: gravity
    else if ( keyStr == "gravity" && keyValuePair->parsed_type & cm_entity_parsed_type_t::ENTITY_PARSED_TYPE_FLOAT ) {
        gravity = keyValuePair->value;
        return true;
    }
    // Match: volume
    else if ( keyStr == "volume" && keyValuePair->parsed_type & cm_entity_parsed_type_t::ENTITY_PARSED_TYPE_FLOAT ) {
        volume = keyValuePair->value;
        return true;
    }
    // Match: attenuation
    else if ( keyStr == "attenuation" && keyValuePair->parsed_type & cm_entity_parsed_type_t::ENTITY_PARSED_TYPE_FLOAT ) {
        attenuation = keyValuePair->value;
        return true;
    }
    // Match: map
    else if ( keyStr == "map" && keyValuePair->parsed_type & cm_entity_parsed_type_t::ENTITY_PARSED_TYPE_STRING ) {
        map = ED_NewString( keyValuePair->string );
        return true;
    }
    // Match: customLightStyle
    else if ( keyStr == "customLightStyle" && keyValuePair->parsed_type & cm_entity_parsed_type_t::ENTITY_PARSED_TYPE_STRING ) {
        customLightStyle = ED_NewString( keyValuePair->string );
        return true;
    }
    // Match: origin
    else if ( keyStr == "origin" && keyValuePair->parsed_type & cm_entity_parsed_type_t::ENTITY_PARSED_TYPE_VECTOR3 ) {
        VectorCopy( keyValuePair->vec3, s.origin );
        return true;
    }
    // Match: angles
    else if ( keyStr == "angles" && keyValuePair->parsed_type & cm_entity_parsed_type_t::ENTITY_PARSED_TYPE_VECTOR3 ) {
        VectorCopy( keyValuePair->vec3, s.angles );
        return true;
    }
    // Match: attenuation
    else if ( keyStr == "angle" && keyValuePair->parsed_type & cm_entity_parsed_type_t::ENTITY_PARSED_TYPE_FLOAT ) {
        s.angles[ 0 ] = 0.f;
        s.angles[ 1 ] = keyValuePair->value;
        s.angles[ 2 ] = 0.f;
        return true;
    }
    // Match: lip
    else if ( keyStr == "lip" && keyValuePair->parsed_type & cm_entity_parsed_type_t::ENTITY_PARSED_TYPE_INTEGER ) {
        lip = keyValuePair->integer;
        return true;
    }
    // Match: distance
    else if ( keyStr == "distance" && keyValuePair->parsed_type & cm_entity_parsed_type_t::ENTITY_PARSED_TYPE_INTEGER ) {
        distance = keyValuePair->integer;
        return true;
    }
    // Match: height
    else if ( keyStr == "height" && keyValuePair->parsed_type & cm_entity_parsed_type_t::ENTITY_PARSED_TYPE_INTEGER ) {
        height = keyValuePair->integer;
        return true;
    }
    // Match: item
    else if ( keyStr == "item" && keyValuePair->parsed_type & cm_entity_parsed_type_t::ENTITY_PARSED_TYPE_STRING ) {
        itemName = svg_level_qstring_t::from_char_str( keyValuePair->string );
        return true;
    }
    // Match: item
    else if ( keyStr == "noise" && keyValuePair->parsed_type & cm_entity_parsed_type_t::ENTITY_PARSED_TYPE_STRING ) {
        noisePath = svg_level_qstring_t::from_char_str( keyValuePair->string );
        return true;
    }
    #if 1
    // Match: pausetime
    else if ( keyStr == "pausetime" && keyValuePair->parsed_type & cm_entity_parsed_type_t::ENTITY_PARSED_TYPE_FLOAT ) {
        pausetime = QMTime::FromSeconds( keyValuePair->value );
        return true;
    }
    #endif

    return false;
}


/**
*
*
*   Core:
*
*
**/
/**
*   @brief  This will check and set the itemptr according to one found in itemstr.
**/
//DEFINE_MEMBER_CALLBACK_SPAWN( svg_base_edict_t, onSpawn ) ( svg_base_edict_t *self ) -> void {
//
//}



/**
*
*   Callbacks(defaults):
*
**/
/**
*   @brief  This will check and set the itemptr according to one found in itemstr.
**/
DEFINE_MEMBER_CALLBACK_SPAWN( svg_base_edict_t, onSpawn )( svg_base_edict_t *self ) -> void {
    // Required for entities which might not directly link to world.
    // This'll position their bbox accordingly.
    VectorCopy( self->s.origin, self->absmin );
    VectorCopy( self->s.origin, self->absmax );
}
/**
*   @brief  PostSpawn Stub.
**/
DEFINE_MEMBER_CALLBACK_POSTSPAWN( svg_base_edict_t, onPostSpawn )( svg_base_edict_t *self ) -> void { }
/**
*   @brief  Thinking Stubs.
**/
DEFINE_MEMBER_CALLBACK_THINK( svg_base_edict_t, onThink )( svg_base_edict_t *self ) -> void { }
DEFINE_MEMBER_CALLBACK_PRETHINK( svg_base_edict_t, onPreThink )( svg_base_edict_t *self ) -> void { }
DEFINE_MEMBER_CALLBACK_POSTTHINK( svg_base_edict_t, onPostThink )( svg_base_edict_t *self ) -> void { }
/**
*   @brief  Blocked Stub.
**/
DEFINE_MEMBER_CALLBACK_BLOCKED( svg_base_edict_t, onBlocked )( svg_base_edict_t *self, svg_base_edict_t *other ) -> void { }
/**
*   @brief  Touched Stub.
**/
DEFINE_MEMBER_CALLBACK_TOUCH( svg_base_edict_t, onTouch )( svg_base_edict_t *self, svg_base_edict_t *other, const cm_plane_t *plane, cm_surface_t *surf ) -> void { }
/**
*   @brief  Use Stub.
**/
DEFINE_MEMBER_CALLBACK_USE( svg_base_edict_t, onUse )( svg_base_edict_t *self, svg_base_edict_t *other, svg_base_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue ) -> void { }
/**
*   @brief  Pain Stub.
**/
DEFINE_MEMBER_CALLBACK_PAIN( svg_base_edict_t, onPain )( svg_base_edict_t *self, svg_base_edict_t *other, const float kick, const int32_t damage ) -> void { }
/**
*   @brief  Die Stub.
**/
DEFINE_MEMBER_CALLBACK_DIE( svg_base_edict_t, onDie )( svg_base_edict_t *self, svg_base_edict_t *inflictor, svg_base_edict_t *attacker, int damage, vec3_t point ) -> void { }
/**
*   @brief  Signal Receiving Stub.
**/
DEFINE_MEMBER_CALLBACK_ON_SIGNALIN( svg_base_edict_t, onSignalIn )( svg_base_edict_t *self, svg_base_edict_t *other, svg_base_edict_t *activator, const char *signalName, const svg_signal_argument_array_t &signalArguments ) -> void { }