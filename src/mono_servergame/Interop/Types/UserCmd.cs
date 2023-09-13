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
        [Flags]
        public enum UserCommandButtons : byte {
            Attack = 1,
            Use = 2,
            Any = 128
        };
        /**
        *  @brief  UserCommand
        **/
        [StructLayout(LayoutKind.Sequential)]
        public unsafe struct UserCmd {
			//! Amount of miliseconds the command took place. (Press time)
            public byte msec;
			//! Buttons that were pressed.
            public UserCommandButtons buttons;
			//! Short encoded angles of where we're at.
            public fixed short angles[3];
			//! Accelerative direction determinants.
            public short forwardMove, sideMove, upMove;
			//! Impulse command.
            public byte impulse;       // remove?
			//! Light Level (Unsupported in RTX.)
            public byte lightLevel;
        }
    }
}