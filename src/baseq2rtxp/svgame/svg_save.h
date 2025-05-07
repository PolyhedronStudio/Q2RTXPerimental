/********************************************************************
*
*
*	Save Game Mechanism: Works by iterating up the hierachy of 
*   derived classes and writing the fields in order of the derived 
*   class inheritance.
*
* 
********************************************************************/
#pragma once


// Forward declarations.
struct save_ptr_t;
struct svg_save_descriptor_funcptr_t;


/**
*	For accessing them outside of g_ptrs.cpp
**/
extern const svg_save_descriptor_funcptr_t save_ptrs[];
extern const int32_t num_save_ptrs;


/**
*
*
*
*   Data structures for declaring save-able callback function pointers.
*
*
*
**/
/**
*	@brief	Indexes what type of pointer is/was being read/written to.
**/
enum svg_funcptr_saveable_type_t : int32_t {
	//
	// invalid type.
	//
	FPTR_SAVEABLE_TYPE_BAD,

	//
	// edict-><methodname> function pointer addresses.
	//
	FPTR_SAVEABLE_TYPE_SPAWN,
	FPTR_SAVEABLE_TYPE_POSTSPAWN,
	FPTR_SAVEABLE_TYPE_PRETHINK,
	FPTR_SAVEABLE_TYPE_THINK,
	FPTR_SAVEABLE_TYPE_POSTTHINK,
	FPTR_SAVEABLE_TYPE_BLOCKED,
	FPTR_SAVEABLE_TYPE_TOUCH,
	FPTR_SAVEABLE_TYPE_USE,
	FPTR_SAVEABLE_TYPE_ONSIGNALIN,
	FPTR_SAVEABLE_TYPE_PAIN,
	FPTR_SAVEABLE_TYPE_DIE,

	//
	// edict->moveinfo.<methodname> function pointer addresses.
	//
	FPTR_SAVEABLE_TYPE_PUSHER_MOVEINFO_ENDMOVECALLBACK,

	//
	// edict->monsterinfo.<methodname> function pointer addresses.
	//
	FPTR_SAVEABLE_TYPE_MONSTERINFO_CURRENTMOVE,
	FPTR_SAVEABLE_TYPE_MONSTERINFO_NEXTMOVE,
	FPTR_SAVEABLE_TYPE_MONSTERINFO_STAND,
	FPTR_SAVEABLE_TYPE_MONSTERINFO_IDLE,
	FPTR_SAVEABLE_TYPE_MONSTERINFO_SEARCH,
	FPTR_SAVEABLE_TYPE_MONSTERINFO_WALK,
	FPTR_SAVEABLE_TYPE_MONSTERINFO_RUN,
	FPTR_SAVEABLE_TYPE_MONSTERINFO_DODGE,
	FPTR_SAVEABLE_TYPE_MONSTERINFO_ATTACK,
	FPTR_SAVEABLE_TYPE_MONSTERINFO_MELEE,
	FPTR_SAVEABLE_TYPE_MONSTERINFO_SIGHT,
	FPTR_SAVEABLE_TYPE_MONSTERINFO_CHECKATTACK
};

/**
*	@brief	A static counter, used by SaveableInstance to get compile-time IDs
**/
class StaticSaveableFuncPtrCounter {
public:
	//! The total number of known about TypeInfo classes.
	inline static size_t GlobalSaveableFuncPtrCount = 0U;

	//! Constructor.
	StaticSaveableFuncPtrCounter() {
		saveableFuncPtrID = GlobalSaveableFuncPtrCount;
		GlobalSaveableFuncPtrCount++;
	}

	/**
	*	@brief	Returns the ID of the TypeInfo class.
	**/
	inline size_t GetID() const {
		return saveableFuncPtrID;
	}

private:
	//! The ID of the StaticSaveableFuncPtrCounter class.
	size_t saveableFuncPtrID;
};

/**
*	@brief	Constructed as a 'forward linked list'.
**/
struct svg_funcptr_saveable_instance_t {
	/**
	*	@brief	Constructor for the svg_funcptr_saveable_instance_t class.
	**/
	svg_funcptr_saveable_instance_t( const char *name, svg_funcptr_saveable_type_t type, void *ptr );
	/**
	*	@brief	Gets the ptr which matches the passed name as well as the actual type of function pointer
	**/
	inline static svg_funcptr_saveable_instance_t *GetForNameType( const char *name, svg_funcptr_saveable_type_t type );
	/**
	*	@brief	Gets a ptr to the saveable instance which matches the passed name as well as the actual address function pointer
	**/
	inline static svg_funcptr_saveable_instance_t *GetForPointerType( svg_funcptr_saveable_type_t type, void *ptr );
	/**
*	@brief	Gets a ptr to the saveable instance which matches the actual address function pointer.
**/
	inline static svg_funcptr_saveable_instance_t *GetForPointer( void *ptr );
	/**
	*	@brief	Gets a ptr to the saveable instance which matches the passed saveAbleTypeID
	**/
	inline static svg_funcptr_saveable_instance_t *GetForTypeID( svg_funcptr_saveable_type_t type, const size_t saveAbleTypeID );


	//! The initial nullptr head svg_funcptr_saveable_instance_t list entry.
	inline static svg_funcptr_saveable_instance_t *head = nullptr;
	//! The next function pointer in the list.
	svg_funcptr_saveable_instance_t *previous = nullptr;

	//! The name of the function pointer.
	const char *name = nullptr;
	//! Hashed version of the name for faster iterating compares.
	uint32_t hashedName = 0;

	//! The type of callback save pointer we are dealing with.
	svg_funcptr_saveable_type_t saveAbleType = svg_funcptr_saveable_type_t::FPTR_SAVEABLE_TYPE_BAD;
	// <Q2RTXP>: WID: TODO: maybe generate a CRC32 for each classname instead?
	StaticEdictTypeInfoCounter saveAbleTypeID = {};
	//! Address to the actual function pointer, as registered in save_func_ptrs.
	void *ptr = nullptr;
};

/**
*	For use with "Non-Static-Member" function callbacks residing 
*	outside of an edict class.
**/
//
//	Think:
//
//! Declares the global function in the notation style compatible for our use with
//! DEFINE_GLOBAL_CALLBACK_THINK
#define DECLARE_GLOBAL_CLASSNAME_CALLBACK_THINK(className, functionName) \
	static auto functionName(className *self) -> void; \

//!
#define DECLARE_GLOBAL_CALLBACK_THINK(functionName) \
	DECLARE_GLOBAL_CLASSNAME_CALLBACK_THINK(svg_base_edict_t, functionName) \
//
//	Spawn:
//
#define DECLARE_GLOBAL_CLASSNAME_CALLBACK_SPAWN(className, functionName) \
	static auto functionName(className *self) -> void; \
//!
#define DECLARE_GLOBAL_CALLBACK_SPAWN(functionName) \
	DECLARE_GLOBAL_CLASSNAME_CALLBACK_SPAWN(svg_base_edict_t, functionName) \
//
//	PostSpawn:
//
#define DECLARE_GLOBAL_CLASSNAME_CALLBACK_POSTSPAWN(className, functionName) \
	static auto functionName(className *self) -> void; \
//!
#define DECLARE_GLOBAL_CALLBACK_POSTSPAWN(functionName) \
	DECLARE_GLOBAL_CLASSNAME_CALLBACK_POSTSPAWN(svg_base_edict_t, functionName) \
//
//	PreThink:
//
#define DECLARE_GLOBAL_CLASSNAME_CALLBACK_PRETHINK(className, functionName) \
	static auto functionName(className *self) -> void; \
//!
#define DECLARE_GLOBAL_CALLBACK_PRETHINK(functionName) \
	DECLARE_GLOBAL_CLASSNAME_CALLBACK_PRETHINK(svg_base_edict_t, functionName) \
//
//	PostThink:
//
#define DECLARE_GLOBAL_CLASSNAME_CALLBACK_POSTTHINK(className, functionName) \
	static auto functionName(className *self) -> void; \
//!
#define DECLARE_GLOBAL_CALLBACK_POSTTHINK(functionName) \
	DECLARE_GLOBAL_CLASSNAME_CALLBACK_POSTTHINK(svg_base_edict_t, functionName) \
//
//	Blocked:
//
#define DECLARE_GLOBAL_CLASSNAME_CALLBACK_BLOCKED(className, functionName) \
	static auto functionName(className *self, svg_base_edict_t *other ) -> void; \
//!
#define DECLARE_GLOBAL_CALLBACK_BLOCKED(functionName) \
	DECLARE_GLOBAL_CLASSNAME_CALLBACK_BLOCKED(svg_base_edict_t, functionName) \
//
//	Touch:
//
#define DECLARE_GLOBAL_CLASSNAME_CALLBACK_TOUCH(className, functionName) \
	static auto functionName(className *self, svg_base_edict_t *other, const cm_plane_t *plane, cm_surface_t *surf )-> void; \
//!
#define DECLARE_GLOBAL_CALLBACK_TOUCH(functionName) \
	DECLARE_GLOBAL_CLASSNAME_CALLBACK_TOUCH(svg_base_edict_t, functionName) \
//
//	Use:
//
#define DECLARE_GLOBAL_CLASSNAME_CALLBACK_USE(className, functionName) \
	static auto functionName(className *self, svg_base_edict_t *other, svg_base_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue ) -> void; \
//!
#define DECLARE_GLOBAL_CALLBACK_USE(functionName) \
	DECLARE_GLOBAL_CLASSNAME_CALLBACK_USE(svg_base_edict_t, functionName) \
//
//	OnSignalIn:
//
#define DECLARE_GLOBAL_CLASSNAME_CALLBACK_ON_SIGNALIN(className, functionName) \
	static auto functionName(className *self, svg_base_edict_t *other, svg_base_edict_t *activator, const char *signalName, const svg_signal_argument_array_t &signalArguments ) -> void; \
//!
#define DECLARE_GLOBAL_CALLBACK_ON_SIGNALIN(functionName) \
	DECLARE_GLOBAL_CLASSNAME_CALLBACK_ON_SIGNALIN(svg_base_edict_t, functionName) \
//
//	Pain:
//
#define DECLARE_GLOBAL_CLASSNAME_CALLBACK_PAIN(className, functionName) \
	static auto functionName(className *self, svg_base_edict_t *other, svg_base_edict_t *other, float kick, int damage ) -> void; \
//!
#define DECLARE_GLOBAL_CALLBACK_PAIN(functionName) \
	DECLARE_GLOBAL_CLASSNAME_CALLBACK_PAIN(svg_base_edict_t, functionName) \
//
//	Die.
//
#define DECLARE_GLOBAL_CLASSNAME_CALLBACK_DIE(className, functionName) \
	static auto functionName(className *self, svg_base_edict_t *inflictor, svg_base_edict_t *attacker, int damage, vec_t *point ) -> void; \
//!
#define DECLARE_GLOBAL_CALLBACK_DIE(functionName) \
	DECLARE_GLOBAL_CLASSNAME_CALLBACK_DIE(svg_base_edict_t, functionName) \


//! Defines the start of the function definition, awaiting to be prepended by its user
//! with the actual arguments and -> returnType;
//! 
//! aka
//! 
//! DEFINE_GLOBAL_CALLBACK_THINK(SVG_FreeEdict)(svg_base_edict_t *self) -> void {
//!		...
//! }
#define DEFINE_GLOBAL_CALLBACK_THINK(functionName) \
	static const svg_funcptr_saveable_instance_t save__global_ ##functionName(#functionName, FPTR_SAVEABLE_TYPE_THINK, reinterpret_cast<void *>(##functionName) ); \
	auto functionName \
//! For Spawn.
#define DEFINE_GLOBAL_CALLBACK_SPAWN(functionName) \
	static const svg_funcptr_saveable_instance_t save__global__ ##functionName(#functionName, FPTR_SAVEABLE_TYPE_SPAWN, reinterpret_cast<void *>(##functionName)); \
	auto functionName \
//! For PostSpawn.
#define DEFINE_GLOBAL_CALLBACK_POSTSPAWN(functionName) \
	static const svg_funcptr_saveable_instance_t save__global__ ##functionName(#functionName, FPTR_SAVEABLE_TYPE_POSTSPAWN, reinterpret_cast<void *>(##functionName)); \
	auto functionName \
//! For PreThink
#define DEFINE_GLOBAL_CALLBACK_PRETHINK(functionName) \
	static const svg_funcptr_saveable_instance_t save__global__ ##functionName(#functionName, FPTR_SAVEABLE_TYPE_PRETHINK, reinterpret_cast<void *>(##functionName)); \
	auto functionName \
//! For PostThink.
#define DEFINE_GLOBAL_CALLBACK_POSTTHINK(functionName) \
	static const svg_funcptr_saveable_instance_t save__global__ ##functionName(#functionName, FPTR_SAVEABLE_TYPE_POSTSPAWN, reinterpret_cast<void *>(##functionName)); \
	auto functionName \
//! For Blocked.
#define DEFINE_GLOBAL_CALLBACK_BLOCKED(functionName) \
	static const svg_funcptr_saveable_instance_t save__global__ ##functionName(#functionName, FPTR_SAVEABLE_TYPE_BLOCKED, reinterpret_cast<void *>(##functionName)); \
	auto functionName \
//! For Touch.
#define DEFINE_GLOBAL_CALLBACK_TOUCH(functionName) \
	static const svg_funcptr_saveable_instance_t save__global__ ##functionName(#functionName, FPTR_SAVEABLE_TYPE_TOUCH, reinterpret_cast<void *>(##functionName)); \
	auto functionName \
//! For Use.
#define DEFINE_GLOBAL_CALLBACK_USE(functionName) \
	static const svg_funcptr_saveable_instance_t save__global__ ##functionName(#functionName, FPTR_SAVEABLE_TYPE_USE, reinterpret_cast<void *>(##functionName)); \
	auto functionName \
//! For OnSignalIn.
#define DEFINE_GLOBAL_CALLBACK_ONSIGNALIN(functionName) \
	static const svg_funcptr_saveable_instance_t save__global__ ##functionName(#functionName, FPTR_SAVEABLE_TYPE_ONSIGNALIN, reinterpret_cast<void *>(##functionName)); \
	auto functionName \
//! For Pain.
#define DEFINE_GLOBAL_CALLBACK_PAIN(functionName) \
	static const svg_funcptr_saveable_instance_t save__global__ ##functionName(#functionName, FPTR_SAVEABLE_TYPE_PAIN, reinterpret_cast<void *>(##functionName)); \
	auto functionName \
//! For Die.
#define DEFINE_GLOBAL_CALLBACK_DIE(functionName) \
	static const svg_funcptr_saveable_instance_t save__global__ ##functionName(#functionName, FPTR_SAVEABLE_TYPE_DIE, reinterpret_cast<void *>(##functionName)); \
	auto functionName \


/**
*	For use with "Static-Member" function callbacks, residing inside 
*	an svg_base_edict_t or derived class.
**/
//! Generates a string such as: "className::functionName".
#define _DEFINE_MEMBER_CALLBACK_FULLNAME(className, functionName) \
	("" className "::" functionName "") \
//
//	Think:
//
//! Declares the member function in the notation style compatible for our use with
//! DEFINE_MEMBER_CALLBACK_THINK
#define DECLARE_MEMBER_CALLBACK_THINK(className, functionName) \
	static auto functionName(className *self) -> void; \
//
//	Spawn:
//
#define DECLARE_MEMBER_CALLBACK_SPAWN(className, functionName) \
	static auto functionName(className *self) -> void; \
//
//	PostSpawn:
//
#define DECLARE_MEMBER_CALLBACK_POSTSPAWN(className, functionName) \
	static auto functionName(className *self) -> void; \
//
//	PreThink:
//
#define DECLARE_MEMBER_CALLBACK_PRETHINK(className, functionName) \
	static auto functionName(className *self) -> void; \
//
//	PostThink:
//
#define DECLARE_MEMBER_CALLBACK_POSTTHINK(className, functionName) \
	static auto functionName(className *self) -> void; \
//
//	Blocked:
//
#define DECLARE_MEMBER_CALLBACK_BLOCKED(className, functionName) \
	static auto functionName(className *self, svg_base_edict_t *other ) -> void; \
//
//	Touch:
//
#define DECLARE_MEMBER_CALLBACK_TOUCH(className, functionName) \
	static auto functionName(className *self, svg_base_edict_t *other, const cm_plane_t *plane, cm_surface_t *surf )-> void; \
//
//	Use:
//
#define DECLARE_MEMBER_CALLBACK_USE(className, functionName) \
	static auto functionName(className *self, svg_base_edict_t *other, svg_base_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue ) -> void; \
//
//	OnSignalIn:
//
#define DECLARE_MEMBER_CALLBACK_ON_SIGNALIN(className, functionName) \
	static auto functionName(className *self, svg_base_edict_t *other, svg_base_edict_t *activator, const char *signalName, const svg_signal_argument_array_t &signalArguments ) -> void; \
//
//	Pain:
//
#define DECLARE_MEMBER_CALLBACK_PAIN(className, functionName) \
	static auto functionName(className *self, svg_base_edict_t *other, svg_base_edict_t *other, float kick, int damage ) -> void; \
//
//	Die.
//
#define DECLARE_MEMBER_CALLBACK_DIE(className, functionName) \
	static auto functionName(className *self, svg_base_edict_t *inflictor, svg_base_edict_t *attacker, int damage, vec_t *point ) -> void; \


//! Defines the start of the static member function definition, awaiting to be prepended by its user
//! with the actual arguments and -> returnType;
//! 
//! aka
//! 
//! DEFINE_MEMBER_CALLBACK_THINK(svg_monster_testdummy_t, monster_testdummy_think)(svg_monster_testdummy_t *self) -> void {
//!		...
//! }
#define DEFINE_MEMBER_CALLBACK_THINK(className, functionName) \
	static const svg_funcptr_saveable_instance_t save__ ##className ##__ ##functionName(_DEFINE_MEMBER_CALLBACK_FULLNAME(#className, #functionName), FPTR_SAVEABLE_TYPE_THINK, reinterpret_cast<void *>(##className::##functionName)); \
	auto className::##functionName \
//! For Spawn.
#define DEFINE_MEMBER_CALLBACK_SPAWN(className, functionName) \
	static const svg_funcptr_saveable_instance_t save__ ##className ##__ ##functionName(_DEFINE_MEMBER_CALLBACK_FULLNAME(#className, #functionName), FPTR_SAVEABLE_TYPE_SPAWN, reinterpret_cast<void *>(##className::##functionName)); \
	auto className::##functionName \
//! For PostSpawn.
#define DEFINE_MEMBER_CALLBACK_POSTSPAWN(className, functionName) \
	static const svg_funcptr_saveable_instance_t save__ ##className ##__ ##functionName(_DEFINE_MEMBER_CALLBACK_FULLNAME(#className, #functionName), FPTR_SAVEABLE_TYPE_POSTSPAWN, reinterpret_cast<void *>(##className::##functionName)); \
	auto className::##functionName \
//! For PreThink
#define DEFINE_MEMBER_CALLBACK_PRETHINK(className, functionName) \
	static const svg_funcptr_saveable_instance_t save__ ##className ##__ ##functionName(_DEFINE_MEMBER_CALLBACK_FULLNAME(#className, #functionName), FPTR_SAVEABLE_TYPE_PRETHINK, reinterpret_cast<void *>(##className::##functionName)); \
	auto className::##functionName \
//! For PostThink.
#define DEFINE_MEMBER_CALLBACK_POSTTHINK(className, functionName) \
	static const svg_funcptr_saveable_instance_t save__ ##className ##__ ##functionName(_DEFINE_MEMBER_CALLBACK_FULLNAME(#className, #functionName), FPTR_SAVEABLE_TYPE_POSTSPAWN, reinterpret_cast<void *>(##className::##functionName)); \
	auto className::##functionName \
//! For Blocked.
#define DEFINE_MEMBER_CALLBACK_BLOCKED(className, functionName) \
	static const svg_funcptr_saveable_instance_t save__ ##className ##__ ##functionName(_DEFINE_MEMBER_CALLBACK_FULLNAME(#className, #functionName), FPTR_SAVEABLE_TYPE_BLOCKED, reinterpret_cast<void *>(##className::##functionName)); \
	auto className::##functionName \
//! For Touch.
#define DEFINE_MEMBER_CALLBACK_TOUCH(className, functionName) \
	static const svg_funcptr_saveable_instance_t save__ ##className ##__ ##functionName(_DEFINE_MEMBER_CALLBACK_FULLNAME(#className, #functionName), FPTR_SAVEABLE_TYPE_TOUCH, reinterpret_cast<void *>(##className::##functionName)); \
	auto className::##functionName \
//! For Use.
#define DEFINE_MEMBER_CALLBACK_USE(className, functionName) \
	static const svg_funcptr_saveable_instance_t save__ ##className ##__ ##functionName(_DEFINE_MEMBER_CALLBACK_FULLNAME(#className, #functionName), FPTR_SAVEABLE_TYPE_USE, reinterpret_cast<void *>(##className::##functionName)); \
	auto className::##functionName \
//! For OnSignalIn.
#define DEFINE_MEMBER_CALLBACK_ONSIGNALIN(className, functionName) \
	static const svg_funcptr_saveable_instance_t save__ ##className ##__ ##functionName(_DEFINE_MEMBER_CALLBACK_FULLNAME(#className, #functionName), FPTR_SAVEABLE_TYPE_ONSIGNALIN, reinterpret_cast<void *>(##className::##functionName)); \
	auto className::##functionName \
//! For Pain.
#define DEFINE_MEMBER_CALLBACK_PAIN(className, functionName) \
	static const svg_funcptr_saveable_instance_t save__ ##className ##__ ##functionName(_DEFINE_MEMBER_CALLBACK_FULLNAME(#className, #functionName), FPTR_SAVEABLE_TYPE_PAIN, reinterpret_cast<void *>(##className::##functionName)); \
	auto className::##functionName \
//! For Die.
#define DEFINE_MEMBER_CALLBACK_DIE(className, functionName) \
	static const svg_funcptr_saveable_instance_t save__ ##className ##__ ##functionName(_DEFINE_MEMBER_CALLBACK_FULLNAME(#className, #functionName), FPTR_SAVEABLE_TYPE_DIE, reinterpret_cast<void *>(##className::##functionName)); \
	auto className::##functionName \



/**
*
* 
*
*   Game Descriptor Function Pointer Types:
*
* 
*
**/
/**
*	@brief	Indexes what type of pointer is/was being read/written to.
**/
enum svg_save_descriptor_funcptr_type_t : int32_t {
	//
	// invalid type.
	//
	FPTR_CALLBACK_BAD,

	//
	// edict-><methodname> function pointer addresses.
	//
	FPTR_CALLBACK_SPAWN,
	FPTR_CALLBACK_POSTSPAWN,
	FPTR_CALLBACK_PRETHINK,
	FPTR_CALLBACK_THINK,
	FPTR_CALLBACK_POSTTHINK,
	FPTR_CALLBACK_BLOCKED,
	FPTR_CALLBACK_TOUCH,
	FPTR_CALLBACK_USE,
	FPTR_CALLBACK_ONSIGNALIN,
	FPTR_CALLBACK_PAIN,
	FPTR_CALLBACK_DIE,
	//
	// edict->moveinfo.<methodname> function pointer addresses.
	//
	FPTR_CALLBACK_PUSHER_MOVEINFO_ENDMOVECALLBACK,
	//
	// edict->monsterinfo.<methodname> function pointer addresses.
	//
	FPTR_CALLBACK_MONSTERINFO_CURRENTMOVE,
	FPTR_CALLBACK_MONSTERINFO_NEXTMOVE,
	FPTR_CALLBACK_MONSTERINFO_STAND,
	FPTR_CALLBACK_MONSTERINFO_IDLE,
	FPTR_CALLBACK_MONSTERINFO_SEARCH,
	FPTR_CALLBACK_MONSTERINFO_WALK,
	FPTR_CALLBACK_MONSTERINFO_RUN,
	FPTR_CALLBACK_MONSTERINFO_DODGE,
	FPTR_CALLBACK_MONSTERINFO_ATTACK,
	FPTR_CALLBACK_MONSTERINFO_MELEE,
	FPTR_CALLBACK_MONSTERINFO_SIGHT,
	FPTR_CALLBACK_MONSTERINFO_CHECKATTACK
};

/**
*	@brief	Used for constructing our array(located in g_ptrs.cpp) containing all possible callback save methods and their type.
**/
struct svg_save_descriptor_funcptr_t {
	//! The type of callback save pointer we are dealing with.
	svg_save_descriptor_funcptr_type_t type;
	//! Address to the actual function pointer, as registered in save_func_ptrs.
	void *ptr;
};

/**
*	@brief	Error Types for SVG_Save_ValidateFuncPtr
**/
enum svg_save_descriptor_funcptr_error_t : int32_t {
	FPTR_ERROR_ADDRESS_MISMATCH,
	FPTR_ERROR_TYPE_MISMATCH,
	FPTR_ERROR_INVALID_DESCRIPTOR,
	FPTR_ERROR_SUCCESS
};
QENUM_BIT_FLAGS( svg_save_descriptor_funcptr_error_t );


/**
*
*
*   SaveGame Field Descriptors:
*
*
**/
/**
*   @brief  Field Descriptor Types:
**/
enum svg_save_descriptor_field_type_t : int32_t {
	SD_FIELD_TYPE_BAD,
	SD_FIELD_TYPE_BYTE,
	SD_FIELD_TYPE_SHORT,
	SD_FIELD_TYPE_INT32,
	SD_FIELD_TYPE_INT64,

	SD_FIELD_TYPE_BOOL,
	SD_FIELD_TYPE_FLOAT,
	SD_FIELD_TYPE_DOUBLE,

	SD_FIELD_TYPE_VECTOR3,
	SD_FIELD_TYPE_VECTOR4,
	SD_FIELD_TYPE_ANGLEHACK,

	SD_FIELD_TYPE_LSTRING,          // string on disk, pointer in memory, TAG_SVGAME_LEVEL
	SD_FIELD_TYPE_GSTRING,          // string on disk, pointer in memory, TAG_SVGAME
	SD_FIELD_TYPE_ZSTRING,          // string on disk, string in memory

	SD_FIELD_TYPE_FRAMETIME,		// Same as SD_FIELD_TYPE_INT64 but for QMTime.

	SD_FIELD_TYPE_LEVEL_QSTRING,	// string on disk, sg_qtag_string_t in memory, TAG_SVGAME_LEVEL
	SD_FIELD_TYPE_GAME_QSTRING,		// string on disk, sg_qtag_string_t in memory, TAG_SVGAME,

	SD_FIELD_TYPE_LEVEL_QTAG_MEMORY,	// variable sized memory blob on disk, sg_qtag-memory_t in memory, TAG_SVGAME_LEVEL
	SD_FIELD_TYPE_GAME_QTAG_MEMORY,		// variable sized memory blob on disk, sg_qtag-memory_t in memory, TAG_SVGAME,

	SD_FIELD_TYPE_EDICT,            // index on disk, pointer in memory
	SD_FIELD_TYPE_ITEM,             // index on disk, pointer in memory
	SD_FIELD_TYPE_CLIENT,           // index on disk, pointer in memory
	SD_FIELD_TYPE_FUNCTION,

	// WID: TODO: Store signal args array.
	//SD_FIELD_TYPE_SIGNAL_ARGUMENTS,

	SD_FIELD_TYPE_IGNORE,

	// WID: This was from Q2RTX 1.7.0
	//SD_FIELD_TYPE_FRAMETIME,         // speciality for savegame compatibility: float on disk, converted to framenum
};

/**
*   @brief  This is used to describe the fields of the svg_base_edict_t,
*           and its derivates, that need to be saved/loaded to drive.
**/
struct svg_save_descriptor_field_t {
	// Type of the save descriptor field.
	svg_save_descriptor_field_type_t type;
	//! String name descriptor.
	const char *name;
	//! Memory offset of the field.
	size_t offset;
	//! Size of the field in bytes.
	size_t size;
	//! Unused.
	int32_t flags;
};

/**
*	@brief	Checks whether the passed (save-) callback function pointer exists within
* 			the registered save pointer table, and has a matching type set.
**/
const svg_save_descriptor_funcptr_error_t SVG_Save_DebugValidateCallbackFuncPtr( svg_base_edict_t *edict, void *p, svg_save_descriptor_funcptr_type_t type, const std::string &functionName );


/**
*
*
*   Save Game Read Context:
* 
* 
**/
//! A read context for the game save/load system.
struct game_read_context_t {
    //! File handle.
    struct gzFile_s *f;

    //! A pointer to the list of saved callbacks.
    const svg_save_descriptor_funcptr_t *save_ptrs;
    //! The number of total enlisted callbacks.
    int32_t num_save_ptrs;

	//!
	//! 
	//! 
	//! 
	//! 
	void read_data( void *buf, size_t len );
	//! 
	const int read_short( );
	//!
	const int read_int( );
	//!
	const int64_t read_int64( );
	//! 
	const float read_float( );
	//! 
	const double read_double( );
	//! 
	char *read_string( );
	//!
	const svg_level_qstring_t read_level_qstring();
	//!
	const svg_game_qstring_t read_game_qstring();
	//! 
	template<typename T>
	sg_qtag_memory_t<T, TAG_SVGAME_LEVEL> *read_level_qtag_memory( sg_qtag_memory_t<T, TAG_SVGAME_LEVEL> *p );
	//! 
	template<typename T>
	sg_qtag_memory_t<T, TAG_SVGAME> *read_game_qtag_memory( sg_qtag_memory_t<T, TAG_SVGAME> *p );
	//! Read a buffer of data from the file.
	void read_zstring( char *s, size_t size );
	//! Read a vector3
	void read_vector3( vec_t *v );
	//! Read a vector4
	void read_vector4( vec_t *v );
	//! Read an index range from the file.
	void *read_index( size_t size, void *start, int max_index );
	//! Read the function pointer index from file.
	void *read_pointer( svg_save_descriptor_funcptr_type_t type );

	//! Read a field from the file.
	void read_field( const svg_save_descriptor_field_t *descriptorField, void *base );
	//! Read all fields from the file.
	void read_fields( const svg_save_descriptor_field_t *descriptorFields, void *base );
};



/**
*
*
*   Save Game Write Context:
*
*
**/
//! A write context for the game save/load system.
struct game_write_context_t {
    //! File handle.
    struct gzFile_s *f;

    //! A pointer to the list of saved callbacks.
    const svg_save_descriptor_funcptr_t *save_ptrs;
    //! The number of total enlisted callbacks.
    int32_t num_save_ptrs;

	//! Write a buffer of data to the file.
	void write_data( void *buf, size_t len );

	//! Write a short to the file.
	void write_int16( int16_t v );
	//! Write an int32 to the file.
	void write_int32( int32_t v );
	//! Write an int32 to the file.
	void write_int64( int64_t v );
	
	//! Write a float to the file.
	void write_float( float v );
	//! Write a double to the file.
	void write_double( double v );
	
	//! Write a string to the file.
	void write_string( char *s );
	//! Write a game qstring to the file.
	void write_game_qstring( svg_game_qstring_t *qstr );
	//! Write a level qstring to the file.
	void write_level_qstring( svg_level_qstring_t *qstr );
	//! Write a zstring to the file.
	//! Substituted by write_string in the save logic.
	//void write_zstring( const char *s );
	/**
	*   @brief  Write level qtag memory block to disk.
	**/
	template<typename T, int32_t tag = TAG_SVGAME_LEVEL>
	void write_level_qtag_memory( sg_qtag_memory_t<T, tag> *qtagMemory );
	/**
	*   @brief  Write game qtag memory block to disk.
	**/
	template<typename T, int32_t tag = TAG_SVGAME>
	void write_game_qtag_memory( sg_qtag_memory_t<T, tag> *qtagMemory );

	//! Write a vector3 to the file.
	void write_vector3( const vec_t *v );
	//! Write a vector4 to the file.
	void write_vector4( const vec_t *v );

	//! Write a pointer to the file.
	void write_index( const void *p, size_t size, const void *start, int max_index );
	//! Write a pointer to the file.
	void write_pointer( void *p, svg_save_descriptor_funcptr_type_t type, const svg_save_descriptor_field_t *saveField );

	//! Write a field to the file.
	void write_field( const svg_save_descriptor_field_t *descriptorField, void *base );
	//! Write all fields to the file.
	void write_fields( const svg_save_descriptor_field_t *descriptorFields, void *base );
};



/**
*
*
*
*   Macros for properly declaring(and thus implementing) the static 
*	save descriptor field logics for any svg_base_edict_t derived classes.
*
*
*
**/
/**
*   @brief  Opens the save descriptor field definition for the specified classType.
**/
#define SAVE_DESCRIPTOR_FIELDS_BEGIN(classType) \
    svg_save_descriptor_field_t classType::saveDescriptorFields[] = {\

/**
*   @brief  This macro actually declares the field descriptor. It is used as an internal macro for
*           the other macros which are used to define the save field descriptors with.
**/
#define _SAVE_DESCRIPTOR_FIELD(classType, fieldName, fieldType, fieldSize, flags)                            \
	{                                                                          \
		fieldType, #fieldName, q_offsetof(classType, fieldName), fieldSize, flags \
	}
/**
*   @brief  This macro is used to define a field descriptor for a field in the save descriptor.
**/
#define SAVE_DESCRIPTOR_DEFINE_FIELD(classType, fieldName, fieldType) _SAVE_DESCRIPTOR_FIELD(classType, fieldName, fieldType, 1, 0)
/**
*   @brief  This macro is used to define a field descriptor for a field in the save descriptor.
**/
#define SAVE_DESCRIPTOR_DEFINE_FUNCPTR(classType, fieldName, fieldType, funcPtrType) _SAVE_DESCRIPTOR_FIELD(classType, fieldName, fieldType, 1, funcPtrType )
/**
*   @brief  This macro is used to define a field descriptor for a field in the save descriptor.
*           It is used for fields that are arrays of a specific size.
**/
#define SAVE_DESCRIPTOR_DEFINE_FIELD_SIZE(classType, fieldName, fieldType, fieldSize) _SAVE_DESCRIPTOR_FIELD(classType, fieldName, fieldType, fieldSize, 0)
/**
*   @brief  This macro is used to define a field descriptor for a field in the save descriptor.
*           It is used for fields that are arrays of a specific size.
**/
#define SAVE_DESCRIPTOR_DEFINE_FIELD_ARRAY(classType, fieldName, fieldType, fieldSize) _SAVE_DESCRIPTOR_FIELD(classType, fieldName, fieldType, fieldSize, 0)

/**
*   @brief  Opens the save descriptor field definition for the specified classType.
**/
#define SAVE_DESCRIPTOR_FIELDS_END() \
    } \


/**
*
*
*
*   Macros for properly defining the static save descriptor field logics 
*	for any svg_base_edict_t derived classes.
*
*
*
**/
/**
*   @brief  Any derived class which needs support for saving implemented,
*           it declare support routines using this macro.
**/
#define SVG_SAVE_DESCRIPTOR_FIELDS_DECLARE_IMPLEMENTATION() \
    static svg_save_descriptor_field_t saveDescriptorFields[]; \
    virtual svg_save_descriptor_field_t *GetSaveDescriptorFields() override; \
    virtual int32_t GetSaveDescriptorFieldsCount() override; \
    virtual svg_save_descriptor_field_t *GetSaveDescriptorField( const char *name ) override;

/**
*   @brief  This macro defines the necessary implementations for the save descriptor fields functions:
*           GetSaveDescriptorFields(), GetSaveDescriptorFieldCount()  and GetSaveDescriptorField().
**/
#define SVG_SAVE_DESCRIPTOR_FIELDS_DEFINE_IMPLEMENTATION( classType, parentClassType ) \
	/*   @return A pointer of type `svg_save_descriptor_field_t` representing the save descriptor fields. */ \
	svg_save_descriptor_field_t *classType::GetSaveDescriptorFields() { \
		return classType::saveDescriptorFields; \
	} \
    /*  @return The number of save descriptor fields. */ \
    int32_t classType::GetSaveDescriptorFieldsCount() { \
		return sizeof( classType::saveDescriptorFields) / sizeof(svg_save_descriptor_field_t); \
    } \
    /*   A pointer to the save descriptor field with the given name. */\
    svg_save_descriptor_field_t *classType::GetSaveDescriptorField( const char *name ) { \
		/* Check if the name is valid. */\
		if ( !name ) { \
			return nullptr; \
		} \
		/* Check if the name is empty. */ \
		if ( *name == '\0' ) { \
			return nullptr; \
		} \
\
		/* Check if parent type has a save descriptor field.*/ \
		if ( parentClassType::GetSaveDescriptorField( name ) ) { \
			return parentClassType::GetSaveDescriptorField( name ); \
		} \
\
		/* Get pointer to the type's save descriptor fields.*/ \
		svg_save_descriptor_field_t *fields = classType::GetSaveDescriptorFields(); \
\
		/* Iterate this edict (derived-) type's save descriptor fields and return a pointer to it if found. */ \
        for ( int32_t i = 0; i < classType::GetSaveDescriptorFieldsCount(); i++ ) { \
			if ( strcmp( name, fields[ i ].name ) == 0 ) { \
				return &classType::saveDescriptorFields[ i ]; \
			} \
		} \
\
        /* Unable to find. */\
        return nullptr;\
    }\


















/******************************
* 
*	
*	LEGACY
* 
* 
*******************************/
#if 1
/**
*	@brief	Fields are needed for spawning from the entity string
*			and saving / loading games
**/
enum fieldtype_t : int32_t {
	F_BAD,
	F_BYTE,
	F_SHORT,
	F_INT,

	F_BOOL,
	F_FLOAT,
	F_DOUBLE,

	F_VECTOR3,
	F_VECTOR4,
	F_ANGLEHACK,

	F_LSTRING,          // string on disk, pointer in memory, TAG_SVGAME_LEVEL
	F_GSTRING,          // string on disk, pointer in memory, TAG_SVGAME
	F_ZSTRING,          // string on disk, string in memory

	F_LEVEL_QSTRING,	// string on disk, sg_qtag_string_t in memory, TAG_SVGAME_LEVEL
	F_GAME_QSTRING,		// string on disk, sg_qtag_string_t in memory, TAG_SVGAME,

	F_LEVEL_QTAG_MEMORY,	// variable sized memory blob on disk, sg_qtag-memory_t in memory, TAG_SVGAME_LEVEL
	F_GAME_QTAG_MEMORY,		// variable sized memory blob on disk, sg_qtag-memory_t in memory, TAG_SVGAME,

	F_EDICT,            // index on disk, pointer in memory
	F_ITEM,             // index on disk, pointer in memory
	F_CLIENT,           // index on disk, pointer in memory
	F_FUNCTION,
	F_POINTER,

	// WID: TODO: Store signal args array.
	//F_SIGNAL_ARGUMENTS,

	F_IGNORE,

	// WID: This was from Q2RTX 1.7.0
	//F_FRAMETIME,         // speciality for savegame compatibility: float on disk, converted to framenum
	// WID: However, we now got QMTime running on int64_t power.
	F_FRAMETIME,	// Same as F_INT64
	F_INT64
};

#if 0
/**
*	@brief	Indexes what type of pointer is/was being read/written to.
**/
enum ptr_type_t : int32_t {
    P_bad,

	//
	// edict-><methodname> function pointer addresses.
	//
	P_postspawn,
    P_prethink,
    P_think,
	P_postthink,
    P_blocked,
    P_touch,
    P_use,
	P_onsignalin,
    P_pain,
    P_die,

	//
	// edict->moveinfo.<methodname> function pointer addresses.
	//
    P_pusher_moveinfo_endmovecallback,

	//
	// edict->monsterinfo.<methodname> function pointer addresses.
	//
    P_monsterinfo_currentmove,
	P_monsterinfo_nextmove,
    P_monsterinfo_stand,
    P_monsterinfo_idle,
    P_monsterinfo_search,
    P_monsterinfo_walk,
    P_monsterinfo_run,
    P_monsterinfo_dodge,
    P_monsterinfo_attack,
    P_monsterinfo_melee,
    P_monsterinfo_sight,
    P_monsterinfo_checkattack
};

/**
*	@brief	Used for constructing our array(located in g_ptrs.cpp) containing all possible callback save methods and their type.
**/
struct save_ptr_t {
    ptr_type_t type;
    void *ptr;
};

/**
*	For accessing them outside of g_ptrs.cpp
**/
extern const save_ptr_t save_ptrs[];
extern const int num_save_ptrs;
#endif // #if 0

#endif