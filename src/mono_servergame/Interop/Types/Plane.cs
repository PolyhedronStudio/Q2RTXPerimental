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
        *  @brief  Plane
        **/
        [StructLayout(LayoutKind.Sequential)]
        public unsafe struct Plane {
			//! The normal this face is facing.
            public Vector3 normal;
			//! Distance from 0,0,0. Multiply normal by distance to get the actual origin.
            public float dist;

            //! Also referred to as short[2] pad; in the C code.
            private int type_and_bits;
        }
    }
}