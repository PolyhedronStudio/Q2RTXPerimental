/********************************************************************
*
*
*	ServerGame: TestDummy Monster Edict
*	NameSpace: "".
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/svg_misc.h"
#include "svgame/svg_trigger.h"

// TODO: Move elsewhere.. ?
#include "refresh/shared_types.h"

// SharedGame UseTargetHints.
#include "sharedgame/sg_usetarget_hints.h"

// Monster Move
#include "svgame/monsters/svg_mmove.h"
#include "svgame/monsters/svg_mmove_slidemove.h"

// TestDummy Monster
#include "svgame/entities/monster/svg_monster_testdummy.h"



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
    // Call upon the base class.
    Super::Reset( retainDictionary );
    // Reset the edict's save descriptor fields.
    testVar = 1337;
    //testVar2 = {};
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
    // Always call upon base methods.
    Super::onSpawn( self );

    // Entity Type:
    self->s.entityType = ET_MONSTER;

    // Solid/MoveType:
    self->solid = SOLID_BOUNDS_OCTAGON;
    self->movetype = MOVETYPE_WALK;
    //self->monsterinfo.aiflags = AI_NOSTEP;

    // Model/BBox:
    self->model = svg_level_qstring_t::from_char_str( "models/characters/mixadummy/tris.iqm" );
    self->s.modelindex = gi.modelindex( self->model );
    // Make sure to restore the actual root motion set data.
    const char *modelname = self->model;
    const model_t *model_forname = gi.GetModelDataForName( modelname );
    self->rootMotionSet = &model_forname->skmConfig->rootMotion;

    VectorCopy( svg_monster_testdummy_t::DUMMY_BBOX_STANDUP_MINS, self->mins );
    VectorCopy( svg_monster_testdummy_t::DUMMY_BBOX_STANDUP_MAXS, self->maxs );

    // Defaults:
    //if ( !self->mass ) {
        self->mass = 200;
    //}
    if ( !self->health ) {
        self->health = 200;
    }
    if ( !self->dmg ) {
        self->dmg = 150;
    }
    // Initialize gravity properly
    if ( !self->gravity ) {
        self->gravity = 1.0f;
    }
    // Monster Entity Faking:
    self->svFlags &= ~SVF_DEADENTITY;
    self->svFlags |= SVF_MONSTER;
    //s.renderfx |= RF_FRAMELERP;
    self->s.skinnum = 0;

    self->takedamage = DAMAGE_AIM;

    self->air_finished_time = level.time + 12_sec;

    self->max_health = self->health;

    self->clipMask = CM_CONTENTMASK_MONSTERSOLID;

    self->takedamage = DAMAGE_YES;
    self->lifeStatus = LIFESTATUS_ALIVE;

    self->useTarget.flags = ENTITY_USETARGET_FLAG_TOGGLE;

    // Think:
    self->nextthink = level.time + 20_hz;
    self->SetThinkCallback( &svg_monster_testdummy_t::onThink );
    // Die:
    self->SetDieCallback( &svg_monster_testdummy_t::onDie );
    // Pain:
    self->SetPainCallback( &svg_monster_testdummy_t::onPain );
    // Post Spawn:
    self->SetPostSpawnCallback( &svg_monster_testdummy_t::onPostSpawn );
    // Touch:
    self->SetTouchCallback( &svg_monster_testdummy_t::onTouch );
    // Use:
    self->SetUseCallback( &svg_monster_testdummy_t::onUse );

    // Reset to engagement mode usehint. (Yes this is a cheap hack., it is not client specific.)
    SVG_Entity_SetUseTargetHintByID( self, USETARGET_HINT_ID_NPC_ENGAGE );

    // Link it in.
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


    // Ensure to remove RF_STAIR_STEP and RF_OLD_FRAME_LERP.
    self->s.renderfx &= ~( RF_STAIR_STEP | RF_OLD_FRAME_LERP );

    self->testVar = level.frameNumber;
    //gi.dprintf( "%s: monster_testdummy(#%d), distanceTraversed(%f)\n", __func__, self->s.number, self->summedDistanceTraversed );
    // Animate.
    if ( self->health > 0 ) {
        #if 0
        self->s.frame++;
        if ( self->s.frame >= 82 ) {
            self->s.frame = 0;
        }
        #endif

        // For summing up distance traversed.
        Vector3 preSumOrigin = self->s.origin;
        // Generate frame velocity vector.
        Vector3 previousVelocity = self->velocity;
        //---------------------------
        // <TEMPORARY FOR TESTING>
        //---------------------------
        if ( self->activator && self->activator->client ) {
            // Get the root motion.
            skm_rootmotion_t *rootMotion = self->rootMotionSet->motions[ 3 ]; // [3] == RUN_FORWARD_PISTOL
            // Calculate the last frame index.
            // 
            // Increment animation.
            self->s.frame++;
            if ( self->s.frame == rootMotion->lastFrameIndex ) {
                self->s.frame = rootMotion->firstFrameIndex;
            }

            // Calculate the index for the rootmotion animation.
            int32_t rootMotionFrame = self->s.frame - rootMotion->firstFrameIndex;

            // The clamp top limit as well as the divider, which acts as a magic number
            // for dividing velocity's that are of the same scale as that of the players.
            // 
            // Desired average speed in units per second to match player movement
            constexpr double desiredAverageSpeed = ( 220. ); // units per second (matching player exactly)

            // Get the raw translation from root motion data
            Vector3 rawTranslation = *rootMotion->translations[ rootMotionFrame ];

            // Calculate the raw 2D distance for this frame (ignore Z for movement)
            double rawFrameDistance = std::sqrt( rawTranslation.x * rawTranslation.x +
                rawTranslation.y * rawTranslation.y );

            // **SIMPLIFIED APPROACH: Just set velocity to desired speed**
            double frameVelocity = 0.0;

            if ( rawFrameDistance > 0.0 ) {
                // If there's any movement this frame, just use the desired speed
                frameVelocity = desiredAverageSpeed;

                // Optional: Scale by the frame's relative movement to preserve some animation rhythm
                if ( rootMotion->totalDistance > 0.0 ) {
                    // Calculate the average frame distance
                    double avgFrameDistance = rootMotion->totalDistance / rootMotion->frameCount;

                    // Scale the velocity based on this frame's movement relative to average
                    double frameRatio = rawFrameDistance / avgFrameDistance;

                    // Clamp the ratio to prevent extreme values
                    frameRatio = QM_Clamp( frameRatio, 0.1, 3.0 );

                    frameVelocity = desiredAverageSpeed * frameRatio;
                }
            }

            #if 0
            // Debug output to see what's actually happening
            gi.dprintf( "Frame %d: raw=%.4f, velocity=%.2f, frameTime=%.4f, totalDist=%.2f, frameCount=%d\n",
                rootMotionFrame, rawFrameDistance, frameVelocity, gi.frame_time_s,
                rootMotion->totalDistance, rootMotion->frameCount );
            #endif
            // Goal Origin:
            Vector3 goalOrigin = self->activator->s.origin;
            if ( self->goalentity ) {
                goalOrigin = self->goalentity->s.origin;
            }
            // Calculate ideal yaw to turn into.
            self->ideal_yaw = QM_Vector3ToYaw(
                QM_Vector3Normalize( goalOrigin - Vector3( self->s.origin ) )
            );
            // Setup decent yaw turning speed.
            self->yaw_speed = 7.5f; // Was 10.f

            // Move yaw a frame into ideal yaw position.
            if ( !SVG_Entity_IsInFrontOf( self, goalOrigin, 3.0f ) ) {
                // Before adding IsInFrontOf it'd rotate too precisely. (This looks better).
                SVG_MMove_FaceIdealYaw( self, self->ideal_yaw, self->yaw_speed );
            }

            // Set follow trail time.
            if ( SVG_Entity_IsVisible( self->goalentity, self ) ) {
                self->trail_time = level.time;
            }

            Vector3 wishDirVelocity = {
                cos( self->s.angles[ YAW ] * QM_DEG2RAD ),
                sin( self->s.angles[ YAW ] * QM_DEG2RAD ),
                0.0 // Don't override Z velocity
            };

            // **SET VELOCITY DIRECTLY TO DESIRED SPEED**
            if ( self->groundInfo.entity != nullptr ) {
                // The movement system expects velocity in units per second
                self->velocity.x = (float)( wishDirVelocity.x * frameVelocity );
                self->velocity.y = (float)( wishDirVelocity.y * frameVelocity );
            }
            SVG_AddGravity( self );

            // Keep existing Z velocity for gravity/jumping
            // Is done in MMove_StepSlideMove.
            // If not onground, zero out the frameVelocity.
            #if 0
            if ( self->groundInfo.entity != nullptr ) {

                const float currentspeed = QM_Vector3DotProduct( previousVelocity, wishDirVelocity );
                const float addSpeed = QM_Vector3NormalizeLength( wishDirVelocity ) - currentspeed;
                if ( addSpeed <= 0 ) {
                    return;
                }
                float accelerationSpeed = distance * gi.frame_time_s * QM_Vector3NormalizeLength( wishDirVelocity );
                if ( accelerationSpeed > addSpeed ) {
                    accelerationSpeed = addSpeed;
                }

                Vector3 oldVelocity = self->velocity;
                self->velocity = QM_Vector3MultiplyAdd( self->velocity, accelerationSpeed, wishDirVelocity );
                wishDirVelocity = self->velocity - oldVelocity;
            }
            #endif

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
                    .mm_time = 0,

                    .gravity = (int16_t)( self->gravity * sv_gravity->value ),

                    .origin = self->s.origin,
                    .velocity = self->velocity,

                    .previousOrigin = self->s.origin,
                    .previousVelocity = previousVelocity,
                },
                
                // Proper ground and liquid information.
                .ground = self->groundInfo,
                .liquid = self->liquidInfo,
            };

            // Perform a step slide move for the specified monstermove state.
            const int32_t blockedMask = SVG_MMove_StepSlideMove( &monsterMove );

            // A step was taken, ensure to apply RF_STAIR_STEP renderflag.
            if ( std::fabs( monsterMove.step.height ) > 0.f + FLT_EPSILON ) {
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
                    .mm_time = 0,

                    .gravity = (int16_t)( self->gravity * sv_gravity->value ),

                    .origin = self->s.origin,
                    .velocity = self->velocity,

                    .previousOrigin = self->s.origin,
                    .previousVelocity = previousVelocity,
                },

                // Proper ground and liquid information.
                .ground = self->groundInfo,
                .liquid = self->liquidInfo,
            };

            // Perform a step slide move for the specified monstermove state.
            const int32_t blockedMask = SVG_MMove_StepSlideMove( &monsterMove );

            // A step was taken, ensure to apply RF_STAIR_STEP renderflag.
            if ( std::fabs( monsterMove.step.height ) > 0.f + FLT_EPSILON ) {
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
            self->s.frame++;
            if ( self->s.frame >= 82 ) {
                self->s.frame = 0;
            }
        }
        Vector3 postSumOrigin = self->s.origin;
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
    VectorCopy( DUMMY_BBOX_DEAD_MINS, self->mins );
    VectorCopy( DUMMY_BBOX_DEAD_MAXS, self->maxs );
    // Make sure to relink.
    gi.linkentity( self );
}
/**
*   @brief  Death routine.
**/
DEFINE_MEMBER_CALLBACK_PAIN( svg_monster_testdummy_t, onPain )( svg_monster_testdummy_t *self, svg_base_edict_t *other, const float kick, const int32_t damage, const entity_damageflags_t damageFlags ) -> void {

}