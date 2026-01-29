/********************************************************************
*
*
*	SVGame: Navigation Generation Internal Helpers
*
*	Shared helper functions used across generation translation units.
*
*
********************************************************************/
#pragma once

#include "svgame/nav/svg_nav.h"

/**
*	@brief	Collects all active entities that reference an inline BSP model ("*N").
*			The returned mapping is keyed by inline model index (N).
*	@note	If multiple entities reference the same "*N", the first one encountered is kept.
**/
void Nav_CollectInlineModelEntities( std::unordered_map<int32_t, svg_base_edict_t *> &out_model_to_ent );
