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
    
	if ( useType == ENTITY_USETARGET_TYPE_SET ) {
		self->count = useValue;

		if ( svg_debug_areaportals->integer ) {
			gi.dprintf( "%s: ENTITY_USETARGET_TYPE_SET targetname(\"%s\"), portalID(#%d), isOpen=(%d)\n",
				__func__, ( self->targetname ? ( const char * )self->targetname : "<null>" ), self->style, ( int )self->count );
		}
	} else if ( useType == ENTITY_USETARGET_TYPE_ON ) {
		self->count = 1;
		if ( svg_debug_areaportals->integer ) {
			gi.dprintf( "%s: ENTITY_USETARGET_TYPE_ON targetname(\"%s\"), portalID(#%d), isOpen=(%d)\n",
				__func__, ( self->targetname ? ( const char * )self->targetname : "<null>" ), self->style, ( int )self->count );
		}
	} else if ( useType == ENTITY_USETARGET_TYPE_OFF ) {
		self->count = 0;
		if ( svg_debug_areaportals->integer ) {
			gi.dprintf( "%s: ENTITY_USETARGET_TYPE_OFF targetname(\"%s\"), portalID(#%d), isOpen=(%d)\n",
				__func__, ( self->targetname ? ( const char * )self->targetname : "<null>" ), self->style, ( int )self->count );
		}
	} else if ( useType == ENTITY_USETARGET_TYPE_TOGGLE ) {
		self->count ^= 1;
		if ( svg_debug_areaportals->integer ) {
			gi.dprintf( "%s: ENTITY_USETARGET_TYPE_TOGGLE targetname(\"%s\"), portalID(#%d), isOpen=(%d)\n",
				__func__, ( self->targetname ? ( const char * )self->targetname : "<null>" ), self->style, ( int )self->count );
		}
	}
		//} else {
	//	self->count ^= 1;
	//	gi.dprintf( "%s: Default UseTarget Path targetname(\"%s\"), portalID(#%d), isOpen=(%d)\n",
	//		__func__, ( self->targetname ? ( const char * )self->targetname : "<null>" ), self->style, ( int )self->count );
	//}

	// Set the area portal state in the collision model.
    gi.SetAreaPortalState( self->style, self->count );
	// Re-link entity so server/client snapshots reflect the change.
	gi.linkentity( self );

    //gi.dprintf( "portalstate(#%i) = \"%s\"\n", self->style, ( self->count ? "isOpen" : "isClosed" ) );
                // Developer debug to help track intermittent map-specific failures.

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
	// (Doors and/or other trigger entities will use this to open/close the portal.)
    self->SetUseCallback( &SelfType::onUse );
	
	// Temporarily set to opened so linking works correctly.
	self->count = 1;
	// Link entity so the server knows about it.
	gi.linkentity( self );

	// Now Ensure entity count mirrors actual portal state.
	self->count = gi.GetAreaPortalState( self->style ); // Always start opened unless specified otherwise.

	if ( svg_debug_areaportals->integer ) {
		// Notify about spawn.
		gi.dprintf( "%s: Spawned func_areaportal targetname(\"%s\"), portalID(#%d), isOpen=(%d)\n",
			__func__, ( self->targetname ? ( const char * )self->targetname : "<null>" ), self->style, ( int )self->count );
	}
}
