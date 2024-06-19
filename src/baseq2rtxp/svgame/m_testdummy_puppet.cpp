/********************************************************************
*
*
*	ClientGame: A 'Test Dummy' monster entity, posing, hence, puppet.
*
*
********************************************************************/
#include "g_local.h"

// TODO: Move elsewhere.. ?
#include "refresh/shared_types.h"

// Monster Move
#include "monsters/g_mmove.h"
#include "monsters/g_mmove_slidemove.h"


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
static sg_skm_rootmotion_set_t rootMotionSet;
static int32_t animationID = 1; // IDLE(0), RUN_FORWARD_PISTOL(1)

//---------------------------
// </TEMPORARY FOR TESTING>
//---------------------------


/**
*   @brief  Death routine.
**/
void monster_testdummy_puppet_die( edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point ) {
    //self->takedamage = DAMAGE_NO;
    //self->nextthink = level.time + 20_hz;
    //self->think = barrel_explode;
    // Set activator.
    self->activator = attacker;

    //---------------------------
    // <TEMPORARY FOR TESTING>
    //---------------------------

    //---------------------------
    // </TEMPORARY FOR TESTING>
    //---------------------------

    if ( self->s.frame < 523 ) {
        int32_t deathanim = irandom( 3 );
        if ( deathanim == 0 ) {
            self->s.frame = 523;
        } else if ( deathanim == 1 ) {
            self->s.frame = 653;
        } else {
            self->s.frame = 812;
        }
        //self->s.frame = 523;

        self->deadflag = DEAD_DYING;
        self->svflags |= SVF_DEADMONSTER;
    } else if ( self->s.frame == 652 ) {
        self->deadflag = DEAD_DEAD;
    } else if ( self->s.frame == 811 ) {
        self->deadflag = DEAD_DEAD;
    } else if ( self->s.frame == 938 ) {
        self->deadflag = DEAD_DEAD;
    }

    VectorCopy( DUMMY_BBOX_DEAD_MINS, self->mins );
    VectorCopy( DUMMY_BBOX_DEAD_MAXS, self->maxs );
    gi.linkentity( self );
}

/**
*   @brief  Touched.
**/
void monster_testdummy_puppet_touch( edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf ) {
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
    if ( other && other->client ) {
        // Assign enemy.
        self->activator = other;
        // Get the root motion.
        sg_skm_rootmotion_t *rootMotion = &rootMotionSet.rootMotions[ 3 ]; // [1] == RUN_FORWARD_PISTOL
        // Transition to its animation.
        self->s.frame = rootMotion->firstFrameIndex;
    }
    //---------------------------
    // </TEMPORARY FOR TESTING>
    //---------------------------
}


/**
*   @brief  Thinking routine.
**/
void monster_testdummy_puppet_think( edict_t *self ) {
    // Make sure to fall to floor.
    if ( !self->activator ) {
        M_droptofloor( self );
    }

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
            sg_skm_rootmotion_t *rootMotion = &rootMotionSet.rootMotions[ 3 ]; // [3] == RUN_FORWARD_PISTOL
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
            // WID: This is exact, but looks odd if you want faster paced gameplay.
            //float distance = rootMotion->distances[ rootMotionFrame ];
            // WID: This is quite 'average':
            //float distance = ( rootMotion->totalDistance ) * ( 1.0f / rootMotion->frameCount );
            // WID: Try and accustom to Velocity 'METHOD':
            // Base Velocity.
            constexpr float speed = 350.f;
            // Determine distance to traverse based on Base Velocity.
            float distance = ( ( speed / rootMotion->frameCount ) * ( rootMotion->frameCount / rootMotion->totalDistance ) );

            // Calculate ideal yaw to turn into.
            self->ideal_yaw = QM_Vector3ToYaw(
                QM_Vector3Normalize( Vector3( self->activator->s.origin ) - Vector3(self->s.origin) )
            );
            // Setup decent yaw speed.
            self->yaw_speed = 10;

            // Move yaw a frame into ideal yaw position.
            SVG_MMove_MoveYawToIdealYaw( self, self->ideal_yaw, self->yaw_speed );

            // Generate frame velocity vector.
            Vector3 entityVelocity = self->velocity;
            Vector3 frameVelocity = {
                cos( self->s.angles[ YAW ] * QM_DEG2RAD ) * distance,
                sin( self->s.angles[ YAW ] * QM_DEG2RAD ) * distance,
                0
            };
            // If not onground, zero out the frameVelocity.
            if ( self->groundInfo.entity == nullptr ) {
                frameVelocity = self->velocity;
            }

            // Setup the monster move structure.
            mm_move_t monsterMove = {
                // Pass/Skip-Entity pointer.
                .monster = self,
                // Frametime.
                .frameTime = 1.f,
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
                .liquid = {
                    .type = self->liquidtype,
                    .level = self->liquidlevel,
                },
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
                self->liquidtype = monsterMove.liquid.type;
                self->liquidlevel = monsterMove.liquid.level;
                // Copy over the resulting origin and velocity back into the entity.
                VectorCopy( monsterMove.state.origin, self->s.origin );
                VectorCopy( monsterMove.state.velocity, self->velocity );
                // Last but not least, make sure to link it back in.
                gi.linkentity( self );
            }
        } else {
            self->s.frame++;
            if ( self->s.frame >= 82 ) {
                self->s.frame = 0;
            }
        }
        //---------------------------
        // </TEMPORARY FOR TESTING>
        //---------------------------
    } else {
        if ( self->s.frame >= 523 && self->s.frame <= 652 ) {
            // Forward Death 01.
            self->s.frame++;
            if ( self->s.frame >= 652 ) {
                self->s.frame = 652;
            }
        } else if ( self->s.frame >= 653 && self->s.frame <= 811 ) {
            // Forward Death 02.
            self->s.frame++;
            if ( self->s.frame >= 811 ) {
                self->s.frame = 811;
            }
        } else if ( self->s.frame >= 812 && self->s.frame <= 938 ) {
            // Backward Death 01.
            self->s.frame++;
            if ( self->s.frame >= 938 ) {
                self->s.frame = 938;
            }
        }
    }

    // Setup nextthink.
    self->nextthink = level.time + FRAME_TIME_MS;
}

/**
*   @brief  Post-Spawn routine.
**/
void monster_testdummy_puppet_post_spawn( edict_t *self ) {
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
    if ( model_forname ) {
        // Load it just once.
        if ( rootMotionSet.totalRootMotions == 0 ) {
            rootMotionSet = SG_SKM_GenerateRootMotionSet( model_forname, 0, 1 );
        }
    }
    //---------------------------
    // </TEMPORARY FOR TESTING>
    //---------------------------
}

/**
*   @brief  Spawn routine.
**/
void SP_monster_testdummy_puppet( edict_t *self ) {
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
    self->s.renderfx |= RF_FRAMELERP;
    self->s.skinnum = 0;
    self->takedamage = DAMAGE_AIM;
    self->air_finished_time = level.time + 12_sec;
    //self->use = monster_use;
    self->max_health = self->health;
    self->clipmask = MASK_MONSTERSOLID;
    self->deadflag = DEAD_NO;
    self->svflags &= ~SVF_DEADMONSTER;

    // Touch:
    self->touch = monster_testdummy_puppet_touch;
    // Die:
    self->die = monster_testdummy_puppet_die;
    self->takedamage = DAMAGE_YES;
    // Think:
    self->think = monster_testdummy_puppet_think;
    self->nextthink = level.time + 20_hz;
    // PostSpawn.
    self->postspawn = monster_testdummy_puppet_post_spawn;

    gi.linkentity( self );
}
