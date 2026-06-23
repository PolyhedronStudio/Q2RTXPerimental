#pragma once

#include "svgame/svg_local.h"

// Constants and definitions for the KD-Tree NavMesh

#define NAV7_MAGIC 'NAV7'
#define NAV7_VERSION 1

// Maximum limits for initial fixed sizing (these can be expanded or turned into dynamic lists later if needed)
constexpr int32_t MAX_NAV_FACES = 262144;
constexpr int32_t MAX_NAV_HALFEDGES = 1048576;
constexpr int32_t MAX_NAV_VERTICES = 524288;
constexpr int32_t MAX_NAV_KDTREE_NODES = 524288;
constexpr int32_t MAX_NAV_LEAF_LINKS = 16384;

// The maximum step height (quake units) that AI can step up natively.
constexpr float NAV_MAX_STEP_HEIGHT = 16.0f;

// The minimum Z normal for a plane to be considered walkable.
constexpr float NAV_MIN_WALKABLE_Z = 0.65f;
