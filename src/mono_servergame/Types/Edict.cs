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
		*	@brief	Entity Dictionary
		**/
		[StructLayout( LayoutKind.Sequential )]
		public unsafe struct Edict {
			/**
			*   Shared memory block: C server side memory. Do not touch.
			**/
			public EntityState state;
			public IntPtr* client;//public Client* client;
			public int inUse;

			//! BSP Clip Linking Properties:
			public int linkCount;

			public IntPtr areaPrevious, areaNext;
			public int numberOfClusters;
			public fixed int clusterNumbers[ 16 ];
			public int headNode;
			public int areaNumber1, areaNumber2;

			//! Server specific entity flags.
			public ServerFlags serverFlags;
			//! Local min and max of entity.
			public Vector3 mins, maxs;
			//! Absolute min,max and size of entity.
			public Vector3 absoluteMin, absoluteMax, size;
			//! Solidity, none, BSP, BoundingBox, etc.
			public Solidity solid;
			//! Specific type of clipping.
			public ContentFlags clipMask;
			//! Entity's can own others in regards to clipping exclusion tests.
			public IntPtr* owner;//public Edict* owner;
			/**
			*   End of shared memory block.
			**/
		};

	};
};