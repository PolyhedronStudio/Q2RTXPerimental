/********************************************************************
*
*
*	ServerGame: Target System Functions:
*
*
********************************************************************/
#pragma once



/**
*   @brief
**/
svg_base_edict_t *SVG_PickTarget( const char *targetname );
/**
*   @brief
**/
void SVG_UseTargets( svg_base_edict_t *ent, svg_base_edict_t *activator, const entity_usetarget_type_t useType = entity_usetarget_type_t::ENTITY_USETARGET_TYPE_TOGGLE, const int32_t useValue = 0 );