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
#define EF_ROTATE           EF_ENGINE_MAX	// rotate (bonus items)
#define EF_GIB              BIT( 3 )		// leave a trail
#define EF_BLASTER          BIT( 4 )		// redlight + trail
#define EF_ROCKET           BIT( 5 )		// redlight + trail
#define EF_GRENADE          BIT( 6 )
#define EF_HYPERBLASTER     BIT( 7 )
#define EF_BFG              BIT( 8 )
#define EF_COLOR_SHELL      BIT( 9 )
#define EF_POWERSCREEN      BIT( 10 )
#define EF_ANIM01           BIT( 11 )      // automatically cycle between frames 0 and 1 at 2 hz
#define EF_ANIM23           BIT( 12 )      // automatically cycle between frames 2 and 3 at 2 hz
#define EF_ANIM_ALL         BIT( 13 )      // automatically cycle through all frames at 2hz
#define EF_ANIM_ALLFAST     BIT( 14 )      // automatically cycle through all frames at 10hz
#define EF_FLIES            BIT( 15 )
#define EF_QUAD             BIT( 16 )
#define EF_PENT             BIT( 17 )
#define EF_TELEPORTER       BIT( 18 )      // particle fountain
#define EF_FLAG1            BIT( 19 )
#define EF_FLAG2            BIT( 20 )
// RAFAEL
#define EF_IONRIPPER        BIT( 21 )
#define EF_GREENGIB         BIT( 22 )
#define EF_BLUEHYPERBLASTER BIT( 23 )
#define EF_SPINNINGLIGHTS   BIT( 24 )
#define EF_PLASMA           BIT( 25 )
#define EF_TRAP             BIT( 26 )

//ROGUE
#define EF_TRACKER          BIT( 27 )
#define EF_DOUBLE           BIT( 28 )
#define EF_TAGTRAIL         BIT( 29 )
#define EF_HALF_DAMAGE      BIT( 30 )
#define EF_TRACKERTRAIL     BIT( 31 )
//ROGUE