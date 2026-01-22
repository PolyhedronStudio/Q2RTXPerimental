/*
Copyright (C) 2021, NVIDIA CORPORATION. All rights reserved.

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

/********************************************************************
*
*
*	Module Name: Local Fog Volume Control
*
*	This module implements host-side control of local fog volumes for the
*	VKPT renderer. Fog volumes are axis-aligned boxes filled with uniform
*	or gradient fog that are manipulated via the "fog" console command.
*
*	Volume Definition:
*	- Up to MAX_FOG_VOLUMES can be defined
*	- Each volume is an axis-aligned box defined by two diagonal points (a, b)
*	- Parameters include: color (RGB), density (half-extinction distance), and
*	  optional density gradient (softface) along one axis
*	- Renderer supports up to 2 volumes per ray intersection
*
*	Console Command Workflow:
*	  1. Position camera at first corner: fog -v <index> -a here
*	  2. Move to opposite corner: fog -v <index> -b here
*	  3. Set color and density: fog -v <index> -c R,G,B -d <distance>
*	  4. Print parameters: fog -v <index> -p
*	  5. Copy output to map config file for persistence
*
*	Renderer Integration:
*	- Volumes are processed in trace_effects_ray() (path_tracer_rgen.h)
*	- Ray intersection finds two closest volumes, passed through payload
*	- Any-hit shaders accumulate transparent effects and fog between them
*	- Final fog blending: origin→first effect, last effect→ray end
*
*	Map Loading:
*	- All volumes are reset before loading a new map
*	- Define volumes in map-specific config files for automatic loading
*
*
********************************************************************/

#include "fog.h"
#include "vkpt.h"

#include <string.h>

/**
*
*
*
*	Global Data and Command Options:
*
*
*
**/

fog_volume_t fog_volumes[MAX_FOG_VOLUMES];

static const cmd_option_t o_fog[] = {
	{ "v:int", "volume", "volume index" },
	{ "p", "print", "print the selected volume to the console" },
	{ "r", "reset", "reset the selected volume" },
	{ "R", "reset-all", "reset all volumes" },
	{ "a:x,y,z", "", "first point on the volume diagonal, or 'here'" },
	{ "b:x,y,z", "", "second point on the volume diagonal, or 'here'" },
	{ "c:r,g,b", "color", "fog color" },
	{ "d:float", "distance", "distance at which objects in the fog are 50% visible" },
	{ "f:face", "softface", "face where the density is zero: none, xa, xb, ya, yb, za, zb" },
	{ "h", "help", "display this message" },
	{ NULL }
};

static const char* o_softface[] = {
	"none", "xa", "xb", "ya", "yb", "za", "zb"
};

/**
*
*
*
*	Console Command Implementation:
*
*
*
**/

/**
*	@brief	Auto-completion handler for the fog console command.
*	@param	ctx		Completion context.
*	@param	argnum	Argument number being completed.
**/
static void Fog_Cmd_c( genctx_t* ctx, int argnum )
{
	Cmd_Option_c( o_fog, NULL, ctx, argnum );
}

/**
*	@brief	Main fog console command handler.
*
*	Parses command options and manipulates fog volume parameters. Supports
*	selecting volumes by index, setting position points (a/b), color, density,
*	and softface gradient. Can print or reset individual volumes or all volumes.
*
*	@note	Use "fog -h" to display full command syntax and options.
**/
static void Fog_Cmd_f( void )
{
	fog_volume_t* volume = nullptr;
	float x, y, z;
	int index = -1;
	int c, i;
	while( ( c = Cmd_ParseOptions( o_fog ) ) != -1 ) {
		switch(c)
		{
		case 'h':
			Cmd_PrintUsage(o_fog, NULL);
			Com_Printf("Set parameters of a fog volume.\n");
			Cmd_PrintHelp(o_fog);
			return;
		case 'v':
			// Parse and validate volume index
			if( 1 != sscanf( cmd_optarg, "%d", &index ) || index < 0 || index >= MAX_FOG_VOLUMES ) {
				Com_WPrintf( "invalid volume index '%s'\n", cmd_optarg );
				return;
			}
			volume = fog_volumes + index;
			break;
		case 'p':
			// Print volume parameters in command format
			if( !volume ) goto no_volume;
			Com_Printf( "fog -v %d ", index );
			Com_Printf( "-a %.2f,%.2f,%.2f ", volume->point_a[0], volume->point_a[1], volume->point_a[2] );
			Com_Printf( "-b %.2f,%.2f,%.2f ", volume->point_b[0], volume->point_b[1], volume->point_b[2] );
			Com_Printf( "-c %.2f,%.2f,%.2f ", volume->color[0], volume->color[1], volume->color[2] );
			Com_Printf( "-d %.0f ", volume->half_extinction_distance );
			Com_Printf( "-f %s\n", o_softface[volume->softface] );
			break;
		case 'r':
			// Reset selected volume
			if( !volume ) goto no_volume;
			memset( volume, 0, sizeof( *volume ) );
			break;
		case 'R':
			// Reset all volumes
			memset( fog_volumes, 0, sizeof( fog_volumes ) );
			break;
		case 'a':
			// Set first diagonal point
			if( !volume ) goto no_volume;
			if( 3 != sscanf( cmd_optarg, "%f,%f,%f", &x, &y, &z ) ) {
				// Allow "here" keyword to use current camera position
				if( strcmp( cmd_optarg, "here" ) == 0 ) {
					VectorCopy( vkpt_refdef.fd->vieworg, volume->point_a );
					continue;
				}

				Com_WPrintf( "invalid coordinates '%s'\n", cmd_optarg );
				return;
			}
			volume->point_a[0] = x;
			volume->point_a[1] = y;
			volume->point_a[2] = z;
			break;
		case 'b':
			// Set second diagonal point
			if( !volume ) goto no_volume;
			if( 3 != sscanf( cmd_optarg, "%f,%f,%f", &x, &y, &z ) ) {
				// Allow "here" keyword to use current camera position
				if( strcmp( cmd_optarg, "here" ) == 0 ) {
					VectorCopy( vkpt_refdef.fd->vieworg, volume->point_b );
					continue;
				}

				Com_WPrintf( "invalid coordinates '%s'\n", cmd_optarg );
				return;
			}
			volume->point_b[0] = x;
			volume->point_b[1] = y;
			volume->point_b[2] = z;
			break;
		case 'c':
			// Set fog color (RGB)
			if( !volume ) goto no_volume;
			if( 3 != sscanf( cmd_optarg, "%f,%f,%f", &x, &y, &z ) ) {
				Com_WPrintf( "invalid color '%s'\n", cmd_optarg );
				return;
			}
			volume->color[0] = x;
			volume->color[1] = y;
			volume->color[2] = z;
			break;
		case 'd':
			// Set half-extinction distance (density)
			if( !volume ) goto no_volume;
			if( 1 != sscanf( cmd_optarg, "%f", &x ) ) {
				Com_WPrintf( "invalid distance '%s'\n", cmd_optarg );
				return;
			}
			volume->half_extinction_distance = x;
			break;
		case 'f':
			// Set softface gradient direction
			if( !volume ) goto no_volume;
			for( i = 0; i < (int)q_countof( o_softface ); i++ ) {
				if( strcmp( cmd_optarg, o_softface[i] ) == 0 ) {
					volume->softface = i;
					break;
				}
			}
			if( i >= (int)q_countof( o_softface ) ) {
				Com_WPrintf( "invalid value for softface '%s'\n", cmd_optarg );
				return;
			}
			break;
		default:
			return;
		}
	}
	return;
	
no_volume:
	Com_WPrintf( "volume not specified\n" );
}

/**
*
*
*
*	Public Interface:
*
*
*
**/

/**
*	@brief	Initialize fog volume system and register console command.
**/
void vkpt_fog_init( void )
{
	vkpt_fog_reset();
	
	cmdreg_t cmds[] = {
		{ "fog", &Fog_Cmd_f, &Fog_Cmd_c },
		{ nullptr, nullptr, nullptr }
	};
	Cmd_Register( cmds );
}

/**
*	@brief	Shutdown fog volume system and unregister console command.
**/
void vkpt_fog_shutdown( void )
{
	Cmd_RemoveCommand( "fog" );
}

/**
*	@brief	Reset all fog volumes to default state.
*
*	Called automatically before loading a new map. Clears all volume
*	parameters including position, color, and density settings.
**/
void vkpt_fog_reset( void )
{
	memset( fog_volumes, 0, sizeof( fog_volumes ) );
}

/**
*	@brief	Upload fog volume data to GPU shader structures.
*
*	Converts host-side fog volume definitions into shader-compatible format.
*	Validates volumes, computes min/max bounds from diagonal points, and
*	converts half-extinction distance to density values. For gradient fog
*	(softface), derives linear density function coefficients along the
*	specified axis.
*
*	@param	dst		Destination shader fog volume array (MAX_FOG_VOLUMES).
*
*	@note	Only uploads volumes with valid density (> 0) and non-zero size.
*	@note	Density calculation: k = -ln(0.5) / half_extinction_distance.
*	@note	Gradient fog uses density vector (ax, ay, az, b) where density = a·pos + b.
**/
void vkpt_fog_upload( ShaderFogVolume* dst )
{
	memset( dst, 0, sizeof( ShaderFogVolume ) * MAX_FOG_VOLUMES );
	
	for( int i = 0; i < MAX_FOG_VOLUMES; i++ ) {
		const fog_volume_t* src = fog_volumes + i;
		
		// Skip invalid volumes (zero density or zero-size box)
		if( src->half_extinction_distance <= 0.f || src->point_a[0] == src->point_b[0] || src->point_a[1] == src->point_b[1] || src->point_a[2] == src->point_b[2] )
			continue;

		VectorCopy( src->color, dst->color );

		// Compute axis-aligned bounding box from diagonal points
		for( int axis = 0; axis < 3; axis++ ) {
			dst->mins[axis] = min( src->point_a[axis], src->point_b[axis] );
			dst->maxs[axis] = max( src->point_a[axis], src->point_b[axis] );
		}

		// Convert half-extinction distance to density using exponential decay formula
		// exp(-kx) = 0.5  =>  k = -ln(0.5) / x  =>  k ≈ 0.69315 / x
		float density = 0.69315f / src->half_extinction_distance;

		if( 1 <= src->softface && src->softface <= 6 ) {
			// Gradient fog: density varies linearly along one axis
			
			// Extract axis index (0=x, 1=y, 2=z) from softface value
			int axis = ( src->softface - 1 ) / 2;

			// Determine gradient endpoints: density is 0 at pos0, full at pos1
			float pos0 = ( src->softface & 1 ) ? src->point_a[axis] : src->point_b[axis];
			float pos1 = ( src->softface & 1 ) ? src->point_b[axis] : src->point_a[axis];

			// Compute linear function coefficients: density(p) = a*p + b
			float a = density / ( pos1 - pos0 );
			float b = -pos0 * a;

			// Store as 4D vector (ax, ay, az, b) for shader evaluation
			dst->density[axis] = a;
			dst->density[3] = b;
		} else {
			// Uniform fog: constant density throughout volume
			Vector4Set( dst->density, 0.f, 0.f, 0.f, density );
		}
		
		dst->is_active = 1;

		++dst;
	}
}
