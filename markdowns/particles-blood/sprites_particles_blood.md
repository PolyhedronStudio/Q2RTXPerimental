Invoked subagent

I have launched two specialized research subagents to inspect the `Q2RTXPerimental` codebase in detail:
1. **VKPT Particle & Sprite Researcher** — investigating `transparency.c`, acceleration structure builds (BLAS/TLAS), ray-tracing shaders, and the `clgame` particle/sprite/decal pipelines.
2. **Model & Alembic Architecture Researcher** — investigating the `.md3`/`.md2`/`.iqm` model loaders, `instance_geometry.comp`, dynamic BLAS topology constraints, and Alembic `.abc` fluid mesh integration strategies.

I will compile a comprehensive technical breakdown as soon as their analysis completes.

Still awaiting the second research subagent's report on the VKPT particle, sprite, and decal rendering pipelines. I will synthesize the complete findings as soon as it finishes.

Here is a complete architectural and performance analysis of **Alembic (`.abc`) blood splatters**, **decals**, and **particle/sprite rendering** in **Q2RTXPerimental**, based on how the `clgame` and `vkpt` pipelines operate today.

---

# Part 1: Alembic (`.abc`) Blood Splatters & Decals

## 1.1 Can `.md3` Store Varying Vertex Counts Per Frame?
**No.** Neither the Quake III `.md3` format specification nor Q2RTXPerimental’s model loader supports varying vertex or triangle counts across frames within a surface:
* **Format Constraint ([`dmd3mesh_t`](file:///d:/Dev/Projects/Q2RTXPerimental/code/inc/format/md3.h#L74-L91))**: Each surface header defines a single `num_frames`, a single fixed `num_verts`, a single fixed `num_tris`, **one** shared index buffer (`ofs_indexes`), and **one** shared UV array (`ofs_tcs`). Only vertex positions and normals (`ofs_verts`) are duplicated across `num_frames`.
* **Loader Constraint ([`MOD_LoadMD3Mesh`](file:///d:/Dev/Projects/Q2RTXPerimental/code/src/refresh/vkpt/models.c#L533-L675))**: Enforces `header.num_verts <= TESS_MAX_VERTICES` (`16,384` in VKPT) and expects the vertex lump size to be strictly `header.num_verts * model->numframes * sizeof(dmd3vertex_t)` ([`models.c`](file:///d:/Dev/Projects/Q2RTXPerimental/code/src/refresh/vkpt/models.c#L574-L575)).

## 1.2 Why Padding an `.md3` with Degenerate Triangles Breaks VKPT
A common workaround in rasterizers is to pad every frame of a fluid mesh with zero-area (`[0,0,0]`) triangles so every frame has `max_tris` triangles. **In Q2RTXPerimental’s path tracer, this causes three severe problems:**

1. **Exploding Geometry During Frame Interpolation (`backlerp`)**:
   In [`instance_geometry.comp`](file:///d:/Dev/Projects/Q2RTXPerimental/code/src/refresh/vkpt/shader/instance_geometry.comp#L168-L205), the compute shader blends vertex $v \in \{0,1,2\}$ of triangle `idx` between `oldframe` and `frame`:
   ```glsl
   t.positions[vtx] = vec3(transform * vec4(mix(t_a_curr.positions[vtx], t_b_curr.positions[vtx], mi.pose_lerp_curr_frame), 1.0));
   ```
   Because an Alembic fluid simulation remeshes every frame (changing topology), triangle `idx` in Frame $N$ has no correspondence to triangle `idx` in Frame $N+1$. Any `backlerp > 0` stretches giant spiking polygons across the splatter volume.
2. **Corrupted Motion Vectors & ASVGF Denoiser Smearing (Even When `backlerp == 0`!)**:
   Even if you force `ent.oldframe = ent.frame` and `ent.backlerp = 0.0f`, [`prepare_entities`](file:///d:/Dev/Projects/Q2RTXPerimental/code/src/refresh/vkpt/main.c#L2608-L2628) matches entities across render frames by hashing `(entity->id, entity->model, mesh_idx, bsp)` ([`main.c`](file:///d:/Dev/Projects/Q2RTXPerimental/code/src/refresh/vkpt/main.c#L1606-L1611)).
   When the splatter advances from Frame $N$ to Frame $N+1$, `mi_curr->prim_offset_curr_pose_prev_frame` still points to Frame $N$'s triangles while `mi_curr->prim_offset_curr_pose_curr_frame` points to Frame $N+1$'s triangles. [`instance_geometry.comp`](file:///d:/Dev/Projects/Q2RTXPerimental/code/src/refresh/vkpt/shader/instance_geometry.comp#L185-L201) then computes `t.positions_prev[vtx] - t.positions[vtx]` between two completely unrelated triangles and writes that into `prim.custom0..2`. [`primary_rays.rgen`](file:///d:/Dev/Projects/Q2RTXPerimental/code/src/refresh/vkpt/shader/primary_rays.rgen#L214-L272) projects those into `IMG_PT_MOTION` and `IMG_PT_VISBUF_PRIM_A`, causing **ASVGF and TAAU to violently smear and ghost** the splatter and background.
3. **Unnecessary Per-Frame Compute & Dynamic BLAS Rebuilds**:
   In [`vkpt_vertex_buffer_upload_models`](file:///d:/Dev/Projects/Q2RTXPerimental/code/src/refresh/vkpt/vertex_buffer.c#L1014), any model with `model->numframes > 1` is classified as `model_is_static = false`:
   ```c
   bool model_is_static = model->numframes == 1 && (!model->skmData || !model->skmData->blend_indices);
   ```
   Every active `numframes > 1` instance must run [`instance_geometry.comp`](file:///d:/Dev/Projects/Q2RTXPerimental/code/src/refresh/vkpt/shader/instance_geometry.comp) and rebuild its triangles inside `blas_dynamic` ([`vkpt_pt_create_all_dynamic`](file:///d:/Dev/Projects/Q2RTXPerimental/code/src/refresh/vkpt/path_tracer.c#L1058)) every single render frame—including thousands of degenerate `[0,0,0]` triangles that degrade BVH build and traversal performance.

---

## 1.3 Best Solutions for 3D Alembic Blood Splatters

### Solution 1 (Immediate, Zero Engine Changes): Per-Frame 1-Frame Static `.md3` / `.iqm` Sequence
Instead of packing 30 fluid frames into one `.md3`, export each frame of the `.abc` animation as its own 1-frame static mesh (`blood_splat01_f00.md3` … `blood_splat01_f29.md3`, or 1-frame `.iqm` without joints for full 32-bit `float` vertex precision) and step `ent.model = splat_frames[frame_idx]` in `clgame`.

**Why this is extraordinarily fast in Q2RTXPerimental:**
* **Pre-Built `PREFER_FAST_TRACE` Static BLAS**: Because each frame model has `model->numframes == 1`, [`vkpt_vertex_buffer_upload_models`](file:///d:/Dev/Projects/Q2RTXPerimental/code/src/refresh/vkpt/vertex_buffer.c#L1014-L1185) classifies it as `model_is_static = true` and builds a persistent, high-quality (`VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR`) BLAS at load time containing only that frame's exact triangle count.
* **Zero Per-Frame Compute & Zero Dynamic BLAS Rebuilds**: At runtime, [`process_regular_entity`](file:///d:/Dev/Projects/Q2RTXPerimental/code/src/refresh/vkpt/main.c#L2147-L2203) takes the `use_static_blas == true` fast path:
  * **0** threads dispatched in `instance_geometry.comp`.
  * **0** triangles written to `qvk.buf_primitive_instanced` or rebuilt in `blas_dynamic`.
  * Each active 3D blood splatter in the scene adds only **1 `VkAccelerationStructureInstanceKHR` (64 bytes)** into `tlas_geometry`!
* **Automatic ASVGF Motion-Vector Safety**: Because `entity_hash_t` ([`main.c`](file:///d:/Dev/Projects/Q2RTXPerimental/code/src/refresh/vkpt/main.c#L1606-L1611)) includes `entity->model` in its hash key, changing `entity->model` each frame step automatically resets temporal triangle matching and uses clean rigid-body `transform_prev` motion vectors.

### Solution 2 (Best Clean Engine Upgrade): Single-Handle Stop-Motion BLAS Sequence
To avoid managing 25–30 `qhandle_t` model handles per splatter variation in `clgame`, we can extend [`model_vbo_t`](file:///d:/Dev/Projects/Q2RTXPerimental/code/src/refresh/vkpt/vkpt.h#L412-L430) to support **multi-frame static BLAS sequences** (loaded either from a lightweight multi-frame variable-topology container, a directory/manifest of frames, or directly from an offline-converted `.abc` stream):
* At load time, `vkpt_vertex_buffer_upload_models` builds a `PREFER_FAST_TRACE` static BLAS per frame (`vbo->frame_geom_opaque[frame]`).
* At runtime, `clgame` uses a **single model handle** and increments `ent.frame++`. [`process_regular_entity`](file:///d:/Dev/Projects/Q2RTXPerimental/code/src/refresh/vkpt/main.c#L2147) simply binds `vbo->frame_geom_opaque[ent.frame]` into `tlas_geometry`.
* You get full path-traced 3D blood (with real refraction/absorption, wet PBR specular highlights, and ray-traced shadow casting) at the exact same **zero dynamic BLAS / zero compute cost** as Solution 1.

| Criteria | Fixed-Topo `.md3` (Degenerate Triangle Padding) | Vertex Animation Texture (VAT) | **Solution 1: Per-Frame 1-Frame `.md3`/`.iqm` Swap** | **Solution 2: Engine Stop-Motion Per-Frame Static BLAS** |
| :--- | :--- | :--- | :--- | :--- |
| **Varying Vertex/Tri Count** | Fake (padded with `[0,0,0]` tris) | Fake (padded with `[0,0,0]` tris) | **Native per-frame count** | **Native per-frame count** |
| **`instance_geometry.comp` Cost** | High (`max_tris` per instance) | High (`max_tris` per instance) | **Zero** (`use_static_blas`) | **Zero** (`use_static_blas`) |
| **Per-Frame `blas_dynamic` Cost** | High (`max_tris` per instance) | High (`max_tris` per instance) | **Zero** (pre-built BLAS) | **Zero** (pre-built BLAS) |
| **ASVGF / Motion Vectors** | Breaks (requires ID/shader hack) | Breaks (requires ID/shader hack) | **Clean out of the box** | **Clean** |
| **Engine Code Changes** | Minimal | High | **None** | Small (`model_vbo_t` array) |

---

## 1.4 Connecting Blood Splatters to the Decal System
Our inspection of `clgame` and `vkpt` revealed why blood decals are currently missing and how to wire them up:
1. **Why Blood Decals Don't Appear Today**:
   * In [`CLG_FX_BloodParticleEffect`](file:///d:/Dev/Projects/Q2RTXPerimental/code/src/baseq2rtxp/clgame/effects/clg_fx_classic.cpp#L143-L200), the old decal spawn code at lines 144–151 is commented out.
   * Meanwhile, Q2RTXPerimental **already has a complete, BSP-clipped decal system** in [`clg_decals.cpp`](file:///d:/Dev/Projects/Q2RTXPerimental/code/src/baseq2rtxp/clgame/decals/clg_decals.cpp) and [`vkpt_decals_geometry.c`](file:///d:/Dev/Projects/Q2RTXPerimental/code/src/refresh/vkpt/decals/vkpt_decals_geometry.c)!
   * In [`sg_decal_shared.h`](file:///d:/Dev/Projects/Q2RTXPerimental/code/src/baseq2rtxp/sharedgame/sg_decal_shared.h#L42-L67), `SG_DECAL_MATERIAL_HASH_BLOOD_FLESH` is already defined, though `SG_Decal_GetMaterialNameByHash()` currently maps it to placeholder `"textures/decals/bullets/bullethole_01"`.
2. **How to Trigger Blood Decals from Splatters**:
   * When `EV_FX_BLOOD` / `EV_FX_MORE_BLOOD` triggers (alongside spawning the 3D Alembic splatter), `clgame` can fire a small fan of ballistic/gravity traces (or trace along the splatter direction and downward droplet arcs) against the local BSP/mover collision model (`CLG_DecalEvents_TraceImpactLine` in [`clg_decal_events.cpp`](file:///d:/Dev/Projects/Q2RTXPerimental/code/src/baseq2rtxp/clgame/decals/clg_decal_events.cpp#L75-L121)) and queue clipped blood decals via `CLG_Decals_QueueSpawn()`.
3. **ImportantVKPT Decal Shading Fix Needed for Blood**:
   * Currently, `blas_decals` is placed in `tlas_effects` ([`path_tracer.c`](file:///d:/Dev/Projects/Q2RTXPerimental/code/src/refresh/vkpt/path_tracer.c#L1088)) and shaded in [`pt_logic_decal`](file:///d:/Dev/Projects/Q2RTXPerimental/code/src/refresh/vkpt/shader/path_tracer_hit_shaders.h#L134-L166) during `trace_effects_ray()`. Because `trace_effects_ray()` accumulates **unlit** color in front of the primary ray hit (`update_payload_transparency`), decals do not receive scene lighting or shadows—meaning a blood decal on a wall in a dark room will appear unlit/glowing!
   * To make blood decals look truly path-traced, primary rays in [`primary_rays.rgen`](file:///d:/Dev/Projects/Q2RTXPerimental/code/src/refresh/vkpt/shader/primary_rays.rgen) should either apply decal albedo/normal/roughness modulation **to the underlying surface hit's PBR material properties before lighting** (stained/wet surface), or multiply non-emissive decals in `tlas_effects` by the hit surface's irradiance.

---

# Part 2: Improving Overall Particles — Option A vs. Option B

## 2.1 How the Current Particle & Sprite Systems Actually Work
First, a key clarification from the codebase:
* **Particles are not hardware GPU point sprites (`VK_PRIMITIVE_TOPOLOGY_POINT_LIST`)**:
  In [`write_particle_geometry`](file:///d:/Dev/Projects/Q2RTXPerimental/code/src/refresh/vkpt/transparency.c#L351-L430), the CPU already expands every [`particle_t`](file:///d:/Dev/Projects/Q2RTXPerimental/code/inc/refresh/shared_types.h#L775-L782) into a **4-vertex, 2-triangle camera-facing quad** in `transparency.vertex_buffer` and builds a single dynamic BLAS (`blas_particles`) in `tlas_effects`.
* **Why particles look like cheap glowing point sprites**:
  1. [`particle_t`](file:///d:/Dev/Projects/Q2RTXPerimental/code/inc/refresh/shared_types.h#L775-L782) (32 bytes) only carries `origin`, `color`, `alpha`, `rgba`, `brightness`, and `radius` (and [`CLG_AddParticles`](file:///d:/Dev/Projects/Q2RTXPerimental/code/src/baseq2rtxp/clgame/effects/clg_fx_particles.cpp#L112) hardcodes `part->radius = 0.f`!).
  2. [`pt_logic_particle`](file:///d:/Dev/Projects/Q2RTXPerimental/code/src/refresh/vkpt/shader/path_tracer_hit_shaders.h#L112-L132) ignores textures entirely, evaluates a procedural radial distance (`length(vec2(0.5) - uv) * 2.0`), and multiplies every particle’s RGB by `global_ubo.prev_adapted_luminance * global_ubo.pt_particle_brightness`—turning every particle (smoke, dust, blood, sparks) into a self-luminous glowing circle.
* **How `MOD_SPRITE` (`.sp2` / `.spj`) works**:
  In [`update_transparency`](file:///d:/Dev/Projects/Q2RTXPerimental/code/src/refresh/vkpt/transparency.c#L223-L237) and [`write_sprite_geometry`](file:///d:/Dev/Projects/Q2RTXPerimental/code/src/refresh/vkpt/transparency.c#L721-L830), entities with `MOD_SPRITE` models are *also* expanded on the CPU into 4-vertex camera-facing quads in the very same `transparency.vertex_buffer`, built into `blas_sprites` in `tlas_effects`, and shaded by [`pt_logic_sprite`](file:///d:/Dev/Projects/Q2RTXPerimental/code/src/refresh/vkpt/shader/path_tracer_hit_shaders.h#L168-L197).

---

## 2.2 Option A vs. Option B: Architectural & Framerate Analysis

### Why Option A (Expanding `r_entities` / `MOD_SPRITE` to Render Particles) Will Tank Framerates
Routing 16,384+ particles through `clgi.V_AddEntity()` as `MOD_SPRITE` entities and expanding `MAX_ENTITIES` (`8,192`) introduces severe CPU and memory bottlenecks:
1. **6.5x Struct Bloat**: [`entity_t`](file:///d:/Dev/Projects/Q2RTXPerimental/code/inc/refresh/shared_types.h#L623-L698) is **~208 bytes**, whereas [`particle_t`](file:///d:/Dev/Projects/Q2RTXPerimental/code/inc/refresh/shared_types.h#L775-L782) is **32 bytes**.
2. **Per-Frame CPU `qsort` Bottleneck**:
   In [`CLG_DrawActiveViewState`](file:///d:/Dev/Projects/Q2RTXPerimental/code/src/baseq2rtxp/clgame/clg_view.cpp#L864), the client game runs `qsort` over the entire `r_entities` array every single frame:
   ```cpp
   qsort( clgi.client->viewScene.r_entities, clgi.client->viewScene.r_numentities, sizeof( clgi.client->viewScene.r_entities[ 0 ] ), CLG_ShellEffectHack );
   ```
   Dumping 16,384 particles into `r_entities` forces an $O(N \log N)$ CPU sort over ~20,000 208-byte structures every frame, thrashing L1/L2 cache.
3. **Redundant Linear Loops & Call-Stack Overflow Hazard**:
   Both [`prepare_entities`](file:///d:/Dev/Projects/Q2RTXPerimental/code/src/refresh/vkpt/main.c#L2389) and [`update_transparency`](file:///d:/Dev/Projects/Q2RTXPerimental/code/src/refresh/vkpt/transparency.c#L223) iterate over `fd->num_entities` and call `MOD_ForHandle(e->model)` on every entry. Worse, `MAX_ENTITIES` (`8,192`) directly sizes `EntityUploadInfo upload_info` **on the C call stack** inside [`prepare_entities`](file:///d:/Dev/Projects/Q2RTXPerimental/code/src/refresh/vkpt/main.c#L2372) (~1.1 MB stack allocation today). Increasing `MAX_ENTITIES` to `24,576+` will **overflow the Windows thread stack**!
4. **`pt_logic_sprite` Is Still Hardcoded for Glowing Fireballs**:
   Even if you used `MOD_SPRITE`, [`write_sprite_geometry`](file:///d:/Dev/Projects/Q2RTXPerimental/code/src/refresh/vkpt/transparency.c#L721-L830) and [`pt_logic_sprite`](file:///d:/Dev/Projects/Q2RTXPerimental/code/src/refresh/vkpt/shader/path_tracer_hit_shaders.h#L168-L197) ignore `entity_t::rgba` tinting, ignore 2D roll rotation (`e->angles[ROLL]`), and apply a hardcoded `pow(lum, 2.2)` contrast curve and `2000 * prev_adapted_luminance` emissive multiplier.

---

### Why Option B (Upgrading the Dedicated Particle Pipeline into a Fast Sprite-Particle System) Is Our Best Bet
Both `blas_particles` and `blas_sprites` already share [`transparency.c`](file:///d:/Dev/Projects/Q2RTXPerimental/code/src/refresh/vkpt/transparency.c) and `tlas_effects`. By **replacing the procedural point-circle particle path in `transparency.c` with a dedicated, high-throughput Sprite-Particle path** (Option B), we get all the visual power of sprites with zero `entity_t` overhead:

1. **Compact Data & Zero `qsort` Overhead**:
   Keep `r_particles` separate from `r_entities`. Extend [`particle_t`](file:///d:/Dev/Projects/Q2RTXPerimental/code/inc/refresh/shared_types.h#L775-L782) (from 32 bytes to 48–64 bytes) to support:
   * `int32_t image_or_material` (bindless texture index / `.spj` frame or `0` for soft procedural fallback)
   * `float radius` / `vec2_t size` (actually passed through from `clg_particle_t` instead of hardcoded to `0.f` in [`CLG_AddParticles`](file:///d:/Dev/Projects/Q2RTXPerimental/code/src/baseq2rtxp/clgame/effects/clg_fx_particles.cpp#L112)!)
   * `float roll` or `vec3_t velocity_stretch` (for rotating smoke/debris or motion-stretched sparks/blood droplets)
   * `uint32_t flags` (e.g., `PARTICLE_FLAG_EMISSIVE` vs. `PARTICLE_FLAG_LIT`, additive vs. alpha-blend, flipbook UV rect)
2. **Optimal Vulkan RT Acceleration Structure Layout**:
   All particle quads remain packed in world space into a **single `PREFER_FAST_BUILD` triangle BLAS (`blas_particles`)** with **1 TLAS instance** in `tlas_effects` ([`path_tracer.c`](file:///d:/Dev/Projects/Q2RTXPerimental/code/src/refresh/vkpt/path_tracer.c#L835-L836)). In hardware ray tracing, 1 world-space triangle BLAS is vastly faster to traverse than thousands of overlapping per-particle TLAS instances.
3. **Negligible PCIe Bandwidth**:
   Adding texture index, UV sub-rect (for flipbook atlases), and shading flags to the particle buffer adds only **16 bytes per particle** (`80 bytes/particle` total = `1.25 MB/frame` at full `16,384` particle capacity).
4. **Lit vs. Emissive Particles Without Secondary Ray Cost**:
   Instead of forcing every particle to glow via `prev_adapted_luminance * pt_particle_brightness`, `pt_logic_particle` can branch on the particle’s mode flag:
   * **Emissive particles** (sparks, plasma, fire, blaster): Use HDR emissive scaling.
   * **Lit dielectric particles** (blood droplets/mist, smoke, dust, dirt): Modulate the sprite texture + RGBA by ambient/SH scene irradiance (or light-cluster lookup) so blood and smoke look grounded in dark corridors instead of glowing in the dark.

---

## 2.3 Two Existing Bugs in `vkpt` You Should Know About Before Touching Particles
While auditing `transparency.c` and `path_tracer_hit_shaders.h`, we uncovered two latent bugs in the current particle/sprite code that **must** be fixed when implementing Option B:

1. **Bug 1 — Broken Triangle 1 UVs in [`pt_logic_particle`](file:///d:/Dev/Projects/Q2RTXPerimental/code/src/refresh/vkpt/shader/path_tracer_hit_shaders.h#L112-L132)**:
   Look at line 114 of [`path_tracer_hit_shaders.h`](file:///d:/Dev/Projects/Q2RTXPerimental/code/src/refresh/vkpt/shader/path_tracer_hit_shaders.h#L114):
   ```glsl
   vec2 uv = vec2(0.0, 0.0) * barycentric.x + vec2(1.0, 0.0) * barycentric.y + vec2(1.0, 1.0) * barycentric.z;
   ```
   Unlike [`pt_logic_sprite`](file:///d:/Dev/Projects/Q2RTXPerimental/code/src/refresh/vkpt/shader/path_tracer_hit_shaders.h#L171-L177), `pt_logic_particle` **forgot to check `if ((primitiveID & 1) == 0)`**! It uses Triangle 0’s UV coordinates for both triangles of the quad. That only went unnoticed because a radial circle centered at `(0.5, 0.5)` is symmetric across the `u == v` diagonal. As soon as you sample a texture in `pt_logic_particle`, half the quad will be mirrored/sheared unless we use the `(primitiveID & 1) == 0` UV mapping from `pt_logic_sprite`.
2. **Bug 2 — `uint16_t` Index Overflow & Buffer Size Typo in [`transparency.c`](file:///d:/Dev/Projects/Q2RTXPerimental/code/src/refresh/vkpt/transparency.c#L123-L125)**:
   * In [`initialize_transparency`](file:///d:/Dev/Projects/Q2RTXPerimental/code/src/refresh/vkpt/transparency.c#L123-L125), `sprite_vertex_position_max_size = TR_SPRITE_MAX_NUM * TR_POSITION_SIZE;` is missing `* 4` (4 vertices per quad), whereas `particle_vertex_position_max_size` accidentally used `TR_VERTEX_MAX_NUM` (`(TR_PARTICLE_MAX_NUM + TR_SPRITE_MAX_NUM) * 4`).
   * In [`fill_index_buffer`](file:///d:/Dev/Projects/Q2RTXPerimental/code/src/refresh/vkpt/transparency.c#L1080-L1095), `const uint16_t base_vertex = i * 4;` overflows 16-bit unsigned integer at `i = 16,384` (`16384 * 4 = 65536 -> 0`). If we ever raise `MAX_PARTICLES` above `16,384`, `transparency.index_buffer` must be switched to `VK_INDEX_TYPE_UINT32`.

---

# Summary of Recommended Action Plan

1. **For Particles (Choose Option B)**:
   * Keep the dedicated particle submission array (`r_particles`) and single dynamic BLAS (`blas_particles`), completely avoiding `entity_t` and `qsort` overhead.
   * Expand `clg_particle_t` and `particle_t` to carry **sprite texture/flipbook frame handle**, **radius/scale**, **roll rotation / velocity stretch**, and **emissive vs. lit flags**.
   * Fix the `(primitiveID & 1) == 0` UV mapping in [`pt_logic_particle`](file:///d:/Dev/Projects/Q2RTXPerimental/code/src/refresh/vkpt/shader/path_tracer_hit_shaders.h#L114) and upgrade `pt_logic_particle` to sample bindless sprite/flipbook textures (with optional normal/lighting modulation for non-emissive particles like blood mist and smoke).
2. **For Alembic (`.abc`) Blood Splatters**:
   * **Do not** pack variable-topology fluid frames into a single `.md3` with degenerate triangles (it bloats `blas_dynamic`, runs unnecessary compute, and corrupts ASVGF motion vectors).
   * **Short-term**: Export each `.abc` frame as a 1-frame `.md3` or `.iqm` (`splat_00` .. `splat_NN`) and step `ent.model` in `clgame` (**Solution 1**), which automatically hits VKPT's `use_static_blas == true` fast path with **zero** dynamic BLAS or compute overhead.
   * **Medium-term**: Add a multi-frame static BLAS array to [`model_vbo_t`](file:///d:/Dev/Projects/Q2RTXPerimental/code/src/refresh/vkpt/vkpt.h#L412-L430) (**Solution 2**) so a single model handle can step through pre-built per-frame `PREFER_FAST_TRACE` BLASes via `ent.frame`.
3. **For Blood Decals**:
   * Hook [`CLG_FX_BloodParticleEffect`](file:///d:/Dev/Projects/Q2RTXPerimental/code/src/baseq2rtxp/clgame/effects/clg_fx_classic.cpp#L143-L200) (and blood splatter impacts) into the existing BSP-clipped decal pipeline (`CLG_Decals_QueueSpawn` in [`clg_decals.cpp`](file:///d:/Dev/Projects/Q2RTXPerimental/code/src/baseq2rtxp/clgame/decals/clg_decals.cpp)), assign real blood textures to [`SG_DECAL_MATERIAL_HASH_BLOOD_FLESH`](file:///d:/Dev/Projects/Q2RTXPerimental/code/src/baseq2rtxp/sharedgame/sg_decal_shared.h#L42-L67), and update decal shading so non-emissive decals stain/modulate the lit surface rather than glowing in the dark.