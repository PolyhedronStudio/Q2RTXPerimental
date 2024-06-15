# Quake II RTXPerimental Todo Lists:
These are mainly my personal notes/ideas/interests, and do not per se reflect the actual changes to be made.

## Notes:
* Is the hard coded speed value limit of 400.0f in CLG_ClampSpeed matching that to pmove's maxspeed? No I don't think so.
* Is the actual bobCycle predicting working or necessary? We can do this in PMove right?
* If we had event entities and then 'morphentity' function, for example: a blaster bullet could convert to an entity, eliminating
the need for temp_entity_t behavior. For hit trace based weapons I suppose the hits could be done client side but that'd require
simulating a frame ahead for all things. Either way, weapon could looks like it might be better off to go to shared some day.
* It seems with cl_async 0/1 it sometimes 'hitches' a bit, likely because of a mistake in implementing client game loop.

## Features:
Features being looked forward on implementation.
### Highest Priority:
* [ ] Get a test dummy model that we can use to replace the current player with.
* [X] Get a similar test dummy, however, this one needs to be kept in mind it will serve monster purposes instead.
### High Priority:
* [X] Add in ``cm_material_t`` accompanied by an actual json based client and server sided material properties file. Allowing for setting material contents ``kind``, acceleration/friction influences, footsteps, etc.
	* [X] Acquire some proper footstep audio files and add in material specific footstep audio.
	* [ ] Have bullet impact display material ``kind`` specific puffs?
* [ ] Move player animation code/task into the player move code for proper synchronisity and consistency reasons.
* [X] Footsteps/ViewBob to Client Game, PMove?
### Medium Priority:
* [X] Remove all Q2 monsters, keep a few around to use for testing.
	* Monsters left are: ``monster_actor``, ``monster_infantry``, ``monster_player``, ``monster_solider``.
* [ ] Eliminate all other Q2-only specific game entities.
* [ ] Add an entity that uses the humanoid ``test dummy`` model from **Mixamo** and **fully** operates at ``40hz``.
* [ ] Add an entity type that can have several ``hull`` varieties set to it, for testing purposes.
* [x] Add some way of having entity 'class' like type support. (At the least, eliminate a need for having such a large edict_t type that holds all sorts of object-type specific variables.)
	* [x] So far this is only in existence for the client game's ``clg_local_entity_t`` type.
	* [ ] Finish the implementation/style of it, make sure it is save game compatible, and implement it all-round.
* [ ] Add support for a proper "+/-use" command such as seen in **Half-Life**.

### Low Priority:
* [ ] Name cvars for game modules respectively ``clg_`` and ``svg_``, also server cvars missing ``sv_`` and client cvars missing ``cl_``
* [X] Why, when func_plat hits ya, it moves you to random origin or such?_
	* Resolved by what seemed incorrect default behavior? Simply passing a knockback of 0 to the hit entity instead of 1.
* [ ] Internal representation of skeletal poses and an API for use by both, client and server, thus residing in /common/
* [ ] Look into Q2RE q2pro for svc_sound additions.
* [ ] Implement a game mode structure that accepts function pointers, these are configured depending on the gamemode that is active within the game.
	* [ ] Rid all places where ``coop`` and ``deathmatch`` cvars are checked by actual individual gamemode related counterparts.
	* [ ] CanDamage and functions alike need to be moved into gamemode.

### Lowest, nearly redundant Priority:
* [ ] model_iqm.c, needs relocating preferably, also use QM_ maths.
* [ ] Modify the code so it does not generate ``hulls`` on the fly, but instead set them at ``LinkEntity`` time. 
	* [ ] Generate and store entity ``hulls`` in their server-side counterpart for later ``collision model`` rework.
	* [ ] Then for tracing, retreives them from their **client/server** counterpart. In case of the **client** they will be regenerated in case of a ``new`` entity, or general ``bounds changes`` has occured for the **client** entity.
* [ ] Calculate the proper entity matrixes/quaternions during Link time.
* [ ] Integrate support for now still commented ATTN_ types.
* [ ] * [ ] Look into allowing each entity classtype/group-type to have its own baseline states as well as matching custom implementations of Read/Write-EntityDeltaState.
* [ ] Look into JoltPhysics and see if it's something realistic for Jolt Physics.
	* [ ] If that fails, look into some library to deal with at least 'tracing' through geometric shapes properly.

## Resources:
### Audio:
* [X] Replace pain25 up to pain100 audio files. (And/or rework its code a bit.)
* [X] Replace explosion sounds.
* [ ] Replace ricochet sounds.
* [X] Replace water in/out/head-under sounds.
* [X] Replace 'heat' in lava sounds.
* [X] Replace UI(menu) sounds.
### Models:
* [ ] Replace..??
* [X] Get ourselves a basic version of the Mixamo testdummy that works in-game.
* [ ] Get ourselves some environment props to use for ``client_misc_model`` decorating purposes.
### Textures:
* [ ] Find some consistent themed PBR texture set?

## Bugfixes: 
Ideally this list would never exist, but in this world we can't have it all so, let me introduce you to a highly most pleasant list of bugs to fix!
### Highest:
* [x] None
### High Priority:
* [X] Ladders from within water, simply isn't working. Likely related to the recent ground entity changes.
### Medium Priority:
* [ ] Find the bug that is currently making the OctagonHull not enjoy colliding to certain specific bounding boxes.
### Low Priority:
* [x] None
### Lowest, nearly redundant Priority:
* [ ] Remove the if statement for cl_batchcmds in the (client/input.c)``ready_to_send``_to re-enable the bug for batched commands movement. The bug is that pushers have a steady pattern of 'spiking', moving neatly 5 units a frame as expected up to suddenly the double.

## Wishlist:
Content Related:
- [ ] Figure out how to create textures like those in the makkon 'pbr' sample map(it lacked actual roughness maps though).
- [ ] Replace the audio with what I seemingly found, Q1 audio replacements.
- [X] Get some custom weapon models so we can implement some new gameplay into baseq2rtxp.

And here a list of things that I keep an eye out and/or may (fail multiple times-) try and implement sooner or later:
- [ ] Implement libRmlUI properly and replace the HUDs, and all menu's with it. This allows for each game to easily adjust their UI to be distinct of its own.
- [ ] Implement proper API and support in general for the IQM Skeletal Model format. We want to take control over those bones and blended animations.
- [ ] Implement various different types of 'Shape' hulls as well as trace functions. (i.e. ``SphereHull, CapsuleHull, CylinderHull``, and ``TraceBox, TraceSphere, etc.``)
	- [ ] Otherwise, research and implement Jolt Physics to take care of dealing with more realistic physics.
- [ ] Research and implement Vulkan support for explosion/hit decals.
- [ ] Research and implement Vulkan support for debug shapes rendering.
- [ ] Research and implement Vulkan support for textured particle effects.

## General Ideas:
- [ ] Weapon Recoil Scale Factors:
	- 1. Each stance has its min/max spread scale variables.
	- 2. Motion(velocity thus), scaled the spread.
	- 3. Crouching, does not scale with velocity, and by default lowers the spread scale largely.
	- 4. Optional: Angular spread, the faster your mouse is, the spread rises.
	- 5. Gun Weight -> Influence player movement speed scale.