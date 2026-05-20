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
bool SVG_SkeletalHitboxes_RefinePointTrace( svg_trace_t &trace, const Vector3 &shotStart, const Vector3 &shotEnd, const svg_base_edict_t *target );
#endif
