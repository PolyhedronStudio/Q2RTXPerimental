/********************************************************************
*
*
*	VKPT Renderer: Data Type Conversion Utilities
*
*	Provides utility functions for converting between different data
*	formats used in the Vulkan path tracing renderer. Currently includes
*	float to half-precision floating point conversion for efficient
*	GPU texture storage.
*
*
********************************************************************/
/*
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

#include "conversion.h"



/**
*
*
*
*	Float to Half-Precision Conversion:
*
*
*
**/
/**
*	@brief	Packs a vec4 (4 floats) into two 32-bit integers as half-precision values.
*	@param	half		Output array of 2 uint32_t values (packed half-precision).
*	@param	vec4		Input array of 4 float values.
*	@note	Each half[i] contains two 16-bit half-precision floats packed together.
*			half[0] contains vec4[0] in bits 0-15 and vec4[1] in bits 16-31.
*			half[1] contains vec4[2] in bits 0-15 and vec4[3] in bits 16-31.
*			This format is efficient for GPU texture uploads where bandwidth is limited.
**/
void packHalf4x16( uint32_t* half, float* vec4 ) {
	// Pack first two floats into first uint32_t.
	half[0] = floatToHalf( vec4[0] ) | ( floatToHalf( vec4[1] ) << 16 );
	
	// Pack second two floats into second uint32_t.
	half[1] = floatToHalf( vec4[2] ) | ( floatToHalf( vec4[3] ) << 16 );
}
