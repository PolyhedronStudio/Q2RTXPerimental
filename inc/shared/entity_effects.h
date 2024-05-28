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
#define EF_ROTATE           BIT( 1 )		// rotate (bonus items)
#define EF_GIB              BIT( 2 )		// leave a trail
#define EF_BLASTER          BIT( 3 )		// redlight + trail
#define EF_ROCKET           BIT( 4 )		// redlight + trail
#define EF_GRENADE          BIT( 5 )
#define EF_HYPERBLASTER     BIT( 6 )
#define EF_BFG              BIT( 7 )
#define EF_COLOR_SHELL      BIT( 8 )
#define EF_POWERSCREEN      BIT( 9 )
#define EF_ANIM01           BIT( 10 )      // automatically cycle between frames 0 and 1 at 2 hz
#define EF_ANIM23           BIT( 11 )      // automatically cycle between frames 2 and 3 at 2 hz
#define EF_ANIM_ALL         BIT( 12 )      // automatically cycle through all frames at 2hz
#define EF_ANIM_ALLFAST     BIT( 13 )      // automatically cycle through all frames at 10hz
#define EF_FLIES            BIT( 14 )
#define EF_QUAD             BIT( 15 )
#define EF_PENT             BIT( 16 )
#define EF_TELEPORTER       BIT( 17 )      // particle fountain
#define EF_FLAG1            BIT( 18 )
#define EF_FLAG2            BIT( 19 )
// RAFAEL
#define EF_IONRIPPER        BIT( 20 )
#define EF_GREENGIB         BIT( 21 )
#define EF_BLUEHYPERBLASTER BIT( 22 )
#define EF_SPINNINGLIGHTS   BIT( 23 )
#define EF_PLASMA           BIT( 24 )
#define EF_TRAP             BIT( 25 )

//ROGUE
#define EF_TRACKER          BIT( 26 )
#define EF_DOUBLE           BIT( 27 )
#define EF_SPOTLIGHT		BIT( 28 )
#define EF_TAGTRAIL         BIT( 29 )
#define EF_HALF_DAMAGE      BIT( 30 )
#define EF_TRACKERTRAIL     BIT( 31 )
//ROGUE