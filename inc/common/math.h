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

#ifndef MATH_H
#define MATH_H

// Extern C
QEXTERN_C_OPEN

// Moved to shared/
//#define NUMVERTEXNORMALS    162
//
//void MakeNormalVectors(const vec3_t forward, vec3_t right, vec3_t up);
//
//extern const vec3_t bytedirs[NUMVERTEXNORMALS];
//
//int DirToByte(const vec3_t dir);
////void ByteToDir(int index, vec3_t dir);

void SetPlaneType(cm_plane_t *plane);
void SetPlaneSignbits(cm_plane_t *plane);

#define BOX_INFRONT     1
#define BOX_BEHIND      2
#define BOX_INTERSECTS  3

int BoxOnPlaneSide(const vec3_t emins, const vec3_t emaxs, const cm_plane_t *p);

static inline int BoxOnPlaneSideFast(const vec3_t emins, const vec3_t emaxs, const cm_plane_t *p)
{
    // fast axial cases
    if (p->type < 3) {
        if (p->dist <= emins[p->type])
            return BOX_INFRONT;
        if (p->dist >= emaxs[p->type])
            return BOX_BEHIND;
        return BOX_INTERSECTS;
    }

    // slow generic case
    return BoxOnPlaneSide(emins, emaxs, p);
}

static inline vec_t PlaneDiffFast(const vec3_t v, const cm_plane_t *p)
{
    // fast axial cases
    if (p->type < 3) {
        return v[p->type] - p->dist;
    }

    // slow generic case
    return PlaneDiff(v, p);
}

void SetupRotationMatrix(vec3_t matrix[3], const vec3_t dir, float degrees);

// Extern C
QEXTERN_C_CLOSE

#endif // MATH_H
