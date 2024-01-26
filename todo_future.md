# Quake II RTXPerimental Contenting Todo Features:
## Features most certain to be on a future Todo List:
* **Engine:**
	* [ ] Take a closer look at the ``system`` codes and unify Windows/Linux, keep things **SDL2** and **OpenAL** only.
* **Client Game:**
	* [ ] Custom ``user input`` code/commands.
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