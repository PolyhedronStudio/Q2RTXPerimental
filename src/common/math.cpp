/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include "shared/shared.h"
#include "common/math.h"

void SetPlaneType(cm_plane_t *plane)
{
    vec_t *normal = plane->normal;

    if ( normal[ 0 ] == 1 ) {
        plane->type = PLANE_X;
        return;
    }
    if ( normal[ 1 ] == 1 ) {
        plane->type = PLANE_Y;
        return;
    }
    if ( normal[ 2 ] == 1 ) {
        plane->type = PLANE_Z;
        return;
    }

    plane->type = PLANE_NON_AXIAL;
}

void SetPlaneSignbits(cm_plane_t *plane)
{
    int bits = 0;

    if ( plane->normal[ 0 ] < 0 ) {
        bits |= 1;
    }
    if ( plane->normal[ 1 ] < 0 ) {
        bits |= 2;
    }
    if ( plane->normal[ 2 ] < 0 ) {
        bits |= 4;
    }

    plane->signbits = bits;
}

/*
==================
BoxOnPlaneSide

Returns 1, 2, or 1 + 2
==================
*/
int BoxOnPlaneSide(const vec3_t emins, const vec3_t emaxs, const cm_plane_t *p)
{
    const vec_t *bounds[ 2 ] = { emins, emaxs };
    int     i = p->signbits & 1;
    int     j = ( p->signbits >> 1 ) & 1;
    int     k = ( p->signbits >> 2 ) & 1;

    #define P(i, j, k) \
    p->normal[0] * bounds[i][0] + \
    p->normal[1] * bounds[j][1] + \
    p->normal[2] * bounds[k][2]

    vec_t   dist1 = P( i ^ 1, j ^ 1, k ^ 1 );
    vec_t   dist2 = P( i, j, k );
    int     sides = 0;

    #undef P

    if ( dist1 >= p->dist ) {
        sides = BOX_INFRONT;
    }
    if ( dist2 < p->dist ) {
        sides |= BOX_BEHIND;
    }

    return sides;
}

void RotatePointAroundVector(vec3_t dst, const vec3_t dir, const vec3_t point, float degrees)
{
    vec3_t  matrix[3];

    SetupRotationMatrix(matrix, dir, degrees);

    dst[0] = DotProduct(matrix[0], point);
    dst[1] = DotProduct(matrix[1], point);
    dst[2] = DotProduct(matrix[2], point);
}

/*
==================
SetupRotationMatrix

Setup rotation matrix given the normalized direction vector and angle to rotate
around this vector. Adapted from Mesa 3D implementation of _math_matrix_rotate.
==================
*/
void SetupRotationMatrix(vec3_t matrix[3], const vec3_t dir, float degrees)
{
    vec_t   angle, s, c, one_c, xx, yy, zz, xy, yz, zx, xs, ys, zs;

    angle = DEG2RAD(degrees);
    s = sin(angle);
    c = cos(angle);
    one_c = 1.0F - c;

    xx = dir[0] * dir[0];
    yy = dir[1] * dir[1];
    zz = dir[2] * dir[2];
    xy = dir[0] * dir[1];
    yz = dir[1] * dir[2];
    zx = dir[2] * dir[0];
    xs = dir[0] * s;
    ys = dir[1] * s;
    zs = dir[2] * s;

    matrix[0][0] = (one_c * xx) + c;
    matrix[0][1] = (one_c * xy) - zs;
    matrix[0][2] = (one_c * zx) + ys;

    matrix[1][0] = (one_c * xy) + zs;
    matrix[1][1] = (one_c * yy) + c;
    matrix[1][2] = (one_c * yz) - xs;

    matrix[2][0] = (one_c * zx) - ys;
    matrix[2][1] = (one_c * yz) + xs;
    matrix[2][2] = (one_c * zz) + c;
}

