/********************************************************************
*
*
*	ClientGame: Footstep Effects for the client.
*
*
********************************************************************/
#include "clgame/clg_local.h"
#include "clgame/clg_effects.h"
#include "clgame/clg_entities.h"

/**
*   @brief  Will play an appropriate footstep sound effect depending on the material that we're currently
*           standing on.
**/
void CLG_FootstepEvent( const int32_t entityNumber, const bool isLadder ) {
    // Opt-out in case footsteps are disabled.
    if ( !cl_footsteps->integer ) {
        return;
    }

    // When isLadder is true, we don't need to do any material checks, instead we assume
    // the ladder is mate out of metal, and apply that material sound instead.
    if ( isLadder ) {
        uint32_t material_num_footsteps = precache.sfx.footsteps.NUM_METAL_STEPS;
        qhandle_t *material_footsteps = precache.sfx.footsteps.metal;

        // Debug print.
        //clgi.Print( PRINT_DEVELOPER, "CLIMBING LADDER(%s), num_footsteps(%i)\n", "metal", material_num_footsteps);

        // Play a randomly appointed footstep from the material_footsteps array.
        clgi.S_StartSound( NULL, entityNumber, CHAN_BODY, material_footsteps[ Q_rand() & material_num_footsteps - 1 ], 1, ATTN_NORM, 0 );

        return;
    }

    // Get predicted state.
    client_predicted_state_t *predictedState = &clgi.client->predictedState;
    //player_state_t *predictedPlayerState = &clgi.client->predictedState;
    // The default type is "floor".
    uint32_t material_num_footsteps = precache.sfx.footsteps.NUM_FLOOR_STEPS;
    qhandle_t *material_footsteps = precache.sfx.footsteps.floor;
    // Determine the material kind to fetch the sound data from.
    cm_material_t *ground_material = predictedState->ground.material;
    const char *material_kind = "floor";

    // Special water feet level footsteps:
    if ( predictedState->liquid.type & CONTENTS_WATER &&
        predictedState->liquid.level == liquid_level_t::LIQUID_FEET ) {
        material_kind = "water";
        material_num_footsteps = precache.sfx.footsteps.NUM_WATER_STEPS;
        material_footsteps = precache.sfx.footsteps.water;

        clgi.S_StartSound( NULL, entityNumber, CHAN_BODY, material_footsteps[ Q_rand() & material_num_footsteps - 1 ], 1, ATTN_NORM, 0 );
        clgi.Print( PRINT_DEVELOPER, "%s: Water Path(material_kind(%s))\n", __func__, material_kind );

        return;
    // Default behavior: Adjust footstep material kind based on the predicted ground material:
    } else {//if ( ground_material ) {
        if ( ground_material ) {
            material_kind = ground_material->physical.kind;
        }

        // See if we're trick jumping, if so, perform a step height + ground offset trace for
        // finding a proper material sound to play while 'landing'.
        player_state_t *currentPs = &predictedState->currentPs;

        if ( currentPs->pmove.pm_flags & PMF_TIME_TRICK_JUMP || currentPs->pmove.velocity.z > 100 ) {
            centity_t *traceSkipEntity = &clg_entities[ clgi.client->clientNumber + 1 ];
            Vector3 traceStart = currentPs->pmove.origin;
            Vector3 traceEnd = traceStart + Vector3{ 0., 0., -( PM_MAX_STEP_SIZE + 0.25 ) };
            Vector3 traceMins = predictedState->mins;
            Vector3 traceMaxs = predictedState->maxs;
            cm_trace_t groundTrace = clgi.Trace( &traceStart.x, &traceMins.x, &traceMaxs.x, &traceEnd.x, traceSkipEntity, MASK_SOLID );

            if ( groundTrace.material ) {
                ground_material = groundTrace.material;
                material_kind = groundTrace.material->physical.kind;
                //clgi.Print( PRINT_DEVELOPER, "%s: Trick Jump Path(material_kind(%s))\n", __func__, material_kind );
            }
        }

        // Determine if we're on a different type of material, and if so, adjust the above variables accordingly.
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

        // Play a randomly appointed footstep from the material_footsteps array.
        //if ( ground_material ) {
            clgi.S_StartSound( NULL, entityNumber, CHAN_BODY, material_footsteps[ Q_rand() & material_num_footsteps - 1 ], 1, ATTN_NORM, 0 );
            //clgi.Print( PRINT_DEVELOPER, "%s: Default Path(material_kind(%s), num_footsteps(%i))\n", __func__, material_kind, material_num_footsteps );
        //}
    }

    


}

/**
*   @brief  Passes on to CLG_FootstepEvent with isLadder beign true. Used by EV_FOOTSTEP_LADDER.
**/
void CLG_FootstepLadderEvent( const int32_t entityNumber ) {
    CLG_FootstepEvent( entityNumber, true );
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