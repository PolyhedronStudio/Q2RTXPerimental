/********************************************************************
*
*
*    ServerGame: Nav3 Core Types
*
*
********************************************************************/
#pragma once

#include "svgame/svg_local.h"


/**
*
*
*    Nav3 Runtime Enumerations:
*
*
**/
/**
*    @brief  Stable publication state for the nav3 runtime owner.
*    @note   Stage 1 keeps this intentionally small so status commands can report bounded state
*            without exposing future asset internals too early.
**/
enum class nav3_runtime_publish_state_t : uint8_t {
	Empty = 0,
	StubPending,
	Published,
	Count
};


/**
*
*
*    Nav3 Runtime Data Structures:
*
*
**/
/**
*    @brief  Compact runtime status summary used by server command diagnostics.
*    @note   This struct is intentionally pointer-free and bounded so stage-level status commands
*            can print deterministic output.
**/
struct nav3_runtime_status_t {
	//! True when nav3 runtime initialization has completed.
	bool initialized = false;
	//! True when `s_nav3_enable` currently enables nav3 backend routing.
	bool enabled = false;
	//! True when nav3 debug drawing is globally enabled.
	bool debug_enabled = false;
	//! True when a nav3 mesh asset is currently published.
	bool has_mesh = false;
	//! Number of generated sparse columns currently published in runtime memory.
	uint32_t generated_column_count = 0;
	//! Number of generated spans currently published in runtime memory.
	uint32_t generated_span_count = 0;
	//! Deterministic checksum for currently published generated mesh payload bytes.
	uint32_t generated_mesh_checksum = 0;
	//! Runtime generation stamp incremented by bounded stage commands.
	uint32_t runtime_generation = 0;
	//! Published mesh generation stamp.
	uint32_t mesh_generation = 0;
	//! Current publication state name for diagnostics.
	nav3_runtime_publish_state_t publish_state = nav3_runtime_publish_state_t::Empty;
};


/**
*
*
*    Nav3 Runtime Type Helpers:
*
*
**/
/**
*    @brief  Return a readable publication-state label.
*    @param  publishState  State to convert.
*    @return Stable string label for diagnostics.
**/
const char *SVG_Nav3_RuntimePublishStateName( const nav3_runtime_publish_state_t publishState );
