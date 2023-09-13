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
        [StructLayout(LayoutKind.Sequential)]
        public unsafe struct SoundHandle
        {
            public int handle;

            public SoundHandle(int _handle) {
                handle = _handle;
            }
        }
    }
}