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
typedef char( &yes )[ 1 ];
typedef char( &no )[ 2 ];

template <typename B, typename D>
struct Host {
	operator B *( ) const;
	operator D *( );
};

template <typename B, typename D>
struct is_base_of {
	template <typename T>
	static yes check( D *, T );
	static no check( B *, int );

	static const bool value = sizeof( check( Host<B, D>(), int() ) ) == sizeof( yes );
};

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
	*	@param	mapClassName		The name of the map class associated with this EdictTypeInfo.
	*	@param	entClassName		The name of the entity class associated with this EdictTypeInfo.
	*	@param	superClassTypeName	The name of the superclass associated with this EdictTypeInfo.
	*	@param	flags			A set of flags that define specific properties of the EdictTypeInfo.
	*	@param	entityAllocator	A function pointer used to allocate instances of the entity.
	**/
	EdictTypeInfo( const char *worldSpawnClassName, const char *classTypeName, const char *superClassTypeName, const uint8_t flags, EdictAllocatorFncPtr edictTypeAllocator )
		: worldSpawnClassName( worldSpawnClassName ), classTypeName( classTypeName ), superClassTypeName( superClassTypeName ), typeFlags( flags ) {
		// Setup the edict instance allocation callback function pointer.
		allocateEdictInstanceCallback = edictTypeAllocator;
		// Previous is always head.
		prev = head;
		// Head becomes the newly created 'this'.
		head = this;
		// Generate a hash of the worldSpawn classname.
		worldSpawnClassNameHash = Q_HashCaseInsensitiveString( worldSpawnClassName );

		// This also gets properly set by TypeInfo initialization.
		super = GetByClassTypeName( superClassTypeName );
	}

	// This will be used to allocate instances of each entity class
	// In theory, multiple map classnames can allocate one C++ class
	EdictAllocatorFncPtr *allocateEdictInstanceCallback;

	/**
	*	@brief	Checks if the given type information corresponds to a class type.
	*	@param	typeInfo The type information to compare against.
	*	@return	True if the type information matches a class type, otherwise false.
	**/
	const bool IsClassTypeInfo( const EdictTypeInfo &typeInfo ) const {
		return classTypeInfoID.GetID() == typeInfo.classTypeInfoID.GetID();
	}

	/**
	*	@brief Determines if the current type is a subclass of the specified type.
	*	@param typeInfo The type information of the class to check against.
	*	@param derivedCheckOnly If true, skips checking if the current TypeInfo is a match.
	*	@return True if the current type is a subclass of the specified type, otherwise false.
	**/
	const bool IsSubClassOfTypeInfo( EdictTypeInfo &typeInfo, const bool derivedCheckOnly = false ) const {
		// We got a match, return true.
		if ( IsClassTypeInfo( typeInfo ) && !derivedCheckOnly ) {
			return true;
		}
		// No parent, we never had a match.
		if ( super == nullptr ) {
			return false;
		}
		// Iterate onto parent type.
		return super->IsSubClassOfTypeInfo( typeInfo, derivedCheckOnly );
	}

	/**
	*	@brief Determines if the current type is a subclass of the specified type.
	*	@param typeInfo The type information of the class to check against.
	*	@param derivedCheckOnly If true, skips checking if the current TypeInfo is a match.
	*	@return True if the current type is a subclass of the specified type, otherwise false.
	**/
	template<class TSubClass>
	const bool IsSubClassType( const bool derivedCheckOnly = false ) const {
		return IsSubClassOfTypeInfo( TSubClass::ClassInfo, derivedCheckOnly );
		//return IsSubClassOfTypeInfo( TSubClass::ClassInfo, derivedCheckOnly );
	}

	/**
	*	@brief	Checks if the type is abstract.
	*	@return	A boolean value indicating whether the type is abstract (true) or not (false).
	**/
	const bool IsAbstract() const {
		return typeFlags & TypeInfoFlag_Abstract;
	}

	/**
	*	@brief	Determines if an object can spawn based on its world spawn class name.
	*	@return	A boolean value indicating whether the object can spawn. Returns true if the object is not abstract and has the 'WorldSpawn' type flag set; otherwise, false.
	**/
	const bool CanWorldSpawn() const {
		return !IsAbstract() && ( typeFlags & TypeInfoFlag_WorldSpawn );
	}

	/**
	*	@return A boolean that indicates whether the entity can be spawned in-game.
	**/
	const bool CanGameSpawn() const {
		return !IsAbstract() && ( typeFlags & TypeInfoFlag_GameSpawn );
	}


	/**
	*	@brief	Retrieves type information based on a world spawn class name.
	*	@param	name The name of the world spawn class to search for. Must be a null-terminated string.
	*	@return	A pointer to the EdictTypeInfo object corresponding to the given class name, or nullptr if no match is found.
	**/
	static EdictTypeInfo *GetInfoByWorldSpawnClassName( const char *name ) {
		// Check if the input name is nullptr.
		if ( nullptr == name ) {
			return nullptr;
		}
		// Iterate over the linked list of EdictTypeInfo objects and set up their superclasses.
		EdictTypeInfo *current = head;
		// Keep on going until we run into nullptr.
		while ( current ) {
			// Return the current EdictTypeInfo object if the worldSpawnClassName type name matches.
			if ( !strcmp( current->worldSpawnClassName, name ) ) {
				return current;
			}
			// Move to the previous EdictTypeInfo object in the linked list.
			current = current->prev;
		}
		// If no match is found, return nullptr.
		return nullptr;
	}

	/**
	*	@brief	Retrieves type information based on a hashed world spawn class name.
	*	@param	hashedName The hashed representation of the world spawn class name to look up.
	*	@return A pointer to the EdictTypeInfo object corresponding to the hashed name, or nullptr if no match is found.
	**/
	static EdictTypeInfo *GetInfoByHashedWorldSpawnClassName( const uint32_t hashedName ) {
		// Check if the hashed name is zero, which indicates an invalid input.
		if ( hashedName == 0 ) {
			return nullptr;
		}
		// Iterate over the linked list of EdictTypeInfo objects and set up their superclasses.
		EdictTypeInfo *current = head;
		// Keep on going until we run into nullptr.
		while ( current ) {
			// Return the current EdictTypeInfo object if the hashed world spawn class name matches.
			if ( current->worldSpawnClassNameHash == hashedName ) {
				return current;
			}
			// Move to the previous EdictTypeInfo object in the linked list.
			current = current->prev;
		}
		// If no match is found, return nullptr.
		return nullptr;
	}

	/**
	*	@brief	Retrieves a EdictTypeInfo object by its C++ class type name.
	*	@param	name The name of the C++ class type to search for. Must be a null-terminated string.
	*	@return	A pointer to the EdictTypeInfo object corresponding to the given class type name, or nullptr if no match is found or if the input name is nullptr.
	**/
	static EdictTypeInfo *GetByClassTypeName( const char *name ) {
		// Check if the input name is nullptr.
		if ( name == nullptr ) {
			return nullptr;
		}
		// Iterate over the linked list of EdictTypeInfo objects and set up their superclasses.
		EdictTypeInfo *current = head;
		// Keep on going until we run into nullptr.
		while ( current ) {
			// Return the current EdictTypeInfo object if the class type name matches.
			if ( !strcmp( current->classTypeName, name ) ) {
				return current;
			}
			// Move to the previous EdictTypeInfo object in the linked list.
			current = current->prev;
		}
		// If no match is found, return nullptr.
		return nullptr;
	}

	/**
	*	@brief	This is called during game initialisation in order to setup 
	*			the superclasses for all EdictTypeInfo objects in the linked list.
	**/
	static void InitializeTypeInfoRegistry() {
		// Iterate over the linked list of EdictTypeInfo objects and set up their superclasses.
		EdictTypeInfo *current = head;
		// Keep on going until we run into nullptr.
		while ( current != nullptr ) {
			// Set the super pointer to the superclass type name.
			current->super = GetByClassTypeName( current->superClassTypeName );
			// Debug output.
			gi.dprintf( "ID: %d ClassName: %s, SuperClass: %s, TypeFlags: %i\n", current->classTypeInfoID.GetID(), current->classTypeName, current->superClassTypeName, current->typeFlags );
			// If the superclass type name is not found, set the super pointer to nullptr.
			current = current->prev;
		}
	}

	//! The initial nullptr head EdictTypeInfo list entry.
	inline static EdictTypeInfo *head = nullptr;

	// <Q2RTXP>: WID: TODO: maybe generate a CRC32 for each classname instead?
	inline static StaticEdictTypeInfoCounter classTypeInfoID = {};
	//! Specified certain rights for the EdictTypeInfo class.
	uint8_t	typeFlags = 0;

	//! A pointer linking to the next EdictTypeInfo class in the list.
	EdictTypeInfo *prev = nullptr;
	//! A pointer to the super EdictTypeInfo class
	EdictTypeInfo *super = nullptr;

	//! The classname as set by the map editor. (e.g. "info_player_start")
	const char *worldSpawnClassName = nullptr;
	//! A hashed version of the worldSpawnClassName.
	uint32_t worldSpawnClassNameHash = 0;
	
	//! The actual C++ type representation of the svg_base_edict_t derived class type.
	const char *classTypeName = nullptr;
	//! A hashed version of classTypeName
	uint32_t hashedClassTypeName = 0;
	
	//! The actual C++ type representation of this class' parent class type.
	const char *superClassTypeName = nullptr;
	//! A hashed version of superClassTypeName
	uint32_t hashedSuperClassTypeName = 0;
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
	using SelfType = className;		\
	using Super = superClass;	\
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
	using SelfType = className;		\
	using Super = superClass;	/* Allows us to refer to super class using Base */ \
	__DeclareTypeInfo( "abstract_"#className, #className, #superClass, EdictTypeInfo::TypeInfoFlag_Abstract, nullptr );




// Declares and initialises the type information for this class, so it can be spawned in a map. 
// NOTE: multiple inheritance not supported
// @param mapClassName (string) - the map classname of this entity, used during entity spawning
// @param classname (symbol) - the internal C++ class name
// @param superClass (symbol) - the class this entity class inherits from
#define DefineWorldSpawnClass( worldSpawnClassName, className, superClass, typeInfoFlags, spawnFunc )	\
	using SelfType = className;		\
	using Super = superClass;	\
	static svg_base_edict_t* AllocateInstance( const cm_entity_t* cm_entity ) {	\
		className *baseEdict = new className( cm_entity );	\
		baseEdict->classname = svg_level_qstring_t::from_char_str( worldSpawnClassName );	\
		baseEdict->SetSpawnCallback( reinterpret_cast<svg_edict_callback_spawn_fptr>( spawnFunc ) );	\
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
using Super = superClass;										\
static GameEntity* AllocateInstance( PODEntity* entity ) {		\
	classname *baseEntity = new classname( entity );			\
	baseEntity->SetClassname(#classname);						\
	entity->hashedClassname = baseEntity->GetTypeInfo()->hashedMapClass;	\
	entity->isLocal = true;										\
	return baseEntity;											\
}																\
__DeclareTypeInfo( #classname, #classname, #superClass, TypeInfo::TypeFlag_GameSpawn, &classname::AllocateInstance );
#endif