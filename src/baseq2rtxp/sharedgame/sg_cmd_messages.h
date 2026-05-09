/********************************************************************
*
*
*	SharedGame: Command Messages
*
*
********************************************************************/
#pragma once

#include <cstdint>

// protocol bytes that can be directly added to messages
//#define svc_muzzleflash     1
//#define svc_muzzleflash2    2
//#define svc_temp_entity     3
//#define svc_layout          4
//#define svc_inventory       5
//#define svc_stufftext       11
//! Debug draw stream protocol version.
static constexpr int32_t SG_SVC_DEBUG_DRAW_VERSION = 1;
//! Client userinfo cvar key used to opt into debug-draw streaming.
static constexpr const char *SG_SVC_DEBUG_DRAW_CLIENT_CVAR_NAME = "clg_nav3_debug_draw";
//! Bounded text label storage per debug draw primitive.
static constexpr int32_t SG_SVC_DEBUG_DRAW_MAX_LABEL_CHARS = 96;

/**
*\t@brief\tWire-level primitive kinds used by `svc_debug_draw`.
**/
enum class sg_svc_debug_draw_primitive_type_t : uint8_t {
	Line = 0,
	Aabb,
	Obb,
	Sphere,
	Arrow,
	Text,
	Count
};

/**
*\t@brief\tStyle flags streamed per primitive.
**/
enum sg_svc_debug_draw_style_flags_t : uint16_t {
	SG_SVC_DEBUG_DRAW_STYLE_FLAG_NONE = 0,
	SG_SVC_DEBUG_DRAW_STYLE_FLAG_DEPTH_TEST = 1 << 0,
	SG_SVC_DEBUG_DRAW_STYLE_FLAG_OUTLINE = 1 << 1,
};

//! Debug-draw command streamed from svgame to opted-in clients.
static constexpr int32_t svc_debug_draw = svc_svgame;
//! Damage indicator command.
static constexpr int32_t svc_damage = svc_svgame + 1;