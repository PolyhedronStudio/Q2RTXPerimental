/********************************************************************
*
*	Copyright (C) 2011 Thilo Schulz, Matthias Bentrup, Zack Middleton
*	Copyright (C) 2011-2019 Quake III Arena source code
*	Copyright (C) 2024 Q2RTXPerimental Contributors
*
*	Module Name: IQM Skeletal Model Loader
*
*	This module implements loading and animation of IQM (Inter-Quake Model)
*	format skeletal animated models. IQM is a compact binary format supporting
*	skeletal animation, vertex blending, tangent-space normals, and multiple
*	animations per model.
*
*	Key Features:
*	- Skeletal hierarchy with parent-child joints
*	- Compressed animation poses (translation, rotation, scale)
*	- 4-bone vertex skinning with blend weights
*	- Multiple meshes per model with material references
*	- Bounding box data per animation frame
*	- Tangent-space support for normal mapping
*
*	IQM Format Structure:
*	- Header with offsets to all data sections
*	- Vertex arrays (position, normal, texcoord, tangent, blend indices/weights, color)
*	- Triangle indices (3 per triangle)
*	- Meshes (submesh ranges with materials)
*	- Joints (bind pose hierarchy)
*	- Poses (animation frame transforms)
*	- Animations (named animation sequences)
*	- Bounds (per-frame bounding boxes)
*
*	Skeletal Animation Pipeline:
*	1. Load IQM file and validate format
*	2. Extract joint hierarchy and bind pose
*	3. Compute inverse bind matrices for skinning
*	4. Decompress animation frame data
*	5. Interpolate between frames at runtime
*	6. Transform from local joint space to model space
*	7. Apply bone controllers and root motion
*
*	License: GNU GPL v2 or later
*
********************************************************************/

#include <assert.h>
#include <shared/shared.h>
#include <format/iqm.h>
#include <refresh/models.h>
#include <refresh/refresh.h>

#include "common/skeletalmodels/cm_skm.h"


/********************************************************************
*
*	Quaternion and Matrix Math Functions
*
*	This section provides mathematical primitives for skeletal animation:
*	- 3x4 matrix operations (multiplication, inversion)
*	- Quaternion operations (slerp, normalization)
*	- TRS (Translation-Rotation-Scale) to matrix conversion
*
*	Matrix Layout: 3x4 row-major
*	[0  1  2  3 ]   [Xx Xy Xz Tx]
*	[4  5  6  7 ]   [Yx Yy Yz Ty]
*	[8  9  10 11]   [Zx Zy Zz Tz]
*	Implied row: [0 0 0 1]
*
*	Quaternion Layout: [x, y, z, w] where w is the scalar component
*
********************************************************************/

/**
*	@brief	Validate that a data section is within the file bounds
*	@param	header IQM header containing filesize
*	@param	offset Byte offset to the section
*	@param	count Number of elements in the section
*	@param	size Size of each element in bytes
*	@return	true if the range is INVALID (out of bounds), false if valid
*	@note	Returns true on error to simplify validation code
**/
static bool IQM_CheckRange(const iqmHeader_t* header, uint32_t offset, uint32_t count, size_t size)
{
	// Check for empty array, offset overflow, or size overflow
	return (count == 0 ||
		offset > header->filesize ||
		offset + count * size > header->filesize);
}

/**
*	@brief	Multiply two 3x4 transformation matrices
*	@param	a Left matrix (row-major 3x4)
*	@param	b Right matrix (row-major 3x4)
*	@param	out Result matrix (row-major 3x4) - computes out = a * b
*	@note	Matrices are the top 3 rows of a 4x4 matrix with implicit last row [0 0 0 1]
*	@note	Used to concatenate joint transformations in the skeletal hierarchy
**/
static void Matrix34Multiply(const float* a, const float* b, float* out)
{
	// Row 0: compute rotation part then add translation
	out[0] = a[0] * b[0] + a[1] * b[4] + a[2] * b[8];
	out[1] = a[0] * b[1] + a[1] * b[5] + a[2] * b[9];
	out[2] = a[0] * b[2] + a[1] * b[6] + a[2] * b[10];
	out[3] = a[0] * b[3] + a[1] * b[7] + a[2] * b[11] + a[3];
	
	// Row 1: compute rotation part then add translation
	out[4] = a[4] * b[0] + a[5] * b[4] + a[6] * b[8];
	out[5] = a[4] * b[1] + a[5] * b[5] + a[6] * b[9];
	out[6] = a[4] * b[2] + a[5] * b[6] + a[6] * b[10];
	out[7] = a[4] * b[3] + a[5] * b[7] + a[6] * b[11] + a[7];
	
	// Row 2: compute rotation part then add translation
	out[8] = a[8] * b[0] + a[9] * b[4] + a[10] * b[8];
	out[9] = a[8] * b[1] + a[9] * b[5] + a[10] * b[9];
	out[10] = a[8] * b[2] + a[9] * b[6] + a[10] * b[10];
	out[11] = a[8] * b[3] + a[9] * b[7] + a[10] * b[11] + a[11];
}

/**
*	@brief	Convert a TRS (Translation-Rotation-Scale) joint transform to a 3x4 matrix
*	@param	rot Rotation quaternion [x, y, z, w]
*	@param	scale Non-uniform scale vector
*	@param	trans Translation vector
*	@param	mat Output 3x4 matrix in row-major order
*	@note	This converts from joint-local TRS to a transformation matrix
*	@note	The rotation is applied via quaternion-to-matrix conversion, then scaled
**/
static void JointToMatrix(const quat_t rot, const vec3_t scale, const vec3_t trans,	float* mat)
{
	// Precompute squared quaternion components (optimization)
	float xx = 2.0f * rot[0] * rot[0];
	float yy = 2.0f * rot[1] * rot[1];
	float zz = 2.0f * rot[2] * rot[2];
	
	// Precompute quaternion cross products
	float xy = 2.0f * rot[0] * rot[1];
	float xz = 2.0f * rot[0] * rot[2];
	float yz = 2.0f * rot[1] * rot[2];
	
	// Precompute quaternion-scalar products (w is scalar component)
	float wx = 2.0f * rot[3] * rot[0];
	float wy = 2.0f * rot[3] * rot[1];
	float wz = 2.0f * rot[3] * rot[2];

	// Row 0: X-axis of rotation matrix, scaled, plus translation
	mat[0] = scale[0] * (1.0f - (yy + zz));
	mat[1] = scale[0] * (xy - wz);
	mat[2] = scale[0] * (xz + wy);
	mat[3] = trans[0];
	
	// Row 1: Y-axis of rotation matrix, scaled, plus translation
	mat[4] = scale[1] * (xy + wz);
	mat[5] = scale[1] * (1.0f - (xx + zz));
	mat[6] = scale[1] * (yz - wx);
	mat[7] = trans[1];
	
	// Row 2: Z-axis of rotation matrix, scaled, plus translation
	mat[8] = scale[2] * (xz - wy);
	mat[9] = scale[2] * (yz + wx);
	mat[10] = scale[2] * (1.0f - (xx + yy));
	mat[11] = trans[2];
}

/**
*	@brief	Compute the inverse of a 3x4 transformation matrix
*	@param	inMat Input matrix in row-major order
*	@param	outMat Output inverse matrix in row-major order
*	@note	This inverts a matrix with rotation+scale+translation components
*	@note	The inverse is computed by transposing the 3x3 rotation part, normalizing
*			the basis vectors (to undo scale), and transforming the translation
*	@note	Used to compute inverse bind matrices for skinning
**/
static void Matrix34Invert(const float* inMat, float* outMat)
{
	// Transpose the 3x3 rotation part (upper-left)
	outMat[0] = inMat[0]; outMat[1] = inMat[4]; outMat[2] = inMat[8];
	outMat[4] = inMat[1]; outMat[5] = inMat[5]; outMat[6] = inMat[9];
	outMat[8] = inMat[2]; outMat[9] = inMat[6]; outMat[10] = inMat[10];
	
	// Normalize each basis vector to undo scale (inverse scale = 1/length²)
	float invSqrLen, *v;
	v = outMat + 0; invSqrLen = 1.0f / DotProduct(v, v); VectorScale(v, invSqrLen, v);  // X-axis
	v = outMat + 4; invSqrLen = 1.0f / DotProduct(v, v); VectorScale(v, invSqrLen, v);  // Y-axis
	v = outMat + 8; invSqrLen = 1.0f / DotProduct(v, v); VectorScale(v, invSqrLen, v);  // Z-axis

	// Extract translation from input matrix
	vec3_t trans;
	trans[0] = inMat[3];
	trans[1] = inMat[7];
	trans[2] = inMat[11];

	// Transform translation by the inverted rotation (T' = -R^T * T)
	outMat[3] = -DotProduct(outMat + 0, trans);
	outMat[7] = -DotProduct(outMat + 4, trans);
	outMat[11] = -DotProduct(outMat + 8, trans);
}

/**
*	@brief	Spherical linear interpolation between two quaternions
*	@param	from Starting quaternion
*	@param	_to Target quaternion
*	@param	fraction Interpolation parameter [0.0 = from, 1.0 = to]
*	@param	out Result quaternion
*	@note	Uses true slerp for large angles, linear interpolation for small angles
*	@note	Automatically chooses shortest path by negating quaternion if needed
*	@note	Critical for smooth skeletal animation between poses
**/
static void QuatSlerp(const quat_t from, const quat_t _to, float fraction, quat_t out)
{
	// Compute dot product = cos(angle) between quaternions
	float cosAngle = from[0] * _to[0] + from[1] * _to[1] + from[2] * _to[2] + from[3] * _to[3];

	// Quaternions have dual coverage: q and -q represent the same rotation
	// If dot product is negative, negate one quaternion to take the shorter path
	quat_t to;
	if (cosAngle < 0.0f)
	{
		cosAngle = -cosAngle;
		to[0] = -_to[0];
		to[1] = -_to[1];
		to[2] = -_to[2];
		to[3] = -_to[3];
	}
	else
	{
		QuatCopy(_to, to);
	}

	float backlerp, lerp;
	if (cosAngle < 0.999999f)
	{
		// Quaternions are far apart - use spherical linear interpolation
		// slerp(q1, q2, t) = (sin((1-t)θ)/sin(θ)) * q1 + (sin(tθ)/sin(θ)) * q2
		const float angle = acosf(cosAngle);
		const float sinAngle = sinf(angle);
		backlerp = sinf((1.0f - fraction) * angle) / sinAngle;
		lerp = sinf(fraction * angle) / sinAngle;
	}
	else
	{
		// Quaternions are very close - use linear interpolation to avoid division by zero
		backlerp = 1.0f - fraction;
		lerp = fraction;
	}

	// Compute interpolated quaternion
	out[0] = from[0] * backlerp + to[0] * lerp;
	out[1] = from[1] * backlerp + to[1] * lerp;
	out[2] = from[2] * backlerp + to[2] * lerp;
	out[3] = from[3] * backlerp + to[3] * lerp;
}

/**
*	@brief	Normalize a quaternion and return its original length
*	@param	v Input quaternion
*	@param	out Normalized quaternion
*	@return	Original length of the quaternion
*	@note	If length is zero, returns identity quaternion [0, 0, 0, -1]
*	@note	Uses fast inverse square root optimization hint for compiler
**/
static vec_t QuatNormalize2(const quat_t v, quat_t out)
{
	// Compute squared length
	float length = v[0] * v[0] + v[1] * v[1] + v[2] * v[2] + v[3] * v[3];

	if (length > 0.f)
	{
		// Compute 1/sqrt(length) - written to allow compiler to use rsqrt instruction
		float ilength = 1 / sqrtf(length);
		
		// Convert squared length to actual length: sqrt(length) = length * (1/sqrt(length))
		length *= ilength;
		
		// Normalize quaternion components
		out[0] = v[0] * ilength;
		out[1] = v[1] * ilength;
		out[2] = v[2] * ilength;
		out[3] = v[3] * ilength;
	}
	else
	{
		// Degenerate quaternion - return identity rotation [0, 0, 0, -1]
		out[0] = out[1] = out[2] = 0;
		out[3] = -1;
	}

	return length;
}



/********************************************************************
*
*	IQM Format Loading and Validation
*
*	This section handles loading IQM files from disk, validating the format,
*	and converting the data into runtime structures.
*
*	IQM File Format Overview:
*	- Magic number: "INTERQUAKEMODEL\0" (16 bytes)
*	- Version: 2 (uint32)
*	- Filesize: Total file size in bytes
*	- Multiple data sections with offset/count pairs:
*	  * Text strings (joint names, animation names, material names)
*	  * Vertex arrays (position, normal, texcoord, tangent, blend data, color)
*	  * Triangle indices (uint32[3] per triangle)
*	  * Meshes (submesh metadata)
*	  * Joints (bind pose hierarchy)
*	  * Poses (animation channel metadata)
*	  * Frames (16-bit compressed animation data)
*	  * Animations (animation sequences)
*	  * Bounds (per-frame bounding boxes)
*
*	Vertex Array Types:
*	- IQM_POSITION: float[3] - vertex position
*	- IQM_NORMAL: float[3] - vertex normal
*	- IQM_TEXCOORD: float[2] - UV coordinates
*	- IQM_TANGENT: float[4] - tangent + handedness
*	- IQM_BLENDINDEXES: ubyte[4] or int[4] - joint indices
*	- IQM_BLENDWEIGHTS: ubyte[4] or float[4] - blend weights
*	- IQM_COLOR: ubyte[4] - vertex color
*
*	Pose Channel Layout (10 channels per joint):
*	- Channels 0-2: Translation X, Y, Z
*	- Channels 3-6: Rotation quaternion X, Y, Z, W
*	- Channels 7-9: Scale X, Y, Z
*
*	Each channel has:
*	- offset: base value
*	- scale: multiplier for 16-bit compressed frame data
*	- mask bit: whether this channel is animated
*
********************************************************************/

/**
*	@brief	Load an IQM skeletal model file and prepare it for rendering
*	@param	model Model structure to fill
*	@param	rawdata Raw IQM file data
*	@param	length Size of the file data in bytes
*	@param	mod_name Model name for error reporting
*	@return	Q_ERR_SUCCESS on success, error code on failure
*
*	This function performs extensive validation of the IQM file format,
*	allocates memory for all model data, and converts the file data into
*	runtime structures. It handles:
*	- Format validation (magic, version, ranges)
*	- Vertex array validation and conversion
*	- Blend index/weight normalization (int→byte, float→byte[0-255])
*	- Joint hierarchy construction
*	- Bind pose matrix computation
*	- Inverse bind matrix computation for skinning
*	- Animation pose decompression from 16-bit frames
*	- Bounding box extraction or computation
*
*	@note	Blend weights are normalized to 0-255 byte range
*	@note	Bind pose matrices are in model space (not local joint space)
*	@note	Inverse bind matrices transform from model space to joint space
**/
int MOD_LoadIQM_Base(model_t* model, const void* rawdata, size_t length, const char* mod_name)
{
	iqm_transform_t* transform;
	float* mat, * matInv;
	size_t joint_names;
	iqm_model_t* iqmData;
	char meshName[MAX_QPATH];
	int vertexArrayFormat[IQM_COLOR + 1];
	int ret;

	// Validate minimum file size
	if (length < sizeof(iqmHeader_t))
	{
		return Q_ERR_FILE_TOO_SMALL;
	}

	// Cast to IQM header structure
	const iqmHeader_t* header = rawdata;
	
	// Verify IQM magic number "INTERQUAKEMODEL\0"
	if (strncmp(header->magic, IQM_MAGIC, sizeof(header->magic)) != 0)
	{
		return Q_ERR_INVALID_FORMAT;
	}

	// Verify IQM version (only version 2 supported)
	if (header->version != IQM_VERSION)
	{
		Com_WPrintf("R_LoadIQM: %s is a unsupported IQM version (%d), only version %d is supported.\n",
			mod_name, header->version, IQM_VERSION);
		return Q_ERR_UNKNOWN_FORMAT;
	}

	// Validate filesize field and enforce 16MB limit
	if (header->filesize > length || header->filesize > 16 << 20)
	{
		return Q_ERR_FILE_TOO_SMALL;
	}

	// Enforce engine joint limit (inherited from ioq3)
	if (header->num_joints > IQM_MAX_JOINTS)
	{
		Com_WPrintf("R_LoadIQM: %s has more than %d joints (%d).\n",
			mod_name, IQM_MAX_JOINTS, header->num_joints);
		return Q_ERR_INVALID_FORMAT;
	}

	// Initialize vertex array format tracker (tracks data type for each array)
	for (uint32_t vertexarray_idx = 0; vertexarray_idx < q_countof(vertexArrayFormat); vertexarray_idx++)
	{
		vertexArrayFormat[vertexarray_idx] = -1;
	}

	// Validate vertex arrays if model has meshes
	if (header->num_meshes)
	{
		// Validate vertex array section bounds
		if (IQM_CheckRange(header, header->ofs_vertexarrays, header->num_vertexarrays, sizeof(iqmVertexArray_t)))
		{
			return Q_ERR_BAD_EXTENT;
		}
		
		const iqmVertexArray_t* vertexarray = (const iqmVertexArray_t*)((const byte*)header + header->ofs_vertexarrays);
		for (uint32_t vertexarray_idx = 0; vertexarray_idx < header->num_vertexarrays; vertexarray_idx++, vertexarray++)
		{
			// Validate component count (e.g., 3 for position, 2 for texcoord)
			if (vertexarray->size <= 0 || vertexarray->size > 4)
			{
				return Q_ERR_INVALID_FORMAT;
			}
			
			// Calculate total number of values in this array
			uint32_t num_values = header->num_vertexes * vertexarray->size;

			// Validate data section bounds based on format
			switch (vertexarray->format) {
			case IQM_BYTE:
			case IQM_UBYTE:
				// 1-byte per component (e.g., blend indices)
				if (IQM_CheckRange(header, vertexarray->offset, num_values, sizeof(byte)))
				{
					return Q_ERR_BAD_EXTENT;
				}
				break;
			case IQM_INT:
			case IQM_UINT:
			case IQM_FLOAT:
				// 4-bytes per component (e.g., positions, normals)
				if (IQM_CheckRange(header, vertexarray->offset, num_values, sizeof(float)))
				{
					return Q_ERR_BAD_EXTENT;
				}
				break;
			default:
				// Format not supported
				return Q_ERR_INVALID_FORMAT;
			}

			// Record the format for this vertex array type
			if (vertexarray->type < q_countof(vertexArrayFormat))
			{
				vertexArrayFormat[vertexarray->type] = (int)vertexarray->format;
			}

			// Validate format and size for each vertex array type
			switch (vertexarray->type)
			{
			case IQM_POSITION:
			case IQM_NORMAL:
				// Must be 3 floats (X, Y, Z)
				if (vertexarray->format != IQM_FLOAT ||
					vertexarray->size != 3)
				{
					return Q_ERR_INVALID_FORMAT;
				}
				break;
			case IQM_TANGENT:
				// Must be 4 floats (tangent X, Y, Z + handedness)
				if (vertexarray->format != IQM_FLOAT ||
					vertexarray->size != 4)
				{
					return Q_ERR_INVALID_FORMAT;
				}
				break;
			case IQM_TEXCOORD:
				// Must be 2 floats (U, V)
				if (vertexarray->format != IQM_FLOAT ||
					vertexarray->size != 2)
				{
					return Q_ERR_INVALID_FORMAT;
				}
				break;
			case IQM_BLENDINDEXES:
				// Can be 4 ints or 4 ubytes (joint indices 0-255)
				if ((vertexarray->format != IQM_INT &&
					vertexarray->format != IQM_UBYTE) ||
					vertexarray->size != 4)
				{
					return Q_ERR_INVALID_FORMAT;
				}
				break;
			case IQM_BLENDWEIGHTS:
				// Can be 4 floats or 4 ubytes (weights 0.0-1.0 or 0-255)
				if ((vertexarray->format != IQM_FLOAT &&
					vertexarray->format != IQM_UBYTE) ||
					vertexarray->size != 4)
				{
					return Q_ERR_INVALID_FORMAT;
				}
				break;
			case IQM_COLOR:
				// Must be 4 ubytes (RGBA)
				if (vertexarray->format != IQM_UBYTE ||
					vertexarray->size != 4)
				{
					return Q_ERR_INVALID_FORMAT;
				}
				break;
			default:
				break;
			}
		}

		// Verify required vertex arrays are present
		if (vertexArrayFormat[IQM_POSITION] == -1 || vertexArrayFormat[IQM_NORMAL] == -1 || vertexArrayFormat[IQM_TEXCOORD] == -1)
		{
			Com_WPrintf("R_LoadIQM: %s is missing IQM_POSITION, IQM_NORMAL, and/or IQM_TEXCOORD array.\n", mod_name);
			return Q_ERR_INVALID_FORMAT;
		}

		// If model has joints, blend data is required for skinning
		if (header->num_joints)
		{
			if (vertexArrayFormat[IQM_BLENDINDEXES] == -1 || vertexArrayFormat[IQM_BLENDWEIGHTS] == -1)
			{
				Com_WPrintf("R_LoadIQM: %s is missing IQM_BLENDINDEXES and/or IQM_BLENDWEIGHTS array.\n", mod_name);
				return Q_ERR_INVALID_FORMAT;
			}
		}
		else
		{
			// Static model (no skeleton) - ignore blend arrays even if present
			vertexArrayFormat[IQM_BLENDINDEXES] = -1;
			vertexArrayFormat[IQM_BLENDWEIGHTS] = -1;
		}

		// Validate triangle section
		if (IQM_CheckRange(header, header->ofs_triangles, header->num_triangles, sizeof(iqmTriangle_t)))
		{
			return Q_ERR_BAD_EXTENT;
		}
		
		// Verify all triangle indices reference valid vertices
		const iqmTriangle_t* triangle = (const iqmTriangle_t*)((const byte*)header + header->ofs_triangles);
		for (uint32_t triangle_idx = 0; triangle_idx < header->num_triangles; triangle_idx++, triangle++)
		{
			if (triangle->vertex[0] > header->num_vertexes ||
				triangle->vertex[1] > header->num_vertexes ||
				triangle->vertex[2] > header->num_vertexes) {
				return Q_ERR_INVALID_FORMAT;
			}
		}

		// Validate mesh section (submeshes with material assignments)
		if (IQM_CheckRange(header, header->ofs_meshes, header->num_meshes, sizeof(iqmMesh_t)))
		{
			return Q_ERR_BAD_EXTENT;
		}
		
		const iqmMesh_t* mesh = (const iqmMesh_t*)((const byte*)header + header->ofs_meshes);
		for (uint32_t mesh_idx = 0; mesh_idx < header->num_meshes; mesh_idx++, mesh++)
		{
			// Extract mesh name for error reporting
			if (mesh->name < header->num_text)
			{
				strncpy(meshName, (const char*)header + header->ofs_text + mesh->name, sizeof(meshName) - 1);
			}
			else
			{
				meshName[0] = '\0';
			}

			// Validate mesh ranges and text offsets
			if (mesh->first_vertex >= header->num_vertexes ||
				mesh->first_vertex + mesh->num_vertexes > header->num_vertexes ||
				mesh->first_triangle >= header->num_triangles ||
				mesh->first_triangle + mesh->num_triangles > header->num_triangles ||
				mesh->name >= header->num_text ||
				mesh->material >= header->num_text) {
				return Q_ERR_INVALID_FORMAT;
			}
		}
	}

	// Validate pose-to-joint relationship (must be 1:1 or no poses for static models)
	if (header->num_poses != header->num_joints && header->num_poses != 0)
	{
		Com_WPrintf("R_LoadIQM: %s has %d poses and %d joints, must have the same number or 0 poses\n",
			mod_name, header->num_poses, header->num_joints);
		return Q_ERR_INVALID_FORMAT;
	}

	joint_names = 0;

	if (header->num_joints)
	{
		// Validate joint section
		if (IQM_CheckRange(header, header->ofs_joints, header->num_joints, sizeof(iqmJoint_t)))
		{
			return Q_ERR_BAD_EXTENT;
		}
		
		// Verify joint hierarchy is valid (no cycles, valid parent indices)
		const iqmJoint_t* joint = (const iqmJoint_t*)((const byte*)header + header->ofs_joints);
		for (uint32_t joint_idx = 0; joint_idx < header->num_joints; joint_idx++, joint++)
		{
			// Parent must be -1 (root) or valid joint index
			if (joint->parent < -1 ||
				joint->parent >= (int)header->num_joints ||
				joint->name >= header->num_text) {
				return Q_ERR_INVALID_FORMAT;
			}
			
			// Calculate total space needed for joint names
			joint_names += strlen((const char*)header + header->ofs_text +
				joint->name) + 1;
		}
	}

	if (header->num_poses)
	{
		// Validate pose section (animation channel metadata)
		if (IQM_CheckRange(header, header->ofs_poses, header->num_poses, sizeof(iqmPose_t)))
		{
			return Q_ERR_BAD_EXTENT;
		}
	}

	if (header->ofs_bounds)
	{
		// Validate bounding box section (one per frame)
		if (IQM_CheckRange(header, header->ofs_bounds, header->num_frames, sizeof(iqmBounds_t)))
		{
			return Q_ERR_BAD_EXTENT;
		}
	}

	if (header->num_anims)
	{
		// Validate animation section (named animation sequences)
		const iqmAnim_t* anim = (const iqmAnim_t*)((const byte*)header + header->ofs_anims);
		for (uint32_t anim_idx = 0; anim_idx < header->num_anims; anim_idx++, anim++)
		{
			// Verify animation frame range is valid
			if (anim->first_frame + anim->num_frames > header->num_frames)
			{
				return Q_ERR_INVALID_FORMAT;
			}
		}
	}

	// Allocate runtime model structure
	CHECK(iqmData = MOD_Malloc(sizeof(iqm_model_t)));
	model->skmData = iqmData;

	// Copy header counts to runtime structure
	iqmData->num_vertexes = (header->num_meshes > 0) ? header->num_vertexes : 0;
	iqmData->num_triangles = (header->num_meshes > 0) ? header->num_triangles : 0;
	iqmData->num_frames = header->num_frames;
	iqmData->num_meshes = header->num_meshes;
	iqmData->num_joints = header->num_joints;
	iqmData->num_poses = header->num_poses;

	// Allocate mesh and vertex data
	if (header->num_meshes)
	{
		CHECK(iqmData->meshes = MOD_Malloc(header->num_meshes * sizeof(iqm_mesh_t)));
		CHECK(iqmData->indices = MOD_Malloc(header->num_triangles * 3 * sizeof(int)));
		CHECK(iqmData->positions = MOD_Malloc(header->num_vertexes * 3 * sizeof(float)));
		CHECK(iqmData->texcoords = MOD_Malloc(header->num_vertexes * 2 * sizeof(float)));
		CHECK(iqmData->normals = MOD_Malloc(header->num_vertexes * 3 * sizeof(float)));

		// Optional: tangent space for normal mapping
		if (vertexArrayFormat[IQM_TANGENT] != -1)
		{
			CHECK(iqmData->tangents = MOD_Malloc(header->num_vertexes * 4 * sizeof(float)));
		}

		// Optional: vertex colors
		if (vertexArrayFormat[IQM_COLOR] != -1)
		{
			CHECK(iqmData->colors = MOD_Malloc(header->num_vertexes * 4 * sizeof(byte)));
		}

		// Skeletal skinning data: up to 4 bones per vertex
		if (vertexArrayFormat[IQM_BLENDINDEXES] != -1)
		{
			CHECK(iqmData->blend_indices = MOD_Malloc(header->num_vertexes * 4 * sizeof(byte)));
		}

		if (vertexArrayFormat[IQM_BLENDWEIGHTS] != -1)
		{
			CHECK(iqmData->blend_weights = MOD_Malloc(header->num_vertexes * 4 * sizeof(byte)));
		}
	}

	// Allocate skeletal data
	if (header->num_joints)
	{
		CHECK(iqmData->jointNames = MOD_Malloc(joint_names));
		CHECK(iqmData->jointParents = MOD_Malloc(header->num_joints * sizeof(int)));
		CHECK(iqmData->bindJoints = MOD_Malloc(header->num_joints * 12 * sizeof(float)));     // Bind pose matrices (model space)
		CHECK(iqmData->invBindJoints = MOD_Malloc(header->num_joints * 12 * sizeof(float)));  // Inverse bind matrices (for skinning)
	}
	
	// Allocate animation data
	if (header->num_poses)
	{
		CHECK(iqmData->poses = MOD_Malloc(header->num_poses * header->num_frames * sizeof(iqm_transform_t)));  // TRS per joint per frame
	}
	
	// Allocate bounding box data
	if (header->ofs_bounds)
	{
		CHECK(iqmData->bounds = MOD_Malloc(header->num_frames * 6 * sizeof(float)));  // Per-frame bounds (bbmin, bbmax)
	}
	else if (header->num_meshes && header->num_frames == 0)
	{
		CHECK(iqmData->bounds = MOD_Malloc(6 * sizeof(float)));  // Static model bounds
	}
	
	// Copy mesh metadata (submesh ranges, material names)
	if (header->num_meshes)
	{
		const iqmMesh_t* mesh = (const iqmMesh_t*)((const byte*)header + header->ofs_meshes);
		iqm_mesh_t* surface = iqmData->meshes;
		const char* str = (const char*)header + header->ofs_text;
		for (uint32_t mesh_idx = 0; mesh_idx < header->num_meshes; mesh_idx++, mesh++, surface++)
		{
			strncpy(surface->name, str + mesh->name, sizeof(surface->name) - 1);
			Q_strlwr(surface->name); // Lowercase for faster skin lookup
			strncpy(surface->material, str + mesh->material, sizeof(surface->material) - 1);
			Q_strlwr(surface->material);
			surface->data = iqmData;
			surface->first_vertex = mesh->first_vertex;
			surface->num_vertexes = mesh->num_vertexes;
			surface->first_triangle = mesh->first_triangle;
			surface->num_triangles = mesh->num_triangles;
		}

		// Copy triangle indices (3 indices per triangle)
		const iqmTriangle_t* triangle = (const iqmTriangle_t*)((const byte*)header + header->ofs_triangles);
		for (uint32_t i = 0; i < header->num_triangles; i++, triangle++)
		{
			iqmData->indices[3 * i + 0] = triangle->vertex[0];
			iqmData->indices[3 * i + 1] = triangle->vertex[1];
			iqmData->indices[3 * i + 2] = triangle->vertex[2];
		}

		// Copy vertex data arrays
		const iqmVertexArray_t* vertexarray = (const iqmVertexArray_t*)((const byte*)header + header->ofs_vertexarrays);
		for (uint32_t vertexarray_idx = 0; vertexarray_idx < header->num_vertexarrays; vertexarray_idx++, vertexarray++)
		{
			// Skip arrays that were disabled during validation
			if (vertexarray->type < q_countof(vertexArrayFormat)
				&& vertexArrayFormat[vertexarray->type] == -1)
				continue;

			// Total number of components in this array
			uint32_t n = header->num_vertexes * vertexarray->size;

			switch (vertexarray->type)
			{
			case IQM_POSITION:
				memcpy(iqmData->positions,
					(const byte*)header + vertexarray->offset,
					n * sizeof(float));
				break;
			case IQM_NORMAL:
				memcpy(iqmData->normals,
					(const byte*)header + vertexarray->offset,
					n * sizeof(float));
				break;
			case IQM_TANGENT:
				memcpy(iqmData->tangents,
					(const byte*)header + vertexarray->offset,
					n * sizeof(float));
				break;
			case IQM_TEXCOORD:
				memcpy(iqmData->texcoords,
					(const byte*)header + vertexarray->offset,
					n * sizeof(float));
				break;
			case IQM_BLENDINDEXES:
				// Joint indices for skinning (up to 4 bones per vertex)
				if (vertexArrayFormat[IQM_BLENDINDEXES] == IQM_UBYTE)
				{
					// Already in byte format - direct copy
					memcpy(iqmData->blend_indices,
						(const byte*)header + vertexarray->offset,
						n * sizeof(byte));
				}
				else if (vertexArrayFormat[IQM_BLENDINDEXES] == IQM_INT)
				{
					const int* indices = (const int*)((const byte*)header + vertexarray->offset);

					// Convert from int[4] to byte[4] per vertex
					for (uint32_t index_idx = 0; index_idx < n; index_idx++)
					{
						int index = indices[index_idx];
						iqmData->blend_indices[index_idx] = (byte)index;
					}
				}
				else
				{
					// Should never happen due to validation
					assert(!"Unsupported IQM_BLENDINDEXES format");
					memset(iqmData->blend_indices, 0, n * sizeof(byte));
				}
				break;
			case IQM_BLENDWEIGHTS:
				// Blend weights for skinning (normalized 0-255 range)
				if (vertexArrayFormat[IQM_BLENDWEIGHTS] == IQM_UBYTE)
				{
					// Already in byte format - direct copy
					memcpy(iqmData->blend_weights,
						(const byte*)header + vertexarray->offset,
						n * sizeof(byte));
				}
				else if(vertexArrayFormat[IQM_BLENDWEIGHTS] == IQM_FLOAT)
				{
					const float* weights = (const float*)((const byte*)header + vertexarray->offset);

					// Convert from float[0.0-1.0] to byte[0-255]
					for (uint32_t weight_idx = 0; weight_idx < n; weight_idx++)
					{
						float integer_weight = weights[weight_idx] * 255.f;
						clamp(integer_weight, 0.f, 255.f);
						iqmData->blend_weights[weight_idx] = (byte)integer_weight;
					}
				}
				else
				{
					// Should never happen due to validation
					assert(!"Unsupported IQM_BLENDWEIGHTS format");
					memset(iqmData->blend_weights, 0, n * sizeof(byte));
				}
				break;
			case IQM_COLOR:
				memcpy(iqmData->colors,
					(const byte*)header + vertexarray->offset,
					n * sizeof(byte));
				break;
			default:
				break;
			}
		}
	}

	// Build skeletal hierarchy
	if (header->num_joints)
	{
		// Copy joint names into contiguous buffer
		char* str = iqmData->jointNames;
		const iqmJoint_t* joint = (const iqmJoint_t*)((const byte*)header + header->ofs_joints);
		for (uint32_t joint_idx = 0; joint_idx < header->num_joints; joint_idx++, joint++)
		{
			const char* name = (const char*)header + header->ofs_text + joint->name;
			size_t len = strlen(name) + 1;
			memcpy(str, name, len);
			str += len;
		}

		// Copy joint parent indices (defines hierarchy)
		joint = (const iqmJoint_t*)((const byte*)header + header->ofs_joints);
		for (uint32_t joint_idx = 0; joint_idx < header->num_joints; joint_idx++, joint++)
		{
			iqmData->jointParents[joint_idx] = joint->parent;
		}

		// Calculate bind pose matrices (model space) and their inverses (for skinning)
		mat = iqmData->bindJoints;
		matInv = iqmData->invBindJoints;
		joint = (const iqmJoint_t*)((const byte*)header + header->ofs_joints);
		for (uint32_t joint_idx = 0; joint_idx < header->num_joints; joint_idx++, joint++)
		{
			float baseFrame[12], invBaseFrame[12];

			// Normalize bind pose quaternion
			quat_t rotate;
			QuatNormalize2(joint->rotate, rotate);

			// Convert bind pose TRS to matrix (local joint space)
			JointToMatrix(rotate, joint->scale, joint->translate, baseFrame);
			Matrix34Invert(baseFrame, invBaseFrame);

			if (joint->parent >= 0)
			{
				// Concatenate with parent transform to get model-space bind pose
				Matrix34Multiply(iqmData->bindJoints + 12 * joint->parent, baseFrame, mat);
				mat += 12;
				
				// Compute inverse bind matrix: invBind[child] = invLocal[child] * invBind[parent]
				Matrix34Multiply(invBaseFrame, iqmData->invBindJoints + 12 * joint->parent, matInv);
				matInv += 12;
			}
			else
			{
				// Root joint has no parent - bind pose = local pose
				memcpy(mat, baseFrame, sizeof(baseFrame));
				mat += 12;
				memcpy(matInv, invBaseFrame, sizeof(invBaseFrame));
				matInv += 12;
			}
		}
	}

	// Decompress animation frames
	if (header->num_poses)
	{
		// IQM stores animation data as compressed 16-bit values with offset/scale per channel
		transform = iqmData->poses;
		const uint16_t* framedata = (const uint16_t*)((const byte*)header + header->ofs_frames);
		
		for (uint32_t frame_idx = 0; frame_idx < header->num_frames; frame_idx++)
		{
			const iqmPose_t* pose = (const iqmPose_t*)((const byte*)header + header->ofs_poses);
			for (uint32_t pose_idx = 0; pose_idx < header->num_poses; pose_idx++, pose++, transform++)
			{
				vec3_t translate;
				quat_t rotate;
				vec3_t scale;

				// Decompress translation channels (X, Y, Z)
				// Each channel: value = offset + framedata * scale (if animated per mask bit)
				translate[0] = pose->channeloffset[0]; if (pose->mask & 0x001) translate[0] += (float)*framedata++ * pose->channelscale[0];
				translate[1] = pose->channeloffset[1]; if (pose->mask & 0x002) translate[1] += (float)*framedata++ * pose->channelscale[1];
				translate[2] = pose->channeloffset[2]; if (pose->mask & 0x004) translate[2] += (float)*framedata++ * pose->channelscale[2];

				// Decompress rotation quaternion (X, Y, Z, W)
				rotate[0] = pose->channeloffset[3]; if (pose->mask & 0x008) rotate[0] += (float)*framedata++ * pose->channelscale[3];
				rotate[1] = pose->channeloffset[4]; if (pose->mask & 0x010) rotate[1] += (float)*framedata++ * pose->channelscale[4];
				rotate[2] = pose->channeloffset[5]; if (pose->mask & 0x020) rotate[2] += (float)*framedata++ * pose->channelscale[5];
				rotate[3] = pose->channeloffset[6]; if (pose->mask & 0x040) rotate[3] += (float)*framedata++ * pose->channelscale[6];

				// Decompress scale channels (X, Y, Z)
				scale[0] = pose->channeloffset[7]; if (pose->mask & 0x080) scale[0] += (float)*framedata++ * pose->channelscale[7];
				scale[1] = pose->channeloffset[8]; if (pose->mask & 0x100) scale[1] += (float)*framedata++ * pose->channelscale[8];
				scale[2] = pose->channeloffset[9]; if (pose->mask & 0x200) scale[2] += (float)*framedata++ * pose->channelscale[9];

				// Store decompressed TRS transform
				VectorCopy(translate, transform->translate);
				QuatNormalize2(rotate, transform->rotate);  // Renormalize quaternion after decompression
				VectorCopy(scale, transform->scale);
			}
		}
	}

	// Extract or compute bounding boxes
	if (header->ofs_bounds)
	{
		// Per-frame bounding boxes provided by the file
		mat = iqmData->bounds;
		const iqmBounds_t* bounds = (const iqmBounds_t*)((const byte*)header + header->ofs_bounds);
		for (uint32_t frame_idx = 0; frame_idx < header->num_frames; frame_idx++)
		{
			mat[0] = bounds->bbmin[0];
			mat[1] = bounds->bbmin[1];
			mat[2] = bounds->bbmin[2];
			mat[3] = bounds->bbmax[0];
			mat[4] = bounds->bbmax[1];
			mat[5] = bounds->bbmax[2];

			mat += 6;
			bounds++;
		}
	}
	else if (header->num_meshes && header->num_frames == 0)
	{
		// Static model - compute bounding box from vertex positions
		mat = iqmData->bounds;

		ClearBounds(&iqmData->bounds[0], &iqmData->bounds[3]);
		for (uint32_t vertex_idx = 0; vertex_idx < header->num_vertexes; vertex_idx++)
		{
			AddPointToBounds(&iqmData->positions[vertex_idx * 3], &iqmData->bounds[0], &iqmData->bounds[3]);
		}
	}

	// Copy animation metadata (named sequences)
	if (header->num_anims)
	{
		iqmData->num_animations = header->num_anims;
		CHECK(iqmData->animations = MOD_Malloc(header->num_anims * sizeof(iqm_anim_t)));

		const iqmAnim_t* src = (const iqmAnim_t*)((const byte*)header + header->ofs_anims);
		iqm_anim_t* dst = iqmData->animations;
		for (uint32_t anim_idx = 0; anim_idx < header->num_anims; anim_idx++, src++, dst++)
		{
			// Copy animation name from string table
			const char* name = (const char*)header + header->ofs_text + src->name;
			strncpy(dst->name, name, sizeof(dst->name));
			dst->name[sizeof(dst->name) - 1] = 0;
			
			// Copy animation parameters
			dst->first_frame = src->first_frame;  // Frame range in pose array
			dst->num_frames = src->num_frames;
			dst->framerate = src->framerate;      // Playback rate (e.g., 30 FPS)
			dst->flags = src->flags;              // IQM_LOOP flag, etc.
		}
	}

	return Q_ERR_SUCCESS;

fail:
	return ret;
}



/********************************************************************
*
*	Skeletal Animation Pose Computation
*
*	This section handles runtime computation of skeletal poses for rendering.
*	The pose computation pipeline:
*
*	1. Source Data Selection:
*	   - Option A: Use entity->bonePoses (direct bone control from game code)
*	   - Option B: Use entity->frame/oldframe (traditional frame-based animation)
*
*	2. Interpolation:
*	   - Lerp between two poses using entity->backlerp
*	   - Supports quaternion slerp for smooth rotation
*	   - Optional root motion extraction (removes translation from specified bone)
*
*	3. Transform to Model Space:
*	   - Convert from joint-local TRS to model-space 3x4 matrices
*	   - Traverse hierarchy from root to leaves
*	   - Each joint inherits parent's transform
*
*	4. Bone Controllers (Optional):
*	   - Apply procedural modifications (e.g., aim offsets, IK)
*	   - Applied after base pose computation
*
*	Output: Array of 3x4 matrices (one per joint) in model space, ready for
*	        GPU skinning via vertex shader.
*
********************************************************************/

/**
*	@brief	Compute final skeletal pose matrices for rendering
*	@param	model The IQM model to compute poses for
*	@param	entity Render entity containing animation state
*	@param	pose_matrices Output buffer for 3x4 matrices [model->num_joints * 12 floats]
*	@return	true on success, false if model is invalid
*
*	This function computes the final skeletal pose for rendering by:
*	- Interpolating between animation frames (or bone poses)
*	- Applying root motion extraction if requested
*	- Converting from joint-local TRS to model-space matrices
*	- Applying bone controllers for procedural animation
*
*	The function supports two animation modes:
*	1. Bone Pose Mode: entity->bonePoses is set (direct bone control)
*	   - Used for complex animation systems, IK, etc.
*	   - Optionally lerps with entity->lastBonePoses
*	2. Frame Mode: traditional frame/oldframe lerping (like MD2/MD3)
*	   - Used for simple keyframe animation
*	   - Automatically extracts poses from frame data
*
*	@note	Output matrices are in model space (not joint-local space)
*	@note	Root motion extraction can remove translation from specified bone
*	@note	Bone controllers are applied after base pose computation
**/
bool R_ComputePoseTransforms( const model_t *model, const entity_t *entity, float *pose_matrices ) {
	// Validate model has skeletal data
	if ( !model || !model->skmData ) {
		return false;
	}

	// Temporary buffer for interpolated bone poses (TRS format)
	static skm_transform_t lerpedBonePoses[ IQM_MAX_JOINTS ] = { 0 };

	// Check if entity provides custom bone poses
	if ( entity->bonePoses ) {	
		// If lastBonePoses is provided, interpolate between current and last
		if ( entity->lastBonePoses ) {
			// Lerp between two bone pose sets
			SKM_LerpBonePoses( model, entity->bonePoses, entity->lastBonePoses, 
				1.0 - entity->backlerp, entity->backlerp, 
				lerpedBonePoses, 
				entity->rootMotionBoneID, entity->rootMotionFlags );
			
			// Convert lerped TRS poses to model-space matrices
			SKM_TransformBonePosesLocalSpace( model->skmData, lerpedBonePoses, entity->boneControllers, pose_matrices );
		} else {	
			// Bone poses are pre-lerped - convert directly to matrices
			SKM_TransformBonePosesLocalSpace( model->skmData, entity->bonePoses, entity->boneControllers, pose_matrices );
		}
	} else {
		// Traditional frame-based animation (like MD2/MD3)
		// Derive bone poses by lerping between animation frames
		SKM_ComputeLerpBonePoses( model, entity->frame, entity->oldframe, 
			1.0 - entity->backlerp, entity->backlerp,
			lerpedBonePoses,
			entity->rootMotionBoneID, entity->rootMotionFlags
		);
		
		// Convert lerped TRS poses to model-space matrices
		SKM_TransformBonePosesLocalSpace( model->skmData, lerpedBonePoses, entity->boneControllers, pose_matrices );
	}

	return true;
}