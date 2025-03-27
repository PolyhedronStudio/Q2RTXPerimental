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
svg_entity_t *SVG_PickTarget( const char *targetname );
/**
*   @brief
**/
void SVG_UseTargets( svg_entity_t *ent, svg_entity_t *activator, const entity_usetarget_type_t useType = entity_usetarget_type_t::ENTITY_USETARGET_TYPE_TOGGLE, const int32_t useValue = 0 );