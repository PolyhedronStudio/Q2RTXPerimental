/********************************************************************
*
*
*	ServerGame: TestDummy Monster Edict
*	NameSpace: "".
*
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

// TestDummy Monster
#include "svgame/entities/monster/svg_monster_testdummy.h"

// Per-entity "current trail spot" cache for stable trail following.
// This avoids calling PickFirst every frame (which can cause oscillation).
#include <unordered_map>
static std::unordered_map<int32_t, svg_base_edict_t*> s_dummyTrailSpot;



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
    
    SVG_Nav_FreeTraversalPath( &navPath );
    navPathIndex = 0;
    navPathGoal = {};
    navPathNextRebuildTime = 0_ms;
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
    
    SVG_Nav_FreeTraversalPath( &navPath );
    navPathIndex = 0;
    navPathGoal = {};
    navPathNextRebuildTime = 0_ms;
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

	self->s.entityType = ET_MONSTER;

	self->solid = SOLID_BOUNDS_BOX;
	self->movetype = MOVETYPE_WALK;

	self->model = svg_level_qstring_t::from_char_str( "models/characters/mixadummy/tris.iqm" );
	self->s.modelindex = gi.modelindex( self->model );
	const char *modelname = self->model;
	const model_t *model_forname = gi.GetModelDataForName( modelname );
	self->rootMotionSet = &model_forname->skmConfig->rootMotion;

	VectorCopy( svg_monster_testdummy_t::DUMMY_BBOX_STANDUP_MINS, self->mins );
	VectorCopy( svg_monster_testdummy_t::DUMMY_BBOX_STANDUP_MAXS, self->maxs );

	self->mass = 200;

	if ( !self->health ) {
		self->health = 200;
	}
	if ( !self->dmg ) {
		self->dmg = 150;
	}
	if ( !self->gravity ) {
		self->gravity = 1.0f;
	}

	self->svFlags &= ~SVF_DEADENTITY;
	self->svFlags |= SVF_MONSTER;

	self->s.skinnum = 0;

	self->takedamage = DAMAGE_AIM;

	self->air_finished_time = level.time + 12_sec;
	self->max_health = self->health;

	self->clipMask = CM_CONTENTMASK_MONSTERSOLID;

	self->takedamage = DAMAGE_YES;
	self->lifeStatus = LIFESTATUS_ALIVE;

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
*   @brief  Thinking routine.
**/
DEFINE_MEMBER_CALLBACK_THINK( svg_monster_testdummy_t, onThink )( svg_monster_testdummy_t *self ) -> void {
	self->s.renderfx &= ~( RF_STAIR_STEP | RF_OLD_FRAME_LERP );

	// Refresh ground + liquid every frame so movement code has correct state.
	{
		const cm_contents_t mask = SVG_GetClipMask( self );
		M_CheckGround( self, mask );
		M_CatagorizePosition( self, self->currentOrigin, self->liquidInfo.level, self->liquidInfo.type );
	}

	self->testVar = level.frameNumber;

	if ( self->health > 0 ) {
		Vector3 preSumOrigin = self->currentOrigin;
		Vector3 previousVelocity = self->velocity;

		if ( self->activator && self->activator->client ) {
			// Always follow the activator (player) while activated.
			self->goalentity = self->activator;

			// Root motion anim (still fine to keep for footsteps/frame stepping).
			skm_rootmotion_t *rootMotion = self->rootMotionSet->motions[ 3 ]; // RUN_FORWARD_PISTOL

			const bool isMoving2D = ( std::fabs( self->velocity.x ) > 1.0f ) || ( std::fabs( self->velocity.y ) > 1.0f );
			if ( isMoving2D ) {
				// Advance the walk animation based on time, not on think calls.
				// Debug drawing can add enough load to make think timing irregular, which shows up as frame jitter.
				const float animHz = 40.0f;
				const double t = level.time.Seconds<double>();
				const int32_t animFrame = (int32_t)floorf( (float)( t * animHz ) );
				const int32_t localFrame = ( rootMotion->frameCount > 0 ) ? ( animFrame % rootMotion->frameCount ) : 0;
				self->s.frame = rootMotion->firstFrameIndex + localFrame;
			} else {
				self->s.frame = rootMotion->firstFrameIndex;
			}

			const int32_t rootMotionFrame = self->s.frame - rootMotion->firstFrameIndex;

			constexpr double desiredAverageSpeed = 220.0;

			Vector3 rawTranslation = *rootMotion->translations[ rootMotionFrame ];
			const double rawFrameDistance = std::sqrt( rawTranslation.x * rawTranslation.x + rawTranslation.y * rawTranslation.y );

			double frameVelocity = 0.0;
			if ( rawFrameDistance > 0.0 ) {
				frameVelocity = desiredAverageSpeed;

				if ( rootMotion->totalDistance > 0.0 ) {
					const double avgFrameDistance = rootMotion->totalDistance / rootMotion->frameCount;
					double frameRatio = rawFrameDistance / avgFrameDistance;
					frameRatio = QM_Clamp( frameRatio, 0.1, 3.0 );
					frameVelocity = desiredAverageSpeed * frameRatio;
				}
			}

			if ( rootMotionFrame == 0 ) {
				SVG_Util_AddEvent( self, EV_OTHER_FOOTSTEP, 0 );
			} else if ( rootMotionFrame == 15 ) {
				SVG_Util_AddEvent( self, EV_OTHER_FOOTSTEP, 0 );
			}

			if ( !g_nav_mesh ) {
				SVG_Nav_GenerateVoxelMesh();
			}
 
			// Prefer direct pursuit when we can see the player; otherwise follow the player trail.
			const bool canSeeGoalEntity = SVG_Entity_IsVisible( self->goalentity, self );
			if ( canSeeGoalEntity ) {
				// "last time we saw the player"
				self->trail_time = level.time;
			}
 
			const Vector3 playerOrigin = self->goalentity->currentOrigin;
			Vector3 goalOrigin = playerOrigin;
 
			// If we're not seeing the player, chase the trail and advance it *persistently*.
			if ( !canSeeGoalEntity ) {
				const int32_t selfNum = self->s.number;
				svg_base_edict_t *&trailSpot = s_dummyTrailSpot[ selfNum ];

				// Ensure we have an initial trail spot.
				if ( !trailSpot || !SVG_Entity_IsActive( trailSpot ) ) {
					trailSpot = PlayerTrail_PickFirst( self );
				}

				if ( trailSpot ) {
					// Pick a trail marker that best matches the player's current Z/floor.
					// This avoids going astray in multi-floor areas where multiple trail points share XY.
					svg_base_edict_t *best = trailSpot;
					float bestScore = std::numeric_limits<float>::max();

					svg_base_edict_t *candidate = trailSpot;
					for ( int32_t i = 0; i < 8 && candidate; ++i ) {
						const float dzToPlayer = std::fabs( candidate->currentOrigin.z - playerOrigin.z );
						const float dzToMonster = std::fabs( candidate->currentOrigin.z - self->currentOrigin.z );

						// Prefer markers close to the player's Z. If the player is above us, slightly prefer
						// markers that are also above us to encourage climbing instead of wandering below.
						float score = dzToPlayer + 0.25f * dzToMonster;
						if ( playerOrigin.z > self->currentOrigin.z && candidate->currentOrigin.z < self->currentOrigin.z ) {
							score += 128.0f;
						}

						if ( score < bestScore ) {
							bestScore = score;
							best = candidate;
						}

						candidate = PlayerTrail_PickNext( self );
					}

					trailSpot = best;
					goalOrigin = trailSpot->currentOrigin;

					// Advance along trail when close enough in 3D.
					// Using 2D only causes "premature" advancement on stairs (same XY, very different Z).
					const Vector3 toTrail = QM_Vector3Subtract( goalOrigin, self->currentOrigin );
					const float toTrail3DSqr = ( toTrail.x * toTrail.x ) + ( toTrail.y * toTrail.y ) + ( toTrail.z * toTrail.z );

					constexpr float trailAdvanceDist = 24.0f;
					if ( toTrail3DSqr <= ( trailAdvanceDist * trailAdvanceDist ) ) {
						svg_base_edict_t *next = PlayerTrail_PickNext( self );
						if ( next ) {
							trailSpot = next;
							goalOrigin = trailSpot->currentOrigin;
						}
					}
				}
			} else {
				// If we can see the player again, clear trail target so we re-acquire next time LOS breaks.
				s_dummyTrailSpot.erase( self->s.number );
			}
   
  			const Vector3 toGoal = QM_Vector3Subtract( goalOrigin, self->currentOrigin );
  			const float dist2DSqr = ( toGoal.x * toGoal.x ) + ( toGoal.y * toGoal.y );
			const float dist2D = std::sqrt( dist2DSqr );
			const float zDist = std::fabs( toGoal.z );
 
			// Separation-aware stop: get as close as 8 units, then halt.
			// Using player origin directly often stops "too far" due to bbox collision/overshoot.
			constexpr float desiredSeparation = 8.0f;
			const float approachDist = std::max( 0.0f, dist2D - desiredSeparation );

			// Ramp speed down when approaching the final 64 units to avoid jitter.
			constexpr float slowDownRange = 64.0f;
			const float speedScale = ( slowDownRange > 0.0f )
				? QM_Clamp( approachDist / slowDownRange, 0.0f, 1.0f )
				: 1.0f;

			frameVelocity *= speedScale;
			// If we're basically aligned in XY but still far in Z (stairs/elevators),
			// don't clamp speed to 0 or we'll stall below/above the target.
			if ( approachDist <= 0.0f && zDist < 32.0f ) {
				frameVelocity = 0.0f;
			}
 
			// Rebuild less aggressively to avoid oscillating between nearby solutions.
			constexpr float navRebuildDist = 96.0f;
			const QMTime navRebuildInterval = 400_ms;
			const Vector3 navGoalDelta = QM_Vector3Subtract( goalOrigin, self->navPathGoal );
			const float navGoalDistSqr = ( navGoalDelta.x * navGoalDelta.x ) + ( navGoalDelta.y * navGoalDelta.y );
 
			if ( level.time >= self->navPathNextRebuildTime && ( self->navPath.num_points == 0 || navGoalDistSqr > ( navRebuildDist * navRebuildDist ) ) ) {
				SVG_Nav_FreeTraversalPath( &self->navPath );
				self->navPathIndex = 0;

				bool built = false;
				Vector3 buildGoal = goalOrigin;

				// If a trail marker is on the wrong side of a blocker (or on a bad floor),
				// try advancing a couple of markers rather than walking toward a stale/invalid direction.
				for ( int32_t attempt = 0; attempt < 3 && !built; ++attempt ) {
					if ( SVG_Nav_GenerateTraversalPathForOriginEx_WithAgentBBox( self->currentOrigin, buildGoal, &self->navPath,
						self->mins, self->maxs, true, 256.0f, 512.0f ) ) {
						built = true;
						break;
					}

					if ( !canSeeGoalEntity ) {
						svg_base_edict_t *&trailSpot = s_dummyTrailSpot[ self->s.number ];
						svg_base_edict_t *next = PlayerTrail_PickNext( self );
						if ( next ) {
							trailSpot = next;
							buildGoal = trailSpot->currentOrigin;
							continue;
						}
					}
					break;
				}

				if ( built ) {
					self->navPathIndex = 0;
					self->navPathGoal = buildGoal;
					gi.dprintf( "TestDummy Puppet: Rebuilt nav path with %d points.\n", self->navPath.num_points );
				} else {
					gi.dprintf( "TestDummy Puppet: Failed to rebuild nav path.\n" );
					self->navPathGoal = buildGoal;
				}

				self->navPathNextRebuildTime = level.time + navRebuildInterval;
			}
 
			Vector3 move_dir = {};
			bool has_nav_direction = SVG_Nav_QueryMovementDirection_Advance2D_Output3D( &self->navPath, self->currentOrigin, 48.0f, &self->navPathIndex, &move_dir );
   			if ( !has_nav_direction ) {
   				move_dir = QM_Vector3Normalize( Vector3{ toGoal.x, toGoal.y, 0.0f } );
   			}
  			move_dir.z = 0.0f;
  			move_dir = QM_Vector3Normalize( move_dir );

			const Vector3 face_dir = ( frameVelocity > 0.0 )
				? move_dir
				: QM_Vector3Normalize( Vector3{ toGoal.x, toGoal.y, 0.0f } );

			self->ideal_yaw = QM_Vector3ToYaw( move_dir );
			self->yaw_speed = 7.5f;
			SVG_MMove_FaceIdealYaw( self, self->ideal_yaw, self->yaw_speed );
   
             // Drive movement from nav direction (not from facing) to ensure steering actually follows nav.
             self->velocity.x = ( float )( move_dir.x * frameVelocity );
             self->velocity.y = ( float )( move_dir.y * frameVelocity );
 
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
 
 			const int32_t blockedMask = SVG_MMove_StepSlideMove( &monsterMove );

			// Prevent walking off large drops. If we're about to leave ground, trace down
			// from the would-be new origin and cancel horizontal movement if the fall is too large.
			if ( self->groundInfo.entityNumber != ENTITYNUM_NONE ) {
				const bool willBeAirborne = ( monsterMove.ground.entityNumber == ENTITYNUM_NONE );
				if ( willBeAirborne ) {
					Vector3 start = monsterMove.state.origin;
					Vector3 end = start;
					end.z -= 128.0f;

					cm_trace_t tr = gi.trace( &start, &self->mins, &self->maxs, &end, self, CM_CONTENTMASK_SOLID );
					const float drop = start.z - tr.endpos[ 2 ];
					if ( tr.fraction < 1.0f && drop > 100.0f ) {
						monsterMove.state.origin = self->currentOrigin;
						monsterMove.state.velocity.x = 0.0f;
						monsterMove.state.velocity.y = 0.0f;
						monsterMove.ground = self->groundInfo;
					}
				}
			}
 
 			if ( std::fabs( monsterMove.step.height ) > 0.f + FLT_EPSILON ) {
 				self->s.renderfx |= RF_STAIR_STEP;
 			}
 
 			if ( !( blockedMask & MM_SLIDEMOVEFLAG_TRAPPED ) ) {
 				self->groundInfo = monsterMove.ground;
 				self->liquidInfo = monsterMove.liquid;
 				SVG_Util_SetEntityOrigin( self, monsterMove.state.origin, true );
 				self->velocity = monsterMove.state.velocity;
 				gi.linkentity( self );
 			}
		} else if ( self->lifeStatus == LIFESTATUS_ALIVE ) {
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

			const int32_t blockedMask = SVG_MMove_StepSlideMove( &monsterMove );

			if ( std::fabs( monsterMove.step.height ) > 0.f + FLT_EPSILON ) {
				self->s.renderfx |= RF_STAIR_STEP;
			}

			if ( !( blockedMask & MM_SLIDEMOVEFLAG_TRAPPED ) ) {
				self->groundInfo = monsterMove.ground;
				self->liquidInfo = monsterMove.liquid;
				SVG_Util_SetEntityOrigin( self, monsterMove.state.origin, true );

				self->velocity = monsterMove.state.velocity;
				gi.linkentity( self );
			}

			self->s.frame++;
			if ( self->s.frame >= 82 ) {
				self->s.frame = 0;
			}
		}

		Vector3 postSumOrigin = self->currentOrigin;
		Vector3 diffOrigin = postSumOrigin - preSumOrigin;
		const double diffLength = QM_Vector3LengthSqr( diffOrigin );
		self->summedDistanceTraversed += diffLength;

    //---------------------------
    // </TEMPORARY FOR TESTING>
    //---------------------------
    } else {
        if ( self->lifeStatus != LIFESTATUS_ALIVE ) {
            if ( self->s.frame >= 512 && self->s.frame < 642 ) {
                // Forward Death 01.
                self->s.frame++;
                if ( self->s.frame >= 642 ) {
                    self->s.frame = 641;
                }
            } else if ( self->s.frame >= 642 && self->s.frame < 801 ) {
                // Forward Death 02.
                self->s.frame++;
                if ( self->s.frame >= 801 ) {
                    self->s.frame = 800;
                }
            } else if ( self->s.frame >= 801 && self->s.frame < 928 ) {
                // Backward Death 01.
                self->s.frame++;
                if ( self->s.frame >= 928 ) {
                    self->s.frame = 927;
                }
            }
        }
    }

	// Apply angles.
	SVG_Util_SetEntityAngles( self, self->currentAngles, true );

    // Setup nextthink.
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
                
                SVG_Nav_FreeTraversalPath( &self->navPath );
                self->navPathIndex = 0;
                self->navPathGoal = {};
                self->navPathNextRebuildTime = 0_ms;

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
    
    SVG_Nav_FreeTraversalPath( &self->navPath );
    self->navPathIndex = 0;
    self->navPathGoal = {};
    self->navPathNextRebuildTime = 0_ms;

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
