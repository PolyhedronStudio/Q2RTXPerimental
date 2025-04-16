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


/**
*   @brief  Save Field Descriptor Types:
**/
enum svg_save_descriptor_field_type_t : int32_t {
    SD_FIELD_TYPE_BAD,
    SD_FIELD_TYPE_BYTE,
    SD_FIELD_TYPE_SHORT,
    SD_FIELD_TYPE_INT32,

    SD_FIELD_TYPE_BOOL,
    SD_FIELD_TYPE_FLOAT,
    SD_FIELD_TYPE_DOUBLE,

    SD_FIELD_TYPE_VECTOR3,
    SD_FIELD_TYPE_VECTOR4,
    SD_FIELD_TYPE_ANGLEHACK,

    SD_FIELD_TYPE_LSTRING,          // string on disk, pointer in memory, TAG_SVGAME_LEVEL
    SD_FIELD_TYPE_GSTRING,          // string on disk, pointer in memory, TAG_SVGAME
    SD_FIELD_TYPE_ZSTRING,          // string on disk, string in memory

    SD_FIELD_TYPE_LEVEL_QSTRING,	// string on disk, sg_qtag_string_t in memory, TAG_SVGAME_LEVEL
    SD_FIELD_TYPE_GAME_QSTRING,		// string on disk, sg_qtag_string_t in memory, TAG_SVGAME,

    SD_FIELD_TYPE_LEVEL_QTAG_MEMORY,	// variable sized memory blob on disk, sg_qtag-memory_t in memory, TAG_SVGAME_LEVEL
    SD_FIELD_TYPE_GAME_QTAG_MEMORY,		// variable sized memory blob on disk, sg_qtag-memory_t in memory, TAG_SVGAME,

    SD_FIELD_TYPE_EDICT,            // index on disk, pointer in memory
    SD_FIELD_TYPE_ITEM,             // index on disk, pointer in memory
    SD_FIELD_TYPE_CLIENT,           // index on disk, pointer in memory
    SD_FIELD_TYPE_FUNCTION,
    SD_FIELD_TYPE_POINTER,

    // WID: TODO: Store signal args array.
    //SD_FIELD_TYPE_SIGNAL_ARGUMENTS,

    SD_FIELD_TYPE_IGNORE,

    // WID: This was from Q2RTX 1.7.0
    //SD_FIELD_TYPE_FRAMETIME,         // speciality for savegame compatibility: float on disk, converted to framenum
    // WID: However, we now got QMTime running on int64_t power.
    SD_FIELD_TYPE_FRAMETIME,	// Same as SD_FIELD_TYPE_INT64
    SD_FIELD_TYPE_INT64
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
*
*
*
*   Macros for implementing the static save descriptor fields
*   array into any svg_base_edict_t derived classes.
*
*
*
**/
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
*           It is used for fields that are arrays of a specific size.
**/
#define SAVE_DESCRIPTOR_DEFINE_FIELD_SIZE(classType, fieldName, fieldType, fieldSize) _SAVE_DESCRIPTOR_FIELD(classType, fieldName, fieldType, fieldSize, 0)
/**
*   @brief  This macro is used to define a field descriptor for a field in the save descriptor.
*           It is used for fields that are arrays of a specific size.
**/
#define SAVE_DESCRIPTOR_DEFINE_FIELD_ARRAY(classType, fieldName, fieldType, fieldSize) _SAVE_DESCRIPTOR_FIELD(classType, fieldName, fieldType, fieldSize, 0)



/**
*
* 
* 
*   Macros for implementing the save descriptor fields into
*   any svg_base_edict_t derived classes.
* 
* 
* 
**/
/**
*   @brief  Any derived class which needs support for saving implemented,
*           it declare support routines using this macro.
**/
#define SVG_SAVE_DESCRIPTOR_FIELDS_DECLARE_IMPLEMENTATION() \
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
    }





















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
typedef struct {
    ptr_type_t type;
    void *ptr;
} save_ptr_t;

/**
*	For accessing them outside of g_ptrs.cpp
**/
extern const save_ptr_t save_ptrs[];
extern const int num_save_ptrs;
#endif