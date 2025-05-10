/********************************************************************
*
*
*	Save Game Mechanism:
*
*	A linked list of saveable function pointers, used to save/load
*	edict_t(possibly other types) function pointers. These are added
*	to the list by using the save callback macros.
*
*
********************************************************************/
#pragma once


// Forward declarations.
struct svg_base_edict_t;



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
*	@brief	Indexes what type of pointer is/was being read/written to.
**/
enum svg_save_funcptr_type_t : int32_t {
	//
	// invalid type.
	//
	FPTR_SAVE_TYPE_BAD,

	//
	// edict-><methodname> function pointer addresses.
	//
	FPTR_SAVE_TYPE_SPAWN,
	FPTR_SAVE_TYPE_POSTSPAWN,
	FPTR_SAVE_TYPE_PRETHINK,
	FPTR_SAVE_TYPE_THINK,
	FPTR_SAVE_TYPE_POSTTHINK,
	FPTR_SAVE_TYPE_BLOCKED,
	FPTR_SAVE_TYPE_TOUCH,
	FPTR_SAVE_TYPE_USE,
	FPTR_SAVE_TYPE_ONSIGNALIN,
	FPTR_SAVE_TYPE_PAIN,
	FPTR_SAVE_TYPE_DIE,

	//
	// edict->moveinfo.<methodname> function pointer addresses.
	//
	FPTR_SAVE_TYPE_PUSHER_MOVEINFO_ENDMOVECALLBACK,

	//
	// edict->monsterinfo.<methodname> function pointer addresses.
	//
	FPTR_SAVE_TYPE_MONSTERINFO_CURRENTMOVE,
	FPTR_SAVE_TYPE_MONSTERINFO_NEXTMOVE,
	FPTR_SAVE_TYPE_MONSTERINFO_STAND,
	FPTR_SAVE_TYPE_MONSTERINFO_IDLE,
	FPTR_SAVE_TYPE_MONSTERINFO_SEARCH,
	FPTR_SAVE_TYPE_MONSTERINFO_WALK,
	FPTR_SAVE_TYPE_MONSTERINFO_RUN,
	FPTR_SAVE_TYPE_MONSTERINFO_DODGE,
	FPTR_SAVE_TYPE_MONSTERINFO_ATTACK,
	FPTR_SAVE_TYPE_MONSTERINFO_MELEE,
	FPTR_SAVE_TYPE_MONSTERINFO_SIGHT,
	FPTR_SAVE_TYPE_MONSTERINFO_CHECKATTACK
};

/**
*	@brief	Constructed as a 'forward linked list'.
**/
struct svg_save_funcptr_instance_t {
	/**
	*	@brief	Constructor for the svg_save_funcptr_instance_t class.
	**/
	svg_save_funcptr_instance_t( const char *name, svg_save_funcptr_type_t type, void *ptr );
	/**
	*	@brief	Gets the ptr which matches the passed name as well as the actual type of function pointer
	**/
	[[nodiscard]] inline static svg_save_funcptr_instance_t *GetForNameType( const char *name, svg_save_funcptr_type_t type );
	/**
	*	@brief	Gets a ptr to the saveable instance which matches the passed name as well as the actual address function pointer
	**/
	[[nodiscard]] inline static svg_save_funcptr_instance_t *GetForPointerType( svg_save_funcptr_type_t type, void *ptr );
	/**
	*	@brief	Gets a ptr to the saveable instance which matches the actual address function pointer.
	**/
	[[nodiscard]] static svg_save_funcptr_instance_t *GetForPointer( void *ptr );
	/**
	*	@brief	Gets a ptr to the saveable instance which matches the passed saveAbleTypeID
	**/
	[[nodiscard]] static svg_save_funcptr_instance_t *GetForTypeID( const size_t saveAbleTypeID );


	//! The initial nullptr head svg_save_funcptr_instance_t list entry.
	inline static svg_save_funcptr_instance_t *head = nullptr;
	//! The next function pointer in the list.
	svg_save_funcptr_instance_t *previous = nullptr;

	//! The name of the function pointer.
	const char *name = nullptr;
	//! Hashed version of the name for faster iterating compares.
	uint32_t hashedName = 0;

	//! The type of callback save pointer we are dealing with.
	svg_save_funcptr_type_t saveAbleType = svg_save_funcptr_type_t::FPTR_SAVE_TYPE_BAD;
	// <Q2RTXP>: WID: TODO: maybe generate a CRC32 for each classname instead?
	StaticSaveableFuncPtrCounter saveAbleTypeID = {};
	//! Address to the actual function pointer, as registered in save_func_ptrs.
	void *ptr = nullptr;
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
