/********************************************************************
*
*
*   Shared Collision Detection: Trace Struct.
*
*
********************************************************************/
#pragma once


/**
*   @brief  A trace is basically 'sweeping' the collision shape(bounding box) through the world
*           and returning the results of that 'sweep'.
*           (ie, how far did it get, whether it is inside of a solid or not, etc)
**/
typedef struct trace_s {
    //! If true, the entire trace took place within a solid(brush) area.
    bool    allsolid;
    //! If true, the initial point was in a solid area.
    bool    startsolid;
    //! Contents on other side of surface hit. (The inside of the brush.)
    cm_contents_t  contents;

    //! Not set by CM_*() functions. But respectively set by the game code.
    int32_t entityNumber;

    //! The fraction of the trace that was completed. ( 1.0 = didn't hit anything. )
    double      fraction;
    //! Final position.
    Vector3     endpos;

    /**
    *   The first (most close) surface hit by the trace.
    **/
    //! First Surface normal at impact.
    cm_plane_t    plane;
    //! First Surface hit.
    cm_surface_t *surface;
    //! First Surface collision model material pointer.
    struct cm_material_s *material;

    /**
    *   The 'second best' surface hit by the trace.
    **/
    //! second surface normal at impact
    cm_plane_t      plane2;
    //! Second surface hit.
    cm_surface_t *surface2;
    //! Second surface collision model material pointer.
    struct cm_material_s *material2;
} cm_trace_t;
