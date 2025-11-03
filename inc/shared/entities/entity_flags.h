/********************************************************************
*
*
*   Entity Effects:
*
*
********************************************************************/
#pragma once


/**
*	entity_state_t->entityFlags
*
*	Entity Flags are sent to the client as part of the entity_state_t structure.
*
*	Indicates special effects/treatment for the entity(usually client side),
*	even if it has a zero index model.
*
**/
#define EF_NONE             0 //BIT( 0 )
#define EF_SPOTLIGHT		BIT( 0 )
//! Game Entity Effects can start from here:
#define EF_ENGINE_MAX		BIT( 1 )