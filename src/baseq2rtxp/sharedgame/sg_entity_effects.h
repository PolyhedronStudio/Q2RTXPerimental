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
// 
// NOTE!!:
// An entity that HAS EFFECTS, WILL BE SENT, to the client EVEN IF it HAS A ZERO MODEL INDEX.
#define EF_ROTATE           EF_ENGINE_MAX	// rotate (bonus items)
#define EF_GIB              BIT( 3 )		// leave a trail
#define EF_MATERIAL_ANIM_NO_AUTO_CYCLE	BIT( 13 ) // Automatically cycle between frames 0 and 1 at 2 hz
#define EF_ANIM_CYCLE2_2HZ	BIT( 12 )		// Automatically cycle between frames 0 and 1 at 2 hz
#define EF_ANIM01           BIT( 4 )		// Automatically cycle between frames 0 and 1 at 2 hz
#define EF_ANIM23           BIT( 5 )		// Automatically cycle between frames 2 and 3 at 2 hz
#define EF_ANIM_ALL         BIT( 6 )		// Automatically cycle through all frames at 2hz
#define EF_ANIM_ALLFAST     BIT( 7 )		// Automatically cycle through all frames at 10hz
#define EF_COLOR_SHELL      BIT( 8 )
#define EF_QUAD             BIT( 9 )
#define EF_PENT             BIT( 10 )
#define EF_TELEPORTER       BIT( 11 )      // particle fountain
//#define EF_UNUSED_04		BIT( 12 )		// redlight + trail
//#define EF_UNUSED_05		BIT( 13 )		// redlight + trail
#define EF_UNUSED_06		BIT( 14 )
#define EF_UNUSED_07	    BIT( 15 )
#define EF_UNUSED_08		BIT( 16 )
#define EF_UNUSED_10		BIT( 17 )
#define EF_UNUSED_15		BIT( 18 )
#define EF_UNUSED_19		BIT( 19 )
#define EF_UNUSED_20		BIT( 20 )
#define EF_UNUSED_21        BIT( 21 )
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
