/********************************************************************
*
*
*	ClientGame: A 'Test Dummy' monster entity, posing, hence, puppet.
*
*
********************************************************************/
#include "g_local.h"
#include "refresh/shared_types.h"

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

/**
*   @brief  Touched.
**/
void monster_testdummy_puppet_touch( edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf ) {

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
}

/**
*   @brief  Thinking routine.
**/
void monster_testdummy_puppet_post_spawn( edict_t *self ) {
    //
    // Test GetModelData functions:
    //
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
        iqm_model_t *iqmModel = model_forname->iqmData;
        if ( iqmModel ) {
            // IQM Anim Count.
            const int32_t numOfAnimations = iqmModel->num_animations;
            // Iterate anim count times.
            for ( int32_t i = 0; i < numOfAnimations; i++ ) {
                iqm_anim_t *iqmAnimation = &iqmModel->animations[ i ];

                if ( iqmAnimation ) {
                    const char *animationName = iqmAnimation->name;
                    const int32_t animationFirstFrame = iqmAnimation->first_frame;
                    const int32_t animationLastFrame = animationFirstFrame + iqmAnimation->num_frames;
                    const int32_t animationLoop = iqmAnimation->loop;
                    gi.dprintf( "%s: testdummy(#%i), animation(#%i), [name='%s', firstFrame(%i), lastFrame(%i), loop(%s)]\n", 
                        __func__, self->s.number, i,
                        animationName,
                        animationFirstFrame,
                        animationLastFrame,
                        animationLoop ? "true" : "false"
                        );
                } else {
                    gi.dprintf( "%s: testdummy(#%i), animation(#%i), UNKNOWN ANIMATION.\n", __func__, self->s.number, i );
                }
            }
        }
    }
}
/**
*   @brief  Thinking routine.
**/
void monster_testdummy_puppet_think( edict_t *self ) {
    // Make sure to fall to floor.
    M_droptofloor( self );

    // Animate.
    if ( self->health > 0 ) {
        self->s.frame++;
        if ( self->s.frame >= 82 ) {
            self->s.frame = 0;
        }
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
//void barrel_explode( edict_t *self ) {
//    vec3_t  org;
//    float   spd;
//    vec3_t  save;
//    int     i;
//
//    T_RadiusDamage( self, self->activator, self->dmg, NULL, self->dmg + 40, MEANS_OF_DEATH_EXPLODED_BARREL );
//
//    VectorCopy( self->s.origin, save );
//    VectorMA( self->absmin, 0.5f, self->size, self->s.origin );
//
//    // a few big chunks
//    spd = 1.5f * (float)self->dmg / 200.0f;
//    VectorMA( self->s.origin, crandom(), self->size, org );
//    ThrowDebris( self, "models/objects/debris1/tris.md2", spd, org );
//    VectorMA( self->s.origin, crandom(), self->size, org );
//    ThrowDebris( self, "models/objects/debris1/tris.md2", spd, org );
//
//    // bottom corners
//    spd = 1.75f * (float)self->dmg / 200.0f;
//    VectorCopy( self->absmin, org );
//    ThrowDebris( self, "models/objects/debris3/tris.md2", spd, org );
//    VectorCopy( self->absmin, org );
//    org[ 0 ] += self->size[ 0 ];
//    ThrowDebris( self, "models/objects/debris3/tris.md2", spd, org );
//    VectorCopy( self->absmin, org );
//    org[ 1 ] += self->size[ 1 ];
//    ThrowDebris( self, "models/objects/debris3/tris.md2", spd, org );
//    VectorCopy( self->absmin, org );
//    org[ 0 ] += self->size[ 0 ];
//    org[ 1 ] += self->size[ 1 ];
//    ThrowDebris( self, "models/objects/debris3/tris.md2", spd, org );
//
//    // a bunch of little chunks
//    spd = 2 * self->dmg / 200;
//    for ( i = 0; i < 8; i++ ) {
//        VectorMA( self->s.origin, crandom(), self->size, org );
//        ThrowDebris( self, "models/objects/debris2/tris.md2", spd, org );
//    }
//
//    VectorCopy( save, self->s.origin );
//    if ( self->groundentity )
//        BecomeExplosion2( self );
//    else
//        BecomeExplosion1( self );
//}

void monster_testdummy_puppet_die( edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point ) {
    //self->takedamage = DAMAGE_NO;
    //self->nextthink = level.time + 20_hz;
    //self->think = barrel_explode;
    // Set activator.
    self->activator = attacker;

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

void SP_monster_testdummy_puppet( edict_t *self ) {
    // Solid/MoveType:
    self->solid = SOLID_BOUNDS_BOX;
    self->movetype = MOVETYPE_STEP;
    //self->monsterinfo.aiflags = AI_NOSTEP;

    // Model/BBox:
    self->model = "models/characters/mixadummy/tris.iqm";
    self->s.modelindex = gi.modelindex( self->model );
    VectorCopy( DUMMY_BBOX_STANDUP_MINS, self->mins );
    VectorCopy( DUMMY_BBOX_STANDUP_MAXS, self->maxs );

    // Defaults:
    if ( !self->mass ) {
        self->mass = 400;
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


//
// miscellaneous specialty items
//

/*QUAKED misc_deadsoldier (1 .5 0) (-16 -16 0) (16 16 16) ON_BACK ON_STOMACH BACK_DECAP FETAL_POS SIT_DECAP IMPALED
This is the dead player model. Comes in 6 exciting different poses!
*/
//void misc_deadsoldier_die( edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point ) {
//    int     n;
//
//    if ( self->health > -80 )
//        return;
//
//    gi.sound( self, CHAN_BODY, gi.soundindex( "world/gib01.wav" ), 1, ATTN_NORM, 0 );
//    for ( n = 0; n < 4; n++ )
//        ThrowGib( self, "models/objects/gibs/sm_meat/tris.md2", damage, GIB_ORGANIC );
//    ThrowHead( self, "models/objects/gibs/head2/tris.md2", damage, GIB_ORGANIC );
//}
//
//void SP_misc_deadsoldier( edict_t *ent ) {
//    if ( deathmatch->value ) {
//        // auto-remove for deathmatch
//        G_FreeEdict( ent );
//        return;
//    }
//
//    ent->movetype = MOVETYPE_NONE;
//    ent->solid = SOLID_BOUNDS_BOX;
//    ent->s.modelindex = gi.modelindex( "models/deadbods/dude/tris.md2" );
//
//    // Defaults to frame 0
//    if ( ent->spawnflags & 2 )
//        ent->s.frame = 1;
//    else if ( ent->spawnflags & 4 )
//        ent->s.frame = 2;
//    else if ( ent->spawnflags & 8 )
//        ent->s.frame = 3;
//    else if ( ent->spawnflags & 16 )
//        ent->s.frame = 4;
//    else if ( ent->spawnflags & 32 )
//        ent->s.frame = 5;
//    else
//        ent->s.frame = 0;
//
//    VectorSet( ent->mins, -16, -16, 0 );
//    VectorSet( ent->maxs, 16, 16, 16 );
//    ent->deadflag = DEAD_DEAD;
//    ent->takedamage = DAMAGE_YES;
//    ent->svflags |= SVF_MONSTER | SVF_DEADMONSTER;
//    ent->die = misc_deadsoldier_die;
//    ent->monsterinfo.aiflags |= AI_GOOD_GUY;
//
//    gi.linkentity( ent );
//}
