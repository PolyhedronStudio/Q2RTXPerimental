/********************************************************************
*
*
*	ServerGame: TypeInfo system for linking a classname onto a C++
*   class type derived svg_edict_base_t.
* 
*   It keeps track of the class types and their classnames, allowing
*   to instantiate the class type from the classname. In doing so,
*   spawning them in-game.
* 
*   This is needed for us to be able to spawn entities during a map
*   load, and to be able to 'reset a map' in-game to its default state.
* 
*	NameSpace: "".
*
*
********************************************************************/
#pragma once


/**
*	@brief	A static counter, used by TypeInfo to get compile-time IDs
**/
class StaticEdictTypeInfoCounter {
public:
	//! The total number of known about TypeInfo classes.
	inline static size_t GlobalTypeInfoCount = 0U;

	//! Constructor.
	StaticEdictTypeInfoCounter() {
		typeInfoID = GlobalTypeInfoCount;
		GlobalTypeInfoCount++;
	}

	/**
	*	@brief	Returns the ID of the TypeInfo class.
	**/
	inline size_t GetID() const {
		return typeInfoID;
	}

private:
	size_t typeInfoID;
};

//! Actual entity allocator function pointer type.
struct svg_base_edict_t;
using EdictAllocatorFncPtr = svg_base_edict_t* ( const cm_entity_t *entityDictionary );



/**
* 
*	EdictTypeInfo:
*
*	The `EdictTypeInfo` class provides metadata and functionality for managing entity types in-game, 
*	including their hierachy order, spawnability, and allocation.
* 
**/
class EdictTypeInfo {
public:
	//! Types of EdictTypeInfo. Can be combined aka TypeInfoFlag_WorldSpawn | TypeInfoFlag_GameSpawn.
	enum {
		TypeInfoFlag_None = 0,
		// Cannot be allocated
		TypeInfoFlag_Abstract = 1 << 0,
		// Can be spawned in the map during map load.
		TypeInfoFlag_WorldSpawn = 1 << 1,
		// Can only be spawned in the map during gameplay.
		TypeInfoFlag_GameSpawn = 1 << 2
	};

public:
	/**
	*	@brief	Constructs a EdictTypeInfo object, initializing its properties and linking it into a global list of EdictTypeInfo objects.
	*	@param	mapClassName	The name of the map class associated with this EdictTypeInfo.
	*	@param	entClassName	The name of the entity class associated with this EdictTypeInfo.
	*	@param	superClassName	The name of the superclass associated with this EdictTypeInfo.
	*	@param	flags			A set of flags that define specific properties of the EdictTypeInfo.
	*	@param	entityAllocator	A function pointer used to allocate instances of the entity.
	**/
	EdictTypeInfo( const char *worldSpawnClassName, const char *classTypeName, const char *superClassName, const uint8_t flags, EdictAllocatorFncPtr edictTypeAllocator )
		: worldSpawnClassName( worldSpawnClassName ), classTypeName( classTypeName ), superClassTypeName( superClassTypeName ), typeFlags( flags ) {
		allocateEdictInstanceCallback = edictTypeAllocator;
		prev = head;
		head = this;
		hashedWorldSpawnClass = HashClassnameString( worldSpawnClassName, strlen( worldSpawnClassName ), 64 );

		// Doesn't actually quite work here, so I wrote SetupSuperClasses
		super = GetByClassTypeName( superClassName );
	}

	// This will be used to allocate instances of each entity class
	// In theory, multiple map classnames can allocate one C++ class
	EdictAllocatorFncPtr *allocateEdictInstanceCallback;

	/**
	*	@brief	Checks if the given type information corresponds to a class type.
	*	@param	typeInfo The type information to compare against.
	*	@return	True if the type information matches a class type, otherwise false.
	**/
	bool IsClass( const EdictTypeInfo &typeInfo ) const {
		return classTypeInfoID.GetID() == typeInfo.classTypeInfoID.GetID();
	}

	/**
	*	@brief Determines if the current type is a subclass of the specified type.
	*	@param typeInfo The type information of the class to check against.
	*	@return True if the current type is a subclass of the specified type, otherwise false.
	**/
	bool IsSubclassOf( const EdictTypeInfo &typeInfo ) const {
		if ( nullptr == super ) {
			return false;
		}

		if ( classTypeInfoID.GetID() == typeInfo.classTypeInfoID.GetID() ) {
			return true;
		}

		return super->IsSubclassOf( typeInfo );
	}

	/**
	*	@brief	Checks if the type is abstract.
	*	@return	A boolean value indicating whether the type is abstract (true) or not (false).
	**/
	bool IsAbstract() const {
		return typeFlags & TypeInfoFlag_Abstract;
	}

	/**
	*	@brief	Determines if an object can spawn based on its world spawn class name.
	*	@return	A boolean value indicating whether the object can spawn. Returns true if the object is not abstract and has the 'WorldSpawn' type flag set; otherwise, false.
	**/
	bool CanSpawnByWorldSpawnClassName() const {
		return !IsAbstract() && ( typeFlags & TypeInfoFlag_WorldSpawn );
	}

	/**
	*	@return A boolean that indicates whether the entity can be spawned in-game.
	**/
	bool CanSpawnInGame() const {
		return !IsAbstract() && ( typeFlags & TypeInfoFlag_GameSpawn );
	}

	/**
	*	@brief	A case-insensitive version of Com_HashString that hashes up to 'len'
	*			characters.
	**/
	static uint32_t HashClassnameString( const char *s, size_t len, unsigned size ) {
		// Winning answer from: https://stackoverflow.com/a/7666577 DJB2 hashing.
		unsigned long hash = 5381;
		int c;

		while ( c = *s++ )
			hash = ( ( hash << 5 ) + hash ) + c; /* hash * 33 + c */

		return hash;
		//uint32_t hash = 0;
		//while (*s && len--) {
		//	uint32_t c = PH_ToLower(*s++);
		//	hash = 127 * hash + c;
		//}

		//hash = (hash >> 20) ^(hash >> 10) ^ hash;
		//return hash & (size - 1);

	}

	/**
	*	@brief	Retrieves type information based on a world spawn class name.
	*	@param	name The name of the world spawn class to search for. Must be a null-terminated string.
	*	@return	A pointer to the EdictTypeInfo object corresponding to the given class name, or nullptr if no match is found.
	**/
	static EdictTypeInfo *GetInfoByWorldSpawnClassName( const char *name ) {
		if ( nullptr == name ) {
			return nullptr;
		}

		EdictTypeInfo *current = nullptr;
		current = head;

		while ( current ) {
			if ( !strcmp( current->worldSpawnClassName, name ) ) {
				return current;
			}
			current = current->prev;
		}

		return nullptr;
	}

	/**
	*	@brief	Retrieves type information based on a hashed world spawn class name.
	*	@param	hashedName The hashed representation of the world spawn class name to look up.
	*	@return A pointer to the EdictTypeInfo object corresponding to the hashed name, or nullptr if no match is found.
	**/
	static EdictTypeInfo *GetInfoByHashedWorldSpawnClassName( const uint32_t hashedName ) {
		if ( hashedName == 0 ) {
			return nullptr;
		}

		EdictTypeInfo *current = nullptr;
		current = head;

		while ( current ) {
			if ( current->hashedWorldSpawnClass == hashedName ) {
				return current;
			}
			current = current->prev;
		}

		return nullptr;
	}

	/**
	*	@brief	Retrieves a EdictTypeInfo object by its C++ class type name.
	*	@param	name The name of the C++ class type to search for. Must be a null-terminated string.
	*	@return	A pointer to the EdictTypeInfo object corresponding to the given class type name, or nullptr if no match is found or if the input name is nullptr.
	**/
	static EdictTypeInfo *GetByClassTypeName( const char *name ) {
		if ( nullptr == name ) {
			return nullptr;
		}

		EdictTypeInfo *current = nullptr;
		current = head;

		while ( current ) {
			if ( !strcmp( current->classTypeName, name ) ) {
				return current;
			}
			current = current->prev;
		}

		return nullptr;
	}

	/**
	*	@brief	This is called during game initialisation in order to setup 
	*			the superclasses for all EdictTypeInfo objects in the linked list.
	**/
	static void SetupSuperClasses() {
		EdictTypeInfo *current = nullptr;
		current = head;

		while ( current ) {
			current->super = GetByClassTypeName( current->superClassTypeName );
			current = current->prev;
		}
	}

	//! The initial nullptr head EdictTypeInfo list entry.
	inline static EdictTypeInfo *head = nullptr;

	// <Q2RTXP>: WID: TODO: maybe generate a CRC32 for each classname instead?
	StaticEdictTypeInfoCounter classTypeInfoID;
	//! Specified certain rights for the EdictTypeInfo class.
	uint8_t	typeFlags;
	//! A pointer linking to the next EdictTypeInfo class in the list.
	EdictTypeInfo *prev;
	//! A pointer to the super EdictTypeInfo class
	EdictTypeInfo *super;

	//! The classname as set by the map editor. (e.g. "info_player_start")
	const char *worldSpawnClassName;
	//! A hashed version of the worldSpawnClassName.
	uint32_t hashedWorldSpawnClass;
	//! The actual string representation of the svg_base_edict_t derived class name within C++.
	const char *classTypeName;
	//! A hashed version of classTypeName
	//uint32_t hashedClassTypeName;
	//! The actual string representation of the svg_base_edict_t parent base class name within C++.
	const char *superClassTypeName;
	//! A hashed version of superClassTypeName
	//uint32_t hashedSuperClassTypeName;
};



/**
*
*
*
*	TypeInfo Macros for use with any derived svg_base_edict_t class.
*
*
*
**/
// Top abstract class, the start of the class tree 
// Instances of this cannot be allocated, as it is abstract.
#define __DeclareTopRootTypeInfo( worldSpawnClassName, className, superClass, typeInfoFlags, allocatorFunction )	\
	virtual inline EdictTypeInfo* GetTypeInfo() const {	\
		return &ClassInfo;	\
	}	\
	inline static EdictTypeInfo ClassInfo = EdictTypeInfo( (worldSpawnClassName), (#className), (#superClass), (typeInfoFlags), (allocatorFunction) );

// Top abstract class, the start of the class tree
#define DefineTopRootClass( worldSpawnClassName, className, superClass, typeInfoFlags )	\
	using Base = superClass;	\
	static className* AllocateInstance( const cm_entity_t* cm_entity ) {	\
		className *baseEdict = new className( cm_entity );	\
		baseEdict->classname = svg_level_qstring_t::from_char_str( worldSpawnClassName );	\
		/*baseEdict->hashedClassname = baseEdict->GetTypeInfo()->hashedMapClass;*/ \
		return baseEdict;	\
	}	\
	__DeclareTopRootTypeInfo( worldSpawnClassName, className, superClass, typeInfoFlags, &className::AllocateInstance );

//__DeclareTopRootTypeInfo( worldSpawnClassName, className, nullptr, EdictTypeInfo::TypeInfoFlag_WorldSpawn | typeInfoFlags, allocatorFunction );

/**
*	@brief	Declares the type information for a class, including its name, superclass, flags, and allocator function.
*	@param	worldSpawnClassName The name of the world spawn class associated with this type.
*	@param	classname The name of the class.
*	@param	superClass The name of the superclass for this type.
*	@param	typeFlags The flags associated with this type.
*	@param	allocatorFunction The function used to allocate instances of this type.
*	@note	This macro should be used in the class definition to declare the type information.
**/
#define __DeclareTypeInfo( worldSpawnClassName, className, superClass, typeInfoFlags, allocatorFunction )	\
	virtual inline EdictTypeInfo* GetTypeInfo() const override {	\
		return &ClassInfo;											\
	}																\
	inline static EdictTypeInfo ClassInfo = EdictTypeInfo( (worldSpawnClassName), (#className), (#superClass), (typeInfoFlags), (allocatorFunction) );

// Abstract class that inherits from another 
// Instances of this cannot be allocated 
// NOTE: multiple inheritance not supported
#define DefineAbstractClass( className, superClass )			\
	using Base = superClass;	/* Allows us to refer to super class using Base */ \
	__DeclareTypeInfo( #className, #className, #superClass, EdictTypeInfo::TypeFlag_Abstract, nullptr );




// Declares and initialises the type information for this class, so it can be spawned in a map. 
// NOTE: multiple inheritance not supported
// @param mapClassName (string) - the map classname of this entity, used during entity spawning
// @param classname (symbol) - the internal C++ class name
// @param superClass (symbol) - the class this entity class inherits from
#define DefineWorldSpawnClass( worldSpawnClassName, className, superClass, typeInfoFlags )	\
	using Base = superClass;	\
	static svg_base_edict_t* AllocateInstance( const cm_entity_t* cm_entity ) {	\
		className *baseEdict = new className( cm_entity );	\
		baseEdict->classname = svg_level_qstring_t::from_char_str( worldSpawnClassName );	\
		/*baseEdict->hashedClassname = baseEdict->GetTypeInfo()->hashedMapClass;*/ \
		return baseEdict;	\
	}	\
	__DeclareTypeInfo( worldSpawnClassName, className, superClass, typeInfoFlags, &className::AllocateInstance );


#if 0
// Declares and initialises the type information for this class, so it can be spawned during gameplay. 
// NOTE: multiple inheritance not supported
// @param mapClassName (string) - the map classname of this entity, used during entity spawning
// @param classname (symbol) - the internal C++ class name
// @param superClass (symbol) - the class this entity class inherits from
#define DefineGameClass( classname, superClass )	\
using Base = superClass;										\
static GameEntity* AllocateInstance( PODEntity* entity ) {		\
	classname *baseEntity = new classname( entity );			\
	baseEntity->SetClassname(#classname);						\
	entity->hashedClassname = baseEntity->GetTypeInfo()->hashedMapClass;	\
	entity->isLocal = true;										\
	return baseEntity;											\
}																\
__DeclareTypeInfo( #classname, #classname, #superClass, TypeInfo::TypeFlag_GameSpawn, &classname::AllocateInstance );
#endif