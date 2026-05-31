/********************************************************************
*
*
*    ServerGame: Skeletal Hitbox Trace Refinement.
*
*
********************************************************************/
#pragma once

// Forward declarations for math and svgame types used by this interface.
struct Vector3;
struct svg_trace_t;
struct svg_base_edict_t;



/**
*   @brief  Refine a coarse point trace against IQM skeletal hitboxes for the target entity.
*   @param  trace       In/out coarse trace result. Updated when a skeletal hitbox is hit.
*   @param  shotStart   World-space trace start.
*   @param  shotEnd     World-space trace end.
*   @param  target      Entity whose skeletal hitboxes should be tested.
*   @return True when a skeletal hitbox hit was found and trace was refined.
*   @note   This helper does not replace generic collision. It only refines a point trace.
**/
#ifdef __cplusplus
/**
*   @brief  Check whether an entity has valid skeletal hitbox data for refinement.
*   @param  target  Entity to inspect.
*   @return True when skeletal refinement data is present and usable.
*   @note   Callers can use this to keep legacy coarse trace behavior for static/non-skeletal models.
**/
bool SVG_SkeletalHitboxes_HasRefinableData( const svg_base_edict_t *target );

bool SVG_SkeletalHitboxes_RefinePointTrace( svg_trace_t &trace, const Vector3 &shotStart, const Vector3 &shotEnd, const svg_base_edict_t *target );

/**
*   @brief  Trace a point segment against the entity animated model bounds envelope.
*   @param  trace       In/out trace result that receives the fallback hit on success.
*   @param  shotStart   World-space trace start.
*   @param  shotEnd     World-space trace end.
*   @param  target      Entity whose animated frame bounds should be tested.
*   @return True when the segment intersects the animated bounds envelope.
*   @note   This is intended as a conservative fallback gate for bullet traces when
*           per-hitbox refinement rejects a coarse entity hit.
**/
bool SVG_SkeletalHitboxes_TracePointAgainstAnimatedBounds( svg_trace_t &trace, const Vector3 &shotStart, const Vector3 &shotEnd, const svg_base_edict_t *target );

#endif
