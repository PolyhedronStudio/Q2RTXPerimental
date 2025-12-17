/********************************************************************
*
*
*	ClientGame:	World (Tracing/Linking etc) Header:
*
*
********************************************************************/
#pragma once



/**
*
*
*
*	World Tracing:
*
*
*
**/
/**
*   @brief  Performs a 'Clipping' trace against the world, and all the active in-frame solidEntities.
**/
const cm_trace_t CLG_Trace( const Vector3 &start, const Vector3 *mins, const Vector3 *maxs, const Vector3 &end, const centity_t *passEntity, const cm_contents_t contentMask );
/**
*   @brief  Will perform a clipping trace to the specified entity.
*           If clipEntity == nullptr, it'll perform a clipping trace against the World.
**/
const cm_trace_t CLG_Clip( const Vector3 &start, const Vector3 *mins, const Vector3 *maxs, const Vector3 &end, const centity_t *clipEntity, const cm_contents_t contentMask );
/**
*   @return The type of 'contents' at the given point.
**/
const cm_contents_t CLG_PointContents( const Vector3 &point );