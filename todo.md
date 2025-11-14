# Quake II RTXPerimental Notes:
These are mainly my personal notes/ideas/interests, and do not per se reflect the actual changes to be made.

## Notes:
### Random Ideas for The Day:
* If we had event entities and then 'morphentity' function, for example: a blaster bullet could convert to an entity, eliminating
the need for temp_entity_t behavior. For hit trace based weapons I suppose the hits could be done client side but that'd require
simulating a frame ahead for all things. Either way, weapon could looks like it might be better off to go to shared some day.
---
#### Animated Brush Textures:
* [ ] Animated Brush Textures: Any brush entity with an animated texture needs to be able to configure its
animations for open/closed/transit-in/transit-out states throughout TB editor. (``func_button,func_door etc``)
---
#### UseTarget Trigger System (To do, or not to do?):
* [ ] Add trigger_state_t and trigger_type_t to store the entity trigger state, entity_usetarget_type_t, the latter holds the usetarget type for +usetarget interaction.
* [ ] Add in the FGD, the basis for triggertype and triggervalue.
* [ ] This'll allow us to determine how an entity should proceed to trigger any other entity.
* [ ] Unify signal names, i.e DoorLock RotatingLock just use Lock, etc etc.


---
## Bugfixes: 
Ideally this list would never exist, but in this world we can't have it all so, let me introduce you to a highly most pleasant list of bugs to fix!
### Highest:
* [x] None
### High Priority:
* [x] None
### Medium Priority:
* [ ] func_door when started open, does not seem to respond to touch trigger brush areas properly..
* [X] Makkon map, when entering the garage area, ensuring the door is out of PVS, then back in, prediction fails for client somehow. (As if the touch trigger is involved?)
* [X] EAX sometimes remains as if we're underwater after exiting water. Re-entering and exiting usually helps though.
* [ ] Find the bug that is currently making the OctagonHull not enjoy colliding to certain specific bounding boxes.
	We'll just go for capsules instead. Perhaps look into even utilizing Jolt Physics at a bare minimum level for geometric clip testing.
### Low Priority:
* [ ] - The demo playback seems to read in an incorrect client number at times, causing it to
	sometimes not show the player model and not play sound properly.

---
## V0.0.7 TODO:
These items are to be done before we can call it a day for v0.0.7.
### Add:
- [ ] Ensure the weapon model material is an actual 'chrome'/'metal' PBR material.
- [ ] Add new weapons: shotgun, machinegun, grenades(flash,smoke,frag), crossbow, knife, and taser.
	- It may vary from what is written down here. It is just a list of ideas.
- [ ] Add our own variation of the CS 'hostage rescue' type of inspired gamemode.
	- [ ] This requires a minimal UI to be implemented.
### Fix:
- [X] Fix the monster code to properly work with gravity, and to properly use the new skeletal model system.
- [ ] Nothing yet.
### Refactor/Rework:
- [ ] Third-person player model and other client animations. (This has been left unfinished).



---
## V0.0.6 TODO:
These items are to be done before we can call it a day for v0.0.6.
### Add:
- [ ] Create a basic deathmatch map, weapons and ammo scattered around, with a set of "custom" PBR materials. ( We got no "buy" menu yet. )
- [ ] Two proper game modes to choose from, deathmatch and team deathmatch.
- [ ] Add a basic hostage rescue gamemode, which will be the first gamemode to be implemented.
	- [ ] This requires a minimal UI to be implemented.
	- [ ] Change the monster_testdummy to an actual hostage monster that we can activate for it to follow us around, and which we can rescue at the end of the map.
	- [ ] Check physics.cpp because we got MOVETYPE_ROOTMOTION, we want to combine the logic of the monster_testdummy that it holds right now, with the movetype, and abstract it away so our monster code remains clean. Yes..._
### Fix:
- [ ] Nothing yet.
### Refactor/Rework:
- [ ] Nothing yet.


---
## Generic TODO:
These are generic TODO items that are not specific to a version, but rather to the project as a whole.

### Common/Collision:
#### Add:
* [ ] A SIMPLE but networked-capable User Interface designed for game mode related user interface elements.
	* [ ] Begin Playing/Spectating Menu??
	* [ ] Select Team Menu.
	* [ ] (Buy?-)Inventory Menu. Obviously this requires more weapons first lol.
* [ ] Move part of weapon processing logic to "playermove", effectively requiring us to rename the mechanic. Perhaps a "Controller" as used in other engines is a good idea?
#### Fix:
* [ ] None.

### VKPT:
#### Add:
* [ ] None.
#### Fix:
* [ ] None.

### Client:
#### Add:
* [ ] Check client download.cpp and add in support for 'SPJ Ident' and 'SPJ Data' for the client to download.
#### Fix:
* [ ] Technically Client Game related also, ensure that the proper (back-) lerp/xerp fractions are used in-place for playerstate and error transitions.

### Server:
#### Add:
* [ ] Check client download.cpp and add in support for 'SPJ Ident' and 'SPJ Data' for the client to download.
* [ ] When Linking an entity, calculate the absolute and relative transform matrices for the entity.
#### Fix:
* [ ] None.

### Client Game:
#### Add:
* [ ] A nice looking, decent HUD, which possibly will also serve its functionality for the currently boring crosshair.
#### Fix:
* [ ] None.

### Server Game:
#### Add:
* [ ] Replace all cvar deathmatch/coop checks with svgame gamemode checks.
	* [ ] Implement a sharedgame ``base`` gamemode structure, this is the base which will contain functionality necessary for both the client and servergame. 
	* [ ] Each client/server game mode will inherit from the ``base``.
* [ ] Add Save/Load support for the (optional) Lua's in-game mapState table data.
* [ ] Add in support for signal_argument_t array.
* [ ] Rework the 'movewith' system using matrixes/quaternions instead of those silly vector maths.
#### Fix:
* [ ] Player animations are still not nice...
#### Experiment:
* [ ] Add a func_door_model which allows for a model to be used as a door, and which can be animated using the IQM animation system.


---
## TODO v0.0.6:
- [ ] **Core/Generic/Code-Style/Important(DoNotForget)**:
	- [X] Fix Save/Load games, the state for client(mostly weaponry) seems to not be (re-)stored properly.
		- [ ] Add Save/Load for the (optional) Lua's in-game mapState table data.
		- [ ] Add in support for signal_argument_t array.
		- [X] Fix pushMoveInfo.curve.positions array, it is dynamic, oof..
			- [X] Fixed by implementing sg_qtag_memory_t however, 
				- [ ] it needs support for multiple types?
				- [ ] 
- [ ] **LUA**:
	- [ ] Look into making it easy to hook up svg_edict_t derived classes to Lua.
- [ ] **HUD**:
	- [ ] Certain people get iffy and uppety about crosshair as it is. So, let's make it configurable.

- [ ] **Entities**:
	- [ ] Rewrite/Rework/Fix func_plat. It needs a different set of usage rules._
	- [ ] Reimplement the 'movewith' system using matrixes/quaternions instead of those silly vector maths.
		- [ ] Calculate the proper entity matrixes/quaternions during Link time.
	- [ ] Reimplement (client-)misc_model properly.
	- [ ] Add proper spawn flag constants.
	- [X] Can we do a, C++ struct inheritance and have edict_t* store a pointer to an instance of the matching entity classname and its 'classdata' struct.
		- [X] Add in a TypeInfo system allowing for:
			- [X] Registering an inhericted svg_edict_t to a classname type.
			- [X] Allocating the matching class type linked to an entity's classname key/value pair.
			- [X] Registering an inherited svg_edict_t class its extra save field offsets.
				- [X] We want to do this Q2RE style since it is more convenient, lesser error prone.
			- [ ] Registering an inherited svg_edict_t class its extra messaging field offsets.

- [ ] The **IQM Animation** Scenario:
	- [ ] Allow attaching models configured by bone tags setup.
	- [ ] Implement model events for animations:
		- [ ] Footsteps implemented using this.
	- [ ] Experiment with Head/Torso/Hips separated animation actions.
		- [ ] Create torso anim actions for fists, pistol, and rifle.
		- [ ] Create hips anims that dominate for crouch, walk, run, and jump.
		- [ ] Event anims are derived for torso based on the active weapon.
		- [ ] Have a good time with Blender :-)
	- [X] For both, client and server, whenever an IQM file is loaded, so will be its matching
			 .cfg file. This file will contain:
	- [X] Name of Root Bone.
	- [ ] Bone Names for the ``Joint Controllers``.
	- [-] Tags assigned to a bone name, with a corresponding ``up/forward/right`` axis set.
		- [ ] Technically, it's probably easier to just use code for this since it allwos for more control as well. 
	- [ ] Animation Events, ``animevent "(animname)" (animframe) (client/server/both) "(animevent data, can be a string)"``
	- [X] Animation Root Motion Translate axes, animrootbonetranslate "animname" ``TRANSLATE_X/TRANSLATE_Y/TRANSLATE_Z``
	- [ ] Perhaps, an optional lua animation state machine script?

- [ ] The **VKPT** Scenario:
	- [ ] Port over/move in the debug drawing code of Q2RTX recent commits.
	- [ ] fill_model_instance, use a proper bbox check for BSP_WORLD_MODEL isntead of pointleaf.
			(See doors in target range map which bug out cluster testing, remaining unlit by interior lights.)

- [ ] The **MethLib** Scenario: (Pun intended)
	- [ ] Still got to redo the whole thing

- [ ] The **LUA** Scenario:
	- [/] (Somewhat optional, but useful really..) Add a stack debugger.
		- It is cheap but we got one, as does Lua itself if needed, so does sol.
	- [X] Implement custom TagMalloc like allocator for Lua memory management.
	- [X] Add proper file chunk loading.
		- [X] Add support for 'including/requiring' other Lua files in their own chunks.
	- [X] Add support for edicts on the C side of life to implement OnSignal functionalities.
	- [X] Add SignalOut funcionality to Lua, so it can fire signals.
	- [X] Add for PushMovers a function to acquire their current state.
	- [X] Add C utilities to define Lua consts for enums/constvals/defines.
	- [ ] Signalling:
		- [-] Patch all entities that might have a use for signalling. Such might be: A target_temp_entity etc. And of course, all PushMovers, killbox, etc. In hindsight, I will actually do these gradually over releases to prevent it from stalling the project overall.
		- [ ] PushMovers Signal I/O Support:
			- [X] func_button
			- [ ] func_breakable
				- [ ] Implement properly.
			- [ ] func_conveyor
			- [x] func_door
			- [x] func_door_rotating
			- [/] func_breakable
				- [ ] Just rewrite it, it is bugged anyhow.
			- [X] func_rotating
			- [ ] func_killbox
				- [ ] Just rework the whole thing since its 'vanilla' behavior is really limited.
			- [ ] func_object
				- [ ] Just rework the whole thing since its 'vanilla' behavior is really limited.
			- [ ] func_plat
				- [ ] Just rework the whole thing since its 'vanilla' behavior is really limited.
			- [X] func_rotating
			- [ ] func_train
				- [ ] Just rework the whole thing since its 'vanilla' behavior is really limited.
			- [X] func_wall
				- [X] Just rework the whole thing since its 'vanilla' behavior is really limited.
		- [ ] (Point-)Triggers Signal I/O Support:
			- [ ] trigger_always
			- [ ] trigger_counter
			- [ ] trigger_elevator
			- [ ] trigger_gravity
			- [ ] trigger_hurt
			- [ ] trigger_multiple
			- [ ] trigger_once
			- [ ] trigger_push
			- [ ] trigger_relay
		- [ ] Others Signal I/O Support, some need type specific signals, others need just the generic signals which also counts for ALL the entities above..
			- [ ] path_corner
			- [ ] spotlight
			- [ ] target_ entities.
	- [X] Add support for passing along values for Signals to be processed.

- [ ] - [ ] The **Monster** Scenario:
	- [ ] 0. We need nav nodes of sorts, probably lets do this KISS first, just use some entities.
	- [ ] 1. There's more I can think of such as detecting whether to strafe and all that...
	- [ ] 2. Add mm_move_t as a member of gedict_t, and/or of a different monster struct that becomes part of gedict_t
	- [ ] 3. Monster code should use Play/Get-Anim, and have actions linked to those consequently.
	- [ ] 4. I'll continue this list by the time I get there.

- [ ] The **EDITOR/VIEWER** Scenario:
	- [ ] 0. A file selector dialog for use with the editors below.
		     Use the demo browser as a reference material.
	- [X] 1. A refresh material editor.
	- [ ] 2. A collision material editor(actually, .wal_json since we use that.)
	- [ ] 3. A model viewer, allowing to speed up/down animations, as well as
			 blend combining them, so it becomes easier to figure out the framerate
			 you may want an animation to play at.
	- [/] 4. A refresh material editor.
		- [ ] Allow for saving materials properly, right now this is a mess.

---
## Technical Things prioritized, not catagorized however:
These are things to fix, or randomly implement(features, ideas), but definitely need to be dealt with before we can call it a day.

### God Priority Bug Fixing:
* [X] prefetch.txt requirements, if it is empty, the floor tile texture alignment is borked for each BSP map we load first. However, if prefetch.txt tries to 'prefetch some textures before hand, none of this is a problem.

### Highest Priority:
* [X] Get a test dummy model that we can use to replace the current player with.
* [X] Get a similar test dummy, however, this one needs to be kept in mind it will serve monster purposes instead.

### High Priority:
* [X] Rethink/reimplement the way how we approach skeletal model 'blending'. Local space it is?
	- Relative we do.
* [X] Implement Lua for game state logic and dynamics.
	- [X] Fix the script file loaded up leaking memory. FS_LoadFileEx(other tag)_
	- [X] Make buttons and doors lockable lol.

### Medium Priority:
* [X] Fix Save/Load games, the state for client(mostly weaponry) seems to not be (re-)stored properly, 
	* [ ] same for usetargethint that was being displayed.
* [X] Remove all Q2 monsters, keep a few around to use for testing.
* [-] Eliminate all other Q2-only specific game entities.

### Low Priority:
* [ ] Use our own C version of glmatrix.net and adjust(C++ify also), and streamline it as the math lib for use.
* [X] Fix up proper C++ enums issue with the whole int32_t by implementing appropriate operators... sigh..._
* [X] Get our own button sound files, also, for press(start) and unpress(end) states.
* [x] Add some way of having entity 'class' like type support. (At the least, eliminate a need for having such a large edict_t type that holds all sorts of object-type specific variables.)
	* [x] So far this is only in existence for the client game's ``clg_local_entity_t`` type.
	* [ ] Finish the implementation/style of it, make sure it is save game compatible, and implement it all-round.
* [ ] In common/messaging.cpp there are debug outputs for printing which bits are sent,
	  in some cases these need to have a game API callback also.
* [ ] For weapon items, move quantity etc from gitem_t into this struct.
* [ ] Have bullet impact display material ``kind`` specific puffs?
* [ ] Add an entity type that can have several ``hull`` varieties set to it, for testing purposes.
* [X] Implement something utilizing ET_BEAM.
* [ ] Fix game loop in client main for CLGame so it is 40hz but does not stall either.
* [ ] Name cvars for game modules respectively ``clg_`` and ``svg_``, also server cvars missing ``sv_`` and client cvars missing ``cl_``
* [X] Why, when func_plat hits ya, it moves you to random origin or such?_
	* Resolved by what seemed incorrect default behavior? Simply passing a knockback of 0 to the hit entity instead of 1.
* [X] Internal representation of skeletal poses and an API for use by both, client and server, thus residing in /common/
* [ ] SharedGame Weapon code, or Client Game sided immitations for predicting. This will make the game feel more responsive, by having possible client side player state events be ran instantly.
* [ ] Look into Q2RE q2pro for svc_sound additions.
* [ ] Implement a game mode structure that accepts function pointers, these are configured depending on the gamemode that is active within the game.
	* [ ] Rid all places where ``coop`` and ``deathmatch`` cvars are checked by actual individual gamemode related counterparts.
	* [ ] SVG_CanDamage and functions alike need to be moved into gamemode.

### Lowest, nearly redundant Priority:
- [ ] Use the actual FS system for read/write context in svgame.
- [ ] And an entity you can 'pick up and move around' for the lulz.
* [ ] Move capability of determining new pvs packet entities to clgame.
* [ ] Add an enter/exit frame/pvs so that we can allocate and deallocate skeletal bone
	  pose caches. Otherwise in hour long games they'd possibly stack up the memory buffer.
* [/] model_iqm.c, needs relocating preferably, also use QM_ maths.
	[x] * Skeletal Model stuff is now in /common/skeletalmodels/
	[ ] * QM_ Maths?_
* [ ] Modify the code so it does not generate ``hulls`` on the fly, but instead set them at ``LinkEntity`` time. 
	* [ ] Generate and store entity ``hulls`` in their server-side counterpart for later ``collision model`` rework.
	* [ ] Then for tracing, retreives them from their **client/server** counterpart. In case of the **client** they will be regenerated in case of a ``new`` entity, or general ``bounds changes`` has occured for the **client** entity.
* [ ] Integrate support for now still commented ATTN_ types.
* [ ] Look into allowing each entity classtype/group-type to have its own baseline states as well as matching custom implementations of Read/Write-EntityDeltaState.
* [ ] Look into JoltPhysics and see if it's something realistic for Jolt Physics.
	* [ ] If that fails, look into some library to deal with at least 'tracing' through geometric shapes properly.

---
## Resources:
### Audio:
* [X] Replace pain25 up to pain100 audio files. (And/or rework its code a bit.)
* [X] Replace explosion sounds.
* [ ] Replace ricochet sounds.
* [ ] Look for unique func_rotating audio, right now it uses func_plat audio instead.
* [X] Replace water in/out/head-under sounds.
* [X] Replace 'heat' in lava sounds.
* [X] Replace UI(menu) sounds.

### Models:
* [ ] Replace..??
	* [ ] Health Pack.
	* [ ] Gib Models.
	* [ ] Debris Models.
	* [ ] misc_explobox model_
* [X] Get ourselves a basic version of the Mixamo testdummy that works in-game.
* [ ] Get ourselves some environment props to use for ``client_misc_model`` decorating purposes.

### Textures:
* [/] Find some consistent themed PBR texture set?
	- [ ] Want more :-)
	- [ ] 

## Wishlist:
Content Related:
- [ ] Figure out how to create textures like those in the makkon 'pbr' sample map(it lacked actual roughness maps though).
- [/] Replace the audio with what I seemingly found, Q1 audio replacements.
	- [X] Scratch that, we're replacing all shit with custom sounds. Nice.
- [X] Get some custom weapon models so we can implement some new gameplay into baseq2rtxp.

And here a list of things that I keep an eye out and/or may (fail multiple times-) try and implement sooner or later:
- [ ] Implement libRmlUI properly and replace the HUDs, and all menu's with it. This allows for each game to easily adjust their UI to be distinct of its own.
- [X] Implement proper API and support in general for the IQM Skeletal Model format. We want to take control over those bones and blended animations.
- [ ] Implement various different types of 'Shape' hulls as well as trace functions. (i.e. ``SphereHull, CapsuleHull, CylinderHull``, and ``TraceBox, TraceSphere, etc.``)
	- [ ] Otherwise, research and implement Jolt Physics to take care of dealing with more realistic physics.
- [ ] Research and implement Vulkan support for explosion/hit decals.
- [ ] Research and implement Vulkan support for debug shapes rendering.
- [ ] Research and implement Vulkan support for textured particle effects.

---
## General Ideas:
- [ ] Weapon Recoil Scale Factors:
	- 1. Each stance has its min/max spread scale variables.
	- 2. Motion(velocity thus), scaled the spread.
	- 3. Crouching, does not scale with velocity, and by default lowers the spread scale largely.
	- 4. Optional: Angular spread, the faster your mouse is, the spread rises.
	- 5. Gun Weight -> Influence player movement speed scale.

- [X] Rename ``svgame/g_*`` to ``svgame/svg_*`` and put ``p_*`` files inside ``svgame/player``, ``m_`` files inside ``svgame/monsters/``
	  move weapons into player folder as a subfolder.

- [X] Remove the whole player gender thing. Wtf...

- [ ] The NetCode Mysteries:
	- [ ] We want bitstreaming to save up massive amounts of packet space.
		https://github.com/mas-bandwidth/serialize ??
	- [X] Add in proper ``Entity Type`` code for ``Packet Entities``
	- [ ] In combination with all of the below, and having a scenario for specific
		  entity types.. It makes sense to have a callback into the game dll so it can
		  determine how to deal with certain fields.