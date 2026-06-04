/********************************************************************
*
*
*	SharedGame: Decal Shared Types.
*
*
********************************************************************/
#pragma once

#include "shared/shared.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
*	@brief	Classifies receiver surface type for decal spawn intent.
**/
typedef enum sg_decal_surface_class_e {
	SG_DECAL_SURFACE_DEFAULT = 0,
	SG_DECAL_SURFACE_CONCRETE,
	SG_DECAL_SURFACE_METAL,
	SG_DECAL_SURFACE_FLESH,
	SG_DECAL_SURFACE_WOOD,
	SG_DECAL_SURFACE_GLASS,
	SG_DECAL_SURFACE_MAX
} sg_decal_surface_class_t;

/**
*	@brief	Selects active decal renderer mode.
**/
typedef enum sg_decal_render_mode_e {
	SG_DECAL_RENDER_DISABLED = 0,
	SG_DECAL_RENDER_SCREENSPACE = 1,
	SG_DECAL_RENDER_PATH_TRACED = 2
} sg_decal_render_mode_t;

/**
*	@brief	Spawn flags controlling decal lifetime policy.
**/
typedef enum sg_decal_flags_e {
	SG_DECAL_FLAG_NONE = 0u,
	SG_DECAL_FLAG_DYNAMIC = 1u << 0,
	SG_DECAL_FLAG_STATIC = 1u << 1
} sg_decal_flags_t;

/**
*	@brief	Stable decal material IDs shared between CLGame and renderer.
*	@note	Each ID maps to one explicit texture path defined below.
**/
typedef enum sg_decal_material_hash_e {
	SG_DECAL_MATERIAL_HASH_DEFAULT = 0xD3CA1001u,
	SG_DECAL_MATERIAL_HASH_GUNSHOT_CONCRETE = 0x9F2A0D3Bu,
	SG_DECAL_MATERIAL_HASH_SPARKS_METAL = 0x61D4C8A5u,
	SG_DECAL_MATERIAL_HASH_BLOOD_FLESH = 0xA17C5E2Du,
	SG_DECAL_MATERIAL_HASH_SPLINTER_WOOD = 0xF4B0E29Cu,
	SG_DECAL_MATERIAL_HASH_CRACK_GLASS = 0x3ED19A77u
} sg_decal_material_hash_t;

//! Default decal material path (resolved through materials/*.mat registration).
#define SG_DECAL_MATERIAL_NAME_DEFAULT "textures/decals/bullets/bullethole_01"
//! Concrete impact decal material path.
#define SG_DECAL_MATERIAL_PATH_GUNSHOT_CONCRETE "textures/decals/bullets/bullethole_01"
//! Metal impact decal material path.
#define SG_DECAL_MATERIAL_PATH_SPARKS_METAL "textures/decals/bullets/bullethole_01"
//! Flesh impact decal material path.
#define SG_DECAL_MATERIAL_PATH_BLOOD_FLESH "textures/decals/bullets/bullethole_01"
//! Wood impact decal material path.
#define SG_DECAL_MATERIAL_PATH_SPLINTER_WOOD "textures/decals/bullets/bullethole_01"
//! Glass impact decal material path.
#define SG_DECAL_MATERIAL_PATH_CRACK_GLASS "textures/decals/bullets/bullethole_01"

/**
*	@brief	Decal spawn request payload shared between clgame and renderer.
**/
typedef struct sg_decal_spawn_params_s {
	vec3_t origin;
	vec3_t normal;
	sg_decal_material_hash_t materialHash;
	float radius;
	float depth;
	float rotationRadians;
	float lifeSeconds;
	float fadeInSeconds;
	float fadeOutSeconds;
	sg_decal_surface_class_t surfaceClass;
	int32_t hitEntityNumber;
	//! Exact world brush-side handle captured from the impact trace when available.
	uintptr_t hitSurfaceHandle;
	uint32_t flags;
} sg_decal_spawn_params_t;

/**
*	@brief	Runtime state snapshot for one decal instance.
**/
typedef struct sg_decal_runtime_state_s {
	uint32_t decalId;
	uint32_t generation;
	QMTime spawnTime;
	QMTime expireTime;
	float normalizedAge;
	float alpha;
	qboolean active;
} sg_decal_runtime_state_t;

#ifdef __cplusplus
}
#endif
