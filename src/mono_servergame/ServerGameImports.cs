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

/**
*
**/
namespace ServerGame {
	public static unsafe class Imports {

        /*************************************************************************
        *
        *
        *	TagMalloc C Imports ( Do not use these directly! ):
        *
        *
        *************************************************************************/
		// /**
        // *	@brief	Export wrapper for TagMalloc.
        // **/
        // [DllImport("__Internal", CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl, EntryPoint = "MonoVM_Export_TagMalloc")]
        // public static extern ref void* _MonoVM_TagMalloc(int size, int tag);
        // /**
	    // *	@brief	Export wrapper for TagFree.
	    // **/
        // [DllImport("__Internal", CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl, EntryPoint = "MonoVM_Export_TagFree")]
        // public static extern void _MonoVM_TagFree(void* ptr);
        // /**
	    // *	@brief	Export wrapper for FreeTags.
	    // **/
        // [DllImport("__Internal", CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl, EntryPoint = "MonoVM_Export_FreeTags")]
        // public static extern void _MonoVM_FreeTags(int tag);

        /*************************************************************************
        *
        *
        *   TagMalloc Wrapper (Use these directly.)
        *
        *
        *************************************************************************/
        //internal static unsafe T* TagMalloc<T>(int count, int tag) where T : unmanaged
        //{
        //    return (T*)_MonoVM_TagMalloc(count * Marshal.SizeOf<T>(), tag);
        //}

        //internal static unsafe void TagFree(ref void* ptr)
        //{
        //    _MonoVM_TagFree(ptr);
        //    ptr = null;
        //}

        /*************************************************************************
        *
        *
        *	ConfigString and Resource Indexing/Precaching.
        *
        *
        *************************************************************************/
        /**
        *	@brief	Export wrapper for PF_ConfigString
        **/
        [DllImport("__Internal", CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl, EntryPoint = "SV_Mono_Export_ConfigString")]
        public static extern void ConfigString(int index, string value);
        /**
        *	@brief	Export wrapper for PF_ModelIndex
        **/
        [DllImport("__Internal", CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl, EntryPoint = "SV_Mono_Export_ModelIndex")]
        public static extern int ModelIndex(string name);
        /**
        *	@brief	Export wrapper for PF_SoundIndex
        **/
        [DllImport("__Internal", CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl, EntryPoint = "SV_Mono_Export_SoundIndex")]
        public static extern int SoundIndex(string name);
        /**
        *	@brief	Export wrapper for PF_ModelIndex
        **/
        [DllImport("__Internal", CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl, EntryPoint = "SV_Mono_Export_ImageIndex")]
        public static extern int ImageIndex(string name);


        /*************************************************************************
        *
        *
        *	Print Messaging
        *
        *
        *************************************************************************/
        /**
        *	@brief	Export wrapper for BPrintf.
        **/
        [DllImport("__Internal", CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl, EntryPoint = "SV_Mono_Export_BPrintf")]
        public static extern void BPrint(GamePrintLevel gamePrintLevel, string str);
        /**
        *	@brief	Export wrapper for DPrintf.
        **/
        [DllImport("__Internal", CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl, EntryPoint = "SV_Mono_Export_DPrintf")]
        public static extern void DPrint(string str);
        /**
        *	@brief	Export wrapper for CPrintf.
        **/
        [DllImport("__Internal", CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl, EntryPoint = "SV_Mono_Export_CPrintf")]
        public static extern void CPrint(Edict* ent, GamePrintLevel gamePrintLevel, string str);
        /**
        *	@brief	Export wrapper for CenterPrintf.
        **/
        [DllImport("__Internal", CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl, EntryPoint = "SV_Mono_Export_CenterPrint")]
        public static extern void CenterPrint(Edict* ent, string str);


        /*************************************************************************
        *
        *
        *	(Positioned-) Sound
        *
        *
        *************************************************************************/
        /**
        *	@brief	Export wrapper for Sound().
        **/
        [DllImport("__Internal", CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl, EntryPoint = "SV_Mono_Export_StartSound")]
        public static extern void StartSound(Edict* ent, SoundChannel channel, int soundIndex, float volume, SoundAttenuation attenuation, float timeOffset);

        /**
        *	@brief	Export wrapper for StartPositionedSound.
        **/
        [DllImport("__Internal", CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl, EntryPoint = "SV_Mono_Export_StartPositionedSound")]
        public static extern void StartPositionedSound(Vector3 *origin, Edict* ent, SoundChannel channel, int soundIndex, float volume, SoundAttenuation attenuation, float timeOffset);


        /*************************************************************************
        *
        *
        *	Message Read/Write, Uni, and Multi -casting.
        *
        *
        *************************************************************************/
        /**
        *	@brief	Export wrapper for Unicast.
        **/
        [DllImport("__Internal", CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl, EntryPoint = "SV_Mono_Export_MSG_Unicast")]
        public static extern void Unicast(Edict* ent, /*qboolean*/ int reliable);
        /**
        *	@brief	Export wrapper for Multicast.
        **/
        [DllImport("__Internal", CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl, EntryPoint = "SV_Mono_Export_MSG_Multicast")]
        public static extern void Multicast(Vector3 *origin, Multicast to);
        /**
        *	@brief	Export wrapper for WriteFloat
        **/
        [DllImport("__Internal", CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl, EntryPoint = "SV_Mono_Export_MSG_WriteFloat")]
        public static extern void WriteFloat(float fl);
        /**
        *	@brief	Export wrapper for WriteChar
        **/
        [DllImport("__Internal", CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl, EntryPoint = "SV_Mono_Export_MSG_WriteChar")]
        public static extern void WriteChar(int i);
        /**
        *	@brief	Export wrapper for WriteByte
        **/
        [DllImport("__Internal", CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl, EntryPoint = "SV_Mono_Export_MSG_WriteByte")]
        public static extern void WriteByte(int i);
        /**
        *	@brief	Export wrapper for WriteShort
        **/
        [DllImport("__Internal", CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl, EntryPoint = "SV_Mono_Export_MSG_WriteShort")]
        public static extern void WriteShort(int i);
        /**
        *	@brief	Export wrapper for WriteLong
        **/
        [DllImport("__Internal", CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl, EntryPoint = "SV_Mono_Export_MSG_WriteLong")]
        public static extern void WriteLong(int i);
        /**
        *	@brief	Export wrapper for WriteString
        **/
        [DllImport("__Internal", CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl, EntryPoint = "SV_Mono_Export_MSG_WriteString")]
        public static extern void WriteString(string str);
        /**
        *	@brief	Export wrapper for WritePos
        **/
        [DllImport("__Internal", CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl, EntryPoint = "SV_Mono_Export_MSG_WritePosition")]
        public static extern void WritePosition(string str);
            /**
        *	@brief	Export wrapper for WriteAngle
        **/
        [DllImport("__Internal", CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl, EntryPoint = "SV_Mono_Export_MSG_WriteAngle")]
        public static extern void WriteAngle(float f);


        /*************************************************************************
        *
        *
        *	VIS/PVS/PHS/Area Functions
        *
        *
        *************************************************************************/
        /**
        *	@brief	Export wrapper for inVIS
        **/
        [DllImport("__Internal", CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl, EntryPoint = "SV_Mono_Export_LinkEntity")]
        public static extern void LinkEntity(Edict *entity);
        /**
        *	@brief	Export wrapper for inPVS
        **/
        [DllImport("__Internal", CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl, EntryPoint = "SV_Mono_Export_UnlinkEntity")]
        public static extern void UnlinkEntity(Edict *entity);
        /**
        *	@brief	Export wrapper for inVIS
        **/
        [DllImport("__Internal", CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl, EntryPoint = "SV_Mono_Export_inVIS")]
        public static extern QBoolean inVIS(Vector3 *p1, Vector3 *p2, int vis);
        /**
        *	@brief	Export wrapper for inPVS
        **/
        [DllImport("__Internal", CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl, EntryPoint = "SV_Mono_Export_inPVS")]
        public static extern QBoolean inPVS(Vector3 *p1, Vector3 *p2);
        /**
        *	@brief	Export wrapper for inPHS
        **/
        [DllImport("__Internal", CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl, EntryPoint = "SV_Mono_Export_inPHS")]
        public static extern QBoolean inPHS(Vector3 *p1, Vector3 *p2);
        /**
        *	@brief	Export wrapper for SetAreaPortalState
        **/
        [DllImport("__Internal", CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl, EntryPoint = "SV_Mono_Export_SetAreaPortalState")]
        public static extern void SetAreaPortalState(int portalNumber, QBoolean open);
        /**
        *	@brief	Export wrapper for AreasConnected.
        **/
        [DllImport("__Internal", CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl, EntryPoint = "SV_Mono_Export_AreasConnected")]
        public static extern QBoolean AreasConnected(int area1, int area2);
        /**
        *	@brief	Export wrapper for SV_Trace function.
        **/
        [DllImport("__Internal", CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl, EntryPoint = "SV_Mono_Export_Trace")]
        public static extern TraceResult Trace(Vector3 *start, Vector3 *mins, Vector3 *maxs, Vector3 *end, Edict* passEdict, ContentFlags contentMask);
       	/**
	    *	@brief	Export wrapper for SV_PointContents function.
	    **/
        [DllImport("__Internal", CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl, EntryPoint = "SV_Mono_Export_PointContents")]
        public static extern int PointContents(Vector3 *point);
        /**
        *	@brief	Export wrapper for SV_AreaEdicts function.
        **/
        [DllImport("__Internal", CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl, EntryPoint = "SV_Mono_Export_AreaEdicts")]
        public static extern int AreaEdicts( Vector3 *mins, Vector3 *maxs, Edict **edictList, int maxCount, AreaType areaType );


        /*************************************************************************
        *
        *
        *	Random/Other Functions
        *
        *
        *************************************************************************/
        /**
        *	@brief	Export wrapper for AddCommandString.
        **/
        [DllImport("__Internal", CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl, EntryPoint = "SV_Mono_Export_CmdArgc")]
        public static extern int CommandArgumentsCount();
        /**
        *	@brief	Export wrapper for AddCommandString.
        **/
        [DllImport("__Internal", CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl, EntryPoint = "SV_Mono_Export_CmdArgv")]
        public static extern sbyte* CommandArgumentsValue(int number);
        /**
        *	@brief	Export wrapper for AddCommandString.
        **/
        [DllImport("__Internal", CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl, EntryPoint = "SV_Mono_Export_CmdArgs")]
        public static extern sbyte* CommandArguments();


        /*************************************************************************
        *
        *
        *	Random/Other Functions
        *
        *
        *************************************************************************/
        /**
        *	@brief	Export wrapper for Error.
        **/
        [DllImport("__Internal", CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl, EntryPoint = "SV_Mono_Export_Error")]
        public static extern void Error(string str);
        /**
        *	@brief	Export wrapper for SetModel.
        **/
        [DllImport("__Internal", CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl, EntryPoint = "SV_Mono_Export_SetModel")]
        public static extern void SetModel(Edict* ent, string name);
        /**
        *	@brief	Export wrapper for CVar.
        **/
        [DllImport("__Internal", CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl, EntryPoint = "SV_Mono_Export_CVar")]
        public static extern cvar_t* CVar(string name, string value, CvarFlags flags);
        /**
        *	@brief	Export wrapper for CVar_ForceSet.
        **/
        [DllImport("__Internal", CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl, EntryPoint = "SV_Mono_Export_CVar_ForceSet")]
        public static extern cvar_t* CVar_ForceSet(string name, string value);
        /**
        *	@brief	Export wrapper for PMove.
        **/
        [DllImport("__Internal", CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl, EntryPoint = "SV_Mono_Export_Pmove")]
        public static extern void Pmove(PmoveInterop* pm);
        /**
        *	@brief	Export wrapper for AddCommandString.
        **/
        [DllImport("__Internal", CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl, EntryPoint = "SV_Mono_Export_AddCommandString")]
        public static extern void AddCommandString(string str);
        /**
        *	@brief	Export wrapper for DebugGraph.
        **/
        [DllImport("__Internal", CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl, EntryPoint = "SV_Mono_Export_DebugGraph")]
        public static extern void DebugGraph(float value, int color);

	}; // Exports
}; // ServerGame