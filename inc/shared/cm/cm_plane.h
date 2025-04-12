/********************************************************************
*
*
*   Shared Collision Detection: Clipping Plane.
*
*
********************************************************************/
#pragma once



/**
*   @brief  BSP Brush plane_t structure. To acquire the origin,
*           scale normal by dist.
**/
typedef struct cm_plane_s {
    vec3_t  normal;
    float   dist;
    byte    type;           // for fast side tests
    byte    signbits;       // signx + (signy<<1) + (signz<<1)
    byte    pad[ 2 ];
} cm_plane_t;

//! 0-2 are axial planes.
#define PLANE_X         0
#define PLANE_Y         1
#define PLANE_Z         2

//! 3-5 are non-axial planes snapped to the nearest.
#define PLANE_ANYX      3
#define PLANE_ANYY      4
#define PLANE_ANYZ      5

//! Planes (x&~1) and (x&~1)+1 are always opposites.
#define PLANE_NON_AXIAL 6
