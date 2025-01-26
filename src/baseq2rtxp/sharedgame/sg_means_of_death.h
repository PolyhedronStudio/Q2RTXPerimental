/********************************************************************
*
*
*	SharedGame: Means of Death Indices.
*	NameSpace: "".
*
*
********************************************************************/
#pragma once



/**
*   @brief  Indicates the cause/reason/method of the entity's death.
**/
typedef enum {
    //! Unknown reasons.
    MEANS_OF_DEATH_UNKNOWN,

    //! When fists(or possibly in the future kicks) were the means of death.
    MEANS_OF_DEATH_HIT_FIGHTING,
    //! Shot down by a Pistol.
    MEANS_OF_DEATH_HIT_PISTOL,
    //! Shot down by a SMG.
    MEANS_OF_DEATH_HIT_SMG,
    //! Shot down by a rifle.
    MEANS_OF_DEATH_HIT_RIFLE,
    //! Shot down by a sniper.
    MEANS_OF_DEATH_HIT_SNIPER,

    //! Environmental drowned by water.
    MEANS_OF_DEATH_WATER,
    //! Environmental killed by slime.
    MEANS_OF_DEATH_SLIME,
    //! Environmental killed by lava.
    MEANS_OF_DEATH_LAVA,
    //! Environmental killed by a crusher.
    MEANS_OF_DEATH_CRUSHED,
    //! Environmental killed by someone teleporting to your location.
    MEANS_OF_DEATH_TELEFRAGGED,

    //! Entity suicide.
    MEANS_OF_DEATH_SUICIDE,
    //! Entity fell too harsh.
    MEANS_OF_DEATH_FALLING,

    //! Death by explosive.
    MEANS_OF_DEATH_EXPLOSIVE,
    //! Death by exploding misc_barrel.
    MEANS_OF_DEATH_EXPLODED_BARREL,

    //! Hit by a laser.
    MEANS_OF_DEATH_LASER,

    //! Hurt by a 'splash' particles of temp entity.
    MEANS_OF_DEATH_SPLASH,
    //! Hurt by a 'trigger_hurt' entity.
    MEANS_OF_DEATH_TRIGGER_HURT,

    //! Killed by bumping into a target_change_level while the gamemode does not allow/support it.
    MEANS_OF_DEATH_EXIT,

    //! Caused by a friendly fire.
    MEANS_OF_DEATH_FRIENDLY_FIRE
} sg_means_of_death_t;
// Enumerator Type Bit Flags Support:
QENUM_BIT_FLAGS( sg_means_of_death_t );