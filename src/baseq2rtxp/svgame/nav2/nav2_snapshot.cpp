/********************************************************************
*
*
*	ServerGame: Nav2 Snapshot Model - Implementation
*
*
********************************************************************/
#include "svgame/nav2/nav2_snapshot.h"


/**
*
*
*	Nav2 Snapshot Internal Helpers:
*
*
**/
/**
*	@brief	Advance a publication counter while avoiding wrap-to-zero semantics.
*	@param	value	Version counter to increment.
*	@return	Updated non-zero version value.
*	@note	Zero remains reserved for "never published" so the first bump always yields 1.
**/
static uint32_t Nav2_Snapshot_BumpVersion( uint32_t &value ) {
    /**
    *    Increment the version and preserve zero as the reserved "uninitialized" state.
    **/
    value++;
    if ( value == 0 ) {
        // Skip zero so versioned handles always distinguish published state from never-initialized state.
        value = 1;
    }
    return value;
}


/**
*
*
*	Nav2 Snapshot Public API:
*
*
**/
/**
*	@brief	Reset the snapshot runtime to an initial zero-version state.
*	@param	runtime	[out] Snapshot runtime to reset.
**/
void SVG_Nav2_Snapshot_ResetRuntime( nav2_snapshot_runtime_t *runtime ) {
    /**
    *    Sanity-check the runtime pointer before resetting the publication counters.
    **/
    if ( !runtime ) {
        return;
    }

    // Reset all published versions back to the initial zero state.
    *runtime = nav2_snapshot_runtime_t{};
}

/**
*	@brief	Return the latest published snapshot handle.
*	@param	runtime	Snapshot runtime to inspect.
*	@return	Copy of the current snapshot handle, or a zeroed handle when runtime is unavailable.
**/
nav2_snapshot_handle_t SVG_Nav2_Snapshot_GetCurrentHandle( const nav2_snapshot_runtime_t *runtime ) {
    /**
    *    Return a zeroed snapshot handle when no runtime publication state is available.
    **/
    if ( !runtime ) {
        return nav2_snapshot_handle_t{};
    }

    // Return a copy of the latest published handle.
    return runtime->current;
}

/**
*	@brief	Publish a new static-nav version.
*	@param	runtime	Snapshot runtime to mutate.
*	@return	Updated static-nav version.
**/
uint32_t SVG_Nav2_Snapshot_BumpStaticNavVersion( nav2_snapshot_runtime_t *runtime ) {
    if ( !runtime ) {
        return 0;
    }
    return Nav2_Snapshot_BumpVersion( runtime->current.static_nav_version );
}

/**
*	@brief	Publish a new occupancy-overlay version.
*	@param	runtime	Snapshot runtime to mutate.
*	@return	Updated occupancy version.
**/
uint32_t SVG_Nav2_Snapshot_BumpOccupancyVersion( nav2_snapshot_runtime_t *runtime ) {
    if ( !runtime ) {
        return 0;
    }
    return Nav2_Snapshot_BumpVersion( runtime->current.occupancy_version );
}

/**
*	@brief	Publish a new mover-transform version.
*	@param	runtime	Snapshot runtime to mutate.
*	@return	Updated mover version.
**/
uint32_t SVG_Nav2_Snapshot_BumpMoverVersion( nav2_snapshot_runtime_t *runtime ) {
    if ( !runtime ) {
        return 0;
    }
    return Nav2_Snapshot_BumpVersion( runtime->current.mover_version );
}

/**
*	@brief	Publish a new dynamic-connector version.
*	@param	runtime	Snapshot runtime to mutate.
*	@return	Updated connector version.
**/
uint32_t SVG_Nav2_Snapshot_BumpConnectorVersion( nav2_snapshot_runtime_t *runtime ) {
    if ( !runtime ) {
        return 0;
    }
    return Nav2_Snapshot_BumpVersion( runtime->current.connector_version );
}

/**
*	@brief	Publish a new model/headnode metadata version.
*	@param	runtime	Snapshot runtime to mutate.
*	@return	Updated model version.
**/
uint32_t SVG_Nav2_Snapshot_BumpModelVersion( nav2_snapshot_runtime_t *runtime ) {
    if ( !runtime ) {
        return 0;
    }
    return Nav2_Snapshot_BumpVersion( runtime->current.model_version );
}

/**
*	@brief	Compare a consumed query snapshot against the current runtime versions.
*	@param	runtime	Snapshot runtime providing the latest published versions.
*	@param	consumed	Snapshot handle used by the query that is finishing or resuming.
*	@return	Structured staleness decision that later scheduler code can map onto restart policy.
*	@note	The current policy is intentionally conservative: static-nav or model changes force resubmit,
*			while dynamic overlay changes request revalidation and stage-aware restart.
**/
nav2_snapshot_staleness_t SVG_Nav2_Snapshot_Compare( const nav2_snapshot_runtime_t *runtime, const nav2_snapshot_handle_t &consumed ) {
    /**
    *    Start from the optimistic assumption that the snapshot is still current and only escalate
    *    the decision when a concrete version mismatch is observed.
    **/
    nav2_snapshot_staleness_t result = {};
    result.is_current = true;

    /**
    *    Without runtime publication state there is nothing meaningful to compare against, so leave
    *    the snapshot marked current.
    **/
    if ( !runtime ) {
        return result;
    }

    /**
    *    Compare each published domain explicitly so diagnostics can tell which source invalidated
    *    the query result.
    **/
    result.static_nav_changed = runtime->current.static_nav_version != consumed.static_nav_version;
    result.occupancy_changed = runtime->current.occupancy_version != consumed.occupancy_version;
    result.mover_changed = runtime->current.mover_version != consumed.mover_version;
    result.connector_changed = runtime->current.connector_version != consumed.connector_version;
    result.model_changed = runtime->current.model_version != consumed.model_version;

    /**
    *    If nothing changed, the query snapshot remains current and can be accepted directly.
    **/
    if ( !result.static_nav_changed && !result.occupancy_changed && !result.mover_changed
        && !result.connector_changed && !result.model_changed ) {
        return result;
    }

    /**
    *    Any mismatch means the snapshot is no longer fully current.
    **/
    result.is_current = false;

    /**
    *    Static-nav or model/headnode changes invalidate the structural assumptions of the query, so
    *    require a full resubmit instead of attempting targeted repair.
    **/
    if ( result.static_nav_changed || result.model_changed ) {
        result.requires_resubmit = true;
        return result;
    }

    /**
    *    Dynamic connector or mover changes can preserve coarse structural identity but may invalidate
    *    route commitments, so request a stage restart and main-thread revalidation.
    **/
    if ( result.connector_changed || result.mover_changed ) {
        result.requires_revalidation = true;
        result.requires_restart = true;
    }

    /**
    *    Occupancy-only changes are softer: keep the result revalidation path available without
    *    forcing immediate full resubmission.
    **/
    if ( result.occupancy_changed ) {
        result.requires_revalidation = true;
    }

    return result;
}
