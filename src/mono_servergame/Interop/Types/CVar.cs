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
        *   @brief  Cvar Interop struct.
        **/
        [StructLayout(LayoutKind.Sequential, CharSet=CharSet.Ansi)]
        public unsafe struct cvar_t {
            public char *name;//public sbyte* name;
            public char *string_;//public sbyte* string_;
            public char *latched_string_;//public sbyte* latched_string;   // for CVAR_LATCH vars

            public CvarFlags flags;
            public QBoolean modified;    // set each time the cvar is changed
            public float value;
        }
    }
}