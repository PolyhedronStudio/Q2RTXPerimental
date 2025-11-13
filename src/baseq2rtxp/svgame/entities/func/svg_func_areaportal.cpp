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
    
 //   // Get current areaportal state and toggle it.
 //   int32_t areaPortalState = gi.GetAreaPortalState( self->style );
 //   areaPortalState ^= 1;
 //   // Persist new state in entity and notify collision model.
 //   self->count = areaPortalState;// ( useValue != false ? 1 : 0 );
 //   gi.SetAreaPortalState( self->style, self->count );
	//gi.linkentity( self );
    
    self->count ^= 1;
    gi.SetAreaPortalState( self->style, self->count );

    //gi.dprintf( "portalstate(#%i) = \"%s\"\n", self->style, ( self->count ? "isOpen" : "isClosed" ) );
                // Developer debug to help track intermittent map-specific failures.
    gi.dprintf( "%s: SetAreaPortal targetname(\"%s\"), portalID(#%d), isOpen=(%d)\n",
        __func__, ( self->targetname ? (const char *)self->targetname : "<null>" ), self->style, (int)self->count );
}

/*QUAKED func_areaportal (0 0 0) ?
This is a non-visible object that divides the world into seperated when this portal is not activated.
Usually enclosed in the middle of a door.
*/
DEFINE_MEMBER_CALLBACK_SPAWN( svg_func_areaportal_t, onSpawn )( svg_func_areaportal_t *self ) -> void {
	// Make sure to super spawn it.
    Super::onSpawn( self );

    // Entity type.
    self->s.entityType = ET_AREA_PORTAL;
    // Use Callback for triggering.
    self->SetUseCallback( &SelfType::onUse );
    // Ensure entity count mirrors portal state.
	self->count = 0; // Always start closed.
	// Link entity so the server knows about it.
    //gi.linkentity( self );

    // Always start closed;
    //gi.SetAreaPortalState( ent->style, 0 );
    // Used for area state.
    //ent->count = gi.GetAreaPortalState( ent->style );     
}
