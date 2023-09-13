// System:
using System;
using System.Diagnostics.CodeAnalysis;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

// Interop:
using Interop;
using Interop.Math;
using Interop.Types;


namespace ServerGame {

	namespace Types {

        /**
        *   @brief  All client data that is persistent during a client's lifetime, including,
        *           across map changes.
        **/
        [StructLayout(LayoutKind.Sequential)]

        public unsafe struct ClientPersistentData {
            //
            // Player Specific Info:
            //
            //! Stores the entire 'user info' string of this client, fetched during connection and updated if needed.
            private fixed sbyte userInfo[ StringLimits.InfoString ];
			//! Property accessor for 'userInfo'.
            public string UserInfo {
				get {
					fixed ( sbyte* str = userInfo ) {
						return Interop.Strings.SBytesToString( str );
					}
				}
				set	{
					int maxChars = Math.Min( value.Length, StringLimits.InfoString );
					string limitedUserInfoStr = value.Substring( 0, maxChars );

					fixed ( sbyte* str = userInfo ) {
						for ( int i = 0; i < maxChars; i++ ) {
							str[ i ] = (sbyte)limitedUserInfoStr[ i ];
						}
					}
				}
            }

            //! The player's 'net' name, "Pisswasser" for example.
            private fixed sbyte netName[ StringLimits.NetName ];
			//! Property accessor for 'netName'.
            public string NetName {
				get {
					fixed ( sbyte* str = netName ) {
						return Interop.Strings.SBytesToString( str ); //new string( str, 0, StringLimits.InfoString );
					}
				}
				set	{
					int maxChars = Math.Min( value.Length, StringLimits.NetName );
					string limitedUserInfoStr = value.Substring( 0, maxChars );

					fixed ( sbyte* str = netName ) {
						for ( int i = 0; i < maxChars; i++ ) {
							str[ i ] = (sbyte)limitedUserInfoStr[ i ];
						}
					}
				}
            }

            //! The sided hand the weapon is displayed on.
            public WeaponHandSide weaponHandSide;


            //
            // State Specific:
            //
            //! Stores whether connected or not. "A loadgame will leave valid entities that, just don't have a connection 'yet'."
            public int isConnected;
            //! True in case this client is an actual in spectator mode residing client.
            public int isSpectator;


            //
            //! Values saved and restored from the client's edicts when changing levels:
            //
            public int health;
            public int maxHealth;

            public EdictFlags savedFlags;
            public int score; // For coop mode.


            //
			//! Inventory. 
            //
			public int selectedItem;
			//[MarshalAs(UnmanagedType.ByValArray, SizeConst = 256)]
			//public int []inventory;
			public fixed int inventory[ 256 ];

            //
            //! Help
            //
            public int gameHelpChanged;
            public int helpChanged;
        };

        
        /**
        *   @brief  Client data that stays persistent during 'deaths'/'respawns' during the same map playthrough.
        **/
        [StructLayout(LayoutKind.Sequential)]
        public unsafe struct ClientRespawnData {
            //! What to set client->pers to on a respawn.
            public ClientPersistentData persistentRespawnData;

            //! The LevelLocals.currentFrame at which the client actually (re-)entered the game.
            public long enterFrame;         // ServerMain.levelLocals.CurrentFrame # the client entered the game
            //! The client's current score. (Frags, etc.)
            public int score;

            //! The angles sent over by the last UserCmd (so to respawn viewing that same direction.)
            public Vector3 userCommandAngles; // (used to be cmd_angles).

            //! True if the client is a spectator.
            public int isSpectator;
        };

		/**
         *  @brief  Client
         **/
		
        [StructLayout(LayoutKind.Sequential)]
        public unsafe struct Client
        {
            /**
            *    Shared memory block: C server side memory. Do not touch.
            **/
            public PlayerState playerState;      // communicated by server to clients
            public int ping;
            public int clientNumber; // TODO: We need this for alignment? 
			/**
			*   End of shared memory block.
			**/


            // //
            // // Save/Restore Specifics:
            // //
            // //! This client's persistent data throughout its connection lifetime, and across map changes.
            // public ClientPersistentData persistentData;
            // //! This client's respawn data during the current map playthrough. This does not specifically transfer across map changes.
            // public ClientRespawnData respawnData;
            
            // //! This client's old, last, previous player movement state.
            // public PmoveState oldPlayerMoveState;


            // //
            // // Timers:
            // //
            // //! Stores the frame number at which we last respawned.
            // public struct FrameTimers {
            //     //! The frame at which we last respawned.
            //     public long respawnFrameNumber;

            //     //! The frame at which we last received a 'pickup' notification.
            //     public long pickupMessageFrameNumber;
            // };
            // public FrameTimers timers; // TODO: frameTimers?


            // //
            // //  Aim View:
            // //
            // public Vector3 v_angle;
            // public double bobTime;

            // public Vector3 oldViewAngles;
            // public Vector3 oldVelocity;


            // //
            // // Buttons:
            // //
            // //! The client's actual frame button bits.
            // public UserCommandButtons buttons;
            // //! Previous frame buttons.
            // public UserCommandButtons oldButtons;
            // //! Latched buttons are those that require toggling.
            // public UserCommandButtons latchedButtons;


            // //
            // // UI/Layout:
            // //
            // public struct Layouts {
            //     public int showScores; // Whether to display the scoreboard layout.
            //     public int showInventory; // Whether to show layout stat for inventory.
            //     public int showHelp; // Whether to show layout stat for help.
            //     public int showHelpIcon; // ...
            // };
            // //! Contains state of whether to show or hide certain client layouts.
			// public Layouts layouts;


            // //
            // // AnimationState
            // //
            // public struct AnimationState {
            //     /**
            //     *   Animation Priorities:
            //     **/
            //     public const int AnimationBasic = 0;
            //     public const int AnimationWave = 1;
            //     public const int AnimationJump = 2;
            //     public const int AnimationPain = 3;
            //     public const int AnimationAttack = 4;
            //     public const int AnimationDeath = 5;
            //     public const int AnimationReverse = 6;

            //     /**
            //     *   State:
            //     **/
            //     public int animationEnd;            // anim_end
            //     public int priorityAnimation;   // anim_priority

            //     public int isDucked;    // anim_duck
            //     public int isRunning;   // anim_run
            // };
            // //! Contains the client's actual current animation state data.
			// public AnimationState animationState;
            

            // //
            // // Inventory/Weapon.
            // //
            // public struct Inventory {
            //     public int ammoIndex; 
            // };
            // public Inventory inventory;


            // /**
            // *   @brief  Resets/clears client state to a fresh one.
            // **/
            // public static void ClearClient( ref Client *client ) {
            //     *client = default( Client );
            // }
        }

	};
};