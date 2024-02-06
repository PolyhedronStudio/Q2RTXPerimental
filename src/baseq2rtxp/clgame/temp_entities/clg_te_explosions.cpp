/********************************************************************
*
*
*	ClientGame: Explosions.
*
*
********************************************************************/
#include "../clg_local.h"

explosion_t  clg_explosions[ MAX_EXPLOSIONS ];

void CLG_ClearExplosions( void ) {
    memset( clg_explosions, 0, sizeof( clg_explosions ) );
}

explosion_t *CLG_AllocExplosion( void ) {
    explosion_t *e, *oldest;
    int     i;
    int     time;

    for ( i = 0, e = clg_explosions; i < MAX_EXPLOSIONS; i++, e++ ) {
        if ( e->type == explosion_t::ex_free ) { // WID: C++20: Was without explosion_t::
            memset( e, 0, sizeof( *e ) );
            return e;
        }
    }
    // find the oldest explosion
    time = cl.time;
    oldest = clg_explosions;

    for ( i = 0, e = clg_explosions; i < MAX_EXPLOSIONS; i++, e++ ) {
        if ( e->start < time ) {
            time = e->start;
            oldest = e;
        }
    }
    memset( oldest, 0, sizeof( *oldest ) );
    return oldest;
}

explosion_t *CLG_PlainExplosion( bool big ) {
    explosion_t *ex;

    ex = CLG_AllocExplosion();
    VectorCopy( te.pos1, ex->ent.origin );
    ex->type = explosion_t::ex_poly; // WID: C++20: Was without explosion_t::
    ex->ent.flags = RF_FULLBRIGHT;
    ex->start = cl.servertime - CL_FRAMETIME;
    ex->light = 350;
    VectorSet( ex->lightcolor, 1.0f, 0.5f, 0.5f );
    ex->ent.angles[ 1 ] = Q_rand() % 360;

    int model_idx = Q_rand() % ( sizeof( cl_mod_explosions ) / sizeof( *cl_mod_explosions ) );
    model_t *sprite_model = MOD_ForHandle( cl_mod_explosions[ model_idx ] );

    if ( cl_explosion_sprites->integer && !big && sprite_model ) {
        ex->ent.model = cl_mod_explosions[ model_idx ];
        ex->frames = sprite_model->numframes;
        ex->frametime = cl_explosion_frametime->integer;
    } else {
        ex->ent.model = big ? cl_mod_explo4_big : cl_mod_explo4;
        ex->baseframe = 15 * ( Q_rand() & 1 );
        ex->frames = 15;
    }

    return ex;
}

/*
=================
CL_SmokeAndFlash
=================
*/
void CLG_SmokeAndFlash( const vec3_t origin ) {
    explosion_t *ex;

    ex = CLG_AllocExplosion();
    VectorCopy( origin, ex->ent.origin );
    ex->type = explosion_t::ex_misc; // WID: C++20: Was without explosion_t::
    ex->frames = 4;
    ex->ent.flags = RF_TRANSLUCENT | RF_NOSHADOW;
    ex->start = cl.servertime - CL_FRAMETIME;
    ex->ent.model = cl_mod_smoke;

    ex = CLG_AllocExplosion();
    VectorCopy( origin, ex->ent.origin );
    ex->type = explosion_t::ex_flash;
    ex->ent.flags = RF_FULLBRIGHT;
    ex->frames = 2;
    ex->start = cl.servertime - CL_FRAMETIME;
    ex->ent.model = cl_mod_flash;
}

#define LENGTH(a) ((sizeof (a)) / (sizeof(*(a))))

typedef struct light_curve_s {
    vec3_t color;
    float radius;
    float offset;
} light_curve_t;

static light_curve_t ex_poly_light[] = {
    { { 0.4f,       0.2f,       0.02f     }, 12.5f, 20.00f },
    { { 0.351563f,  0.175781f,  0.017578f }, 15.0f, 23.27f },
    { { 0.30625f,   0.153125f,  0.015312f }, 20.0f, 24.95f },
    { { 0.264062f,  0.132031f,  0.013203f }, 22.5f, 25.01f },
    { { 0.225f,     0.1125f,    0.01125f  }, 25.0f, 27.53f },
    { { 0.189063f,  0.094531f,  0.009453f }, 27.5f, 28.55f },
    { { 0.15625f,   0.078125f,  0.007813f }, 30.0f, 30.80f },
    { { 0.126563f,  0.063281f,  0.006328f }, 27.5f, 40.43f },
    { { 0.1f,       0.05f,      0.005f    }, 25.0f, 49.02f },
    { { 0.076563f,  0.038281f,  0.003828f }, 22.5f, 58.15f },
    { { 0.05625f,   0.028125f,  0.002812f }, 20.0f, 61.03f },
    { { 0.039063f,  0.019531f,  0.001953f }, 17.5f, 63.59f },
    { { 0.025f,     0.0125f,    0.00125f  }, 15.0f, 66.47f },
    { { 0.014063f,  0.007031f,  0.000703f }, 12.5f, 71.34f },
    { { 0.f,        0.f,        0.f       }, 10.0f, 72.00f }
};

static light_curve_t ex_blaster_light[] = {
    { { 0.04f,      0.02f,      0.0f      },  5.f, 15.00f },
    { { 0.2f,       0.15f,      0.01f     }, 15.f, 15.00f },
    { { 0.04f,      0.02f,      0.0f      },  5.f, 15.00f },
};

static void CL_AddExplosionLight( explosion_t *ex, float phase ) {
    int curve_size;
    light_curve_t *curve;

    switch ( ex->type ) {
    case explosion_t::ex_poly: // WID: C++20: Used to be without explosion_t::
        curve = ex_poly_light;
        curve_size = LENGTH( ex_poly_light );
        break;
    case explosion_t::ex_blaster:
        curve = ex_blaster_light;
        curve_size = LENGTH( ex_blaster_light );
        break;
    default:
        return;
    }

    float timeAlpha = ( (float)( curve_size - 1 ) ) * phase;
    int baseSample = (int)floorf( timeAlpha );
    baseSample = max( 0, min( curve_size - 2, baseSample ) );

    float w1 = timeAlpha - (float)( baseSample );
    float w0 = 1.f - w1;

    light_curve_t *s0 = curve + baseSample;
    light_curve_t *s1 = curve + baseSample + 1;

    float offset = w0 * s0->offset + w1 * s1->offset;
    float radius = w0 * s0->radius + w1 * s1->radius;

    vec3_t origin;
    vec3_t up;
    AngleVectors( ex->ent.angles, NULL, NULL, up );
    VectorMA( ex->ent.origin, offset, up, origin );

    vec3_t color;
    VectorClear( color );
    VectorMA( color, w0, s0->color, color );
    VectorMA( color, w1, s1->color, color );

    V_AddSphereLight( origin, 500.f, color[ 0 ], color[ 1 ], color[ 2 ], radius );
}

void CLG_AddExplosions( void ) {
    entity_t *ent;
    int         i;
    explosion_t *ex;
    float       frac;
    int         f;

    for ( i = 0, ex = clg_explosions; i < MAX_EXPLOSIONS; i++, ex++ ) {
        if ( ex->type == explosion_t::ex_free )
            continue;
        float inv_frametime = ex->frametime ? 1.f / (float)ex->frametime : BASE_1_FRAMETIME;
        frac = ( clgi.client->time - ex->start ) * inv_frametime;
        f = floor( frac );

        ent = &ex->ent;

        switch ( ex->type ) {
        case explosion_t::ex_mflash: // WID: C++20: This was without explosion_t::
            if ( f >= ex->frames - 1 )
                ex->type = explosion_t::ex_free;
            break;
        case explosion_t::ex_misc:
        case explosion_t::ex_blaster:
        case explosion_t::ex_flare:
        case explosion_t::ex_light:
            if ( f >= ex->frames - 1 ) {
                ex->type = explosion_t::ex_free;
                break;
            }
            ent->alpha = 1.0f - frac / ( ex->frames - 1 );
            break;
        case explosion_t::ex_flash:
            if ( f >= 1 ) {
                ex->type = explosion_t::ex_free;
                break;
            }
            ent->alpha = 1.0f;
            break;
        case explosion_t::ex_poly:
            if ( f >= ex->frames - 1 ) {
                ex->type = explosion_t::ex_free;
                break;
            }

            ent->alpha = ( (float)ex->frames - (float)f ) / (float)ex->frames;
            ent->alpha = max( 0.f, min( 1.f, ent->alpha ) );
            ent->alpha = ent->alpha * ent->alpha * ( 3.f - 2.f * ent->alpha ); // smoothstep

            if ( f < 10 ) {
                ent->skinnum = ( f >> 1 );
                if ( ent->skinnum < 0 )
                    ent->skinnum = 0;
            } else {
                ent->flags |= RF_TRANSLUCENT;
                if ( f < 13 )
                    ent->skinnum = 5;
                else
                    ent->skinnum = 6;
            }
            break;
        case explosion_t::ex_poly2:
            if ( f >= ex->frames - 1 ) {
                ex->type = explosion_t::ex_free;
                break;
            }

            ent->alpha = ( 5.0f - (float)f ) / 5.0f;
            ent->skinnum = 0;
            ent->flags |= RF_TRANSLUCENT;
            break;
        default:
            break;
        }

        if ( ex->type == explosion_t::ex_free )
            continue;

        if ( clgi.GetRefreshType() == REF_TYPE_VKPT )
            CLG_AddExplosionLight( ex, frac / ( ex->frames - 1 ) );
        else {
            if ( ex->light )
                clgi.V_AddLight( ent->origin, ex->light * ent->alpha,
                    ex->lightcolor[ 0 ], ex->lightcolor[ 1 ], ex->lightcolor[ 2 ] );
        }

        if ( ex->type != explosion_t::ex_light ) {
            VectorCopy( ent->origin, ent->oldorigin );

            if ( f < 0 )
                f = 0;
            ent->frame = ex->baseframe + f + 1;
            ent->oldframe = ex->baseframe + f;
            ent->backlerp = 1.0f - ( frac - f );

            V_AddEntity( ent );
        }
    }
}