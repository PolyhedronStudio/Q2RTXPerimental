#pragma once

#include "nav_core.h"

// Saves the current navmesh to a .nav6 file
bool Nav_Save(const char* filepath);

// Loads a navmesh from a .nav6 file
bool Nav_Load(const char* filepath);
