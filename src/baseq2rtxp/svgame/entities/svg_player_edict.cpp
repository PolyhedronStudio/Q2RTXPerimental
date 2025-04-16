

/********************************************************************
*
*
*	SVGame: Edicts Functionalities:
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/svg_save.h"
#include "svgame/entities/svg_player_edict.h"



/**
*
* 
*
*   Save Descriptor Field Setup: svg_player_edict_t
*
* 
*
**/
//! Save descriptor for the player entity.
svg_save_descriptor_field_t svg_player_edict_t::saveDescriptorFields[] = {
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_player_edict_t, testVar, SD_FIELD_TYPE_INT32 ),
    SAVE_DESCRIPTOR_DEFINE_FIELD_ARRAY( svg_player_edict_t, testVar2, SD_FIELD_TYPE_VECTOR3, 1 ),
};

//! Implement the methods for saving this edict type's save descriptor fields.
SVG_SAVE_DESCRIPTOR_FIELDS_DEFINE_IMPLEMENTATION( svg_player_edict_t, svg_base_edict_t );


/**
*   Reconstructs the object, optionally retaining the entityDictionary.
**/
void svg_player_edict_t::Reset( bool retainDictionary ) {
    // Call upon the base class.
    svg_base_edict_t::Reset( retainDictionary );
	// Reset the edict's save descriptor fields.
    testVar = 0;
    testVar2 = {};
}