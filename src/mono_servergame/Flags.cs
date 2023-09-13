// System:
using System;
using System.Diagnostics.CodeAnalysis;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

// Interop:
using Interop;
using Interop.Math;
using Interop.Types;

// Game:
using ServerGame.Types;


namespace ServerGame {
    /**
    *   @brief  Edict flags.
    **/
    [Flags]
    public enum EdictFlags : int {
        Fly = 0x00000001,
        //! Implied immunity to drowning. ( Entity can NOT drown. )
        Swim = 0x00000002,
        //! Entity is immune to lasers.
        Laser = 0x00000004,
        InWater = 0x00000008,
        God = 0x00000010,
        NoTarget = 0x00000020,
        Slime = 0x00000040,
        Lava = 0x00000080,
        //! Not all corners are valid on-ground.
        PartialGround = 0x00000100,
        //! Player jumping out of water.
        WaterJump = 0x00000200,
        //! Not the first on the team.
        TeamSlave = 0x00000400,
        //! Prevent knockbacks from explosions and alike.
        NoKnockback = 0x00000800,
        //! Power armor (if any) is active
        PowerArmor = 0x00001000,
        //! Used for item respawning
        //Respawn = 0x80000000
    };

    /**
    *   @brief  Flags dtermining what kind of 'game item' type we're dealing with.
    **/
    [Flags]
    public enum GameItemFlags : int {
        Weapon = 1,
        Ammo = 2,
        Armor = 4,
        StayCoop = 8,
        Key = 16,
        PowerUp = 32
    };
}