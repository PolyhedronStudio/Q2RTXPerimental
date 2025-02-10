/********************************************************************
*
*
*	SharedGame: UseTarget Hints Data Array. Used for displaying info
*				when targeting a (not per seUseTarget) capable entity.
*
*
********************************************************************/
#include "shared/shared.h"

#include "sharedgame/sg_shared.h"
#include "sharedgame/sg_usetarget_hints.h"


//! Uncomment to enable debug printing of when it is unable to find an ID matching UseTarget Hint.
#define SG_USETARGET_HINT_DEBUG 1

/**
*   @brief  If id == 0, returns an empty hint, meaning nothing.
*           If id > 0, it iterates the UseTargetHint array in order to find one with a matching ID.
*           If id < 0, it will return the specific hint index meant for custom strings.
**/
const sg_usetarget_hint_t *SG_UseTargetHint_GetByID( const int32_t id ) {
    // For 'nothing':
    if ( id > 0 ) {
        // Iterate to find one.
        for ( const sg_usetarget_hint_t &useTargetHint : useTargetHints ) {
            if ( useTargetHint.index == id ) {
                return &useTargetHint;
            }
        }
        // Found nothing, return first index.
        if ( SG_USETARGET_HINT_DEBUG ) {
            SG_DPrintf( "(%s): Failed to find a matching 'sg_usetarget_hint_t' value in the data array for ID(#%d)\n",
                __func__,
                id
            );
        }
    // For custom strings:
    } else if ( id < 0 ) {
        return &useTargetHints[ 1 ];
    }

    // Default.
    return &useTargetHints[ 0 ];
}