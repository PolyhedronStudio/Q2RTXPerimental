/********************************************************************
*
*
*	ServerGame: Misc Entity 'misc_explobox'.
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/svg_misc.h"
#include "svgame/entities/misc/svg_misc_explobox.h"



/*QUAKED misc_explobox (0 .5 .8) (-16 -16 0) (16 16 40)
Large exploding box.  You can override its mass (100),
health (80), and dmg (150).
*/
/**
*   @brief
**/
void barrel_touch( edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf ) {

    if ( ( !other->groundInfo.entity ) || ( other->groundInfo.entity == self ) ) {
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

    // WID: TODO: Use new monster walkmove/slidebox implementation.
    // Perform move.
    //M_walkmove( self, direction, distance );
}

/**
*   @brief
**/
void barrel_explode( edict_t *self ) {
    vec3_t  org;
    float   spd;
    vec3_t  save;
    int     i;

    SVG_RadiusDamage( self, self->activator, self->dmg, NULL, self->dmg + 40, MEANS_OF_DEATH_EXPLODED_BARREL );

    VectorCopy( self->s.origin, save );
    VectorMA( self->absmin, 0.5f, self->size, self->s.origin );

    // a few big chunks
    spd = 1.5f * (float)self->dmg / 200.0f;
    VectorMA( self->s.origin, crandom(), self->size, org );
    SVG_Misc_ThrowDebris( self, "models/objects/debris1/tris.md2", spd, org );
    VectorMA( self->s.origin, crandom(), self->size, org );
    SVG_Misc_ThrowDebris( self, "models/objects/debris1/tris.md2", spd, org );

    // bottom corners
    spd = 1.75f * (float)self->dmg / 200.0f;
    VectorCopy( self->absmin, org );
    SVG_Misc_ThrowDebris( self, "models/objects/debris3/tris.md2", spd, org );
    VectorCopy( self->absmin, org );
    org[ 0 ] += self->size[ 0 ];
    SVG_Misc_ThrowDebris( self, "models/objects/debris3/tris.md2", spd, org );
    VectorCopy( self->absmin, org );
    org[ 1 ] += self->size[ 1 ];
    SVG_Misc_ThrowDebris( self, "models/objects/debris3/tris.md2", spd, org );
    VectorCopy( self->absmin, org );
    org[ 0 ] += self->size[ 0 ];
    org[ 1 ] += self->size[ 1 ];
    SVG_Misc_ThrowDebris( self, "models/objects/debris3/tris.md2", spd, org );

    // a bunch of little chunks
    spd = 2 * self->dmg / 200;
    for ( i = 0; i < 8; i++ ) {
        VectorMA( self->s.origin, crandom(), self->size, org );
        SVG_Misc_ThrowDebris( self, "models/objects/debris2/tris.md2", spd, org );
    }

    VectorCopy( save, self->s.origin );
    if ( self->groundInfo.entity ) {
        SVG_Misc_BecomeExplosion2( self );
    } else {
        SVG_Misc_BecomeExplosion1( self );
    }
}

/**
*   @brief
**/
void barrel_delay( edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point ) {
    self->takedamage = DAMAGE_NO;
    self->nextthink = level.time + random_time( 150_ms );
    self->think = barrel_explode;
    self->activator = attacker;
}

/**
*   @brief  
**/
void SP_misc_explobox( edict_t *self ) {
    if ( deathmatch->value ) {
        // auto-remove for deathmatch
        SVG_FreeEdict( self );
        return;
    }

    gi.modelindex( "models/objects/debris1/tris.md2" );
    gi.modelindex( "models/objects/debris2/tris.md2" );
    gi.modelindex( "models/objects/debris3/tris.md2" );

    self->solid = SOLID_BOUNDS_OCTAGON;
    self->movetype = MOVETYPE_STEP;

    self->model = "models/objects/barrels/tris.md2";
    self->s.modelindex = gi.modelindex( self->model );
    VectorSet( self->mins, -16, -16, 0 );
    VectorSet( self->maxs, 16, 16, 40 );

    if ( !self->mass )
        self->mass = 400;
    if ( !self->health )
        self->health = 10;
    if ( !self->dmg )
        self->dmg = 150;

    self->die = barrel_delay;
    self->takedamage = DAMAGE_YES;
    //self->monsterinfo.aiflags = AI_NOSTEP;

    self->touch = barrel_touch;

    self->think = M_droptofloor;
    self->nextthink = level.time + 20_hz;

    gi.linkentity( self );
}