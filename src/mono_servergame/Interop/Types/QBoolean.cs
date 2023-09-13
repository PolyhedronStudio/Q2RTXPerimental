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
        /**
        *   @brief  A wrapper around qboolean(actually is an int32_t internally).
        **/
        [StructLayout(LayoutKind.Sequential)]
        public struct QBoolean {
            private int value;

            public QBoolean( int _value = 0 ) { value = _value;  }
            public QBoolean(bool v) { value = v ? 1 : 0; }

            public static implicit operator bool(QBoolean v) => v.value == 1;
            //public static implicit operator QBoolean(bool v) => value = v;
        }
    }
}