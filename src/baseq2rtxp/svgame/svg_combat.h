/********************************************************************
*
*
*	ServerGame: Local Game Header
*	NameSpace: "".
*
*
********************************************************************/
#pragma once



/***
*
*
*
*   Damage Indicator and Means Of Death:
*
*
*
***/
/**
*   @brief  Stores data indicating where damage came from, and how much it damage it did.
**/
struct damage_indicator_t {
    //! Direction indicated hit came from.
    Vector3 from;
    //! Health taken.
    int32_t health;
    //! Armor taken.
    int32_t armor;
};







/**
*   @brief  TakeDamage, no, yes, yes(auto target recognization).
**/
typedef enum entity_takedamage_e {
    DAMAGE_NO   = 0,    //! Won't take damage when hit.
    DAMAGE_YES,         //! Will take damage if hit.
    DAMAGE_AIM          //! Auto targeting recognizes this.
} entity_takedamage_t;

/**
*   @brief  Damage effect information.
**/
typedef enum entity_damageflags_e {
    DAMAGE_NONE             = 0,
    DAMAGE_RADIUS           = BIT( 0 ), // Damage was indirect.
    DAMAGE_NO_ARMOR         = BIT( 1 ), // Armour does not protect from this damage.
    DAMAGE_ENERGY           = BIT( 2 ), // Damage is from an energy based weapon.
    DAMAGE_NO_KNOCKBACK     = BIT( 3 ), // Do not affect velocity, just view angles.
    DAMAGE_BULLET           = BIT( 4 ), // Damage is from a bullet (used for ricochets).
    DAMAGE_NO_PROTECTION    = BIT( 5 ), // Armor, shields, invulnerability, and godmode have no effect.
    DAMAGE_NO_INDICATOR     = BIT( 6 )  // For clients: No damage indicators.
} entity_damageflags_t;
// Bit Operators:
QENUM_BIT_FLAGS( entity_damageflags_t );


/**
*   DeadFlag: Alive, dying, dead, and optional respawn ability.
**/
typedef enum entity_lifestatus_e {
    //! The entity is alive and kicking.
    LIFESTATUS_ALIVE  = 0,
    //! The entity is in the process of dying.
    LIFESTATUS_DYING  = 1, // Acts as: BIT( 0 ),
    //! The entity is actually dead, on floor somewhere or gibbed out.
    LIFESTATUS_DEAD   = 2, // Acts as: BIT( 1 ),
    //! It is dead, but, respawnable.
    LIFESTATUS_RESPAWNABLE = BIT( 2 ) // BIT( 2 )== 4
} entity_lifestatus_t;
// Bit Operators:
QENUM_BIT_FLAGS( entity_lifestatus_t );

/**
*   @brief  WID: TODO: What are we gonna do, reuse these or something?
**/
#if 0
typedef enum entity_range_distance_e {
    //! Melee range, will become hostile even if back is turned?
    RANGE_DISTANCE_MELEE = 0,
    //! Visibility and infront, or visibility and show hostile.
    RANGE_DISTANCE_NEAR,
    //! Infront and show hostile.
    RANGE_DISTANCE_MID,
    //! Only triggered by damage.
    RANGE_DISTANCE_FAR,
} entity_range_distance_t;
#endif

/**
*   Noise types for SVG_PlayerNoise
**/
static constexpr int32_t PNOISE_SELF = 0;
static constexpr int32_t PNOISE_WEAPON = 1;
static constexpr int32_t PNOISE_IMPACT = 2;



// WID: TODO: Kept for future inspiration :-)
#if 0
// Monster AI flags:
#define AI_STAND_GROUND         0x00000001
#define AI_TEMP_STAND_GROUND    0x00000002
#define AI_SOUND_TARGET         0x00000004
#define AI_LOST_SIGHT           0x00000008
#define AI_PURSUIT_LAST_SEEN    0x00000010
#define AI_PURSUE_NEXT          0x00000020
#define AI_PURSUE_TEMP          0x00000040
#define AI_HOLD_FRAME           0x00000080
#define AI_GOOD_GUY             0x00000100
#define AI_BRUTAL               0x00000200
#define AI_NOSTEP               0x00000400
#define AI_DUCKED               0x00000800
#define AI_COMBAT_POINT         0x00001000
#define AI_MEDIC                0x00002000
#define AI_RESURRECTING         0x00004000
#define AI_HIGH_TICK_RATE		0x00008000

//monster attack state
#define AS_STRAIGHT             1
#define AS_SLIDING              2
#define AS_MELEE                3
#define AS_MISSILE              4
#endif


/**
*   @brief
**/
const bool SVG_OnSameTeam( svg_base_edict_t *ent1, svg_base_edict_t *ent2 );
/**
*   @brief
**/
const bool SVG_CanDamage( svg_base_edict_t *targ, svg_base_edict_t *inflictor );
/**
*   @brief
**/
void SVG_TriggerDamage( svg_base_edict_t *targ, svg_base_edict_t *inflictor, svg_base_edict_t *attacker, const vec3_t dir, vec3_t point, const vec3_t normal, const int32_t damage, const int32_t knockBack, const entity_damageflags_t damageFlags, const sg_means_of_death_t meansOfDeath );
/**
*   @brief
**/
void SVG_RadiusDamage( svg_base_edict_t *inflictor, svg_base_edict_t *attacker, float damage, svg_base_edict_t *ignore, float radius, const sg_means_of_death_t meansOfDeath );