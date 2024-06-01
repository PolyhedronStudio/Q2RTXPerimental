/********************************************************************
*
*
*   Entity Effects:
*
*
********************************************************************/
#pragma once

// entity_state_t->effects
// Effects are things handled on the client side (lights, particles, frame animations)
// that happen constantly on the given entity.
// An entity that has effects will be sent to the client
// even if it has a zero index model.
#define EF_NONE             BIT( 0 )
#define EF_SPOTLIGHT		BIT( 1 )
//! Game Entity Effects can start from here:
#define EF_ENGINE_MAX		BIT( 2 )