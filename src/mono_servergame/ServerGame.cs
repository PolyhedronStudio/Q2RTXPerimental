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

		/****
		*
		*
		*	Game API Callback Implementations:
		*
		*
		****/
		//! ge->Init
		public void Initialize() {
			DPrint( "GameMain::Initialize( );\n" );
		}
		//! ge->Shutdown
		public void Shutdown() { 
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