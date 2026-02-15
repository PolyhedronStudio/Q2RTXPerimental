/********************************************************************
*
*    ServerGame: TestDummy Debug Monster Edict (A* only) - Implementation
*
********************************************************************/

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

#include "svgame/entities/monster/svg_monster_testdummy_debug.h"
// TestDummy helper functions live in the puppet TU and are not visible here by default.
// Declare the helpers we use so this TU can call them without pulling in the entire puppet CPP.
void SVG_TestDummy_GetNavAgentBBox( const svg_monster_testdummy_t *ent, Vector3 *out_mins, Vector3 *out_maxs );
bool SVG_TestDummy_TryQueueNavRebuild( svg_monster_testdummy_t *self, const Vector3 &start_origin,
	const Vector3 &goal_origin, const svg_nav_path_policy_t &policy, const Vector3 &agent_mins,
	const Vector3 &agent_maxs, const bool force = false );
#include "svgame/nav/svg_nav_request.h"
#include "svgame/nav/svg_nav.h"


// Navigation cluster routing (coarse tile routing pre-pass).
#include "svgame/nav/svg_nav_clusters.h"
// Async navigation queue helpers.
#include "svgame/nav/svg_nav_request.h"
// Traversal helpers required for path invalidation.
#include "svgame/nav/svg_nav_traversal.h"

// (Prototypes already declared above.)

//! Optional debug toggle for emitting async queue statistics.
extern cvar_t *s_nav_nav_async_log_stats;

/**
*    @brief Spawn for debug testdummy: call base onSpawn then set think to our simple loop.
*/
DEFINE_MEMBER_CALLBACK_SPAWN( svg_monster_testdummy_debug_t, onSpawn )( svg_monster_testdummy_debug_t *self ) -> void {
    // Call base spawn to initialize model/physics/etc.
    svg_monster_testdummy_t::onSpawn( self );
    // Always run our debug thinker.
    self->SetThinkCallback( &svg_monster_testdummy_debug_t::onThink );
    self->nextthink = level.time + FRAME_TIME_MS;
}

/**
*    @brief Simple thinker: always attempt async A* to activator if present, otherwise idle.
*/
DEFINE_MEMBER_CALLBACK_THINK( svg_monster_testdummy_debug_t, onThink )( svg_monster_testdummy_debug_t *self ) -> void {
    // Basic grounding and animation like the base class.
    self->s.renderfx &= ~(RF_STAIR_STEP | RF_OLD_FRAME_LERP);
    const cm_contents_t mask = SVG_GetClipMask( self );
    M_CheckGround( self, mask );
    M_CatagorizePosition( self, self->currentOrigin, self->liquidInfo.level, self->liquidInfo.type );

    if ( self->health <= 0 || self->lifeStatus != LIFESTATUS_ALIVE ) {
        self->nextthink = level.time + FRAME_TIME_MS;
        return;
    }

	// If we have an activator, set it as goal and enqueue A*.
	if ( self->activator ) {
		self->goalentity = self->activator;
	}

	// A save descriptor test ,after all for now this is a test monster.
	self->testVar = level.frameNumber;

	/**
	*	If alive, perform AI thinking and movement.
	**/
	if ( self->health > 0 && self->lifeStatus == LIFESTATUS_ALIVE ) {
		Vector3 preSumOrigin = self->currentOrigin;
		Vector3 previousVelocity = self->velocity;

		/**
		*    Decide targeting: if an activator exists treat it as the active goal.
		*    This guarantees that we always attempt A* toward the activator even
		*    when it becomes temporarily occluded or stands on a brush. Do not
		*    clear the activator on interim failures so A* can be retried next frame.
		**/
		// Does this entity have an activator?
		const bool hasActivator = ( self->activator != nullptr );
		// Set goalentity to activator if present.
		if ( hasActivator ) {
			self->goalentity = self->activator;
		}

		/**
		*    If we have no goalentity (no activator set and no other goal), idle.
		**/
		if ( !self->goalentity ) {
			self->ThinkIdle();
		} else {
			/**
			*    Always attempt A* pathing toward the current goalentity. When the
			*    activator becomes temporarily visible, run LOS pursuit instead so we
			*    do not needlessly rebuild or race between A* and direct motion.
			**/
			/**
			*    Debug visibility flag:
			*        Use actual line-of-sight from monster to activator.
			**/
			const bool activatorVisible = ( self->activator && SVG_Entity_IsVisible( self, self->activator ) );
			// Hysteresis thresholds: avoid flip-flopping between LOS direct pursuit
			// and A* rebuilding when the player briefly peeks around a corner.
			// These can be tuned for responsiveness vs stability.
			constexpr int NAV_LOS_HYST_FRAMES = 6; // default chosen by dev

			// Update frame counters for LOS/hide durations.
			if ( activatorVisible ) {
				self->visible_los_frames++;
				self->hidden_los_frames = 0;
			} else {
				self->hidden_los_frames++;
				self->visible_los_frames = 0;
			}

			// Determine if we've stably been in one mode long enough to switch.
			const bool stable_visible = ( self->visible_los_frames >= NAV_LOS_HYST_FRAMES );
			const bool stable_hidden = ( self->hidden_los_frames >= NAV_LOS_HYST_FRAMES );

			bool pursuing = false;
			// If we've stably seen the activator, force direct pursuit.
			if ( stable_visible ) {
				self->last_pursuit_was_direct = true;
				self->ThinkDirectPursuit();
				pursuing = true;
			}
			// If we've stably been hidden long enough, prefer A*/trail logic.
			else if ( stable_hidden ) {
				self->last_pursuit_was_direct = false;
				pursuing = self->ThinkAStarToPlayer();
				if ( !pursuing ) {
					pursuing = self->ThinkFollowTrail();
					if ( !pursuing ) {
						self->velocity.x = 0.0f;
						self->velocity.y = 0.0f;
						self->ThinkIdle();
					}
				}
			}
			// If neither stable, stick to the last mode to avoid rapid toggles.
			else {
				if ( self->last_pursuit_was_direct ) {
					  /**
					  *    Hysteresis guard:
					  *        When LOS just dropped, prefer A* / trail so we avoid corner hugging
					  *        caused by continuing straight-line pursuit without visibility.
					  **/
					  // If LOS is currently blocked, attempt A*/trail instead of direct pursuit.
					if ( !activatorVisible ) {
						pursuing = self->ThinkAStarToPlayer();
						if ( !pursuing ) {
							pursuing = self->ThinkFollowTrail();
						}
					} else {
						self->ThinkDirectPursuit();
						pursuing = true;
					}
				} else {
					pursuing = self->ThinkAStarToPlayer();
					if ( !pursuing ) {
						pursuing = self->ThinkFollowTrail();
					}
				}
			}
		}

		/**
		*    Always refresh ground and liquid state before movement.
		*    groundInfo contains the previous frame's ground data.
		**/
		// Always refresh ground and liquid state.
		// Ground/liq state is still available in self->groundInfo;

		/**
		*    Prepare movement state for the monster move subsystem.
		*    This struct is consumed by SVG_MMove_StepSlideMove which performs
		*    collision, step, and slide handling for this frame.
		**/
		mm_move_t monsterMove = {
			.monster = self,
			.frameTime = gi.frame_time_s,
			.mins = self->mins,
			.maxs = self->maxs,
			.state = {
				.mm_type = MM_NORMAL,
				.mm_flags = ( self->groundInfo.entityNumber != ENTITYNUM_NONE ? MMF_ON_GROUND : 0 ),
				.mm_time = 0,
				.gravity = ( int16_t )( self->gravity * sv_gravity->value ),
				.origin = self->currentOrigin,
				.velocity = self->velocity,
				.previousOrigin = self->currentOrigin,
				.previousVelocity = previousVelocity,
			},
			.ground = self->groundInfo,
			.liquid = self->liquidInfo,
		};

		const int32_t blockedMask = SVG_MMove_StepSlideMove( &monsterMove, self->navPathPolicy );

		/**
		*	Obstruction recovery:
		*		If movement was blocked/trapped, force a path rebuild on the next frame so the
		*		AI can route around dynamic obstacles (doors, moving brush models, other entities).
		**/
		//if ( ( blockedMask & ( MM_SLIDEMOVEFLAG_BLOCKED | MM_SLIDEMOVEFLAG_TRAPPED ) ) != 0 ) {
		//	// Clear any backoff so we can attempt a rebuild immediately.
		//	self->navPathProcess.consecutive_failures = 0;
		//	self->navPathProcess.backoff_until = 0_ms;
		//	self->navPathProcess.last_failure_time = 0_ms;
		//	// Force rebuild by clearing next rebuild time.
		//	self->navPathProcess.next_rebuild_time = 0_ms;
		//}


		//// Prevent walking off large drops.
		//if ( self->groundInfo.entityNumber != ENTITYNUM_NONE ) {
		//	const bool willBeAirborne = ( monsterMove.ground.entityNumber == ENTITYNUM_NONE );
		//	if ( willBeAirborne ) {
		//		Vector3 start = monsterMove.state.origin;
		//		// Project the vector distance into the direction we're heading forward to.
		//		Vector3 forwardDir = QM_Vector3Normalize( monsterMove.state.velocity );
		//		Vector3 end = start + forwardDir * 24.0f;
		//		//Vector3 end = start;
		//		end.z -= self->navPathPolicy.max_drop_height;
		//		svg_trace_t tr = gi.trace( &start, &self->mins, &self->maxs, &end, self, CM_CONTENTMASK_SOLID );
		//		const float drop = start.z - tr.endpos[ 2 ];
		//		/**
		//		*	Determine the drop allowance derived from the path policy so we respect the drop cap.
		//		**/
		//		const float policyDropLimit = ( self->navPathPolicy.drop_cap > 0.0f )
		//			? ( float )self->navPathPolicy.drop_cap
		//			: ( self->navPathPolicy.cap_drop_height ? ( float )self->navPathPolicy.max_drop_height : 0.0f );
		//		if ( tr.fraction < 1.0f && policyDropLimit > 0.0f && drop > policyDropLimit ) {
		//			monsterMove.state.origin = monsterMove.state.origin - ( forwardDir * 24.f );//self->currentOrigin - forwardDir * 24.f;
		//			monsterMove.state.velocity.x = 0.0f;
		//			monsterMove.state.velocity.y = 0.0f;
		//			monsterMove.ground = self->groundInfo;
		//		}
		//	}
		//}

		/**
		*    If we stepped, set renderfx to trigger stair-step effects.
		**/
		if ( std::fabs( monsterMove.step.height ) > 0.f + FLT_EPSILON ) {
			self->s.renderfx |= RF_STAIR_STEP;
		}

		/**
		*    Commit movement results if not trapped: update ground/liquid info,
		*    set entity origin/velocity and relink entity into world.
		**/
		if ( !( blockedMask & MM_SLIDEMOVEFLAG_TRAPPED ) ) {
			self->groundInfo = monsterMove.ground;
			self->liquidInfo = monsterMove.liquid;
			SVG_Util_SetEntityOrigin( self, monsterMove.state.origin, true );
			self->velocity = monsterMove.state.velocity;
			// If on ground, zero out Z velocity to prevent floating (disabled by default).
			//if (self->groundInfo.entityNumber != ENTITYNUM_NONE) {
			//    self->velocity.z = 0.0f;
			//}
			gi.linkentity( self );
		}

		Vector3 postSumOrigin = self->currentOrigin;
		Vector3 diffOrigin = postSumOrigin - preSumOrigin;
		const double diffLength = QM_Vector3LengthSqr( diffOrigin );
		self->summedDistanceTraversed += diffLength;
	} else if ( self->lifeStatus != LIFESTATUS_ALIVE ) {
		// Dead: play death anims as before.
		if ( self->s.frame >= 512 && self->s.frame < 642 ) {
			self->s.frame++;
			if ( self->s.frame >= 642 ) {
				self->s.frame = 641;
			}
		} else if ( self->s.frame >= 642 && self->s.frame < 801 ) {
			self->s.frame++;
			if ( self->s.frame >= 801 ) {
				self->s.frame = 800;
			}
		} else if ( self->s.frame >= 801 && self->s.frame < 928 ) {
			self->s.frame++;
			if ( self->s.frame >= 928 ) {
				self->s.frame = 927;
			}
		}
	}

	// Animation/angles always applied.
	SVG_Util_SetEntityAngles( self, self->currentAngles, true );
	self->nextthink = level.time + FRAME_TIME_MS;
}
