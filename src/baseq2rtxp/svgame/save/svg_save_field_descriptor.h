/********************************************************************
*
*
*	Save Game Mechanism: 
*
*	Every edict can implement save/load support by declaring and
*	defining the 'save descriptor fields' for the class type.
* 
*	The save descriptor fields are used to describe the fields of the
*	edict, and its derivates, that need to be saved/loaded to drive.
* 
*	A save descriptor field is a structure that contains the type of
*	field, the name of the field, the offset of the field in the
*	structure, the size of the field in bytes, and a set of flags.
* 
*
********************************************************************/
#pragma once


// Forward declarations.
struct svg_base_edict_t;



/**
*   @brief  Field Descriptor Types:
**/
enum svg_save_field_descriptor_type_t : int32_t {
	SD_FIELD_TYPE_BAD,
	SD_FIELD_TYPE_INT8,
	SD_FIELD_TYPE_INT16,
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

	SD_FIELD_TYPE_MAX_TYPES
	// WID: This was from Q2RTX 1.7.0
	//SD_FIELD_TYPE_FRAMETIME,         // speciality for savegame compatibility: float on disk, converted to framenum
};

/**
*   @brief  This is used to describe the fields of the svg_base_edict_t,
*           and its derivates, that need to be saved/loaded to drive.
**/
struct svg_save_descriptor_field_t {
	// Type of the save descriptor field.
	svg_save_field_descriptor_type_t type;
	//! String name descriptor.
	const char *name;
	//! Memory offset of the field.
	size_t offset;
	//! Size of the field in bytes.
	size_t size;
	//! Currently only used for storing an edict's actual function pointer callback type.
	int32_t flags;
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
*   Macros for properly declaring and defining the static 
*	save descriptor field logics for any svg_base_edict_t 
*	derived classes.
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
