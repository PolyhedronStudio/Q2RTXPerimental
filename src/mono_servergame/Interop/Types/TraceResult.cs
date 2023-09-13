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
         *  @brief  TraceResults
         **/
        [StructLayout(LayoutKind.Sequential)]
        public unsafe struct TraceResult
        {
			//! Whether the trace is considered entirely inside of a 'Solid'.
            public int allSolid;
			//! Whether the trace started off inside of a 'Solid'.
            public int startSolid;
			//! Total traced fraction of the 'bounds' traced from start to end.
            public float fraction;
			//! Actual absolute world end position vector.
            public Vector3 endPosition;

			//! A copy of the actual plane that was hit.
            public Plane plane;
			//! A pointer to a BSP surface that was his, if any.
            public IntPtr surface; // todo
			//! Flags determining the contents of the 'Solid' that we hit.
            public ContentFlags contents;

			//! World in case of not hitting any entity, otherwise a pointer to the actual entity instead. 
            public Edict* entity;
        }
    }
}