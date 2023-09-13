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
    *   @brief  AreaType for the AreaEdicts function.
    **/
    public enum AreaType : int {
        Solid = 1,
        Triggers = 2,
        // TODO: Did this mean, add a "Both" options? // FIXME: eliminate AREA_ distinction?
    };


    /**
    *   @brief  DeadFlags determine the actual 'life' of an entity. ( This means that an entity's health is NOT the determinant factor. )
    **/
    public enum DeadFlags : int {
        //! It is alive and kicking.
        None,
        //! It is in the process of dying. ( Likely, playing a death animation and pain sounds. )
        Dying,
        //! Truly dead, idle/sliding on-ground.
        Dead,
        //! Truly dead, idle/sliding on-ground, BUT, respawnable.
        Respawnable
    };

    /**
    *   @brief  'entity_state_t->event' values. Ertity events are for effects that take place relative
    *           to an existing entities origin. Very network efficient.
    *
    *           TODO: lol, from the OG comments:  All muzzle flashes really should be converted to events...
    **/
    public enum EntityEvents : int  {
        None, //EV_NONE,
        ItemRespawn, //EV_ITEM_RESPAWN,
        FootStep, //EV_FOOTSTEP,
        FallShort, //EV_FALLSHORT,
        Fall, //EV_FALL,
        FallFar, //EV_FALLFAR,
        PlayerTeleport, //EV_PLAYER_TELEPORT,
        OtherTeleport, //EV_OTHER_TELEPORT
    };

    /**
    *  @brief  Game Print Levels:
    **/
    public enum GamePrintLevel : int {
        //! Pickup messages. ( In BaseQ2: PRINT_LOW )
        PickupMessage = 0,
        //! Death messages. ( In BaseQ2: PRINT_MEDIUM )
        DeathMessage = 1,
        //! Critical messages. ( In BaseQ2: PRINT_HIGH )
        CriticalMessage = 2,
        //! Chat messages. ( In BaseQ2: PRINT_CHAT )  
        ChatMessage = 3,
    };

    /**
    *   @brief  Determines what movement physics to operate the entity with.
    **/
    public enum MoveType : int {
        //! Never moves.
        None,          
        //! Origin and angles change with no worldly, nor any entity interaction.
        NoClip,
        //! No clip to world, push on box contact.
        Push,
        //! No clip to world, stops on box contact.
        Stop,          

        //! Player movement, reacts to world, box, and performs gravity
        Walk,
        //! Monster 'step' movement, reacts to world, box, and performs gravity
        Step,
        //! Monster 'step' movement, reacts to world, box, no gravity
        Fly,
        //! Item/Object movement, performs a toss like motion, responding to gravity.
        Toss,          // gravity
        //! MoveType_Fly but has a bigger size to monsters.
        FlyMissile,    // extra size to monsters
        //! Item/Object movement, performs a bouncing toss like motion, responding to gravity.
        Bounce
    };

    /**
    *   @brief  Determines to what clients to send the current written message.
    **/
    public enum Multicast : int {
        All = 0,    // Send the message as 'Unreliable' to all clients.
        PHS = 1,    // Send the message as 'Unreliable' to all clients which are in the same 'Possible Hearing Set'.
        PVS = 2,    // Send the message as 'Unreliable' to all clients which are in the same 'Possible Visibility Set'.

        ReliableAll = 3,    // Send the message as 'Reliable' to all clients.
        ReliablePHS = 4,    // Send the message as 'Reliable' to all clients which are in the same 'Possible Hearing Set'.
        ReliablePVS = 5,    // Send the message as 'Reliable' to all clients which are in the same 'Possible Visibility Set'.
    };

    /**
    *   @brief  PlayerState stats[] indices.
    **/
    public enum PlayerStateStats : int {
        HealthIcon = 0,
        Health,
        AmmoIcon,
        Ammo,
        ArmorIcon,
        Armor,
        SelectedIcon,
        PickupIcon,
        PickupString,
        TimerIcon,
        Timer,
        HelpIcon,
        SelectedItem,
        Layouts,
        Frags,
        Flashes,        //! For operating FLASHING NUMBERS on HUD, cleared each frame, stats[Flashes]:  1 = health, 2 = armor
        Chase,
        Spectator,

        MaximumStats = 32
    };

    /**
    *   @brief  Determines the 'physical body' type of the entity.
    **/
    public enum Solidity : int {
        //! No interaction with other objects at all. (Ignored by clipping mechanisms.)
        None,
        //! Only touch when inside, after moving.
        Trigger,
        //! Clip and 'touch' at any edge of the box.
        BoundingBox,         // touch on edge
        //! BSP Clip, 'touch' at any edge of the BSP Model.
        BSP
    };


    /**
    *   @brief  Ssound attenuation values
    **/
    public enum SoundAttenuation : int {
        None = 0,   // Full volume regardless, the entire level.
        Normal = 1,
        Idle = 2,
        Static = 3, // Diminish very rapidly with distance.
    };
}