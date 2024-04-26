# Quake II RTXPerimental Changelogs:

## Q2RTXPerimental Changes(New, listed per version):
Changes will from now on be listed as additions per version. 
## (v0.0.4):
* The ``collision model`` code now has ``material awareness``, for now still separated from the refresh material system and operates by loading properties out the ``*.wal_json`` texture name files. These are also supported by recent ericw-tools branches to substitute non ``.wal`` textures with. Currently two extra fields can be added, this allows you to set a material its ``kind``(string) as well as its ``friction``(float).
* In accordance to the above, each ``mtexinfo_t`` now has its own unique ID, as well as its ``csurface_t`` member pointing to the corresponding material pointer as well as having the proper ``materialID`` set for it.
* Player Move code now has an optional ``PMOVE_USE_MATERIAL_FRICTION`` define which when defined has the Player Move code respond to the ground material's ``physical_material_friction``.
* Restructured and cleaned up the Player Move prediction code as well as corresponding view calculation code.
* Added ``EAX`` support for OpenAL and a ``client_env_sound`` entity to accomodate with it. This allows for reverb effect zones, where the ``client_env_sound`` entity performs a clipping trace to the (predicted-)player origin, where if successful and within radius the specific reverb environment is set. 
* Added ``Footsteps`` support on a per ``material`` basis. It operates based on the ``physical_material_kind`` property.
* Added ``Ladder Footsteps`` which defaults to the ``metal`` material ``kind`` for selecting the footstep sounds from.
* Added what is a temporary fix for ericw-tools-2.0.0-alpha7 bug that does not properly fill leaf node contents with all brush contents. The fix is thus applying them in-engine.

## (v0.0.3):
* Fixed problems with stair step smoothing interfering with pushers.
* Fixed pushers not acting properly, causing jitters/lag.
* Added a 'Developer Stuff' menu which currently allows you to select maps that showcase specific (new/updated-)features.

## Q2RTXPerimental Changes(Before listing them per version):
These changes spawn from v0.0.0 up to 0.0.2
### General:
* **CPP-ify the codebase, meaning that it now compiles using a CPP compiler** 
	* The only exception being the **VKPT** code, to make life easy merging in any new **Q2RTX VKPT** features.
* Kept the good old ``baseq2`` game code as intact as possible and will try to maintain it somehwat.
* Added a copy of ``baseq2`` folder, renamed to ``base2rtxp`` serving as the experimental game project of this engine.
* Customizable Tick Rate (``40hz`` is the current default).
* Removed MVD, and GTV, although 'luxury' features, make things harder to maintain and I doubt it is within most target audience interest rates.
* Removed Anti-Cheat, OS specific and h4x0rz are going to h4x anyway. Makes life easier to maintain this project.
* Increased ``MAX_EDICTS(8192)``, ``MAX_MODELS(8192)``, ``MAX_SOUNDS(2048)``, ``MAX_IMAGES(512)``.
* Increased maximum ConfigString length ``(96)``.
* Enabled ``USE_SMOOTH_DELTA_ANGLES`` by default.
* Adjusted stair **Step Smoothing** to ``BASE_FRAMETIME // (25ms for 40hz)`` instead of its old hard values: ``100ms at 10hz``.
* Entity origins and player origin are ``wired/transferred`` as full floating point precision values.
* Client-Side awareness of the gamemode that is being played.
* Brush Shaped Triggers: Triggers can now have a "Clipped" flag set, in which case it only triggers when an actual collision(clip to brush) has occured.
* Comes with basic vector, matrix and quaternion math utilities derived from raylib1.5, and adds in C++ utilities such as operator support.
* Modern player movement derived from Q2RE.
* BoundingBox entities can have their hull contents be set to any CONTENTS_ values.
* The ``entityString`` is now parsed into a list of key/value pairs, the so called ``Collision Model Entities``.
### Client:
* The client now also makes use of the ``cm_t``(Collision Model) struct for its ``BSP`` and ``inline brush models``.
* Does proper lerping for entities that run lower than ``40hz``.
* Does proper lerping for weapons based on their 'gunrate', defaults to ``10hz``
* Added RF_OLD_FRAME_LERP support: ``[Paril-KEX] force model to lerp from oldframe in entity state; otherwise it uses last frame client received``
* Added support for monster 'step' Z-Axis lerping when RenderFlag ``RF_STAIR_STEP`` is set.
* Added support to lerp from old entity state frame when RenderFlag ``RF_OLD_FRAME_LERP`` is set.
* ``EF_SPOTLIGHT`` support.
### Net Code:
* Uses its own protocol, partially based on Q2 Protocol #34 and Q2PRO its own. This supports proper fragmenting, allowing for far more in-vis entities to be transfered over ``The Wire``. This does come with the drawback of needing a proper ``25ms`` ping in order to have a smooth gameplay flow.
* Changed Solids from ``int32_t`` to ``uint32_t``, so that ``SOLID_BOUNDS_BOX`` can now have **BoundingBox** sizes up to those of **Q2RE/Q3**.
* Network origins using full floating point precision, angles are half-floats.
* Use ReadBase128 message read/write functions where applicable.
### Refresh(VKPT):
* BSP maps compiled with ``texinfos`` such as: ``textures/test/01.tga`` now will load with their proper dimensions, meaning one does not need low-res .wal textures to substitute for any of these.
* RF_NOSHADOW is now respected for both, regular and brush entities.
* Added "UNLIT" material type. Example of use would be to use it for where a ``spotlight`` entity emits from.

## Q2RTXPerimental Game Changes:
### Server Game:
* Replaced (most)_framenum based functionality with time again, borrowing ``sg_time_t`` type from **Q2RE**.
* AI now has the option to run at ``tick rate``(defaults to ``40hz``), when the ``AI_HIGH_TICK_RATE`` flag is set.
* Guns can operate at a varying tick rate, default is ``10hz``
* Brush Triggers can now be set a spawnflag to trigger only when actually clipping with the trigger brush.
* Basic configurable Spotlight entity that can be given its own ``customLightStyle`` property as well.
* Now spawns entities based on the key/value pairs retreived from the ``Collision Model``.
### Client Game:
* Responsible for preparing the view by adding all the packet entities as well as the player viewweapon/thirdperson model.
* Control over the user input, including the capability of adding new custom User Input ``KeyButton`` registration and handling thereof.
* Player Movement Prediction.
* Parse, precache, or modify the clientinfo data.
* Control over the layout string system.
* Control over the implementations of ``Temp Entity Events``, ``Particles``, ``Explosions``, ``Beams/Lasers/Trails``, ``Sustains``.
* A ``local entity`` system that has a class struct like implementation. Its main intended use is for client only specific entities, think of decorating or certain particle emitters that you do not want to clutter the network with.
* View/Scene handling, this includes: ``Packet Entities``, ``Local Entities``, ``Temp Entity Events``, ``Particles``, ``Explosions``, ``Beams/Lasers/Trails``, ``Sustains``.
### Shared Game:
* Contains the shared used ``sg_time_t`` which replaces all the vanilla frametime/framenum work.
* Customizable PlayerMove code.

## 1.7.0

**Fixed Issues:**
  * Fixed a crash in the game logic when a monster interacts with a door in notarget mode (https://github.com/NVIDIA/Q2RTX/issues/92)
  * Fixed a crash when the map file doesn't have a VIS hunk (https://github.com/NVIDIA/Q2RTX/pull/223)
  * Fixed some Vulkan validation layer issues (https://github.com/NVIDIA/Q2RTX/pull/229, https://github.com/NVIDIA/Q2RTX/pull/246, more)
  * Fixed texture alignment issues on some doors (https://github.com/NVIDIA/Q2RTX/issues/211)
  * Fixed the flare gun still using ammo with `dmflags` including 8192 (`DF_INFINITE_AMMO`) (https://github.com/NVIDIA/Q2RTX/issues/)191)
  * Fixed Vulkan queue initialization on platforms that don't support split queues (https://github.com/NVIDIA/Q2RTX/pull/248)
  * Switched the OpenAL dependency with a statically linked library (https://github.com/NVIDIA/Q2RTX/pull/224)

**Misc Improvements:**
  * Added support for building on PowerPC 64 LE CPU architecture (https://github.com/NVIDIA/Q2RTX/pull/260)
  * Adjusted the automatic UI scaling to avoid making the UI too big
  * Improved precision of target frame rate adjustment (https://github.com/NVIDIA/Q2RTX/pull/242)
  * Tuned the full-screen blend effects to be less intensive
  * Updated the Loading plaque texture (https://github.com/NVIDIA/Q2RTX/issues/265)

**Contributions by GitHub user @res2k**:

  * Added a warning when screen-space image memory usage is very high (https://github.com/NVIDIA/Q2RTX/pull/179)
  * Added control over fallback radiance of emissive materials (https://github.com/NVIDIA/Q2RTX/pull/210)
  * Added menu controls for the full screen blend effects (https://github.com/NVIDIA/Q2RTX/pull/216)
  * Added support for spotlights with an emission profile and a player flashlight (https://github.com/NVIDIA/Q2RTX/pull/203, https://github.com/NVIDIA/Q2RTX/pull/214)
  * Fixed a crash when some textures are missing (https://github.com/NVIDIA/Q2RTX/pull/263)
  * Fixed an overflow condition when `pt_bsp_sky_lights` is more than 1 (https://github.com/NVIDIA/Q2RTX/pull/262)
  * Fixed animated textures on BSP models (https://github.com/NVIDIA/Q2RTX/pull/187)
  * Fixed crashes when renderer initialization fails (https://github.com/NVIDIA/Q2RTX/pull/199)
  * Fixed FSR image scaling in some cases (https://github.com/NVIDIA/Q2RTX/pull/232)
  * Fixed incorrect scaling of textures without a custom material definition (https://github.com/NVIDIA/Q2RTX/pull/235)
  * Fixed mode setting on Linux in GitGub CI builds (https://github.com/NVIDIA/Q2RTX/pull/268)
  * Fixed save game compatibility with Q2RTX 1.5.0 (https://github.com/NVIDIA/Q2RTX/pull/193)
  * Fixed some issues with lighting in custom maps (https://github.com/NVIDIA/Q2RTX/pull/189)
  * Fixed texture data size computation for R16_UNORM textures (https://github.com/NVIDIA/Q2RTX/pull/236)
  * Fixed the HDR screenshot feature (https://github.com/NVIDIA/Q2RTX/pull/190)
  * Fixed the look of smoke effects (https://github.com/NVIDIA/Q2RTX/pull/195)
  * Fixed the range of animated light intensities (https://github.com/NVIDIA/Q2RTX/pull/200)
  * Fixed the replacement textures when multiple materials are used with the same base texture (https://github.com/NVIDIA/Q2RTX/pull/222)
  * Improved light list handling to fix excessive flicker and noise (https://github.com/NVIDIA/Q2RTX/pull/234)
  * Improved material system robustness for games with custom textures (https://github.com/NVIDIA/Q2RTX/pull/201)
  * Improved polygonal light sampling to reduce noise and darkening (https://github.com/NVIDIA/Q2RTX/pull/266)
  * Improved Wayland support (https://github.com/NVIDIA/Q2RTX/pull/261, https://github.com/NVIDIA/Q2RTX/pull/221)
  * Integrated several fixes from Q2PRO (https://github.com/NVIDIA/Q2RTX/pull/196)
  * Replaced the single `sky_clusters.txt` file with per-map files (https://github.com/NVIDIA/Q2RTX/pull/219)
  * Tweaked particles to have more nuanced colors (https://github.com/NVIDIA/Q2RTX/pull/197)
  * Updated SDL2 to 2.26.1 (https://github.com/NVIDIA/Q2RTX/pull/252)

## 1.6.0

**Breaking Changes:**

  * Re-designed the material definition system for flexibility and modding.
  * Removed support for the `VK_NV_ray_tracing` Vulkan extension, which is superseded by `VK_KHR_ray_tracing_pipeline` and `VK_KHR_ray_query` that were added earlier.

**New Features:**

  * Added a setting to enable nearest filtering on world textures, `pt_nearest`.
  * Added a setting to enable the use of texture and model overrides in the GL renderer, `gl_use_hd_assets` (https://github.com/NVIDIA/Q2RTX/issues/151)
  * Added support for converting sky surfaces into lights based on their flags, see `pt_bsp_sky_lights`.
  * Added support for IQM models and skeletal animation for the RTX renderer.
  * Added support for making any models translucent, and `cl_gunalpha` specifically.
  * Added support for masked materials (https://github.com/NVIDIA/Q2RTX/issues/127)
  * Added support for polygonal light extraction from MD2/MD3/IQM models.
  * Added support for smooth normals on the world mesh through a BSPX extension.
  * Added support for unlit fog volumes. See the comment in `fog.c` for more information.
  * Enabled game builds for ARM64 processors.
  * Extended the "shader balls" feature to support arbitrary test models with animation.

**Fixed Issues:**

  * Fixed a crash that happened when loading a map with non-emissive lava material.
  * Fixed loading of multi-skin MD3 models.
  * Fixed long texture animation sequences.
  * Fixed some bugs in the model validation code (https://github.com/NVIDIA/Q2RTX/pull/149)
  * Fixed some self-shadowing artifacts by increasing the shadow and bounce ray offsets.
  * Fixed some unlit or partially lit triangles by improving the BSP cluster detection logic.
  * Fixed the `MZ_IONRIPPER` sound (https://github.com/NVIDIA/Q2RTX/pull/143)
  * Fixed the `rcon_password` variable flags to prevent the password from being stored (https://github.com/NVIDIA/Q2RTX/issues/176)
  * Fixed the background blur behavior when the menu is opened on a system with over 24 days of uptime.
  * Fixed the barriers in non-uniform control flow in the tone mapping shader (https://github.com/NVIDIA/Q2RTX/pull/129)
  * Fixed the buffer flags on the acceleration structure scratch buffer (https://github.com/NVIDIA/Q2RTX/pull/142)
  * Fixed the crash that sometimes happened when entering The Reactor map (https://github.com/NVIDIA/Q2RTX/issues/123)
  * Fixed the disappearing light surfaces on some polygons with almost-collinear edges.
  * Fixed the lighting on the first person weapon when it's left-handed.
  * Fixed the missing frame 0 in repeated entity texture animations.
  * Fixed the pipeline layout mismatch in `asvgf.c` (https://github.com/NVIDIA/Q2RTX/pull/140)
  * Fixed the rendering of the planet's atmosphere in the space environment.
  * Fixed the sampled lighting estimator math, improved specular MIS.

**Misc Improvements:**

  * Allowed changing the VSync setting without reloading the renderer.
  * Extended the supported light style range to 200% to fix over-bright lighting.
  * Implemented anisotropic texture sampling for objects seen in reflections and refractions using ray cones.
  * Improved CPU performance by not re-allocating the TLAS on every frame.
  * Improved the handling of transparent effects in the acceleration structures.
  * Removed the fake ambient that was added when global illumination is set to "off".
  * Removed the initialization of the async compute queue, which was unused. This improves rendering performance and fixes some compatibility issues with AMD drivers.
  * Removed the MAX_SWAPCHAIN_IMAGES limit for XWayland (https://github.com/NVIDIA/Q2RTX/pull/122)
  * Replaced the implementation of model data handling on the GPU to improve scalability (https://github.com/NVIDIA/Q2RTX/pull/171)
  * Replaced the material BRDF with a more physically correct one and removed the non-linear albedo correction function.
  * Replaced the normal map normalization on load with a compute shader to speed up engine startup and map loading.

**Contributions by GitHub user @res2k:**

  * Added auto-complete for the `ray_tracing_api` console variable (https://github.com/NVIDIA/Q2RTX/pull/112)
  * Added support for AMD FidelityFX Super Resolution (https://github.com/NVIDIA/Q2RTX/pull/145)
  * Added support for HDR monitors (https://github.com/NVIDIA/Q2RTX/pull/159)
  * Added support for synthesizing emissive textures and fixing lighting in custom maps (https://github.com/NVIDIA/Q2RTX/pull/111, https://github.com/NVIDIA/Q2RTX/pull/136)
  * Allowed saving and loading games in expansion packs (https://github.com/NVIDIA/Q2RTX/pull/157, https://github.com/NVIDIA/Q2RTX/issues/150)
  * Fixed a crash due to invalid clusters on some world geometry (https://github.com/NVIDIA/Q2RTX/pull/165, https://github.com/NVIDIA/Q2RTX/issues/163)
  * Fixed the debugging features of the bloom pass (https://github.com/NVIDIA/Q2RTX/pull/160)
  * Fixed the lighting from light surfaces with animated textures (https://github.com/NVIDIA/Q2RTX/pull/137)
  * Implemented full-screen blend effects (such as on item pickup) in the RTX renderer (https://github.com/NVIDIA/Q2RTX/pull/110)
  * Improved support for old mods and enabled x86 builds of the dedicated server (https://github.com/NVIDIA/Q2RTX/pull/116)
  * Improved the behavior of Dynamic Resolution Scaling on map changes (https://github.com/NVIDIA/Q2RTX/pull/155)
  * Improved the FPS counter behavior when `r_maxfps` is set (https://github.com/NVIDIA/Q2RTX/issues/117, https://github.com/NVIDIA/Q2RTX/pull/118)
  * Improved the tone mapper (https://github.com/NVIDIA/Q2RTX/pull/156)
  * Replaced the rendering of laser beams as billboards with volumetric primitives (https://github.com/NVIDIA/Q2RTX/pull/108)

**Contributions by GitHub user @Paril:**

  * Added settings for texture filtering in the UI (https://github.com/NVIDIA/Q2RTX/pull/173)
  * Added support for maps in QBSP format (https://github.com/NVIDIA/Q2RTX/pull/133, https://github.com/NVIDIA/Q2RTX/pull/154)
  * Merged over 350 commits from Q2PRO (https://github.com/NVIDIA/Q2RTX/pull/166)
  * Moved the security camera definitions to per-map files for modding (https://github.com/NVIDIA/Q2RTX/pull/169)



## 1.5.0

**New Features:**

  * Added support for ray tracing using the `VK_KHR_ray_query` extension API. _NOTE:_ This is an optional feature, and the two previously supported methods, `VK_NV_ray_tracing` and `VK_KHR_ray_tracing_pipeline`, are still supported.

**Fixed issues:**

  * Fixed the crash that happened on some systems when the game is minimized: https://github.com/NVIDIA/Q2RTX/issues/103
  * Fixed the invalid Vulkan API usage that happened in the bloom pass: https://github.com/NVIDIA/Q2RTX/issues/104
  * Fixed the invalid barrier for an inter-queue resource transition: https://github.com/NVIDIA/Q2RTX/issues/105 
  * Fixed the out-of-bounds addressing of the framebuffer array: https://github.com/NVIDIA/Q2RTX/issues/107

**Misc Improvements:**

  * Reduced the delay after resolution changes by avoiding re-initialization of the RT pipelines.
  * Changed the memory type required for the UBO and transparency upload buffers to `(HOST_VISIBLE | HOST_COHERENT)`.
  * Improved logging around SLI initialization.

## 1.4.1

**Fixed issues:**

  * Fixed a crash on launch when there is no "newgame" command, for example, when someone overrides the default.cfg file.
  * Fixed crashes or corruptions on AMD GPUs by increasing the size of the AS build scratch buffer and using correct scratch buffer alignment: https://github.com/NVIDIA/Q2RTX/issues/99
  * Fixed some potential memory leaks as noted in https://github.com/NVIDIA/Q2RTX/pull/84
  * Fixed the bloom output jittering when DRS is used.
  * Fixed the game not launching on pre-r460 NVIDIA GPU drivers: https://github.com/NVIDIA/Q2RTX/issues/100
  * Fixed the non-TAAU upscaling when DRS is enabled and its maximum scale is set to lower than 100%: https://github.com/NVIDIA/Q2RTX/issues/96
  * Fixed the render corruption when running the game on GPUs with 6 GB of memory at 4K resolution: https://github.com/NVIDIA/Q2RTX/issues/98
  * Fixed the SBT size for hit and miss shaders, preventing potential issues with future drivers that might rely on that information.

**Denoiser Improvements:**

  * Reduced the noise on first person weapons.

**Misc Improvements:**
  
  * Added a driver version check for AMD GPUs to make sure that at least version 21.1.1 is used.
  * Added an option to build `glslangValidator` as a submodule.


## 1.4.0

**New Features:**

  * Added support for the final Vulkan Ray Tracing API. The game can now run on any GPU supporting the `VK_KHR_ray_tracing_pipeline` extension.
  * Added temporal upscaling, or TAAU, for improved image quality at lower resolution scales.

**Fixed Issues:**

  * Fixed a crash that happened when there are no available sound devices.
  * Fixed a few issues with the tone mapper and the profiler for AMD GPU compatibility.
  * Fixed a server crash: https://github.com/NVIDIA/Q2RTX/issues/86
  * Fixed black materials and some light leaks: https://github.com/NVIDIA/Q2RTX/issues/55
  * Fixed building the game with GCC10 on Linux: https://github.com/NVIDIA/Q2RTX/issues/80
  * Fixed missing railgun lights in photo mode: https://github.com/NVIDIA/Q2RTX/issues/75
  * Fixed missing sun light on geometry with invalid clusters.
  * Fixed the CFLAGS for MinSizeRel and RelWithDebInfo builds to generate correct debug symbols.
  * Fixed the game stuttering on Linux: https://github.com/NVIDIA/Q2RTX/issues/62
  * Fixed the issue with all models being missing or corrupted on some maps during network play.
  * Fixed the nearest filter when DRS was enabled and then disabled.
  
**Denoiser Improvements:**

  * Implemented a new gradient estimation algorithm that makes the image more stable in reflections and refractions.
  * Implemented sampling across checkerboard fields in the temporal filter to reduce blurring.
  * Improved motion vectors for multiple refraction, in particular when thick glass is enabled.
  * Improved the temporal filter to avoid smearing on surfaces that appear at small glancing angles, e.g. on the floor when going up the stairs.
  * Improved the temporal filter to make lighting more stable on high-detail surfaces.

  
**Misc Improvements:**

  * Added git branch name to the game version info.
  * Improved in-game screenshot feature performance.
  * Improved the console log to get more information in case of game crashes.
  * Increased precision of printed FPS when running timedemos.
  * Made the `wrote <filename>` message that was issued when taking screenshots optional, controlled by the `gl_screenshot_message` cvar.
  * Reduced the amount of stutter that happened when new geometry is loaded, like on weapon pickup.
  * Replaced the Vulkan headers stored in the repository with a submodule pointing to https://github.com/KhronosGroup/Vulkan-Headers
  * Static resolution scale can now be set to as low as 25%.
  * Updated SDL2 version to changeset 13784.
  * Vulkan validation layer can now be enabled through the `vk_validation` cvar.


## 1.3.0

**New Features:**

  * Added support for video cutscenes.
  * Added support for Depth of Field in the reference path tracing mode.
  * Added free camera controls when the game is paused. See the [Readme](readme.md) for more information.
  * Added support for selecting which display should be used for the fullscreen mode.
  * Added support for loading map-specific files with sky clusters, which should be useful for custom maps.
  * Added display of the selected inventory item name in the status bar.

**Fixed Issues:**

  * Fixed a crash that happened at map load time when a custom map has no analytic lights.
  * Reduced the noise in the `biggun` map next to the barred windows.
  * Reduced the noise from yellow lamps next to the entrance of the `jail4` map at night.

**Misc Improvements:**

  * Improved the menu settings to show units for various sliders, such as degrees or percentage.
  * Made the volume controls logarithmic instead of linear.

## 1.2.1

**Fixed Issues:**

  * Fixed the bug with broken OpenAL sound on certain maps: https://github.com/NVIDIA/Q2RTX/issues/47
  * Fixed the material on the pipe at the end of the `strike` map.
  * Fixed a typo in the `pt_enable_sprites` cvar name.
  * Fixed the handling of swap chain image layouts to avoid the black screen bug on a future driver version [200570279]
  * Restored the "projection" setting in the Video menu.

## 1.2.0

**New Features:**

  * Added support for dynamic resolution scaling that adjusts rendering resolution to meet an FPS target.
  * Added support for multiple reflection or refraction bounces.
  * Added light coloring by tinted glass.
  * Added support for security camera views on some monitors in the game.
  * Added god rays in reflections and refractions, improved god rays filtering.
  * Added a spatial denoiser for the specular channel.
  * Added support for loading custom sky (portal) light meshes from .obj files, and added portal light definitions for many maps in the base game.
  * Added triangular lights for laser beams: https://github.com/NVIDIA/Q2RTX/issues/43 
  * Added “shader balls” to the shipping builds.
  * Added cvar `pt_accumulation_rendering_framenum` to control how many frames to accumulate in the reference mode.
  * Added cvar `pt_show_sky` to make analyzing skybox geometry easier.

**Fixed Issues:**

  * Fixed the stutter caused by Steam overlay by updating to the latest version of SDL2.
  * Fixed Stroggos atmospheric scattering (sky color) and overall sky brightness.
  * Fixed light scattering on the clouds.
  * Fixed the issue with overexposed player setup screen: https://github.com/NVIDIA/Q2RTX/issues/18
  * Fixed the sudden darkening of the screen: https://github.com/NVIDIA/Q2RTX/issues/26
  * Fixed the "PF_centerprintf to a non-client" warnings that happened on the "command" map when the computers are blown up, instead of informative on-screen messages.
  * Fixed missing GI on reflections of the first person model.

**Denoising and image stability improvements:**

  * Improved image quality and temporal stability of reflections and refractions by computing correct motion vectors for reflected surfaces and surfaces visible through flat glass.
  * Disabled the pixel jitter when temporal AA is turned off.
  * Added sample jitter to the spatial denoiser to improve the noise patterns that appear after light invalidation.
  * Improved image stability around blinking lights by using the light intensities from the previous frame on gradient pixels.
  * Improved stability of indirect lighting from sphere lights by limiting their contribution.
  * Added storage scaling for all lighting channels to avoid color quantization.
  * Fixed flickering that happened when the number of dynamic lights changes.
  * Improved sharpness of textured glass and similar transmissive effects by passing them around the denoiser.
  * Added multiple importance sampling of specular reflections of direct lights.
  * Replaced sphere lights that were used for wall lamps (mostly in the “train” map) with polygon lights to reduce noise.
  * Added an upper limit on sky luminance to avoid oversampling the sky in shadowed areas and thus reduce noise from direct lights.
  * Added light sampling correction based on statistical per-cluster light visibility. The idea is, if we see that a light is usually not visible, let's not sample it so much.

**Material improvements:**

  * Metals are now rendered much better thanks to the denoiser and BRDF improvements.
  * Over 400 textures have been adjusted or repainted.
  * Removed the nonlinear transform from emissive textures, and reduced the negative bias applied to them.
  * Force light materials to be opaque to make the laser lights in mine maps appear in reflections.
  * Restore specular on materials with roughness = 1, but make specular on rough dielectrics much dimmer.
  
**Shading and BRDF improvements:**

  * Fixed scaling of diffuse and specular reflections: https://github.com/NVIDIA/Q2RTX/issues/37
  * Fixed relative brightness and spotlight terms for different light types.
  * Hemisphere sampling for indirect diffuse tuned to make the results better match the cosine-weighted sampling in reference mode.
  * Trace more specular rays for shiny metals that do not need indirect diffuse lighting.
  * Fixed the misdirection of second bounce lighting into the diffuse channel when the light path starts as specular on the primary surface: https://github.com/NVIDIA/Q2RTX/issues/25
  * Replaced regular NDF sampling for specular bounce rays with visible NDF sampling: https://github.com/NVIDIA/Q2RTX/issues/40 
  * Tuned fake specular to be closer to reference specular.
  * Added normalization of normal map vectors on load to avoid false roughness adjustments by the Toksvig factor.
  * Improved roughness correction to not happen on texture magnification, and to better handle cases like zero roughness.
  * Fixed the computation of N.V to avoid potential NaNs: https://github.com/NVIDIA/Q2RTX/issues/23

**Misc Improvements:**

  * Removed the multiplayer specific sun position setting, and changed the remaining setting to be morning by default.
  * Changed the default value of texture LOD bias to 0 for extra sharpness.
  * Use nearest filter for upscaling from 50% resolution scale (a.k.a integer scaling).
  * Made the brightness of sprites, beams, particles, smoke, and effects like quad damage be similar to overall scene brightness, to avoid having them washed out under direct sunlight.
  * Made the explosions and other sprites appear in low-roughness specular reflections.
  * Changed the flare lights to be derived from the flare model entity instead of the effect event, which makes the lights follow the flares smoothly in flight, and reduces flicker that was due to frequent light creation and destruction.
  * Removed insane and dead soldiers with `nomonsters 1`.
  * Random number generator now produces enough different sequences to cover thousands of frames for reference mode accumulation.
  * The `pt_direct_polygon_lights` cvar has a new meaning when set to -1: all polygon lights are sampled through the indirect lighting shader, for comparison purposes.
  * Moved the first person player model a bit backwards to avoid having it block reflections.

## 1.1.0

**New Features:**

  * Added music playback support - see the [Readme](readme.md) for instructions.

**Fixed Issues:**

  * Fixed the crash with message "recursive error: bad tail" that sometimes happened at the end of the `biggun` map.
  * Fixed the issue with players spawning at the wrong level entrance after loading an autosave: https://github.com/NVIDIA/Q2RTX/issues/13
  * Fixed the Linux install script to work with spaces in paths: https://github.com/NVIDIA/Q2RTX/pull/1
  * Fixed the interpretation of `pt_fake_roughness_threshold`: https://github.com/NVIDIA/Q2RTX/issues/9
  * Fixed the bright noise that appeared at the end of the `hangar2` map after closing the hangar doors.
  * Fixed the image blurring on FOV changes.
  * Added limits for sky brightness to avoid denoiser artifacts when the sky is too bright.

**Other Improvements:**

  * Re-arranged some menu options to make the menu less confusing.
  * Added the player models from the Quake II shareware demo to the package.
  * Added a menu option to invert mouse controls.
  * Enabled cl_adjustfov by default because that works better for wide screens.
  * Tweaked the tone mapper to make really dark places brighter.

## 1.0.0

**Initial Release**
