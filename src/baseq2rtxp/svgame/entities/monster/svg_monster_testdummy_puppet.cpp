/********************************************************************
*
*
*	ClientGame: A 'Test Dummy' monster entity, posing, hence, puppet.
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/svg_misc.h"

// TODO: Move elsewhere.. ?
#include "refresh/shared_types.h"

#include "sharedgame/sg_usetarget_hints.h"

// Monster Move
#include "svgame/monsters/svg_mmove.h"
#include "svgame/monsters/svg_mmove_slidemove.h"

//! For when dummy is standing straight up.
static constexpr Vector3 DUMMY_BBOX_STANDUP_MINS = { -16.f, -16.f, 0.f };
static constexpr Vector3 DUMMY_BBOX_STANDUP_MAXS = { 16.f, 16.f, 72.f };
static constexpr float   DUMMY_VIEWHEIGHT_STANDUP = 30.f;
//! For when dummy is crouching.
static constexpr Vector3 DUMMY_BBOX_DUCKED_MINS = { -16.f, -16.f, -36.f };
static constexpr Vector3 DUMMY_BBOX_DUCKED_MAXS = { 16.f, 16.f, 8.f };
static constexpr float   DUMMY_VIEWHEIGHT_DUCKED = 4.f;
//! For when dummy is dead.
static constexpr Vector3 DUMMY_BBOX_DEAD_MINS = { -16.f, -16.f, -36.f };
static constexpr Vector3 DUMMY_BBOX_DEAD_MAXS = { 16.f, 16.f, 8.f };
static constexpr float   DUMMY_VIEWHEIGHT_DEAD = 8.f;


//---------------------------
// <TEMPORARY FOR TESTING>
//---------------------------
//static sg_skm_rootmotion_set_t rootMotionSet;
skm_rootmotion_set_t *rootMotionSet;
static int32_t animationID = 1; // IDLE(0), RUN_FORWARD_PISTOL(1)

//---------------------------
// </TEMPORARY FOR TESTING>
//---------------------------


/**
*   @brief  Death routine.
**/
void monster_testdummy_puppet_die( svg_edict_t *self, svg_edict_t *inflictor, svg_edict_t *attacker, int damage, vec3_t point ) {
    //self->takedamage = DAMAGE_NO;
    //self->nextthink = level.time + 20_hz;
    //self->think = barrel_explode;

    if ( self->lifeStatus == LIFESTATUS_DEAD ) {
        return;
    }

    if ( self->lifeStatus == LIFESTATUS_DYING ) {
        // Gib Death:
        if ( self->health < -40 ) {
            // Play gib sound.
            gi.sound( self, CHAN_BODY, gi.soundindex( "world/gib01.wav" ), 1, ATTN_NORM, 0 );
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
        self->svflags |= SVF_DEADMONSTER;
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
    VectorCopy( DUMMY_BBOX_DEAD_MINS, self->mins );
    VectorCopy( DUMMY_BBOX_DEAD_MAXS, self->maxs );
    // Make sure to relink.
    gi.linkentity( self );
}

///**
//*   @brief  Touched.
//**/
//void monster_testdummy_puppet_touch( svg_edict_t *self, svg_edict_t *other, const cm_plane_t *plane, cm_surface_t *surf ) {
//    #if 0
//    if ( ( !other->groundentity ) || ( other->groundentity == self ) ) {
//        return;
//    }
//
//    // Calculate direction.
//    vec3_t v = { };
//    VectorSubtract( self->s.origin, other->s.origin, v );
//
//    // Move ratio(based on their masses).
//    const float ratio = (float)other->mass / (float)self->mass;
//
//    // Yaw direction angle.
//    const float yawAngle = QM_Vector3ToYaw( v );
//    const float direction = yawAngle;
//    // Distance to travel.
//    float distance = 20 * ratio * FRAMETIME;
//
//    // Debug output:
//    if ( plane ) {
//        gi.dprintf( "self->s.origin( %s ), other->s.origin( %s )\n", vtos( self->s.origin ), vtos( other->s.origin ) );
//        gi.dprintf( "v( %s ), plane->normal( %s ), direction(%f), distance(%f)\n", vtos( v ), vtos( plane->normal ), direction, distance );
//    } else {
//        gi.dprintf( "self->s.origin( %s ), other->s.origin( %s )\n", vtos( self->s.origin ), vtos( other->s.origin ) );
//        gi.dprintf( "v( %s ), direction(%f), distance(%f)\n", vtos( v ), direction, distance );
//    }
//
//    // Perform move.
//    M_walkmove( self, direction, distance );
//    #endif
//    //---------------------------
//    // <TEMPORARY FOR TESTING>
//    //---------------------------
//    if ( other && other->client ) {
//        // Assign enemy.
//        self->activator = other;
//        self->goalentity = other;
//        // Get the root motion.
//        skm_rootmotion_t *rootMotion = rootMotionSet->motions[ 3 ]; // [1] == RUN_FORWARD_PISTOL
//        // Transition to its animation.
//        self->s.frame = rootMotion->firstFrameIndex;
//    }
//    //---------------------------
//    // </TEMPORARY FOR TESTING>
//    //---------------------------
//}
/**
*   @brief  Touched.
**/
void monster_testdummy_puppet_touch( svg_edict_t *self, svg_edict_t *other, const cm_plane_t *plane, cm_surface_t *surf ) {
    #if 0
    if ( ( !other->groundentity ) || ( other->groundentity == self ) ) {
        return;
    }

    // Calculate direction.
    vec3_t v = { };
    VectorSubtract( self->s.origin, other->s.origin, v );

    // Move ratio(based on their masses).
    const float ratio = (float)other->mass / (float)self->mass;

    // Yaw direction angle.
    const float yawAngle = QM_Vector3ToYaw( v );
    const float direction = yawAngle;
    // Distance to travel.
    float distance = 20 * ratio * FRAMETIME;

    // Debug output:
    if ( plane ) {
        gi.dprintf( "self->s.origin( %s ), other->s.origin( %s )\n", vtos( self->s.origin ), vtos( other->s.origin ) );
        gi.dprintf( "v( %s ), plane->normal( %s ), direction(%f), distance(%f)\n", vtos( v ), vtos( plane->normal ), direction, distance );
    } else {
        gi.dprintf( "self->s.origin( %s ), other->s.origin( %s )\n", vtos( self->s.origin ), vtos( other->s.origin ) );
        gi.dprintf( "v( %s ), direction(%f), distance(%f)\n", vtos( v ), direction, distance );
    }

    // Perform move.
    M_walkmove( self, direction, distance );
    #endif
    //---------------------------
    // <TEMPORARY FOR TESTING>
    //---------------------------

    //---------------------------
    // </TEMPORARY FOR TESTING>
    //---------------------------
}
/**
*   @brief
**/
void monster_testdummy_puppet_use( svg_edict_t *self, svg_edict_t *other, svg_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue ) {
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
                skm_rootmotion_t *rootMotion = rootMotionSet->motions[ 3 ]; // [1] == RUN_FORWARD_PISTOL
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

void SV_AddGravity( svg_edict_t *ent );
/**
*   @brief  Thinking routine.
**/
void monster_testdummy_puppet_think( svg_edict_t *self ) {
    // Make sure to fall to floor.
    if ( !self->activator ) {
        M_droptofloor( self );
    }

    // Ensure to remove RF_STAIR_STEP and RF_OLD_FRAME_LERP.
    self->s.renderfx &= ~( RF_STAIR_STEP | RF_OLD_FRAME_LERP );

    // Animate.
    if ( self->health > 0 ) {
        #if 0
        self->s.frame++;
        if ( self->s.frame >= 82 ) {
            self->s.frame = 0;
        }
        #endif
        //---------------------------
        // <TEMPORARY FOR TESTING>
        //---------------------------
        if ( self->activator && self->activator->client ) {
            // Get the root motion.
            skm_rootmotion_t *rootMotion = rootMotionSet->motions[ 3 ]; // [3] == RUN_FORWARD_PISTOL
            // Calculate the last frame index.
            // 
            // Increment animation.
            self->s.frame++;
            if ( self->s.frame == rootMotion->lastFrameIndex ) {
                self->s.frame = rootMotion->firstFrameIndex;
            }

            // Calculate the index for the rootmotion animation.
            int32_t rootMotionFrame = self->s.frame - rootMotion->firstFrameIndex;

            // Get the distance for the frame:
            #if 1
                // WID: This is exact, but looks odd if you want faster paced gameplay.
                //float distance = rootMotion->distances[ rootMotionFrame ];
                // WID: This is quite 'average':
                //float distance = ( rootMotion->totalDistance ) * ( 1.0f / rootMotion->frameCount );
                // WID: Try and accustom to Velocity 'METHOD':
                //*pflGroundSpeed = sqrt( pseqdesc->linearmovement[ 0 ] * pseqdesc->linearmovement[ 0 ] + pseqdesc->linearmovement[ 1 ] * pseqdesc->linearmovement[ 1 ] + pseqdesc->linearmovement[ 2 ] * pseqdesc->linearmovement[ 2 ] );
                //*pflGroundSpeed = *pflGroundSpeed * pseqdesc->fps / ( pseqdesc->numframes - 1 );

			    // WID: This one is solid from what I can tell right now.
                Vector3 translation = *rootMotion->translations[ rootMotionFrame ];
                double distance = std::sqrt( translation.x * translation.x +
                                        translation.y * translation.y /*+ translation.z * translation.z*/ );
                // Unit distance per 'frame'.
                double unitDistance = 8.;
                // Scale distance to frame count
				distance = distance * ( 40. / rootMotion->frameCount - 1 );
                // Scale distance to 'unit distance'.
				distance = unitDistance * ( unitDistance / distance );
            #else
                // WID: Works also, somewhat.
                #if 0
                    // Base Velocity.
                    constexpr float speed = 250.f;
                    constexpr float maxSpeed = 325.f;
                    float distance = QM_Normalizef( speed / rootMotion->frameCount, 0, maxSpeed );
                    distance *= speed;
                // WID: This one actually is the best attempt so far.
                #else
                    // Base Velocity.
                    constexpr float speed = 250.f;
                    // Determine distance to traverse based on Base Velocity.
                    float distance = ( ( speed / rootMotion->frameCount ) * ( rootMotion->frameCount / rootMotion->totalDistance ) );
                #endif
            #endif

            // Goal Origin:
            Vector3 goalOrigin = self->activator->s.origin;
            if ( self->goalentity) {
                goalOrigin = self->goalentity->s.origin;
            }
            // Calculate ideal yaw to turn into.
            self->ideal_yaw = QM_Vector3ToYaw(
                QM_Vector3Normalize( goalOrigin - Vector3(self->s.origin) )
            );
            // Setup decent yaw turning speed.
            self->yaw_speed = 10.f;

            // Move yaw a frame into ideal yaw position.
            SVG_MMove_FaceIdealYaw( self, self->ideal_yaw, self->yaw_speed );

            // Set follow trail time.
            if ( SVG_Entity_IsVisible( self->goalentity, self ) ) {
                self->trail_time = level.time;
            }

			// Is done in MMove_StepSlideMove.
            //SV_AddGravity( self );

            // Generate frame velocity vector.
            Vector3 entityVelocity = self->velocity;
            Vector3 frameVelocity = {
                cos( self->s.angles[ YAW ] * QM_DEG2RAD ) * (double)distance,
                sin( self->s.angles[ YAW ] * QM_DEG2RAD ) * (double)distance,
                (double)entityVelocity.z
            };
            //// If not onground, zero out the frameVelocity.
            //if ( self->groundInfo.entity == nullptr ) {
            //    frameVelocity = self->velocity;
            //}

            // Setup the monster move structure.
            mm_move_t monsterMove = {
                // Pass/Skip-Entity pointer.
                .monster = self,
                // Frametime.
                .frameTime = gi.frame_time_s,
                // Bounds
                .mins = self->mins,
                .maxs = self->maxs,

                // Movement state, TODO: Store as part of gedict_t??
                .state = {
                    .mm_type = MM_NORMAL,
                    .mm_flags = ( self->groundInfo.entity != nullptr ? MMF_ON_GROUND : 0 ),
                    .mm_time = 8,

                    .gravity = (int16_t)sv_gravity->integer,

                    .origin = self->s.origin,
                    .velocity = frameVelocity,

                    .previousOrigin = self->s.origin,
                    .previousVelocity = self->velocity,
                },

                // Proper ground and liquid information.
                .ground = self->groundInfo,
                .liquid = self->liquidInfo,
            };

            // Perform a step slide move for the specified monstermove state.
            const int32_t blockedMask = SVG_MMove_StepSlideMove( &monsterMove );

            // A step was taken, ensure to apply RF_STAIR_STEP renderflag.
            if ( monsterMove.step.height != 0 ) {
                self->s.renderfx |= RF_STAIR_STEP;
            }
            // If the move was succesfull, copy over the state results into the entity's state.
            if ( !( blockedMask & MM_SLIDEMOVEFLAG_TRAPPED ) ) {
                // Copy over the monster's move state.
                // self->monsterMove = monsterMove;
                // Copy over the resulting ground and liquid info.
                self->groundInfo = monsterMove.ground;
                self->liquidInfo = monsterMove.liquid;
                // Copy over the resulting origin and velocity back into the entity.
                VectorCopy( monsterMove.state.origin, self->s.origin );
                VectorCopy( monsterMove.state.velocity, self->velocity );
                // Last but not least, make sure to link it back in.
                gi.linkentity( self );
            }
        } else if ( self->lifeStatus == LIFESTATUS_ALIVE ) {
            self->s.frame++;
            if ( self->s.frame >= 82 ) {
                self->s.frame = 0;
            }
        }
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

    // Setup nextthink.
    self->nextthink = level.time + FRAME_TIME_MS;
}

/**
*   @brief  Post-Spawn routine.
**/
void monster_testdummy_puppet_post_spawn( svg_edict_t *self ) {
    //
    // Test GetModelData functions:
    //
    #if 0
    const char *modelname = self->model;
    const model_t *model_forname = gi.GetModelDataForName( modelname );
    const model_t *model_forhandle = gi.GetModelDataForHandle( self->s.modelindex );
    if ( model_forname ) {
        gi.dprintf( "%s: testdummy(#%i), model_forname(%s)\n", __func__, self->s.number, model_forname->name );
    }
    if ( model_forhandle ) {
        gi.dprintf( "%s: testdummy(#%i), model_forhandle(#%i, %s)\n", __func__, self->s.number, self->s.modelindex, model_forhandle->name );
    }

    //
    // Iterate over all IQM animations and print out its information.
    //
    if ( model_forname ) {
        SG_SKM_GenerateRootMotionSet( model_forname, 0, 0 );
    }
    #endif

    //---------------------------
    // <TEMPORARY FOR TESTING>
    //---------------------------
    const char *modelname = self->model;
    const model_t *model_forname = gi.GetModelDataForName( modelname );
    rootMotionSet = &model_forname->skmConfig->rootMotion;
    //---------------------------
    // </TEMPORARY FOR TESTING>
    //---------------------------
}

/**
*   @brief  Spawn routine.
**/
void SP_monster_testdummy_puppet( svg_edict_t *self ) {
    // Entity Type:
    self->s.entityType = ET_MONSTER;
    
    // Solid/MoveType:
    self->solid = SOLID_BOUNDS_BOX;
    self->movetype = MOVETYPE_ROOTMOTION;
    //self->monsterinfo.aiflags = AI_NOSTEP;

    // Model/BBox:
    self->model = "models/characters/mixadummy/tris.iqm";
    self->s.modelindex = gi.modelindex( self->model );
    VectorCopy( DUMMY_BBOX_STANDUP_MINS, self->mins );
    VectorCopy( DUMMY_BBOX_STANDUP_MAXS, self->maxs );

    // Defaults:
    if ( !self->mass ) {
        self->mass = 200;
    }
    if ( !self->health ) {
        self->health = 100;
    }
    if ( !self->dmg ) {
        self->dmg = 150;
    }

    // Monster Entity Faking:
    self->svflags |= SVF_MONSTER;
    //self->s.renderfx |= RF_FRAMELERP;
    self->s.skinnum = 0;
    self->takedamage = DAMAGE_AIM;
    self->air_finished_time = level.time + 12_sec;
    self->max_health = self->health;
    self->clipmask = CM_CONTENTMASK_MONSTERSOLID;
    self->lifeStatus = LIFESTATUS_ALIVE;
    self->svflags &= ~SVF_DEADMONSTER;
    self->useTarget.flags = ENTITY_USETARGET_FLAG_TOGGLE;

    // Touch:
    //self->touch = monster_testdummy_puppet_touch;
    // Die:
    self->die = monster_testdummy_puppet_die;
    self->takedamage = DAMAGE_YES;
    // Think:
    self->think = monster_testdummy_puppet_think;
    self->nextthink = level.time + 20_hz;
    // PostSpawn.
    self->postspawn = monster_testdummy_puppet_post_spawn;
    // Use:
    self->use = monster_testdummy_puppet_use;

    // Reset to engagement mode usehint. (Yes this is a cheap hack., it is not client specific.)
    SVG_Entity_SetUseTargetHintByID( self, USETARGET_HINT_ID_NPC_ENGAGE );

    gi.linkentity( self );
}
