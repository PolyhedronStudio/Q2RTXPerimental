/********************************************************************
*
*
*	Save Game Mechanism: 
*
*	These are used to declare, and define an edict's global, 
*	non-static, function callbacks.
*
*	They will be automatically registered with the save game system
*	its linked list of function pointers. We use this to restore the
* 	callback function pointer address when loading a savegame.
*
*
********************************************************************/
#pragma once



/**
*	For use with "Non-Static-Member" function callbacks residing
*	outside of an edict class.
**/
//
//	Think:
//
//! Declares the global function in the notation style compatible for our use with
//! DEFINE_GLOBAL_CALLBACK_THINK
#if 1
#define DECLARE_GLOBAL_CLASSNAME_CALLBACK_THINK(className, functionName) \
	auto functionName( className *self ) -> void \

#endif
//!
#define DECLARE_GLOBAL_CALLBACK_THINK(functionName) \
	auto functionName( svg_base_edict_t *self ) -> void \
//
//	Spawn:
//
#if 1
#define DECLARE_GLOBAL_CLASSNAME_CALLBACK_SPAWN(className, functionName) \
	auto functionName( className *self ) -> void \

#endif
//!
#define DECLARE_GLOBAL_CALLBACK_SPAWN(functionName) \
	auto functionName( svg_base_edict_t *self ) -> void \
//
//	PostSpawn:
//
#if 1
#define DECLARE_GLOBAL_CLASSNAME_CALLBACK_POSTSPAWN(className, functionName) \
	auto functionName( className *self ) -> void \

#endif
//!
#define DECLARE_GLOBAL_CALLBACK_POSTSPAWN(functionName) \
	auto functionName( svg_base_edict_t *self ) -> void \
//
//	PreThink:
//
#if 1
#define DECLARE_GLOBAL_CLASSNAME_CALLBACK_PRETHINK(className, functionName) \
	auto functionName( className *self ) -> void \

#endif
//!
#define DECLARE_GLOBAL_CALLBACK_PRETHINK(functionName) \
	auto functionName( svg_base_edict_t *self ) -> void \

//
//	PostThink:
//
#if 1
#define DECLARE_GLOBAL_CLASSNAME_CALLBACK_POSTTHINK(className, functionName) \
	auto functionName( className *self ) -> void \

#endif
//!
#define DECLARE_GLOBAL_CALLBACK_POSTTHINK(functionName) \
	auto functionName( svg_base_edict_t *self ) -> void \
//
//	Blocked:
//
#if 1
#define DECLARE_GLOBAL_CLASSNAME_CALLBACK_BLOCKED(className, functionName) \
	auto functionName( className *self, svg_base_edict_t *other ) -> void \

#endif
//!
#define DECLARE_GLOBAL_CALLBACK_BLOCKED(functionName) \
	auto functionName( svg_base_edict_t *self, svg_base_edict_t *other ) -> void \
//
//	Touch:
//
#if 1
#define DECLARE_GLOBAL_CLASSNAME_CALLBACK_TOUCH(className, functionName) \
	auto functionName( className *self, svg_base_edict_t *other, const cm_plane_t *plane, cm_surface_t *surf )-> void \

#endif
//!
#define DECLARE_GLOBAL_CALLBACK_TOUCH(functionName) \
	auto functionName( svg_base_edict_t *self, svg_base_edict_t *other, const cm_plane_t *plane, cm_surface_t *surf) -> void \
//
//	Use:
//
#if 1
#define DECLARE_GLOBAL_CLASSNAME_CALLBACK_USE(className, functionName) \
	auto functionName( className *self, svg_base_edict_t *other, svg_base_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue ) -> void \

#endif
//!
#define DECLARE_GLOBAL_CALLBACK_USE(functionName) \
	auto functionName( svg_base_edict_t *self, svg_base_edict_t *other, svg_base_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue ) -> void \
//
//	OnSignalIn:
//
#if 1
#define DECLARE_GLOBAL_CLASSNAME_CALLBACK_ON_SIGNALIN(className, functionName) \
	auto functionName( className *self, svg_base_edict_t *other, svg_base_edict_t *activator, const char *signalName, const svg_signal_argument_array_t &signalArguments ) -> void \

#endif
//!
#define DECLARE_GLOBAL_CALLBACK_ON_SIGNALIN(functionName) \
	auto functionName( svg_base_edict_t *self, svg_base_edict_t *other, svg_base_edict_t *activator, const char *signalName, const svg_signal_argument_array_t &signalArguments ) -> void \
//
//	Pain:
//
#if 1
#define DECLARE_GLOBAL_CLASSNAME_CALLBACK_PAIN(className, functionName) \
	auto functionName( className *self, svg_base_edict_t *other, float kick, int damage ) -> void \

#endif
//!
#define DECLARE_GLOBAL_CALLBACK_PAIN(functionName) \
	auto functionName( svg_base_edict_t *self, svg_base_edict_t *other, float kick, int damage ) -> void \	
//
//	Die.
//
#if 1
#define DECLARE_GLOBAL_CLASSNAME_CALLBACK_DIE(className, functionName) \
	auto functionName( className *self, svg_base_edict_t *inflictor, svg_base_edict_t *attacker, int damage, vec_t *point ) -> void \

#endif
//!
#define DECLARE_GLOBAL_CALLBACK_DIE(functionName) \
	auto functionName( svg_base_edict_t *self, svg_base_edict_t *inflictor, svg_base_edict_t *attacker, int damage, vec_t *point ) -> void \


//! Defines the start of the function definition, awaiting to be prepended by its user
//! with the actual arguments and -> returnType;
//! 
//! aka
//! 
//! DEFINE_GLOBAL_CALLBACK_THINK(SVG_FreeEdict)(svg_base_edict_t *self) -> void {
//!		...
//! }
#define DEFINE_GLOBAL_CALLBACK_THINK(functionName) \
	static const svg_save_funcptr_instance_t save__global_ ##functionName(#functionName, FPTR_SAVE_TYPE_THINK, reinterpret_cast<void *>(##functionName) ); \
	auto functionName \
//! For Spawn.
#define DEFINE_GLOBAL_CALLBACK_SPAWN(functionName) \
	static const svg_save_funcptr_instance_t save__global__ ##functionName(#functionName, FPTR_SAVE_TYPE_SPAWN, reinterpret_cast<void *>(##functionName)); \
	auto functionName \
//! For PostSpawn.
#define DEFINE_GLOBAL_CALLBACK_POSTSPAWN(functionName) \
	static const svg_save_funcptr_instance_t save__global__ ##functionName(#functionName, FPTR_SAVE_TYPE_POSTSPAWN, reinterpret_cast<void *>(##functionName)); \
	auto functionName \
//! For PreThink
#define DEFINE_GLOBAL_CALLBACK_PRETHINK(functionName) \
	static const svg_save_funcptr_instance_t save__global__ ##functionName(#functionName, FPTR_SAVE_TYPE_PRETHINK, reinterpret_cast<void *>(##functionName)); \
	auto functionName \
//! For PostThink.
#define DEFINE_GLOBAL_CALLBACK_POSTTHINK(functionName) \
	static const svg_save_funcptr_instance_t save__global__ ##functionName(#functionName, FPTR_SAVE_TYPE_POSTSPAWN, reinterpret_cast<void *>(##functionName)); \
	auto functionName \
//! For Blocked.
#define DEFINE_GLOBAL_CALLBACK_BLOCKED(functionName) \
	static const svg_save_funcptr_instance_t save__global__ ##functionName(#functionName, FPTR_SAVE_TYPE_BLOCKED, reinterpret_cast<void *>(##functionName)); \
	auto functionName \
//! For Touch.
#define DEFINE_GLOBAL_CALLBACK_TOUCH(functionName) \
	static const svg_save_funcptr_instance_t save__global__ ##functionName(#functionName, FPTR_SAVE_TYPE_TOUCH, reinterpret_cast<void *>(##functionName)); \
	auto functionName \
//! For Use.
#define DEFINE_GLOBAL_CALLBACK_USE(functionName) \
	static const svg_save_funcptr_instance_t save__global__ ##functionName(#functionName, FPTR_SAVE_TYPE_USE, reinterpret_cast<void *>(##functionName)); \
	auto functionName \
//! For OnSignalIn.
#define DEFINE_GLOBAL_CALLBACK_ONSIGNALIN(functionName) \
	static const svg_save_funcptr_instance_t save__global__ ##functionName(#functionName, FPTR_SAVE_TYPE_ONSIGNALIN, reinterpret_cast<void *>(##functionName)); \
	auto functionName \
//! For Pain.
#define DEFINE_GLOBAL_CALLBACK_PAIN(functionName) \
	static const svg_save_funcptr_instance_t save__global__ ##functionName(#functionName, FPTR_SAVE_TYPE_PAIN, reinterpret_cast<void *>(##functionName)); \
	auto functionName \
//! For Die.
#define DEFINE_GLOBAL_CALLBACK_DIE(functionName) \
	static const svg_save_funcptr_instance_t save__global__ ##functionName(#functionName, FPTR_SAVE_TYPE_DIE, reinterpret_cast<void *>(##functionName)); \
	auto functionName \