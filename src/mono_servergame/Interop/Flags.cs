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

namespace Interop
{

    /**
    *   @brief  Brush/Entity content flags: Lower bits are stronger, and will eat weaker brushes completely!
    **/
    [Flags]
    public enum ContentFlags : uint {
        //! An entity/player its 'eye' is never valid in a solid
        Solid = 1, 
        //! Translucent, but not watery.
        Window = 2,
        Aux = 4,
        Lava = 8,
        Slime = 16,
        Water = 32,
        Mist = 64,

        //! remaining contents are non-visible, and don't eat brushes.

        AreaPortal = 0x8000,

        PlayerClip = 0x10000,
        MonsterClip = 0x20000,

        //! Currents can be added to any other contents, and may be mixed.
        Current0 = 0x40000,
        Current90 = 0x80000,
        Current180 = 0x100000,
        Current270 = 0x200000,
        CurrentUp = 0x400000,
        CurrentDown = 0x800000,

        //! Removed before bsping an entity.
        Origin = 0x1000000,    

        //! Should never be on a brush, only intent for in-game use.
        Monster = 0x2000000,
        DeadMonster = 0x4000000,
        //! Brushes to be added after vis leafs.
        Detail = 0x8000000,
        //! Auto set if any surface has trans.
        Translucent = 0x10000000,  
        Ladder = 0x20000000,

        //! Content Masks.
        MaskAll = 0xFFFFFFFF,
        MaskSolid = (Solid | Window),
        MaskPlayerSolid = (Solid | PlayerClip | Window | Monster),
        MaskDeadSolid = (Solid | PlayerClip | Window),
        MaskMonsterSolid = (Solid | MonsterClip | Window | Monster),
        MaskWater = (Water | Lava | Slime),
        MaskOpaque = (Solid | Slime | Lava),
        MaskShot = (Solid | Monster | Window | DeadMonster),
        MaskCurrents = (Current0 | Current90 | Current180 | Current270 | CurrentUp | CurrentDown)
    };

    /**
    *   @brief Actual Cvar configuration flags.
    **/
    [Flags]
    public enum CvarFlags : int {
        None = 0,
        //! Set to cause it to be saved to vars.rc
        Archived = 1,
        //! Added to userinfo  when changed.
        UserInfo = 2,
        //! Added to serverinfo when changed.
        ServerInfo = 4,
        //! Do NOT allow change from console at all, but can be set from the command line.
        ReadOnly = 8,
        //! Save changes until server restart
        Latched = 16
    };

    /**
    *   @brief  Player movement 'state' flags.
    **/
    [Flags]
    public enum PmoveFlags : byte {
        PMF_DUCKED = 1,
        PMF_JUMP_HELD = 2,
        PMF_ON_GROUND = 4,
        //! Uses: pm_time is waterjump
        PMF_TIME_WATERJUMP = 8,	
        //! Uses: pm_time is time before rejump
        PMF_TIME_LAND = 16,
        //! Uses: pm_time is non-moving time
        PMF_TIME_TELEPORT = 32,
        //! Uses: temporarily disables prediction (used for grappling hook)
        PMF_NO_PREDICTION = 64
    };


    // pmove_state_t is the information necessary for client side movement
    // prediction
    /**
    *   @brief  The actual player movement type for the 'Pmove' system.
    **/
    public enum PmoveType : int {
        //! Normal movement, can accelerate and turn, clips to all entities and world.
        Normal,
        //! Same as normal movement however, without gravity and clipping.
        Spectator,
        //! Dead players can't 'accelerate', or 'turn' any further based on any following up user input.
        Dead,
        //! Special 'Gib' bounding box size for when a player 'gibs out'.
        Gib,
        //! Frozen acceleration. No movement based on user input.
        Freeze
    };

    /**
    *   @brief  RenderFlags determining how to interpret/effect the entity for rendering.
    **/
    public enum RenderEffects : int {
        MinimalLight = 1,   // Always have some light ( used on weapon view models. )
        ViewerModel = 2,    // Don't draw through eyes, only through mirrors.
        WeaponModel = 4,    // Only draw through eyes.
        Fullbright = 8,     // Always draw at full color intensity. (No shading)
        DepthHack = 16,     // For view weapon Z crunching.
        Translucent = 32,
        FrameLerp = 64,
        Beam = 128,
        CustomSkin = 256,
        Glow = 512,
        ShellRed = 1024,
        ShellGreen = 2048,
        ShellBlue = 4096,

        InfraRedVisible = 0x0008000,
        ShellDouble = 0x00010000,
        ShellHalfDamage = 0x00020000,
        UseDisguise = 0x00040000
    };

    /**
    *   @brief  Render definition flags for clients as on how to interpret the entity rendering wise.
    **/
    [Flags]
    public enum RenderDefFlags : int {
        // player_state_t->refdef flags

        //! Warp the screen as apropriate.
        RDF_UNDERWATER = 1,
        //! Used for player configuration screen.
        RDF_NOWORLDMODEL = 2,		

        //! ROGUE
        RDF_IRGOGGLES = 4,
        RDF_UVGOGGLES = 8
        //! ROGUE
    };   

    /**
    *   @brief  Server specific entity flags.
    **/
    [Flags]
    public enum ServerFlags : int {
        //! Don't send entity to clients, even if it has effects set to it.
        NoClient = 0x00000001,
        //! Treat as CONTENTS_DEADMONSTER for collision.
        DeadMonster = 0x00000002,
        //! Treat as CONTENTS_MONSTER for collision.
        Monster = 0x00000004,
    };

    /**
    *   @brief  Sound Channels  Channel(0):  will never willingly override.
    *                           Channels(1 - 7): Always override a playing sound on that channel.
    **/
    [Flags]
    public enum SoundChannel : int {
        Auto = 0,
        Weapon = 1,
        Voice = 2,
        Item = 3,
        Body = 4,

        // Modifier Flags.
        NoPHSAdd,   // Send to all clients, not just the ones that are in the PHS( Attenuation 0 will also do this. )
        Reliable    // Send by reliable message, and not the 'datagram'.
    };
}