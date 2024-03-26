# Quake II RTXPerimental Contending Todo Features:
## Features most certain to be on a future Todo List:
* **Engine:**
	* [ ] Take a closer look at the ``system`` codes and unify Windows/Linux, keep things **SDL2** and **OpenAL** only.
* **Client Game:**
	* [X] Custom ``user input`` code/commands.
	* [ ] Code ``client-side`` specific effects.
	* [ ] Proper control over **HUD**.
* **Shared Game:**
	* [X] PMove being extracted to Shared Game allows for customization of player movement.

## Low priority must do:
* [ ] Add support for stair step smoothing during demo playback.

## Possible contending ideas:
* [ ] Rework CMake to how it was in Polyhedron.
* [ ] Make entity dictionaries 'stand on their own', acting as the ``BSP Entity Definition``.
* [ ] Allow for entity class type specific read/write entity ``net code``?
* [ ] Collision code rewrite. (Use proper **matrix/quaternions**, and allow for different ``hull`` types such as ``Spheres``, ``Cylinders`` and ``Capsules``.)
	* This requires several editing/rewriting parts of the game **Physics** as well.
* [ ] Proper control over ``IQM's`` Skeletal Animation capabilities.
* [ ] Add in [RmlUI](https://github.com/mikke89/RmlUi) and replace the HUD and menus with it. (And possibly, allow in-game menus as well.)
* [ ] For the game modules, possibly deploy and rewrite in and embedded type of ``script`` environment. Think [Mono](https://github.com/mono/mono) or [Wren](https://github.com/wren-lang/wren)
* [ ] https://winter.dev/articles

# My Personal Notes/Wishes:
Think that to set forward for whichever project I might see fit with this, the main features I really badly want to have are:

* Improved entity system, which at least does not use one single edict_t struct to store all and any type of entity fields. Even a simple entity_class_type_t entClassType; entity_class_type *ptr; with casting if needed would do. As long as it can still properly save/load callbacks that are set. (Those function pointers.) This proved to be a real bitch to even try when C++ OO is in play. So I'll settle with less this time around.
* RMLUI for 2D.
* At the very least, decent humanoid skeletal model animation support. I think I did actually nail this right with Polyhedron.
* A rewrite of the collision code(for additional other hull shape types, and to just get rotating objects act properly) and/or actual physics.
* Somewhat related to number 4 and that includes actual proper "materials", which currently do not exist. Q2RTX only has .mat files that are refresh VKPT code related.
* That cool audio shit, somewhat related to number 5, like HL1 has. When inside of a vent, audio goes all vent like. This is doable since OpenAL is already implemented.

So I suppose I'm trying to sort of round up what I got right now as a release 0.0.2