// System.
using System;
using System.Runtime.InteropServices;

namespace Interop
{
    namespace Math
    {
        /// <summary>
        /// Vector3 Type.
        /// </summary>
        [StructLayout(LayoutKind.Sequential)]
        public unsafe struct Vector3
        {
            /// <summary>
            /// ! A zero, origin, 0,0,0 vector.;
            /// </summary>
            public static Vector3 Zero = new Vector3( 0f, 0f, 0f );

            //! X Y Z components.
            public float x, y, z;


            #region Constructors
            //! Constructors.
            public Vector3(Vector3 v)
            {
                x = v.x; y = v.y; z = v.z;
            }
            public Vector3(ref Vector3 v)
            {
                x = v.x; y = v.y; z = v.z;
            }
            public Vector3(float _x, float _y, float _z)
            {
                x = _x;
                y = _y;
                z = _z;
            }
            #endregion // Constructors.

            #region Properties_PitchYawRoll
            public float Pitch {
                get { return x; }
                set { this.x = value; }
            }
            public float Yaw {
                get { return y; }
                set { this.y = value; }
            }
            public float Roll {
                get { return z; }
                set { this.z = value; }
            }
            #endregion // Properties_PitchYawRoll

            #region OperatorOverloading
            //
            // Operator overloading:
            //
            // Minus(+) Operator.
            public static Vector3 operator +(Vector3 vA, Vector3 vB)
            {
                return new Vector3(
                    vA.x + vB.x,
                    vA.y + vB.y,
                    vA.z + vB.z
                );
            }
            // Minus(-) Operator.
            public static Vector3 operator -(Vector3 vA, Vector3 vB)
            {
                return new Vector3(
                    vA.x - vB.x,
                    vA.y - vB.y,
                    vA.z - vB.z
                );
            }
            // Multiply(*) Operator.
            public static Vector3 operator *(Vector3 vA, Vector3 vB)
            {
                return new Vector3(
                    vA.x * vB.x,
                    vA.y * vB.y,
                    vA.z * vB.z
                );
            }
            // Divide(/) Operator.
            public static Vector3 operator /(Vector3 vA, Vector3 vB)
            {
                return new Vector3(
                    vA.x / vB.x,
                    vA.y / vB.y,
                    vA.z / vB.z
                );
            }
            #endregion // OperatorOverloading

            #region CoordinateTranslations
            /**
            *   @brief  Encodes the components into 'short' based coordinates.
            *   @return A short[3] array(x,y,z).
            **/
            public static short[] ConvertToShortCoordinates(Vector3 v) => new short[3] {
                ((short) ( (v.x) * 8.0f ) ),
                ((short) ( (v.y) * 8.0f ) ),
                ((short) ( (v.z) * 8.0f ) )
            };
            /**
            *   @brief  Decodes the components back into 'float' based coordinates.
            **/
            public static Vector3 ReadFromShortCoordinates(short[] coords) => new Vector3(
                ((coords[0]) * (1.0f / 8)),
                ((coords[1]) * (1.0f / 8)),
                ((coords[2]) * (1.0f / 8))
            );
            public static Vector3 ReadFromShortCoordinates(short* coords) => new Vector3(
                ((coords[0]) * (1.0f / 8)),
                ((coords[1]) * (1.0f / 8)),
                ((coords[2]) * (1.0f / 8))
            );
            #endregion

            #region StaticMathFunctions
            /**
            *   @brief  Outputs the forward, right and up vector for the given Euler angles.
            **/
            public static unsafe void AngleVectors(ref Vector3 angles, ref Vector3 forward, ref Vector3 right, ref Vector3 up) {
                float        angle;
                float        sr, sp, sy, cr, cp, cy;

                angle = (angles.y) * (MathF.PI / 180f);
                sy = MathF.Sin(angle);
                cy = MathF.Cos(angle);
                angle = (angles.x) * (MathF.PI / 180f);
                sp = MathF.Sin(angle);
                cp = MathF.Cos(angle);
                angle = (angles.z) * (MathF.PI / 180f);
                sr = MathF.Sin(angle);
                cr = MathF.Cos(angle);

                //if (forward != null) {
                    forward.x = cp * cy;
                    forward.y = cp * sy;
                    forward.z = -sp;
                //}
                //if (right != null) {
                    right.x = (-1 * sr * sp * cy + -1 * cr * -sy);
                    right.y = (-1 * sr * sp * sy + -1 * cr * cy);
                    right.z = -1 * sr * cp;
                //}
                //if (up != null) {
                    up.x = (cr * sp * cy + -sr * -sy);
                    up.y = (cr * sp * sy + -sr * cy);
                    up.z = cr * cp;
                //}
            }
            #endregion // StaticMathFunctions

            #region Other            
            /**
            *   @return A parentheses enclosed formatted string of the Vector3 and its components.
            *           Example: (.x:30, .y:60, .z:20)
            **/
            public override string ToString() {
                return string.Format("(.x:{0}, .y:{1}, .z:{2})", x, y, z);
            }
            #endregion // Other

        };
    };
};