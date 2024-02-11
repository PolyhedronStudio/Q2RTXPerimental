/*
Copyright (C) 1997-2001 Id Software, Inc.
Copyright (C) 2019, NVIDIA CORPORATION. All rights reserved.

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
// cl_fx.c -- entity effects parsing and management

#include "cl_client.h"
//#include "shared/m_flash.h"

static void CL_LogoutEffect(const vec3_t org, int type);

static vec3_t avelocities[NUMVERTEXNORMALS];





// ==============================================================


/*
==============================================================

PARTICLE MANAGEMENT

==============================================================
*/

// We extern "C" so that C files can extern link
extern "C" {

//static cparticle_t  *active_particles, *free_particles;
//
//static cparticle_t  particles[MAX_PARTICLES];

extern uint32_t d_8to24table[256];

cvar_t* cvar_pt_particle_emissive = NULL;
static cvar_t* cl_particle_num_factor = NULL;

};

void FX_Init(void)
{
    cvar_pt_particle_emissive = Cvar_Get("pt_particle_emissive", "10.0", 0);
	cl_particle_num_factor = Cvar_Get("cl_particle_num_factor", "1", 0);
}




