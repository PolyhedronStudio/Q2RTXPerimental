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
using static ServerGame.Imports;



namespace ServerGame {
	/**
	*	@brief	Contains all static 'entry point methods' of the ServerGame for use with ServerGameExports.
	**/
	class GameMain {
		GameMain() {
			DPrint( "GameMain::GameMain();\n" );
		}
		~GameMain() {
			DPrint( "GameMain::~GameMain();\n" );
		}

		/****
		*
		*
		*	API Tests
		*
		*	We'll test the following things:
		*	1. Allocate an array of Edict, maxSize = 1024
		*	2. Pass a "UnsafeAddrOfPinnedObject" to C, so it can read
		*	   its server side shared memory.
		*	3. We'll pass one of the array's elements over as a pointer to a C# function.
		*	   This function will set some values as if it were a legitimate scenario.
		*
		****/
		// //! Edicts array, allocated in unmanaged memory.
		// IntPtr edicts = (IntPtr)0;

		// //! Maximum number of edicts.
		// const int MAX_EDICTS = (int)LevelLimits.MaxEdicts;

		// //! Number of edicts.
		// int numberOfEdicts = 0;

		// /**
		// *	@brief	Initialize the edicts array, returning its unsafe pinned address ptr.
		// **/
		// public unsafe IntPtr AllocateEdictsArray( int size, int *managedSize ) {
		// 	// Clamp size.
		// 	size = Math.Clamp( 0, MAX_EDICTS, size );

		// 	*managedSize = Marshal.SizeOf<Edict>();

		// 	// Free edicts array if we already had one.
		// 	if ( edicts != (IntPtr)0 ) {
		// 		Marshal.FreeHGlobal( edicts );
		// 	}

		// 	// Allocate edicts array in unmanaged memory.
		// 	edicts = Marshal.AllocHGlobal( Marshal.SizeOf<Edict>() * size );


		// 	// Zero out all edicts.
		// 	for( int i = 0; i < size; i++ ) {
		// 		Edict* edict = (Edict*)( IntPtr.Add( edicts, i * Marshal.SizeOf<Edict>() ) );
		// 		// Ensure its memory is zeroed out.
		// 		*edict = default(Edict);
		// 	}

		// 	// Return unsafe addr of.
		// 	return edicts;
		// }

		// /**
		// *	Test modifying an edict, when accepted a non IntPtr arg.
		// **/
		// public unsafe void PassEdictPtr( Edict *entity ) {
		// 	Edict *edict = (Edict*)entity;
		// 	//Edict edict = Marshal.PtrToStructure<Edict>(entity);
		// 	// Set some values.
		// 	edict->serverFlags = ServerFlags.Monster;
		// 	edict->state.number = 50;

		// 	DPrint( string.Format( "PassEdictPtr(#{0}) \n", edict->state.number ) );

		// 	// Copy/Convert back in to unmanaged memory.
		// 	//Marshal.StructureToPtr<Edict>(edict, entity, false);
		// }

		// /**
		// *	Test modifying an edict, when accepted as IntPtr arg.
		// **/
		// public unsafe void PassEdictIntPtr( IntPtr entity ) {
		// 	// Temporary managed memory.
		// 	Edict mEdict = default(Edict);

		// 	Marshal.PtrToStructure<Edict>( entity, mEdict );
		// 	// Set some values.
		// 	mEdict.serverFlags = ServerFlags.Monster;
		// 	mEdict.state.number = 20;
		// 	// Copy/Convert back in to unmanaged memory.
		// 	Marshal.StructureToPtr<Edict>(mEdict, entity, true);
		// }

		/****
		*
		*	Set/Gets for the pointers that are set in Initalize methods.
		*
		****/
		struct ServerGameExports {
			//! API Version number.
			public unsafe int* _apiVersion;
			public unsafe int APIVersion {
				set { *_apiVersion = value; }
				get { return *_apiVersion; }
			}
			//! Edicts.
			public unsafe Edict *_edicts;
			//public unsafe Edict* Edicts {
			//	get { return edicts[ 0 ]; }
			//}
			//! Managed Edict Size.
			public unsafe int* _managedEdictSize;
			public unsafe int ManagedEdictSize {
				set { *_managedEdictSize = value; }
				get { return *_managedEdictSize; }
			}
			//! Maximum Amount of Edicts.
			public unsafe int* _maximumEdicts;
			public unsafe int MaximumEdicts {
				set { *_maximumEdicts = value; }
				get { return *_maximumEdicts; }
			}
			//! (Never decreases) Number of Edicts initialized/spawned.
			public unsafe int* _numberOfEdicts;
			public unsafe int NumberOfEdicts {
				set { *_numberOfEdicts = value; }
				get { return *_numberOfEdicts; }
			}
		};
		static ServerGameExports gameExports;// = new ServerGameExports();

		/****
		*
		*
		*	Game API Callback Implementations:
		*
		*
		****/
		//! ge->Init
		public unsafe Edict *Initialize( int *apiVersion, int *managedEdictSize, int *maximumEdicts, int *numberOfEdicts ) {
			// Allocate fresh gameExports.
			gameExports = new ServerGameExports();

			// Assign our pointers to those passed in from the C server.
			gameExports._apiVersion = apiVersion;
			gameExports._managedEdictSize = managedEdictSize;
			gameExports._maximumEdicts = maximumEdicts;
			gameExports._numberOfEdicts = numberOfEdicts;

			// Now that our pointers are set: Assign the correct values for the server to know about.
			gameExports.MaximumEdicts = 1024;
			gameExports.ManagedEdictSize = sizeof( Edict );
			gameExports.NumberOfEdicts = 1;
			gameExports.APIVersion = 1;

			DPrint( "GameMain::Initialize( );\n" );

			// Now allocate the edicts array pointer for residing in 'unmanaged' memory.			
			//return gameExports._edicts = (Edict*)(IntPtr.Add( Marshal.AllocHGlobal( Marshal.SizeOf<Edict>() * gameExports.MaximumEdicts ), 8 ));
			return gameExports._edicts = (Edict*)( Marshal.AllocHGlobal( Marshal.SizeOf<Edict>() * gameExports.MaximumEdicts ) );
		}

		// <API Tests>
		public unsafe int TestEntityIn( IntPtr entity ) {
			Edict *edict = (Edict*)entity;
			DPrint( string.Format( "(TestEntityIn): entity(#{0})\n", edict->state.number ) );
			//entity->serverFlags = ServerFlags.Monster;
			//return entity->state.number;
			return 0;
		}
		// <End Of API Tests>
		
		//! ge->Shutdown
		public unsafe void Shutdown() {
			if ( gameExports._edicts != null ) {
		 		Marshal.FreeHGlobal( (IntPtr)gameExports._edicts );
			}
			DPrint( "GameMain::Shutdown( );\n" );
		}

		//! ge->SpawnEntities
		public void SpawnEntities( string mapName, string entityString, string spawnPoint ) { 
			DPrint( string.Format("GameMain::SpawnEntities( {0}, {1}, {2} )\n",
				mapName, spawnPoint, entityString ) );
		}

		//! ge->WriteGame
		public void WriteGame( string fileName ) { 
			DPrint( string.Format( "GameMain::WriteGame( \"{0}\" );\n", fileName ) );
		}
		//! ge->ReadGame
		public void ReadGame( string fileName ) { 
			DPrint( string.Format( "GameMain::ReadGame( \"{0}\" );\n", fileName ) );
		}

		//! ge->WriteLevel
		public void WriteLevel( string fileName ) { 
			DPrint( string.Format( "GameMain::WriteLevel( \"{0}\" );\n", fileName ) );
		}
		//! ge->ReadLevel
		public void ReadLevel( string fileName ) { 
			DPrint( string.Format( "GameMain::ReadLevel( \"{0}\" );\n", fileName ) );
		}

		//! ge->ClientConnect - Note that qboolean is an integer for C#.
		public int ClientConnect( IntPtr entity, string userInfo ) { 
			int entityNumber = 0;
			DPrint( string.Format( "GameMain::ClientConnect( {0}, \"{1}\" );\n", entityNumber, userInfo ) );
		
			return 0; 
		}
		//! ge->ClientBegin
		public void ClientBegin( IntPtr entity ) { 
			int entityNumber = 0;
			DPrint( string.Format( "GameMain::ClientBegin( {0} );\n", entityNumber ) );
		}
		//! ge->ClientUserinfoChanged
		public void ClientUserinfoChanged( IntPtr entity, string userInfo ) { 
			int entityNumber = 0;
			DPrint( string.Format( "GameMain::UserinfoChanged( {0}, \"{1}\" );\n", entityNumber, userInfo ) );
		}
		//! ge->ClientDisconnect
		public void ClientDisconnect( IntPtr entity ) { 
			int entityNumber = 0;
			DPrint( string.Format( "GameMain::ClientDisconnect( {0} );\n", entityNumber ) );
		}
		//! ge->ClientCommand
		public void ClientCommand( IntPtr entity ) { 
			int entityNumber = 0;
			DPrint( string.Format( "GameMain::ClientCommand( {0} );\n", entityNumber ) );
		}
		//! ge->ClientThink
		public void ClientThink( IntPtr entity, IntPtr userCommand ) { 
			int entityNumber = 0;
			DPrint( string.Format( "GameMain::ClientThink( {0}, UserCommand* );\n", entityNumber ) );
		}

		//! ge->RunFrame
		public void RunFrame() { 
			int entityNumber = 0;
			DPrint( string.Format( "GameMain::RunFrame( {0} );\n", entityNumber ) );
		}
		//! ge->ServerCommand
		public void ServerCommand() {
			string serverCommand = "TODO";
			DPrint( string.Format( "GameMain::ServerCommand( {0} );\n", serverCommand ) );
		}
    }

}