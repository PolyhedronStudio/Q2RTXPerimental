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
        *  @brief  EntityState
        **/
        [StructLayout(LayoutKind.Sequential)]
			//! Edict index.
        public unsafe struct EntityState
        {
            // public int number;          // edict index

            // public Vector3 origin;
            // public Vector3 angles;
            // public Vector3 old_origin;     // for lerping
            // public ModelHandle modelIndex, modelIndex2, modelIndex3, modelIndex4;   // weapons, CTF flags, etc
            // public int frame;
            // public int skinNumber;
            // public uint effects;        // PGM - we're filling it, so it needs to be unsigned
            // public int renderFx;
            // public int solid;           // for client side prediction, 8*(bits 0-4) is x/y radius
            //                             // 8*(bits 5-9) is z down distance, 8(bits10-15) is z up
            //                             // gi.linkentity sets this properly
            // public SoundHandle sound;          // for looping sounds, to guarantee shutoff
            // public int event_;          // impulse events -- muzzle flashes, footsteps, etc
            //                             // events only go out for a single frame, they
            //                             // are automatically cleared each frame
            public int number;

            public Vector3 origin;
            public Vector3 angles;

			//! For lerping.
            public Vector3 old_origin;

			//! Weapons, CTF flags, etc
            public ModelHandle modelIndex, modelIndex2, modelIndex3, modelIndex4;   

            public int frame;
            public int skinNumber;

			//! PGM - we're filling it, so it needs to be unsigned
            public uint effects;        
            public RenderEffects renderFx;

			//! For client side prediction, 8*(bits 0-4) is x/y radius,
            //! and 8*(bits 5-9) is z down distance, 8(bits10-15) is z up.
            //! LinkEntity sets this properly.
            public int solid;

			//! For looping sounds, to guarantee shutoff
            public SoundHandle sound;	

            public EntityEvents event_;//public int event_;          // impulse events -- muzzle flashes, footsteps, etc
                                        // events only go out for a single frame, they
                                        // are automatically cleared each frame
        }
    }
}