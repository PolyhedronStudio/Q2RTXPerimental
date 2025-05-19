/********************************************************************
*
*
*	ServerGame: Entity 'func_areaportal'.
*
*
********************************************************************/
#include "svgame/svg_local.h"

#include "svgame/entities/func/svg_func_entities.h"
#include "svgame/entities/func/svg_func_areaportal.h"



/**
*   @brief  Use Callback: Toggles the areaportal's state.
**/
DEFINE_MEMBER_CALLBACK_USE( svg_func_areaportal_t, onUse )( svg_func_areaportal_t *self, svg_base_edict_t *other, svg_base_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue ) -> void {
    
    // Get areaportal state.
    int32_t areaPortalState = gi.GetAreaPortalState( self->style );
    // Toggle bit.
    areaPortalState ^= 1;
    //ent->count ^= 1;        // toggle state
    //  gi.dprintf ("portalstate: %i = %i\n", ent->style, ent->count);
    
    // Set new state.
    gi.SetAreaPortalState( self->style, areaPortalState );
}

/*QUAKED func_areaportal (0 0 0) ?
This is a non-visible object that divides the world into seperated when this portal is not activated.
Usually enclosed in the middle of a door.
*/
DEFINE_MEMBER_CALLBACK_SPAWN( svg_func_areaportal_t, onSpawn )( svg_func_areaportal_t *ent ) -> void {
    // Entity type.
    ent->s.entityType = ET_AREA_PORTAL;
    // Use Callback for triggering.
    ent->SetUseCallback( &SelfType::onUse );
    // Always start closed;
    gi.SetAreaPortalState( ent->style, 0 );
    //ent->count = 0; // gi.GetAreaPortalState( ent->style );     
}
