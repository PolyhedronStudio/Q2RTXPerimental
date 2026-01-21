/********************************************************************
*
*
*	VKPT Renderer: Matrix Operations
*
*	Provides matrix construction and manipulation functions for the
*	Vulkan path tracing renderer. Includes entity matrix creation,
*	projection matrices, view matrices, matrix inversion, and
*	matrix multiplication operations.
*
*
********************************************************************/
/*
Copyright (C) 2018 Christoph Schied
Copyright (C) 2003-2006 Andrey Nazarov

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

#include "vkpt.h"



/**
*
*
*
*	Entity Matrix Construction (Internal):
*
*
*
**/
/**
*	@brief	Internal function to create a 4x4 transformation matrix for an entity.
*	@param	matrix		Output matrix (4x4 in column-major order).
*	@param	e			Pointer to entity data containing position, angles, and scale.
*	@param	mirror		If true, mirrors the entity on the Y axis (for left-handed models).
*	@note	This function interpolates between the entity's current and old origin
*			using the backlerp value for smooth animation. The matrix is constructed
*			by combining rotation (from angles), scale, and translation (origin).
**/
static void internal_create_entity_matrix( mat4_t matrix, entity_t *e, bool mirror ) {
	vec3_t axis[3];
	vec3_t origin;

	// Interpolate origin between current and old position for smooth movement.
	origin[0] = ( 1.f - e->backlerp ) * e->origin[0] + e->backlerp * e->oldorigin[0];
	origin[1] = ( 1.f - e->backlerp ) * e->origin[1] + e->backlerp * e->oldorigin[1];
	origin[2] = ( 1.f - e->backlerp ) * e->origin[2] + e->backlerp * e->oldorigin[2];

	// Convert entity angles to axis vectors (rotation basis).
	AnglesToAxis( e->angles, axis );

	// Get entity scale, defaulting to 1.0 if not specified.
	float scale = ( e->scale > 0.f ) ? e->scale : 1.f;

	// Apply scale to each axis, with optional Y-axis mirroring.
	vec3_t scales = { scale, scale, scale };
	if ( mirror ) {
		scales[1] *= -1.f;
	}

	// Construct the 4x4 matrix in column-major order:
	// First column (X axis after rotation and scale).
	matrix[0]  = axis[0][0] * scales[0];
	matrix[1]  = axis[0][1] * scales[0];
	matrix[2]  = axis[0][2] * scales[0];
	matrix[3]  = 0.0f;

	// Second column (Y axis after rotation and scale).
	matrix[4]  = axis[1][0] * scales[1];
	matrix[5]  = axis[1][1] * scales[1];
	matrix[6]  = axis[1][2] * scales[1];
	matrix[7]  = 0.0f;

	// Third column (Z axis after rotation and scale).
	matrix[8]  = axis[2][0] * scales[2];
	matrix[9]  = axis[2][1] * scales[2];
	matrix[10] = axis[2][2] * scales[2];
	matrix[11] = 0.0f;

	// Fourth column (translation/origin).
	matrix[12] = origin[0];
	matrix[13] = origin[1];
	matrix[14] = origin[2];
	matrix[15] = 1.0f;
}



/**
*
*
*
*	Entity Matrix Construction (Public):
*
*
*
**/
/**
*	@brief	Creates a standard transformation matrix for a world entity.
*	@param	matrix		Output matrix (4x4 in column-major order).
*	@param	e			Pointer to entity data.
**/
void create_entity_matrix( mat4_t matrix, entity_t *e ) {
	internal_create_entity_matrix( matrix, e, false );
}

/**
*	@brief	Creates a transformation matrix for the viewweapon (first-person weapon model).
*	@param	matrix		Output matrix (4x4 in column-major order).
*	@param	e			Pointer to entity data for the viewweapon.
*	@note	This function applies additional FOV adjustments to make the weapon appear
*			at the correct scale when rendered with a different FOV than the main view.
*			It also handles left-handed weapon models by mirroring when info_hand is set.
**/
void create_viewweapon_matrix( mat4_t matrix, entity_t *e ) {
	extern cvar_t *info_hand;
	extern cvar_t *cl_adjustfov;
	extern cvar_t *cl_gunfov;

	// Create base entity matrix, with mirroring if left-handed model.
	internal_create_entity_matrix( matrix, e, info_hand->integer == 1 );

	// Apply gun FOV adjustment if cl_gunfov is set.
	if ( cl_gunfov->value > 0 ) {
		float gunfov_x, gunfov_y;

		// Clamp gun FOV to reasonable range.
		gunfov_x = Cvar_ClampValue( cl_gunfov, 30, 160 );

		// Calculate vertical gun FOV based on aspect ratio settings.
		if ( cl_adjustfov->integer ) {
			gunfov_y = V_CalcFov( gunfov_x, 4, 3 );
			gunfov_x = V_CalcFov( gunfov_y, vkpt_refdef.fd->height, vkpt_refdef.fd->width );
		} else {
			gunfov_y = V_CalcFov( gunfov_x, vkpt_refdef.fd->width, vkpt_refdef.fd->height );
		}

		// Construct a matrix that adjusts the weapon scale to match the gun FOV.
		// This is done by transforming the weapon into view space, applying the FOV
		// adjustment, then transforming back to world space.
		mat4_t tmp;
		mult_matrix_matrix( tmp, vkpt_refdef.view_matrix, matrix );

		// Create FOV adjustment matrix that scales X and Y based on FOV ratio.
		mat4_t adjust;
		adjust[0] = tan( vkpt_refdef.fd->fov_x * M_PI / 360.0 ) / tan( gunfov_x * M_PI / 360.0 );
		adjust[4] = 0;
		adjust[8] = 0;
		adjust[12] = 0;

		adjust[1] = 0;
		adjust[5] = tan( vkpt_refdef.fd->fov_y * M_PI / 360.0 ) / tan( gunfov_y * M_PI / 360.0 );
		adjust[9] = 0;
		adjust[13] = 0;

		adjust[2] = 0;
		adjust[6] = 0;
		adjust[10] = 1;
		adjust[14] = 0;

		adjust[3] = 0;
		adjust[7] = 0;
		adjust[11] = 0;
		adjust[15] = 1;

		// Apply adjustment and transform back to world space.
		mat4_t tmp2;
		mult_matrix_matrix( tmp2, adjust, tmp );
		mult_matrix_matrix( matrix, vkpt_refdef.view_matrix_inv, tmp2 );
	}
}


/**
*
*
*
*	Projection Matrix Construction:
*
*
*
**/
/**
*	@brief	Creates a perspective projection matrix for 3D rendering.
*	@param	matrix		Output matrix (4x4 in column-major order).
*	@param	znear		Near clipping plane distance.
*	@param	zfar		Far clipping plane distance.
*	@param	fov_x		Horizontal field of view in degrees.
*	@param	fov_y		Vertical field of view in degrees.
*	@note	This function constructs a projection matrix suitable for Vulkan,
*			with the Y axis inverted compared to OpenGL (negative Y scale).
**/
void create_projection_matrix( mat4_t matrix, float znear, float zfar, float fov_x, float fov_y ) {
	float xmin, xmax, ymin, ymax;
	float width, height, depth;

	// Calculate frustum bounds from field of view.
	ymax = znear * tan( fov_y * M_PI / 360.0 );
	ymin = -ymax;

	xmax = znear * tan( fov_x * M_PI / 360.0 );
	xmin = -xmax;

	width = xmax - xmin;
	height = ymax - ymin;
	depth = zfar - znear;

	// Construct perspective projection matrix (column-major order).
	matrix[0] = 2 * znear / width;
	matrix[4] = 0;
	matrix[8] = ( xmax + xmin ) / width;
	matrix[12] = 0;

	matrix[1] = 0;
	matrix[5] = -2 * znear / height;  // Negative for Vulkan Y-flip.
	matrix[9] = ( ymax + ymin ) / height;
	matrix[13] = 0;

	matrix[2] = 0;
	matrix[6] = 0;
	matrix[10] = ( zfar + znear ) / depth;
	matrix[14] = 2 * zfar * znear / depth;

	matrix[3] = 0;
	matrix[7] = 0;
	matrix[11] = 1;
	matrix[15] = 0;
}

/**
*	@brief	Creates an orthographic projection matrix (parallel projection).
*	@param	matrix		Output matrix (4x4 in column-major order).
*	@param	xmin		Left edge of the view volume.
*	@param	xmax		Right edge of the view volume.
*	@param	ymin		Bottom edge of the view volume.
*	@param	ymax		Top edge of the view volume.
*	@param	znear		Near clipping plane distance.
*	@param	zfar		Far clipping plane distance.
*	@note	Used for 2D rendering and shadow maps. This matrix maps the specified
*			box directly to normalized device coordinates without perspective division.
**/
void create_orthographic_matrix( mat4_t matrix, float xmin, float xmax,
		float ymin, float ymax, float znear, float zfar ) {
	float width, height, depth;

	width = xmax - xmin;
	height = ymax - ymin;
	depth = zfar - znear;

	// Construct orthographic projection matrix (column-major order).
	matrix[0] = 2 / width;
	matrix[4] = 0;
	matrix[8] = 0;
	matrix[12] = -( xmax + xmin ) / width;

	matrix[1] = 0;
	matrix[5] = 2 / height;
	matrix[9] = 0;
	matrix[13] = -( ymax + ymin ) / height;

	matrix[2] = 0;
	matrix[6] = 0;
	matrix[10] = 1 / depth;
	matrix[14] = -znear / depth;

	matrix[3] = 0;
	matrix[7] = 0;
	matrix[11] = 0;
	matrix[15] = 1;
}



/**
*
*
*
*	View Matrix Construction:
*
*
*
**/
/**
*	@brief	Creates a view matrix from camera position and orientation.
*	@param	matrix		Output matrix (4x4 in column-major order).
*	@param	fd			Reference definition containing view position and angles.
*	@note	This matrix transforms world coordinates to view (camera) space.
*			The camera looks down the positive X axis, with Y right and Z up.
**/
void create_view_matrix( mat4_t matrix, refdef_t *fd ) {
	vec3_t viewaxis[3];

	// Convert view angles to axis vectors.
	AnglesToAxis( fd->viewangles, viewaxis );

	// Construct view matrix (column-major order).
	// First column: right vector (negative Y axis in world).
	matrix[0]  = -viewaxis[1][0];
	matrix[1]  = viewaxis[2][0];
	matrix[2]  = viewaxis[0][0];
	matrix[3]  = 0;

	// Second column: up vector.
	matrix[4]  = -viewaxis[1][1];
	matrix[5]  = viewaxis[2][1];
	matrix[6]  = viewaxis[0][1];
	matrix[7]  = 0;

	// Third column: forward vector.
	matrix[8]  = -viewaxis[1][2];
	matrix[9]  = viewaxis[2][2];
	matrix[10] = viewaxis[0][2];
	matrix[11] = 0;

	// Fourth column: translation (negative dot product with origin).
	matrix[12] = DotProduct( viewaxis[1], fd->vieworg );
	matrix[13] = -DotProduct( viewaxis[2], fd->vieworg );
	matrix[14] = -DotProduct( viewaxis[0], fd->vieworg );
	matrix[15] = 1;
}



/**
*
*
*
*	Matrix Inversion:
*
*
*
**/
/**
*	@brief	Computes the inverse of a 4x4 matrix using cofactor expansion.
*	@param	m			Input matrix (4x4 in column-major order).
*	@param	inv			Output inverted matrix (4x4 in column-major order).
*	@note	This function uses the analytic inversion formula based on cofactors
*			and determinant. It assumes the matrix is invertible (non-zero determinant).
**/
void inverse( const mat4_t m, mat4_t inv ) {
	// Compute cofactors for each element of the inverse matrix.
	inv[0] = m[5]  * m[10] * m[15] -
	         m[5]  * m[11] * m[14] -
	         m[9]  * m[6]  * m[15] +
	         m[9]  * m[7]  * m[14] +
	         m[13] * m[6]  * m[11] -
	         m[13] * m[7]  * m[10];
	
	inv[1] = -m[1]  * m[10] * m[15] +
	          m[1]  * m[11] * m[14] +
	          m[9]  * m[2] * m[15] -
	          m[9]  * m[3] * m[14] -
	          m[13] * m[2] * m[11] +
	          m[13] * m[3] * m[10];
	
	inv[2] = m[1]  * m[6] * m[15] -
	         m[1]  * m[7] * m[14] -
	         m[5]  * m[2] * m[15] +
	         m[5]  * m[3] * m[14] +
	         m[13] * m[2] * m[7] -
	         m[13] * m[3] * m[6];
	
	inv[3] = -m[1] * m[6] * m[11] +
	          m[1] * m[7] * m[10] +
	          m[5] * m[2] * m[11] -
	          m[5] * m[3] * m[10] -
	          m[9] * m[2] * m[7] +
	          m[9] * m[3] * m[6];
	
	inv[4] = -m[4]  * m[10] * m[15] +
	          m[4]  * m[11] * m[14] +
	          m[8]  * m[6]  * m[15] -
	          m[8]  * m[7]  * m[14] -
	          m[12] * m[6]  * m[11] +
	          m[12] * m[7]  * m[10];
	
	inv[5] = m[0]  * m[10] * m[15] -
	         m[0]  * m[11] * m[14] -
	         m[8]  * m[2] * m[15] +
	         m[8]  * m[3] * m[14] +
	         m[12] * m[2] * m[11] -
	         m[12] * m[3] * m[10];
	
	inv[6] = -m[0]  * m[6] * m[15] +
	          m[0]  * m[7] * m[14] +
	          m[4]  * m[2] * m[15] -
	          m[4]  * m[3] * m[14] -
	          m[12] * m[2] * m[7] +
	          m[12] * m[3] * m[6];
	
	inv[7] = m[0] * m[6] * m[11] -
	         m[0] * m[7] * m[10] -
	         m[4] * m[2] * m[11] +
	         m[4] * m[3] * m[10] +
	         m[8] * m[2] * m[7] -
	         m[8] * m[3] * m[6];
	
	inv[8] = m[4]  * m[9] * m[15] -
	         m[4]  * m[11] * m[13] -
	         m[8]  * m[5] * m[15] +
	         m[8]  * m[7] * m[13] +
	         m[12] * m[5] * m[11] -
	         m[12] * m[7] * m[9];
	
	inv[9] = -m[0]  * m[9] * m[15] +
	          m[0]  * m[11] * m[13] +
	          m[8]  * m[1] * m[15] -
	          m[8]  * m[3] * m[13] -
	          m[12] * m[1] * m[11] +
	          m[12] * m[3] * m[9];
	
	inv[10] = m[0]  * m[5] * m[15] -
	          m[0]  * m[7] * m[13] -
	          m[4]  * m[1] * m[15] +
	          m[4]  * m[3] * m[13] +
	          m[12] * m[1] * m[7] -
	          m[12] * m[3] * m[5];
	
	inv[11] = -m[0] * m[5] * m[11] +
	           m[0] * m[7] * m[9] +
	           m[4] * m[1] * m[11] -
	           m[4] * m[3] * m[9] -
	           m[8] * m[1] * m[7] +
	           m[8] * m[3] * m[5];
	
	inv[12] = -m[4]  * m[9] * m[14] +
	           m[4]  * m[10] * m[13] +
	           m[8]  * m[5] * m[14] -
	           m[8]  * m[6] * m[13] -
	           m[12] * m[5] * m[10] +
	           m[12] * m[6] * m[9];
	
	inv[13] = m[0]  * m[9] * m[14] -
	          m[0]  * m[10] * m[13] -
	          m[8]  * m[1] * m[14] +
	          m[8]  * m[2] * m[13] +
	          m[12] * m[1] * m[10] -
	          m[12] * m[2] * m[9];
	
	inv[14] = -m[0]  * m[5] * m[14] +
	           m[0]  * m[6] * m[13] +
	           m[4]  * m[1] * m[14] -
	           m[4]  * m[2] * m[13] -
	           m[12] * m[1] * m[6] +
	           m[12] * m[2] * m[5];
	
	inv[15] = m[0] * m[5] * m[10] -
	          m[0] * m[6] * m[9] -
	          m[4] * m[1] * m[10] +
	          m[4] * m[2] * m[9] +
	          m[8] * m[1] * m[6] -
	          m[8] * m[2] * m[5];

	// Calculate determinant from first row of cofactors.
	float det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];

	// Divide all elements by determinant to get final inverse.
	det = 1.0f / det;

	for ( int i = 0; i < 16; i++ ) {
		inv[i] = inv[i] * det;
	}
}



/**
*
*
*
*	Matrix Multiplication:
*
*
*
**/
/**
*	@brief	Multiplies two 4x4 matrices.
*	@param	p			Output matrix (result of a * b).
*	@param	a			First input matrix (4x4 in column-major order).
*	@param	b			Second input matrix (4x4 in column-major order).
*	@note	Performs matrix multiplication p = a * b. The result is stored in p,
*			which can be the same as either input matrix.
**/
void mult_matrix_matrix( mat4_t p, const mat4_t a, const mat4_t b ) {
	// Compute each element of the product matrix.
	for ( int i = 0; i < 4; i++ ) {
		for ( int j = 0; j < 4; j++ ) {
			p[i * 4 + j] =
				a[0 * 4 + j] * b[i * 4 + 0] +
				a[1 * 4 + j] * b[i * 4 + 1] +
				a[2 * 4 + j] * b[i * 4 + 2] +
				a[3 * 4 + j] * b[i * 4 + 3];
		}
	}
}

/**
*	@brief	Multiplies a 4x4 matrix by a 4D vector.
*	@param	v			Output vector (result of a * b).
*	@param	a			Input matrix (4x4 in column-major order).
*	@param	b			Input vector (4D).
*	@note	Performs matrix-vector multiplication v = a * b. This is used to
*			transform a point or direction by a matrix.
**/
void mult_matrix_vector( vec4_t v, const mat4_t a, const vec4_t b ) {
	// Compute each component of the result vector.
	for ( int j = 0; j < 4; j++ ) {
		v[j] =
			a[0 * 4 + j] * b[0] +
			a[1 * 4 + j] * b[1] +
			a[2 * 4 + j] * b[2] +
			a[3 * 4 + j] * b[3];
	}
}
