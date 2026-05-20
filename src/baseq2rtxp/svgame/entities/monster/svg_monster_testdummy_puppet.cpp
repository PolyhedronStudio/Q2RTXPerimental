/********************************************************************
*
*    ServerGame: TestDummy Puppet (Straight-Line)
*    File: svg_monster_testdummy_puppet.cpp
*    Description:
*        Minimal test dummy for hitbox debugging. Moves in a straight line
*        toward a target point, no navigation system required.
*
********************************************************************/
#include "svgame/entities/monster/svg_monster_testdummy_puppet.h"
#include "svgame/svg_entity_events.h"
#include "svgame/svg_misc.h"
#include "svgame/monsters/svg_mmove.h"
#include "svgame/monsters/svg_mmove_slidemove.h"
#include "svgame/player/svg_player_weapon.h"
#include "svgame/svg_utils.h"
#include "svgame/svg_edicts.h"
#include "sharedgame/sg_entities.h"
#include "refresh/shared_types.h"

/**
*   @brief  Pick the freshest audible world sound entity without depending on nav helpers.
*   @param  listener    Puppet evaluating the global sound slots.
*   @return The newest audible sound entity, or nullptr when no sound is usable.
**/
static svg_base_edict_t *Puppet_FindFreshestAudibleSound( svg_base_edict_t *listener ) {
    svg_base_edict_t *freshestSound = nullptr;

    /**
    *   Check the global sound slots and keep the newest audible candidate.
    **/
    const svg_base_edict_t *soundSlots[] = {
        level.weapon_sound_entity,
        level.impact_sound_entity,
        level.personal_sound_entity,
    };

    for ( const svg_base_edict_t *slotEntity : soundSlots ) {
        svg_base_edict_t *soundEntity = const_cast< svg_base_edict_t * >( slotEntity );
        if ( !SVG_PlayerNoise_IsEntityAlive( soundEntity ) ) {
            continue;
        }
        if ( !SVG_Util_IsEntityAudibleByPHS( listener, soundEntity, true, false ) ) {
            continue;
        }
        if ( !freshestSound || soundEntity->last_sound_time > freshestSound->last_sound_time ) {
            freshestSound = soundEntity;
        }
    }

    return freshestSound;
}

/**
*   @brief  Compute the current animation frame for a looping rootmotion clip.
*   @param  rootMotion  Animation clip metadata.
*   @param  animHz      Playback rate used for frame stepping.
*   @return Absolute frame index for the current server time.
**/
static int32_t Puppet_ComputeAnimFrameFromRootMotion( const skm_rootmotion_t *rootMotion, const float animHz ) {
    if ( !rootMotion ) {
        return 0;
    }

    const double timeSeconds = level.time.Seconds<double>();
    const int32_t animFrame = ( int32_t )std::floor( ( float )( timeSeconds * animHz ) );
    const int32_t localFrame = ( rootMotion->frameCount > 0 ) ? ( animFrame % rootMotion->frameCount ) : 0;
    return rootMotion->firstFrameIndex + localFrame;
}

/**
*   @brief  Slerp two direction vectors without relying on the navigation dummy helpers.
*   @param  from  Starting direction.
*   @param  to    Target direction.
*   @param  t     Interpolation factor in the 0..1 range.
*   @return Interpolated direction vector.
**/
static Vector3 Puppet_SlerpDirectionVector3( const Vector3 &from, const Vector3 &to, const float t ) {
    const float dot = QM_Vector3DotProduct( from, to );
    float aFactor = 0.0f;
    float bFactor = 0.0f;

    if ( std::fabs( dot ) > 0.9995f ) {
        aFactor = 1.0f - t;
        bFactor = t;
    } else {
        const float angle = std::acos( dot );
        const float sinOmega = std::sin( angle );
        aFactor = std::sin( ( 1.0f - t ) * angle ) / sinOmega;
        bFactor = std::sin( t * angle ) / sinOmega;
    }

    return from * aFactor + to * bFactor;
}

/**
*   @brief  Face the horizontal target direction using the same smoothing style as the other dummies.
*   @param  self            Puppet entity being rotated.
*   @param  directionHint   Current movement direction hint.
*   @param  targetPoint     World-space point to face.
*   @param  blendFactor     Blend amount between hint and exact target direction.
*   @param  yawSpeed        Yaw turn speed.
**/
static void Puppet_FaceTargetHorizontal( svg_monster_testdummy_puppet_t *self, const Vector3 &directionHint, const Vector3 &targetPoint, const float blendFactor, const float yawSpeed ) {
    Vector3 target = targetPoint;
    target.z = targetPoint.z;

    Vector3 toTarget = QM_Vector3Subtract( target, self->currentOrigin );
    const float dist2D = std::sqrt( toTarget.x * toTarget.x + toTarget.y * toTarget.y );
    if ( dist2D <= 0.001f ) {
        return;
    }

    Vector3 faceDir = Puppet_SlerpDirectionVector3( directionHint, QM_Vector3Normalize( toTarget ), blendFactor );
    const double currentYaw = self->ideal_yaw;
    const double targetYaw = QM_Vector3ToYaw( faceDir );
    const double deltaYaw = QM_AngleDelta( targetYaw, currentYaw );
    const float lerpFactor = QM_Clamp( ( float )( gi.frame_time_s * 6.0 ), 0.05f, 0.5f );
    self->ideal_yaw = currentYaw + deltaYaw * lerpFactor;
    self->yaw_speed = yawSpeed;
    SVG_MMove_FaceIdealYaw( self, self->ideal_yaw, self->yaw_speed );
}

/**
*   @brief  Advance the active death animation until it reaches its terminal corpse frame.
*   @param  self  Puppet entity currently dying or dead.
**/
static void Puppet_AdvanceDeathAnimation( svg_monster_testdummy_puppet_t *self ) {
    if ( self->s.frame >= 512 && self->s.frame < 642 ) {
        self->s.frame++;
        if ( self->s.frame >= 642 ) {
            self->s.frame = 641;
            self->s.entityType = ET_MONSTER_CORPSE;
            self->lifeStatus = LIFESTATUS_DEAD;
        }
    } else if ( self->s.frame >= 642 && self->s.frame < 801 ) {
        self->s.frame++;
        if ( self->s.frame >= 801 ) {
            self->s.frame = 800;
            self->s.entityType = ET_MONSTER_CORPSE;
            self->lifeStatus = LIFESTATUS_DEAD;
        }
    } else if ( self->s.frame >= 801 && self->s.frame < 928 ) {
        self->s.frame++;
        if ( self->s.frame >= 928 ) {
            self->s.frame = 927;
            self->s.entityType = ET_MONSTER_CORPSE;
            self->lifeStatus = LIFESTATUS_DEAD;
        }
    }
}

/**
*   @brief  Apply non-nav gravity and floor state before slide movement.
*   @param  self  Puppet entity being updated.
**/
static void Puppet_RefreshGroundAndGravity( svg_monster_testdummy_puppet_t *self ) {
    const cm_contents_t mask = SVG_GetClipMask( self );
    M_CheckGround( self, mask );

    if ( self->groundInfo.entityNumber == ENTITYNUM_NONE ) {
        self->velocity.z -= ( float )( self->gravity * sv_gravity->value * gi.frame_time_s );
    } else if ( self->velocity.z < 0.0f ) {
        self->velocity.z = 0.0f;
    }
}

/**
*   @brief  Run the puppet through plain slide movement with current velocity.
*   @param  self  Puppet entity being moved.
**/
static void Puppet_RunSlideMove( svg_monster_testdummy_puppet_t *self ) {
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
            .previousVelocity = self->velocity,
        },
        .ground = self->groundInfo,
        .liquid = self->liquidInfo,
    };

    SVG_MMove_SlideMove(
        monsterMove.state.origin,
        monsterMove.state.velocity,
        ( float )monsterMove.frameTime,
        monsterMove.mins,
        monsterMove.maxs,
        monsterMove.monster,
        monsterMove.touchTraces,
        monsterMove.state.mm_time != 0
    );

    SVG_Util_SetEntityOrigin( self, monsterMove.state.origin, true );
    self->velocity = monsterMove.state.velocity;
    gi.linkentity( self );
}

DEFINE_MEMBER_CALLBACK_SPAWN(svg_monster_testdummy_puppet_t, onSpawn)(svg_monster_testdummy_puppet_t *self) -> void {
    Super::onSpawn(self);

    /**
    *   Basic entity type and movement properties.
    **/
    self->s.entityType = ET_MONSTER;
    self->solid = SOLID_BOUNDS_BOX;
    self->movetype = MOVETYPE_WALK;

    /**
    *   Load model and cache rootmotion set for animation playback.
    **/
    self->model = svg_level_qstring_t::from_char_str("models/characters/mixadummy/tris.iqm");
    self->s.modelindex = gi.modelindex(self->model);
    const model_t *modelData = gi.GetModelDataForName( self->model );
    if ( modelData && modelData->skmConfig ) {
        self->rootMotionSet = &modelData->skmConfig->rootMotion;
    }

    /**
    *   Collision bbox and physics defaults.
    **/
    VectorCopy(PHYS_DEFAULT_BBOX_STANDUP_MINS, self->mins);
    VectorCopy(PHYS_DEFAULT_BBOX_STANDUP_MAXS, self->maxs);
    self->viewheight = PHYS_DEFAULT_VIEWHEIGHT_STANDUP;
    self->mass = 200;
    self->health = 200;
    self->max_health = 200;
    self->dmg = 150;
    self->gravity = 1.0f;
    self->svFlags &= ~SVF_DEADENTITY;
    self->svFlags |= SVF_MONSTER;
    self->s.skinnum = 0;
    self->takedamage = DAMAGE_YES;
    self->lifeStatus = LIFESTATUS_ALIVE;
    self->clipMask = CM_CONTENTMASK_MONSTERSOLID;
    self->useTarget.flags = ENTITY_USETARGET_FLAG_TOGGLE;

    /**
    *   Register core callbacks.
    **/
    self->nextthink = level.time + 20_hz;
    self->SetThinkCallback(&svg_monster_testdummy_puppet_t::onThink);
    self->SetPostSpawnCallback(&svg_monster_testdummy_puppet_t::onPostSpawn);
    self->SetUseCallback(&svg_monster_testdummy_puppet_t::onUse);
    self->SetPainCallback(&svg_monster_testdummy_puppet_t::onPain);
    self->SetDieCallback(&svg_monster_testdummy_puppet_t::onDie);
    gi.linkentity(self);

    /**
    *   By default the puppet starts without an active target.
    **/
    self->hasMoveTarget = false;
    self->isActivated = false;
}

DEFINE_MEMBER_CALLBACK_POSTSPAWN(svg_monster_testdummy_puppet_t, onPostSpawn)(svg_monster_testdummy_puppet_t *self) -> void {
    /**
    *   Drop the puppet to the floor on spawn so gravity starts from a valid state.
    **/
    if ( !self->activator ) {
        const cm_contents_t mask = SVG_GetClipMask( self );
        M_CheckGround( self, mask );
        M_droptofloor( self );
    }
}

DEFINE_MEMBER_CALLBACK_USE(svg_monster_testdummy_puppet_t, onUse)(svg_monster_testdummy_puppet_t *self, svg_base_edict_t *other, svg_base_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue) -> void {
    /**
    *   Only an explicit toggle-on from a player activates following behavior.
    **/
    self->other = other;
    self->activator = activator;
    self->isActivated = ( useType == entity_usetarget_type_t::ENTITY_USETARGET_TYPE_TOGGLE
        && useValue == 1
        && activator
        && activator->client );

    if ( !self->isActivated ) {
        self->hasMoveTarget = false;
        self->velocity = { 0, 0, 0 };
    }
}

DEFINE_MEMBER_CALLBACK_THINK(svg_monster_testdummy_puppet_t, onThink)(svg_monster_testdummy_puppet_t *self) -> void {
    /**
    *   Death handling: only advance the selected death animation and stop all movement.
    **/
    if ( self->lifeStatus != LIFESTATUS_ALIVE ) {
        self->SetThinkCallback( &svg_monster_testdummy_puppet_t::onThink_Dead );
        self->nextthink = level.time + FRAME_TIME_MS;
        return;
    }

    /**
    *   Stay idle until explicitly activated by use.
    **/
    if ( !self->isActivated ) {
        self->hasMoveTarget = false;
        self->velocity = { 0, 0, 0 };
        if ( self->rootMotionSet && self->rootMotionSet->motions[ 1 ] ) {
            self->s.frame = Puppet_ComputeAnimFrameFromRootMotion( self->rootMotionSet->motions[ 1 ], 40.0f );
        } else {
            self->s.frame = 0;
        }
        Puppet_RefreshGroundAndGravity( self );
        Puppet_RunSlideMove( self );
        self->nextthink = level.time + FRAME_TIME_MS;
        return;
    }

    /**
    *   Acquire the freshest available audible or visible target.
    **/
    // Find the freshest audible entity (sound event).
    svg_base_edict_t *audible = Puppet_FindFreshestAudibleSound( self );
    svg_base_edict_t *visible = nullptr;
    // Find the first visible player/client (simple scan)
    for (int i = 1; i <= maxclients->integer; ++i) {
        svg_base_edict_t *ent = ((svg_base_edict_t *)g_edicts) + i;
        if (!ent || !ent->inUse || !ent->client)
            continue;
        if (SVG_Entity_IsVisible(self, ent)) {
            visible = ent;
            break;
        }
    }

    svg_base_edict_t *target = nullptr;
    // Prefer audible if present, otherwise visible
    if (audible) {
        target = audible;
    } else if (visible) {
        target = visible;
    }

    if (target) {
        self->moveTarget = target->currentOrigin;
        self->hasMoveTarget = true;
    } else {
        self->hasMoveTarget = false;
    }

    /**
    *   Idle fallback: stop and play the idle rootmotion clip if it exists.
    **/
    if (!self->hasMoveTarget) {
        self->velocity = {0,0,0};
        if ( self->rootMotionSet && self->rootMotionSet->motions[ 1 ] ) {
            self->s.frame = Puppet_ComputeAnimFrameFromRootMotion( self->rootMotionSet->motions[ 1 ], 40.0f );
        } else {
            self->s.frame = 0;
        }
        Puppet_RefreshGroundAndGravity( self );
        Puppet_RunSlideMove( self );
        self->nextthink = level.time + FRAME_TIME_MS;
        return;
    }

    /**
    *   Move directly toward the current target using the run animation and straight-line velocity.
    **/
    Vector3 toTarget = QM_Vector3Subtract(self->moveTarget, self->currentOrigin);
    float dist = QM_Vector3Length(toTarget);
    if (dist < 2.0f) {
        self->velocity = {0,0,0};
        if ( self->rootMotionSet && self->rootMotionSet->motions[ 1 ] ) {
            self->s.frame = Puppet_ComputeAnimFrameFromRootMotion( self->rootMotionSet->motions[ 1 ], 40.0f );
        } else {
            self->s.frame = 0;
        }
        self->nextthink = level.time + FRAME_TIME_MS;
        return;
    }

    Vector3 dir = QM_Vector3Normalize(toTarget);
    constexpr float desiredSeparation = 8.0f;
    constexpr float slowDownRange = 64.0f;
    constexpr float desiredAverageSpeed = 220.0f;
    const float approachDist = std::max( 0.0f, dist - desiredSeparation );
    const float speedScale = ( slowDownRange > 0.0f ) ? QM_Clamp( approachDist / slowDownRange, 0.0f, 1.0f ) : 1.0f;
    float frameVelocity = desiredAverageSpeed * speedScale;
    if ( approachDist <= 0.0f && std::fabs( toTarget.z ) < 8.0f ) {
        frameVelocity = 0.0f;
    }

    Puppet_FaceTargetHorizontal( self, dir, self->moveTarget, 0.35f, 15.0f );

    self->velocity.x = dir.x * frameVelocity;
    self->velocity.y = dir.y * frameVelocity;

    if ( self->rootMotionSet && self->rootMotionSet->motions[ 4 ] ) {
        self->s.frame = Puppet_ComputeAnimFrameFromRootMotion( self->rootMotionSet->motions[ 4 ], 40.0f );
    }

    /**
    *   Apply gravity and run the plain slide move path only. No navigation code is involved here.
    **/
    Puppet_RefreshGroundAndGravity( self );
    Puppet_RunSlideMove( self );
    self->nextthink = level.time + FRAME_TIME_MS;
}

DEFINE_MEMBER_CALLBACK_THINK(svg_monster_testdummy_puppet_t, onThink_Dead)(svg_monster_testdummy_puppet_t *self) -> void {
    /**
    *   Keep the corpse linked, animated, and affected by simple gravity so it remains a persistent body.
    **/
    self->s.renderfx &= ~( RF_STAIR_STEP | RF_OLD_FRAME_LERP );
    self->svFlags &= ~( SVF_MONSTER );
    self->svFlags |= SVF_DEADENTITY;

    self->velocity.x *= 0.8f;
    self->velocity.y *= 0.8f;
    if ( std::fabs( self->velocity.x ) < 0.1f ) self->velocity.x = 0.0f;
    if ( std::fabs( self->velocity.y ) < 0.1f ) self->velocity.y = 0.0f;

    Puppet_AdvanceDeathAnimation( self );
    Puppet_RefreshGroundAndGravity( self );
    Puppet_RunSlideMove( self );

    self->SetThinkCallback( &svg_monster_testdummy_puppet_t::onThink_Dead );
    self->nextthink = level.time + FRAME_TIME_MS;
}

DEFINE_MEMBER_CALLBACK_DIE(svg_monster_testdummy_puppet_t, onDie)(svg_monster_testdummy_puppet_t *self, svg_base_edict_t *inflictor, svg_base_edict_t *attacker, int32_t damage, Vector3 *point) -> void {
    ( void )inflictor;
    ( void )point;

    /**
    *   Allow gibbing even after the corpse has fully reached its dead state.
    **/
    if ( ( self->lifeStatus == LIFESTATUS_DYING || self->lifeStatus == LIFESTATUS_DEAD ) && self->health < GIB_DEATH_HEALTH ) {
        SVG_EntityEvent_GeneralSoundEx( self, CHAN_BODY, gi.soundindex( "world/gib01.wav" ), ATTN_NORM );
        for ( int32_t gibIndex = 0; gibIndex < 4; gibIndex++ ) {
            SVG_Misc_ThrowGib( self, "models/objects/gibs/sm_meat/tris.md2", damage, GIB_TYPE_ORGANIC );
        }
        SVG_Misc_ThrowHead( self, "models/objects/gibs/head2/tris.md2", damage, GIB_TYPE_ORGANIC );
        self->takedamage = DAMAGE_NO;
        self->lifeStatus = LIFESTATUS_DEAD;
        return;
    }

    if ( self->lifeStatus == LIFESTATUS_DEAD ) {
        return;
    }

    /**
    *   Start one of the stock death animation sequences and switch to dead collision bounds.
    **/
    self->activator = attacker;
    if ( self->lifeStatus == LIFESTATUS_ALIVE ) {
        const int32_t deathAnim = irandom( 3 );
        if ( deathAnim == 0 ) {
            self->s.frame = 512;
        } else if ( deathAnim == 1 ) {
            self->s.frame = 642;
        } else {
            self->s.frame = 801;
        }

        self->lifeStatus = LIFESTATUS_DYING;
        self->svFlags |= SVF_DEADENTITY;
        self->takedamage = DAMAGE_YES;
        self->s.sound = 0;
        self->mins = { -16.0f, -16.0f, -36.0f };
        self->maxs = { 16.0f, 16.0f, 8.0f };
        self->s.entityType = ET_MONSTER_CORPSE;
        gi.linkentity( self );
        self->SetThinkCallback( &svg_monster_testdummy_puppet_t::onThink_Dead );
        self->nextthink = level.time + FRAME_TIME_MS;
    }
}

DEFINE_MEMBER_CALLBACK_PAIN(svg_monster_testdummy_puppet_t, onPain)(svg_monster_testdummy_puppet_t *self, svg_base_edict_t *other, const float kick, const int damage, const entity_damageflags_t damageFlags) -> void {
    ( void )self;
    ( void )other;
    ( void )kick;
    ( void )damage;
    ( void )damageFlags;
}
