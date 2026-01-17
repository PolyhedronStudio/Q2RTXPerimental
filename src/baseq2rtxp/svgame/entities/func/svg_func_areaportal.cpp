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
	// Const targetname.
	const char *targetName = ( self->targetname ? ( const char * )self->targetname : "<null>" );
	
	// Reference-counted model:
	// - `self->count` is authoritative refcount (0..N)
	// - collision model only stores open/closed (count > 0)
	if ( useType == ENTITY_USETARGET_TYPE_SET ) {
		self->count = ( useValue > 0 ? useValue : 0 );

		if ( svg_debug_areaportals->integer ) {
			gi.dprintf( "%s: %s targetname(\"%s\"), portalID(#%d), count=(%d)\n",
				__func__, _STR( ENTITY_USETARGET_TYPE_SET ), targetName, self->style, ( int32_t )self->count );
		}
	} else if ( useType == ENTITY_USETARGET_TYPE_ON ) {
		self->count += 1;
		if ( self->count < 0 ) {
			self->count = 1;
		}

		if ( svg_debug_areaportals->integer ) {
			gi.dprintf( "%s: %s targetname(\"%s\"), portalID(#%d), count=(%d)\n",
				__func__, _STR( ENTITY_USETARGET_TYPE_ON ), targetName, self->style, ( int32_t )self->count );
		}
	} else if ( useType == ENTITY_USETARGET_TYPE_OFF ) {
		self->count -= 1;
		if ( self->count <= 0 ) {
			self->count = 0;
		}

		if ( svg_debug_areaportals->integer ) {
			gi.dprintf( "%s: %s targetname(\"%s\"), portalID(#%d), count=(%d)\n",
				__func__, _STR( ENTITY_USETARGET_TYPE_OFF ), targetName, self->style, ( int32_t )self->count );
		}
	} else if ( useType == ENTITY_USETARGET_TYPE_TOGGLE ) {
		self->count = ( self->count <= 0 ? 1 : 0 );

		if ( svg_debug_areaportals->integer ) {
			gi.dprintf( "%s: %s targetname(\"%s\"), portalID(#%d), count=(%d)\n",
				__func__, _STR( ENTITY_USETARGET_TYPE_TOGGLE ), targetName, self->style, ( int32_t )self->count );
		}
	}

	// Push derived boolean state into collision model.
	gi.SetAreaPortalState( self->style, ( self->count > 0 ? 1 : 0 ) );

	// Re-link entity so server/client snapshots reflect the change.
	gi.linkentity( self );
}

DEFINE_MEMBER_CALLBACK_POSTSPAWN( svg_func_areaportal_t, onPostSpawn )( svg_func_areaportal_t *self ) -> void {
	// Make sure to super post spawn it.
	Super::onPostSpawn( self );

	// Apply initial refcount to collision model (boolean open/closed).
	gi.SetAreaPortalState( self->style, ( self->count > 0 ? 1 : 0 ) );
	gi.linkentity( self );

	if ( svg_debug_areaportals->integer ) {
		gi.dprintf( "%s: Post Spawned func_areaportal targetname(\"%s\"), portalID(#%d), count=(%d)\n",
			__func__, ( self->targetname ? ( const char * )self->targetname : "<null>" ), self->style, ( int )self->count );
	}
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
	self->SetUseCallback( &svg_func_areaportal_t::onUse );
	self->SetPostSpawnCallback( &svg_func_areaportal_t::onPostSpawn );
	gi.linkentity( self );

	if ( svg_debug_areaportals->integer ) {
		gi.dprintf( "%s: Spawned func_areaportal targetname(\"%s\"), portalID(#%d), count=(%d)\n",
			__func__, ( self->targetname ? ( const char * )self->targetname : "<null>" ), self->style, ( int )self->count );
	}
}
