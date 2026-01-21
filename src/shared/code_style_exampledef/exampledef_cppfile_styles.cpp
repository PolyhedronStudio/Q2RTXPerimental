//! This is where GitHub CoPilot can analyse header file style from.
#ifdef STYLE_TYPE_FOR_COPILOT
/********************************************************************
*
*
*	ServerGame: Small Description, e.g Entities: "func_door"
*
*   Brief and/or long description depending on the complexity of our needs.
*
*
********************************************************************/
// Includes needed.
#include "svgame/svg_local.h"
#include "svgame/svg_utils.h"
#include "svgame/svg_entity_events.h"

// Entity includes.
#include "svgame/entities/svg_base_edict.h"
#include "sharedgame/sg_entities.h"

// CoPilot style includes.
#include "shared/copilot_cppfile_styles.h"


/**
*
*
*
*	Static Member Initialization and Method Definitions:
*
*
*
**/
/**
*	@brief	This is a reference...
*			New lines go like this...
**/
svg_example_copilot_class::staticExampleMember = 42; // Static member initialization outside of class definition.



/**
*
*
*
*	Catagory Description and stating it is their function implementations:
*
*
*
**/
/**
*	@brief	An example method. We can place more text here and either keep it on this line but ultimately we
*			break down to a second line because horizontal scrolling sucks.
**/
void svg_example_copilot_class::SetCurrentHealth( const int32_t newCurrentHealth, const health_set_flags flags ) {
	// Example code here.
	if ( ( flags & HEALTH_SET_FLAG_NO_OVERHEAL ) != 0 && ( newCurrentHealth > 100 ) ) {
		currentHealth = 100;
	} else {
		currentHealth = newCurrentHealth;
	}

	if ( ( flags & HEALTH_SET_FLAG_NOTIFY ) != 0 ) {
		Com_Error( ERR_DISCONNECT, "%s's health is now %d.\n", displayName.c_str(), currentHealth );
		return;
	}

	// Just a random extra style example for reference.
	const uint64_t exampleBitmask = COPILOT_STYLE_EXAMPLE | BIT_ULL( 44 );
	if ( ( exampleBitmask & COPILOT_STYLE_EXAMPLE ) != 0 ) {
		// Do something here. Like print a debug message.
		gi.dprintf( "%s: Example bitmask flag is set!\n", __func__ );

		// Iterate over all the bits in the bitmask. 
		// Debug printing as we go which bits are precisely set.
		for ( int32_t i = 0; i < 64; i++ ) {
			// Check if bit i is set.
			if ( ( exampleBitmask & ( 1ULL << i ) ) != 0 ) {
				// Debug print the bit that is set.
				gi.dprintf( "%s: Bit %d is set in exampleBitmask.\n", __func__, i );
			}
		}
	}
}
/**
*	@brief	Get current Health.
*	@note	To @CoPilot, these functions are sometimes inlined in the class, or merely declared inside of it and defined outside of it in a general translation unit.
**/
const int32_t svg_example_copilot_class::GetCurrentHealth() const { // <-- Note the const at the end for const correctness. Only apply if necessary.
	return currentHealth;
}
#endif