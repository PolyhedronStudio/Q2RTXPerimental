/********************************************************************
*
*
*	ServerGame: TestDummy Monster Edict
*	File: svg_monster_testdummy_puppet.cpp
*	Description:
*		Entity logic and puppet/thinking behaviour for the TestDummy monster.
*
*
********************************************************************/

// Includes: engine, game, utilities and entity headers.
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

// TestDummy Monster
#include "svgame/entities/monster/svg_monster_testdummy.h"

// Navigation cluster routing (coarse tile routing pre-pass).
#include "svgame/nav/svg_nav_clusters.h"

// Local debug toggle for noisy per-frame prints in this test monster.
static constexpr bool DUMMY_NAV_DEBUG = false;

/**
*	@brief	Derive the nav-agent bbox (feet-origin) used for traversal/pathfinding.
*	@param	ent		Entity whose bbox we will use.
*	@param	out_mins	[out] Feet-origin mins (negative Z).
*	@param	out_maxs	[out] Feet-origin maxs.
*	@note	The nav traversal code expects an entity-style feet-origin bbox. The TestDummy
*			stores a collision bbox in `mins/maxs` but in some maps/spawn setups those can end
*			up being authored as a 0..height range. That breaks the nav center-offset
*			conversion and causes start-node selection to land on the wrong Z layer.
*/
static inline void SVG_TestDummy_GetNavAgentBBox( const svg_monster_testdummy_t *ent, Vector3 *out_mins, Vector3 *out_maxs ) {
	/**
	*	Sanity: require valid pointers.
	**/
	if ( !ent || !out_mins || !out_maxs ) {
		return;
	}

	/**
	*	Initialize from entity collision bounds.
	**/
	*out_mins = ent->mins;
	*out_maxs = ent->maxs;

	/**
	*	Fix-up: if bbox appears to be authored in a 0..height style, convert it to a
	*	feet-origin bbox by shifting it down by half-height.
	**/
	if ( out_mins->z >= 0.0f && out_maxs->z > 0.0f ) {
		const float centerOffsetZ = ( out_mins->z + out_maxs->z ) * 0.5f;
		out_mins->z -= centerOffsetZ;
		out_maxs->z -= centerOffsetZ;
	}
}

// -----------------------------------------------------------------
// Static helpers (translation-unit local)
// -----------------------------------------------------------------
// Per-entity "current trail spot" cache for stable trail following.
// This avoids calling PickFirst every frame (which can cause oscillation).
#include <unordered_map>
static std::unordered_map<int32_t, svg_base_edict_t*> s_dummyTrailSpot;

/**
*	@brief Small spherical linear interpolation for directions.
*	@note Local to this translation unit.
*/
static inline Vector3 QM_Vector3SlerpLocal( const Vector3 &from, const Vector3 &to, float t )
{
    float dot = QM_Vector3DotProduct( from, to );
    float aFactor;
    float bFactor;
    if ( fabsf( dot ) > 0.9995f ) {
        aFactor = 1.0f - t;
        bFactor = t;
    } else {
        float ang = acosf( dot );
        float sinOmega = sinf( ang );
        float sinAOmega = sinf( ( 1.0f - t ) * ang );
        float sinBOmega = sinf( t * ang );
        aFactor = sinAOmega / sinOmega;
        bFactor = sinBOmega / sinOmega;
    }
    return from * aFactor + to * bFactor;
}


/**
*    @brief    Member helper: compute an absolute frame index for a root motion animation.
*/
int32_t svg_monster_testdummy_t::ComputeAnimFrameFromRootMotion( const skm_rootmotion_t *rootMotion, float animHz ) const {
    if ( !rootMotion ) {
        return 0;
    }
    const double t = level.time.Seconds<double>();
    const int32_t animFrame = ( int32_t )floorf( ( float )( t * animHz ) );
    const int32_t localFrame = ( rootMotion->frameCount > 0 ) ? ( animFrame % rootMotion->frameCount ) : 0;
    return rootMotion->firstFrameIndex + localFrame;
}


/**
*	@brief	ember helper: Face a horizontal target smoothly by blending between a hint direction and the exact target direction.f
*/	
void svg_monster_testdummy_t::FaceTargetHorizontal( const Vector3 &directionHint, const Vector3 &targetPoint, float blendFactor, float yawSpeed ) {
	// Initially initialize to current yaw. (It will slerp so what we eventually face targetPoint ).
    Vector3 target = targetPoint;
	// Project to feet
	target.z = targetPoint.z;
	// Compute direction to target.
    Vector3 toTarget = QM_Vector3Subtract( target, currentOrigin );
	// Project to horizontal plane.
    const float dist2D = std::sqrt( toTarget.x * toTarget.x + toTarget.y * toTarget.y );
	// Face only if we have a valid horizontal direction to face.
    if ( dist2D > 0.001f ) {
		// Determine blended facing direction.
        Vector3 faceDir = QM_Vector3SlerpLocal( directionHint, QM_Vector3Normalize( toTarget ), blendFactor );
		// Update the yaw properties.
        ideal_yaw = QM_Vector3ToYaw( faceDir );
        yaw_speed = yawSpeed;
		// Finally face the ideal yaw.
        SVG_MMove_FaceIdealYaw( this, ideal_yaw, yaw_speed );
    }
}



/**
*
*
*
*   Save Descriptor Field Setup: svg_monster_testdummy_t
*
*
*
**/
/**
*   @brief  Save descriptor array definition for all the members of svg_monster_testdummy_t.
**/
SAVE_DESCRIPTOR_FIELDS_BEGIN( svg_monster_testdummy_t )
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_monster_testdummy_t, summedDistanceTraversed, SD_FIELD_TYPE_DOUBLE ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_monster_testdummy_t, testVar, SD_FIELD_TYPE_INT32 ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_monster_testdummy_t, testVar2, SD_FIELD_TYPE_VECTOR3 ),
SAVE_DESCRIPTOR_FIELDS_END();

//! Implement the methods for saving this edict type's save descriptor fields.
SVG_SAVE_DESCRIPTOR_FIELDS_DEFINE_IMPLEMENTATION( svg_monster_testdummy_t, svg_base_edict_t );



/**
*
*   Core:
*
**/
/**
*   Reconstructs the object, optionally retaining the entityDictionary.
**/
void svg_monster_testdummy_t::Reset( const bool retainDictionary ) {
    // Now, reset derived-class state.
    IMPLEMENT_EDICT_RESET_BY_COPY_ASSIGNMENT( Super, SelfType, retainDictionary );
    #if 0
    // Call upon the base class.
    Super::Reset( retainDictionary );
    #endif

    // 
    testVar = 1337;
}


/**
*   @brief  Save the entity into a file using game_write_context.
*   @note   Make sure to call the base parent class' Save() function.
**/
void svg_monster_testdummy_t::Save( struct game_write_context_t *ctx ) {
    // Call upon the base class.
    //sv_shared_edict_t<svg_base_edict_t, svg_client_t>::Save( ctx );
    Super::Save( ctx );
    // Save all the members of this entity type.
    ctx->write_fields( svg_monster_testdummy_t::saveDescriptorFields, this );
}
/**
*   @brief  Restore the entity from a loadgame read context.
*   @note   Make sure to call the base parent class' Restore() function.
**/
void svg_monster_testdummy_t::Restore( struct game_read_context_t *ctx ) {
    // Restore parent class fields.
    Super::Restore( ctx );
    // Restore all the members of this entity type.
    ctx->read_fields( svg_monster_testdummy_t::saveDescriptorFields, this );

    // Make sure to restore the actual root motion set data.
    const char *modelname = model;
    const model_t *model_forname = gi.GetModelDataForName( modelname );
    rootMotionSet = &model_forname->skmConfig->rootMotion;
}


/**
*   @brief  Called for each cm_entity_t key/value pair for this entity.
*           If not handled, or unable to be handled by the derived entity type, it will return
*           set errorStr and return false. True otherwise.
**/
const bool svg_monster_testdummy_t::KeyValue( const cm_entity_t *keyValuePair, std::string &errorStr ) {
    return Super::KeyValue( keyValuePair, errorStr );
}



/**
*
*   TestDummy Callback Member Functions:
*
**/
/**
*   @brief  Spawn routine.
**/
DEFINE_MEMBER_CALLBACK_SPAWN( svg_monster_testdummy_t, onSpawn )( svg_monster_testdummy_t *self ) -> void {
	Super::onSpawn( self );

    /**
    *    Basic entity type and movement properties.
    **/
    self->s.entityType = ET_MONSTER;

	self->solid = SOLID_BOUNDS_BOX;
	self->movetype = MOVETYPE_WALK;


    /**
    *    Load model and cache root-motion set for animations.
    **/
    self->model = svg_level_qstring_t::from_char_str( "models/characters/mixadummy/tris.iqm" );
    self->s.modelindex = gi.modelindex( self->model );
    const char *modelname = self->model;
    const model_t *model_forname = gi.GetModelDataForName( modelname );
    self->rootMotionSet = &model_forname->skmConfig->rootMotion;


    /**
    *    Collision bbox and physics defaults.
    **/
    VectorCopy( svg_monster_testdummy_t::DUMMY_BBOX_STANDUP_MINS, self->mins );
    VectorCopy( svg_monster_testdummy_t::DUMMY_BBOX_STANDUP_MAXS, self->maxs );
	// Very important to set in order for its AI navigation to work properly.
	self->viewheight = 60.0f;
	// Important to set for physics interactions.
    self->mass = 200;


    /**
    *    Default attribute fallbacks for spawned entity.
    **/
    if ( !self->health ) {
        self->health = 200;
    }
    if ( !self->dmg ) {
        self->dmg = 150;
    }
    if ( !self->gravity ) {
        self->gravity = 1.0f;
    }


    /**
    *    Entity flags and render properties.
    **/
    self->svFlags &= ~SVF_DEADENTITY;
    self->svFlags |= SVF_MONSTER;

	self->s.skinnum = 0;

	self->takedamage = DAMAGE_AIM;

	self->air_finished_time = level.time + 12_sec;
	self->max_health = self->health;

	self->clipMask = CM_CONTENTMASK_MONSTERSOLID;

	self->takedamage = DAMAGE_YES;
	self->lifeStatus = LIFESTATUS_ALIVE;


    /**
    *    Interaction hooks and think scheduling.
    **/
    self->useTarget.flags = ENTITY_USETARGET_FLAG_TOGGLE;

    self->nextthink = level.time + 20_hz;
    self->SetThinkCallback( &svg_monster_testdummy_t::onThink );

    self->SetDieCallback( &svg_monster_testdummy_t::onDie );
    self->SetPainCallback( &svg_monster_testdummy_t::onPain );
    self->SetPostSpawnCallback( &svg_monster_testdummy_t::onPostSpawn );
    self->SetTouchCallback( &svg_monster_testdummy_t::onTouch );
    self->SetUseCallback( &svg_monster_testdummy_t::onUse );

	SVG_Entity_SetUseTargetHintByID( self, USETARGET_HINT_ID_NPC_ENGAGE );

	gi.linkentity( self );
}

/**
*   @brief  Post-Spawn routine.
**/
DEFINE_MEMBER_CALLBACK_POSTSPAWN( svg_monster_testdummy_t, onPostSpawn )( svg_monster_testdummy_t *self ) -> void {
    // Make sure to fall to floor.
    if ( !self->activator ) {
		const cm_contents_t mask = SVG_GetClipMask( self );
		M_CheckGround( self, mask );
        M_droptofloor( self );


    }
    //---------------------------
    // <TEMPORARY FOR TESTING>
    //---------------------------
    //const char *modelname = self->model.ptr;
    //const model_t *model_forname = gi.GetModelDataForName( modelname );
    //self->rootMotionSet = &model_forname->skmConfig->rootMotion;
    //---------------------------
    // </TEMPORARY FOR TESTING>
    //---------------------------
}


/**
*	@brief	Idle behavior: stop movement and play idle animation.
**/
void svg_monster_testdummy_t::ThinkIdle() {
	//velocity = {};
	if ( rootMotionSet && rootMotionSet->motions[ 0 ] ) {
		// Animation: idle anim frame selection centralized.
		skm_rootmotion_t *rootMotion = rootMotionSet->motions[ 0 ]; // IDLE
		s.frame = ComputeAnimFrameFromRootMotion( rootMotion, 40.0f );
	}
}

/**
*	@brief	Direct pursuit of the player (LOS).
**/
void svg_monster_testdummy_t::ThinkDirectPursuit() {
	// Set the last trail time now for future trail spot picking.
	trail_time = level.time;

    /**
    *    Mark trail timestamp: used to prefer recent trail spots when picking waypoints.
    **/

	/**
	*	Direct-pursuit should not constantly wipe path state.
	*		Resetting here every frame prevents the monster from resuming a previously
	*		computed navigation path once LOS is lost again.
	**/

	/**
    *    Clear any previously computed navigation path so we start fresh for direct pursuit.
    **/

	if ( DUMMY_NAV_DEBUG ) {
		gi.dprintf("[DEBUG] ThinkDirectPursuit: LOS to player. Monster %d pursuing player at (%.1f %.1f %.1f) from (%.1f %.1f %.1f)\n",
			s.number, goalentity->currentOrigin.x, goalentity->currentOrigin.y, goalentity->currentOrigin.z,
			currentOrigin.x, currentOrigin.y, currentOrigin.z);
	}

	/**
    *    Animation: select run animation frame for current time.
    **/
	skm_rootmotion_t *rootMotion = rootMotionSet->motions[ 3 ]; // RUN_FORWARD_PISTOL
	// Animation frame selection (member helper).
	s.frame = ComputeAnimFrameFromRootMotion( rootMotion, 40.0f );

	/**
    *    Compute 3D movement direction toward goal (preserves vertical component for stairs/steps).
    **/
	// Move directly toward player using full 3D direction (for stairs/steps/slopes).
	Vector3 toGoal = QM_Vector3Subtract( goalentity->currentOrigin, currentOrigin );
	Vector3 move_dir = QM_Vector3Normalize( toGoal );

	constexpr float desiredSeparation = 8.0f;
	constexpr float slowDownRange = 64.0f;
	constexpr double desiredAverageSpeed = 220.0;

	float dist3D = QM_Vector3Length( toGoal );
	float approachDist = std::max( 0.0f, dist3D - desiredSeparation );
	float speedScale = ( slowDownRange > 0.0f )
		? QM_Clamp( approachDist / slowDownRange, 0.0f, 1.0f )
		: 1.0f;
	double frameVelocity = desiredAverageSpeed * speedScale;

	// If close enough, stop.
	if ( approachDist <= 0.0f && std::fabs( toGoal.z ) < 32.0f ) {
		frameVelocity = 0.0f;
	}


    /**
    *    Facing: blend between move direction and exact target direction for snappier yaw.
    **/
    {
        const float playerYawSpeed = 15.0f;
        const float blendFactor = 0.35f; // 0..1 where 1 is full target-facing
        FaceTargetHorizontal( move_dir, goalentity->currentOrigin, blendFactor, playerYawSpeed );
    }

	velocity.x = ( float )( move_dir.x * frameVelocity );
	velocity.y = ( float )( move_dir.y * frameVelocity );
	// Do not directly set Z velocity from path/player direction — allow step/physics to handle vertical movement.
}

/**
*	@brief	Try to pathfind to the player using A* (even if not visible).
*	@return	True if a path is found and movement is set, false otherwise.
**/
const bool svg_monster_testdummy_t::ThinkAStarToPlayer()
{
	if (!goalentity) {
		//gi.dprintf("[DEBUG] ThinkAStarToPlayer: No goalentity, cannot path.\n");
		return false;
	}

	/**
	*	If the goal is on another floor (Z gap beyond our step height), do not ask the
	*	nav system to path directly to that elevated point.
	*		Instead we path in XY toward a reachable proxy goal on our current floor.
	*		This prevents repeated rebuild failures (and hitches) when the player is on
	*		a platform above a solid ceiling/floor separation.
	**/
	/**
	*	Goal selection:
	*		When the target is far above our current floor (platform/ledge), pathing to the
	*		exact goal origin frequently fails (huge Z gap). In that case, prefer a trail
	*		spot goal so the nav system can route to the stairs first.
	**/
	Vector3 goalOrigin = goalentity->currentOrigin;
	{
		// Determine our effective step capability.
		const float stepLimit = ( navPathPolicy.max_step_height > 0.0 ) ? ( float )navPathPolicy.max_step_height : 18.0f;
		const float zGapAbs = std::fabs( goalOrigin.z - currentOrigin.z );
		if ( zGapAbs > ( stepLimit + 1.0f ) ) {
			// Clamp goal Z to our current Z so A* finds a 2D route to a staircase/connection first.
			goalOrigin.z = currentOrigin.z;
		}
	}
	{
		const float zGapAbs = std::fabs( goalOrigin.z - currentOrigin.z );
		// If the goal is well above our current floor, prefer breadcrumb navigation.
		if ( zGapAbs > 96.0f ) {
			svg_base_edict_t *trailSpot = PlayerTrail_PickFirst( this );
			if ( trailSpot ) {
				goalOrigin = trailSpot->currentOrigin;
			}
		}
	}

	/**
	*	Derive the correct nav-agent bbox (feet-origin) for traversal.
	**/
	Vector3 agent_mins = {};
	Vector3 agent_maxs = {};
	SVG_TestDummy_GetNavAgentBBox( this, &agent_mins, &agent_maxs );

    // Configure path following policy for direct A* pursuit.
    // These values control waypoint acceptance radius and rebuild heuristics.
    navPathPolicy.waypoint_radius = 32.0f;
    navPathPolicy.rebuild_goal_dist_2d = 32.0f;
    // Use a reasonable 3D rebuild threshold (typo previously produced an absurd value).
    navPathPolicy.rebuild_goal_dist_3d = 256.0f;
    navPathPolicy.rebuild_interval = 250_ms;
    navPathPolicy.fail_backoff_base = 100_ms;
    navPathPolicy.fail_backoff_max_pow = 4;

    // Ensure step and drop safety parameters match intended agent capabilities.
    // Allow stepping up to 18 units and cap drops to 128 units when following paths.
    navPathPolicy.max_step_height = 18.0;
    navPathPolicy.max_drop_height = 128.0;
    navPathPolicy.cap_drop_height = true;

    // Goal Z-layer blending: bias layer selection toward goal Z quickly so the
    // pathfinder will prefer climbing stairs/up to the target when appropriate.
    navPathPolicy.enable_goal_z_layer_blend = true;
    navPathPolicy.blend_start_dist = 0.0f;   // begin blending immediately
    navPathPolicy.blend_full_dist = 32.0f;   // fully prefer goal Z at this range

    // Small obstruction jump tuning: allow small jumps over low obstacles.
    navPathPolicy.allow_small_obstruction_jump = true;
    navPathPolicy.max_obstruction_jump_height = 33.0;

	/**
	*	Cheap short-range fallback:
	*		If we are close enough that a straight chase is likely to work (even if nav
	*		rebuild is failing due to Z-layer selection), prefer direct pursuit.
	*		This avoids frame spikes from hammering A* rebuilds.
	**/
	{
		const Vector3 toGoal = QM_Vector3Subtract( goalOrigin, currentOrigin );
		const float dist2 = ( toGoal.x * toGoal.x ) + ( toGoal.y * toGoal.y );
		const float dist3 = ( toGoal.x * toGoal.x ) + ( toGoal.y * toGoal.y ) + ( toGoal.z * toGoal.z );
		constexpr float kNear2D = 128.0f;
		constexpr float kNear3D = 196.0f;
		if ( dist2 <= ( kNear2D * kNear2D ) && dist3 <= ( kNear3D * kNear3D ) ) {
			// Use direct pursuit so step/slide movement can walk up small stair chains.
			ThinkDirectPursuit();
			return true;
		}
	}

    /**
    *    Pathfinding: build A* path using the agent bbox converted to navmesh space.
    *    svg_nav_path_process handles the coordinate conversions internally.
    **/
    // Convert entity feet-origin bbox to navmesh-centered bbox/origins.
    // Call nav path process using entity origin; svg_nav_path_process will handle conversions.
    // Use 'force=true' for active pursuit attempts so the AI can bypass throttle/backoff
    // when explicitly commanded to attempt immediate path reconstructions.
    const bool forceRebuild = false;
    bool pathOk = navPathProcess.RebuildPathToWithAgentBBox(
        currentOrigin, goalOrigin, navPathPolicy,
		agent_mins, agent_maxs,
		forceRebuild
    );

    Vector3 move_dir = {};
    bool hasPathDir = navPathProcess.QueryDirection3D(currentOrigin, navPathPolicy, &move_dir);

	if ( DUMMY_NAV_DEBUG ) {
		gi.dprintf("[DEBUG] ThinkAStarToPlayer: Monster %d pathOk=%d hasPathDir=%d from (%.1f %.1f %.1f) to (%.1f %.1f %.1f)\n",
			s.number, pathOk ? 1 : 0, hasPathDir ? 1 : 0,
			currentOrigin.x, currentOrigin.y, currentOrigin.z,
			goalOrigin.x, goalOrigin.y, goalOrigin.z);
	}

    if (!pathOk || !hasPathDir) {
        // If the target is roughly in front of us, prefer direct pursuit fallback
        // (face-and-walk) instead of idling when pathfinding momentarily fails
        // on stairs/quantization edges.
		/**
		*	Fallback:
		*		If we can see the goal (or it's in front), prefer direct pursuit rather than
		*		idling when A* momentarily fails on vertical transitions.
		**/
		// Do not fallback to direct-pursuit when goal is substantially above us; we need stairs.
		const float zGapAbs = std::fabs( goalentity->currentOrigin.z - currentOrigin.z );
		if ( zGapAbs <= 64.0f
			&& goalentity
			&& ( !navPathPolicy.ignore_visibility ? SVG_Entity_IsVisible( this, goalentity ) : true )
			&& ( !navPathPolicy.ignore_infront ? SVG_Entity_IsInFrontOf( this, goalentity, { 1.0f, 1.0f, 0.0f }, 0.35 ) : true ) ) {
			if ( DUMMY_NAV_DEBUG ) {
				gi.dprintf( "[DEBUG] ThinkAStarToPlayer: Pathfinding failed but target in front — falling back to direct pursuit.\n" );
			}
			ThinkDirectPursuit();
			return true;
		}

		if ( DUMMY_NAV_DEBUG ) {
			gi.dprintf( "[DEBUG] ThinkAStarToPlayer: Pathfinding failed, will try trail/noise next.\n" );
		}

        return false;
    }

    /**
    *    Convert next navmesh-centered waypoint to entity feet-origin space for comparison.
    */
    // Path points are nav-centered; convert next nav point to feet-origin for comparisons.
    // Convert next nav point from navmesh space to entity feet-origin space.
    Vector3 nextNavPoint_feet = {};
    navPathProcess.GetNextPathPointEntitySpace( &nextNavPoint_feet );
    // Compute vertical delta between nav point and our feet-origin.
    float zDelta = std::fabs( nextNavPoint_feet.z - currentOrigin.z );
    // Threshold under which we treat the waypoint as same step level
    constexpr float zStepThreshold = 32.0f;
    // Base movement speed used for frame velocity calculations.
    double frameVelocity = 220.0;
    // If the target is roughly on the same horizontal plane, compute slowdown.
    if ( zDelta < zStepThreshold ) {
        // Vector from us to the nav waypoint.
        Vector3 toGoal = QM_Vector3Subtract( nextNavPoint_feet, currentOrigin );
        // Horizontal distance to the waypoint.
        float dist2D = std::sqrt( toGoal.x * toGoal.x + toGoal.y * toGoal.y );
        // Desired separation so we don't clip into the target.
        constexpr float desiredSeparation = 8.0f;
        // Distance remaining until desired separation.
        float approachDist = std::max( 0.0f, dist2D - desiredSeparation );
        // Range over which to slow down when approaching.
        constexpr float slowDownRange = 64.0f;
        // Scale speed between 0..1 based on approach distance.
        float speedScale = ( slowDownRange > 0.0f )
            ? QM_Clamp( approachDist / slowDownRange, 0.0f, 1.0f )
            : 1.0f;
        // Apply slowdown.
        frameVelocity *= speedScale;
        // If we're effectively at the target, stop.
        if ( approachDist <= 0.0f ) {
            frameVelocity = 0.0f;
        }
    }

	if ( DUMMY_NAV_DEBUG ) {
		gi.dprintf("[DEBUG] ThinkAStarToPlayer: Next nav point feet(%.1f %.1f %.1f) zDelta=%.1f\n",
			nextNavPoint_feet.x, nextNavPoint_feet.y, nextNavPoint_feet.z, zDelta);
	}

    {
        // Face the next navigation waypoint on the horizontal plane for smoother turning.
        const float blendFactor = 0.2f;
        FaceTargetHorizontal( move_dir, nextNavPoint_feet, blendFactor, 7.5f );
    }

	// Use full 3D direction for velocity
	velocity.x = (float)(move_dir.x * frameVelocity);
	velocity.y = (float)(move_dir.y * frameVelocity);
	// Let physics/step logic manage vertical movement; don't set Z directly here.

	return true;
}

/**
*	@brief	Follow the player trail (breadcrumbs), including up/down stairs.
*	@return	True if a trail spot is found and movement is set, false otherwise.
**/
const bool svg_monster_testdummy_t::ThinkFollowTrail()
{
	/**
	*	Trail selection:
	*		Prefer the trail system for multi-floor pursuit, but avoid targeting a trail
	*		point that is significantly above our current floor as an immediate A* goal.
	*		When that happens we path in XY first (goal Z projected to our current Z) so
	*		we can route toward the staircase that eventually reaches that trail spot.
	**/
	svg_base_edict_t *trailSpot = PlayerTrail_PickFirst( this );
	//svg_base_edict_t *trailSpot = PlayerTrail_LastSpot();
	if (!trailSpot) {
		gi.dprintf("[DEBUG] ThinkFollowTrail: No trail spot found for monster %d\n", s.number);
		return false;
	}

	Vector3 goalOrigin = trailSpot->currentOrigin;

	/**
	*	Derive the correct nav-agent bbox (feet-origin) for traversal.
	**/
	Vector3 agent_mins = {};
	Vector3 agent_maxs = {};
	SVG_TestDummy_GetNavAgentBBox( this, &agent_mins, &agent_maxs );

    // Cache the current trail spot origin for stable facing if needed.
    s_dummyTrailSpot[ s.number ] = trailSpot;

    /**
    *    Trail-following: update nav policy for breadcrumb following and attempt to rebuild a path.
    **/
	navPathPolicy.ignore_visibility = true;
	navPathPolicy.ignore_infront = true;
	
	navPathPolicy.waypoint_radius = 32.0f;
	navPathPolicy.rebuild_goal_dist_2d =  32.0f;
	navPathPolicy.rebuild_goal_dist_3d = 1024.0f;
	navPathPolicy.rebuild_interval = 100_ms;
	navPathPolicy.fail_backoff_base = 100_ms;
	navPathPolicy.fail_backoff_max_pow = 4;

	// Ensure step and drop safety parameters for trail-following match agent.
	// This allows stepping up to 18 units and caps allowed drops to 128 units.
	navPathPolicy.max_step_height = 18.0;
	navPathPolicy.max_drop_height = 128.0;
	navPathPolicy.cap_drop_height = true;

    // Bias layer selection toward goal Z for trail-following so the monster will
    // attempt to climb stairs to reach breadcrumb spots on higher floors.
    navPathPolicy.enable_goal_z_layer_blend = true;
    navPathPolicy.blend_start_dist = 0.0f;
    navPathPolicy.blend_full_dist = 32.0f;

    navPathPolicy.allow_small_obstruction_jump = true;
    navPathPolicy.max_obstruction_jump_height = 33.0;

    // Convert entity feet-origin bbox to navmesh-centered bbox/origins.
    // Call nav path process using entity origin; svg_nav_path_process will handle conversions.
    bool pathOk = navPathProcess.RebuildPathToWithAgentBBox(
        currentOrigin, goalOrigin, navPathPolicy,
		agent_mins, agent_maxs
    );

    Vector3 move_dir = {};
    bool hasPathDir = navPathProcess.QueryDirection3D(currentOrigin, navPathPolicy, &move_dir);

	if ( DUMMY_NAV_DEBUG ) {
		gi.dprintf("[DEBUG] ThinkFollowTrail: Monster %d pathOk=%d hasPathDir=%d from (%.1f %.1f %.1f) to (%.1f %.1f %.1f)\n",
			s.number, pathOk ? 1 : 0, hasPathDir ? 1 : 0,
			currentOrigin.x, currentOrigin.y, currentOrigin.z,
			goalOrigin.x, goalOrigin.y, goalOrigin.z);
	}

    if (!pathOk || !hasPathDir) {
        // If the trail spot (goal) is roughly in front, attempt direct pursuit as a
        // robust fallback for stairs/step transitions where A* may momentarily fail.
        if ( SVG_Entity_IsInFrontOf( this, trailSpot, { 1., 1., 0. }, 0.35 ) ) {
		if ( DUMMY_NAV_DEBUG ) {
			gi.dprintf( "[DEBUG] ThinkFollowTrail: Pathfinding failed but trail spot in front — falling back to direct pursuit.\n" );
		}
            // Set goalentity so direct pursuit targets the trail spot and run.
            goalentity = trailSpot;
            ThinkDirectPursuit();
            return true;
        }

		if ( DUMMY_NAV_DEBUG ) {
			gi.dprintf( "[DEBUG] ThinkFollowTrail: Pathfinding to trail failed, will try noise/idle next.\n" );
		}
        return false;
    }

    /**
    *    Convert next nav point from navmesh space to entity feet-origin space for comparisons.
    **/
    // Path points are nav-centered; convert next nav point to feet-origin for comparisons.
    // Convert next nav point from navmesh space to entity feet-origin space.
    Vector3 nextNavPoint_feet = {};
    navPathProcess.GetNextPathPointEntitySpace( &nextNavPoint_feet );
    // Compute vertical delta between nav point and our feet-origin.
    float zDelta = std::fabs( nextNavPoint_feet.z - currentOrigin.z );
    // Threshold under which we treat the waypoint as same step level
    constexpr float zStepThreshold = 32.0f;
    // Base movement speed used for frame velocity calculations.
    double frameVelocity = 220.0;
    // If the target is roughly on the same horizontal plane, compute slowdown.
    if ( zDelta < zStepThreshold ) {
        // Vector from us to the nav waypoint.
        Vector3 toGoal = QM_Vector3Subtract( nextNavPoint_feet, currentOrigin );
        // Horizontal distance to the waypoint.
        float dist2D = std::sqrt( toGoal.x * toGoal.x + toGoal.y * toGoal.y );
        // Desired separation so we don't clip into the target.
        constexpr float desiredSeparation = 8.0f;
        // Distance remaining until desired separation.
        float approachDist = std::max( 0.0f, dist2D - desiredSeparation );
        // Range over which to slow down when approaching.
        constexpr float slowDownRange = 64.0f;
        // Scale speed between 0..1 based on approach distance.
        float speedScale = ( slowDownRange > 0.0f )
            ? QM_Clamp( approachDist / slowDownRange, 0.0f, 1.0f )
            : 1.0f;
        // Apply slowdown.
        frameVelocity *= speedScale;
        // If we're effectively at the target, stop.
        if ( approachDist <= 0.0f ) {
            frameVelocity = 0.0f;
        }
    }

	if ( DUMMY_NAV_DEBUG ) {
		gi.dprintf("[DEBUG] ThinkFollowTrail: Next nav point feet(%.1f %.1f %.1f) zDelta=%.1f\n",
			nextNavPoint_feet.x, nextNavPoint_feet.y, nextNavPoint_feet.z, zDelta);
	}
    /**
    *    If there was a recent heard sound, prefer facing it briefly (additional short blend).
    **/
    // If there was a recent heard sound, prefer facing it briefly.
    const QMTime now = level.time;
    const QMTime soundWindow = 500_ms;
    Vector3 soundFaceDir = {};
    bool haveSoundToFace = false;
    if ( last_sound_time_seen > 0_ms && ( now - last_sound_time_seen ) <= soundWindow ) {
        soundFaceDir = last_sound_origin;
        soundFaceDir.z = currentOrigin.z;
        haveSoundToFace = true;
    } else if ( last_sound2_time_seen > 0_ms && ( now - last_sound2_time_seen ) <= soundWindow ) {
        soundFaceDir = last_sound2_origin;
        soundFaceDir.z = currentOrigin.z;
        haveSoundToFace = true;
    }
    {
        // Face the next navigation waypoint (trail-following) on the horizontal plane.
        Vector3 target = nextNavPoint_feet;
        target.z = currentOrigin.z; // project to feet plane
        Vector3 toTarget = QM_Vector3Subtract( target, currentOrigin );
        const float dist2D = std::sqrt( toTarget.x * toTarget.x + toTarget.y * toTarget.y );
        if ( dist2D > 0.001f ) {
            const float blendFactor = 0.25f;
            Vector3 desiredFace = QM_Vector3Normalize( toTarget );
            // If we have a recent sound, blend towards sound origin a bit.
            if ( haveSoundToFace ) {
                Vector3 toSound = QM_Vector3Normalize( QM_Vector3Subtract( soundFaceDir, currentOrigin ) );
                // Prefer facing sound for a short time (additional small blend)
                desiredFace = QM_Vector3SlerpLocal( desiredFace, toSound, 0.35f );
            }
            Vector3 faceDir = QM_Vector3SlerpLocal( move_dir, desiredFace, blendFactor );
            ideal_yaw = QM_Vector3ToYaw( faceDir );
            yaw_speed = 7.5f;
            SVG_MMove_FaceIdealYaw( this, ideal_yaw, yaw_speed );
        }
    }

	// Use full 3D direction for velocity
	velocity.x = ( float )( move_dir.x * frameVelocity );
	velocity.y = ( float )( move_dir.y * frameVelocity );
	// Let physics/step logic manage vertical movement; don't set Z directly here.
	//velocity.z = 0.0f;

	return true;
}

/**
 *	@brief	Thinking routine. Main AI loop for the TestDummy monster.
 *	@note	Uses A* pathing for all active pursuit. If an activator exists it is
 *		treated as the primary goal and A* is rebuilt toward it each frame. Falls
 *		back to trail-following if pathfinding fails. Animation and root-motion
 *		processing as well as slide/step movement are preserved.
 **/
DEFINE_MEMBER_CALLBACK_THINK(svg_monster_testdummy_t, onThink)(svg_monster_testdummy_t *self) -> void
{
	self->s.renderfx &= ~(RF_STAIR_STEP | RF_OLD_FRAME_LERP);

	/**
	*	Re-ensure to check for ground and stick us to it if we can.
	**/
	const cm_contents_t mask = SVG_GetClipMask( self );
	M_CheckGround( self, mask );
	// Recatagorize position so we know what we're dealing with liquidity wise.
	M_CatagorizePosition( self, self->currentOrigin, self->liquidInfo.level, self->liquidInfo.type );
	
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
        *    This guarantees that we always attempt A* towards the activator even
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
            *    activator becomes occluded we still rebuild A* and follow the path
            *    toward the activator. If A* fails, attempt trail-following as a
            *    fallback; only when all navigation options fail do we idle for the
            *    frame while preserving the activator reference for retries.
            **/
			/**
			*	Debug visibility flag:
			*		Use actual line-of-sight from monster to activator.
			**/
			const bool activatorVisible = ( self->activator && SVG_Entity_IsVisible( self, self->activator ) );
			if ( DUMMY_NAV_DEBUG ) {
				gi.dprintf( "[DEBUG] onThink: Monster %d attempting A* toward goal (activatorPresent=%d visible=%d).\n",
					self->s.number, hasActivator ? 1 : 0, activatorVisible ? 1 : 0 );
			}

            bool pursuing = self->ThinkAStarToPlayer();

            if ( !pursuing ) {
                // Try trail-following as a robust fallback when A* fails.
				if ( DUMMY_NAV_DEBUG ) {
					gi.dprintf( "[DEBUG] onThink: Monster %d A* failed, attempting trail follow.\n", self->s.number );
				}
                pursuing = self->ThinkFollowTrail();
                if ( !pursuing ) {
                    // Preserve activator so we can retry A* next frame; stop movement this frame
                    // to avoid sliding from residual velocity. Keep Z velocity to allow physics
                    // to handle stepping/falling if appropriate.
					if ( DUMMY_NAV_DEBUG ) {
						gi.dprintf( "[DEBUG] onThink: Monster %d failed all pursuit, stopping movement and idling this frame (will retry next frame).\n", self->s.number );
					}
                    self->velocity.x = 0.0f;
                    self->velocity.y = 0.0f;
                    // Play idle animation frame selection.
                    self->ThinkIdle();
                }
            }
        }

        /**
        *    Always refresh ground and liquid state before movement.
        *    groundInfo contains the previous frame's ground data.
        **/
        // Always refresh ground and liquid state.
        // Ground/liq state is still available in self->groundInfo.

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
				.mm_flags = (self->groundInfo.entityNumber != ENTITYNUM_NONE ? MMF_ON_GROUND : 0),
				.mm_time = 0,
				.gravity = (int16_t)(self->gravity * sv_gravity->value),
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
		if ( ( blockedMask & ( MM_SLIDEMOVEFLAG_BLOCKED | MM_SLIDEMOVEFLAG_TRAPPED ) ) != 0 ) {
			// Clear any backoff so we can attempt a rebuild immediately.
			self->navPathProcess.consecutive_failures = 0;
			self->navPathProcess.backoff_until = 0_ms;
			self->navPathProcess.last_failure_time = 0_ms;
			// Force rebuild by clearing next rebuild time.
			self->navPathProcess.next_rebuild_time = 0_ms;
		}


		// Prevent walking off large drops.
		if ( self->groundInfo.entityNumber != ENTITYNUM_NONE ) {
			const bool willBeAirborne = ( monsterMove.ground.entityNumber == ENTITYNUM_NONE );
			if ( willBeAirborne ) {
				Vector3 start = monsterMove.state.origin;
				// Project the vector distance into the direction we're heading forward to.
				Vector3 forwardDir = QM_Vector3Normalize( monsterMove.state.velocity );
				Vector3 end = start + forwardDir * 24.0f;
				//Vector3 end = start;
				end.z -= self->navPathPolicy.max_drop_height;
				svg_trace_t tr = gi.trace( &start, &self->mins, &self->maxs, &end, self, CM_CONTENTMASK_SOLID );
				const float drop = start.z - tr.endpos[ 2 ];
				if ( tr.fraction < 1.0f && drop > self->navPathPolicy.max_drop_height ) {
					monsterMove.state.origin = monsterMove.state.origin - ( forwardDir * 24.f );//self->currentOrigin - forwardDir * 24.f;
					monsterMove.state.velocity.x = 0.0f;
					monsterMove.state.velocity.y = 0.0f;
					monsterMove.ground = self->groundInfo;
				}
			}
		}

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

/**
*   @brief  Touched.
**/
DEFINE_MEMBER_CALLBACK_TOUCH( svg_monster_testdummy_t, onTouch )( svg_monster_testdummy_t *self, svg_base_edict_t *other, const cm_plane_t *plane, cm_surface_t *surf ) -> void {
    gi.dprintf( "onTouch\n" );
}

/**
*   @brief
**/
DEFINE_MEMBER_CALLBACK_USE( svg_monster_testdummy_t, onUse )( svg_monster_testdummy_t *self, svg_base_edict_t *other, svg_base_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue ) -> void {
    // Apply activator.
    self->activator = activator;
    self->other = other;

    // "Toggle" between follow/unfollow.
    // Cheap hack.
    if ( useType == entity_usetarget_type_t::ENTITY_USETARGET_TYPE_TOGGLE ) {
        if ( useValue == 1 ) {
            if ( activator && activator->client ) {
                self->goalentity = activator;

                // Get the root motion.
                skm_rootmotion_t *rootMotion = self->rootMotionSet->motions[ 3 ]; // [1] == RUN_FORWARD_PISTOL
                // Transition to its animation.
                self->s.frame = rootMotion->firstFrameIndex;

				// Set to disengagement mode usehint. (Yes this is a cheap hack., it is not client specific.)
                SVG_Entity_SetUseTargetHintByID( self, USETARGET_HINT_ID_NPC_DISENGAGE );
                return;
            }
        }
    }

    // Reset to engagement mode usehint. (Yes this is a cheap hack., it is not client specific.)
    SVG_Entity_SetUseTargetHintByID( self, USETARGET_HINT_ID_NPC_ENGAGE );

    self->goalentity = nullptr;
    self->activator = nullptr;
	self->other = nullptr;

    // Fire set target.
    SVG_UseTargets( self, activator );
}

/**
*   @brief  Death routine.
**/
DEFINE_MEMBER_CALLBACK_DIE( svg_monster_testdummy_t, onDie )( svg_monster_testdummy_t *self, svg_base_edict_t *inflictor, svg_base_edict_t *attacker, int32_t damage, Vector3 *point ) -> void {
    //self->takedamage = DAMAGE_NO;
    //self->nextthink = level.time + 20_hz;
    //self->think = barrel_explode;

    if ( self->lifeStatus == LIFESTATUS_DEAD ) {
        return;
    }

    if ( self->lifeStatus == LIFESTATUS_DYING ) {
        // Gib Death:
        if ( self->health < GIB_DEATH_HEALTH ) {
            // Play gib sound.
            //gi.sound( self, CHAN_BODY, gi.soundindex( "world/gib01.wav" ), 1, ATTN_NORM, 0 );
			SVG_EntityEvent_GeneralSoundEx( self, CHAN_BODY, gi.soundindex( "world/gib01.wav" ), ATTN_NORM );
            //! Throw 4 small meat gibs around.
            for ( int32_t n = 0; n < 4; n++ ) {
                SVG_Misc_ThrowGib( self, "models/objects/gibs/sm_meat/tris.md2", damage, GIB_TYPE_ORGANIC );
            }
            // Turn ourself into the thrown head entity.
            SVG_Misc_ThrowHead( self, "models/objects/gibs/head2/tris.md2", damage, GIB_TYPE_ORGANIC );

            // Gibs don't take damage, but fade away as time passes.
            self->takedamage = DAMAGE_NO;

            // Set lifeStatus.
            self->lifeStatus = LIFESTATUS_DEAD;
        }
    }
    // Set activator.
    self->activator = attacker;

    //---------------------------
    // <TEMPORARY FOR TESTING>
    //---------------------------
    if ( self->lifeStatus == LIFESTATUS_ALIVE ) {
        // Pick a random death animation.
        int32_t deathanim = irandom( 3 );
        if ( deathanim == 0 ) {
            self->s.frame = 512;
        } else if ( deathanim == 1 ) {
            self->s.frame = 642;
        } else {
            self->s.frame = 801;
        }

        self->lifeStatus = LIFESTATUS_DYING;
        // Set this here so the entity does not block traces while playing death animation.
        self->svFlags |= SVF_DEADENTITY;
    } else if ( self->s.frame == 643 ) {
        // Monster Corpse Entity Type:
        self->s.entityType = ET_MONSTER_CORPSE;
    } else if ( self->s.frame == 800 ) {
        // Monster Corpse Entity Type:
        self->s.entityType = ET_MONSTER_CORPSE;
    } else if ( self->s.frame == 937 ) {
        // Monster Corpse Entity Type:
        self->s.entityType = ET_MONSTER_CORPSE;
    }
    //---------------------------
    // </TEMPORARY FOR TESTING>
    //---------------------------
    // Stop playing any sounds.
    self->s.sound = 0;
    // Setup the death bounding box.
	self->mins = DUMMY_BBOX_DEAD_MINS; // VectorCopy( DUMMY_BBOX_DEAD_MINS, self->mins );
	self->maxs = DUMMY_BBOX_DEAD_MAXS; // VectorCopy( DUMMY_BBOX_DEAD_MAXS, self->maxs );
    // Make sure to relink.
    gi.linkentity( self );
}
 /**
*   @brief  Death routine.
**/
DEFINE_MEMBER_CALLBACK_PAIN( svg_monster_testdummy_t, onPain )( svg_monster_testdummy_t *self, svg_base_edict_t *other, const float kick, const int32_t damage, const entity_damageflags_t damageFlags ) -> void {

}////////////////////
