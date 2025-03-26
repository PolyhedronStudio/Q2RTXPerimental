/********************************************************************
*
*
*	ClientGame: Explosions.
*
*
********************************************************************/
#include "clgame/clg_local.h"
#include "clgame/clg_temp_entities.h"

//! An array storing slots for explosions to be stored in. 
//! The type of explosion is determined by the 'type' field. Which is clg_explosion_t::ex_free if the slot is free.
clg_explosion_t  clg_explosions[ MAX_EXPLOSIONS ] = {};

/**
*   @brief  Clears all explosions instances, effectively zeroing out memory (thus type becomes ex_free).
**/
void CLG_ClearExplosions( void ) {
	// Zero out all explosion instances.
    for ( int32_t i = 0; i < MAX_EXPLOSIONS; i++ ) {
        clg_explosions[ i ] = {};
    }
}

/**
*   @brief  Locates a free explosion slot, or the oldest one to be reused instead.
**/
clg_explosion_t *CLG_AllocExplosion( void ) {
	// Find a free explosion slot.
    clg_explosion_t *explosionEntry = clg_explosions;
    for ( int32_t i = 0; i < MAX_EXPLOSIONS; i++, explosionEntry++ ) {
        if ( explosionEntry->type == clg_explosion_t::ex_free ) { // WID: C++20: Was without clg_explosion_t::
			// Make sure to zero out the memory just in case.
            *explosionEntry = {};
            // Return slot ptr.
            return explosionEntry;
        }
    }

	// Find the oldest explosion we can use to overwrite.
    uint64_t clientTime = clgi.client->time;
	// The oldest entry found.
    clg_explosion_t *oldestEntry = clg_explosions;
    // Reset explosion entry for iteration purposes.
    explosionEntry = clg_explosions;
    // Iterate.
    for ( int32_t i = 0; i < MAX_EXPLOSIONS; i++, explosionEntry++ ) {
		// If the current explosion slot entry is older than the current client time.
        if ( explosionEntry->start < clientTime ) {
			// Update the client time.
            clientTime = explosionEntry->start;
			// Update it to now be our the oldest entry.
            oldestEntry = explosionEntry;
        }
    }

	// Zero out the memory just in case.
    *oldestEntry = {};
	// Return the oldest entry.
    return oldestEntry;
}

/**
*   @brief  Creates a plain explosion effect.
**/
clg_explosion_t *CLG_PlainExplosion( const bool withSmoke ) {

    qhandle_t spriteHandle = ( withSmoke ? precache.models.sprite_explo01 : precache.models.sprite_explo00 );
    clg_explosion_t *ex = CLG_AllocExplosion();
    VectorCopy( level.parsedMessage.events.tempEntity.pos1, ex->ent.origin );
    ex->type = clg_explosion_t::ex_poly; // WID: C++20: Was without clg_explosion_t::
    ex->ent.flags = RF_FULLBRIGHT;
    ex->start = clgi.client->servertime - CL_FRAMETIME;
    ex->light = 350;
    VectorSet( ex->lightcolor, 1.0f, 0.5f, 0.5f );
    ex->ent.angles[ 1 ] = irandom( 360 );

    const qboolean isValidSpriteModel = clgi.IsValidSpriteModelHandle( spriteHandle );
    //model_t *sprite_model = MOD_ForHandle( cl_mod_explosions[ model_idx ] );

    //if ( cl_explosion_sprites->integer && !big && isValidSpriteModel ) {
        ex->ent.model = spriteHandle;//precache.models.explosions[ model_idx ];
        //ex->frames = sprite_model->numframes;
        
        ex->frames = clgi.GetSpriteModelFrameCount( ex->ent.model );
        ex->baseframe = ex->frames * ( Q_rand() & 1 );
        ex->frametime = cl_explosion_frametime->integer;
    //} else {
    //    ex->ent.model = big ? precache.models.explo4_big : precache.models.explo4;
    //    ex->baseframe = 15 * ( Q_rand() & 1 );
    //    ex->frames = 15;
    //}

    return ex;
}

/*
=================
CL_SmokeAndFlash
=================
*/
// <Q2RTXP>: WID: We'll do something similar but need models.
#if 0
void CLG_SmokeAndFlash( const vec3_t origin ) {
    clg_explosion_t *ex;

    ex = CLG_AllocExplosion();
    VectorCopy( origin, ex->ent.origin );
    ex->type = clg_explosion_t::ex_misc; // WID: C++20: Was without clg_explosion_t::
    ex->frames = 4;
    ex->ent.flags = RF_TRANSLUCENT | RF_NOSHADOW;
    ex->start = clgi.client->servertime - clgi.frame_time_ms;
    ex->ent.model = precache.models.smoke;

    ex = CLG_AllocExplosion();
    VectorCopy( origin, ex->ent.origin );
    ex->type = clg_explosion_t::ex_flash;
    ex->ent.flags = RF_FULLBRIGHT;
    ex->frames = 2;
    ex->start = clgi.client->servertime - clgi.frame_time_ms;
    ex->ent.model = precache.models.flash;
}
#endif


/**
*
*
*
*   Light Curve Explosions:
*
*
*
**/
#define LENGTH(a) ((sizeof (a)) / (sizeof(*(a))))

typedef struct light_curve_s {
    vec3_t color;
    double radius;
    double offset;
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

static void CLG_AddExplosionLight( clg_explosion_t *ex, float phase ) {
    int64_t curve_size;
    light_curve_t *curve;

    switch ( ex->type ) {
    case clg_explosion_t::ex_poly: // WID: C++20: Used to be without clg_explosion_t::
        curve = ex_poly_light;
        curve_size = LENGTH( ex_poly_light );
        break;
    case clg_explosion_t::ex_blaster:
        curve = ex_blaster_light;
        curve_size = LENGTH( ex_blaster_light );
        break;
    default:
        return;
    }

    double timeAlpha = ( (double)( curve_size - 1 ) ) * phase;
    int64_t baseSample = (int64_t)std::floor( timeAlpha );
    baseSample = std::max<int64_t>( 0, std::min( curve_size - 2, baseSample ) );

    double w1 = timeAlpha - (double)( baseSample );
    double w0 = 1.f - w1;

    light_curve_t *s0 = curve + baseSample;
    light_curve_t *s1 = curve + baseSample + 1;

    double offset = w0 * s0->offset + w1 * s1->offset;
    double radius = w0 * s0->radius + w1 * s1->radius;
    switch ( ex->type ) {
    case clg_explosion_t::ex_poly: // WID: C++20: Used to be without clg_explosion_t::
        radius *= 0.5;
        break;
    }
    vec3_t origin;
    vec3_t up;
    AngleVectors( ex->ent.angles, NULL, NULL, up );
    VectorMA( ex->ent.origin, offset, up, origin );

    vec3_t color;
    VectorClear( color );
    VectorMA( color, w0, s0->color, color );
    VectorMA( color, w1, s1->color, color );

    clgi.V_AddSphereLight( origin, 500.f, color[ 0 ], color[ 1 ], color[ 2 ], radius );
}

void CLG_AddExplosions( void ) {
    entity_t *ent = nullptr;
    int32_t i = 0;
    clg_explosion_t *ex = nullptr;
    double  frac = 0.;
    int64_t f = 0;

    for ( i = 0, ex = clg_explosions; i < MAX_EXPLOSIONS; i++, ex++ ) {
        if ( ex->type == clg_explosion_t::ex_free ) {
            continue;
        }

        double inv_frametime = ex->frametime ? 1.f / (double)ex->frametime : BASE_1_FRAMETIME;
        frac = ( clgi.client->time - ex->start ) * inv_frametime;
        f = std::floor( frac );

        ent = &ex->ent;

        switch ( ex->type ) {
        case clg_explosion_t::ex_mflash: // WID: C++20: This was without clg_explosion_t::
            // Reset to free if we're passed the last frame.
            if ( f >= ex->frames - 1 ) {
                ex->type = clg_explosion_t::ex_free;
            }
            break;
        case clg_explosion_t::ex_misc:
        case clg_explosion_t::ex_blaster:
        case clg_explosion_t::ex_flare:
        case clg_explosion_t::ex_light:
            // Reset to free if we're passed the last frame.
            if ( f >= ex->frames - 1 ) {
                ex->type = clg_explosion_t::ex_free;
                break;
            }
            ent->alpha = 1.0f - frac / ( ex->frames - 1 );
            break;
        case clg_explosion_t::ex_flash:
            // Reset to free if we're passed the last frame.
            if ( f >= 1 ) {
                ex->type = clg_explosion_t::ex_free;
                break;
            }
            ent->alpha = 1.0f;
            break;
        case clg_explosion_t::ex_poly:
            // Reset to free if we're passed the last frame.
            if ( f >= ex->frames - 1 ) {
                ex->type = clg_explosion_t::ex_free;
                break;
            }

            ent->alpha = ( (float)ex->frames - (float)f ) / (float)ex->frames;
            ent->alpha = std::max( 0.f, std::min( 1.f, ent->alpha ) );
            ent->alpha = ent->alpha * ent->alpha * ( 3.f - 2.f * ent->alpha ); // smoothstep
            if ( ent->alpha < 1.0 ) {
                ent->flags |= RF_TRANSLUCENT;
            }

            // Would control alpha based on frames. Leftover from Q@.
            #if 0
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
            #endif
            break;
        case clg_explosion_t::ex_poly2:
            // Reset to free if we're passed the last frame.
            if ( f >= ex->frames - 1 ) {
                ex->type = clg_explosion_t::ex_free;
                break;
            }

            ent->alpha = ( 5.0f - (float)f ) / 5.0f;
            ent->skinnum = 0;
            ent->flags |= RF_TRANSLUCENT;
            break;
        default:
            break;
        }

        if ( ex->type == clg_explosion_t::ex_free )
            continue;

        if ( clgi.GetRefreshType() == REF_TYPE_VKPT )
            CLG_AddExplosionLight( ex, frac / ( ex->frames - 1 ) );
        else {
            if ( ex->light )
                clgi.V_AddLight( ent->origin, ex->light * ent->alpha,
                    ex->lightcolor[ 0 ], ex->lightcolor[ 1 ], ex->lightcolor[ 2 ] );
        }

        if ( ex->type != clg_explosion_t::ex_light ) {
            VectorCopy( ent->origin, ent->oldorigin );

            if ( f < 0 )
                f = 0;
            ent->frame = ex->baseframe + f + 1;
            ent->oldframe = ex->baseframe + f;
            ent->backlerp = 1.0f - ( frac - f );

            clgi.V_AddEntity( ent );
        }
    }
}