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
    QMTime clientTime = QMTime::FromMilliseconds( clgi.client->time );
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
*
*
*
*   "Explosion" Polygonal Light Curvatures:
*
*
*
**/
/**
*   For calculating the light polygon curve array length with.
**/
template<typename T>
static inline constexpr uint64_t CLG_LightPolygonCurveLength( const T &curvePolygonList ) {
    return ( ( sizeof( curvePolygonList ) ) / ( sizeof( *( curvePolygonList ) ) ) );
}

/**
*   @brief  Polygon curvature list for use by "TE_PLAIN_EXPLOSION" type.
**/
static const constexpr clg_light_curve_polygon_t ex_plain_explosion_polygons[] = {
    { { 0.4f,       0.2f,       0.02f     }, 12.5, 20.00, 0.35 }, // 12.5f, 20.00f },
    { { 0.351563f,  0.175781f,  0.017578f }, 15.0, 23.27, 0.45 },//15.0f, 23.27f },
    { { 0.30625f,   0.153125f,  0.015312f }, 20.0, 24.95, 0.50 },//20.0f, 24.95f },
    { { 0.264062f,  0.132031f,  0.013203f }, 25.5, 25.01, 0.75 },//22.5f, 25.01f },
    { { 0.225f,     0.1125f,    0.01125f  }, 30.0, 27.53, 1.  },
    { { 0.189063f,  0.094531f,  0.009453f }, 27.5, 28.55, 0.85 },
    { { 0.15625f,   0.078125f,  0.007813f }, 25.0, 30.80, 0.75 },
    { { 0.126563f,  0.063281f,  0.006328f }, 22.5, 40.43, 0.65 },
    { { 0.1f,       0.05f,      0.005f    }, 20.0, 49.02, 0.50 },
    { { 0.076563f,  0.038281f,  0.003828f }, 18.5, 58.15, 0.475 },
    { { 0.05625f,   0.028125f,  0.002812f }, 10.0, 61.03, 0.25 },
    { { 0.039063f,  0.019531f,  0.001953f }, 5.5, 63.59, 0.125 },//17.5f, 63.59f },
    { { 0.025f,     0.0125f,    0.00125f  }, 2.5, 66.47, 0.0625 },//15.0f, 66.47f },
    { { 0.f,        0.f,        0.f       }, 2.0, 72.00, 0.03125 }//10.0f, 72.00f }
};
//! For "TE_PLAIN_EXPLOSION".
static const constexpr clg_light_curve_t ex_plain_explosion_curve = {
    //! Ease method to apply.
    .easeMethod = QM_QuarticEaseInOut,
    //! Polygon list.
    .polygons = ex_plain_explosion_polygons,
    //! Length of polygon list.
    .length = CLG_LightPolygonCurveLength( ex_plain_explosion_polygons )
    
};


/**
*   @brief  Processes a frame worth of moment in time for the light, in order to lerps over the polygonal curvature.
*   @note   Also calculates the actual scale for use with the sprite.
**/
static void CLG_ProcessPolygonCurveLight( clg_explosion_t *ex, const double phase ) {
    // Got valid explosion?
    if ( !ex ) {
        return;
    }
    // Got curvature polygons?
    if ( !ex->lightCurvature || !ex->lightCurvature->polygons || !ex->lightCurvature->length ) {
        return;
    }

    //! Curvature polygon list.
    const clg_light_curve_t *curve = ex->lightCurvature;
    const int64_t curve_size = ex->lightCurvature->length;

    //! Proceed and calculate.
    const double timeAlpha = ( (double)( curve_size - 1 ) ) * phase;
    int64_t baseSample = (int64_t)std::floor( timeAlpha );
    baseSample = std::max<int64_t>( 0, std::min( (int64_t)curve_size - 2, (int64_t)baseSample ) );

    const double w1 = timeAlpha - (double)( baseSample );
    const double w0 = 1. - w1;

    const clg_light_curve_polygon_t *s0 = curve->polygons + baseSample;
    const clg_light_curve_polygon_t *s1 = curve->polygons + baseSample + 1;

    const double offset = w0 * s0->offset + w1 * s1->offset;
    const double radius = w0 * s0->radius + w1 * s1->radius;
    const double scale =  w0 * s0->scale + w1 * s1->scale;
    
    ex->ent.scale = scale + ex->lightCurvature->easeMethod( phase );// ( ex->scale + scale );// ex->lightCurvature->easeMethod( w1 ) );

    vec3_t origin;
    vec3_t up;
    AngleVectors( ex->ent.angles, NULL, NULL, up );
    VectorMA( ex->ent.origin, offset, up, origin );

    vec3_t color;
    VectorClear( color );
    VectorMA( color, w0, s0->color, color );
    VectorMA( color, w1, s1->color, color );

    clgi.V_AddSphereLight( origin, ex->light, color[ 0 ], color[ 1 ], color[ 2 ], radius );
}



/**
*
*
*
*   Explosion Scene Management:
*
*
*
**/
/**
*   @brief  Add all explosions for this frame to the scene. (All non free slots).
**/
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

        double inv_frametime = ex->frametime.Milliseconds() ? 1.f / (double)ex->frametime.Milliseconds() : BASE_1_FRAMETIME;
        frac = ( clgi.client->time - ex->start.Milliseconds() ) * inv_frametime;
        f = std::floor( frac );

        ent = &ex->ent;

        // Default to a scale of 1 if not a curvature light.
        if ( ex->type != clg_explosion_t::ex_polygon_curvature ) {
            ex->scale = 1.;
        }

        switch ( ex->type ) {
        // Default explosion effect sprite alpha handling.
        case clg_explosion_t::ex_misc:
        case clg_explosion_t::ex_light:
            // Reset to free if we're passed the last frame.
            if ( f >= ex->frames - 1 ) {
                ex->type = clg_explosion_t::ex_free;
                break;
            }
            ent->alpha = 1.0f - frac / ( ex->frames - 1 );
        break;
        // Flash explosion effect sprite alpha handling.
        case clg_explosion_t::ex_flash:
            // Reset to free if we're passed the last frame.
            if ( f >= 1 ) {
                ex->type = clg_explosion_t::ex_free;
                break;
            }
            ent->alpha = 1.0f;
        break;
        // Polygon curvature light sprite alpha handling.
        case clg_explosion_t::ex_polygon_curvature:
            // Reset to free if we're passed the last frame.
            if ( f >= ex->frames - 1 ) {
                ex->type = clg_explosion_t::ex_free;
                break;
            }
            ent->alpha = ( (float)ex->frames - (float)f ) / (float)ex->frames;
            ent->alpha = std::max( 0.f, std::min( 1.f, ent->alpha ) );
            //ent->scale = 1 + ent->alpha * ent->alpha * ( 3.f - 2.f * QM_QuarticEaseInOut( ent->alpha ) ); //0.5 + QM_QuarticEaseInOut( ent->alpha );
            //ent->alpha = ent->alpha * ent->alpha * ( 3.f - 2.f * ent->alpha ); // smoothstep
            
            // Temporarily store ent->alpha value here, since we need that value later to multiply with the base explosion curve poly scale.
            ex->scale = ent->alpha;
            // Now calculate the actual resulting alpha based on the curvature easing.
            ent->alpha = ex->lightCurvature->easeMethod( ent->alpha );
            if ( ent->alpha < 1.0 ) {
                ent->flags |= RF_TRANSLUCENT;
            }
        break;
        #if 0
        case clg_explosion_t::ex_polygon_curvature2:
            // Reset to free if we're passed the last frame.
            if ( f >= ex->frames - 1 ) {
                ex->type = clg_explosion_t::ex_free;
                break;
            }

            ent->alpha = ( 5.0f - (float)f ) / 5.0f;
            ent->skinnum = 0;
            ent->flags |= RF_TRANSLUCENT;
            break;
        #endif // #if 0 
        default:
            break;
        }

        // Don't add if its' type is ex_free.
        if ( ex->type == clg_explosion_t::ex_free )
            continue;

        if ( clgi.GetRefreshType() == REF_TYPE_VKPT ) {
            if ( ex->type == clg_explosion_t::ex_polygon_curvature ) {
                CLG_ProcessPolygonCurveLight( ex, frac / ( ex->frames - 1 ) );
                // Temporarily store ent->alpha value here, since we need that value later to multiply with the base explosion curve poly scale.
            } else {
                // Process it.
                CLG_ProcessPolygonCurveLight( ex, frac / ( ex->frames - 1 ) );

            }
        } else {
            if ( ex->light ) {
                clgi.V_AddLight( ent->origin, ex->light * ent->alpha,
                    ex->lightcolor[ 0 ], ex->lightcolor[ 1 ], ex->lightcolor[ 2 ] );
            }
        }

        //! A light only explosion, does not render the sprite/model.
        if ( ex->type != clg_explosion_t::ex_light ) {
            // Copy origin directly into old origin.
            VectorCopy( ent->origin, ent->oldorigin );
            // Ensure f >= 0.
            if ( f < 0 ) {
                f = 0;
            }
            // Determine frame.
            ent->frame = ex->baseframe + f + 1;
            // Old frame.
            ent->oldframe = ex->baseframe + f;
            // Backlerp.
            ent->backlerp = 1.0 - ( frac - f );
            // Add the explosion entity for rendering.
            clgi.V_AddEntity( ent );
        }
    }
}






/**
*
*
*
*   Public, "Add Explosion Effects" Functions:
*
*
*
**/
/**
*   @brief  Creates a plain explosion effect, with optional smoke explosion sprite.
**/
clg_explosion_t *CLG_PlainExplosion( const bool withSmoke ) {

    qhandle_t spriteHandle = ( withSmoke ? precache.models.sprite_explo01 : precache.models.sprite_explo00 );
    clg_explosion_t *ex = CLG_AllocExplosion();
    VectorCopy( level.parsedMessage.events.tempEntity.pos1, ex->ent.origin );
    ex->type = clg_explosion_t::ex_polygon_curvature; // WID: C++20: Was without clg_explosion_t::
    ex->lightCurvature = &ex_plain_explosion_curve;
    ex->ent.flags = RF_FULLBRIGHT;
    ex->scale = 1.5;
    ex->start = QMTime::FromMilliseconds( clgi.client->servertime - CL_FRAMETIME );
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
    ex->frametime = QMTime::FromMilliseconds( cl_explosion_frametime->integer );

    //ex->easeState = QMEaseState::new_ease_state(
    //    QMTime::FromMilliseconds( clgi.GetRealTime() ),
    //    QMTime::FromMilliseconds( ex->frames * ex->frametime )
    //);
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