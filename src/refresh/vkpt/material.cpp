/********************************************************************
*
*
*	VKPT Renderer: PBR Material System
*
*	Implements a comprehensive physically-based rendering (PBR) material
*	system that manages texture properties, surface characteristics, and
*	emissive light sources for the Q2RTX path tracer. Handles loading,
*	parsing, and runtime management of material definitions from .mat files.
*
*	Architecture:
*	- Three-tier material hierarchy: Global → Map-specific → Runtime
*	- Hash table for fast material lookup by name
*	- Binary search in sorted material dictionaries
*	- Material override system (map materials override global)
*	- Registration sequence tracking for resource cleanup
*
*	Algorithm:
*	1. Initialization: Load all materials/*.mat files into global dictionary
*	2. Map Load: Load map-specific .mat file, sort and deduplicate
*	3. Material Request: Search hash table → check map overrides → create runtime entry
*	4. Image Loading: Load base/normal/emissive textures with fallback paths
*	5. Emissive Synthesis: Optionally generate fake emissive maps from bright pixels
*	6. Cleanup: Free unused materials based on registration sequence
*
*	Features:
*	- PBR properties: roughness, metalness, bump scale, emissive factor
*	- Material kinds: Regular, Chrome, Glass, Water, Lava, Sky, Slime, etc.
*	- Custom texture assignment: base, normals, emissive, mask
*	- Auto-detection of emissive surfaces based on brightness threshold
*	- Synthetic emissive generation for dynamic lighting
*	- Map-specific material overrides
*	- Game-specific material loading (baseq2 vs mod directories)
*	- Console interface for runtime material editing
*	- Material save/export functionality
*
*	Configuration:
*	- MAX_PBR_MATERIALS: Maximum number of material slots (8192)
*	- RMATERIALS_HASH: Hash table size for fast lookup (256 buckets)
*	- cvar_pt_surface_lights_threshold: Brightness threshold for emissive detection
*	- cvar_pt_surface_lights_fake_emissive_algo: Algorithm for synthetic emissive
*
*	File Format (.mat files):
*	- Section format: "texture_name:" or "name," for material groups
*	- Key-value pairs: "attribute value"
*	- Comments: "# comment text"
*	- Wildcard substitution: "*" for material name, "**" for full path
*	- Attributes: bump_scale, roughness_override, metalness_factor,
*	  emissive_factor, specular_factor, base_factor, kind, is_light,
*	  texture_base, texture_normals, texture_emissive, texture_mask,
*	  light_styles, bsp_radiance, default_radiance, synth_emissive,
*	  emissive_threshold
*
*	Material Override System:
*	- Global materials: Loaded from materials/*.mat (baseq2 or mod directory)
*	- Map materials: Loaded from maps/mapname.mat (overrides global definitions)
*	- Runtime materials: Created on-demand, inherit from global/map definitions
*	- Priority: Map-specific > Global > Auto-generated default
*
*	Emissive Detection:
*	- Brightness threshold: Pixels above threshold considered emissive
*	- Fake emissive algorithms:
*	  0: Use diffuse texture directly
*	  1: Generate filtered bright-pixel mask
*	- Synthetic emissive: Automatically create emissive map from bright areas
*	- Manual emissive: Explicit texture_emissive specification
*
*	Performance:
*	- Hash table provides O(1) average-case lookup
*	- Binary search in dictionaries: O(log n)
*	- Materials sorted and deduplicated once at load time
*	- Registration sequence avoids full table scans
*	- Lazy texture loading (images loaded on first use)
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

#include "material.h"
#include "vkpt.h"
#include <common/prompt.h>

#include <stdlib.h>
#include <string.h>
#include <assert.h>


extern cvar_t *cvar_pt_surface_lights_fake_emissive_algo;
extern cvar_t* cvar_pt_surface_lights_threshold;

extern void CL_PrepRefresh(void);



/**
*
*
*
*	Module State and Data Structures:
*
*
*
**/
//! Runtime material table: active materials currently in use by the renderer.
//! Each entry represents a fully-loaded material with resolved textures and properties.
//! Materials are allocated on-demand when textures are first referenced.
pbr_material_t r_materials[MAX_PBR_MATERIALS];

//! Global material definitions: loaded from materials/*.mat files at initialization.
//! These serve as templates that are copied to r_materials when needed.
//! Sorted alphabetically by material name for binary search.
static pbr_material_t r_global_materials[MAX_PBR_MATERIALS];

//! Map-specific material definitions: loaded from maps/mapname.mat on map change.
//! Override global definitions for the same material names.
//! Also sorted alphabetically for binary search.
static pbr_material_t r_map_materials[MAX_PBR_MATERIALS];

//! Number of valid entries in r_global_materials array.
static uint32_t num_global_materials = 0;

//! Number of valid entries in r_map_materials array.
static uint32_t num_map_materials = 0;

//! Hash table size for fast material lookup (power of 2 for efficient modulo).
#define RMATERIALS_HASH 256

//! Hash table for O(1) average-case lookup of active materials in r_materials.
//! Each bucket contains a linked list of materials with the same hash value.
static list_t r_materialsHash[RMATERIALS_HASH];

//! Reload flag: indicates map geometry needs to be reloaded.
#define RELOAD_MAP		1

//! Reload flag: indicates emissive textures need to be regenerated.
#define RELOAD_EMISSIVE	2

//! Reload flag: indicates emissive textures need to be regenerated.
#define RELOAD_EMISSIVE	2

//! Forward declarations for internal functions.
static uint32_t load_material_file(const char* file_name, pbr_material_t* dest, uint32_t max_items);
static void material_command(void);
static void material_completer(genctx_t* ctx, int argnum);



/**
*
*
*
*	Material Sorting and Deduplication:
*
*
*
**/
/**
*	@brief	Comparison function for sorting materials by name, source file, and line number.
*	@param	a	Pointer to first material (const void* for qsort compatibility).
*	@param	b	Pointer to second material (const void* for qsort compatibility).
*	@return	Negative if a < b, zero if equal, positive if a > b.
*	@note	Used by qsort() to order materials for deduplication.
*			Sort order: material name (primary), source file (secondary), line number (tertiary).
**/
static int compare_materials(const void* a, const void* b)
{
	const pbr_material_t* ma = a;
	const pbr_material_t* mb = b;

	// Primary sort: material name (alphabetical).
	int names = strcmp(ma->name, mb->name);
	if (names != 0)
		return names;

	// Secondary sort: source file (alphabetical).
	// This groups materials from the same file together.
	int sources = strcmp(ma->source_matfile, mb->source_matfile);
	if (sources != 0)
		return sources;

	// Tertiary sort: line number (ascending).
	// Materials defined later in the file override earlier definitions.
	return (int)ma->source_line - (int)mb->source_line;
}

/**
*	@brief	Sorts and deduplicates a material array, keeping the last definition of each material.
*	@param	first	Pointer to the first element of the material array.
*	@param	pCount	Pointer to the count of materials (updated to new count after deduplication).
*	@note	Materials with the same name are merged, with later entries overriding earlier ones.
*			This implements the material override system where definitions at the end of a file
*			or in later files take precedence.
*	@note	The array is sorted by name (primary), source file (secondary), and line number (tertiary).
*	@note	After deduplication, only one entry per material name remains in the array.
**/

static void sort_and_deduplicate_materials(pbr_material_t* first, uint32_t* pCount)
{
	const uint32_t count = *pCount;
	
	if (count == 0)
		return;

	// Sort the materials by name (primary), then by source file, then by line number.
	// This groups all definitions of the same material together.
	qsort(first, count, sizeof(pbr_material_t), compare_materials);

	// Deduplicate materials with the same name; latter entries override former ones.
	// This implements the material override system where later definitions win.
	uint32_t write_ptr = 0;
	
	for (uint32_t read_ptr = 0; read_ptr < count; read_ptr++) {
		// If there is a next entry and its name is the same, skip the current entry.
		// This keeps only the LAST definition of each material name.
		if ((read_ptr + 1 < count) && strcmp(first[read_ptr].name, first[read_ptr + 1].name) == 0)
			continue;

		// Copy the input entry to the output entry if they are not the same.
		// This compacts the array by removing duplicates in-place.
		if (read_ptr != write_ptr)
			memcpy(first + write_ptr, first + read_ptr, sizeof(pbr_material_t));

		++write_ptr;
	}

	if (write_ptr < count) {
		// If we've removed some entries, clear the garbage at the end.
		// This ensures unused slots are zeroed out.
		memset(first + write_ptr, 0, sizeof(pbr_material_t) * (count - write_ptr));

		// Return the new count (number of unique materials).
		*pCount = write_ptr;
	}
}



/**
*
*
*
*	Utility Functions:
*
*
*
**/
/**
*	@brief	Checks if the current game is a custom mod (not baseq2).
*	@return	True if running a custom game/mod, false if running baseq2.
*	@note	Used to determine which directory to search for game-specific assets.
**/
static qboolean is_game_custom(void)
{
	return fs_game->string[0] && strcmp(fs_game->string, BASEGAME) != 0;
}



/**
*
*
*
*	Initialization and Cleanup:
*
*
*
**/
/**
*	@brief	Initializes the material system and loads all global material definitions.
*	@note	Called once at engine startup.
*			- Registers the "mat" console command for runtime material editing
*			- Initializes material hash table for fast lookup
*			- Scans materials/ directory for .mat files
*			- Loads and parses all global material definitions
*			- Sorts and deduplicates material entries
*	@note	Global materials serve as templates for runtime materials.
**/

void MAT_Init()
{
	// Register console command for runtime material manipulation.
	cmdreg_t commands[2];
	commands[0].name = "mat";
	commands[0].function = (xcommand_t)&material_command;
	commands[0].completer = &material_completer;
	commands[1].name = NULL;
	Cmd_Register(commands);
	
	// Clear all material arrays to ensure clean state.
	memset(r_materials, 0, sizeof(r_materials));
	memset(r_global_materials, 0, sizeof(r_global_materials));
	memset(r_map_materials, 0, sizeof(r_map_materials));
	num_global_materials = 0;
	num_map_materials = 0;

	// Initialize the hash table for fast material lookup.
	// Each bucket contains a linked list for collision resolution.
	for (int i = 0; i < RMATERIALS_HASH; i++)
	{
		List_Init(r_materialsHash + i);
	}

	// Find all *.mat files in the materials/ directory.
	// These are global material definitions shared across all maps.
	int num_files;
	void** list = FS_ListFiles("materials", ".mat", 0, &num_files);
	
	for (int i = 0; i < num_files; i++) {
		char* file_name = list[i];
		char buffer[MAX_QPATH];
		Q_concat(buffer, sizeof(buffer), "materials/", file_name);
		
		// Load material definitions into the global array.
		int mat_slots_available = MAX_PBR_MATERIALS - num_global_materials;
		if (mat_slots_available > 0) {
			uint32_t count = load_material_file(buffer, r_global_materials + num_global_materials,
				mat_slots_available);
			num_global_materials += count;
			
			Com_Printf("Loaded %d materials from %s\n", count, buffer);
		}
		else {
			Com_WPrintf("Coundn't load materials from %s: no free slots.\n", buffer);
		}
		
		Z_Free(file_name);
	}
	Z_Free(list);

	// Sort and deduplicate global materials for efficient binary search.
	// Later definitions override earlier ones (e.g., mod materials override baseq2).
	sort_and_deduplicate_materials(r_global_materials, &num_global_materials);
}

/**
*	@brief	Shuts down the material system and frees resources.
*	@note	Called at engine shutdown. Unregisters console commands.
**/

void MAT_Shutdown()
{
	Cmd_RemoveCommand("mat");
}



/**
*
*
*
*	Material Initialization and Registration:
*
*
*
**/
/**
*	@brief	Sets the material index field in the material's flags.
*	@param	mat	Pointer to the material to update.
*	@note	The material index is used to quickly reference materials by index.
*			Also initializes next_frame to point to itself for single-frame materials.
**/

static void MAT_SetIndex(pbr_material_t* mat)
{
	uint32_t mat_index = (uint32_t)(mat - r_materials);
	mat->flags = (mat->flags & ~MATERIAL_INDEX_MASK) | (mat_index & MATERIAL_INDEX_MASK);
	mat->next_frame = mat_index;
}

/**
*	@brief	Resets a material to default PBR values.
*	@param	mat	Pointer to the material to reset.
*	@note	Default values:
*			- bump_scale: 1.0 (no scaling)
*			- roughness_override: -1.0 (use texture value)
*			- metalness_factor: 1.0 (full metalness if present in texture)
*			- emissive_factor: 1.0 (full emissive brightness)
*			- specular_factor: 1.0 (full specularity)
*			- base_factor: 1.0 (full diffuse color)
*			- light_styles: true (affected by dynamic lights)
*			- bsp_radiance: true (receives indirect lighting)
*			- default_radiance: 1.0 (fully emissive without CM_SURFACE_FLAG_LIGHT)
*			- num_frames: 1 (static, non-animated)
*			- kind: MATERIAL_KIND_REGULAR
*			- emissive_threshold: from cvar_pt_surface_lights_threshold
**/

void MAT_Reset(pbr_material_t * mat)
{
	memset(mat, 0, sizeof(pbr_material_t));
	mat->bump_scale = 1.0f;
	mat->roughness_override = -1.0f;
	mat->metalness_factor = 1.f;
	mat->emissive_factor = 1.f;
	mat->specular_factor = 1.f;
	mat->base_factor = 1.f;
	mat->light_styles = true;
	mat->bsp_radiance = true;
	/* Treat absence of CM_SURFACE_FLAG_LIGHT flag as "fully emissive" by default.
	 * Typically works well with explicit emissive image. */
	mat->default_radiance = 1.f;
	mat->flags = MATERIAL_KIND_REGULAR;
	mat->num_frames = 1;
	mat->emissive_threshold = cvar_pt_surface_lights_threshold->integer;
}



/**
*
*
*
*	Material Kinds and Type System:
*
*
*
**/
//! Material kind lookup table: maps string names to flag values.
//! Used for parsing .mat files and console commands.

static struct MaterialKind {
	const char * name;  //! Human-readable material kind name (uppercase).
	uint32_t flag;      //! Corresponding material flag bits.
} materialKinds[] = {
	{"INVALID", MATERIAL_KIND_INVALID},      //! Invalid/uninitialized material.
	{"REGULAR", MATERIAL_KIND_REGULAR},      //! Standard opaque PBR material.
	{"CHROME", MATERIAL_KIND_CHROME},        //! Reflective chrome surface.
	{"GLASS", MATERIAL_KIND_GLASS},          //! Transparent glass material.
	{"WATER", MATERIAL_KIND_WATER},          //! Animated water surface.
	{"LAVA", MATERIAL_KIND_LAVA},            //! Animated lava surface (emissive).
	{"SKY", MATERIAL_KIND_SKY},              //! Sky material (uses skybox).
	{"SLIME", MATERIAL_KIND_SLIME},          //! Slime/hazardous liquid.
	{"INVISIBLE", MATERIAL_KIND_INVISIBLE},  //! Invisible surface (clip).
	{"SCREEN", MATERIAL_KIND_SCREEN},        //! Monitor/display screen.
	{"CAMERA", MATERIAL_KIND_CAMERA},        //! Camera view surface.
	{"UNLIT", MATERIAL_KIND_UNLIT},          //! Unlit material (no lighting).
};

//! Number of material kinds defined in the lookup table.
static int nMaterialKinds = sizeof(materialKinds) / sizeof(struct MaterialKind);

/**
*	@brief	Converts a material kind name string to its corresponding flag value.
*	@param	kindname	Material kind name (case-insensitive, e.g., "CHROME", "glass").
*	@return	Material kind flag value, or MATERIAL_KIND_REGULAR if not found.
*	@note	Used when parsing .mat files and processing console commands.
**/
uint32_t MAT_GetMaterialKindForName(const char * kindname) {
	for ( int i = 0; i < nMaterialKinds; ++i ) {
		if ( Q_stricmp( kindname, materialKinds[ i ].name ) == 0 ) {
			return materialKinds[ i ].flag;
		}
	}
	return MATERIAL_KIND_REGULAR;  // Default to regular if not found.
}

/**
*	@brief	Converts a material kind flag value to its string name.
*	@param	flag	Material flags containing kind bits.
*	@return	Material kind name string, or NULL if not found.
*	@note	Used for displaying material information in console and debug output.
**/
const char * MAT_GetMaterialKindName(uint32_t flag) {
	for ( int i = 0; i < nMaterialKinds; ++i ) {
		if ( ( flag & MATERIAL_KIND_MASK ) == materialKinds[ i ].flag ) {
			return materialKinds[ i ].name;
		}
	}
	return NULL;
}



/**
*
*
*
*	Material Attribute System:
*
*
*
**/
/**
*	@brief	Truncates the file extension from a texture name.
*	@param	src		Source texture name with extension (e.g., "textures/base.tga").
*	@param	dest	Destination buffer for name without extension.
*	@return	Length of the truncated name (excluding null terminator).
*	@note	Assumes 4-character extensions (.tga, .jpg, .png, .wal).
*			If no extension found, copies the full name.
**/

static size_t truncate_extension(char const* src, char dest[MAX_QPATH])
{
	size_t len = strlen(src);
	assert(len < MAX_QPATH);

	if (len > 4 && src[len - 4] == '.')
		len -= 4;
	
	Q_strlcpy(dest, src, len + 1);
	
	return len;
}



/**
*
*
*
*	Material Allocation and Lookup:
*
*
*
**/
/**
*	@brief	Allocates a free material slot from the runtime materials array.
*	@return	Pointer to an unused material slot, or triggers fatal error if none available.
*	@note	Searches for a material with registration_sequence == 0 (unused).
*			Fatal error if all MAX_PBR_MATERIALS slots are occupied.
**/
static pbr_material_t* allocate_material(void)
{
	for (uint32_t i = 0; i < MAX_PBR_MATERIALS; i++)
	{
		pbr_material_t* mat = r_materials + i;
		if (!mat->registration_sequence)
			return mat;
	}

	Com_Error(ERR_FATAL, "Couldn't allocate a new material: insufficient r_materials slots.\n"
		"Increase MAX_PBR_MATERIALS and rebuild the engine.\n");
	return NULL;
}

/**
*	@brief	Searches for a material by name in the hash table (runtime materials).
*	@param	name	Material name (without extension, lowercase).
*	@param	hash	Pre-computed hash value for the material name.
*	@param	first	Unused parameter (kept for API consistency).
*	@param	count	Unused parameter (kept for API consistency).
*	@return	Pointer to material if found, NULL otherwise.
*	@note	Searches only the hash table (active runtime materials).
*			Uses linked list traversal for collision resolution.
*			Only returns materials with valid registration_sequence.
**/
static pbr_material_t* find_material(const char* name, uint32_t hash, pbr_material_t* first, uint32_t count)
{
	pbr_material_t* mat;
	
	LIST_FOR_EACH(pbr_material_t, mat, &r_materialsHash[hash], entry)
	{
		if (!mat->registration_sequence)
			continue;

		if (!strcmp(name, mat->name))
			return mat;
	}

	return NULL;
}

/**
*	@brief	Searches for a material by name using binary search in a sorted array.
*	@param	name	Material name (without extension, lowercase).
*	@param	first	Pointer to the first element of the sorted material array.
*	@param	count	Number of materials in the array.
*	@return	Pointer to material if found, NULL otherwise.
*	@note	Used to search global and map-specific material dictionaries.
*			Requires the array to be sorted alphabetically by material name.
*			Time complexity: O(log n)
**/
static pbr_material_t* find_material_sorted(const char* name, pbr_material_t* first, uint32_t count)
{
	// binary search in a sorted table: global and per-map material dictionaries
	int left = 0, right = (int)count - 1;

	while (left <= right)
	{
		int middle = (left + right) / 2;
		pbr_material_t* mat = first + middle;

		int cmp = strcmp(name, mat->name);

		if (cmp < 0)
			right = middle - 1;
		else if (cmp > 0)
			left = middle + 1;
		else
			return mat;
	}

	return NULL;
}

//! Material attribute indices: used for attribute lookup and parsing.
enum AttributeIndex
{
	MAT_BUMP_SCALE,              //! Normal map bump intensity multiplier.
	MAT_ROUGHNESS_OVERRIDE,      //! Override roughness value (ignores texture).
	MAT_METALNESS_FACTOR,        //! Metalness multiplier.
	MAT_EMISSIVE_FACTOR,         //! Emissive intensity multiplier.
	MAT_KIND,                    //! Material type (water, glass, chrome, etc.).
	MAT_IS_LIGHT,                //! Whether surface emits light.
	MAT_BASE_FACTOR,             //! Diffuse color multiplier.
	MAT_TEXTURE_BASE,            //! Base color/diffuse texture path.
	MAT_TEXTURE_NORMALS,         //! Normal map texture path.
	MAT_TEXTURE_EMISSIVE,        //! Emissive texture path.
	MAT_LIGHT_STYLES,            //! Enable dynamic lighting (light styles).
	MAT_BSP_RADIANCE,            //! Enable indirect lighting from lightmaps.
	MAT_DEFAULT_RADIANCE,        //! Emissive fallback for surfaces without CM_SURFACE_FLAG_LIGHT.
	MAT_TEXTURE_MASK,            //! Alpha/transparency mask texture path.
	MAT_SYNTH_EMISSIVE,          //! Enable synthetic emissive generation.
	MAT_EMISSIVE_THRESHOLD,      //! Brightness threshold for emissive detection (0-255).
	MAT_SPECULAR_FACTOR,         //! Specular reflection intensity multiplier.
};

//! Material attribute data types for parsing and validation.
enum AttributeType { ATTR_BOOL, ATTR_FLOAT, ATTR_STRING, ATTR_INT };

//! Material attribute definition table: maps attribute names to indices and types.
//! Used for parsing .mat files and console commands.
static struct MaterialAttribute {
	enum AttributeIndex index;  //! Attribute identifier.
	const char* name;           //! Attribute name in .mat files.
	enum AttributeType type;    //! Data type for parsing.
} c_Attributes[] = {
	{MAT_BUMP_SCALE, "bump_scale", ATTR_FLOAT},
	{MAT_ROUGHNESS_OVERRIDE, "roughness_override", ATTR_FLOAT},
	{MAT_METALNESS_FACTOR, "metalness_factor", ATTR_FLOAT},
	{MAT_EMISSIVE_FACTOR, "emissive_factor", ATTR_FLOAT},
	{MAT_KIND, "kind", ATTR_STRING},
	{MAT_IS_LIGHT, "is_light", ATTR_BOOL},
	{MAT_BASE_FACTOR, "base_factor", ATTR_FLOAT},
	{MAT_TEXTURE_BASE, "texture_base", ATTR_STRING},
	{MAT_TEXTURE_NORMALS, "texture_normals", ATTR_STRING},
	{MAT_TEXTURE_EMISSIVE, "texture_emissive", ATTR_STRING},
	{MAT_LIGHT_STYLES, "light_styles", ATTR_BOOL},
	{MAT_BSP_RADIANCE, "bsp_radiance", ATTR_BOOL},
	{MAT_DEFAULT_RADIANCE, "default_radiance", ATTR_FLOAT},
	{MAT_TEXTURE_MASK, "texture_mask", ATTR_STRING},
	{MAT_SYNTH_EMISSIVE, "synth_emissive", ATTR_BOOL},
	{MAT_EMISSIVE_THRESHOLD, "emissive_threshold", ATTR_INT},
	{MAT_SPECULAR_FACTOR, "specular_factor", ATTR_FLOAT},
};

//! Number of defined material attributes.
static int c_NumAttributes = sizeof(c_Attributes) / sizeof(struct MaterialAttribute);

/**
*	@brief	Sets a material texture path and optionally loads the image.
*	@param	mat				Material to modify.
*	@param	svalue			Texture path or "0" to clear.
*	@param	mat_texture_path	Destination path buffer in material structure.
*	@param	mat_image		Destination image pointer in material structure.
*	@param	flags			Image loading flags (IF_SRGB, IF_NONE, etc.).
*	@param	from_console	True if called from console (loads image immediately).
*	@note	If from_console is false (loading from .mat file), only stores the path.
*			If from_console is true, validates and loads the image immediately.
*			Special value "0" clears the texture assignment.
**/

static void set_material_texture(pbr_material_t* mat, const char* svalue, char mat_texture_path[MAX_QPATH],
	image_t** mat_image, imageflags_t flags, bool from_console)
{
	if (strcmp(svalue, "0") == 0) {
		mat_texture_path[0] = 0;
		*mat_image = NULL;
	}
	else if (from_console) {
		image_t* image = IMG_Find(svalue, mat->image_type, flags | IF_EXACT | (mat->image_flags & IF_SRC_MASK));
		if (image != R_NOTEXTURE) {
			Q_strlcpy(mat_texture_path, svalue, MAX_QPATH);
			*mat_image = image;
		}
		else
			Com_WPrintf("Cannot load texture '%s'\n", svalue);
	}
	else {
		Q_strlcpy(mat_texture_path, svalue, MAX_QPATH);
	}
}

/**
*	@brief	Parses and applies a material attribute from a .mat file or console command.
*	@param	mat			Material to modify.
*	@param	attribute	Attribute name (e.g., "roughness_override", "texture_base").
*	@param	value		Attribute value as string.
*	@param	sourceFile	Source .mat file (NULL if from console).
*	@param	lineno		Line number in source file (0 if from console).
*	@param	reload_flags	Optional flags indicating what needs reloading (RELOAD_MAP, RELOAD_EMISSIVE).
*	@return	Q_ERR_SUCCESS on success, Q_ERR_FAILURE on error.
*	@note	Supports wildcard substitution in string values:
*			- "*" expands to material base name (without path)
*			- "**" expands to full material name (with path)
*			Example: "textures/base" with "**_n" becomes "textures/base_n"
*	@note	Some attributes trigger map reload (kind, is_light, mask changes).
*			Some attributes trigger emissive regeneration (synth_emissive, emissive_threshold).
**/
static int set_material_attribute(pbr_material_t* mat, const char* attribute, const char* value,
	const char* sourceFile, uint32_t lineno, unsigned int* reload_flags)
{
	assert(mat);

	// valid attribute-value pairs

	if (attribute == NULL || value == NULL)
		return Q_ERR_FAILURE;

	struct MaterialAttribute const* t = NULL;
	for (int i = 0; i < c_NumAttributes; ++i)
	{
		if (strcmp(attribute, c_Attributes[i].name) == 0)
			t = &c_Attributes[i];
	}
	
	if (!t)
	{
		if (sourceFile)
			Com_EPrintf("%s:%d: unknown material attribute '%s'\n", sourceFile, lineno, attribute);
		else
			Com_EPrintf("Unknown material attribute '%s'\n", attribute);
		return Q_ERR_FAILURE;
	}

	char svalue[MAX_QPATH];

	float fvalue = 0.f; bool bvalue = false;
	int ivalue = 0;
	switch (t->type)
	{
	case ATTR_BOOL:   bvalue = atoi(value) == 0 ? false : true; break;
	case ATTR_FLOAT:  fvalue = (float)atof(value); break;
	case ATTR_STRING: {
		char* asterisk = strchr(value, '*');
		if (asterisk) {
			if(*(asterisk + 1) == '*') {
				// double asterisk: insert complete material name, including path
				Q_strlcpy(svalue, value, min(asterisk - value + 1, sizeof(svalue)));
				Q_strlcat(svalue, mat->name, sizeof(svalue));
				Q_strlcat(svalue, asterisk + 2, sizeof(svalue));
			} else {
				// get the base name of the material, i.e. without the path
				// material names have no extensions, so no need to remove that
				char* slash = strrchr(mat->name, '/');
				char* mat_base = slash ? slash + 1 : mat->name;

				// concatenate: the value before the asterisk, material base name, the rest of the value
				Q_strlcpy(svalue, value, min(asterisk - value + 1, sizeof(svalue)));
				Q_strlcat(svalue, mat_base, sizeof(svalue));
				Q_strlcat(svalue, asterisk + 1, sizeof(svalue));
			}
		}
		else
			Q_strlcpy(svalue, value, sizeof(svalue));
		break;
	case ATTR_INT:
		ivalue = atoi(value);
		break;
	}
	default:
		assert(!"unknown PBR MAT attribute attribute type");
	}

	// set material

	switch (t->index)
	{
	case MAT_BUMP_SCALE: mat->bump_scale = fvalue; break;
	case MAT_ROUGHNESS_OVERRIDE: mat->roughness_override = fvalue; break;
	case MAT_METALNESS_FACTOR: mat->metalness_factor = fvalue; break;
	case MAT_EMISSIVE_FACTOR: mat->emissive_factor = fvalue; break;
	case MAT_KIND: {
		uint32_t kind = MAT_GetMaterialKindForName(svalue);
		if (kind != 0)
			mat->flags = MAT_SetKind(mat->flags, kind);
		else
		{
			if (sourceFile)
				Com_EPrintf("%s:%d: unknown material kind '%s'\n", sourceFile, lineno, svalue);
			else
				Com_EPrintf("Unknown material kind '%s'\n", svalue);
			return Q_ERR_FAILURE;
		}
		if (reload_flags) *reload_flags |= RELOAD_MAP;
	} break;
	case MAT_IS_LIGHT:
		mat->flags = bvalue == true ? mat->flags | MATERIAL_FLAG_LIGHT : mat->flags & ~(MATERIAL_FLAG_LIGHT);
		if (reload_flags) *reload_flags |= RELOAD_MAP;
		break;
	case MAT_BASE_FACTOR:
		mat->base_factor = fvalue;
		break;
	case MAT_TEXTURE_BASE:
		set_material_texture(mat, svalue, mat->filename_base, &mat->image_base, IF_SRGB, !sourceFile);
		break;
	case MAT_TEXTURE_NORMALS:
		set_material_texture(mat, svalue, mat->filename_normals, &mat->image_normals, IF_NONE, !sourceFile);
		break;
	case MAT_TEXTURE_EMISSIVE:
		set_material_texture(mat, svalue, mat->filename_emissive, &mat->image_emissive, IF_SRGB, !sourceFile);
		break;
	case MAT_LIGHT_STYLES:
		mat->light_styles = bvalue;
		if (reload_flags) *reload_flags |= RELOAD_MAP;
		break;
	case MAT_BSP_RADIANCE:
		mat->bsp_radiance = bvalue;
		if (reload_flags) *reload_flags |= RELOAD_MAP;
		break;
	case MAT_DEFAULT_RADIANCE:
		mat->default_radiance = fvalue;
		if (reload_flags) *reload_flags |= RELOAD_MAP;
		break;
	case MAT_TEXTURE_MASK:
		set_material_texture(mat, svalue, mat->filename_mask, &mat->image_mask, IF_NONE, !sourceFile);
		if (reload_flags) *reload_flags |= RELOAD_MAP;
		break;
	case MAT_SYNTH_EMISSIVE:
		mat->synth_emissive = bvalue;
		if (reload_flags) *reload_flags |= RELOAD_EMISSIVE;
		break;
	case MAT_EMISSIVE_THRESHOLD:
		mat->emissive_threshold = ivalue;
		if (reload_flags) *reload_flags |= RELOAD_EMISSIVE;
		break;
	case MAT_SPECULAR_FACTOR: mat->specular_factor = fvalue; break;
	default:
		assert(!"unknown PBR MAT attribute index");
	}
	
	return Q_ERR_SUCCESS;
}



/**
*
*
*
*	Material File Loading and Parsing:
*
*
*
**/
/**
*	@brief	Loads and parses a .mat material definition file.
*	@param	file_name	Path to .mat file (e.g., "materials/base.mat" or "maps/dm1.mat").
*	@param	dest		Destination array to store parsed materials.
*	@param	max_items	Maximum number of materials that can be stored in dest.
*	@return	Number of materials successfully loaded and parsed.
*	@note	File format:
*			- Material sections: "texture_name:" or "name1, name2, name3:" for groups
*			- Comments: "# comment text" (space after # required)
*			- Attributes: "attribute_name value" (whitespace-separated)
*			- Comma syntax: Share attributes across multiple materials
*			- Colon syntax: Final material name before attributes
*	@note	Parser state machine:
*			1. INITIAL: Waiting for first material section
*			2. ACCUMULATING_NAMES: Reading comma-separated material names (shared attributes)
*			3. READING_PARAMS: Reading attribute lines for current material(s)
*	@note	Searches game-specific directory first, then falls back to baseq2.
*			Materials loaded from game directory are tagged with IF_SRC_GAME.
*			Materials loaded from baseq2 are tagged with IF_SRC_BASE.
**/
static uint32_t load_material_file(const char* file_name, pbr_material_t* dest, uint32_t max_items)
{
	assert(max_items >= 1);
	
	char* filebuf = NULL;
	unsigned source = IF_SRC_GAME;

	if (is_game_custom()) {
		// try the game specific path first
		FS_LoadFileEx(file_name, (void**)&filebuf, FS_PATH_GAME, TAG_FILESYSTEM);
	}

	if (!filebuf) {
		// game specific path not found, or we're playing baseq2
		source = IF_SRC_BASE;
		FS_LoadFileEx(file_name, (void**)&filebuf, FS_PATH_BASE, TAG_FILESYSTEM);
	}
	
	if (!filebuf)
		return 0;

	enum
	{
		INITIAL,
		ACCUMULATING_NAMES,
		READING_PARAMS
	} state = INITIAL;

	const char* ptr = filebuf;
	char linebuf[1024];
	uint32_t count = 0;
	uint32_t lineno = 0;
	
	const char* delimiters = " \t\r\n";
	uint32_t num_materials_in_group = 0;

	while (sgets(linebuf, sizeof(linebuf), &ptr))
	{
		++lineno;
		
		{
			char *t = strchr( linebuf, '#' );
			// remove comments, in case it actually IS a comment...
			if ( t && ( (t + 1) < (char*)sizeof(linebuf) && *(t + 1) == ' ' ) ) { // <Q2RTXP> WID: Added cast to char* for this in MSVC: warning C4047: '<': 'char *' differs in levels of indirection from 'size_t'
				*t = 0;
			}   
		}
		
		size_t len = strlen(linebuf);

		// remove trailing whitespace
		while (len > 0 && strchr(delimiters, linebuf[len - 1]))
			--len;
		linebuf[len] = 0;

		if (len == 0)
			continue;


		// if the line ends with a colon (:) or comma (,) it's a new material section
		char last = linebuf[len - 1];
		if (last == ':' || last == ',')
		{
			if (state == READING_PARAMS || state == ACCUMULATING_NAMES)
			{
				++dest;

				if (count > max_items)
				{
					Com_WPrintf("%s:%d: too many materials, expected up to %d.\n", file_name, lineno, max_items);
					Z_Free(filebuf);
					return count;
				}	
			}
			++count;
			
			MAT_Reset(dest);

			// copy the material name but not the colon
			linebuf[len - 1] = 0;
			Q_strlcpy(dest->name, linebuf, sizeof(dest->name));
			dest->image_flags = source;
			dest->registration_sequence = registration_sequence;

			// copy the material file name
			Q_strlcpy(dest->source_matfile, file_name, sizeof(dest->source_matfile));
			dest->source_line = lineno;

			if (state == ACCUMULATING_NAMES)
				++num_materials_in_group;
			else
				num_materials_in_group = 1;

			// state transition - same logic for all states
			state = (last == ',') ? ACCUMULATING_NAMES : READING_PARAMS;
			
			continue;
		}

		// all other lines are material parameters in the form of "key value" pairs
		
		char* key = strtok(linebuf, delimiters);
		char* value = strtok(NULL, delimiters);
		char* extra = strtok(NULL, delimiters);

		if (!key || !value)
		{
			Com_WPrintf("%s:%d: expected key and value\n", file_name, lineno);
			continue;
		}

		if (extra)
		{
			Com_WPrintf("%s:%d: unexpected extra characters after the key and value\n", file_name, lineno);
			continue;
		}

		if (state == INITIAL)
		{
			Com_WPrintf("%s:%d: expected material name section before any parameters\n", file_name, lineno);
			continue;
		}

		if (state == ACCUMULATING_NAMES)
		{
			Com_WPrintf("%s:%d: expected a final material name section ending with a colon before any parameters\n", file_name, lineno);

			// rollback the current material group
			dest -= (num_materials_in_group - 1);
			num_materials_in_group = 0;
			state = INITIAL;
			continue;
		}

		for (uint32_t i = 0; i < num_materials_in_group; i++) 
		{
			set_material_attribute(dest - i, key, value, file_name, lineno, NULL);
		}
	}

	Z_Free(filebuf);
	return count;
}

/**
*	@brief	Exports active materials to a .mat file.
*	@param	file_name	Output file path.
*	@param	save_all	If true, saves all materials; if false, only auto-generated ones.
*	@param	force		If true, overwrites existing file; if false, warns and aborts.
*	@note	Used by "mat save" console command to export material definitions.
*			Auto-generated materials are those without source_matfile (created on-demand).
*			All materials include their full property set in .mat file format.
*	@note	Non-default values are omitted to keep output clean and maintainable.
**/
static void save_materials(const char* file_name, bool save_all, bool force)
{
	if (!force && FS_FileExistsEx(file_name, FS_TYPE_REAL))
	{
		Com_WPrintf("File '%s' already exists, add 'force' to overwrite\n", file_name);
		return;
	}

	qhandle_t file = 0;
	int err = FS_OpenFile(file_name, &file, FS_MODE_WRITE);
	
	if (err < 0 || !file)
	{
		Com_WPrintf("Cannot open file '%s' for writing: %s\n", file_name, Q_ErrorString(err));
		return;
	}

	uint32_t count = 0;

	for (uint32_t i = 0; i < MAX_PBR_MATERIALS; i++)
	{
		const pbr_material_t* mat = r_materials + i;
		
		if (!mat->registration_sequence)
			continue;

		// when save_all == false, only save auto-generated materials,
		// i.e. those without a source file
		if (!save_all && mat->source_matfile[0])
			continue;

		FS_FPrintf(file, "%s:\n", mat->name);
		
		if (mat->filename_base[0])
			FS_FPrintf(file, "\ttexture_base %s\n", mat->filename_base);
		
		if (mat->filename_normals[0])
			FS_FPrintf(file, "\ttexture_normals %s\n", mat->filename_normals);
		
		if (mat->filename_emissive[0])
			FS_FPrintf(file, "\ttexture_emissive %s\n", mat->filename_emissive);

		if (mat->filename_mask[0])
			FS_FPrintf(file, "\ttexture_mask %s\n", mat->filename_mask);
		
		if (mat->bump_scale != 1.f)
			FS_FPrintf(file, "\tbump_scale %f\n", mat->bump_scale);
		
		if (mat->roughness_override > 0.f)
			FS_FPrintf(file, "\troughness_override %f\n", mat->roughness_override);
		
		if (mat->metalness_factor != 1.f)
			FS_FPrintf(file, "\tmetalness_factor %f\n", mat->metalness_factor);
		
		if (mat->emissive_factor != 1.f)
			FS_FPrintf(file, "\temissive_factor %f\n", mat->emissive_factor);

		if (mat->specular_factor != 1.f)
			FS_FPrintf(file, "\tspecular_factor %f\n", mat->specular_factor);

		if (mat->base_factor != 1.f)
			FS_FPrintf(file, "\tbase_factor %f\n", mat->base_factor);

		if (!MAT_IsKind(mat->flags, MATERIAL_KIND_REGULAR)) {
			const char* kind = MAT_GetMaterialKindName(mat->flags);
			FS_FPrintf(file, "\tkind %s\n", kind ? kind : "");
		}
		
		if (mat->flags & MATERIAL_FLAG_LIGHT)
			FS_FPrintf(file, "\tis_light 1\n");
		
		if (!mat->light_styles)
			FS_FPrintf(file, "\tlight_styles 0\n");
		
		if (!mat->bsp_radiance)
			FS_FPrintf(file, "\tbsp_radiance 0\n");

		if (mat->default_radiance != 1.f)
			FS_FPrintf(file, "\tdefault_radiance %f\n", mat->default_radiance);

		if (mat->synth_emissive)
			FS_FPrintf(file, "\tsynth_emissive 1\n");

		if (mat->emissive_threshold != cvar_pt_surface_lights_threshold->integer)
			FS_FPrintf(file, "\temissive_threshold %d\n", mat->emissive_threshold);
		
		FS_FPrintf(file, "\n");
		
		++count;
	}

	FS_CloseFile(file);

	Com_Printf("saved %d materials\n", count);
}



/**
*
*
*
*	Map-Specific Material Management:
*
*
*
**/
/**
*	@brief	Loads map-specific material overrides and invalidates affected runtime materials.
*	@param	map_name	Map filename (with or without extension).
*	@note	Called when changing maps to apply map-specific material definitions.
*			1. Clears previous map materials
*			2. Loads new map's .mat file (maps/mapname.mat)
*			3. Invalidates all wall materials so they reload with new overrides
*	@note	Map materials override global materials for the same texture names.
*			This allows per-map customization without modifying global definitions.
*	@note	If any map materials exist (now or previously), ALL wall materials are
*			invalidated to ensure consistent application of overrides.
**/
void MAT_ChangeMap(const char* map_name)
{
	// Clear the old map-specific materials.
	uint32_t old_map_materials = num_map_materials;
	if (num_map_materials > 0) {
		memset(r_map_materials, 0, sizeof(pbr_material_t) * num_map_materials);
	}

	// Load the new map's material overrides.
	char map_name_no_ext[MAX_QPATH];
	truncate_extension(map_name, map_name_no_ext);
	char file_name[MAX_OSPATH];
	Q_snprintf(file_name, sizeof(file_name), "%s.mat", map_name_no_ext);
	num_map_materials = load_material_file(file_name, r_map_materials, MAX_PBR_MATERIALS);
	if (num_map_materials > 0) {	
		Com_Printf("Loaded %d materials from %s\n", num_map_materials, file_name);
	}

	// If there are any overrides now or there were some overrides before,
	// unload all wall materials to re-initialize them with the overrides.
	// This ensures that map-specific materials take effect consistently.
	if (old_map_materials > 0 || num_map_materials > 0)
	{
		for (uint32_t i = 0; i < MAX_PBR_MATERIALS; i++)
		{
			pbr_material_t* mat = r_materials + i;

			if (mat->registration_sequence && mat->image_type == IT_WALL)
			{
				// Remove the material from the hash table.
				List_Remove(&mat->entry);

				// Invalidate the material entry (will be recreated on next use).
				MAT_Reset(mat);
			}
		}
	}
}



/**
*
*
*
*	Material Image Loading:
*
*
*
**/
/**
*	@brief	Loads a material's texture image with fallback logic for overrides.
*	@param	image		Destination image pointer.
*	@param	filename	Texture path to load.
*	@param	mat			Material requesting the image (for flag inheritance).
*	@param	type		Image type (IT_WALL, IT_SKIN, etc.).
*	@param	flags		Image loading flags (IF_SRGB, IF_EXACT, etc.).
*	@note	Special handling for "overrides/" textures: tries exact match first,
*			then inexact match as fallback for cross-game compatibility.
**/

static void load_material_image(image_t** image, const char* filename, pbr_material_t* mat, imagetype_t type, imageflags_t flags)
{
	*image = IMG_Find(filename, type, flags | IF_EXACT | (mat->image_flags & IF_SRC_MASK));
	if (*image == R_NOTEXTURE) {
		/* Also try inexact loading in case of an override texture.
		 * Useful for games that ship a texture in a different directory
		 * (compared to the baseq2 location): the override is still picked
		 * up, but if the same path is written to a materials file, a
		 * warning is emitted, since overrides are in generally in baseq2. */
		if(strncmp(filename, "overrides/", 10) == 0)
			*image = IMG_Find(filename, type, flags | IF_EXACT);
	}
}

/**
*	@brief	Checks if a game-specific texture is identical to the baseq2 version.
*	@param	name	Texture file path.
*	@return	True if game and base versions are byte-for-byte identical, false otherwise.
*	@note	Some game mods (e.g., Rogue/Ground Zero) ship copies of baseq2 textures
*			that are identical to the originals. This function detects such cases
*			to allow baseq2 material definitions and overrides to apply.
*	@note	Compares file sizes first (fast), then performs byte-by-byte comparison
*			if sizes match. Returns false if files don't exist or differ.
*	@note	Used during material lookup to decide whether to use baseq2 or game-specific
*			material definitions when a texture exists in both directories.
**/
static qboolean game_image_identical_to_base(const char* name)
{
	/* Check if a game image is actually different from the base version,
	   as some games (eg rogue) ship image assets that are identical to the
	   baseq2 version.
	   If that is the case, ignore the game image, and just use everything
	   from baseq2, especially overrides/other images. */
	qboolean result = false;

	// Open both baseq2 and game versions of the file.
	qhandle_t base_file = -1, game_file = -1;
	if((FS_OpenFile(name, &base_file, FS_MODE_READ | FS_PATH_BASE | FS_BUF_NONE) >= 0)
		&& (FS_OpenFile(name, &game_file, FS_MODE_READ | FS_PATH_GAME | FS_BUF_NONE) >= 0))
	{
		// Quick check: compare file sizes first.
		int64_t base_len = FS_Length(base_file), game_len = FS_Length(game_file);
		if(base_len == game_len)
		{
			// Same size: perform byte-by-byte comparison.
			char *base_data = FS_Malloc(base_len);
			char *game_data = FS_Malloc(game_len);
			if(FS_Read(base_data, base_len, base_file) >= 0
				&& FS_Read(game_data, game_len, game_file) >= 0)
			{
				result = memcmp(base_data, game_data, base_len) == 0;
			}
			Z_Free(base_data);
			Z_Free(game_data);
		}
	}
	if (base_file >= 0)
		FS_CloseFile(base_file);
	if (game_file >= 0)
		FS_CloseFile(game_file);

	return result;
}

/**
*	@brief	Loads the base/diffuse texture for a material with fallback logic.
*	@param	mat		Material to load texture for.
*	@param	name	Original texture name from BSP/model.
*	@param	type	Image type (IT_WALL, IT_SKIN, etc.).
*	@param	flags	Image loading flags.
*	@note	Multi-step fallback logic:
*			1. If filename_base specified in .mat file, try loading that (HD texture)
*			2. If HD texture not found, fall back to original name (might be .wal)
*			3. If all fails, use R_NOTEXTURE
*	@note	Handles texture dimension detection:
*			- For .wal/.pcx: Uses IMG_GetDimensions to get original size
*			- For other formats (TGA/PNG/JPG): Uses actual image dimensions
*			- Supports maps compiled without .wal files (ericw-tools)
*	@note	original_width/original_height are critical for texture coordinate scaling
*			in the BSP renderer to maintain correct appearance.
**/
void MAT_FindBaseTexture( pbr_material_t *mat, const char *name, imagetype_t type, imageflags_t flags ) {
	char mat_name_no_ext[ MAX_QPATH ];
	truncate_extension( name, mat_name_no_ext );
	Q_strlwr( mat_name_no_ext );

	// WID: This will try and load the image that is specified, expecting a 'hi-res HD' texture.
	if ( mat->filename_base[ 0 ] ) {
		load_material_image( &mat->image_base, mat->filename_base, mat, type, flags | IF_SRGB );
		Q_strlcpy( mat->filename_base, mat->image_base->filepath, sizeof( mat->filename_base ) );
	} else {
		mat->image_base = R_NOTEXTURE;
	}

	// WID: If unable to find, try again but this time try and acquire its low-res '.wal' type.
	if ( mat->image_base == R_NOTEXTURE ) {
		//Com_WPrintf( "Hi-Res texture(\"%s\") specified in material(\"%s\") could not be found. Attempting the low-res texture.\n", mat->filename_base, mat_name_no_ext );
		mat->image_base = IMG_Find( name, type, flags | IF_SRGB );
		if ( mat->image_base == R_NOTEXTURE ) {
			Com_WPrintf( "Texture(\"%s\") in Material(\"%s\") could not be found. Resorting to R_NOTEXTURE\n", mat->filename_base, mat_name_no_ext );
			mat->image_base = R_NOTEXTURE;//NULL;
		} else {
			// Assign the image's filepath to the internally created material.
			Q_strlcpy( mat->filename_base, mat->image_base->filepath, sizeof( mat->filename_base ) );
			// Fetch its image's size.
			mat->original_width = mat->image_base->width;
			mat->original_height = mat->image_base->height;
		}
	} else {
		// WID: When this else statement hits, it may still have been a '.wal' texture that IS specified in the 'texture_base' property.
		// Now, IMG_GetDimensions only supports .wal and .pcx, so when the image is neither of the two, it won't return: Q_ERR_SUCCESS
		int foundDimension = IMG_GetDimensions( name, &mat->original_width, &mat->original_height );

		// WID: This allows support for maps that have been compiled without .wal textures using 'ericw-tools'.
		// We now know that the texinfo name is most definitely not a '.wal', in other words the BSP contains 
		// texinfo a path such as: textures/tests/01.tga
		//
		// This scenario leaves us with no '.wal' to derive any 'original' width and height from. 
		// Meaning that instead we'll just use the '.tga/.png/.jpg' image its own original width and height.
		if ( /*type == IT_WALL &&*/ foundDimension != Q_ERR_SUCCESS ) {
			if ( mat->image_base ) {
				mat->original_width = mat->image_base->width;
				mat->original_height = mat->image_base->height;
			}
			//mat->original_width = mat->image_base->upload_width;
			//mat->original_height = mat->image_base->upload_height;
			//mat->original_width = mat->image_base->upload_width = mat->image_base->width;
			//mat->original_height = mat->image_base->upload_height = mat->image_base->height;
		}
	}
}

/**
*	@brief	Finds or creates a material for the given texture name.
*	@param	name	Texture name (e.g., "textures/base.tga" or "textures/base").
*	@param	type	Image type (IT_WALL, IT_SKIN, IT_SPRITE, IT_PIC).
*	@param	flags	Image loading flags (IF_SRGB, IF_EXACT, etc.).
*	@return	Pointer to material (never NULL).
*	@note	Material lookup algorithm:
*			1. Check hash table for existing runtime material → return if found
*			2. Allocate new runtime material slot
*			3. Search for definition: check map materials → check global materials
*			4. If definition found: copy properties, load textures
*			5. If no definition: create auto-generated material with defaults
*			6. Load associated textures: base, normals (_n.tga), emissive (_light.tga), mask (_m.tga)
*			7. Synthesize emissive if synth_emissive flag enabled
*			8. Add to hash table for fast future lookups
*	@note	Material override priority:
*			1. Map-specific materials (highest priority)
*			2. Global materials
*			3. Auto-generated defaults (lowest priority)
*	@note	Game-specific asset handling:
*			- Detects if game overrides baseq2 assets with different content
*			- Ignores baseq2 material definitions for overridden game assets
*			- Allows game-specific materials to take precedence
*	@note	This function is called for every texture reference in the map and models.
*			Performance is critical - uses hash table for O(1) average-case lookup.
**/
pbr_material_t* MAT_Find(const char* name, imagetype_t type, imageflags_t flags)
{
	char mat_name_no_ext[MAX_QPATH];
	truncate_extension(name, mat_name_no_ext);
	Q_strlwr(mat_name_no_ext);

	uint32_t hash = Com_HashString(mat_name_no_ext, RMATERIALS_HASH);
	
	pbr_material_t* mat = find_material(mat_name_no_ext, hash, r_materials, MAX_PBR_MATERIALS);
	
	if (mat)
	{
		MAT_UpdateRegistration(mat);
		return mat;
	}

	// Find the default material in the parsed materials list.
	mat = allocate_material();
	pbr_material_t* matdef = find_material_sorted(mat_name_no_ext, r_global_materials, num_global_materials);
	
	// In case the map had a custom materials file, override the material definition with that found in there.
	if (type == IT_WALL)
	{
		pbr_material_t* map_mat = find_material_sorted(mat_name_no_ext, r_map_materials, num_map_materials);

		if (map_mat)
			matdef = map_mat;
	}

	/* Some games override baseq2 assets without changing the name -
	   e.g. 'action' replaces models/weapons/v_blast with something
	   looking completely differently.
	   Using the material definition from baseq2 makes things look wrong.
	   So try to detect if the game is overriding an image from baseq2
	   with a different image and, if that is the case, ignore the material
	   definition (if it's from baseq2 - to allow for a game-specific material
	   definition).
	   There's also the wrinkle that some games ship with a copy of an image
	   that is identical in baseq2 (see game_image_identical_to_base()),
	   in that case, _do_ use the material definition. */
	if (matdef
		&& (matdef->image_flags & IF_SRC_MASK) == IF_SRC_BASE
		&& is_game_custom()
		&& FS_FileExistsEx(name, FS_PATH_GAME) != 0
		&& !game_image_identical_to_base(name))
	{
		matdef = NULL;
		/* Forcing image to load from game prevents a normal or emissive map in baseq2
		 * from being picked up. */
		flags = (flags & ~IF_SRC_MASK) | IF_SRC_GAME;
	}
	// Path for when a material definition was found:
	if (matdef)
	{
		// Copy over the definition info into the current entry.
		memcpy(mat, matdef, sizeof(pbr_material_t));
		// Determine its index.
		uint32_t index = (uint32_t)(mat - r_materials);
		// Set index and next frame.
		mat->flags = (mat->flags & ~MATERIAL_INDEX_MASK) | index;
		mat->next_frame = index;
		
		// Load its base texture.
		if (mat->filename_base[0]) {
			MAT_FindBaseTexture( mat, name, type, flags );
		}

		if (mat->filename_normals[0]) {
			load_material_image(&mat->image_normals, mat->filename_normals, mat, type, flags);
			if (mat->image_normals == R_NOTEXTURE) {
				Com_WPrintf("Texture '%s' specified in material '%s' could not be found.\n", mat->filename_normals, mat_name_no_ext);
				mat->image_normals = NULL;
			}
		}
		
		if (mat->filename_emissive[0]) {
			load_material_image(&mat->image_emissive, mat->filename_emissive, mat, type, flags | IF_SRGB);
			if (mat->image_emissive == R_NOTEXTURE) {
				Com_WPrintf("Texture '%s' specified in material '%s' could not be found.\n", mat->filename_emissive, mat_name_no_ext);
				mat->image_emissive = NULL;
			}
		}
		
		if (mat->filename_mask[0]) {
			//load_material_image( mat->image_mask, mat->filename_mask, mat, type, flags | IF_EXACT | ( mat->image_flags & IF_SRC_MASK ) );
			mat->image_mask = IMG_Find(mat->filename_mask, type, flags | IF_EXACT | (mat->image_flags & IF_SRC_MASK));
			if (mat->image_mask == R_NOTEXTURE) {
				Com_WPrintf("Texture '%s' specified in material '%s' could not be found.\n", mat->filename_mask, mat_name_no_ext);
				mat->image_mask = NULL;
			}
		}
	// This path is taken when for whatever reason no proper material definitions were found.
	} else {
		// Create a fresh internal material for this material slot.
		MAT_Reset(mat);
		// Assign its internal material name.
		Q_strlcpy(mat->name, mat_name_no_ext, sizeof(mat->name));
		
		// <Q2RTXP>: Fixed materials not loading kindly with a lack of a .wall and/or material definition.
		#if 1
		MAT_FindBaseTexture( mat, name, type, flags );
		#else
		// WID: This doesn't work for animated materials that lack a .wal
		// it gives incorrect size results due to a lack of any better.
		mat->image_base = IMG_Find(name, type, flags | IF_SRGB);
		mat->original_width = mat->image_base->width;
		mat->original_height = mat->image_base->height;
		if (mat->image_base == R_NOTEXTURE)
			mat->image_base = NULL;
		else
			Q_strlcpy(mat->filename_base, mat->image_base->filepath, sizeof(mat->filename_base));
		#endif
		// Normal map:
		char file_name[MAX_QPATH];
		Q_snprintf(file_name, sizeof(file_name), "%s_n.tga", mat_name_no_ext);
		mat->image_normals = IMG_Find(file_name, type, flags);
		if ( mat->image_normals == R_NOTEXTURE ) {
			mat->image_normals = NULL;
		} else {
			Q_strlcpy( mat->filename_normals, mat->image_normals->filepath, sizeof( mat->filename_normals ) );
		}
		// Emissive Map:
		Q_snprintf(file_name, sizeof(file_name), "%s_light.tga", mat_name_no_ext);
		mat->image_emissive = IMG_Find(file_name, type, flags | IF_SRGB);
		if ( mat->image_emissive == R_NOTEXTURE ) {
			mat->image_emissive = NULL;
		} else {
			Q_strlcpy( mat->filename_emissive, mat->image_emissive->filepath, sizeof( mat->filename_emissive ) );
		}
		// Mask Map:
		Q_snprintf( file_name, sizeof( file_name ), "%s_m.tga", mat_name_no_ext );
		mat->image_mask = IMG_Find( file_name, type, flags | IF_SRGB );
		if ( mat->image_mask == R_NOTEXTURE ) {
			mat->image_mask = NULL;
		} else {
			Q_strlcpy( mat->filename_mask, mat->image_mask->filepath, sizeof( mat->filename_mask ) );
		}
		// If there is no normals/metalness image, assume that the material is a basic diffuse one.
		if ( !mat->image_normals ) {
			mat->specular_factor = 0.f;
			mat->metalness_factor = 0.f;
		}
	}

	if ( mat->synth_emissive && !mat->image_emissive ) {
		MAT_SynthesizeEmissive( mat );
	}

	if ( mat->image_normals ) {
		mat->image_normals->flags |= IF_NORMAL_MAP;
	}
	if ( mat->image_emissive && !mat->image_emissive->processing_complete ) {
		vkpt_extract_emissive_texture_info( mat->image_emissive );
	}

	mat->image_type = type;
	mat->image_flags |= flags;

	MAT_SetIndex( mat );
	MAT_UpdateRegistration( mat );

	List_Append( &r_materialsHash[ hash ], &mat->entry );

	return mat;
}



/**
*
*
*
*	Material Registration and Cleanup:
*
*
*
**/
/**
*	@brief	Updates the registration sequence for a material and all its textures.
*	@param	mat	Material to update (can be NULL).
*	@note	The registration sequence is used to track which materials are actively
*			used in the current frame/map. Materials with outdated registration
*			sequences can be freed during cleanup.
*	@note	Also updates registration for all associated images (base, normals,
*			emissive, mask) to prevent premature texture freeing.
**/
void MAT_UpdateRegistration(pbr_material_t * mat)
{
	if (!mat)
		return;

	mat->registration_sequence = registration_sequence;
	if (mat->image_base) mat->image_base->registration_sequence = registration_sequence;
	if (mat->image_normals) mat->image_normals->registration_sequence = registration_sequence;
	if (mat->image_emissive) mat->image_emissive->registration_sequence = registration_sequence;
	if (mat->image_mask) mat->image_mask->registration_sequence = registration_sequence;
}

/**
*	@brief	Frees materials that are no longer in use.
*	@return	Q_ERR_SUCCESS.
*	@note	Called periodically to clean up materials from previous maps or unused textures.
*			Frees materials whose registration_sequence doesn't match current sequence.
*			Skips materials marked with IF_PERMANENT flag.
*			Removes freed materials from hash table and resets their slots.
**/
int MAT_FreeUnused()
{
	for (uint32_t i = 0; i < MAX_PBR_MATERIALS; ++i)
	{
		pbr_material_t * mat = r_materials + i;

		if (mat->registration_sequence == registration_sequence)
			continue;

		if (!mat->registration_sequence)
			continue;

		if (mat->image_flags & IF_PERMANENT)
			continue;

		// delete the material from the hash table
		List_Remove(&mat->entry);

		MAT_Reset(mat);
	}

	return Q_ERR_SUCCESS;
}

/**
*	@brief	Retrieves a material by its index.
*	@param	index	Material index (0 to MAX_PBR_MATERIALS-1).
*	@return	Pointer to material if valid and registered, NULL otherwise.
*	@note	Used to convert material indices (embedded in flags) back to pointers.
*			Returns NULL if index is out of range or material is not registered.
**/
pbr_material_t* MAT_ForIndex(int index)
{
	if (index < 0 || index >= MAX_PBR_MATERIALS)
		return NULL;

	pbr_material_t* mat = r_materials + index;
	if (mat->registration_sequence == 0)
		return NULL;

	return mat;
}

/**
*	@brief	Finds or creates a material for a model skin texture.
*	@param	image_base	Base texture image for the skin.
*	@return	Pointer to material for this skin (never NULL).
*	@note	Called when rendering models to get the material for a skin texture.
*			If the material already uses this skin image, returns it immediately.
*			Otherwise, updates the material's base image and registration sequence.
*	@note	Also updates registration sequence for all related textures (normals,
*			emissive, mask) to keep them in sync with the skin.
**/
pbr_material_t* MAT_ForSkin(image_t* image_base)
{
	// find the material
	pbr_material_t* mat = MAT_Find(image_base->name, IT_SKIN, IF_NONE);
	assert(mat);

	// if it's already using this skin, do nothing
	if (mat->image_base == image_base)
		return mat;
	
	mat->image_base = image_base;

	// update registration sequence of material and its textures
	int rseq = image_base->registration_sequence;

	if (mat->image_normals)
		mat->image_normals->registration_sequence = rseq;

	if (mat->image_emissive)
		mat->image_emissive->registration_sequence = rseq;

	if ( mat->image_mask ) {
		mat->image_mask->registration_sequence = rseq;
	}

	mat->registration_sequence = rseq;

	return mat;
}



/**
*
*
*
*	Material Debug and Console Interface:
*
*
*
**/
/**
*	@brief	Prints all properties of a material to the console.
*	@param	mat	Material to print.
*	@note	Used by the "mat print" console command for debugging.
*			Displays all material attributes in .mat file format.
**/
void MAT_Print(pbr_material_t const * mat)
{
	Com_Printf("%s:\n", mat->name);
	Com_Printf("    texture_base %s\n", mat->filename_base);
	Com_Printf("    texture_normals %s\n", mat->filename_normals);
	Com_Printf("    texture_emissive %s\n", mat->filename_emissive);
	Com_Printf("    texture_mask %s\n", mat->filename_mask);
	Com_Printf("    bump_scale %f\n", mat->bump_scale);
	Com_Printf("    roughness_override %f\n", mat->roughness_override);
	Com_Printf("    metalness_factor %f\n", mat->metalness_factor);
	Com_Printf("    emissive_factor %f\n", mat->emissive_factor);
	Com_Printf("    specular_factor %f\n", mat->specular_factor);
	Com_Printf("    base_factor %f\n", mat->base_factor);
	const char * kind = MAT_GetMaterialKindName(mat->flags);
	Com_Printf("    kind %s\n", kind ? kind : "");
	Com_Printf("    is_light %d\n", (mat->flags & MATERIAL_FLAG_LIGHT) != 0);
	Com_Printf("    light_styles %d\n", mat->light_styles ? 1 : 0);
	Com_Printf("    bsp_radiance %d\n", mat->bsp_radiance ? 1 : 0);
	Com_Printf("    default_radiance %f\n", mat->default_radiance);
	Com_Printf("    synth_emissive %d\n", mat->synth_emissive ? 1 : 0);
	Com_Printf("    emissive_threshold %d\n", mat->emissive_threshold);
}

/**
*	@brief	Prints help text for the "mat" console command.
*	@note	Lists all available subcommands and their usage.
**/
static void material_command_help(void)
{
	Com_Printf("mat command - interface to the material system\n");
	Com_Printf("usage: mat <command> <arguments...>\n");
	Com_Printf("available commands:\n");
	Com_Printf("    help: print this message\n");
	Com_Printf("    print: print the current material, i.e. one at the crosshair\n");
	Com_Printf("    which: tell where the current material is defined\n");
	Com_Printf("    save <filename> <options>: save the active materials to a file\n");
	Com_Printf("        option 'all': save all materials (otherwise only the undefined ones)\n");
	Com_Printf("        option 'force': overwrite the output file if it exists\n");
	Com_Printf("    <attribute> <value>: set an attribute of the current material\n");
	Com_Printf("        use 'mat print' to list the available attributes\n");
}

/**
*	@brief	Console command handler for "mat" command.
*	@note	Provides runtime material editing and debugging capabilities:
*			- "mat help": Print help text
*			- "mat print": Display properties of material at crosshair
*			- "mat which": Show source file and line number for material definition
*			- "mat save <file> [all] [force]": Export materials to .mat file
*			- "mat <attribute> <value>": Modify material attribute at crosshair
*	@note	Some attribute changes trigger map reload (kind, is_light, masks).
*			Some trigger emissive regeneration (synth_emissive, emissive_threshold).
*	@note	Material at crosshair is identified via vkpt_refdef.fd->feedback.view_material_index.
**/
static void material_command(void)
{
	if (Cmd_Argc() < 2)
	{
		material_command_help();
		return;
	}

	const char* key = Cmd_Argv(1);

	if (strcmp(key, "help") == 0)
	{
		material_command_help();
		return;
	}

	if (strcmp(key, "save") == 0)
	{
		if (Cmd_Argc() < 3)
		{
			Com_Printf("expected file name\n");
			return;
		}
		
		const char* file_name = Cmd_Argv(2);

		bool save_all = false;
		bool force = false;
		for (int i = 3; i < Cmd_Argc(); i++)
		{
			if (strcmp(Cmd_Argv(i), "all") == 0)
				save_all = true;
			else if (strcmp(Cmd_Argv(i), "force") == 0)
				force = true;
			else {
				Com_Printf("unrecognized argument: %s\n", Cmd_Argv(i));
				return;
			}
		}

		save_materials(file_name, save_all, force);
		return;
	}
	
	pbr_material_t* mat = NULL;

	if (vkpt_refdef.fd)
		mat = MAT_ForIndex(vkpt_refdef.fd->feedback.view_material_index);

	if (!mat)
		return;

	if (strcmp(key, "print") == 0)
	{
		MAT_Print(mat);
		return;
	}
	
	if (strcmp(key, "which") == 0)
	{
		Com_Printf("%s: ", mat->name);
		if (mat->source_matfile[0])
			Com_Printf("%s line %d\n", mat->source_matfile, mat->source_line);
		else
			Com_Printf("automatically generated\n");
		return;
	}

	if (Cmd_Argc() < 3)
	{
		Com_Printf("expected value for attribute\n");
		return;
	}

	unsigned int reload_flags = 0;
	set_material_attribute(mat, key, Cmd_Argv(2), NULL, 0, &reload_flags);

	if ((reload_flags & RELOAD_EMISSIVE) != 0)
	{
		if(mat->image_emissive && strstr(mat->image_emissive->name, "*E"))
		{
			// Prevent previous image being reused to allow emissive_threshold changes
			mat->image_emissive->name[0] = 0;
			mat->image_emissive = NULL;
		}
		if(mat->synth_emissive && !mat->image_emissive)
		{
			// Regenerate emissive image
			MAT_SynthesizeEmissive(mat);

			// In some cases, MAT_SynthesizeEmissive might not create an emissive image - test for that
			if (mat->image_emissive)
			{
				// Make sure it's loaded by CL_PrepRefresh()
				IMG_Load(mat->image_emissive, mat->image_emissive->pix_data);
			}

			reload_flags |= RELOAD_MAP;
		}
	}

	if ((reload_flags & RELOAD_MAP) != 0)
	{
		// Trigger a re-upload and rebuild of the models that use this material.
		// Reason to rebuild: some material changes result in meshes being classified as
		// transparent or masked, which affects the static model BLAS.
		vkpt_vertex_buffer_invalidate_static_model_vbos(vkpt_refdef.fd->feedback.view_material_index);

		// Reload the map and necessary models.
		CL_PrepRefresh();
	}
}

/**
*	@brief	Tab-completion handler for "mat" console command.
*	@param	ctx		Completion context.
*	@param	argnum	Current argument number being completed.
*	@note	Provides intelligent completions:
*			- Argument 1: Command names and attribute names
*			- Argument 2: Context-specific options (save options, kind values, boolean values)
*	@note	For "kind" attribute, suggests lowercase material kind names.
*			For boolean attributes, suggests "0" and "1".
**/
static void material_completer(genctx_t* ctx, int argnum)
{
	if (argnum == 1) {
		// sub-commands
		
		Prompt_AddMatch(ctx, "print");
		Prompt_AddMatch(ctx, "save");
		Prompt_AddMatch(ctx, "which");

		for (int i = 0; i < c_NumAttributes; i++)
			Prompt_AddMatch(ctx, c_Attributes[i].name);
	}
	else if (argnum == 2)
	{
		if(strcmp(Cmd_Argv(1), "save") == 0)
		{
			// extra arguments for 'save'
			Prompt_AddMatch(ctx, "all");
			Prompt_AddMatch(ctx, "force");
		}
		else if((strcmp(Cmd_Argv(1), "print") == 0) || (strcmp(Cmd_Argv(1), "which") == 0))
		{
			// Nothing to complete for these
		}
		else
		{
			// Material property completion
			struct MaterialAttribute const* t = NULL;
			for (int i = 0; i < c_NumAttributes; ++i)
			{
				if (strcmp(Cmd_Argv(1), c_Attributes[i].name) == 0)
				{
					t = &c_Attributes[i];
					break;
				}
			}

			if(!t)
				return;

			// Attribute-specific completions
			switch(t->index)
			{
			case MAT_KIND:
				for (int i = 0; i < nMaterialKinds; ++i)
				{
					// Use lower-case for completion, that is what you'd typically type
					char kind[32];
					Q_strlcpy(kind, materialKinds[i].name, sizeof(kind));
					Q_strlwr(kind);
					Prompt_AddMatch(ctx, kind);
				}
				return;
			default:
				break;
			}

			// Type-specific completions
			if(t->type == ATTR_BOOL)
			{
				Prompt_AddMatch(ctx, "0");
				Prompt_AddMatch(ctx, "1");
				return;
			}
		}
	}
}



/**
*
*
*
*	Material Kind Helpers:
*
*
*
**/
/**
*	@brief	Sets the material kind bits in a material flags value.
*	@param	material	Current material flags.
*	@param	kind		New material kind to set.
*	@return	Updated material flags with new kind.
*	@note	Preserves all non-kind flag bits.
**/
uint32_t MAT_SetKind(uint32_t material, uint32_t kind)
{
	return (material & ~MATERIAL_KIND_MASK) | kind;
}

/**
*	@brief	Checks if a material is of a specific kind.
*	@param	material	Material flags to test.
*	@param	kind		Material kind to check for.
*	@return	True if material matches the specified kind, false otherwise.
**/
bool MAT_IsKind(uint32_t material, uint32_t kind)
{
	return (material & MATERIAL_KIND_MASK) == kind;
}



/**
*
*
*
*	Emissive Texture Synthesis:
*
*
*
**/
/**
*	@brief	Generates a fake emissive texture from the diffuse texture.
*	@param	diffuse				Base diffuse texture to analyze.
*	@param	bright_threshold_int	Brightness threshold (0-255) for emissive detection.
*	@return	Emissive texture image, or NULL if generation disabled.
*	@note	Algorithm selection via cvar_pt_surface_lights_fake_emissive_algo:
*			- 0: Use diffuse texture directly (full image is emissive)
*			- 1: Generate filtered mask of bright pixels only
*			- Other: Disable synthetic emissive
*	@note	Used to automatically create emissive maps for bright surfaces without
*			explicit emissive textures (e.g., light panels, glowing signs).
**/
static image_t* get_fake_emissive_image(image_t* diffuse, int bright_threshold_int)
{
	switch(cvar_pt_surface_lights_fake_emissive_algo->integer)
	{
	case 0:
		// Use diffuse directly: entire surface glows with diffuse colors.
		return diffuse;
	case 1:
		// Generate bright-pixel mask: only bright areas glow.
		return vkpt_fake_emissive_texture(diffuse, bright_threshold_int);
	default:
		// Disabled: no synthetic emissive.
		return NULL;
	}
}

/**
*	@brief	Creates a synthetic emissive texture for a material if needed.
*	@param	mat	Material to synthesize emissive for.
*	@note	Only creates emissive if mat->image_emissive is NULL.
*			Sets mat->synth_emissive flag to indicate synthetic origin.
*			Extracts emissive texture metadata for lighting calculations.
*	@note	Used for materials with synth_emissive flag enabled but no explicit
*			emissive texture. Common for bright surfaces like lights and monitors.
**/
void MAT_SynthesizeEmissive(pbr_material_t * mat)
{
	if (!mat->image_emissive) {
		mat->image_emissive = get_fake_emissive_image(mat->image_base, mat->emissive_threshold);
		mat->synth_emissive = true;

		if (mat->image_emissive) {
			vkpt_extract_emissive_texture_info(mat->image_emissive);
		}
	}
}

/**
*	@brief	Checks if a material kind represents a transparent surface.
*	@param	material	Material flags to test.
*	@return	True if material is transparent (slime, water, glass, etc.).
*	@note	Transparent materials require special rendering (alpha blending, refraction).
*			Used to classify geometry for rendering pipeline selection.
**/
bool MAT_IsTransparent(uint32_t material)
{
	return MAT_IsKind(material, MATERIAL_KIND_SLIME)
		|| MAT_IsKind(material, MATERIAL_KIND_WATER)
		|| MAT_IsKind(material, MATERIAL_KIND_GLASS)
		|| MAT_IsKind(material, MATERIAL_KIND_TRANSPARENT)
		|| MAT_IsKind(material, MATERIAL_KIND_TRANSP_MODEL);
}

/**
*	@brief	Checks if a material uses alpha masking (cutout transparency).
*	@param	material	Material flags/index to test.
*	@return	True if material has a mask texture assigned.
*	@note	Masked materials use binary transparency (fully opaque or fully transparent).
*			Different from alpha-blended transparency (smooth transitions).
*	@note	Used for foliage, chain-link fences, and other cutout geometry.
**/
bool MAT_IsMasked(uint32_t material)
{
	const pbr_material_t* mat = MAT_ForIndex((int)(material & MATERIAL_INDEX_MASK));

	return mat && mat->image_mask;
}
