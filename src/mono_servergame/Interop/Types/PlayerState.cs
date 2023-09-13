// System:
using System;
// using System.Diagnostics.CodeAnalysis;
// using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

// Interop:
using Interop;
using Interop.Math;
using Interop.Types;
//using static Interop.ServerImports;

// ServerGame:
using ServerGame.Types;


namespace Interop
{
	namespace Types {
		/**
        *  @brief  PlayerState
        **/
		[StructLayout( LayoutKind.Sequential )]
		public unsafe struct PlayerState {
			//! For prediction, needs to be communicated 'bit-precise'.
			public PmoveState pmove;

			// These fields do not need to be communicated '.bit-precise'.

			//! For fixed views
			public Vector3 viewAngles;
			//! Add to pmovestate->origin
			public Vector3 viewOffset;

			//! Add to view direction to get render angles, set by weapon kicks, pain effects, etc
			public Vector3 kickAngles;

			//! Gun angles.
			public Vector3 gunAngles;
			//! Gun origin offset.
			public Vector3 gunOffset;

			//! Gun weapon ('model' also) index.
			public ModelHandle gunIndex;
			//! Gun animation frame.
			public int gunFrame;

			//! RGBA Fullscreen color overlay blend effect.
			public Vector4 blend;

			//! horizontal field of view
			public float fov;

			//! Render definition flags.
			public RenderDefFlags rdFlags;

			//! Real Time Statusbar update slots.
			public fixed short stats[ 32 ];
		}
	}
}