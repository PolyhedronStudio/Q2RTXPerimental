#pragma once

#include "nav_core.h"

/**
* @brief Save the current navmesh to disk.
* @param filepath Absolute or map-relative path to the destination file.
* @return True when the navmesh was written successfully.
**/
bool Nav_Save( const char *filepath );

/**
* @brief Load a navmesh from disk.
* @note The loader rejects stale caches when the format version changes.
* @param filepath Absolute or map-relative path to the source file.
* @return True when the navmesh was loaded successfully.
**/
bool Nav_Load( const char *filepath );
