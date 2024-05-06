# Quake II RTXPerimental Todo Lists:
These are mainly my personal notes/ideas/interests, and do not per se reflect the actual changes to be made.

## Notes:
* Is the hard coded speed value limit of 400.0f in CLG_ClampSpeed matching that to pmove's maxspeed? No I don't think so.
* Is the actual bobCycle predicting working or necessary? We can do this in PMove right?
* If we have event entities and 'morphentity', a blaster bullet could convert to an entity, eliminating
the need for temp_entity_t behavior. For hit trace based weapons I suppose the hits could be done client side but that'd require
simulating a frame ahead for all things. Either way, weapon could looks like it might be better off to go to shared some day.


## Features:
Features being looked forward on implementation.
### Highest Priority:
* [ ] ...
### High Priority:
* [X] Add in ``cm_material_t`` accompanied by an actual json based client and server sided material properties file. Allowing for setting material contents ``kind``, acceleration/friction influences, footsteps, etc.
	* [X] Acquire some proper footstep audio files and add in material specific footstep audio.
	* [ ] Have bullet impact display material ``kind`` specific puffs?
* [ ] Move player animation code/task into the player move code for proper synchronisity and consistency reasons.
* [ ] Footsteps/ViewBob to Client Game, PMove?
### Medium Priority:
* [ ] Add an entity that uses the humanoid ``test dummy`` model from **Mixamo** and **fully** operates at ``40hz``.
* [ ] Add an entity type that can have several ``hull`` varieties set to it, for testing purposes.
* [x] Add some way of having entity 'class' like type support. (At the least, eliminate a need for having such a large edict_t type that holds all sorts of object-type specific variables.)
	* [x] So far this is only in existence for the client game's ``clg_local_entity_t`` type.
	* [ ] Finish the implementation/style of it, make sure it is save game compatible, and implement it all-round.
### Low Priority:
* [ ] Rid all places where ``coop`` and ``deathmatch`` cvars are checked by actual individual gamemode related counterparts.
* [ ] Calculate the proper entity matrixes/quaternions during Link time.
* [ ] Internal representation of skeletal poses and an API for use by both, client and server, thus residing in /common/
* [ ] Modify the code so it does not generate ``hulls`` on the fly, but instead set them at ``LinkEntity`` time. 
	* [ ] Generate and store entity ``hulls`` in their server-side counterpart for later ``collision model`` rework.
	* [ ] Then for tracing, retreives them from their **client/server** counterpart. In case of the **client** they will be regenerated in case of a ``new`` entity, or general ``bounds changes`` has occured for the **client** entity.

### Lowest, nearly redundant Priority:
* [ ] Add support for a proper "+/-use" command such as seen in **Half-Life**.

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
- [ ] Get some custom weapon models so we can implement some new gameplay into baseq2rtxp.

And here a list of things that I keep an eye out and/or may (fail multiple times-) try and implement sooner or later:
- [ ] Implement libRmlUI properly and replace the HUDs, and all menu's with it. This allows for each game to easily adjust their UI to be distinct of its own.
- [ ] Implement proper API and support in general for the IQM Skeletal Model format. We want to take control over those bones and blended animations.
- [ ] Implement various different types of 'Shape' hulls as well as trace functions. (i.e. ``SphereHull, CapsuleHull, CylinderHull``, and ``TraceBox, TraceSphere, etc.``)
	- [ ] Otherwise, research and implement Jolt Physics to take care of dealing with more realistic physics.
- [ ] Research and implement Vulkan support for explosion/hit decals.
- [ ] Research and implement Vulkan support for debug shapes rendering.
- [ ] Research and implement Vulkan support for textured particle effects.
