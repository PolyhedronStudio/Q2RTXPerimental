/********************************************************************
*
*
*	Save Game Mechanism: 
*
*	These are used to declare, and define an edict's static member 
*	function callbacks.
* 
*	They will be automatically registered with the save game system
*	its linked list of function pointers. We use this to restore the
* 	callback function pointer address when loading a savegame.
* 
*
********************************************************************/
#pragma once



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
	static auto functionName(className *self, svg_base_edict_t *other, float kick, int damage ) -> void; \
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
	static const svg_save_funcptr_instance_t save__ ##className ##__ ##functionName(_DEFINE_MEMBER_CALLBACK_FULLNAME(#className, #functionName), FPTR_SAVE_TYPE_THINK, reinterpret_cast<void *>(##className::##functionName)); \
	auto className::##functionName \
//! For Spawn.
#define DEFINE_MEMBER_CALLBACK_SPAWN(className, functionName) \
	static const svg_save_funcptr_instance_t save__ ##className ##__ ##functionName(_DEFINE_MEMBER_CALLBACK_FULLNAME(#className, #functionName), FPTR_SAVE_TYPE_SPAWN, reinterpret_cast<void *>(##className::##functionName)); \
	auto className::##functionName \
//! For PostSpawn.
#define DEFINE_MEMBER_CALLBACK_POSTSPAWN(className, functionName) \
	static const svg_save_funcptr_instance_t save__ ##className ##__ ##functionName(_DEFINE_MEMBER_CALLBACK_FULLNAME(#className, #functionName), FPTR_SAVE_TYPE_POSTSPAWN, reinterpret_cast<void *>(##className::##functionName)); \
	auto className::##functionName \
//! For PreThink
#define DEFINE_MEMBER_CALLBACK_PRETHINK(className, functionName) \
	static const svg_save_funcptr_instance_t save__ ##className ##__ ##functionName(_DEFINE_MEMBER_CALLBACK_FULLNAME(#className, #functionName), FPTR_SAVE_TYPE_PRETHINK, reinterpret_cast<void *>(##className::##functionName)); \
	auto className::##functionName \
//! For PostThink.
#define DEFINE_MEMBER_CALLBACK_POSTTHINK(className, functionName) \
	static const svg_save_funcptr_instance_t save__ ##className ##__ ##functionName(_DEFINE_MEMBER_CALLBACK_FULLNAME(#className, #functionName), FPTR_SAVE_TYPE_POSTSPAWN, reinterpret_cast<void *>(##className::##functionName)); \
	auto className::##functionName \
//! For Blocked.
#define DEFINE_MEMBER_CALLBACK_BLOCKED(className, functionName) \
	static const svg_save_funcptr_instance_t save__ ##className ##__ ##functionName(_DEFINE_MEMBER_CALLBACK_FULLNAME(#className, #functionName), FPTR_SAVE_TYPE_BLOCKED, reinterpret_cast<void *>(##className::##functionName)); \
	auto className::##functionName \
//! For Touch.
#define DEFINE_MEMBER_CALLBACK_TOUCH(className, functionName) \
	static const svg_save_funcptr_instance_t save__ ##className ##__ ##functionName(_DEFINE_MEMBER_CALLBACK_FULLNAME(#className, #functionName), FPTR_SAVE_TYPE_TOUCH, reinterpret_cast<void *>(##className::##functionName)); \
	auto className::##functionName \
//! For Use.
#define DEFINE_MEMBER_CALLBACK_USE(className, functionName) \
	static const svg_save_funcptr_instance_t save__ ##className ##__ ##functionName(_DEFINE_MEMBER_CALLBACK_FULLNAME(#className, #functionName), FPTR_SAVE_TYPE_USE, reinterpret_cast<void *>(##className::##functionName)); \
	auto className::##functionName \
//! For OnSignalIn.
#define DEFINE_MEMBER_CALLBACK_ONSIGNALIN(className, functionName) \
	static const svg_save_funcptr_instance_t save__ ##className ##__ ##functionName(_DEFINE_MEMBER_CALLBACK_FULLNAME(#className, #functionName), FPTR_SAVE_TYPE_ONSIGNALIN, reinterpret_cast<void *>(##className::##functionName)); \
	auto className::##functionName \
//! For Pain.
#define DEFINE_MEMBER_CALLBACK_PAIN(className, functionName) \
	static const svg_save_funcptr_instance_t save__ ##className ##__ ##functionName(_DEFINE_MEMBER_CALLBACK_FULLNAME(#className, #functionName), FPTR_SAVE_TYPE_PAIN, reinterpret_cast<void *>(##className::##functionName)); \
	auto className::##functionName \
//! For Die.
#define DEFINE_MEMBER_CALLBACK_DIE(className, functionName) \
	static const svg_save_funcptr_instance_t save__ ##className ##__ ##functionName(_DEFINE_MEMBER_CALLBACK_FULLNAME(#className, #functionName), FPTR_SAVE_TYPE_DIE, reinterpret_cast<void *>(##className::##functionName)); \
	auto className::##functionName \
