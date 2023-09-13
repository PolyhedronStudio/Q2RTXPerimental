using System;
using System.Runtime.InteropServices;

namespace Interop
{
    namespace Math
    {
        [StructLayout(LayoutKind.Sequential)]
        public unsafe struct Vector4
        {
            // X Y Z components.
            public float x, y, z, w;

            // Constructors.
            public Vector4(Vector4 v)
            {
                x = v.x; y = v.y; z = v.z; w = v.w;
            }
            public Vector4(ref Vector4 v)
            {
                x = v.x; y = v.y; z = v.z; w = v.w;
            }
            public Vector4(float _x, float _y, float _z, float _w)
            {
                x = _x;
                y = _y;
                z = _z;
                w = _w;
            }
        };
    };
};