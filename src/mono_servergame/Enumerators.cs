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
    // public enum ConfigString : int {
    //     Name = 0,
    //     CompactDiskTrack = 1,
    //     Sky = 2,
    //     SkyAxis = 3,    // %f %f %f format.
    //     SkyRotate = 4,
    //     StatusBar = 5,  // Display "statusbar draw program string".
    //     AirAcceleration = 29,   // Air Acceleration Control.
    //     MaxClients = 30,
    //     MapCheckSum = 31,       // For catching 'cheater' maps.
    //     Models = 32,
    //     Sounds = ( Models + LevelLimits.MaxModels ),// In C: CS_SOUNDS (CS_MODELS+MAX_MODELS)
    //     Images = ( Sounds + LevelLimits.MaxSounds ),// In C: CS_IMAGES (CS_SOUNDS+MAX_SOUNDS)
    //     Lights = ( Images + LevelLimits.MaxImages ),// In C: CS_LIGHTS (CS_IMAGES+MAX_IMAGES)
    //     Items = ( Lights + LevelLimits.MaxLightStyles ),// In C: (CS_LIGHTS+MAX_LIGHTSTYLES)
    //     PlayerSkins = ( Lights + LevelLimits.MaxItems ),// In C: (CS_ITEMS+MAX_ITEMS)
    //     General = ( PlayerSkins + LevelLimits.MaxClients ),// In C: (CS_PLAYERSKINS+MAX_CLIENTS)
    //     ConfigStrings = ( General + LevelLimits.MaxGeneral ),// In C: (CS_GENERAL+MAX_GENERAL)
    // };

	/**
	*	@brief	ConfigStrings Enum Utility Class.
	**/
	static class ConfigStrings {
		public const int Name = 0;
        public const int CompactDiskTrack = 1;
        public const int Sky = 2;
        public const int SkyAxis = 3;           // %f %f %f format.
        public const int SkyRotate = 4;
        public const int StatusBar = 5;         // Display "statusbar draw program string".

        public const int AirAcceleration = 29;  // Air Acceleration Control.
        public const int MaxClients = 30;
        public const int MapCheckSum = 31;      // For catching 'cheater' maps.

        public const int Models = 32;

        public const int Sounds = ( Models + (int)LevelLimits.MaxModels );          // In C: CS_SOUNDS (CS_MODELS+MAX_MODELS)
        public const int Images = ( Sounds + (int)LevelLimits.MaxSounds );          // In C: CS_IMAGES (CS_SOUNDS+MAX_SOUNDS)
        public const int Lights = ( Images + (int)LevelLimits.MaxImages );          // In C: CS_LIGHTS (CS_IMAGES+MAX_IMAGES)
        public const int Items = ( Lights + (int)LevelLimits.MaxLightStyles );      // In C: (CS_LIGHTS+MAX_LIGHTSTYLES)
        public const int PlayerSkins = ( Lights + (int)LevelLimits.MaxItems );      // In C: (CS_ITEMS+MAX_ITEMS)
        public const int General = ( PlayerSkins + (int)LevelLimits.MaxClients );   // In C: (CS_PLAYERSKINS+MAX_CLIENTS)
        public const int TotalConfigStrings = ( General + (int)LevelLimits.MaxGeneral ); // In C: (CS_GENERAL+MAX_GENERAL)

		// Suggest inlining this method instead.
		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public static int Size( int configString ) {
			// Some mods actually exploit CS_STATUSBAR to take space up to CS_AIRACCEL
			return ( ( configString ) >= ConfigStrings.StatusBar && ( configString ) < ConfigStrings.AirAcceleration 
					? (int)StringLimits.QPath * ( ConfigStrings.AirAcceleration - ( configString ) ) : (int)StringLimits.QPath );
		}
	};

	/**
	*	@brief	Weapon view model positionin
	**/
    public enum WeaponHandSide : int {
        Right,
        Left,
        Center
    };


    /**
    *   @brief  Per Level Limits
    **/
    public enum LevelLimits : int {
        MaxClients = 256,   // Absolute total limit (connection wise)
        MaxEdicts = 1024,   // Must change protocol to increase to a higher count.
        MaxLightStyles = 256,
        MaxModels = 256,
        MaxSounds = 256,
        MaxImages = 256,
        MaxItems = 256,
        MaxGeneral = (MaxClients * 2),

        MaxClientName = 16,
    };
    
    /**
    *   @brief  The specific type of Muzzleflash for ServerCommand.Muzzleflash
    *           also used for 'monster/player effects'.
    **/
    public enum MuzzleFlash : int {
        Blaster = 0,
        MachineGun = 1,
        ShotGun = 2,
        ChainGun1 = 3,
        ChainGun2 = 4,
        ChainGun3 = 5,
        RailGun = 6,
        Rocket = 7,
        Grenade = 8,
        Login = 9,
        Logout = 10,
        Respawn = 11,
        BFG = 12,
        SuperShotGun = 13,
        HyperBlaster = 14,
        ItemRespawn = 15,

        // RAFAEL
        IonRipper = 16,
        BlueHyperBlaster = 17,
        Phalanx = 18,
        Silenced = 128,     // bit flag ORed with one of the above numbers

            // //ROGUE
        ETFRifle = 30,
        Unused = 31,
        Shotgun2 = 32,
        HeatBeam = 33,
        Blaster2 = 34,
        Tracker = 35,
        Nuke1 = 36,
        Nuke2 = 37,
        Nuke4 = 38,
        Nuke8 = 39,
            
        // Q2RTX
        Flare = 40,
    };

    /**
    *   @brief  Command Instruction Messages. These are sent from the server to the client.
    **/
    public enum ServerCommands : byte {
        Bad = 0,    // svc_bad,

        // TODO: These should value wise move up and start from svc_num_types
        // however that would also effectively be the immediate breaking of the
        // protocol as is, so for now we keep them as they are.
        // These ops are known to the game dll.
        Muzzleflash,    // svc_muzzleflash,
        Muzzleflash2,   // svc_muzzleflash2,
        TempEntity,     // svc_temp_entity,
        Layout,         // svc_layout,
        Inventory,      // svc_inventory,

        // The rest are private to the client and server.
        Nop,
        Disconnect,
        Reconnect,
        Sound,                  // See code.
        Print,                  // [byte] id [string] null terminated string.
        StuffText,              // [string] stuffed into client's console buffer should be \n terminated
        ServerData,             // [long] protocol ...
        ConfigString,           // [short] [string]
        SpawnBaseline,  
        CenterPrint,            // [string] to put in center of the screen.
        Download,               // [short] size [size bytes]
        PlayerInfo,             // variable
        PacketEntities,         // [...]
        DeltaPacketEntities,    // [...]
        Frame,                  // [...]
        
        // R1Q2 Protocol Sspecific Operations:
        ZPacket,    // svc_zpacket,
        ZDownload,  // svc_zdownload,
        GameState,  // svc_gamestate, // q2pro specific, means svc_playerupdate in r1q2
        Setting,    // svc_setting,

        // TODO: Game Server Commands should START from here. (Read up, breaks protocols.)
        NumberOfServerCommandTypes
    };

    /**
    *   @brief  All the char buffers/string related maximum sizes.
    **/
    public class StringLimits {
        public const int CharBuffer      = 4096;    // Max length of a string passed to Cmd_TokenizeString.
        
		public const int Tokens          = 256;     // Max tokens resulting from Cmd_TokenizeString.
        public const int TokenCharacters = 1024;	// Maximum length of an individual token.
        
		public const int NetString      = 2048;		// Maximum length of a string used in the network protocol.
		public const int NetName		= 16;		// Maximum length of a client's 'netname'.

        public const int QPath          = 64;   // Maximum Length of a 'Quake Game' pathname.
        public const int OSPath         = 256;  // Maximum Length of a 'Filesystem' pathname.

		public const int InfoKey		= 64;	// Maximum Length of the 'info key' name.
		public const int InfoValue		= 64;	// Maximum Length of the 'info key' its 'info value'.
		public const int InfoString		= 512;	// Maximum Length of the 'info string'.
    };

    /**
    *   @brief  TakeDamage flags.
    **/
    public enum TakeDamage : int {
        // Will not take damage.
        No,
        //! Will take damage IF hit.
        Yes,
        //! Recognized by auto targetting also.
        Aim
    };
}