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
    namespace Types
    {
        /**
        *  @brief  PmoveState (Player Move State).
        **/
        [StructLayout(LayoutKind.Sequential)]
        public unsafe struct PmoveState {
			//! The player move type to use.
            public PmoveType pm_type;

			//! 12.3
            public fixed short origin[3];
			//! 12.3
            public fixed short velocity[3];

			//! Whether we're 'ducked', 'jump_held', etc 
            public PmoveFlags pm_flags;

			//! Each unit = 8ms.
            public byte pm_time;

			//! Gravity for player move simulation.
            public short gravity;

			//! The delta angles, use as: Add to command angles to get view direction.
			//! It is changed by spawns, rotating objects, and teleporters.
            public fixed short delta_angles[3]; 
        }
    }
}