#pragma once

#include "svgame/svg_local.h"

// Constants and definitions for the KD-Tree NavMesh

#define NAV6_MAGIC 'NAV6'
#define NAV6_VERSION 1

// Maximum limits for initial fixed sizing (these can be expanded or turned into dynamic lists later if needed)
constexpr int32_t MAX_NAV_POLYS = 32768;
constexpr int32_t MAX_NAV_KDTREE_NODES = 65536;
constexpr int32_t MAX_NAV_LEAF_LINKS = 16384;

// The maximum step height (quake units) that AI can step up natively.
constexpr float NAV_MAX_STEP_HEIGHT = 16.0f;

// The minimum Z normal for a plane to be considered walkable.
constexpr float NAV_MIN_WALKABLE_Z = 0.7f;
