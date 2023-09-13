// System:
using System;
using System.Diagnostics.CodeAnalysis;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

// Interop:
using Interop;
using Interop.Math;
using Interop.Types;

// Game:
using ServerGame.Types;

namespace Interop
{

    public static class Strings
    {
        /**
        *   @brief  Will convert the signed byte array string into an ASCII encoded C# string type.
        **/
        public static unsafe string SBytesToString( sbyte* str ) {
			int len = 0;

			for ( sbyte *b = str; *b != 0; b++ ) {
				len++;
			}
			
			return System.Text.Encoding.ASCII.GetString( (byte*)str, len );
        }
        public static unsafe string SBytesToString( sbyte* str, int offset, int maxLength ) {
			int len = 0;

			for ( sbyte *b = str + offset; (*b != 0 || b < b + maxLength); b++ ) {
				len++;
			}
			
			return System.Text.Encoding.ASCII.GetString( (byte*)str, len );
        }

        /**
        *   @brief  Converts the string into an ASCII encoded signed byte string.
        **/
        public static byte[] StringToSBytes( string str ) {
            return System.Text.Encoding.ASCII.GetBytes(str);
        }
        public static byte[] StringToSBytes( string str, int offset, int maxLength ) {
            return System.Text.Encoding.ASCII.GetBytes(str, offset, maxLength );
        }
    };
}