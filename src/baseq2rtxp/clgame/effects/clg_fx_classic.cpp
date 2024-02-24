/********************************************************************
*
*
*	ClientGame: Used to be just 'effects.c', therefor, now named classic.
*
*
********************************************************************/
#include "../clg_local.h"


// Wall Impact Puffs.
void CLG_ParticleEffect( const vec3_t org, const vec3_t dir, int color, int count ) {
    vec3_t oy;
    VectorSet( oy, 0.0f, 1.0f, 0.0f );
    if ( fabsf( DotProduct( oy, dir ) ) > 0.95f )
        VectorSet( oy, 1.0f, 0.0f, 0.0f );

    vec3_t ox;
    CrossProduct( oy, dir, ox );

    count *= cl_particle_num_factor->value;
    const int spark_count = count / 10;

    const float dirt_horizontal_spread = 2.0f;
    const float dirt_vertical_spread = 1.0f;
    const float dirt_base_velocity = 40.0f;
    const float dirt_rand_velocity = 70.0f;

    const float spark_horizontal_spread = 1.0f;
    const float spark_vertical_spread = 1.0f;
    const float spark_base_velocity = 50.0f;
    const float spark_rand_velocity = 130.0f;

    for ( int i = 0; i < count; i++ ) {
        cparticle_t *p = CLG_AllocParticle();
        if ( !p )
            return;

        p->time = clgi.client->time;

        p->color = color + ( Q_rand() & 7 );
        p->brightness = 0.5f;

        vec3_t origin;
        VectorCopy( org, origin );
        VectorMA( origin, dirt_horizontal_spread * crand(), ox, origin );
        VectorMA( origin, dirt_horizontal_spread * crand(), oy, origin );
        VectorMA( origin, dirt_vertical_spread * frand() + 1.0f, dir, origin );
        VectorCopy( origin, p->org );

        vec3_t velocity;
        VectorSubtract( origin, org, velocity );
        VectorNormalize( velocity );
        VectorScale( velocity, dirt_base_velocity + frand() * dirt_rand_velocity, p->vel );

        p->accel[ 0 ] = p->accel[ 1 ] = 0;
        p->accel[ 2 ] = -PARTICLE_GRAVITY;
        p->alpha = 1.0f;

        p->alphavel = -1.0f / ( 0.5f + frand() * 0.3f );
    }

    for ( int i = 0; i < spark_count; i++ ) {
        cparticle_t *p = CLG_AllocParticle();
        if ( !p )
            return;

        p->time = clgi.client->time;

        p->color = 0xe0 + ( Q_rand() & 7 );
        p->brightness = cvar_pt_particle_emissive->value;

        vec3_t origin;
        VectorCopy( org, origin );
        VectorMA( origin, spark_horizontal_spread * crand(), ox, origin );
        VectorMA( origin, spark_horizontal_spread * crand(), oy, origin );
        VectorMA( origin, spark_vertical_spread * frand() + 1.0f, dir, origin );
        VectorCopy( origin, p->org );

        vec3_t velocity;
        VectorSubtract( origin, org, velocity );
        VectorNormalize( velocity );
        VectorScale( velocity, spark_base_velocity + powf( frand(), 2.0f ) * spark_rand_velocity, p->vel );

        p->accel[ 0 ] = p->accel[ 1 ] = 0;
        p->accel[ 2 ] = -PARTICLE_GRAVITY;
        p->alpha = 1.0f;

        p->alphavel = -2.0f / ( 0.5f + frand() * 0.3f );
    }
}

void CLG_ParticleEffectWaterSplash( const vec3_t org, const vec3_t dir, int color, int count ) {
    vec3_t oy;
    VectorSet( oy, 0.0f, 1.0f, 0.0f );
    if ( fabsf( DotProduct( oy, dir ) ) > 0.95f )
        VectorSet( oy, 1.0f, 0.0f, 0.0f );

    vec3_t ox;
    CrossProduct( oy, dir, ox );

    count *= cl_particle_num_factor->value;

    const float water_horizontal_spread = 0.25f;
    const float water_vertical_spread = 1.0f;
    const float water_base_velocity = 80.0f;
    const float water_rand_velocity = 150.0f;

    for ( int i = 0; i < count; i++ ) {
        cparticle_t *p = CLG_AllocParticle();
        if ( !p )
            return;

        p->time = clgi.client->time;

        p->color = color + ( Q_rand() & 7 );
        p->brightness = 1.0f;

        vec3_t origin;
        VectorCopy( org, origin );
        VectorMA( origin, water_horizontal_spread * crand(), ox, origin );
        VectorMA( origin, water_horizontal_spread * crand(), oy, origin );
        VectorMA( origin, water_vertical_spread * frand() + 1.0f, dir, origin );
        VectorCopy( origin, p->org );

        vec3_t velocity;
        VectorSubtract( origin, org, velocity );
        VectorNormalize( velocity );
        VectorScale( velocity, water_base_velocity + frand() * water_rand_velocity, p->vel );

        p->accel[ 0 ] = p->accel[ 1 ] = 0;
        p->accel[ 2 ] = -PARTICLE_GRAVITY;
        p->alpha = 1.0f;

        p->alphavel = -1.0f / ( 0.5f + frand() * 0.3f );
    }
}

void CLG_BloodParticleEffect( const vec3_t org, const vec3_t dir, int color, int count ) {
    int         i, j;
    cparticle_t *p;
    float       d;

    // add decal:
    decal_t dec = {
      .pos = {org[ 0 ],org[ 1 ],org[ 2 ]},
      .dir = {dir[ 0 ],dir[ 1 ],dir[ 2 ]},
      .spread = 0.25f,
      .length = 350
    };
    clgi.R_AddDecal( &dec );

    float a[ 3 ] = { dir[ 1 ], -dir[ 2 ], dir[ 0 ] };
    float b[ 3 ] = { -dir[ 2 ], dir[ 0 ], dir[ 1 ] };

    count *= cl_particle_num_factor->value;

    for ( i = 0; i < count; i++ ) {
        p = CLG_AllocParticle();
        if ( !p )
            return;

        p->time = clgi.client->time;

        p->color = color + ( Q_rand() & 7 );
        p->brightness = 0.5f;

        d = ( Q_rand() & 31 ) * 10.0f;
        for ( j = 0; j < 3; j++ ) {
            p->org[ j ] = org[ j ] + ( (int)( Q_rand() & 7 ) - 4 ) + d * ( dir[ j ]
                + a[ j ] * 0.5f * ( ( Q_rand() & 31 ) / 32.0f - .5f )
                + b[ j ] * 0.5f * ( ( Q_rand() & 31 ) / 32.0f - .5f ) );

            p->vel[ j ] = 10.0f * dir[ j ] + crand() * 20;
        }
        // fake gravity
        p->org[ 2 ] -= d * d * .001f;

        p->accel[ 0 ] = p->accel[ 1 ] = 0;
        p->accel[ 2 ] = -PARTICLE_GRAVITY;
        p->alpha = 0.5f;

        p->alphavel = -1.0f / ( 0.5f + frand() * 0.3f );
    }
}


/*
===============
CLG_ParticleEffect2
===============
*/
void CLG_ParticleEffect2( const vec3_t org, const vec3_t dir, int color, int count ) {
    int         i, j;
    cparticle_t *p;
    float       d;

    count *= cl_particle_num_factor->value;

    for ( i = 0; i < count; i++ ) {
        p = CLG_AllocParticle();
        if ( !p )
            return;

        p->time = clgi.client->time;

        p->color = color;
        p->brightness = 1.0f;

        d = Q_rand() & 7;
        for ( j = 0; j < 3; j++ ) {
            p->org[ j ] = org[ j ] + ( (int)( Q_rand() & 7 ) - 4 ) + d * dir[ j ];
            p->vel[ j ] = crand() * 20;
        }

        p->accel[ 0 ] = p->accel[ 1 ] = 0;
        p->accel[ 2 ] = -PARTICLE_GRAVITY;
        p->alpha = 1.0f;

        p->alphavel = -1.0f / ( 0.5f + frand() * 0.3f );
    }
}


/*
===============
CLG_TeleporterParticles
===============
*/
void CLG_TeleporterParticles( const vec3_t org ) {
    int         i, j;
    cparticle_t *p;

    const int count = 8 * cl_particle_num_factor->value;

    for ( i = 0; i < count; i++ ) {
        p = CLG_AllocParticle();
        if ( !p )
            return;

        p->time = clgi.client->time;

        p->color = 0xdb;
        p->brightness = 1.0f;

        for ( j = 0; j < 2; j++ ) {
            p->org[ j ] = org[ j ] - 16 + ( Q_rand() & 31 );
            p->vel[ j ] = crand() * 14;
        }

        p->org[ 2 ] = org[ 2 ] - 8 + ( Q_rand() & 7 );
        p->vel[ 2 ] = 80 + ( Q_rand() & 7 );

        p->accel[ 0 ] = p->accel[ 1 ] = 0;
        p->accel[ 2 ] = -PARTICLE_GRAVITY;
        p->alpha = 1.0f;

        p->alphavel = -0.5f;
    }
}


/*
===============
CLG_LogoutEffect

===============
*/
void CLG_LogoutEffect( const vec3_t org, int type ) {
    int         i, j;
    cparticle_t *p;

    for ( i = 0; i < 500; i++ ) {
        p = CLG_AllocParticle();
        if ( !p )
            return;

        p->time = clgi.client->time;

        int color;
        if ( type == MZ_LOGIN )
            color = 0xd0 + ( Q_rand() & 7 ); // green
        else if ( type == MZ_LOGOUT )
            color = 0x40 + ( Q_rand() & 7 ); // red
        else
            color = 0xe0 + ( Q_rand() & 7 ); // yellow

        p->color = color;
        p->brightness = 1.0f;

        p->org[ 0 ] = org[ 0 ] - 16 + frand() * 32;
        p->org[ 1 ] = org[ 1 ] - 16 + frand() * 32;
        p->org[ 2 ] = org[ 2 ] - 24 + frand() * 56;

        for ( j = 0; j < 3; j++ )
            p->vel[ j ] = crand() * 20;

        p->accel[ 0 ] = p->accel[ 1 ] = 0;
        p->accel[ 2 ] = -PARTICLE_GRAVITY;
        p->alpha = 1.0f;

        p->alphavel = -1.0f / ( 1.0f + frand() * 0.3f );
    }
}


/*
===============
CLG_ItemRespawnParticles

===============
*/
void CLG_ItemRespawnParticles( const vec3_t org ) {
    int         i, j;
    cparticle_t *p;

    const int count = 64 * cl_particle_num_factor->value;

    for ( i = 0; i < count; i++ ) {
        p = CLG_AllocParticle();
        if ( !p )
            return;

        p->time = clgi.client->time;

        p->color = 0xd4 + ( Q_rand() & 3 ); // green
        p->brightness = 1.0f;

        p->org[ 0 ] = org[ 0 ] + crand() * 8;
        p->org[ 1 ] = org[ 1 ] + crand() * 8;
        p->org[ 2 ] = org[ 2 ] + crand() * 8;

        for ( j = 0; j < 3; j++ )
            p->vel[ j ] = crand() * 8;

        p->accel[ 0 ] = p->accel[ 1 ] = 0;
        p->accel[ 2 ] = -PARTICLE_GRAVITY * 0.2f;
        p->alpha = 1.0f;

        p->alphavel = -1.0f / ( 1.0f + frand() * 0.3f );
    }
}


/*
===============
CLG_ExplosionParticles
===============
*/
void CLG_ExplosionParticles( const vec3_t org ) {
    int         i, j;
    cparticle_t *p;

    const int count = 256 * cl_particle_num_factor->value;

    for ( i = 0; i < count; i++ ) {
        p = CLG_AllocParticle();
        if ( !p )
            return;

        p->time = clgi.client->time;

        p->color = 0xe0 + ( Q_rand() & 7 );
        p->brightness = cvar_pt_particle_emissive->value;

        for ( j = 0; j < 3; j++ ) {
            p->org[ j ] = org[ j ] + ( (int)( irandom( 32 ) ) - 16 );
            p->vel[ j ] = (int)( irandom( 384 ) ) - 192;
        }

        p->accel[ 0 ] = p->accel[ 1 ] = 0;
        p->accel[ 2 ] = -PARTICLE_GRAVITY;
        p->alpha = 1.0f;

        p->alphavel = -0.8f / ( 0.5f + frand() * 0.3f );
    }
}

/*
===============
CLG_BigTeleportParticles
===============
*/
void CLG_BigTeleportParticles( const vec3_t org ) {
    static const byte   colortable[ 4 ] = { 2 * 8, 13 * 8, 21 * 8, 18 * 8 };
    int         i;
    cparticle_t *p;
    float       angle, dist;

    for ( i = 0; i < 4096; i++ ) {
        p = CLG_AllocParticle();
        if ( !p )
            return;

        p->time = clgi.client->time;

        p->color = colortable[ Q_rand() & 3 ];
        p->brightness = 1.0f;

        angle = ( Q_rand() & 1023 ) * ( M_PI * 2 / 1023 );
        dist = Q_rand() & 31;
        p->org[ 0 ] = org[ 0 ] + cos( angle ) * dist;
        p->vel[ 0 ] = cos( angle ) * ( 70 + ( Q_rand() & 63 ) );
        p->accel[ 0 ] = -cos( angle ) * 100;

        p->org[ 1 ] = org[ 1 ] + sin( angle ) * dist;
        p->vel[ 1 ] = sin( angle ) * ( 70 + ( Q_rand() & 63 ) );
        p->accel[ 1 ] = -sin( angle ) * 100;

        p->org[ 2 ] = org[ 2 ] + 8 + ( irandom( 90 ) );
        p->vel[ 2 ] = -100 + ( Q_rand() & 31 );
        p->accel[ 2 ] = PARTICLE_GRAVITY * 4;
        p->alpha = 1.0f;

        p->alphavel = -0.3f / ( 0.5f + frand() * 0.3f );
    }
}


/*
===============
CLG_BlasterParticles

Wall impact puffs
===============
*/
void CLG_BlasterParticles( const vec3_t org, const vec3_t dir ) {
    int         i, j;
    cparticle_t *p;
    float       d;

    const int count = 40 * cl_particle_num_factor->value;

    for ( i = 0; i < count; i++ ) {
        p = CLG_AllocParticle();
        if ( !p )
            return;

        p->time = clgi.client->time;

        p->color = 0xe0 + ( Q_rand() & 7 );
        p->brightness = cvar_pt_particle_emissive->value;

        d = Q_rand() & 15;
        for ( j = 0; j < 3; j++ ) {
            p->org[ j ] = org[ j ] + ( (int)( Q_rand() & 7 ) - 4 ) + d * dir[ j ];
            p->vel[ j ] = dir[ j ] * 30 + crand() * 40;
        }

        p->accel[ 0 ] = p->accel[ 1 ] = 0;
        p->accel[ 2 ] = -PARTICLE_GRAVITY;
        p->alpha = 1.0f;

        p->alphavel = -1.0f / ( 0.5f + frand() * 0.3f );
    }
}


/*
===============
CLG_BlasterTrail

===============
*/
void CLG_BlasterTrail( const vec3_t start, const vec3_t end ) {
    vec3_t      move;
    vec3_t      vec;
    float       len;
    int         j;
    cparticle_t *p;
    int         dec;

    VectorCopy( start, move );
    VectorSubtract( end, start, vec );
    len = VectorNormalize( vec );

    dec = 5;
    VectorScale( vec, 5, vec );

    // FIXME: this is a really silly way to have a loop
    while ( len > 0 ) {
        len -= dec;

        p = CLG_AllocParticle();
        if ( !p )
            return;
        VectorClear( p->accel );

        p->time = clgi.client->time;

        p->alpha = 1.0;
        p->alphavel = -1.0f / ( 0.3f + frand() * 0.2f );

        p->color = 0xe0;
        p->brightness = cvar_pt_particle_emissive->value;

        for ( j = 0; j < 3; j++ ) {
            p->org[ j ] = move[ j ] + crand();
            p->vel[ j ] = crand() * 5;
            p->accel[ j ] = 0;
        }

        VectorAdd( move, vec, move );
    }
}

/*
===============
CLG_FlagTrail

===============
*/
void CLG_FlagTrail( const vec3_t start, const vec3_t end, int color ) {
    vec3_t      move;
    vec3_t      vec;
    float       len;
    int         j;
    cparticle_t *p;
    int         dec;

    VectorCopy( start, move );
    VectorSubtract( end, start, vec );
    len = VectorNormalize( vec );

    dec = 5;
    VectorScale( vec, 5, vec );

    while ( len > 0 ) {
        len -= dec;

        p = CLG_AllocParticle();
        if ( !p )
            return;
        VectorClear( p->accel );

        p->time = clgi.client->time;

        p->alpha = 1.0;
        p->alphavel = -1.0f / ( 0.8f + frand() * 0.2f );

        p->color = color;
        p->brightness = 1.0f;

        for ( j = 0; j < 3; j++ ) {
            p->org[ j ] = move[ j ] + crand() * 16;
            p->vel[ j ] = crand() * 5;
            p->accel[ j ] = 0;
        }

        VectorAdd( move, vec, move );
    }
}

/*
===============
CLG_DiminishingTrail

===============
*/
void CLG_DiminishingTrail( const vec3_t start, const vec3_t end, centity_t *old, int flags ) {
    vec3_t      move;
    vec3_t      vec;
    float       len;
    int         j;
    cparticle_t *p;
    float       dec;
    float       orgscale;
    float       velscale;

    VectorCopy( start, move );
    VectorSubtract( end, start, vec );
    len = VectorNormalize( vec );

    dec = 0.5f;
    VectorScale( vec, dec, vec );

    if ( old->trailcount > 900 ) {
        orgscale = 4;
        velscale = 15;
    } else if ( old->trailcount > 800 ) {
        orgscale = 2;
        velscale = 10;
    } else {
        orgscale = 1;
        velscale = 5;
    }

    while ( len > 0 ) {
        len -= dec;

        // drop less particles as it flies
        if ( ( Q_rand() & 1023 ) < old->trailcount ) {
            p = CLG_AllocParticle();
            if ( !p )
                return;
            VectorClear( p->accel );

            p->time = clgi.client->time;

            if ( flags & EF_GIB ) {
                p->alpha = 1.0;
                p->alphavel = -1.0f / ( 1 + frand() * 0.4f );

                p->color = 0xe8 + ( Q_rand() & 7 );
                p->brightness = 1.0f;

                for ( j = 0; j < 3; j++ ) {
                    p->org[ j ] = move[ j ] + crand() * orgscale;
                    p->vel[ j ] = crand() * velscale;
                    p->accel[ j ] = 0;
                }
                p->vel[ 2 ] -= PARTICLE_GRAVITY;
            } else if ( flags & EF_GREENGIB ) {
                p->alpha = 1.0f;
                p->alphavel = -1.0f / ( 1 + frand() * 0.4f );

                p->color = 0xdb + ( Q_rand() & 7 );
                p->brightness = 1.0f;

                for ( j = 0; j < 3; j++ ) {
                    p->org[ j ] = move[ j ] + crand() * orgscale;
                    p->vel[ j ] = crand() * velscale;
                    p->accel[ j ] = 0;
                }
                p->vel[ 2 ] -= PARTICLE_GRAVITY;
            } else {
                p->alpha = 1.0f;
                p->alphavel = -1.0f / ( 1 + frand() * 0.2f );

                p->color = 4 + ( Q_rand() & 7 );
                p->brightness = 1.0f;

                for ( j = 0; j < 3; j++ ) {
                    p->org[ j ] = move[ j ] + crand() * orgscale;
                    p->vel[ j ] = crand() * velscale;
                }
                p->accel[ 2 ] = 20;
            }
        }

        old->trailcount -= 5;
        if ( old->trailcount < 100 )
            old->trailcount = 100;
        VectorAdd( move, vec, move );
    }
}

/*
===============
CLG_RocketTrail

===============
*/
void CLG_RocketTrail( const vec3_t start, const vec3_t end, centity_t *old ) {
    vec3_t      move;
    vec3_t      vec;
    float       len;
    int         j;
    cparticle_t *p;
    float       dec;

    // smoke
    CLG_DiminishingTrail( start, end, old, EF_ROCKET );

    // fire
    VectorCopy( start, move );
    VectorSubtract( end, start, vec );
    len = VectorNormalize( vec );

    dec = 1;
    VectorScale( vec, dec, vec );

    while ( len > 0 ) {
        len -= dec;

        if ( ( Q_rand() & 7 ) == 0 ) {
            p = CLG_AllocParticle();
            if ( !p )
                return;

            VectorClear( p->accel );
            p->time = clgi.client->time;

            p->alpha = 1.0;
            p->alphavel = -1.0f / ( 1 + frand() * 0.2f );

            p->color = 0xdc + ( Q_rand() & 3 );
            p->brightness = cvar_pt_particle_emissive->value;

            for ( j = 0; j < 3; j++ ) {
                p->org[ j ] = move[ j ] + crand() * 5;
                p->vel[ j ] = crand() * 20;
            }
            p->accel[ 2 ] = -PARTICLE_GRAVITY;
        }
        VectorAdd( move, vec, move );
    }
}

/*
===============
CLG_RailTrail

===============
*/
void CLG_OldRailTrail( void ) {
    vec3_t      move;
    vec3_t      vec;
    float       len;
    int         j;
    cparticle_t *p;
    float       dec;
    vec3_t      right, up;
    int         i;
    float       d, c, s;
    vec3_t      dir;
    byte        clr = 0x74;

    VectorCopy( level.parsedMessage.events.tempEntity.pos1, move );
    VectorSubtract( level.parsedMessage.events.tempEntity.pos2, level.parsedMessage.events.tempEntity.pos1, vec );
    len = VectorNormalize( vec );

    MakeNormalVectors( vec, right, up );

    for ( i = 0; i < len; i++ ) {
        p = CLG_AllocParticle();
        if ( !p )
            return;

        p->time = clgi.client->time;
        VectorClear( p->accel );

        d = i * 0.1;
        c = cos( d );
        s = sin( d );

        VectorScale( right, c, dir );
        VectorMA( dir, s, up, dir );

        p->alpha = 1.0;
        p->alphavel = -1.0f / ( 1 + frand() * 0.2f );

        p->color = clr + ( Q_rand() & 7 );
        p->brightness = cvar_pt_particle_emissive->value;

        for ( j = 0; j < 3; j++ ) {
            p->org[ j ] = move[ j ] + dir[ j ] * 3;
            p->vel[ j ] = dir[ j ] * 6;
        }

        VectorAdd( move, vec, move );
    }

    dec = 0.75f;
    VectorScale( vec, dec, vec );
    VectorCopy( level.parsedMessage.events.tempEntity.pos1, move );

    while ( len > 0 ) {
        len -= dec;

        p = CLG_AllocParticle();
        if ( !p )
            return;

        p->time = clgi.client->time;
        VectorClear( p->accel );

        p->alpha = 1.0;
        p->alphavel = -1.0f / ( 0.6f + frand() * 0.2f );

        p->color = Q_rand() & 15;
        p->brightness = 1.0f;

        for ( j = 0; j < 3; j++ ) {
            p->org[ j ] = move[ j ] + crand() * 3;
            p->vel[ j ] = crand() * 3;
            p->accel[ j ] = 0;
        }

        VectorAdd( move, vec, move );
    }
}


/*
===============
CLG_BubbleTrail

===============
*/
void CLG_BubbleTrail( const vec3_t start, const vec3_t end ) {
    vec3_t      move;
    vec3_t      vec;
    float       len;
    int         i, j;
    cparticle_t *p;
    float       dec;

    VectorCopy( start, move );
    VectorSubtract( end, start, vec );
    len = VectorNormalize( vec );

    dec = 32;
    VectorScale( vec, dec, vec );

    for ( i = 0; i < len; i += dec ) {
        p = CLG_AllocParticle();
        if ( !p )
            return;

        VectorClear( p->accel );
        p->time = clgi.client->time;

        p->alpha = 1.0;
        p->alphavel = -1.0f / ( 1 + frand() * 0.2 );

        p->color = 4 + ( Q_rand() & 7 );
        p->brightness = 1.0f;

        for ( j = 0; j < 3; j++ ) {
            p->org[ j ] = move[ j ] + crand() * 2;
            p->vel[ j ] = crand() * 5;
        }
        p->vel[ 2 ] += 6;

        VectorAdd( move, vec, move );
    }
}


/*
===============
CLG_FlyParticles
===============
*/

#define BEAMLENGTH  16

static void CLG_FlyParticles( const vec3_t origin, int count ) {
    int         i;
    cparticle_t *p;
    float       angle;
    float       sp, sy, cp, cy;
    vec3_t      forward;
    float       dist;
    float       ltime;

    if ( count > NUMVERTEXNORMALS )
        count = NUMVERTEXNORMALS;

    ltime = clgi.client->time * 0.001f;
    for ( i = 0; i < count; i += 2 ) {
        angle = ltime * game.avelocities[ i ][ 0 ];
        sy = sin( angle );
        cy = cos( angle );
        angle = ltime * game.avelocities[ i ][ 1 ];
        sp = sin( angle );
        cp = cos( angle );

        forward[ 0 ] = cp * cy;
        forward[ 1 ] = cp * sy;
        forward[ 2 ] = -sp;

        p = CLG_AllocParticle();
        if ( !p )
            return;

        p->time = clgi.client->time;

        dist = sin( ltime + i ) * 64;
        p->org[ 0 ] = origin[ 0 ] + bytedirs[ i ][ 0 ] * dist + forward[ 0 ] * BEAMLENGTH;
        p->org[ 1 ] = origin[ 1 ] + bytedirs[ i ][ 1 ] * dist + forward[ 1 ] * BEAMLENGTH;
        p->org[ 2 ] = origin[ 2 ] + bytedirs[ i ][ 2 ] * dist + forward[ 2 ] * BEAMLENGTH;

        VectorClear( p->vel );
        VectorClear( p->accel );

        p->color = 0;
        p->brightness = 1.0f;

        p->alpha = 1;
        p->alphavel = INSTANT_PARTICLE;
    }
}

void CLG_FlyEffect( centity_t *ent, const vec3_t origin ) {
    int     n;
    int     count;
    int     starttime;

    if ( ent->fly_stoptime < clgi.client->time ) {
        starttime = clgi.client->time;
        ent->fly_stoptime = clgi.client->time + 60000;
    } else {
        starttime = ent->fly_stoptime - 60000;
    }

    n = clgi.client->time - starttime;
    if ( n < 20000 )
        count = n * NUMVERTEXNORMALS / 20000;
    else {
        n = ent->fly_stoptime - clgi.client->time;
        if ( n < 20000 )
            count = n * NUMVERTEXNORMALS / 20000;
        else
            count = NUMVERTEXNORMALS;
    }

    CLG_FlyParticles( origin, count );
}

/*
===============
CLG_BfgParticles
===============
*/
void CLG_BfgParticles( entity_t *ent ) {
    int         i;
    cparticle_t *p;
    float       angle;
    float       sp, sy, cp, cy;
    vec3_t      forward;
    float       dist;
    float       ltime;

    const int count = NUMVERTEXNORMALS * cl_particle_num_factor->value;

    ltime = clgi.client->time * 0.001f;
    for ( i = 0; i < count; i++ ) {
        angle = ltime * game.avelocities[ i ][ 0 ];
        sy = sin( angle );
        cy = cos( angle );
        angle = ltime * game.avelocities[ i ][ 1 ];
        sp = sin( angle );
        cp = cos( angle );

        forward[ 0 ] = cp * cy;
        forward[ 1 ] = cp * sy;
        forward[ 2 ] = -sp;

        p = CLG_AllocParticle();
        if ( !p )
            return;

        p->time = clgi.client->time;

        dist = sin( ltime + i ) * 64;
        p->org[ 0 ] = ent->origin[ 0 ] + bytedirs[ i ][ 0 ] * dist + forward[ 0 ] * BEAMLENGTH;
        p->org[ 1 ] = ent->origin[ 1 ] + bytedirs[ i ][ 1 ] * dist + forward[ 1 ] * BEAMLENGTH;
        p->org[ 2 ] = ent->origin[ 2 ] + bytedirs[ i ][ 2 ] * dist + forward[ 2 ] * BEAMLENGTH;

        VectorClear( p->vel );
        VectorClear( p->accel );

        dist = Distance( p->org, ent->origin ) / 90.0f;
        p->color = floor( 0xd0 + dist * 7 );
        p->brightness = cvar_pt_particle_emissive->value;

        p->alpha = 1.0f - dist;
        p->alphavel = INSTANT_PARTICLE;
    }
}


/*
===============
CLG_BFGExplosionParticles
===============
*/
//FIXME combined with CLG_ExplosionParticles
void CLG_BFGExplosionParticles( const vec3_t org ) {
    int         i, j;
    cparticle_t *p;

    const int count = 256 * cl_particle_num_factor->value;

    for ( i = 0; i < count; i++ ) {
        p = CLG_AllocParticle();
        if ( !p )
            return;

        p->time = clgi.client->time;

        p->color = 0xd0 + ( Q_rand() & 7 );
        p->brightness = cvar_pt_particle_emissive->value;

        for ( j = 0; j < 3; j++ ) {
            p->org[ j ] = org[ j ] + ( (int)( irandom( 32 ) ) - 16 );
            p->vel[ j ] = (int)( irandom( 384 ) ) - 192;
        }

        p->accel[ 0 ] = p->accel[ 1 ] = 0;
        p->accel[ 2 ] = -PARTICLE_GRAVITY;
        p->alpha = 1.0f;

        p->alphavel = -0.8f / ( 0.5f + frand() * 0.3f );
    }
}


/*
===============
CLG_TeleportParticles

===============
*/
void CLG_TeleportParticles( const vec3_t org ) {
    int         i, j, k;
    cparticle_t *p;
    float       vel;
    vec3_t      dir;

    for ( i = -16; i <= 16; i += 4 )
        for ( j = -16; j <= 16; j += 4 )
            for ( k = -16; k <= 32; k += 4 ) {
                p = CLG_AllocParticle();
                if ( !p )
                    return;

                p->time = clgi.client->time;

                p->color = 7 + ( Q_rand() & 7 );
                p->brightness = 1.0f;

                p->alpha = 1.0f;
                p->alphavel = -1.0f / ( 0.3f + ( Q_rand() & 7 ) * 0.02f );

                p->org[ 0 ] = org[ 0 ] + i + ( Q_rand() & 3 );
                p->org[ 1 ] = org[ 1 ] + j + ( Q_rand() & 3 );
                p->org[ 2 ] = org[ 2 ] + k + ( Q_rand() & 3 );

                dir[ 0 ] = j * 8;
                dir[ 1 ] = i * 8;
                dir[ 2 ] = k * 8;

                VectorNormalize( dir );
                vel = 50 + ( Q_rand() & 63 );
                VectorScale( dir, vel, p->vel );

                p->accel[ 0 ] = p->accel[ 1 ] = 0;
                p->accel[ 2 ] = -PARTICLE_GRAVITY;
            }
}
