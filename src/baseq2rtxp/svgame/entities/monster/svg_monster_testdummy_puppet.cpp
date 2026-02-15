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
// Async navigation queue helpers.
#include "svgame/nav/svg_nav_request.h"
// Traversal helpers required for path invalidation.
#include "svgame/nav/svg_nav_traversal.h"

// Local debug toggle for noisy per-frame prints in this test monster.
static constexpr bool DUMMY_NAV_DEBUG = true;

/**
*	@brief	Derive the nav-agent bbox (feet-origin) used for traversal/pathfinding.
*	@param	ent		Entity whose bbox we will use.
*	@param	out_mins	[out] Feet-origin mins (negative Z).
*	@param	out_maxs	[out] Feet-origin maxs.
*	@note	The navmesh is generated for the global nav agent profile, so path queries
*			must use the same mins/maxs to compute center offsets. Using an entity-specific
*			bbox (especially 0..height authored bounds) shifts the start/goal centers into
*			the wrong Z layer and causes A* to fail node resolution or traversal.
*/
inline void SVG_TestDummy_GetNavAgentBBox( const svg_monster_testdummy_t *ent, Vector3 *out_mins, Vector3 *out_maxs ) {
	/**
	*	Sanity: require valid pointers.
	**/
	if ( !ent || !out_mins || !out_maxs ) {
		return;
	}

	/**
   *	Prefer the live navmesh agent bounds when available.
    *        This keeps the query hull aligned with the mesh that was generated.
    **/
  const nav_mesh_t *mesh = g_nav_mesh.get();
    const bool meshAgentValid = mesh
        && ( mesh->agent_maxs.z > mesh->agent_mins.z )
        && ( mesh->agent_maxs.x > mesh->agent_mins.x )
        && ( mesh->agent_maxs.y > mesh->agent_mins.y );
    // If the mesh is valid, use its agent bounds directly.
    if ( meshAgentValid ) {
        *out_mins = mesh->agent_mins;
        *out_maxs = mesh->agent_maxs;
        if ( DUMMY_NAV_DEBUG ) {
            gi.dprintf( "[NAV DEBUG] SVG_TestDummy_GetNavAgentBBox: using MESH agent bounds mins=(%.1f %.1f %.1f) maxs=(%.1f %.1f %.1f)\n",
                mesh->agent_mins.x, mesh->agent_mins.y, mesh->agent_mins.z,
                mesh->agent_maxs.x, mesh->agent_maxs.y, mesh->agent_maxs.z );
        }
        return;
    }

    /**
    *    Fallback: resolve the nav agent profile from cvars when the mesh is unavailable.
    **/
    const nav_agent_profile_t agentProfile = SVG_Nav_BuildAgentProfileFromCvars();

    /**
  *	Validate the profile bounds before using them.
    **/
    const bool profileValid = ( agentProfile.maxs.z > agentProfile.mins.z )
        && ( agentProfile.maxs.x > agentProfile.mins.x )
        && ( agentProfile.maxs.y > agentProfile.mins.y );
   // Use the profile bounds when they are valid and we lack mesh data.
    if ( profileValid ) {
        *out_mins = agentProfile.mins;
        *out_maxs = agentProfile.maxs;
        if ( DUMMY_NAV_DEBUG ) {
            gi.dprintf( "[NAV DEBUG] SVG_TestDummy_GetNavAgentBBox: using PROFILE agent bounds mins=(%.1f %.1f %.1f) maxs=(%.1f %.1f %.1f)\n",
                agentProfile.mins.x, agentProfile.mins.y, agentProfile.mins.z,
                agentProfile.maxs.x, agentProfile.maxs.y, agentProfile.maxs.z );
        }
        return;
    }

    /**
    *	Fallback: use entity collision bounds if the profile is invalid.
    **/
    *out_mins = ent->mins;
    *out_maxs = ent->maxs;
    if ( DUMMY_NAV_DEBUG ) {
        gi.dprintf( "[NAV DEBUG] SVG_TestDummy_GetNavAgentBBox: using ENTITY collision bounds mins=(%.1f %.1f %.1f) maxs=(%.1f %.1f %.1f) ent=%d\n",
            ent->mins.x, ent->mins.y, ent->mins.z,
            ent->maxs.x, ent->maxs.y, ent->maxs.z,
            ent ? ent->s.number : -1 );
    }
}

/**
*    @brief	Apply a direct-move fallback toward a specified goal while async pathing runs.
*    @param	self		Monster to move.
*    @param	goal_origin	Target position in entity feet-origin space.
*    @return	True when movement/animation was updated.
*    @note	Uses full 3D direction so steps/slopes remain walkable during fallback pursuit.
**/
bool SVG_TestDummy_ApplyAsyncFallbackPursuit( svg_monster_testdummy_t *self, const Vector3 &goal_origin ) {
    /**
    *    Sanity checks: ensure we have a valid entity to move.
    **/
    if ( !self ) {
        return false;
    }

    /**
    *    Compute goal delta and ensure we have a valid direction to move.
    **/
    Vector3 toGoal = QM_Vector3Subtract( goal_origin, self->currentOrigin );
    const float dist3D = QM_Vector3Length( toGoal );
    if ( dist3D <= 0.001f ) {
        return false;
    }
    Vector3 move_dir = QM_Vector3Normalize( toGoal );

    /**
    *    Animation: use the run animation so motion stays consistent with pursuit.
    **/
    skm_rootmotion_t *rootMotion = self->rootMotionSet ? self->rootMotionSet->motions[ 4 ] : nullptr;
    if ( rootMotion ) {
        self->s.frame = self->ComputeAnimFrameFromRootMotion( rootMotion, 40.0f );
    }

    /**
    *    Velocity: scale speed based on approach distance to avoid overshooting.
    **/
    constexpr float desiredSeparation = 8.0f;
    constexpr float slowDownRange = 64.0f;
    constexpr double desiredAverageSpeed = 220.0;
    const float approachDist = std::max( 0.0f, dist3D - desiredSeparation );
    const float speedScale = ( slowDownRange > 0.0f )
        ? QM_Clamp( approachDist / slowDownRange, 0.0f, 1.0f )
        : 1.0f;
    const double frameVelocity = desiredAverageSpeed * speedScale;

    /**
    *    Facing: keep yaw aligned with movement, blending toward the target direction.
    **/
    {
        const float blendFactor = 0.35f;
        const float yawSpeed = 15.0f;
        self->FaceTargetHorizontal( move_dir, goal_origin, blendFactor, yawSpeed );
    }

    /**
    *    Apply horizontal velocity; vertical movement is handled by step/physics logic.
    **/
    self->velocity.x = ( float )( move_dir.x * frameVelocity );
    self->velocity.y = ( float )( move_dir.y * frameVelocity );
    return true;
}

/**
*    @brief	Reset cached navigation path state for the test dummy.
*    @param	self	Monster whose path state should be cleared.
*    @note	Cancels any queued async request and clears cached path buffers.
**/
void SVG_TestDummy_ResetNavPath( svg_monster_testdummy_t *self ) {
    /**
    *    Sanity check: require a valid entity before touching path state.
    **/
    if ( !self ) {
        return;
    }

    /**
    *    Cancel any pending async request so we do not reuse stale results.
    **/
    if ( self->navPathProcess.pending_request_handle > 0 ) {
        SVG_Nav_CancelRequest( self->navPathProcess.pending_request_handle );
    }

    /**
    *    Clear cached path buffers and reset traversal bookkeeping.
    **/
    SVG_Nav_FreeTraversalPath( &self->navPathProcess.path );
    self->navPathProcess.path_index = 0;
    self->navPathProcess.path_goal = {};
    self->navPathProcess.path_start = {};
    self->navPathProcess.path_center_offset_z = 0.0f;
    self->navPathProcess.rebuild_in_progress = false;
    self->navPathProcess.pending_request_handle = 0;
}

/**
*	@brief	Enqueue a navigation rebuild request when the async queue is enabled.
*	@param	self	Monster owning the path process state.
*	@param	start_origin	Current feet-origin start position.
*	@param	goal_origin	Desired feet-origin goal position.
*	@param	policy	Path-follow policy tuning rebuild heuristics.
*	@param	agent_mins	Feet-origin agent bbox minimums.
*	@param	agent_maxs	Feet-origin agent bbox maximums.
*	@param	force	When true, bypass throttles/heuristics and rebuild immediately.
*	@return	True if the queue accepted the request or already had one pending.
*	@note	When this returns true the path process relies on the queued rebuild instead
*		of immediate synchronous execution so we do not spam blocking calls.
**/
bool SVG_TestDummy_TryQueueNavRebuild( svg_monster_testdummy_t *self, const Vector3 &start_origin,
    const Vector3 &goal_origin, const svg_nav_path_policy_t &policy, const Vector3 &agent_mins,
    const Vector3 &agent_maxs, const bool force = false ) {
    /**
    *    @brief	Attempt to enqueue an asynchronous navigation rebuild for this entity.
    *    @note	Lightweight diagnostic logging is emitted when DUMMY_NAV_DEBUG is enabled
    *
    *    Guard: only enqueue when the async queue mode is explicitly enabled.
    **/
    if ( !self || !nav_nav_async_queue_mode || nav_nav_async_queue_mode->integer == 0 ) {
        if ( DUMMY_NAV_DEBUG ) {
            gi.dprintf( "[DEBUG] TryQueueNavRebuild: async queue mode disabled, cannot enqueue. ent=%d\n", self ? self->s.number : -1 );
        }
        return false;
    }

    if ( !SVG_Nav_IsAsyncNavEnabled() ) {
        if ( DUMMY_NAV_DEBUG ) {
            gi.dprintf( "[DEBUG] TryQueueNavRebuild: async nav globally disabled, ent=%d\n", self->s.number );
        }
        return false;
    }

    /**
    *    Throttle guard:
    *        If the path process is not allowed to rebuild yet, skip enqueuing and
    *        let callers keep following the current path without forcing sync rebuilds.
    **/
    // Force bypass ensures explicit breadcrumb goals always queue new work.
    if ( !force && !self->navPathProcess.CanRebuild( policy ) ) {
        // Movement throttled/backoff prevents enqueuing now; callers should keep using current path.
        if ( DUMMY_NAV_DEBUG ) {
            gi.dprintf( "[DEBUG] TryQueueNavRebuild: CanRebuild() == false, throttled/backoff. ent=%d next_rebuild=%lld backoff_until=%lld\n",
                self->s.number, ( long long )self->navPathProcess.next_rebuild_time.Milliseconds(), ( long long )self->navPathProcess.backoff_until.Milliseconds() );
        }
        return true;
    }

    /**
    *    Movement heuristic:
    *        Only queue rebuilds when the path process thinks goal/start movement
    *        warrants it; this prevents re-queueing every frame for static goals.
    **/
    const bool movementWarrantsRebuild =
        self->navPathProcess.ShouldRebuildForGoal2D( goal_origin, policy )
        || self->navPathProcess.ShouldRebuildForGoal3D( goal_origin, policy )
        || self->navPathProcess.ShouldRebuildForStart2D( start_origin, policy )
        || self->navPathProcess.ShouldRebuildForStart3D( start_origin, policy );
    // Force bypass ensures explicit breadcrumb goals bypass movement heuristics.
    if ( !force && !movementWarrantsRebuild ) {
        if ( DUMMY_NAV_DEBUG ) {
            gi.dprintf( "[DEBUG] TryQueueNavRebuild: movement does not warrant rebuild, ent=%d\n", self->s.number );
        }
        return true;
    }

    /**
    *    Guard: avoid redundant requests when one is already pending for this process.
    **/
    if ( SVG_Nav_IsRequestPending( &self->navPathProcess ) ) {
        if ( DUMMY_NAV_DEBUG ) {
            gi.dprintf( "[DEBUG] TryQueueNavRebuild: request already pending for ent=%d\n", self->s.number );
        }
        return true;
    }

    /**
    *\tEnqueue the rebuild request and record the handle for diagnostics.
    **/
    const nav_request_handle_t handle = SVG_Nav_RequestPathAsync( &self->navPathProcess, start_origin, goal_origin, policy, agent_mins, agent_maxs, force );
    if ( handle <= 0 ) {
        if ( DUMMY_NAV_DEBUG ) {
            gi.dprintf( "[DEBUG] TryQueueNavRebuild: enqueue failed (handle=%d) ent=%d\n", handle, self->s.number );
        }
        return false;
    }

    // Record that a rebuild is in progress for diagnostics and possible cancellation.
    self->navPathProcess.rebuild_in_progress = true;
    self->navPathProcess.pending_request_handle = handle;
    if ( DUMMY_NAV_DEBUG ) {
        gi.dprintf( "[DEBUG] TryQueueNavRebuild: queued rebuild handle=%d ent=%d force=%d\n", handle, self->s.number, force ? 1 : 0 );
        // Also print the converted nav-center origins so we can correlate node resolution.
        const nav_mesh_t *mesh = g_nav_mesh.get();
        if ( mesh ) {
            const Vector3 start_center = SVG_Nav_ConvertFeetToCenter( mesh, start_origin, &agent_mins, &agent_maxs );
            const Vector3 goal_center = SVG_Nav_ConvertFeetToCenter( mesh, goal_origin, &agent_mins, &agent_maxs );
            gi.dprintf( "[DEBUG] TryQueueNavRebuild: start=(%.1f %.1f %.1f) start_center=(%.1f %.1f %.1f) goal=(%.1f %.1f %.1f) goal_center=(%.1f %.1f %.1f)\n",
                start_origin.x, start_origin.y, start_origin.z,
                start_center.x, start_center.y, start_center.z,
                goal_origin.x, goal_origin.y, goal_origin.z,
                goal_center.x, goal_center.y, goal_center.z );
        } else {
            gi.dprintf( "[DEBUG] TryQueueNavRebuild: start=(%.1f %.1f %.1f) goal=(%.1f %.1f %.1f) (no mesh)\n",
                start_origin.x, start_origin.y, start_origin.z,
                goal_origin.x, goal_origin.y, goal_origin.z );
        }
    }
    return true;
}

// -----------------------------------------------------------------
// Static helpers (translation-unit local)
// -----------------------------------------------------------------
//! Per-entity "current trail spot" cache for stable trail following.
//! This avoids calling PickFirst every frame (which can cause oscillation).
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
	self->viewheight = DUMMY_VIEWHEIGHT_STANDUP;
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
	if ( rootMotionSet && rootMotionSet->motions[ 1 ] ) {
		// Animation: idle anim frame selection centralized.
		skm_rootmotion_t *rootMotion = rootMotionSet->motions[ 1 ]; // IDLE
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
    *    Update cached goal visibility state so LOS flips invalidate old paths.
    **/
    if ( goalentity ) {
        // Record the current goal origin for future path invalidation checks.
        last_nav_goal_origin = goalentity->currentOrigin;
        // Mark that we are currently in a visible goal state.
        last_nav_goal_visible = true;
        // Flag that we have a valid goal snapshot.
        last_nav_goal_valid = true;
    }

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
	skm_rootmotion_t *rootMotion = rootMotionSet->motions[ 4 ]; // RUN_FORWARD_PISTOL
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
	if ( approachDist <= 0.0f && std::fabs( toGoal.z ) < 8.0f ) {
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
	/**
	*	Sanity: cannot pathfind without a valid goal entity.
	**/
	if ( !goalentity ) {
		//gi.dprintf("[DEBUG] ThinkAStarToPlayer: No goalentity, cannot path.\n");
		return false;
	}

 /**
    *	Goal selection:
    *		Prefer breadcrumb goals when the target is out of LOS to avoid
    *		pathing directly into unreachable tiles on another floor.
    **/
    Vector3 goalOrigin = goalentity->currentOrigin;
    bool usingTrailGoal = false;
    // Track whether we have direct LOS so we can decide when to clamp or trail.
    const bool targetVisible = SVG_Entity_IsVisible( this, goalentity );
    {
        // If we do not see the target, prefer a trail spot to keep routes on valid layers.
     if ( !targetVisible ) {
            svg_base_edict_t *trailSpot = PlayerTrail_PickFirst( this );
            /**
            *    Trail validity:
            *        Ignore uninitialized trail slots so we do not chase (0,0,0).
            **/
         /**
            *    Trail freshness:
            *        Allow a short grace window so we can still follow the last known
            *        breadcrumb chain if LOS drops before a newer trail point appears.
            **/
            const QMTime trailCutoff = ( trail_time > 0_ms ) ? ( trail_time - 500_ms ) : 0_ms;
            if ( trailSpot && trailSpot->timestamp > trailCutoff ) {
                goalOrigin = trailSpot->currentOrigin;
                usingTrailGoal = true;
            }
        }
    }

    /**
	If the goal is on another floor (Z gap beyond our step height), avoid aiming
	A* directly at an unreachable Z unless we have a validated trail goal that can
	steer us toward stairs.
    **/
    {
        // Determine our effective step capability using the configured navigation policy.
        const float stepLimit = ( navPathPolicy.max_step_height > 0.0f )
            ? ( float )navPathPolicy.max_step_height
            : 0.0f;
        // Compare vertical gap against the step limit.
        const float zGapAbs = std::fabs( goalOrigin.z - currentOrigin.z );
        /**
        *    Visible target handling:
        *        When the target is visible but sits above our step limit, keep the goal
        *        on our current Z so we can search for stairs while still using the
        *        target's XY position.
        **/
        if ( targetVisible && !usingTrailGoal && zGapAbs > ( stepLimit + 1.0f ) ) {
            // Clamp goal Z to our current Z so A* seeks a valid layer near the target XY.
            goalOrigin.z = currentOrigin.z;
        }
        /**
        *    Occluded target handling:
        *        When we cannot see the target, only clamp to the current Z if the
        *        vertical gap exceeds our safe traversal envelope. This avoids forcing
        *        A* onto the wrong Z layer for gentle slopes or walkable ramps.
        **/
        const float maxOccludedZGap = ( navPathPolicy.max_drop_height > 0.0f )
            ? ( float )navPathPolicy.max_drop_height
            : ( stepLimit + 1.0f );
        // Clamp only when the target is far above/below our traversable range.
        if ( !targetVisible && zGapAbs > maxOccludedZGap ) {
            // Clamp goal Z to our current Z so A* seeks a staircase/connection first.
            goalOrigin.z = currentOrigin.z;
        }
    }

	/**
	*	If the goal is well above our current floor and we have not already adopted a
	*	trail target, fall back to breadcrumb navigation.
	**/
   {
        // Only re-evaluate trail goals when the target is not visible and we are still using the raw target.
        if ( !usingTrailGoal && !targetVisible ) {
          const float zGapAbs = std::fabs( goalOrigin.z - currentOrigin.z );
            // Only switch to breadcrumbs when the target is significantly above us.
            constexpr float trailFallbackGap = 96.0f;
            if ( zGapAbs > trailFallbackGap ) {
                svg_base_edict_t *trailSpot = PlayerTrail_PickFirst( this );
                if ( trailSpot && trailSpot->timestamp > 0_ms ) {
                    goalOrigin = trailSpot->currentOrigin;
					usingTrailGoal = true;
                }
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
    // Tight waypoint radius keeps selection on the correct layer while navigating stairs.
    navPathPolicy.waypoint_radius = 32.0f;
    // Modest goal-change thresholds avoid unnecessary rebuild spam.
    navPathPolicy.rebuild_goal_dist_2d = 32.0;
    // Allow larger 3D goal movement before rebuild to stabilize upstairs pursuit.
    navPathPolicy.rebuild_goal_dist_3d = 32.0f;
    // Use the baseline rebuild interval to keep A* attempts steady.
    navPathPolicy.rebuild_interval = 100_ms;
    // Backoff retries so repeated failures do not stall the frame.
    navPathPolicy.fail_backoff_base = 25_ms;
    navPathPolicy.fail_backoff_max_pow = 3;

    // Ensure step and drop safety parameters match intended agent capabilities.
    // Allow stepping up to 18 units and cap drops to 128 units when following paths.
    navPathPolicy.max_step_height = 18.0;
    navPathPolicy.max_drop_height = 128.0;
    navPathPolicy.cap_drop_height = true;
    navPathPolicy.drop_cap = 64.0;

    // Goal Z-layer blending: bias layer selection toward goal Z quickly so the
    // pathfinder will prefer climbing stairs/up to the target when appropriate.
    navPathPolicy.enable_goal_z_layer_blend = true;
    navPathPolicy.blend_start_dist = 18.0f;
    navPathPolicy.blend_full_dist = 64.0f;

    // Small obstruction jump tuning: allow small jumps over low obstacles.
    navPathPolicy.allow_small_obstruction_jump = true;
    navPathPolicy.max_obstruction_jump_height = 36.0;

    /**
    *    Path invalidation:
    *        When the goal moves significantly or LOS flips, clear the cached path
    *        so we do not reuse stale waypoints from a previous pursuit.
    **/
    {
        /**
        *    Determine whether a cached path exists before evaluating invalidation.
        **/
        const bool hasCachedPath = ( navPathProcess.path.num_points > 0 && navPathProcess.path.points );
        if ( hasCachedPath ) {
            // Compute goal delta against the cached path goal.
            const Vector3 goalDelta = QM_Vector3Subtract( goalOrigin, navPathProcess.path_goal );
            // Compare 2D and 3D deltas against rebuild thresholds.
            const float goalDelta2D = ( goalDelta.x * goalDelta.x ) + ( goalDelta.y * goalDelta.y );
            const float goalDelta3D = goalDelta2D + ( goalDelta.z * goalDelta.z );
            // Use policy thresholds to decide what counts as a significant move.
            const float goalThreshold2D = navPathPolicy.rebuild_goal_dist_2d;
            const float goalThreshold3D = navPathPolicy.rebuild_goal_dist_3d > 0.0f
                ? navPathPolicy.rebuild_goal_dist_3d
                : 0.0f;
            // Treat visibility flips as a reason to discard cached paths.
            const bool visibilityFlipped = last_nav_goal_valid && ( targetVisible != last_nav_goal_visible );
            // Check if the goal moved beyond the configured thresholds.
            const bool goalMoved2D = ( goalThreshold2D > 0.0f ) && ( goalDelta2D > ( goalThreshold2D * goalThreshold2D ) );
            const bool goalMoved3D = ( goalThreshold3D > 0.0f ) && ( goalDelta3D > ( goalThreshold3D * goalThreshold3D ) );
            if ( goalMoved2D || goalMoved3D || visibilityFlipped ) {
                // Clear cached path data so future queries rebuild against the new goal.
                SVG_TestDummy_ResetNavPath( this );
            }
        }

        /**
        *    Record the current goal/visibility for the next update.
        **/
        last_nav_goal_origin = goalOrigin;
        last_nav_goal_visible = targetVisible;
        last_nav_goal_valid = true;
    }

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

    /**
    *    Async-only policy:
    *        The test dummy should only use queued A* rebuilds to avoid synchronous stalls.
    **/
    // Determine if async queue processing is enabled.
    const bool asyncQueueEnabled = SVG_Nav_IsAsyncNavEnabled()
        && nav_nav_async_queue_mode
        && nav_nav_async_queue_mode->integer != 0;
    // Track whether we successfully queued a rebuild request.
    const bool queuedRebuild = asyncQueueEnabled
        ? SVG_TestDummy_TryQueueNavRebuild( this, currentOrigin, goalOrigin, navPathPolicy, agent_mins, agent_maxs )
        : false;
    ( void )queuedRebuild;

    /**
    *    Track whether an async request is still in flight for this path process.
    **/
    const bool requestPending = asyncQueueEnabled && SVG_Nav_IsRequestPending( &navPathProcess );

    /**
    *    Path rebuild result:
    *        When async is enabled, rely on cached path data only. If async is disabled,
    *        report failure so the caller falls back instead of running sync A*.
    **/
    const bool pathOk = asyncQueueEnabled
        ? ( navPathProcess.path.num_points > 0 && navPathProcess.path.points )
        : false;

    Vector3 move_dir = {};
    // Guard: only query movement direction when a valid path buffer exists.
    bool hasPathDir = false;
    if ( pathOk ) {
        hasPathDir = navPathProcess.QueryDirection3D( currentOrigin, navPathPolicy, &move_dir );
    }

    Vector3 nextNavPoint_feet = {};
    Vector3 nextNavPoint_nav = {};
    bool hasNextNavPoint = false;
    // Only attempt to inspect the next path point when a path exists and the
    // stored index is within the path bounds.
    const bool nextPointIndexValid = ( navPathProcess.path.num_points > 0 && navPathProcess.path.points
        && navPathProcess.path_index >= 0 && navPathProcess.path_index < navPathProcess.path.num_points );
    if ( nextPointIndexValid ) {
        hasNextNavPoint = navPathProcess.GetNextPathPointEntitySpace( &nextNavPoint_feet );
        if ( hasNextNavPoint ) {
            nextNavPoint_nav = navPathProcess.path.points[ navPathProcess.path_index ];
            const Vector3 derivedNavPointNavSpace = QM_Vector3Add( nextNavPoint_feet, Vector3{ 0.0f, 0.0f, navPathProcess.path_center_offset_z } );
            const float navPointMismatch = QM_Vector3Length( QM_Vector3Subtract( nextNavPoint_nav, derivedNavPointNavSpace ) );
            constexpr float kNavPointMatchTolerance = 0.25f;
            if ( DUMMY_NAV_DEBUG ) {
                gi.dprintf( "[DEBUG] ThinkAStarToPlayer: Next nav point nav(%.1f %.1f %.1f) feet(%.1f %.1f %.1f) mismatch=%.2f\n",
                    nextNavPoint_nav.x, nextNavPoint_nav.y, nextNavPoint_nav.z,
                    nextNavPoint_feet.x, nextNavPoint_feet.y, nextNavPoint_feet.z,
                    navPointMismatch );
            }
            if ( navPointMismatch > kNavPointMatchTolerance ) {
                hasNextNavPoint = false;
                if ( DUMMY_NAV_DEBUG ) {
                    gi.dprintf( "[DEBUG] ThinkAStarToPlayer: Nav mismatch (%.2f) exceeds tolerance %.2f, invalidating waypoint.\n",
                        navPointMismatch, kNavPointMatchTolerance );
                }
            }
        }
 } else if ( DUMMY_NAV_DEBUG && pathOk ) {
        /**
        *    Debug: only warn about out-of-range indices when a path buffer exists.
        **/
        gi.dprintf( "[DEBUG] ThinkAStarToPlayer: QueryDirection3D advanced path_index out of range (%d/%d) after retrieval.\n",
            navPathProcess.path_index, navPathProcess.path.num_points );
    }

    if ( DUMMY_NAV_DEBUG ) {
        gi.dprintf( "[DEBUG] ThinkAStarToPlayer: Monster %d pathOk=%d hasPathDir=%d from (%.1f %.1f %.1f) to (%.1f %.1f %.1f)\n",
            s.number, pathOk ? 1 : 0, hasPathDir ? 1 : 0,
            currentOrigin.x, currentOrigin.y, currentOrigin.z,
            goalOrigin.x, goalOrigin.y, goalOrigin.z );
    }

    /**
    *    Path-query result:
    *        If we failed to rebuild or query the path direction, fall back to LOS logic.
    **/
    const bool canFollowPath = pathOk && hasPathDir && hasNextNavPoint;
 if ( !canFollowPath ) {
        /**
        *    Async rebuild fallback:
        *        While the async search is still running, keep advancing toward the
        *        goal so the monster does not stall on LOS breaks.
        **/
        if ( requestPending ) {
            // Apply a direct-move fallback so we keep pressure while waiting for A*.
            if ( SVG_TestDummy_ApplyAsyncFallbackPursuit( this, goalOrigin ) ) {
                return true;
            }
        }
		/**
		*    Fallback:
		*        If we can see the goal (or it's in front), prefer direct pursuit rather than
		*        idling when A* momentarily fails on vertical transitions.
		**/
		/**
		* 	Fallback gating:
		* 		Only switch to direct pursuit when the target is actually visible
		* 		and roughly in front. This avoids oscillating to LOS pursuit when
		* 		pathing is still valid but direction queries temporarily fail.
		**/
		// Do not fallback to direct-pursuit when goal is substantially above us; we need stairs.
		const float zGapAbs = std::fabs( goalentity->currentOrigin.z - currentOrigin.z );
		// Allow direct pursuit within the configured drop limit when we can see the goal.
		const float maxDirectPursuitGap = ( navPathPolicy.max_drop_height > 0.0f )
			? ( float )navPathPolicy.max_drop_height
			: 64.0f;
		// Require actual visibility and in-front checks for LOS fallback.
		const bool canSeeGoal = goalentity && SVG_Entity_IsVisible( this, goalentity );
		const bool inFrontOfGoal = goalentity && SVG_Entity_IsInFrontOf( this, goalentity, { 1.0f, 1.0f, 0.0f }, 0.35 );
		if ( zGapAbs <= maxDirectPursuitGap && canSeeGoal && inFrontOfGoal ) {
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
    *    Use the already-converted next waypoint when computing vertical deltas and speed.
    **/
    float zDelta = std::fabs( nextNavPoint_feet.z - currentOrigin.z );
	// Threshold under which we treat the waypoint as same step level
	constexpr float zStepThreshold = 8.0f;
	// Base movement speed used for frame velocity calculations.
	double frameVelocity = 220.0;
	//// If the target is roughly on the same horizontal plane, compute slowdown.
	//if ( zDelta < zStepThreshold ) {
	//	// Vector from us to the nav waypoint.
	//	Vector3 toGoal = QM_Vector3Subtract( nextNavPoint_feet, currentOrigin );
	//	// Horizontal distance to the waypoint.
	//	float dist2D = std::sqrt( toGoal.x * toGoal.x + toGoal.y * toGoal.y );
	//	// Desired separation so we don't clip into the target.
	//	constexpr float desiredSeparation = 8.0f;
	//	// Distance remaining until desired separation.
	//	float approachDist = std::max( 0.0f, dist2D - desiredSeparation );
	//	// Range over which to slow down when approaching.
	//	constexpr float slowDownRange = 64.0f;
	//	// Scale speed between 0..1 based on approach distance.
	//	float speedScale = ( slowDownRange > 0.0f )
	//		? QM_Clamp( approachDist / slowDownRange, 0.0f, 1.0f )
	//		: 1.0f;
	//	// Apply slowdown.
	//	frameVelocity *= speedScale;
	//	// If we're effectively at the target, stop.
	//	if ( approachDist <= 0.0f ) {
	//		frameVelocity = 0.0f;
	//	}
	//}


    if ( DUMMY_NAV_DEBUG ) {
        gi.dprintf( "[DEBUG] ThinkAStarToPlayer: Next nav point feet(%.1f %.1f %.1f) zDelta=%.1f\n",
            nextNavPoint_feet.x, nextNavPoint_feet.y, nextNavPoint_feet.z, zDelta );
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
   if ( !trailSpot ) {
        gi.dprintf( "[DEBUG] ThinkFollowTrail: No trail spot found for monster %d\n", s.number );
        return false;
    }
    /**
    *    Trail validity:
    *        Ignore empty trail slots so we do not path toward the origin.
    **/
    if ( trailSpot->timestamp <= 0_ms ) {
        gi.dprintf( "[DEBUG] ThinkFollowTrail: Trail spot uninitialized for monster %d\n", s.number );
        return false;
    }

	Vector3 goalOrigin = trailSpot->currentOrigin;

	/**
	*	Derive the correct nav-agent bbox (feet-origin) for traversal.
	**/
	Vector3 agent_mins = {};
	Vector3 agent_maxs = {};
	SVG_TestDummy_GetNavAgentBBox( this, &agent_mins, &agent_maxs );

    /**
    *    Trail-following: update nav policy for breadcrumb following and attempt to rebuild a path.
    **/
	navPathPolicy.ignore_visibility = true;
	navPathPolicy.ignore_infront = true;
	
  // Tight waypoint radius keeps trail-following aligned to stair layers.
    navPathPolicy.waypoint_radius = 32.0f;
   // Tight horizontal threshold keeps trail goals responsive when the player moves.
    navPathPolicy.rebuild_goal_dist_2d = 32.0f;
    // Moderate 3D threshold avoids noisy rebuilds due to small Z offsets.
    navPathPolicy.rebuild_goal_dist_3d = 32.0f;
    // Short rebuild interval keeps trail-following reactive to small goal shifts.
    navPathPolicy.rebuild_interval = 100_ms;
    // Short backoff keeps retries responsive when stairs are present.
    navPathPolicy.fail_backoff_base = 25_ms;
	navPathPolicy.fail_backoff_max_pow = 3;

	// Ensure step and drop safety parameters for trail-following match agent.
	// This allows stepping up to 18 units and caps allowed drops to 128 units.
	navPathPolicy.max_step_height = 18.0;
	navPathPolicy.max_drop_height = 128.0;
	navPathPolicy.cap_drop_height = true;
	navPathPolicy.drop_cap = 64.0;

    // Bias layer selection toward goal Z for trail-following so the monster will
    // attempt to climb stairs to reach breadcrumb spots on higher floors.
 navPathPolicy.enable_goal_z_layer_blend = true;
    navPathPolicy.blend_start_dist = 18.0f;
    navPathPolicy.blend_full_dist = 64.0f;

    /**
    *	Keep trail goal Z within our step capability so RebuildPathToWithAgentBBox
    *	only routes toward reachable layers instead of failing early on high-ups.
    **/
    {
        const float stepLimit = ( navPathPolicy.max_step_height > 0.0f )
            ? ( float )navPathPolicy.max_step_height
            : 0.0f;
        const float zGapAbs = std::fabs( goalOrigin.z - currentOrigin.z );
        if ( zGapAbs > ( stepLimit + 1.0f ) ) {
            goalOrigin.z = currentOrigin.z;
        }
    }

    navPathPolicy.allow_small_obstruction_jump = true;
    navPathPolicy.max_obstruction_jump_height = 36.0;

    // Convert entity feet-origin bbox to navmesh-centered bbox/origins.
    // Call nav path process using entity origin; svg_nav_path_process will handle conversions.
    const bool queuedRebuild = SVG_TestDummy_TryQueueNavRebuild( this, currentOrigin, goalOrigin, navPathPolicy, agent_mins, agent_maxs );
    ( void )queuedRebuild;

    /**
    *    Track whether an async request is still in flight for this trail rebuild.
    **/
    const bool requestPending = SVG_Nav_IsAsyncNavEnabled() && SVG_Nav_IsRequestPending( &navPathProcess );

    /**
    *    Path rebuild result:
    *        Rely exclusively on the queued async path so the TestDummy never stalls 
    *        on synchronous rebuilds during trail following.
    **/
    const bool pathOk = ( navPathProcess.path.num_points > 0 && navPathProcess.path.points );

    Vector3 move_dir = {};
    bool hasPathDir = navPathProcess.QueryDirection3D(currentOrigin, navPathPolicy, &move_dir);

	if ( DUMMY_NAV_DEBUG ) {
		gi.dprintf("[DEBUG] ThinkFollowTrail: Monster %d pathOk=%d hasPathDir=%d from (%.1f %.1f %.1f) to (%.1f %.1f %.1f)\n",
			s.number, pathOk ? 1 : 0, hasPathDir ? 1 : 0,
			currentOrigin.x, currentOrigin.y, currentOrigin.z,
			goalOrigin.x, goalOrigin.y, goalOrigin.z);
	}

    if ( !pathOk || !hasPathDir ) {
        /**
        *    Async rebuild fallback:
        *        While the trail path is still generating, keep moving toward the
        *        breadcrumb goal so we do not stall in place.
        **/
        if ( requestPending ) {
            // Apply a direct-move fallback toward the trail spot while waiting.
            if ( SVG_TestDummy_ApplyAsyncFallbackPursuit( this, goalOrigin ) ) {
                return true;
            }
        }
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
    constexpr float zStepThreshold = 8.0f;
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
    if ( pathOk && hasPathDir && trailSpot && trailSpot->timestamp > 0_ms ) {
        svg_base_edict_t *&cachedSpot = s_dummyTrailSpot[ s.number ];
      const bool acceptedNewTrail = ( cachedSpot != trailSpot )
            || ( cachedSpot && cachedSpot->timestamp != trailSpot->timestamp );
        cachedSpot = trailSpot;
        if ( acceptedNewTrail ) {
            trail_time = trailSpot->timestamp;
            /**
            *    Accepting a new breadcrumb invalidates the current path so the
            *    nav system does not reuse stale data while the forced rebuild runs.
            **/
            SVG_Nav_FreeTraversalPath( &navPathProcess.path );
            // Ensure index reset when clearing path to avoid out-of-range queries.
            navPathProcess.path_index = 0;
            navPathProcess.path_goal = {};
            navPathProcess.path_start = {};
            navPathProcess.path_center_offset_z = 0.0f;
            navPathProcess.rebuild_in_progress = false;
            navPathProcess.pending_request_handle = 0;
          const bool queuedForcedRebuild = SVG_TestDummy_TryQueueNavRebuild( this, currentOrigin, goalOrigin, navPathPolicy, agent_mins, agent_maxs, true );
            ( void )queuedForcedRebuild;
            return false;
        }
    }
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
            if ( DUMMY_NAV_DEBUG ) {
                gi.dprintf( "[DEBUG] onThink: Monster %d deciding pursuit (activatorPresent=%d visible=%d).\n",
                    self->s.number, hasActivator ? 1 : 0, activatorVisible ? 1 : 0 );
            }
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
                if ( DUMMY_NAV_DEBUG ) {
                    gi.dprintf( "[DEBUG] onThink: Monster %d using direct pursuit (stable_visible=%d), skipping A*.\n", self->s.number, stable_visible ? 1 : 0 );
                }
                self->last_pursuit_was_direct = true;
                self->ThinkDirectPursuit();
                pursuing = true;
            }
            // If we've stably been hidden long enough, prefer A*/trail logic.
            else if ( stable_hidden ) {
                if ( DUMMY_NAV_DEBUG ) {
                    gi.dprintf( "[DEBUG] onThink: Monster %d attempting A* toward goal (stable_hidden=%d).\n", self->s.number, stable_hidden ? 1 : 0 );
                }
                self->last_pursuit_was_direct = false;
                pursuing = self->ThinkAStarToPlayer();
                if ( !pursuing ) {
                    if ( DUMMY_NAV_DEBUG ) {
                        gi.dprintf( "[DEBUG] onThink: Monster %d A* failed, attempting trail follow.\n", self->s.number );
                    }
                    pursuing = self->ThinkFollowTrail();
                    if ( !pursuing ) {
                        if ( DUMMY_NAV_DEBUG ) {
                            gi.dprintf( "[DEBUG] onThink: Monster %d failed all pursuit, stopping movement and idling this frame (will retry next frame).\n", self->s.number );
                        }
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
                        if ( DUMMY_NAV_DEBUG ) {
                            gi.dprintf( "[DEBUG] onThink: Monster %d LOS lost, attempting A*/trail despite direct hysteresis.\n", self->s.number );
                        }
                        pursuing = self->ThinkAStarToPlayer();
                        if ( !pursuing ) {
                            pursuing = self->ThinkFollowTrail();
                        }
                    } else {
                        if ( DUMMY_NAV_DEBUG ) {
                            gi.dprintf( "[DEBUG] onThink: Monster %d continuing previous direct pursuit mode (hysteresis).\n", self->s.number );
                        }
                        self->ThinkDirectPursuit();
                        pursuing = true;
                    }
                } else {
                    if ( DUMMY_NAV_DEBUG ) {
                        gi.dprintf( "[DEBUG] onThink: Monster %d continuing previous A*/trail mode (hysteresis).\n", self->s.number );
                    }
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
			/**
			*	Determine the drop allowance derived from the path policy so we respect the drop cap.
			**/
			const float policyDropLimit = ( self->navPathPolicy.drop_cap > 0.0f )
				? ( float )self->navPathPolicy.drop_cap
				: ( self->navPathPolicy.cap_drop_height ? ( float )self->navPathPolicy.max_drop_height : 0.0f );
			if ( tr.fraction < 1.0f && policyDropLimit > 0.0f && drop > policyDropLimit ) {
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
                skm_rootmotion_t *rootMotion = self->rootMotionSet->motions[ 4 ]; // [1] == RUN_FORWARD_PISTOL
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
