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


namespace Interop
{
    namespace Types
    {
        [StructLayout(LayoutKind.Sequential)]
        public unsafe struct ImageHandle {
            public int handle;

            public ImageHandle(int _handle) {
                handle = _handle;
            }
        }
    }
}