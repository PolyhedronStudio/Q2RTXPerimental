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

#ifndef REFRESH_SHARED_TYPES_H
#define REFRESH_SHARED_TYPES_H

// WID: C++20: In case of C++ including this..
#ifdef __cplusplus
// We extern "C"
extern "C" {
#endif


/**
*
*
*
*   Constants:
*
*
*
**/
// Needed for IQM_MAX_JOINTS
#include "format/iqm.h"
//! Maximum joints for SKM configuration info.
#define SKM_MAX_BONES IQM_MAX_JOINTS

//! Refresh limits.
#define MAX_DLIGHTS 32
#define MAX_ENTITIES 8192 // == MAX_PACKET_ENTITIES * 2
#define MAX_PARTICLES 16384
#define MAX_LIGHTSTYLES 256

#define POWERSUIT_SCALE 4.0f
#define WEAPONSHELL_SCALE 0.5f

#define SHELL_RED_COLOR 0xF2
#define SHELL_GREEN_COLOR 0xD0
#define SHELL_BLUE_COLOR 0xF3

#define SHELL_RG_COLOR 0xDC
// #define SHELL_RB_COLOR        0x86
#define SHELL_RB_COLOR 0x68
#define SHELL_BG_COLOR 0x78

// ROGUE
#define SHELL_DOUBLE_COLOR 0xDF // 223
#define SHELL_HALF_DAM_COLOR 0x90
#define SHELL_CYAN_COLOR 0x72
// ROGUE

#define SHELL_WHITE_COLOR 0xD7

#define RF_SHELL_MASK       (RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE | \
                            RF_SHELL_DOUBLE | RF_SHELL_HALF_DAM)

#define DLIGHT_CUTOFF 64



/**
*
*   Sprite Model:
*
**/
/**
*   @brief
**/
typedef struct mspriteframe_s {
    int             width, height;
    int             origin_x, origin_y;
    struct image_s  *image;
} mspriteframe_t;



/**
*
*   Model Class and LightPoly:
*
**/
/**
*   @brief
**/
typedef enum
{
	MCLASS_REGULAR,
	MCLASS_EXPLOSION,
	MCLASS_FLASH,
	MCLASS_SMOKE,
    MCLASS_STATIC_LIGHT,
    MCLASS_FLARE
} model_class_t;
/**
*   @brief
**/
typedef struct light_poly_s {
    float positions[ 9 ]; // 3x vec3_t
    vec3_t off_center;
    vec3_t color;
    struct pbr_material_s *material;
    int cluster;
    int style;
    float emissive_factor;
} light_poly_t;



/**
*
* 
*   IQM Mesh Data:  
*
* 
* 
*   TODO: 
* 
*   Rename instead of typedef these to skm_name_t. The reason for that is this is
*   actual in-memory representation of the IQM, and not the actual IQM data structs themselves.
*
*   'But why did you not do this yet?', because I prefer not to touch the VKPT code much right now.
* 
* 
**/
/**
*   @brief
**/
typedef struct {
	vec3_t translate;
	quat_t rotate;
	vec3_t scale;
} iqm_transform_t;
//! Typedef for convenience.
typedef iqm_transform_t skm_transform_t;

/**
*   @brief
**/
typedef struct {
	char name[MAX_QPATH];
	uint32_t first_frame;
	uint32_t num_frames;
    float framerate;
    int32_t flags;
	//bool loop;
} iqm_anim_t;
//! Typedef for convenience.
typedef iqm_anim_t skm_anim_t;

/**
*   @brief  'Inter-Quake-Model'
**/
typedef struct {
	uint32_t num_vertexes;
	uint32_t num_triangles;
	uint32_t num_frames;
	uint32_t num_meshes;
	uint32_t num_joints;
	uint32_t num_poses;
	uint32_t num_animations;
	struct iqm_mesh_s* meshes;

	uint32_t* indices;

	// vertex arrays
	float* positions;
	float* texcoords;
	float* normals;
	float* tangents;
	byte* colors;
    byte* blend_indices; // byte4 per vertex
	byte* blend_weights; // byte4 per vertex
	
	char* jointNames;
	int* jointParents;
	float* bindJoints; // [num_joints * 12]
	float* invBindJoints; // [num_joints * 12]
	iqm_transform_t* poses; // [num_frames * num_poses]
	float* bounds;
	
	iqm_anim_t* animations;
} iqm_model_t;
//! Typedef for convenience.
typedef iqm_model_t skm_model_t;

/**
*   @brief 'Inter-Quake-Model' Mesh.
**/
typedef struct iqm_mesh_s
{
	char name[MAX_QPATH];
	char material[MAX_QPATH];
	iqm_model_t* data;
	uint32_t first_vertex, num_vertexes;
	uint32_t first_triangle, num_triangles;
	uint32_t first_influence, num_influences;
} iqm_mesh_t;
// Typedef for convenience.
typedef iqm_mesh_t skm_mesh_t;

/**
*
*
*   SKM Bone Node
*
*
**/
/**
*   @brief  Stores (generated from IQM data) bone information to make it more convenient
*           0for proper working access.
**/
typedef struct skm_bone_node_s skm_bone_node_t;
typedef struct skm_bone_node_s {
    //! Bone name.
    char name[ MAX_QPATH ];
    //! Bone Number (This is also the actual index inside the boneArray).
    int32_t number;
    //! Bone parent. (Usually not -1.)
    int32_t parentNumber;

    //! Pointer to parent bone. (nullptr if this is the root bone.)
    skm_bone_node_t *parentBone;

    //! Number of child bone nodes.
    int32_t numberOfChildNodes;
    //! Allocated array of child bones.
    skm_bone_node_t **childBones;
} skm_bone_node_t;



/**
*
*
*   Skeletal Model Bone RootMotion:
*
*
**/
//! Calculate it for all axis.
#define SKM_POSE_TRANSLATE_ALL 0
//! Calculate translation of the bone's X Axis.
#define SKM_POSE_TRANSLATE_X ( 1 << 0 )
//! Calculate translation of the bone's Y Axis.
#define SKM_POSE_TRANSLATE_Y ( 1 << 1 )
//! Calculate translation of the bone's Z Axis.
#define SKM_POSE_TRANSLATE_Z ( 1 << 2 )

/**
*	@brief
**/
typedef struct skm_rootmotion_s {
    //! Name of this root motion.
    char name[ MAX_QPATH ];

    //! The first frame index into the animation frames array.
    int32_t firstFrameIndex;
    //! The last frame index into the animation frames array.
    int32_t lastFrameIndex;

    //! The total number of 'Root Bone' frames this motion contains.
    int32_t frameCount;

    //! The 'Root Bone' translation offset relative to each frame. From 0 to frameCount.
    /*std::vector<Vector3>*/
    Vector3 **translations;
    //! The total 'Root Bone' of all translations by this motion.
    Vector3 totalTranslation;

    //! The 'Root Bone' translation distance relative to each frame. From 0 to frameCount.
    /*std::vector<float> */
    float **distances;
    //! The total 'Root Bone' distance traversed by this motion.
    float totalDistance;
} skm_rootmotion_t;

/**
*	@brief
**/
typedef struct skm_rootmotion_set_s {
    //! The total number of Root Motions contained in this set.
    //! (Technically this'll always be identical to IQM Data its num_animations.)
    int32_t numberOfRootMotions;

    //! Stores the root motions sorted by animation index number.
    skm_rootmotion_t **motions;
} skm_rootmotion_set_t;



/**
*
*
*   Skeletal Model (Configuration/Analyzed-) Data:
*
*   Contains a Bones Array for easy indexed access, as well as an actual
*   Bones Node Tree for recursive traversing.
* 
*   Also stores the pre-calculated Root Bone Motion data for each IQM
*   animation, which are optionally configured to cancel out axes for
*   rendering as well as the actual tracking of the Bone Motion.
*
**/
/**
*   @brief  Stores Skeletal Model data conveniently, sourced from the IQM mesh and
*           an (optional but mostly required) '.skc' model configuration file.
* 
*           It stores a bone array for easy accessing bones by their number, as well
*           as a dynamically allocated bone tree for iterative purposes.
* 
*           Also pre-calculated and stored are the rootmotion translation/distance for
*           all frames per animation.
**/
typedef struct skm_config_s {
    //! Actual number of bones stored in the parenting model_t iqm data.
    int32_t numberOfBones;

    //! Stores the bones indexed by bone number, for quick access.
    skm_bone_node_t boneArray[ SKM_MAX_BONES ];
    //! This is the root bone node, for iterative purposes.
    skm_bone_node_t *boneTree;
    //! Pointers to the root bones in the tree, for easy access.
    struct {
        //! For the 'RootMotion' system.
        skm_bone_node_t *motion;

        //! For the 'Head'(Well, head, duhh) bone.
        skm_bone_node_t *head;
        //! For the 'Torso'(Upper Body) bone.
        skm_bone_node_t *torso;
        //! For the 'Hip'(Lower Body) bone.
        skm_bone_node_t *hip;
    } rootBones;

    //! Stores all pre-calculated motions of the Root Motion Bone, for all frames, of each animation.
    skm_rootmotion_set_t rootMotion;
} skm_config_t;


/**
*
*   Actual model_t data.
*
**/
typedef struct model_s {
    enum {
        MOD_FREE,
        MOD_ALIAS,
        MOD_SPRITE,
        MOD_EMPTY
    } type;
    char name[MAX_QPATH];
    int registration_sequence;
    memhunk_t hunk;

    // alias models
    int numframes;
    struct maliasframe_s *frames;

#if 0 // WID: We use this data structure on the server also, so we don't need these checks.
    #if USE_REF == REF_GL || USE_REF == REF_VKPT
        int nummeshes;
        struct maliasmesh_s *meshes;
	    model_class_t model_class;
    #endif
#else
    int nummeshes;
    struct maliasmesh_s *meshes;
    model_class_t model_class;
#endif
//#else
//    int numskins;
//    struct image_s *skins[MAX_ALIAS_SKINS];
//    int numtris;
//    struct maliastri_s *tris;
//    int numsts;
//    struct maliasst_s *sts;
//    int numverts;
//    int skinwidth;
//    int skinheight;
//#endif

    // sprite models
    struct mspriteframe_s *spriteframes;
	bool sprite_vertical;

	skm_model_t* skmData;
    skm_config_t *skmConfig;

	int num_light_polys;
	light_poly_t* light_polys;
} model_t;



/**
*
* 
*   Refresh Entity:
*
* 
**/
/**
*   @brief  Entity for the refresh module, these define the objects 
*           processed for rendering to screen.
* 
*   @note   WID: TODO: Rename r_entity_t, however this involves VKPT code changes which right now,
*           I am trying really hard to avoid.
**/
typedef struct entity_s {
    //! Opaque type outside refresh.
    qhandle_t model;
    vec3_t angles;

    //
    // most recent data
    //
    //! Also used as RF_BEAM's "from".
    vec3_t origin;
    //! Also used as RF_BEAM's diameter.
    int frame;

    //
    // previous data for lerping
    //
    //! Also used as RF_BEAM's "to".
    vec3_t oldorigin;
    int oldframe;

    //
    // misc
    //
    //! 0.0 = current, 1.0 = old
    float backlerp;
    //! Also used as RF_BEAM's palette index, // -1 => use rgba.
    int skinnum; 
    
    //! Ignored if RF_TRANSLUCENT isn't set.
    float alpha; 
    //! RGBA Color for when skinnum == 01.
    color_t rgba;

    //! NULL for inline skin
    qhandle_t skin;
    //! RenderFX flags.
    int flags;

    //! Refresh Entity ID.
    int id;
    //! Temp Entity Type.
    int tent_type;

    //! Scale.
    float scale;

    //
    // Q2RTXP: Skeletal Model Stuff
    //
    //! Pointer to the blend composed final animation pose of the skeletal model data.
    //! Buffer is expected to be SKM_MAX_BONES(256) tops.
    iqm_transform_t *bonePoses;

    //! Whether to use Root Motion at all.
    //! Root Motion Bone ID.
    int32_t rootMotionBoneID;
    //! Root Motion Axis Flags. (If 0, no root motion is taken into account when computing poses.)
    int32_t rootMotionFlags;

} entity_t;



/**
*
* 
*   Dynamic Lights:
*
* 
**/
/**
*   @brief
**/
typedef enum dlight_type_e {
    DLIGHT_SPHERE = 0,
    DLIGHT_SPOT
} dlight_type;
/**
*   @brief
**/
typedef enum dlight_spot_emission_profile_e {
    DLIGHT_SPOT_EMISSION_PROFILE_FALLOFF = 0,
    DLIGHT_SPOT_EMISSION_PROFILE_AXIS_ANGLE_TEXTURE
} dlight_spot_emission_profile;
/**
*   @brief
**/
typedef struct dlight_s {
    vec3_t origin;
    // WID: Leave it uncommented so memory is aligned properly either way.
    //#if USE_REF == REF_GL
    vec3_t transformed;
    //#endif
    vec3_t color;
    float intensity;
    float radius;

    //! VKPT light types support
    dlight_type light_type;
    //! Spotlight options
    struct {
        //! Spotlight emission profile
        dlight_spot_emission_profile emission_profile;
        //! Spotlight direction
        vec3_t direction;
        union {
            //! Options for DLIGHT_SPOT_EMISSION_PROFILE_FALLOFF
            struct {
                //! Cosine of angle of spotlight cone width (no emission beyond that)
                float cos_total_width;
                //! Cosine of angle of start of falloff (full emission below that)
                float cos_falloff_start;
            };
            //! Options for DLIGHT_SPOT_EMISSION_PROFILE_AXIS_ANGLE_TEXTURE
            struct {
                //! Angle of spotlight cone width (no emission beyond that), in radians
                float total_width;
                //! Emission profile texture, indexed by 'angle / total_width'
                qhandle_t texture;
            };
        };
    } spot;
} dlight_t;



/**
*
*
*   Particles:
*
*
**/
/**
*   @brief
**/
typedef struct particle_s {
    vec3_t origin;
    int color; // -1 => use rgba
    float alpha;
    color_t rgba;
    float brightness;
    float radius;
} particle_t;



/**
*
*
*   LightStyles:
*
*
**/
/**
*   @brief
**/
typedef struct lightstyle_s {
    float           white;          // highest of RGB
    vec3_t          rgb;            // 0.0 - 2.0
} lightstyle_t;



/**
*
*
*   Decals:
*
*
**/
/**
*   @brief
**/
#define MAX_DECALS 50
typedef struct decal_s {
    vec3_t pos;
    vec3_t dir;
    float spread;
    float length;
    float dummy;
} decal_t;



/**
*
*
*   Refresh Module:
*
*
**/
/**
*   @brief  Self descriptive, used by refresh code for scissor/view regions.
**/
typedef struct {
    int left, right, top, bottom;
} clipRect_t;

/**
*   @brief  Passes information back from the RTX renderer to the engine for various development maros
**/
typedef struct ref_feedback_s {
    int viewcluster;
    int lookatcluster;
    int num_light_polys;
    int resolution_scale;

    char view_material[ MAX_QPATH ];
    char view_material_override[ MAX_QPATH ];
    int view_material_index;

    vec3_t hdr_color;
    float adapted_luminance;
} ref_feedback_t;

/**
*   @brief
**/
typedef struct refdef_s {
    int x, y, width, height; // in virtual screen coordinates
    float fov_x, fov_y;
    vec3_t vieworg;
    vec3_t viewangles;
    vec4_t screen_blend; // rgba 0-1 full screen blend
    //vec4_t damageblend; // rgba 0-1 full screen damage blend
    float time; // time is uesed to auto animate
    int rdflags; // RDF_UNDERWATER, etc

    byte *areabits; // if not NULL, only areas with set bits will be drawn

    lightstyle_t *lightstyles; // [MAX_LIGHTSTYLES]

    int num_entities;
    entity_t *entities;

    int num_dlights;
    dlight_t *dlights;

    int num_particles;
    particle_t *particles;

    int decal_beg;
    int decal_end;
    decal_t decal[ MAX_DECALS ];

    ref_feedback_t feedback;
} refdef_t;

/**
*   @brief
**/
typedef enum {
    QVF_ACCELERATED = ( 1 << 0 ),
    QVF_GAMMARAMP = ( 1 << 1 ),
    QVF_FULLSCREEN = ( 1 << 2 )
} vidFlags_t;

/**
*   @brief
**/
typedef struct {
    int width;
    int height;
    vidFlags_t flags;
} refcfg_t;

/**
*   @brief
**/
typedef enum {
    IF_NONE = 0,
    IF_PERMANENT = ( 1 << 0 ),
    IF_TRANSPARENT = ( 1 << 1 ),
    IF_PALETTED = ( 1 << 2 ),
    IF_UPSCALED = ( 1 << 3 ),
    IF_SCRAP = ( 1 << 4 ),
    IF_TURBULENT = ( 1 << 5 ),
    IF_REPEAT = ( 1 << 6 ),
    IF_NEAREST = ( 1 << 7 ),
    IF_OPAQUE = ( 1 << 8 ),
    IF_SRGB = ( 1 << 9 ),
    IF_FAKE_EMISSIVE = ( 1 << 10 ),
    IF_EXACT = ( 1 << 11 ),
    IF_NORMAL_MAP = ( 1 << 12 ),
    IF_BILERP = ( 1 << 13 ), // always lerp, independent of bilerp_pics cvar

    // Image source indicator/requirement flags
    IF_SRC_BASE = ( 0x1 << 16 ),
    IF_SRC_GAME = ( 0x2 << 16 ),
    IF_SRC_MASK = ( 0x3 << 16 ),
} imageflags_t;
// Shift amount for storing fake emissive synthesis threshold
#define IF_FAKE_EMISSIVE_THRESH_SHIFT 20

/**
*   @brief
**/
typedef enum {
    IT_PIC,
    IT_FONT,
    IT_SKIN,
    IT_SPRITE,
    IT_WALL,
    IT_SKY,

    IT_MAX
} imagetype_t;

/**
*   @brief
**/
typedef enum ref_type_e {
    REF_TYPE_NONE = 0,
    REF_TYPE_GL,
    REF_TYPE_VKPT
} ref_type_t;

// WID: C++20: In case of C++ including this..
#ifdef __cplusplus
};
#endif

#endif // REFRESH_H
