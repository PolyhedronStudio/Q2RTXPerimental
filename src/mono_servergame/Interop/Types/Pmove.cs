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
        *   @brief Delegate function callback types for Pmove.
        **/
		//[UnmanagedFunctionPointer(CallingConvention.Cdecl)]
		public delegate TraceResult PM_TraceDelegate( [In,Out] ref Vector3 start, [In,Out] ref Vector3 mins, [In,Out] ref Vector3 maxs, [In,Out] ref Vector3 end );
		//[UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate ContentFlags PM_PointContentsDelegate( [In,Out] ref Vector3 point );

        [StructLayout(LayoutKind.Sequential)]
        public unsafe struct PmoveInterop
        {
			//
            // State (in / out)
			//
            public PmoveState pmoveState;

			//
            // Command (in)
			//
            public UserCmd cmd;

			//! True if the player state has been changed outside of the pmove.
            public bool snapInitial; // WID: For C/C++ this should map to int

			//
            // Actual move results (out)
			//
			//! Number of touched entities.
            public int numTouch;
			//! Pointers to all entities possibly touched.
            //public IntPtr *touchEntities[ 32 ];
            public edict_t* touchEntities_0, touchEntities_1, touchEntities_2, touchEntities_3, touchEntities_4, touchEntities_5, touchEntities_6, touchEntities_7,
                 touchEntities_8, touchEntities_9, touchEntities_10, touchEntities_11, touchEntities_12, touchEntities_13, touchEntities_14, touchEntities_15,
                 touchEntities_16, touchEntities_17, touchEntities_18, touchEntities_19, touchEntities_20, touchEntities_21, touchEntities_22, touchEntities_23,
                 touchEntities_24, touchEntities_25, touchEntities_26, touchEntities_27, touchEntities_28, touchEntities_29, touchEntities_30, touchEntities_31;

			//! Clamped.
            public Vector3 viewAngles;
            //! View height from origin.
			public float viewHeight;

			//! BoundingBox size.
            public Vector3 mins, maxs;

			//! If any, a pointer to the ground entity we're currently standing on top of.
            public edict_t* groundEntity;

			//! Type of water we're in. (Solid content's type(s).)
            public ContentFlags waterType;
			//! The water level we're at.
            public int waterLevel;

			//! Function Pointers to the actual trace and pointcontents methods to use during player movement.
            public IntPtr trace;
            public IntPtr pointcontents;
            //public PM_TraceDelegate trace;
            //public PM_PointContentsDelegate pointContents;
        }
    }
}