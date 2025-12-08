/********************************************************************
*
*
*	ClientGame: Footstep Effects for the client.
*
*
********************************************************************/
#include "clgame/clg_local.h"
#include "clgame/clg_precache.h"
#include "clgame/clg_entities.h"
#include "clgame/effects/clg_fx_footsteps.h"

#define _DEBUG_PRINT_LOCAL_CLIENT_FOOTSTEPS 1
#define _DEBUG_PRINT_ENTITY_FOOTSTEPS 1

/**
*   @brief  Plays the footstep sound effect for the specified entity number,
* 		    using the specified material footsteps and count.
* 
*   @note   That for local client entities, the attenuation is set to ATTN_NORM,
* 		    while for other entities, it is set to ATTN_IDLE.
**/
static void FootStepSound( const int32_t entityNumber, const Vector3 *origin, const centity_t *cent, const int32_t attenuation, const char *material_kind, const qhandle_t *material_footsteps, const uint32_t material_num_footsteps ) {
    /**
    *   View Bound Entity:
    **/
    // Play a randomly appointed footstep from the material_footsteps array that now points to the water footsteps.
    clgi.S_StartSound(
        // No specific origin, as the entity number is specified.
        ( origin ? &origin->x : nullptr ),
        // Entity number.
        entityNumber,
        // Channel.
        CHAN_BODY,
        // Random footstep sound from water footsteps.
        material_footsteps[ Q_rand() & material_num_footsteps - 1 ],
        // Volume.
        1.,
        // IDLE Attenuation.
        attenuation,
        // No time offset.
        0.
    );

    #if 0
    /**
    *   Player Client Entity:
    **/
    if ( entityNumber == clgi.client->frame.ps.clientNumber + 1 ) {
        // Play a randomly appointed footstep from the material_footsteps array that now points to the water footsteps.
        clgi.S_StartSound(
            // Play at the origin of the predicted state.
            ( origin ? &origin->x : nullptr ),
            // Entity number.
            entityNumber,
            // Channel.
            CHAN_BODY,
            // Random footstep sound from water footsteps.
            material_footsteps[ Q_rand() & material_num_footsteps - 1 ],
            // Volume.
            1.,
            // Attenuation.
            attenuation,
            // No time offset.
            0.
        );
        // Debug print.
        #if _DEBUG_PRINT_LOCAL_CLIENT_FOOTSTEPS
            clgi.Print( PRINT_DEVELOPER, "%s: [Local Client] FootStep Path(material_kind(%s), num_footsteps(%i))\n", __func__, material_kind, material_num_footsteps );
        #endif // _DEBUG_PRINT_LOCAL_CLIENT_FOOTSTEPS           
    /**
    *   Other Entity:
    **/
    } else {
        // Play a randomly appointed footstep from the material_footsteps array that now points to the water footsteps.
        clgi.S_StartSound(
            // No specific origin, as the entity number is specified.
            ( origin ? &origin->x : nullptr ),
            // Entity number.
            entityNumber,
            // Channel.
            CHAN_BODY,
            // Random footstep sound from water footsteps.
            material_footsteps[ Q_rand() & material_num_footsteps - 1 ],
            // Volume.
            1.,
            // IDLE Attenuation.
            ATTN_IDLE,
            // No time offset.
            0.
        );
        // Debug print.
        #if _DEBUG_PRINT_ENTITY_FOOTSTEPS
            clgi.Print( PRINT_DEVELOPER, "%s: [OTHER Entity] FootStep Path(material_kind(%s), num_footsteps(%i))\n", __func__, material_kind, material_num_footsteps );
        #endif // _DEBUG_PRINT_ENTITY_FOOTSTEPS
    }
    #endif
}
/**
*   @brief  Will play an appropriate footstep sound effect depending on the material that we're currently
*           standing on.
**/
void CLG_FX_FootStepSound( const int32_t entityNumber, const Vector3 &lerpOrigin, const bool isLadder, const bool isLocalClient ) {
    // Opt-out in case footsteps are disabled.
    if ( !clg_footsteps->integer ) {
        return;
    }

    // Get predicted state.
    client_predicted_state_t *predictedState = &game.predictedState;

    // Get local client entity pointer:
    centity_t *localClientEntity = CLG_GetLocalClientEntity(); //&clg_entities[ clgi.client->clientNumber + 1 ]
	// Get the centity_t for this entity number.
	centity_t *entityNumberEntity = &clg_entities[ entityNumber ];

    // Material we're currently standing on: Defaults to null, varies by entity type.
    cm_material_t *ground_material = nullptr;

    // The default type is "floor".
    const char *material_kind = "floor";
	// Default footstep sound data:
    qhandle_t *material_footsteps = precache.sfx.footsteps.floor;
	// Default number of footsteps in the material_footsteps array:
    uint32_t material_num_footsteps = precache.sfx.footsteps.NUM_FLOOR_STEPS;

    /**
    * 
	*   Ladder Path:
    * 
    **/
    if ( isLadder ) {
        // When isLadder is true, we don't need to do any material checks, instead we assume
        // the ladder is made out of metal, and apply that material sound instead.
        const uint32_t material_num_footsteps = precache.sfx.footsteps.NUM_METAL_STEPS;
        const qhandle_t *material_footsteps = precache.sfx.footsteps.metal;

        // Origin if local client entity.
        if ( isLocalClient ) {
            // Play the footstep sound for metal ladders.
            FootStepSound( entityNumber, &lerpOrigin, localClientEntity, ATTN_NORM, "ladder(metal)", material_footsteps, material_num_footsteps );
        } else {
            // Play the footstep sound for metal ladders.
            FootStepSound( entityNumber, &lerpOrigin, entityNumberEntity, ATTN_STATIC, "ladder(metal)", material_footsteps, material_num_footsteps );
        }

        return;
    }

    /**
    * 
	*   Ground and Water Paths for both local client entity and other (packet-)entities:
    *
	*   Check for water at feet level first, and play sound and return if so.
	*   Else, continue to ground material check and footstep sound playing.
    * 
    **/
    /**
    *   Path for local client entity (Play sound with ATTN_NORM):
    **/
    if ( isLocalClient ) {
        // Determine the material kind to fetch the sound data from.
        if ( predictedState->ground.material ) {
            ground_material = predictedState->ground.material;
        }

        #if 0
		// If we have no ground material, and we're moving upwards fast enough, perform a ground trace to find the material.
        if ( !ground_material && predictedState->currentPs.pmove.velocity.z > 100 ) {
           // Perform a ground trace to determine the material we're currently standing on.
            centity_t *traceSkipEntity = CLG_GetLocalClientEntity(); //&clg_entities[ clgi.client->clientNumber + 1 ];

			// Trace from current origin to just below current origin to find ground material.
            Vector3 traceStart = predictedState->currentPs.pmove.origin;
            Vector3 traceEnd = traceStart + Vector3{ 0., 0., -( PM_STEP_MAX_SIZE + 0.25 ) };

			// Perform the trace.
            cm_trace_t groundTrace = clgi.Trace( &traceStart.x, &predictedState->mins.x, &predictedState->maxs.x, &traceEnd.x, traceSkipEntity, CM_CONTENTMASK_SOLID );

			// If we got a material from the ground trace, use it.
            if ( groundTrace.material ) {
                // Assign it.
                ground_material = groundTrace.material;
                material_kind = groundTrace.material->physical.kind;

				#ifdef _DEBUG_PRINT_LOCAL_CLIENT_FOOTSTEPS
                    clgi.Print( PRINT_DEVELOPER, "%s: Jump Path(material_kind(%s))\n", __func__, material_kind );
                #endif
            }
        }
        #endif

		// Must be in a liquid of contents type water, and feet level liquid.
        if ( predictedState->liquid.type & CONTENTS_WATER &&
            predictedState->liquid.level == cm_liquid_level_t::LIQUID_FEET ) {
            // Play the footstep sound for water.
            FootStepSound( entityNumber, &lerpOrigin, localClientEntity, ATTN_NORM, "water", precache.sfx.footsteps.water, precache.sfx.footsteps.NUM_WATER_STEPS );
            //return;
        }
    /**
    *   Path for other (packet-)entities (Play sound with ATTN_IDLE):
    *
    *   Check for Water Feet Level first, and play sound and return if so.
    **/
    } else {
        // Get the centity_t for this entity number.
        centity_t *cent = &clg_entities[ entityNumber ];

		// Perform a ground trace to determine the material we're currently standing on.
        if ( cent ) {
            // Trace from current origin to just below current origin to find ground material.
            Vector3 traceStart = cent->current.origin;
            Vector3 traceEnd = traceStart + Vector3{ 0., 0., -0.25 };
            // Perform the trace.
            cm_trace_t groundTrace = clgi.Trace( &traceStart, &cent->mins, &cent->maxs, &traceEnd, cent, CM_CONTENTMASK_SOLID );

			// If we got a material from the ground trace, use it.
            //if ( groundTrace.material ) {
				// Default to the traced ground material.
                ground_material = groundTrace.material;                
                
				// Check for whether we're in water at feet level:
                if ( groundTrace.contents & CONTENTS_WATER 
					// <Q2RTXP>: TODO: Fix liquid level checking for packet entities.
                    && predictedState->liquid.level == cm_liquid_level_t::LIQUID_FEET ) {
                    // Play the footstep sound for water.
                    FootStepSound( entityNumber, &lerpOrigin, entityNumberEntity, ATTN_STATIC, "water", precache.sfx.footsteps.water, precache.sfx.footsteps.NUM_WATER_STEPS );

                    // Exit.
                    //return;
                }
            //}
		}
    }
    
    /**
	*   We did not early out, so now we proceed with playing the footstep sound 
    *   based on material kind that we found. Its default is "floor".
    **/
    {
		// If we have a ground material, override the default floor with acquire ground material kind.
        if ( ground_material ) {
            material_kind = ground_material->physical.kind;
        }

        /**
		*   Determine precache pointers for the footstep data based on the material kind string.
        **/
        if ( strcmp( "carpet", material_kind ) == 0 ) {
            material_num_footsteps = precache.sfx.footsteps.NUM_CARPET_STEPS;
            material_footsteps = precache.sfx.footsteps.carpet;
        } else if ( strcmp( "dirt", material_kind ) == 0 ) {
            material_num_footsteps = precache.sfx.footsteps.NUM_DIRT_STEPS;
            material_footsteps = precache.sfx.footsteps.dirt;
        } else if ( strcmp( "floor", material_kind ) == 0 ) {
            material_num_footsteps = precache.sfx.footsteps.NUM_FLOOR_STEPS;
            material_footsteps = precache.sfx.footsteps.floor;
        } else if ( strcmp( "grass", material_kind ) == 0 ) {
            material_num_footsteps = precache.sfx.footsteps.NUM_GRASS_STEPS;
            material_footsteps = precache.sfx.footsteps.grass;
        } else if ( strcmp( "gravel", material_kind ) == 0 ) {
            material_num_footsteps = precache.sfx.footsteps.NUM_GRAVEL_STEPS;
            material_footsteps = precache.sfx.footsteps.gravel;
        } else if ( strcmp( "metal", material_kind ) == 0 ) {
            material_num_footsteps = precache.sfx.footsteps.NUM_METAL_STEPS;
            material_footsteps = precache.sfx.footsteps.metal;
        } else if ( strcmp( "snow", material_kind ) == 0 ) {
            material_num_footsteps = precache.sfx.footsteps.NUM_SNOW_STEPS;
            material_footsteps = precache.sfx.footsteps.snow;
        } else if ( strcmp( "tile", material_kind ) == 0 ) {
            material_num_footsteps = precache.sfx.footsteps.NUM_TILE_STEPS;
            material_footsteps = precache.sfx.footsteps.tile;
        } else if ( strcmp( "water", material_kind ) == 0 ) {
            material_num_footsteps = precache.sfx.footsteps.NUM_WATER_STEPS;
            material_footsteps = precache.sfx.footsteps.water;
        } else if ( strcmp( "wood", material_kind ) == 0 ) {
            material_num_footsteps = precache.sfx.footsteps.NUM_WOOD_STEPS;
            material_footsteps = precache.sfx.footsteps.wood;
        }

        /**
        *   Now play the sound:
        **/
        // Play the footstep sound for local client entity.
        if ( isLocalClient ) {
            FootStepSound( entityNumber, &lerpOrigin, localClientEntity, ATTN_NORM, material_kind, material_footsteps, material_num_footsteps );
        // Play the footstep sound for other (packet-)entities.
        } else {
            FootStepSound( entityNumber, &lerpOrigin, entityNumberEntity, ATTN_STATIC, material_kind, material_footsteps, material_num_footsteps );
        }
    }
}

/**
*   @brief  Fills up the path char pointer with the distinct path to the footstep sound file.
*   @param  path  A pointer for storing the path to the footstep sound file. Expected to be MAX_QPATH size.
**/
static void CLG_GetFootstepPath( char *path, const char *kindName, const uint32_t index ) {
    //char    name[ MAX_QPATH ] = {};

    if ( index + 1 < 10 ) {
        Q_snprintf( path, MAX_QPATH, "footsteps/%s/0%i.wav", kindName, index + 1 );
    } else {
        Q_snprintf( path, MAX_QPATH, "footsteps/%s/%i.wav", kindName, index + 1 );
    }
}

/**
*	@brief  Precaches footstep audio for all material "kinds".
**/
void CLG_PrecacheFootsteps( void ) {
    /**
    *   Kind - "default"/"floor":
    **/
    for ( int32_t i = 0; i < precache.sfx.footsteps.NUM_FLOOR_STEPS; i++ ) {
        char path[ MAX_QPATH ] = {};
        CLG_GetFootstepPath( path, "floor", i );

        // Precache footsteps for material kind.
        precache.sfx.footsteps.floor[ i ] = clgi.S_RegisterSound( path );
    }
    /**
    *   Kind - "carpet":
    **/
    for ( int32_t i = 0; i < precache.sfx.footsteps.NUM_CARPET_STEPS; i++ ) {
        char path[ MAX_QPATH ] = {};
        CLG_GetFootstepPath( path, "carpet", i );

        // Precache footsteps for material kind.
        precache.sfx.footsteps.carpet[ i ] = clgi.S_RegisterSound( path );
    }
    /**
    *   Kind - "dirt":
    **/
    for ( int32_t i = 0; i < precache.sfx.footsteps.NUM_DIRT_STEPS; i++ ) {
        char path[ MAX_QPATH ] = {};
        CLG_GetFootstepPath( path, "dirt", i );

        // Precache footsteps for material kind.
        precache.sfx.footsteps.dirt[ i ] = clgi.S_RegisterSound( path );
    }
    /**
    *   Kind - "grass":
    **/
    for ( int32_t i = 0; i < precache.sfx.footsteps.NUM_GRASS_STEPS; i++ ) {
        char path[ MAX_QPATH ] = {};
        CLG_GetFootstepPath( path, "grass", i );

        // Precache footsteps for material kind.
        precache.sfx.footsteps.grass[ i ] = clgi.S_RegisterSound( path );
    }
    /**
    *   Kind - "gravel":
    **/
    for ( int32_t i = 0; i < precache.sfx.footsteps.NUM_GRAVEL_STEPS; i++ ) {
        char path[ MAX_QPATH ] = {};
        CLG_GetFootstepPath( path, "gravel", i );

        // Precache footsteps for material kind.
        precache.sfx.footsteps.gravel[ i ] = clgi.S_RegisterSound( path );
    }
    /**
    *   Kind - "metal":
    **/
    for ( int32_t i = 0; i < precache.sfx.footsteps.NUM_METAL_STEPS; i++ ) {
        char path[ MAX_QPATH ] = {};
        CLG_GetFootstepPath( path, "metal", i );

        // Precache footsteps for material kind.
        precache.sfx.footsteps.metal[ i ] = clgi.S_RegisterSound( path );
    }
    /**
    *   Kind - "snow":
    **/
    for ( int32_t i = 0; i < precache.sfx.footsteps.NUM_SNOW_STEPS; i++ ) {
        char path[ MAX_QPATH ] = {};
        CLG_GetFootstepPath( path, "snow", i );

        // Precache footsteps for material kind.
        precache.sfx.footsteps.snow[ i ] = clgi.S_RegisterSound( path );
    }
    /**
    *   Kind - "tile":
    **/
    for ( int32_t i = 0; i < precache.sfx.footsteps.NUM_TILE_STEPS; i++ ) {
        char path[ MAX_QPATH ] = {};
        CLG_GetFootstepPath( path, "tile", i );

        // Precache footsteps for material kind.
        precache.sfx.footsteps.tile[ i ] = clgi.S_RegisterSound( path );
    }
    /**
    *   Kind - "water":
    **/
    for ( int32_t i = 0; i < precache.sfx.footsteps.NUM_WATER_STEPS; i++ ) {
        char path[ MAX_QPATH ] = {};
        CLG_GetFootstepPath( path, "water", i );

        // Precache footsteps for material kind.
        precache.sfx.footsteps.water[ i ] = clgi.S_RegisterSound( path );
    }
    /**
    *   Kind - "wood":
    **/
    for ( int32_t i = 0; i < precache.sfx.footsteps.NUM_WOOD_STEPS; i++ ) {
        char path[ MAX_QPATH ] = {};
        CLG_GetFootstepPath( path, "wood", i );

        // Precache footsteps for material kind.
        precache.sfx.footsteps.wood[ i ] = clgi.S_RegisterSound( path );
    }
}



/**
*
*
*
*
*	FootStep Effect Wrappers:
*
*
*
*
**/
/**
*   @brief  The (LOCAL PLAYER) footstep sound event handler.
**/
void CLG_FX_LocalFootStep( const int32_t entityNumber, const Vector3 &lerpOrigin ) {
    // Pass on to the footstep sound effect handler.
    CLG_FX_FootStepSound(
        // The entity number.
        entityNumber,
        // The origin to play the sound at.
        lerpOrigin,
        // No ladder.
        false,
        // This is invoked by the local client itself.
        true
    );
}
/**
*   @brief  The generic (PLAYER) footstep sound event handler.
**/
void CLG_FX_PlayerFootStep( const int32_t entityNumber, const Vector3 &lerpOrigin ) {
    // Pass on to the footstep sound effect handler.
    CLG_FX_FootStepSound(
        // The entity number.
        entityNumber,
        // The origin to play the sound at.
        lerpOrigin,
        // No ladder.
        false,
        // This event is never invoked by the local client itself.
        false
    );
}
/**
*   @brief  The generic (OTHER entity) footstep sound event handler.
**/
void CLG_FX_OtherFootStep( const int32_t entityNumber, const Vector3 &lerpOrigin ) {
    // Pass on to the footstep sound effect handler.
    CLG_FX_FootStepSound(
        // The entity number.
        entityNumber,
        // The origin to play the sound at.
        lerpOrigin,
        // No ladder.
        false,
        // This event is never invoked by the local client itself.
        false
    );
}
/**
*   @brief  Passes on to CLG_FX_FootStepSound with isLadder beign true. Used by EV_FOOTSTEP_LADDER.
**/
void CLG_FX_LadderFootStep( const int32_t entityNumber, const Vector3 &lerpOrigin ) {
    // Pass on to the footstep sound effect handler.
    CLG_FX_FootStepSound(
        entityNumber,
        lerpOrigin,
        true,
        CLG_IsLocalClientEntity( entityNumber )
    );
}