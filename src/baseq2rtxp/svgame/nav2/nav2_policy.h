/********************************************************************
*
*
*	ServerGame: Nav2 Policy Defaults and CVar Helpers
*
*
********************************************************************/
#pragma once

#include "svgame/svg_local.h"

#include "svgame/nav2/nav2_types.h"


/**
*
*
*	Nav2 Policy Public API:
*
*
**/
/**
*	@brief	Resolve the effective nav2 max-drop-height-cap fallback used by gameplay policy bootstrap.
*	@return	Cvar-backed cap when configured, otherwise the nav2 default cap.
*	@note	This keeps gameplay bootstrap on nav2-owned helper surfaces even while the underlying cvar is still legacy-owned.
**/
inline double SVG_Nav2_Policy_GetMaxDropHeightCap( void ) {
    /**
    *	Cache the runtime cvar pointer locally so repeated policy queries stay cheap.
    **/
    static cvar_t *s_nav2_policy_max_drop_height_cap = nullptr;

    /**
    *	Resolve the runtime cvar lazily through the engine import the first time the helper is used.
    **/
    if ( !s_nav2_policy_max_drop_height_cap ) {
        s_nav2_policy_max_drop_height_cap = gi.cvar( "nav_max_drop_height_cap", "192", 0 );
    }

    /**
    *	Prefer the configured runtime cap when it is positive.
    **/
    if ( s_nav2_policy_max_drop_height_cap && s_nav2_policy_max_drop_height_cap->value > 0.0f ) {
        return s_nav2_policy_max_drop_height_cap->value;
    }

    /**
    *	Fall back to the nav2-owned default cap when no positive override is configured.
    **/
    return NAV_DEFAULT_MAX_DROP_HEIGHT_CAP;
}

/**
*	@brief	Return whether nav2 async queue mode is currently enabled.
*	@return	True when async queue mode is enabled by cvar.
*	@note	This helper centralizes the temporary legacy-cvar bridge so gameplay code no longer reads the cvar directly.
**/
inline const bool SVG_Nav2_Policy_IsAsyncQueueEnabled( void ) {
    /**
    *	Cache the runtime cvar pointer locally so repeated queue-mode checks stay cheap.
    **/
    static cvar_t *s_nav2_policy_async_queue_mode = nullptr;

    /**
    *	Resolve the runtime cvar lazily through the engine import the first time the helper is used.
    **/
    if ( !s_nav2_policy_async_queue_mode ) {
        s_nav2_policy_async_queue_mode = gi.cvar( "nav_nav_async_queue_mode", "1", 0 );
    }

    /**
    *	Require the queue-mode cvar to exist and hold a non-zero enabled value.
    **/
    return s_nav2_policy_async_queue_mode && s_nav2_policy_async_queue_mode->integer != 0;
}
