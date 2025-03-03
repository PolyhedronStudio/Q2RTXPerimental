# Quake II RTXPerimental Todo Lists:
These are mainly my personal notes/ideas/interests, and do not per se reflect the actual changes to be made.

## Notes:
* Make sure that client and server both deduce frameTime based on animation framerates instead.
---
* Is the actual bobCycle predicting working and necessary? We can do this in PMove right?
---

## Random Todo for The Day:
* If we had event entities and then 'morphentity' function, for example: a blaster bullet could convert to an entity, eliminating
the need for temp_entity_t behavior. For hit trace based weapons I suppose the hits could be done client side but that'd require
simulating a frame ahead for all things. Either way, weapon could looks like it might be better off to go to shared some day.
---
* [X] It seems with cl_async 0/1 it sometimes 'hitches' a bit, likely because of a mistake in implementing client game loop.
	- It is a mistake in client game loop.
	- [X] This actually seems fixed now, yay.
---
* [ ] Animated Brush Textures: Any brush entity with an animated texture needs to be able to configure its
animations for open/closed/transit-in/transit-out states throughout TB editor. (``func_button,func_door etc``)
---
* [ ] Add trigger_state_t and trigger_type_t to store the entity trigger state, entity_usetarget_type_t, the latter holds the usetarget type for +usetarget interaction.
* [ ] Add in the FGD, the basis for triggertype and triggervalue.
* [ ] This'll allow us to determine how an entity should proceed to trigger any other entity.
---
* [ ] Unify signal names, i.e DoorLock RotatingLock just use Lock, etc etc.
* [ ] 

## For v0.0.6 or so:
- [ ] The **Monster** Scenario:
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

- [ ] **HUD**:
	- [ ] Certain people get iffy and uppety about crosshair as it is. So, let's make it configurable.

- [ ] **Entities**:
	- [ ] Reimplement (client-)misc_model properly.
	- [ ] Add proper spawn flag constants.
	- [ ] Can we do a, C++ struct inheritance and have edict_t* store a pointer to an instance of the matching entity classname and its 'classdata' struct.
	- [ ] .. Some more I bet ..

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

---
## For v0.0.5(Being idealistic here, not realistic, that is when it comes to time lol):
- [ ] Core/Generic/Code-Style/Important(DoNotForget):
	- [X] Fix Save/Load games, the state for client(mostly weaponry) seems to not be (re-)stored properly.
		- [ ] Add Save/Load for the (optional) Lua's in-game mapState table data.
		- [ ] Add in support for signal_argument_t array.
		- [X] Fix pushMoveInfo.curve.positions array, it is dynamic, oof..
			- [X] Fixed by implementing sg_qtag_memory_t however, 
				- [ ] it needs support for multiple types?
- [ ] The **VKPT** Scenario:
	- [X] Target Range -> Animated Textures which lol, do not animate, we merely use them right now
			for visual trickstery. Such as a light switching colors. However, this fails, it ends up
		    doing a silly light animation either way.
	- [ ] fill_model_instance, use a proper bbox check for BSP_WORLD_MODEL isntead of pointleaf.
			(See doors in target range map which bug out cluster testing, remaining unlit by interior lights.)

- [ ] The **MethLib** Scenario: (Pun intended)
	- [ ] Still got to redo the whole thing

- [ ] The **Entities** Scenario:
	- [X] 0. Fix func_button, KISS for now.
		- [X] Test func_button map properly and add a few extra signal related features.
	- [ ] 1. Calculate the proper entity matrixes/quaternions during Link time.
		- [ ] Reimplement the 'movewith' system using matrixes/quaternions instead of those silly vector maths.
- [X] The **IQM Animation** Scenario:
	- [ ] 0. Redo the player animations properly once and for allOnce again.. sigh

- [ ] The **Skeletal Model Info** Scenario:

	- [X] 1. The shared game code needs and is responsible for a ``Pose API`` which essentially is capable of:
			- Taking an animation from Pose A, and blend it into Pose B starting from a specified bone.
			- This'll use a simple memory cache that grows in POW2 size whenever there isn't enough space to be used.
		      (The more entities there are using a skeletal model, the more space we need.)
	- [ ] 2. I am most likely forgetting some things, so first finish this entire list.

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

---
## Technical Things prioritized, not catagorized however:
These are things to fix, or randomly implement(features, ideas), but definitely need to be dealt with before we can call it a day.


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
* [ ] Fix Save/Load games, the state for client(mostly weaponry) seems to not be (re-)stored properly, 
	* [ ] same for usetargethint that was being displayed.
* [ ] * [X] Remove all Q2 monsters, keep a few around to use for testing.
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
## Bugfixes: 
Ideally this list would never exist, but in this world we can't have it all so, let me introduce you to a highly most pleasant list of bugs to fix!
### Highest:
* [x] None
### High Priority:
* [x] None
### Medium Priority:
* [ ] Getting stuck by a pusher brush entity has us 'wrap/teleport' far off.
* [ ] Find the bug that is currently making the OctagonHull not enjoy colliding to certain specific bounding boxes.
* [X] It seems for thirdperson camera, func_wall hitting traces get the camera inside the mesh..?
	- [X] Filter so it doesn't clip to all entities.
### Low Priority:
*	[ ] // TODO: Fix the whole max shenanigan in shared.h,  because this is wrong... #undef max in svg_local.h etc_
* [x] None
### Lowest, nearly redundant Priority:
* [ ] Remove the if statement for cl_batchcmds in the (client/input.c)``ready_to_send``_to re-enable the bug for batched commands movement. The bug is that pushers have a steady pattern of 'spiking', moving neatly 5 units a frame as expected up to suddenly the double.

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
* [ ] Replace explosions, use and customize Kenney Smoke Particles or something.
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