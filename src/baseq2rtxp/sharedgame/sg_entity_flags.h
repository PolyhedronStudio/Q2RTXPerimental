/********************************************************************
*
*
*   Entity Effects:
*
*	NOTE: These are also passed into Lua, make sure to (re-)adjust `RenderFx` enum in svg_lua_gamelib.cpp
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
#define EF_ROTATE           EF_ENGINE_MAX	// Rotate (Bonus Items).
#define EF_GIB              BIT( 2 )		// Leave a trail.
#define EF_ANIM_CYCLE2_2HZ	BIT( 3 )		// Automatically cycle between frames 0 and 1 at 2 hz.
#define EF_ANIM01           BIT( 4 )		// Automatically cycle between frames 0 and 1 at 2 hz.
#define EF_ANIM23           BIT( 5 )		// Automatically cycle between frames 2 and 3 at 2 hz.
#define EF_ANIM_ALL         BIT( 6 )		// Automatically cycle through all frames at 2hz.
#define EF_ANIM_ALLFAST     BIT( 7 )		// Automatically cycle through all frames at 10hz.
#define EF_COLOR_SHELL      BIT( 8 )
#define EF_QUAD             BIT( 9 )
#define EF_PENT             BIT( 10 )
#define EF_DOUBLE			BIT( 11 )
//#define EF_UNUSED_03		BIT( 11 )
#define EF_HALF_DAMAGE		BIT( 12 )
#define EF_TELEPORTER		BIT( 13 )		// Particle fountain.
#define EF_ENTITY_EVENT_TARGET_OTHER	    BIT( 14 )
//#define EF_ENTITY_EVENT_TARGET_OTHER	    BIT( 15 )
#define EF_UNUSED_15		BIT( 15 )
#define EF_UNUSED_16		BIT( 16 )
#define EF_UNUSED_17		BIT( 17 )
#define EF_UNUSED_18		BIT( 18 )
#define EF_UNUSED_19		BIT( 19 )
#define EF_UNUSED_20        BIT( 20 )
#define EF_UNUSED_21		BIT( 21 )
#define EF_UNUSED_22		BIT( 22 )
#define EF_UNUSED_23		BIT( 23 )
#define EF_UNUSED_24		BIT( 24 )
#define EF_UNUSED_25		BIT( 25 )
#define EF_UNUSED_26		BIT( 26 )
#define EF_UNUSED_27		BIT( 27 )
#define EF_UNUSED_28		BIT( 28 )
#define EF_UNUSED_29		BIT( 29 )
#define EF_UNUSED_30		BIT( 30 )
#define EF_UNUSED_31		BIT( 31 )
