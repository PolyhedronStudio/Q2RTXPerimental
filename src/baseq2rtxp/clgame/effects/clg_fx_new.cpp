/********************************************************************
*
*
*	ClientGame: This used to be client/newfx.cpp.
*
*
********************************************************************/
#include "clgame/clg_local.h"
#include "clgame/clg_effects.h"


void CLG_Flashlight( int ent, const Vector3 &pos ) {
    clg_dlight_t *dl;

    dl = CLG_AllocDlight( ent );
    VectorCopy( pos, dl->origin );
    dl->radius = 400;
    dl->die = clgi.client->time + 100;
    VectorSet( dl->color, 1, 1, 1 );
}

/*
======
CLG_ColorFlash - flash of light
======
*/
void CLG_ColorFlash( const Vector3 &pos, int ent, int intensity, float r, float g, float b ) {
    clg_dlight_t *dl;

    dl = CLG_AllocDlight( ent );
    VectorCopy( pos, dl->origin );
    dl->radius = intensity;
    dl->die = clgi.client->time + 100;
    VectorSet( dl->color, r, g, b );
}

/*
======
CLG_DebugTrail
======
*/
void CLG_DebugTrail( const Vector3 &start, const Vector3 &end ) {
    vec3_t      move;
    vec3_t      vec;
    float       len;
    clg_particle_t *p;
    float       dec;
    vec3_t      right, up;

    VectorCopy( start, move );
    VectorSubtract( end, start, vec );
    len = VectorNormalize( vec );

    MakeNormalVectors( vec, right, up );

    dec = 3;
    VectorScale( vec, dec, vec );
    VectorCopy( start, move );

    while ( len > 0 ) {
        len -= dec;

        p = CLG_AllocParticle();
        if ( !p )
            return;

        p->time = clgi.client->time;
        VectorClear( p->accel );
        VectorClear( p->vel );
        p->alpha = 1.0f;
        p->alphavel = -0.1f;
        p->color = 0x74 + ( Q_rand() & 7 );
        VectorCopy( move, p->org );
        VectorAdd( move, vec, move );
    }
}

void CLG_ForceWall( const Vector3 &start, const Vector3 &end, int color ) {
    vec3_t      move;
    vec3_t      vec;
    float       len;
    int         j;
    clg_particle_t *p;

    VectorCopy( start, move );
    VectorSubtract( end, start, vec );
    len = VectorNormalize( vec );

    VectorScale( vec, 4, vec );

    // FIXME: this is a really silly way to have a loop
    while ( len > 0 ) {
        len -= 4;

        if ( frand() > 0.3f ) {
            p = CLG_AllocParticle();
            if ( !p )
                return;
            VectorClear( p->accel );

            p->time = clgi.client->time;

            p->alpha = 1.0f;
            p->alphavel = -1.0f / ( 3.0f + frand() * 0.5f );
            p->color = color;
            for ( j = 0; j < 3; j++ ) {
                p->org[ j ] = move[ j ] + crand() * 3;
                p->accel[ j ] = 0;
            }
            p->vel[ 0 ] = 0;
            p->vel[ 1 ] = 0;
            p->vel[ 2 ] = -40 - ( crand() * 10 );
        }

        VectorAdd( move, vec, move );
    }
}


/*
===============
CLG_BubbleTrail2 (lets you control the # of bubbles by setting the distance between the spawns)

===============
*/
void CLG_BubbleTrail2( const Vector3 &start, const Vector3 &end, int dist ) {
    vec3_t      move;
    vec3_t      vec;
    float       len;
    int         i, j;
    clg_particle_t *p;
    float       dec;

    VectorCopy( start, move );
    VectorSubtract( end, start, vec );
    len = VectorNormalize( vec );

    dec = dist;
    VectorScale( vec, dec, vec );

    for ( i = 0; i < len; i += dec ) {
        p = CLG_AllocParticle();
        if ( !p )
            return;

        VectorClear( p->accel );
        p->time = clgi.client->time;

        p->alpha = 1.0f;
        p->alphavel = -1.0f / ( 1 + frand() * 0.1f );
        p->color = 4 + ( Q_rand() & 7 );
        for ( j = 0; j < 3; j++ ) {
            p->org[ j ] = move[ j ] + crand() * 2;
            p->vel[ j ] = crand() * 10;
        }
        p->org[ 2 ] -= 4;
        p->vel[ 2 ] += 20;

        VectorAdd( move, vec, move );
    }
}

void CLG_Heatbeam( const Vector3 &start, const Vector3 &forward ) {
    vec3_t      move;
    vec3_t      vec;
    float       len;
    int         j;
    clg_particle_t *p;
    int         i;
    float       c, s;
    vec3_t      dir;
    float       ltime;
    float       step = 32.0f, rstep;
    float       start_pt;
    float       rot;
    float       variance;
    vec3_t      end;

    VectorMA( start, 4096, forward, end );

    VectorCopy( start, move );
    VectorSubtract( end, start, vec );
    len = VectorNormalize( vec );

    ltime = clgi.client->time * 0.001f;
    start_pt = fmod( ltime * 96.0f, step );
    VectorMA( move, start_pt, vec, move );

    VectorScale( vec, step, vec );

    rstep = M_PI / 10.0f;
    for ( i = start_pt; i < len; i += step ) {
        if ( i > step * 5 ) // don't bother after the 5th ring
            break;

        for ( rot = 0; rot < M_PI * 2; rot += rstep ) {
            p = CLG_AllocParticle();
            if ( !p )
                return;

            p->time = clgi.client->time;
            VectorClear( p->accel );
            variance = 0.5f;
            c = cos( rot ) * variance;
            s = sin( rot ) * variance;

            // trim it so it looks like it's starting at the origin
            if ( i < 10 ) {
                VectorScale( clgi.client->v_right, c * ( i / 10.0f ), dir );
                VectorMA( dir, s * ( i / 10.0f ), clgi.client->v_up, dir );
            } else {
                VectorScale( clgi.client->v_right, c, dir );
                VectorMA( dir, s, clgi.client->v_up, dir );
            }

            p->alpha = 0.5f;
            p->alphavel = -1000.0f;
            p->color = 223 - ( Q_rand() & 7 );
            for ( j = 0; j < 3; j++ ) {
                p->org[ j ] = move[ j ] + dir[ j ] * 3;
                p->vel[ j ] = 0;
            }
        }

        VectorAdd( move, vec, move );
    }
}


/*
===============
CLG_ParticleSteamEffect

Puffs with velocity along direction, with some randomness thrown in
===============
*/
void CLG_ParticleSteamEffect( const Vector3 &org, const Vector3 &dir, int color, int count, int magnitude ) {
    int         i, j;
    clg_particle_t *p;
    float       d;
    vec3_t      r, u;


    MakeNormalVectors( &dir.x, r, u );

    for ( i = 0; i < count; i++ ) {
        p = CLG_AllocParticle();
        if ( !p )
            return;

        p->time = clgi.client->time;
        p->color = color + ( Q_rand() & 7 );

        for ( j = 0; j < 3; j++ ) {
            p->org[ j ] = org[ j ] + magnitude * 0.1f * crand();
        }
        VectorScale( dir, magnitude, p->vel );
        d = crand() * magnitude / 3;
        VectorMA( p->vel, d, r, p->vel );
        d = crand() * magnitude / 3;
        VectorMA( p->vel, d, u, p->vel );

        p->accel[ 0 ] = p->accel[ 1 ] = 0;
        p->accel[ 2 ] = -PARTICLE_GRAVITY / 2;
        p->alpha = 1.0f;

        p->alphavel = -1.0f / ( 0.5f + frand() * 0.3f );
    }
}

void CLG_ParticleSteamEffect2( clg_sustain_t *self ) {
    CLG_ParticleSteamEffect( self->org, self->dir, self->color, self->count, self->magnitude );
    self->nextthink += 100;
}

/*
===============
CLG_TrackerTrail
===============
*/
void CLG_TrackerTrail( const Vector3 &start, const Vector3 &end, int particleColor ) {
    vec3_t      move;
    vec3_t      vec;
    vec3_t      forward, right, up, angle_dir;
    float       len;
    int         j;
    clg_particle_t *p;
    int         dec;
    float       dist;

    VectorCopy( start, move );
    VectorSubtract( end, start, vec );
    len = VectorNormalize( vec );

    VectorCopy( vec, forward );
    QM_Vector3ToAngles( forward, angle_dir );
    AngleVectors( angle_dir, forward, right, up );

    dec = 3;
    VectorScale( vec, 3, vec );

    // FIXME: this is a really silly way to have a loop
    while ( len > 0 ) {
        len -= dec;

        p = CLG_AllocParticle();
        if ( !p )
            return;
        VectorClear( p->accel );

        p->time = clgi.client->time;

        p->alpha = 1.0f;
        p->alphavel = -2.0f;
        p->color = particleColor;
        dist = DotProduct( move, forward );
        VectorMA( move, 8 * cos( dist ), up, p->org );
        for ( j = 0; j < 3; j++ ) {
            p->vel[ j ] = 0;
            p->accel[ j ] = 0;
        }
        p->vel[ 2 ] = 5;

        VectorAdd( move, vec, move );
    }
}

void CLG_Tracker_Shell( const Vector3 &origin ) {
    vec3_t          dir;
    int             i;
    clg_particle_t *p;

    for ( i = 0; i < 300; i++ ) {
        p = CLG_AllocParticle();
        if ( !p )
            return;
        VectorClear( p->accel );

        p->time = clgi.client->time;

        p->alpha = 1.0f;
        p->alphavel = INSTANT_PARTICLE;
        p->color = 0;

        dir[ 0 ] = crand();
        dir[ 1 ] = crand();
        dir[ 2 ] = crand();
        VectorNormalize( dir );

        VectorMA( origin, 40, dir, p->org );
    }
}

void CLG_MonsterPlasma_Shell( const Vector3 &origin ) {
    vec3_t          dir;
    int             i;
    clg_particle_t *p;

    for ( i = 0; i < 40; i++ ) {
        p = CLG_AllocParticle();
        if ( !p )
            return;
        VectorClear( p->accel );

        p->time = clgi.client->time;

        p->alpha = 1.0f;
        p->alphavel = INSTANT_PARTICLE;
        p->color = 0xe0;

        dir[ 0 ] = crand();
        dir[ 1 ] = crand();
        dir[ 2 ] = crand();
        VectorNormalize( dir );

        VectorMA( origin, 10, dir, p->org );
    }
}

void CLG_Widowbeamout( clg_sustain_t *self ) {
    static const byte   colortable[ 4 ] = { 2 * 8, 13 * 8, 21 * 8, 18 * 8 };
    vec3_t          dir;
    int             i;
    clg_particle_t *p;
    float           ratio;

    ratio = 1.0f - ( ( (float)self->endtime - (float)clgi.client->time ) / 2100.0f );

    for ( i = 0; i < 300; i++ ) {
        p = CLG_AllocParticle();
        if ( !p )
            return;
        VectorClear( p->accel );

        p->time = clgi.client->time;

        p->alpha = 1.0f;
        p->alphavel = INSTANT_PARTICLE;
        p->color = colortable[ Q_rand() & 3 ];

        dir[ 0 ] = crand();
        dir[ 1 ] = crand();
        dir[ 2 ] = crand();
        VectorNormalize( dir );

        VectorMA( self->org, ( 45.0f * ratio ), dir, p->org );
    }
}

void CLG_Nukeblast( clg_sustain_t *self ) {
    static const byte   colortable[ 4 ] = { 110, 112, 114, 116 };
    vec3_t          dir;
    int             i;
    clg_particle_t *p;
    float           ratio;

    ratio = 1.0f - ( ( (float)self->endtime - (float)clgi.client->time ) / 1000.0f );

    for ( i = 0; i < 700; i++ ) {
        p = CLG_AllocParticle();
        if ( !p )
            return;
        VectorClear( p->accel );

        p->time = clgi.client->time;

        p->alpha = 1.0f;
        p->alphavel = INSTANT_PARTICLE;
        p->color = colortable[ Q_rand() & 3 ];

        dir[ 0 ] = crand();
        dir[ 1 ] = crand();
        dir[ 2 ] = crand();
        VectorNormalize( dir );

        VectorMA( self->org, ( 200.0f * ratio ), dir, p->org );
    }
}

void CLG_WidowSplash( void ) {
    static const byte   colortable[ 4 ] = { 2 * 8, 13 * 8, 21 * 8, 18 * 8 };
    int         i;
    clg_particle_t *p;
    vec3_t      dir;

    for ( i = 0; i < 256; i++ ) {
        p = CLG_AllocParticle();
        if ( !p )
            return;

        p->time = clgi.client->time;
        p->color = colortable[ Q_rand() & 3 ];

        dir[ 0 ] = crand();
        dir[ 1 ] = crand();
        dir[ 2 ] = crand();
        VectorNormalize( dir );
        VectorMA( level.parsedMessage.events.tempEntity.pos1, 45.0f, dir, p->org );
        VectorScale( dir, 40.0f, p->vel );

        p->accel[ 0 ] = p->accel[ 1 ] = 0;
        p->alpha = 1.0f;

        p->alphavel = -0.8f / ( 0.5f + frand() * 0.3f );
    }
}

/*
===============
CLG_TagTrail

===============
*/
void CLG_TagTrail( const Vector3 &start, const Vector3 &end, int color ) {
    vec3_t      move;
    vec3_t      vec;
    float       len;
    int         j;
    clg_particle_t *p;
    int         dec;

    VectorCopy( start, move );
    VectorSubtract( end, start, vec );
    len = VectorNormalize( vec );

    dec = 5;
    VectorScale( vec, 5, vec );

    while ( len >= 0 ) {
        len -= dec;

        p = CLG_AllocParticle();
        if ( !p )
            return;
        VectorClear( p->accel );

        p->time = clgi.client->time;

        p->alpha = 1.0f;
        p->alphavel = -1.0f / ( 0.8f + frand() * 0.2f );
        p->color = color;
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
CLG_ColorExplosionParticles
===============
*/
void CLG_ColorExplosionParticles( const Vector3 &org, int color, int run ) {
    int         i, j;
    clg_particle_t *p;

    for ( i = 0; i < 128; i++ ) {
        p = CLG_AllocParticle();
        if ( !p )
            return;

        p->time = clgi.client->time;
        p->color = color + ( irandom( run ) );

        for ( j = 0; j < 3; j++ ) {
            p->org[ j ] = org[ j ] + ( (int)( irandom( 32 ) ) - 16 );
            p->vel[ j ] = (int)( irandom( 256 ) ) - 128;
        }

        p->accel[ 0 ] = p->accel[ 1 ] = 0;
        p->accel[ 2 ] = -PARTICLE_GRAVITY;
        p->alpha = 1.0f;

        p->alphavel = -0.4f / ( 0.6f + frand() * 0.2f );
    }
}

/*
===============
CLG_ParticleSmokeEffect - like the steam effect, but unaffected by gravity
===============
*/
void CLG_ParticleSmokeEffect( const Vector3 &org, const Vector3 &dir, int color, int count, int magnitude ) {
    int         i, j;
    clg_particle_t *p;
    float       d;
    vec3_t      r, u;

    MakeNormalVectors( &dir.x, r, u );

    for ( i = 0; i < count; i++ ) {
        p = CLG_AllocParticle();
        if ( !p )
            return;

        p->time = clgi.client->time;
        p->color = color + ( Q_rand() & 7 );

        for ( j = 0; j < 3; j++ ) {
            p->org[ j ] = org[ j ] + magnitude * 0.1f * crand();
        }
        VectorScale( dir, magnitude, p->vel );
        d = crand() * magnitude / 3;
        VectorMA( p->vel, d, r, p->vel );
        d = crand() * magnitude / 3;
        VectorMA( p->vel, d, u, p->vel );

        p->accel[ 0 ] = p->accel[ 1 ] = p->accel[ 2 ] = 0;
        p->alpha = 1.0f;

        p->alphavel = -1.0f / ( 0.5f + frand() * 0.3f );
    }
}

/*
===============
CLG_BlasterParticles2

Wall impact puffs (Green)
===============
*/
void CLG_BlasterParticles2( const Vector3 &org, const Vector3 &dir, unsigned int color ) {
    int         i, j;
    clg_particle_t *p;
    float       d;
    int         count;

    count = 40;
    for ( i = 0; i < count; i++ ) {
        p = CLG_AllocParticle();
        if ( !p )
            return;

        p->time = clgi.client->time;
        p->color = color + ( Q_rand() & 7 );

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
CLG_BlasterTrail2

Green!
===============
*/
void CLG_BlasterTrail2( const Vector3 &start, const Vector3 &end ) {
    vec3_t      move;
    vec3_t      vec;
    float       len;
    int         j;
    clg_particle_t *p;
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

        p->alpha = 1.0f;
        p->alphavel = -1.0f / ( 0.3f + frand() * 0.2f );
        p->color = 0xd0;
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
CLG_IonripperTrail
===============
*/
void CLG_IonripperTrail( const Vector3 &start, const Vector3 &ent ) {
    vec3_t  move;
    vec3_t  vec;
    float   len;
    int     j;
    clg_particle_t *p;
    int     dec;
    int     left = 0;

    VectorCopy( start, move );
    VectorSubtract( ent, start, vec );
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
        p->alpha = 0.5f;
        p->alphavel = -1.0f / ( 0.3f + frand() * 0.2f );
        p->color = 0xe4 + ( Q_rand() & 3 );

        for ( j = 0; j < 3; j++ ) {
            p->org[ j ] = move[ j ];
            p->accel[ j ] = 0;
        }
        if ( left ) {
            left = 0;
            p->vel[ 0 ] = 10;
        } else {
            left = 1;
            p->vel[ 0 ] = -10;
        }

        p->vel[ 1 ] = 0;
        p->vel[ 2 ] = 0;

        VectorAdd( move, vec, move );
    }
}

/*
===============
CLG_TrapParticles
===============
*/
void CLG_TrapParticles( centity_t *ent, const Vector3 &origin ) {
    vec3_t      move;
    vec3_t      vec;
    vec3_t      start, end;
    float       len;
    int         j;
    clg_particle_t *p;
    int         dec;

    if ( clgi.client->time - ent->fly_stoptime < 10 )
        return;
    ent->fly_stoptime = clgi.client->time;

    VectorCopy( origin, start );
    VectorCopy( origin, end );
    start[ 2 ] -= 14;
    end[ 2 ] += 50;

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

        p->alpha = 1.0f;
        p->alphavel = -1.0f / ( 0.3f + frand() * 0.2f );
        p->color = 0xe0;
        for ( j = 0; j < 3; j++ ) {
            p->org[ j ] = move[ j ] + crand();
            p->vel[ j ] = crand() * 15;
            p->accel[ j ] = 0;
        }
        p->accel[ 2 ] = PARTICLE_GRAVITY;

        VectorAdd( move, vec, move );
    }

    {
        int         i, j, k;
        clg_particle_t *p;
        float       vel;
        vec3_t      dir;
        vec3_t      org;

        VectorCopy( origin, org );

        for ( i = -2; i <= 2; i += 4 )
            for ( j = -2; j <= 2; j += 4 )
                for ( k = -2; k <= 4; k += 4 ) {
                    p = CLG_AllocParticle();
                    if ( !p )
                        return;

                    p->time = clgi.client->time;
                    p->color = 0xe0 + ( Q_rand() & 3 );

                    p->alpha = 1.0f;
                    p->alphavel = -1.0f / ( 0.3f + ( Q_rand() & 7 ) * 0.02f );

                    p->org[ 0 ] = org[ 0 ] + i + ( ( Q_rand() & 23 ) * crand() );
                    p->org[ 1 ] = org[ 1 ] + j + ( ( Q_rand() & 23 ) * crand() );
                    p->org[ 2 ] = org[ 2 ] + k + ( ( Q_rand() & 23 ) * crand() );

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
}

/*
===============
CLG_ParticleEffect3
===============
*/
void CLG_ParticleEffect3( const Vector3 &org, const Vector3 &dir, int color, int count ) {
    int         i, j;
    clg_particle_t *p;
    float       d;

    for ( i = 0; i < count; i++ ) {
        p = CLG_AllocParticle();
        if ( !p )
            return;

        p->time = clgi.client->time;
        p->color = color;

        d = Q_rand() & 7;
        for ( j = 0; j < 3; j++ ) {
            p->org[ j ] = org[ j ] + ( (int)( Q_rand() & 7 ) - 4 ) + d * dir[ j ];
            p->vel[ j ] = crand() * 20;
        }

        p->accel[ 0 ] = p->accel[ 1 ] = 0;
        p->accel[ 2 ] = PARTICLE_GRAVITY;
        p->alpha = 1.0f;

        p->alphavel = -1.0f / ( 0.5f + frand() * 0.3f );
    }
}

