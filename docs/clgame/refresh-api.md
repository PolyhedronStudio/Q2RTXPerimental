# Client Game Refresh API & Rendering Pipeline

A comprehensive, production-grade guide to the 2D canvas, TrueType MTSDF font engine, and 3D world-space rendering pipeline exposed to the ClientGame (`clgame`) subsystem.

---

## Table of Contents
1. [Architectural Overview & Design Principles](#1-architectural-overview--design-principles)
2. [The ClientGame Refresh Architecture](#2-the-clientgame-refresh-architecture)
3. [The 2D Canvas & Styling Subsystem](#3-the-2d-canvas--styling-subsystem)
	- [3.1 Virtual Resolution, Coordinates & Viewport Scissoring](#31-virtual-resolution-coordinates--viewport-scissoring)
	- [3.2 Image Registration, Atlases & Texture Lifecycles](#32-image-registration-atlases--texture-lifecycles)
	- [3.3 2D Primitives: Pictures, Fills, Lines & Polygons](#33-2d-primitives-pictures-fills-lines--polygons)
	- [3.4 Modern GPU Styling Pipeline: Strokes, Glows & Rounded Corners](#34-modern-gpu-styling-pipeline-strokes-glows--rounded-corners)
4. [TrueType / OpenType & The MTSDF Font Engine](#4-truetype--opentype--the-mtsdf-font-engine)
	- [4.1 The Fundamental Flaw of Single-Channel Signed Distance Fields (SDF)](#41-the-fundamental-flaw-of-single-channel-signed-distance-fields-sdf)
	- [4.2 Multi-Channel Signed Distance Fields (MSDF) & Median Evaluation](#42-multi-channel-signed-distance-fields-msdf--median-evaluation)
	- [4.3 Why MTSDF: Multi-Channel True-Distance Signed Distance Fields with Alpha](#43-why-mtsdf-multi-channel-true-distance-signed-distance-fields-with-alpha)
	- [4.4 Technical Operation: Vector Decomposition, Edge Coloring & Atlas Packing](#44-technical-operation-vector-decomposition-edge-coloring--atlas-packing)
	- [4.5 The `.mtsdf` Binary Disk Cache & Zero-Cost Runtime Scaling](#45-the-mtsdf-binary-disk-cache--zero-cost-runtime-scaling)
	- [4.6 Text Measurement, Metrics & Dynamic Effect Spacing](#46-text-measurement-metrics--dynamic-effect-spacing)
5. [3D World-Space Refresh & View Scene Construction](#5-3d-world-space-refresh--view-scene-construction)
	- [5.1 Frame Lifecycle & View Scene Assembly (`CLG_DrawActiveViewState`)](#51-frame-lifecycle--view-scene-assembly-clg_drawactiveviewstate)
	- [5.2 View Definition (`refdef_t`) & Screen Blends](#52-view-definition-refdef_t--screen-blends)
	- [5.3 Render Entities (`entity_t`) & RenderFX Flags](#53-render-entities-entity_t--renderfx-flags)
	- [5.4 Dynamic Lighting: Spherical, Spot & Texture Emission Gobos](#54-dynamic-lighting-spherical-spot--texture-emission-gobos)
	- [5.5 Particle Systems & Atmospheric Effects](#55-particle-systems--atmospheric-effects)
	- [5.6 Skeletal Mesh API (IQM), Bone Hierarchies & Pose Blending](#56-skeletal-mesh-api-iqm-bone-hierarchies--pose-blending)
	- [5.7 Decal Subsystem & Pre-Clipped Meshes](#57-decal-subsystem--pre-clipped-meshes)
6. [3D World-Space Text & Primitives: Occluded vs Non-Occluded](#6-3d-world-space-text--primitives-occluded-vs-non-occluded)
	- [6.1 Dedicated 3D World Text API](#61-dedicated-3d-world-text-api)
	- [6.2 Billboard Projection vs Fixed Surface Orientation](#62-billboard-projection-vs-fixed-surface-orientation)
	- [6.3 Occluded vs Non-Occluded Rendering Mechanics](#63-occluded-vs-non-occluded-rendering-mechanics)
	- [6.4 3D Debug Primitive Drawing](#64-3d-debug-primitive-drawing)
7. [How-To Guide: Working Examples from A to Z](#7-how-to-guide-working-examples-from-a-to-z)
	- [7.1 Example 1: Basic 2D HUD Widget with Smooth Alpha & Icons](#71-example-1-basic-2d-hud-widget-with-smooth-alpha--icons)
	- [7.2 Example 2: Modern Glassmorphic Card (Rounded Corners, Outlines & Glows)](#72-example-2-modern-glassmorphic-card-rounded-corners-outlines--glows)
	- [7.3 Example 3: MicroUI / GameUI Integration with MTSDF Typography](#73-example-3-microui--gameui-integration-with-mtsdf-typography)
	- [7.4 Example 4: 3D Camera-Facing Floating Nameplate (Non-Occluded)](#74-example-4-3d-camera-facing-floating-nameplate-non-occluded)
	- [7.5 Example 5: 3D Surface Computer Terminal (Occluded World Orientation)](#75-example-5-3d-surface-computer-terminal-occluded-world-orientation)
	- [7.6 Example 6: 3D Tactical Debug Visualization (Capsules, Boxes & Arrows)](#76-example-6-3d-tactical-debug-visualization-capsules-boxes--arrows)
	- [7.7 Example 7: Dynamic Player Flashlight with Texture Emission Gobo](#77-example-7-dynamic-player-flashlight-with-texture-emission-gobo)
8. [Comprehensive Public API Reference](#8-comprehensive-public-api-reference)
	- [8.1 Core Types & Bitflags](#81-core-types--bitflags)
	- [8.2 2D Rendering & Canvas Functions](#82-2d-rendering--canvas-functions)
	- [8.3 2D Shader Styling & Effect Functions](#83-2d-shader-styling--effect-functions)
	- [8.4 MTSDF Font & Text Functions](#84-mtsdf-font--text-functions)
	- [8.5 3D World Scene & Entity Functions](#85-3d-world-scene--entity-functions)
	- [8.6 3D Dynamic Lighting Functions](#86-3d-dynamic-lighting-functions)
	- [8.7 3D World-Space Text & Debug Primitives](#87-3d-world-space-text--debug-primitives)
	- [8.8 Skeletal Model (IQM) API](#88-skeletal-model-iqm-api)
9. [Elaborative Interrogation & Technical Deep-Dive](#9-elaborative-interrogation--technical-deep-dive)
10. [Console Variables & Diagnostic Commands](#10-console-variables--diagnostic-commands)

---

## 1. Architectural Overview & Design Principles

The rendering pipeline in **Quake II RTXPerimental** decouples game simulation from hardware graphics through an explicit client-game interface ([`clgame_import_t`](../../inc/shared/client/cl_game.h#L580-L1008)). Whether rendering in the **Vulkan Path Tracer (`VKPT`)** or legacy OpenGL (`GL`), game logic interacts with a unified API that abstracts hardware intricacies while providing advanced graphical capabilities:

```
┌────────────────────────────────────────────────────────────────────────┐
│                   ClientGame Subsystem (clgame.dll)                    │
│   HUD, Menus, Scoreboard, GameUI, Predict, Weapon Models, View Scene   │
└────────────────────────────────────┬───────────────────────────────────┘
                                     │ clgi imports
┌────────────────────────────────────▼───────────────────────────────────┐
│                    Client Engine Core (cl_*.cpp)                       │
│    Screen Management, Virtual Canvas Scaling, PVS, Media Registrars    │
└────────────────────────────────────┬───────────────────────────────────┘
                                     │ R_* function pointers
┌────────────────────────────────────▼───────────────────────────────────┐
│                     Refresh Module (refresh_*.dll)                     │
│  ┌───────────────────────────────────┐ ┌─────────────────────────────┐ │
│  │   2D StretchPic & Font Pipeline   │ │   3D World Scene Pipeline   │ │
│  │  - MTSDF Shader (stretch_pic.frag)│ │  - AS/BVH Construction      │ │
│  │  - Analytic Rounded Box SDF       │ │  - Path Tracing Integrator  │ │
│  │  - Multi-Channel Edge Filtering   │ │  - Direct Dynamic Lights    │ │
│  │  - Inset/Outset Stroke Outlines   │ │  - Skeletal Matrix SSBOs    │ │
│  │  - Hermite & Exp Outer/Inner Glow │ │  - Projected Decal Meshes   │ │
│  └───────────────────────────────────┘ └─────────────────────────────┘ │
└────────────────────────────────────────────────────────────────────────┘
```

### Core Design Principles
* **Hardware-Accelerated Vector Quality**: UI elements, text, panels, and borders are rendered analytically or via Multi-Channel Distance Fields directly inside GPU pixel shaders, remaining pin-sharp at any resolution (from 720p up to 4K/8K) without raster blur or jagged texel aliasing.
* **Separation of 2D HUD Canvas & 3D View Scene**: 2D HUD elements execute within a resolution-independent virtual canvas, while 3D entities, dynamic lights, and debug primitives are projected into the real physical scene and integrated seamlessly into path-traced lighting passes.
* **Deterministic Resource Caching**: Heavy TrueType/OpenType vector parsing and edge decomposition are executed once at reference scale and serialized to binary `.mtsdf` disk caches, ensuring map loads and UI instantiation take $O(1)$ time without runtime stalls.
* **Depth-Tested World Space UI**: In-game text and markers can effortlessly toggle between depth-occluded positioning (hidden behind walls) and always-on-top non-occluded overlays (floating overhead nameplates) with zero pipeline switching overhead.

---

## 2. The ClientGame Refresh Architecture

All interaction between the client game DLL and the engine refresh subsystem takes place through the [`clgi`](../../src/baseq2rtxp/clgame/clg_local.h#L24) structure, an instance of [`clgame_import_t`](../../inc/shared/client/cl_game.h#L580-L1008). 

At DLL initialization inside [`GetGameAPI`](../../src/baseq2rtxp/clgame/clg_main.cpp#L749), the engine populates function pointers into `clgi`. The client game can invoke these methods at any point during frame prediction, HUD drawing, or menu processing.

The client game frame renders in two primary phases:
1. **3D World Scene Pass (`CLG_DrawActiveViewState`)**: Constructs [`refdef_t`](../../inc/refresh/shared_types.h#L797-L825) containing the camera frustum, listener orientation, interpolated entity states, dynamic lights, particles, and decal meshes, and passes it to [`clgi.R_RenderFrame`](../../inc/shared/client/cl_game.h#L586).
2. **2D Canvas & HUD Pass (`SCR_Draw2D` / `CLG_UI_DrawRenderCommands`)**: After the 3D scene has rendered, the 2D orthographic canvas takes over. HUD widgets, crosshairs, popups, and MicroUI elements execute draw commands against the virtual screen bounds.

---

## 3. The 2D Canvas & Styling Subsystem

### 3.1 Virtual Resolution, Coordinates & Viewport Scissoring

The 2D canvas operates in a virtual screen coordinate space configured via `clgi.client->refdef` and `scr_vrect` ([`vrect_t`](../../inc/shared/shared_types.h)):

* **Virtual vs. Physical**: The UI canvas maintains a virtual aspect-corrected layout. Positions $(x, y)$ and dimensions $(w, h)$ passed to `R_Draw*` functions are automatically scaled by the active UI scale factor ([`R_SetScale`](../../inc/shared/client/cl_game.h#L614) / [`R_ClampScale`](../../inc/shared/client/cl_game.h#L612)).
* **Clipping & Scissoring**: Hardware scissor rectangles are set via [`clgi.R_SetClipRect`](../../inc/shared/client/cl_game.h#L610). Any command drawn while a clip rectangle is active will be strictly clamped in hardware to the specified bounds:

```cpp
clipRect_t clip;
clip.left   = panel_x;
clip.right  = panel_x + panel_w;
clip.top    = panel_y;
clip.bottom = panel_y + panel_h;

// Restrict all subsequent 2D draw calls to the panel window
clgi.R_SetClipRect( &clip );

// Draw scrollable list content...
clgi.R_DrawString( panel_x + 10, item_y, 0, MAX_STRING_CHARS, item_text, font );

// Reset clipping to full canvas
clgi.R_SetClipRect( nullptr );
```

---

### 3.2 Image Registration, Atlases & Texture Lifecycles

Textures for 2D pictures, HUD icons, and sprite particles are registered through the refresh image subsystem:

| Function | Purpose | Retention |
| :--- | :--- | :--- |
| [`clgi.R_RegisterPic( name )`](../../inc/shared/client/cl_game.h#L597) | Loads a 2D HUD or menu texture from the virtual filesystem. | Purged on map change load sequence. |
| [`clgi.R_RegisterPic2( name )`](../../inc/shared/client/cl_game.h#L599) | Loads a permanent 2D texture (e.g. console font, system icons). | Remains resident until game shutdown. |
| [`clgi.R_RegisterImage( name, type, flags )`](../../inc/shared/client/cl_game.h#L592) | Explicit image loader with [`imageflags_t`](../../inc/refresh/shared_types.h#L850) and [`imagetype_t`](../../inc/refresh/shared_types.h#L899). | Depends on `IF_PERMANENT` flag. |
| [`clgi.R_RegisterRawImage( name, w, h, pic, type, flags )`](../../inc/shared/client/cl_game.h#L716) | Uploads in-memory RGBA8/BGRA8 pixel buffers directly to GPU. | Managed by client; GPU takes ownership. |
| [`clgi.R_UpdateRawPic( w, h, pic )`](../../inc/shared/client/cl_game.h#L720) | Re-streams dynamic procedural pixels to the primary raw image. | Transient dynamic buffer. |

To inspect image dimensions dynamically:
```cpp
int32_t width = 0, height = 0;
qhandle_t icon = clgi.R_RegisterPic( "pics/i_health" );
if ( clgi.R_GetPicSize( &width, &height, icon ) ) {
	// width and height are now populated with texture dimensions
}
```

---

### 3.3 2D Primitives: Pictures, Fills, Lines & Polygons

The refresh subsystem provides both basic and extended 2D drawing primitives:

#### 1. Image Rendering
* [`clgi.R_DrawPic( x, y, pic )`](../../inc/shared/client/cl_game.h#L680): Draws the picture at 1:1 pixel scale.
* [`clgi.R_DrawStretchPic( x, y, w, h, pic )`](../../inc/shared/client/cl_game.h#L685): Scales the picture to fill $(w, h)$.
* [`clgi.R_DrawPicEx( destX, destY, destW, destH, pic, srcX, srcY, srcW, srcH )`](../../inc/shared/client/cl_game.h#L682): Crops a sub-rectangle `(srcX, srcY, srcW, srcH)` from an atlas texture and renders it into destination coordinates `(destX, destY, destW, destH)`. Essential for texture atlases and UI sprite sheets.
* [`clgi.R_DrawRotateStretchPic( x, y, w, h, angle, pivot_x, pivot_y, pic )`](../../inc/shared/client/cl_game.h#L687): Rotates the quad around an arbitrary pivot point `(pivot_x, pivot_y)` by `angle` degrees.
* [`clgi.R_DrawKeepAspectPic( x, y, w, h, pic )`](../../inc/shared/client/cl_game.h#L689): Scales the image to fit into bounding box $(w, h)$ while strictly preserving its original aspect ratio.

#### 2. Colored Fills
* [`clgi.R_DrawFill32( x, y, w, h, color )`](../../inc/shared/client/cl_game.h#L697) & [`clgi.R_DrawFill32f( x, y, w, h, color )`](../../inc/shared/client/cl_game.h#L701): Fills a rectangular region with packed 32-bit `RGBA` (integer or sub-pixel float coordinates).
* [`clgi.R_DrawFill8( x, y, w, h, c )`](../../inc/shared/client/cl_game.h#L695): Fills using an 8-bit Quake palette color index converted via the internal palette table.

#### 3. Analytical 2D Lines
* [`clgi.R_DrawLine2D( x1, y1, x2, y2, thickness, color )`](../../inc/shared/client/cl_game.h#L654): Renders an analytical sub-pixel anti-aliased 2D line segment between $(x_1, y_1)$ and $(x_2, y_2)$ with rounded capsule end-caps.

---

### 3.4 Modern GPU Styling Pipeline: Strokes, Glows & Rounded Corners

A standout feature of Q2RTXPerimental is the **GPU StretchPic Styling Pipeline** ([`stretch_pic.frag`](../../src/refresh/vkpt/shader/stretch_pic.frag)). Rather than issuing dozens of textured quads or tessellating complex geometry for rounded corners, borders, and shadows, the refresh pipeline evaluates analytical Signed Distance Fields (SDF) inside the fragment shader:

```
┌────────────────────────────────────────────────────────────────────────┐
│                        GPU Fragment Evaluation                         │
│                                                                        │
│               q = p - (W / 2)       [Local Centered Point]             │
│               r = corner_radii[quadrant]                               │
│               d = length(max(abs(q) - (W / 2) + r, 0)) - r             │
│                                                                        │
│   Outer Glow Falloff:                                                  │
│     Hermite: 1.0 - smoothstep(0.0, 1.0, -d / radius)                   │
│     Exp:     exp(-3.0 * (-d / radius))                                 │
│                                                                        │
│   Stroke Outlines:                                                     │
│     Centered:  abs(d) - (thickness / 2)                                │
│     Outset:    abs(d - (thickness / 2)) - (thickness / 2)              │
│     Inset:     abs(d + (thickness / 2)) - (thickness / 2)              │
└────────────────────────────────────────────────────────────────────────┘
```

#### Styling Control Flow
Before issuing any 2D draw call (`R_DrawFill32`, `R_DrawStretchPic`, `R_DrawString`), activate styling parameters on the renderer state:

```cpp
// 1. Configure 8px rounded corners on top-left and top-right, 0px on bottom
clgi.R_SetCornerRadius4( 8.0f, 8.0f, 0.0f, 0.0f );

// 2. Configure a 2px crisp white outline stroke
clgi.R_SetStroke( MakeColor( 255, 255, 255, 255 ), 2.0f );

// 3. Configure a soft cyan outer glow / drop shadow (12px radius)
clgi.R_SetOuterGlow( MakeColor( 0, 200, 255, 180 ), 12.0f );

// 4. Render the styled card panel
clgi.R_DrawFill32( 100, 100, 300, 180, MakeColor( 20, 25, 35, 230 ) );

// 5. CRITICAL: Always clear styling state so subsequent primitives render clean
clgi.R_ClearStyle();
```

#### Extended Styling Flags ([`refresh.h`](../../inc/refresh/refresh.h#L78-L108))

* **Stroke Alignment**:
  * `STYLE_FLAG_STROKE_ALIGN_CENTER` (Default): Stroke expands symmetrically $[-W/2, +W/2]$ across the geometry contour.
  * `STYLE_FLAG_STROKE_ALIGN_INSET`: Stroke expands strictly inward $[-W, 0]$, preserving outer dimensions.
  * `STYLE_FLAG_STROKE_ALIGN_OUTSET`: Stroke expands strictly outward $[0, +W]$, preserving inner area.
* **Glow Falloff & Blending**:
  * `STYLE_FLAG_GLOW_FALLOFF_HERMITE` (Default): Smooth cubic Hermite curve ($1 - \text{smoothstep}$).
  * `STYLE_FLAG_GLOW_FALLOFF_EXP`: Exponential neon decay curve ($e^{-3t}$), perfect for sci-fi HUDs.
  * `STYLE_FLAG_GLOW_BLEND_ADDITIVE`: Additive HDR blending for bright emissive bloom.
* **Independent Edge Masks**:
  * `STYLE_FLAG_EDGE_TOP`, `STYLE_FLAG_EDGE_RIGHT`, `STYLE_FLAG_EDGE_BOTTOM`, `STYLE_FLAG_EDGE_LEFT`, `STYLE_FLAG_EDGE_ALL`. Allows selectively glowing only top/bottom tabs or side indicators!

---

## 4. TrueType / OpenType & The MTSDF Font Engine

### 4.1 The Fundamental Flaw of Single-Channel Signed Distance Fields (SDF)

Traditional Signed Distance Field (SDF) font rendering computes the Euclidean distance from each texel in an atlas to the nearest point on any glyph contour:

$$d(p) = \min_{q \in \partial \Omega} \|p - q\| \times \text{sgn}(p)$$

While standard single-channel SDF solves linear edge scaling and antialiasing, it exhibits a catastrophic mathematical flaw: **gradient ambiguity at sharp corners**. 

At an acute corner (such as the tip of an 'A', 'M', 'N', 'V', or 'Z'), the vector distance field forms a non-differentiable ridge. Because bilinear texture filtering on the GPU averages surrounding scalar values across the corner, the reconstructed $0.5$ contour becomes circular. At magnified scales, sharp corners round off into blurry, balloon-like arcs.

---

### 4.2 Multi-Channel Signed Distance Fields (MSDF) & Median Evaluation

Multi-Channel Signed Distance Fields (**MSDF**) solve corner rounding by decomposing glyph contours into separate segments and storing distances across three independent color channels: **Red, Green, and Blue**.

When a contour turns a sharp corner (detected via tangent dot product $\le \cos 3^\circ$), the coloring algorithm changes color flags between adjacent segments using a 3-color palette:
* `MTSDF_COLOR_YELLOW` = Red + Green
* `MTSDF_COLOR_CYAN` = Green + Blue
* `MTSDF_COLOR_MAGENTA` = Red + Blue
* `MTSDF_COLOR_WHITE` = Red + Green + Blue (used on smooth continuous contours)

At render time, the GPU evaluates the distance using the **median** of the three channels:

$$\text{sd} = \text{median}(R, G, B) = \max(\min(R, G), \min(\max(R, G), B))$$

Because the intersecting channels create independent half-planes, the median operator reconstructs sharp convex and concave corners with mathematical precision, preserving crisp vertices at infinite magnification.

---

### 4.3 Why MTSDF: Multi-Channel True-Distance Signed Distance Fields with Alpha

While 3-channel MSDF provides razor-sharp corners, it creates a new challenge: **it cannot produce smooth isotropic outer glows, drop shadows, or wide outlines**.

Because each color channel measures distance only to its *assigned* edge subset, distances sampled far from the contour break down into directional artifacts, resulting in noticeable radial creases and corner distortions when evaluating glows or soft shadows.

To resolve this, Q2RTXPerimental implements **MTSDF** (Multi-channel True-distance Signed Distance Field) ([`fonts_mtsdf.cpp`](../../src/refresh/fonts_mtsdf.cpp)):

```
┌────────────────────────────────────────────────────────────────────────┐
│                        MTSDF 4-Channel Texel                           │
│                                                                        │
│   ┌─────────────────────────────┐  ┌────────────────────────────────┐  │
│   │     RGB Channels (MSDF)     │  │      Alpha Channel (SDF)       │  │
│   │   - Yellow, Cyan, Magenta   │  │   - True Euclidean Distance    │  │
│   │   - Multi-channel partitions│  │   - Universal to all contours  │  │
│   │   - Sharp corner median     │  │   - Isotropic outer glows      │  │
│   │   - Infinite scale edge     │  │   - Soft drop shadows & halos  │  │
│   └──────────────┬──────────────┘  └───────────────┬────────────────┘  │
│                  │                                 │                   │
│                  ▼                                 ▼                   │
│         Razor-Sharp Glyphs              Smooth Omnidirectional Glows   │
└────────────────────────────────────────────────────────────────────────┘
```

In the fragment shader ([`stretch_pic.frag#L148-L156`](../../src/refresh/vkpt/shader/stretch_pic.frag#L148-L156)):
```glsl
// Median of RGB channels for razor-sharp corners:
sd = max( min( samp.r, samp.g ), min( max( samp.r, samp.g ), samp.b ) );

// True isotropic Euclidean distance in alpha channel for glows and shadows:
sd_glow = samp.a;
```

---

### 4.4 Technical Operation: Vector Decomposition, Edge Coloring & Atlas Packing

The compilation pipeline inside [`fonts_mtsdf.cpp`](../../src/refresh/fonts_mtsdf.cpp) executes automatically when loading any `.ttf` or `.otf` font:

1. **Vector Parsing**: Loads the font binary via [`stb_truetype`](../../src/refresh/fonts_mtsdf.cpp#L35) and queries vector outlines (`stbtt_GetGlyphShape`).
2. **Contour Extraction**: Segments outline curves into linear segments (`EDGE_LINE`) and quadratic Bézier curves (`EDGE_QUADRATIC`).
3. **Corner Detection & Edge Coloring**:
   * Evaluates end-tangents of segment $i-1$ against start-tangents of segment $i$.
   * Vertices with $\vec{t}_{\text{prev}} \cdot \vec{t}_{\text{cur}} \le \cos(3^\circ)$ or $|\vec{t}_{\text{prev}} \times \vec{t}_{\text{cur}}| > 0.5735$ are classified as sharp corners.
   * Partitions the contour into segments and assigns alternating color flags (`YELLOW`, `CYAN`, `MAGENTA`).
4. **Subpixel Distance Evaluation**:
   * Computes the unsigned Euclidean distance to linear segments and quadratic curves.
   * Evaluates winding numbers (`stbtt__compute_crossings_x`) to establish interior vs exterior polarity.
   * Maps signed distance $sd$ to normalized byte range:
     $$\text{norm} = \text{clamp}\left(0.5 + \frac{sd}{2 \times \text{pixel\_range}}, 0.0, 1.0\right)$$
   * Writes multi-channel distances to `R`, `G`, `B` and global minimum Euclidean distance to `A`.
5. **Atlas Packing**: Uses [`stb_rect_pack`](../../src/refresh/fonts_mtsdf.cpp#L29) to tightly pack 256 glyph bitmaps into an optimal power-of-two texture (progressively doubling from $512 \times 512$ up to $2048 \times 2048$).
6. **Master Texture Registration**: Registers the RGBA8 buffer with `IF_PERMANENT | IF_SDF_SILHOUETTE`.

---

### 4.5 The `.mtsdf` Binary Disk Cache & Zero-Cost Runtime Scaling

Generating distance fields texel-by-texel from Bézier curves requires significant CPU floating-point operations. Doing this on every map load or client launch would introduce unacceptable stutter.

#### Disk Serialization
To eliminate runtime cost, [`Font_SaveMasterMTSDF`](../../src/refresh/fonts_mtsdf.cpp#L678) automatically serializes the completed master atlas directly to disk as `<stem>.mtsdf`:

```
┌────────────────────────────────────────────────────────────────────────┐
│                        .mtsdf Disk File Layout                         │
├──────────────────────────┬─────────────────────────────────────────────┤
│ Header (64 bytes)        │ magic ('MTSD' = 0x4D545344), version (1),   │
│                          │ original path, pixel height, atlas W/H,     │
│                          │ ascent, descent, line gap, sdf pixel range  │
├──────────────────────────┼─────────────────────────────────────────────┤
│ Reference Glyph Table    │ font_glyph_mtsdf_t ref_glyphs[ 256 ]        │
│ (8,192 bytes)            │ Advance, bearings, geometry size, UV coords │
├──────────────────────────┼─────────────────────────────────────────────┤
│ Glyph Validity Flags     │ bool glyph_valid[ 256 ]                     │
├──────────────────────────┼─────────────────────────────────────────────┤
│ Atlas Pixels (Variable)  │ Raw RGBA8 pixel payload (W * H * 4 bytes)   │
└──────────────────────────┴─────────────────────────────────────────────┘
```

When [`clgi.R_RegisterFontTTF("fonts/segoeui.ttf", size)`](../../inc/shared/client/cl_game.h#L656) is called:
1. It checks whether `fonts/segoeui.mtsdf` exists on disk.
2. If found, [`Font_LoadMasterMTSDF`](../../src/refresh/fonts_mtsdf.cpp#L734) loads the binary cache in a single fast filesystem read, transfers the RGBA8 bytes directly to the GPU, and bypasses vector compilation entirely!
3. If not found, it compiles from the `.ttf` file once, saves the `.mtsdf` file to disk for future runs, and uploads to the GPU.

#### Zero-Cost Runtime Scaling
Fonts are generated once at a high-fidelity reference height ([`MTSDF_REFERENCE_HEIGHT = 48.0f`](../../inc/refresh/fonts_mtsdf.h#L36)). 

When the game requests different font sizes (e.g. 14px for HUD, 24px for menus, 36px for titles), the engine **does not compile new textures**. Instead, [`Font_CreateInstance`](../../src/refresh/fonts_mtsdf.cpp#L1141) creates a lightweight descriptor that shares the exact same GPU master texture atlas, scaling only the layout metrics:

$$\text{scale} = \frac{\text{nominal\_height}}{\text{master.ref\_pixel\_height}}$$

Multiple font instances consume **zero additional GPU VRAM** and provide instantaneous instantiation!

---

### 4.6 Text Measurement, Metrics & Dynamic Effect Spacing

Text measurement functions calculate precise pixel layouts using TrueType vector metrics:

* [`clgi.Font_StringWidthTTF( font, text )`](../../inc/shared/client/cl_game.h#L658): Measures total horizontal width in pixels.
* [`clgi.Font_StringWidthTTF_N( font, text, maxlen )`](../../inc/shared/client/cl_game.h#L660): Measures width up to `maxlen` characters.
* [`clgi.Font_GetHeightTTF( font )`](../../inc/shared/client/cl_game.h#L666): Returns line height $(\text{ascent} - \text{descent} + \text{line\_gap})$.

#### Dynamic Effect Spacing
When text is styled with thick outline strokes or heavy outer glows, the effects expand outward horizontally from each character's contour. Without adjustment, neighboring characters would visually collide and blur together.

[`R_Font_CalculateEffectSpacing`](../../src/refresh/fonts_mtsdf.cpp#L1309) analytically determines the outward intrusion:

$$\Delta w = \max\left(\text{stroke\_expansion}, \text{glow\_expansion}\right)$$

This spacing additive is automatically fetched via [`clgi.R_GetFontEffectSpacing()`](../../inc/shared/client/cl_game.h#L664) and applied per character advance in `Font_StringWidthTTF_N` and `R_DrawString`, ensuring perfectly kerned text regardless of active outlines or glow halos.

---

## 5. 3D World-Space Refresh & View Scene Construction

### 5.1 Frame Lifecycle & View Scene Assembly (`CLG_DrawActiveViewState`)

The client game drives the 3D scene every frame inside [`CLG_DrawActiveViewState`](../../src/baseq2rtxp/clgame/clg_view.cpp#L761):

```
┌────────────────────────────────────────────────────────────────────────┐
│                        3D Frame Pipeline Steps                         │
│                                                                        │
│  1. CLG_ClearViewScene()        Reset entities, dlights, particles     │
│  2. CLG_CalculateViewValues()   Eye position, kick angles, weapon swing│
│  3. CLG_FinishViewValues()      Select 1st person or 3rd person camera │
│  4. clgi.V_CalculateLocalPVS()  Query BSP PVS for view origin cluster  │
│  5. CLG_PrepareViewEntities()   Populate entities from server/predict  │
│  6. Add Dynamic Lights          Sphere lights, spot lights, profiles   │
│  7. qsort(entities)             Sort by model and skin for GPU cache   │
│  8. clgi.R_RenderFrame(&refdef) Submit frame to RTX/GL renderer        │
└────────────────────────────────────────────────────────────────────────┘
```

---

### 5.2 View Definition (`refdef_t`) & Screen Blends

The [`refdef_t`](../../inc/refresh/shared_types.h#L797-L825) structure defines the camera and scene context submitted to [`clgi.R_RenderFrame`](../../inc/shared/client/cl_game.h#L586):

```cpp
refdef_t *rd = &clgi.client->refdef;

// Camera Transform
VectorCopy( eye_origin, rd->vieworg );
VectorCopy( eye_angles, rd->viewangles );

// Viewport Geometry
rd->x      = scr_vrect->x;
rd->y      = scr_vrect->y;
rd->width  = scr_vrect->width;
rd->height = scr_vrect->height;
rd->fov_x  = calculated_fov_x;
rd->fov_y  = calculated_fov_y;
rd->time   = clgi.client->time * 0.001f;

// Screen Tint Blend (Damage flashes, underwater, powerups)
rd->screen_blend[0] = blend_r; // 0.0 to 1.0
rd->screen_blend[1] = blend_g;
rd->screen_blend[2] = blend_b;
rd->screen_blend[3] = blend_a;

// Scene Elements
rd->num_entities   = clgi.client->viewScene.r_numentities;
rd->entities       = clgi.client->viewScene.r_entities;
rd->num_dlights    = clgi.client->viewScene.r_numdlights;
rd->dlights        = clgi.client->viewScene.r_dlights;
rd->num_particles  = clgi.client->viewScene.r_numparticles;
rd->particles      = clgi.client->viewScene.r_particles;
rd->lightstyles    = clgi.client->viewScene.r_lightstyles;

// View Definition Flags (RDF_UNDERWATER, RDF_IRGOGGLES, RDF_NOWORLDMODEL)
rd->rdflags = CLG_GetViewRenderDefinitionFlags();
```

---

### 5.3 Render Entities (`entity_t`) & RenderFX Flags

Every 3D object drawn in the world (monsters, players, weapons, items, doors, projectiles) is submitted as an [`entity_t`](../../inc/refresh/shared_types.h#L553-L628) via [`clgi.V_AddEntity`](../../inc/shared/client/cl_game.h#L868):

```cpp
entity_t ent{};
ent.model     = clgi.R_RegisterModel( "models/monsters/soldier/tris.iqm" );
ent.skin      = clgi.R_RegisterSkin( "models/monsters/soldier/soldier.png" );
VectorCopy( current_origin, ent.origin );
VectorCopy( previous_origin, ent.oldorigin );
VectorCopy( current_angles, ent.angles );
ent.frame     = current_frame;
ent.oldframe  = previous_frame;
ent.backlerp  = interpolation_backlerp; // 0.0 = current, 1.0 = old
ent.flags     = RF_NONE;

clgi.V_AddEntity( &ent );
```

#### RenderFX Flags ([`entity_renderflags.h`](../../inc/shared/entities/entity_renderflags.h))
* `RF_MINLIGHT`: Ensures model maintains minimum ambient lighting (crucial for viewmodel weapons in dark corridors).
* `RF_WEAPONMODEL`: Special camera projection for first-person view weapons.
* `RF_VIEWERMODEL`: Prevents rendering when viewed from own eyes; only visible in reflective mirrors.
* `RF_FULLBRIGHT`: Bypasses lighting attenuation, rendering at maximum intensity.
* `RF_DEPTHHACK`: Compresses Z depth range to guarantee the model draws in front of world geometry without clipping into walls.
* `RF_TRANSLUCENT`: Enables alpha blending using `ent.alpha`.
* `RF_NOSHADOW`: Prevents the entity from casting ray-traced shadows in Vulkan Path Tracing.
* `RF_BEAM`: Renders an analytical laser beam from `ent.origin` to `ent.oldorigin` with diameter `ent.frame`.
* `RF_GLOW`: Adds pulsating emissive illumination (bonus items, powerups).
* `RF_SHELL_RED` / `GREEN` / `BLUE`: Renders energy armor shell effects around the model.

---

### 5.4 Dynamic Lighting: Spherical, Spot & Texture Emission Gobos

The refresh engine features full path-traced analytical dynamic lighting ([`clgi.V_Add*Light*`](../../inc/shared/client/cl_game.h#L874-L888)):

#### 1. Spherical Point Light
```cpp
// Add a spherical warm fire light (intensity 250, radius 16 units)
clgi.V_AddSphereLight( fire_origin, 250.0f, 1.0f, 0.6f, 0.2f, 16.0f );
```

#### 2. Analytical Spotlight
```cpp
// Add a spotlight with cone width 60 degrees and falloff angle 45 degrees
clgi.V_AddSpotLight(
	camera_origin, camera_forward,
	400.0f,               // Intensity
	1.0f, 1.0f, 1.0f,     // Color (RGB)
	60.0f,                // Total cone width (degrees)
	45.0f                 // Inner falloff start angle (degrees)
);
```

#### 3. Textured Spotlight (Flashlight Gobo / Emission Profile)
```cpp
// Register an emission cookie/gobo texture
qhandle_t flashlight_gobo = clgi.R_RegisterPic( "textures/flashlight_cookie" );

// Projects textured flashlight beam directly into RTX scene
clgi.V_AddSpotLightTexEmission(
	flashlight_pos, flashlight_dir,
	clg_flashlight_intensity->value,
	1.0f, 1.0f, 1.0f,     // White tint
	75.0f,                // Cone angle in degrees
	flashlight_gobo       // Projection profile texture
);
```

---

### 5.5 Particle Systems & Atmospheric Effects

Particles are submitted directly to the view scene each frame via [`clgi.V_AddParticle`](../../inc/shared/client/cl_game.h#L872):

```cpp
particle_t p{};
VectorCopy( spark_origin, p.origin );
p.color      = -1; // -1 tells renderer to use explicit RGBA color below
p.rgba       = MakeColor( 255, 180, 50, 255 );
p.alpha      = current_alpha;
p.radius     = 2.5f;
p.brightness = 4.0f; // Emissive boost for RTX path-traced bloom

clgi.V_AddParticle( &p );
```

---

### 5.6 Skeletal Mesh API (IQM), Bone Hierarchies & Pose Blending

Q2RTXPerimental supports modern skeletal models via Inter-Quake Model (`.iqm`) files. The client game includes a comprehensive skeletal animation API ([`clgi.SKM_*`](../../inc/shared/client/cl_game.h#L761-L818)):

* **Bone Queries**: [`clgi.SKM_GetBoneByName`](../../inc/shared/client/cl_game.h#L763) and [`clgi.SKM_GetBoneByNumber`](../../inc/shared/client/cl_game.h#L767).
* **Bone Cache Allocator**: [`clgi.SKM_PoseCache_AcquireCachedMemoryBlock(size)`](../../inc/shared/client/cl_game.h#L782) acquires pre-allocated memory from the high-performance scratch ring buffer.
* **Frame Pose Extraction**: [`clgi.SKM_GetBonePosesForFrame(model, frame)`](../../inc/shared/client/cl_game.h#L790).
* **Pose Interpolation**: [`clgi.SKM_LerpBonePoses`](../../inc/shared/client/cl_game.h#L795) and [`clgi.SKM_ComputeLerpBonePoses`](../../inc/shared/client/cl_game.h#L800) interpolate rotations via SLERP and translations linearly between animation frames.
* **Recursive Branch Blending**: [`clgi.SKM_RecursiveBlendFromBone`](../../inc/shared/client/cl_game.h#L808) blends an animation layer on top of a base animation starting from an arbitrary bone node (e.g. blending an upper-body aiming/firing animation while the lower body runs or jumps).
* **Matrix Space Conversions**:
  * [`clgi.SKM_TransformBonePosesLocalSpace`](../../inc/shared/client/cl_game.h#L812): Computes model-space matrices with optional procedural bone controllers (head tracking, spine aiming).
  * [`clgi.SKM_TransformBonePosesWorldSpace`](../../inc/shared/client/cl_game.h#L818): Evaluates full world-space positions for muzzle flashes, attachment points, and particle emitters.

---

### 5.7 Decal Subsystem & Pre-Clipped Meshes

Decals (bullet impacts, blood splatters, plasma burns) are submitted to the renderer through pre-clipped triangle meshes ([`clgi.R_AddDecalMesh`](../../inc/shared/client/cl_game.h#L727)):

```cpp
// Submit pre-clipped decal triangles
clgi.R_AddDecalMesh(
	vertices.data(),      // decal_mesh_vertex_t array (position, normal, uv)
	vertices.size(),      // Vertex count (multiple of 3)
	albedo_tint,          // RGB albedo tint
	current_alpha,        // Fade alpha
	material_hash,        // Hash identifier matching material name
	remaining_life_sec    // Lifetime remaining in seconds
);
```

---

## 6. 3D World-Space Text & Primitives: Occluded vs Non-Occluded

### 6.1 Dedicated 3D World Text API

In traditional engines, rendering 3D text in the world requires manually projecting 3D points to 2D screen coordinates via matrix multiplication, estimating screen scale, checking bounding frustums, and drawing in 2D.

In Q2RTXPerimental, **3D world-space text is a native first-class citizen of the refresh pipeline**:

| Function | Depth Testing Behavior | Typical Usage |
| :--- | :--- | :--- |
| [`clgi.R_DrawString3DOccluded`](../../inc/shared/client/cl_game.h#L668) | **Enabled** (`STYLE_FLAG_DEPTH_TEST`). Occluded by walls, pillars, entities, and geometry. | Wall computer displays, room signage, objective labels painted on floors, world puzzles. |
| [`clgi.R_DrawString3DNonOccluded`](../../inc/shared/client/cl_game.h#L670) | **Disabled**. Always rendered on top of geometry. | Player overhead nameplates, tactical teammate pings, navigation breadcrumbs, debug metrics. |
| [`clgi.R_DrawString3D`](../../inc/shared/client/cl_game.h#L672) | **Enabled** (Alias for `R_DrawString3DOccluded`). | Standard world-space text. |

```cpp
void R_DrawString3DOccluded(
	const vec3_t origin,  // World position (X, Y, Z)
	const vec3_t angles,  // Orientation (Pitch, Yaw, Roll), or NULL for camera billboard
	const float  scale,   // Character height in Quake world units
	const char  *text,    // Text string content
	const qhandle_t font, // MTSDF or bitmap font handle
	const uint32_t color  // Packed RGBA color tint
);
```

---

### 6.2 Billboard Projection vs Fixed Surface Orientation

The `angles` parameter controls whether text dynamically faces the player or attaches rigidly to world geometry:

#### 1. Camera-Facing Billboard Mode (`angles == nullptr`)
When `angles` is null, the renderer calculates an orthonormal basis facing directly toward the camera eye:
$$\vec{f}_{\text{billboard}} = \text{normalize}(\text{origin} - \text{vieworg})$$
$$\vec{r} = \text{normalize}(\vec{u}_{\text{world}} \times \vec{f}_{\text{billboard}})$$
$$\vec{u} = \vec{f}_{\text{billboard}} \times \vec{r}$$
Text automatically turns to face the spectator or player regardless of viewing angle.

#### 2. Rigid Surface Orientation (`angles != nullptr`)
When `angles` is provided, the renderer evaluates Euler angles into forward, right, and up vectors (`AngleVectors`). Text adheres flat against map brushes, computer screens, signs, or entity doors.

---

### 6.3 Occluded vs Non-Occluded Rendering Mechanics

Inside [`vkpt/draw.c#DrawString3D_Internal`](../../src/refresh/vkpt/draw.c#L1988), each 3D character quad is transformed into world space by concatenating its model matrix with the camera View-Projection matrix:

$$\text{matTransform} = \text{VP} \times \text{matCharWorld}$$

The difference between occluded and non-occluded modes comes down to **scene depth buffer testing**:

* **Occluded (`STYLE_FLAG_DEPTH_TEST` set)**:
  The fragment shader samples the Vulkan Path Tracer scene depth buffer (`TEX_PT_VIEW_DEPTH_A`). If `view_depth > sampled_depth`, the text pixel is occluded by the world and discarded.
* **Non-Occluded (`STYLE_FLAG_DEPTH_TEST` cleared)**:
  Depth testing is bypassed. The character quads composite directly over the final image, ensuring tactical markers and player nameplates remain visible through solid geometry.

---

### 6.4 3D Debug Primitive Drawing

The client game has direct access to high-performance 3D debug drawing primitives ([`clgi.R_DrawDebug*`](../../inc/shared/client/cl_game.h#L703-L713)):

```cpp
// Box AABB
clgi.R_DrawDebugBox( mins, maxs, color, thickness_px, outline_thickness_px, style_flags );

// Line Segment
clgi.R_DrawDebugLine( start, end, color, thickness_px, outline_thickness_px, style_flags );

// Directional Arrow
clgi.R_DrawDebugArrow( start, end, head_len, color, thickness_px, outline_thickness_px, style_flags );

// Sphere
clgi.R_DrawDebugSphere( center, radius, color, thickness_px, outline_thickness_px, style_flags );

// Capsule (cylinder with hemispherical caps)
clgi.R_DrawDebugCapsule( start, end, radius, color, thickness_px, outline_thickness_px, style_flags );

// Cylinder
clgi.R_DrawDebugCylinder( start, end, radius, color, thickness_px, outline_thickness_px, style_flags );
```

#### Debug Style Flags ([`debug_draw.h`](../../src/refresh/vkpt/debug/debug_draw.h#L34-L41))
* `VKPT_DEBUG_DRAW_FLAG_NONE` (0): Non-occluded (always visible on top).
* `VKPT_DEBUG_DRAW_FLAG_DEPTH_TEST` (`1 << 0`): Depth tested against world geometry (occluded).
* `VKPT_DEBUG_DRAW_FLAG_OUTLINE` (`1 << 1`): Renders a contrasting dark outline around the primitive so it remains readable against complex path-traced lighting.

---

## 7. How-To Guide: Working Examples from A to Z

### 7.1 Example 1: Basic 2D HUD Widget with Smooth Alpha & Icons

A complete implementation of a health counter with an animated health icon and smooth alpha fading:

```cpp
void CLG_HUD_DrawHealthWidget( const int32_t x, const int32_t y, const int32_t health, const float alpha ) {
	if ( alpha <= 0.0f ) {
		return;
	}

	// Set active alpha for all subsequent 2D draw calls
	clgi.R_SetAlpha( alpha );

	// 1. Draw subtle dark background panel
	clgi.R_DrawFill32( x, y, 120, 36, MakeColor( 10, 15, 20, 160 ) );

	// 2. Draw health icon
	qhandle_t health_pic = clgi.R_RegisterPic( "pics/i_health" );
	clgi.R_DrawStretchPic( x + 6, y + 6, 24, 24, health_pic );

	// 3. Select color tint based on health threshold
	uint32_t text_color;
	if ( health > 50 ) {
		text_color = MakeColor( 255, 255, 255, 255 ); // White
	} else if ( health > 20 ) {
		text_color = MakeColor( 255, 200, 50, 255 );  // Yellow
	} else {
		text_color = MakeColor( 255, 50, 50, 255 );    // Critical Red
	}

	clgi.R_SetColor( text_color );

	// 4. Format and draw text using the screen MTSDF font
	char buffer[ 16 ];
	Q_snprintf( buffer, sizeof( buffer ), "%d", health );
	clgi.R_DrawString( x + 38, y + 8, UI_LEFT, sizeof( buffer ), buffer, precache.screen.font_pic );

	// 5. Cleanup renderer state
	clgi.R_ClearColor();
	clgi.R_SetAlpha( 1.0f );
}
```

---

### 7.2 Example 2: Modern Glassmorphic Card (Rounded Corners, Outlines & Glows)

Construct a sleek cyberpunk/sci-fi glassmorphic menu container:

```cpp
void CLG_UI_DrawGlassmorphicCard( const float x, const float y, const float w, const float h ) {
	// 1. Configure 12px rounded corners on all four quadrants
	clgi.R_SetCornerRadius( 12.0f );

	// 2. Configure a 1.5px subtle cyan outline stroke
	clgi.R_SetStroke( MakeColor( 80, 180, 255, 140 ), 1.5f );

	// 3. Configure exponential outer glow for an electric neon aura
	clgi.R_SetOuterGlowEx(
		MakeColor( 0, 140, 255, 120 ),
		16.0f,
		STYLE_FLAG_GLOW_FALLOFF_EXP | STYLE_FLAG_GLOW_BLEND_ADDITIVE
	);

	// 4. Configure subtle inner contour glow for edge glass refraction
	clgi.R_SetInnerGlow( MakeColor( 120, 200, 255, 45 ), 6.0f );

	// 5. Draw the semi-translucent dark glass card fill
	clgi.R_DrawFill32f( x, y, w, h, MakeColor( 15, 20, 30, 210 ) );

	// 6. Reset styling state
	clgi.R_ClearStyle();
}
```

---

### 7.3 Example 3: MicroUI / GameUI Integration with MTSDF Typography

Demonstrates binding TrueType MTSDF metric callbacks and executing draw commands ([`clg_ui_main.cpp#L534`](../../src/baseq2rtxp/clgame/game_ui/clg_ui_main.cpp#L534)):

```cpp
// 1. Register TrueType font at nominal size 16px
qhandle_t ui_font = clgi.R_RegisterFontTTF( "fonts/segoeui.ttf", 16.0f );

// 2. MicroUI text measurement callbacks
static int UI_TextWidthCallback( mu_Font font, const char *text, int len ) {
	if ( !text || !text[ 0 ] || ui_font == 0 ) return 0;
	if ( len == -1 ) {
		return Q_rint( clgi.Font_StringWidthTTF( ui_font, text ) );
	}
	return Q_rint( clgi.Font_StringWidthTTF_N( ui_font, text, (size_t)len ) );
}

static int UI_TextHeightCallback( mu_Font font ) {
	return Q_rint( clgi.Font_GetHeightTTF( ui_font ) );
}

// 3. Dispatch MicroUI commands in render loop
void CLG_UI_RenderMicroUI( mu_Context *ctx ) {
	mu_Command *cmd = nullptr;
	while ( mu_next_command( ctx, &cmd ) ) {
		switch ( cmd->type ) {
			case MU_COMMAND_TEXT: {
				clgi.R_SetColor( MakeColor( cmd->text.color.r, cmd->text.color.g, cmd->text.color.b, cmd->text.color.a ) );
				clgi.R_DrawString( cmd->text.pos.x, cmd->text.pos.y, 0, MAX_STRING_CHARS, cmd->text.str, ui_font );
				clgi.R_ClearColor();
				break;
			}
			case MU_COMMAND_RECT: {
				clgi.R_DrawFill32( cmd->rect.rect.x, cmd->rect.rect.y, cmd->rect.rect.w, cmd->rect.rect.h,
					MakeColor( cmd->rect.color.r, cmd->rect.color.g, cmd->rect.color.b, cmd->rect.color.a ) );
				break;
			}
			case MU_COMMAND_CLIP: {
				const clipRect_t clip = {
					cmd->clip.rect.x, cmd->clip.rect.x + cmd->clip.rect.w,
					cmd->clip.rect.y, cmd->clip.rect.y + cmd->clip.rect.h
				};
				clgi.R_SetClipRect( &clip );
				break;
			}
		}
	}
	clgi.R_SetClipRect( nullptr );
}
```

---

### 7.4 Example 4: 3D Camera-Facing Floating Nameplate (Non-Occluded)

Renders a teammate nameplate and health bar above a player model, visible through walls:

```cpp
void CLG_DrawTeammateNameplate( const centity_t *cent, const char *playerName, const int32_t healthPercent ) {
	// Position 42 units above entity head origin
	vec3_t text_pos;
	VectorCopy( cent->current.origin, text_pos );
	text_pos[ 2 ] += 42.0f;

	// 1. Draw camera-facing billboard text (always on top via NonOccluded)
	// Character height = 6.0 Quake units
	clgi.R_DrawString3DNonOccluded(
		text_pos,
		nullptr, // NULL angles = automatic camera billboard
		6.0f,
		playerName,
		precache.screen.font_pic,
		MakeColor( 100, 220, 255, 240 ) // Team Cyan
	);

	// 2. Draw 3D debug health indicator bar below nameplate
	vec3_t bar_start, bar_end;
	VectorCopy( text_pos, bar_start );
	VectorCopy( text_pos, bar_end );
	bar_start[ 0 ] -= 10.0f; bar_start[ 2 ] -= 4.0f;
	bar_end[ 0 ]   += 10.0f; bar_end[ 2 ]   -= 4.0f;

	// Background track (Gray)
	clgi.R_DrawDebugLine( bar_start, bar_end, MakeColor( 50, 50, 50, 180 ), 4.0f, 1.0f, VKPT_DEBUG_DRAW_FLAG_NONE );

	// Foreground health fill (Green to Red)
	const float fill_frac = std::clamp( (float)healthPercent / 100.0f, 0.0f, 1.0f );
	vec3_t fill_end;
	VectorLerp( bar_start, bar_end, fill_frac, fill_end );
	clgi.R_DrawDebugLine( bar_start, fill_end, MakeColor( 50, 255, 80, 255 ), 3.0f, 0.0f, VKPT_DEBUG_DRAW_FLAG_NONE );
}
```

---

### 7.5 Example 5: 3D Surface Computer Terminal (Occluded World Orientation)

Renders text physically aligned to a terminal screen mesh on a wall that is properly hidden when the player walks behind an obstacle:

```cpp
void CLG_DrawTerminalScreen( const vec3_t terminal_screen_pos, const vec3_t surface_angles, const char *status_text ) {
	// 1. Configure glowing orange text styling
	clgi.R_SetOuterGlow( MakeColor( 255, 120, 0, 160 ), 4.0f );

	// 2. Render text flat against the screen plane, occluded by walls
	// Character height = 4.0 Quake units
	clgi.R_DrawString3DOccluded(
		terminal_screen_pos,
		surface_angles, // Fixed surface orientation
		4.0f,
		status_text,
		precache.screen.font_pic,
		MakeColor( 255, 180, 50, 255 )
	);

	// 3. Clear styling
	clgi.R_ClearStyle();
}
```

---

### 7.6 Example 6: 3D Tactical Debug Visualization (Capsules, Boxes & Arrows)

Renders collision bounding boxes and AI pathfinding trajectories:

```cpp
void CLG_DrawAgentTacticalDebug( const vec3_t feet_pos, const vec3_t target_waypoint, const bool hasLOS ) {
	// 1. Draw collision cylinder around agent (Height 56 units, Radius 16 units)
	vec3_t top_pos;
	VectorCopy( feet_pos, top_pos );
	top_pos[ 2 ] += 56.0f;

	clgi.R_DrawDebugCylinder(
		feet_pos, top_pos, 16.0f,
		MakeColor( 0, 255, 150, 200 ),
		2.0f, 1.0f,
		VKPT_DEBUG_DRAW_FLAG_DEPTH_TEST | VKPT_DEBUG_DRAW_FLAG_OUTLINE
	);

	// 2. Draw steering direction arrow towards waypoint
	const uint32_t arrow_color = hasLOS ? MakeColor( 50, 255, 50, 255 ) : MakeColor( 255, 50, 50, 255 );
	clgi.R_DrawDebugArrow(
		feet_pos, target_waypoint,
		12.0f, // 12 unit arrow head
		arrow_color,
		3.0f, 1.0f,
		VKPT_DEBUG_DRAW_FLAG_NONE // Non-occluded, see arrow through walls
	);

	// 3. Draw waypoint arrival sphere
	clgi.R_DrawDebugSphere(
		target_waypoint, 8.0f,
		MakeColor( 255, 220, 0, 180 ),
		2.0f, 1.0f,
		VKPT_DEBUG_DRAW_FLAG_DEPTH_TEST
	);
}
```

---

### 7.7 Example 7: Dynamic Player Flashlight with Texture Emission Gobo

Mounts a real-time path-traced spotlight onto the player's viewmodel weapon:

```cpp
void CLG_UpdatePlayerFlashlight( void ) {
	if ( !clg_flashlight || !clg_flashlight->integer ) {
		return;
	}

	// 1. Compute weapon muzzle origin from player eye position and view vectors
	vec3_t light_pos;
	VectorCopy( clgi.client->refdef.vieworg, light_pos );

	// Offset downward, forward, and right to match weapon muzzle
	VectorMA( light_pos, 12.0f, clgi.client->vForward, light_pos );
	VectorMA( light_pos, 4.0f, clgi.client->vRight, light_pos );
	VectorMA( light_pos, -3.0f, clgi.client->vUp, light_pos );

	// 2. Load flashlight aperture cookie texture
	qhandle_t gobo = clgi.R_RegisterPic( "textures/lights/flashlight_gobo" );

	// 3. Emit textured spotlight into the RTX scene
	clgi.V_AddSpotLightTexEmission(
		light_pos,
		clgi.client->vForward,
		clg_flashlight_intensity->value, // Intensity (e.g. 500.0)
		0.95f, 0.98f, 1.0f,              // Subtle cool white tint
		70.0f,                           // 70 degree cone width
		gobo                             // Texture emission gobo
	);
}
```

---

## 8. Comprehensive Public API Reference

### 8.1 Core Types & Bitflags

#### Color Creation
```cpp
// Packs RGBA [0..255] into 32-bit unsigned integer
uint32_t MakeColor( uint8_t r, uint8_t g, uint8_t b, uint8_t a );
```

#### Style Bitflags ([`refresh.h`](../../inc/refresh/refresh.h#L78-L108))
* `STYLE_FLAG_OUTLINE`: Outline stroke enabled.
* `STYLE_FLAG_OUTER_GLOW`: Outer glow / drop shadow enabled.
* `STYLE_FLAG_INNER_GLOW`: Inner edge refraction glow enabled.
* `STYLE_FLAG_SDF_MTSDF`: MTSDF multi-channel distance field active.
* `STYLE_FLAG_STROKE_ALIGN_CENTER` / `INSET` / `OUTSET`: Outline border alignment.
* `STYLE_FLAG_GLOW_FALLOFF_HERMITE` / `EXP`: Glow decay curve.
* `STYLE_FLAG_GLOW_BLEND_ADDITIVE`: Additive HDR blending.
* `STYLE_FLAG_EDGE_TOP` / `RIGHT` / `BOTTOM` / `LEFT` / `ALL`: Selective edge glow masks.
* `STYLE_FLAG_DEPTH_TEST`: 3D linear depth test against scene buffer.

---

### 8.2 2D Rendering & Canvas Functions

```cpp
// Register textures
const qhandle_t clgi.R_RegisterPic( const char *name );
const qhandle_t clgi.R_RegisterPic2( const char *name );
const qhandle_t clgi.R_RegisterImage( const char *name, imagetype_t type, imageflags_t flags );
qhandle_t       clgi.R_RegisterRawImage( const char *name, int w, int h, byte *pic, imagetype_t type, imageflags_t flags );
void            clgi.R_UnregisterImage( qhandle_t handle );
const qboolean  clgi.R_GetPicSize( int32_t *w, int32_t *h, const qhandle_t pic );

// Primitive Draw Calls
void clgi.R_DrawPic( const int32_t x, const int32_t y, const qhandle_t pic );
void clgi.R_DrawStretchPic( const int32_t x, const int32_t y, const int32_t w, const int32_t h, const qhandle_t pic );
void clgi.R_DrawPicEx( double destX, double destY, double destW, double destH, qhandle_t pic, double srcX, double srcY, double srcW, double srcH );
void clgi.R_DrawRotateStretchPic( const int32_t x, const int32_t y, const int32_t w, const int32_t h, const float angle, const int32_t pivot_x, const int32_t pivot_y, const qhandle_t pic );
void clgi.R_DrawKeepAspectPic( const int32_t x, const int32_t y, const int32_t w, const int32_t h, const qhandle_t pic );
void clgi.R_DrawFill32( const int32_t x, const int32_t y, const int32_t w, const int32_t h, const uint32_t color );
void clgi.R_DrawFill32f( float x, float y, float w, float h, uint32_t color );
void clgi.R_DrawLine2D( const float x1, const float y1, const float x2, const float y2, const float thickness, const uint32_t color );
```

---

### 8.3 2D Shader Styling & Effect Functions

```cpp
// Color & Canvas State
void clgi.R_SetColor( const uint32_t color );
void clgi.R_ClearColor( void );
void clgi.R_SetAlpha( const float alpha );
void clgi.R_SetAlphaScale( const float alphaScale );
void clgi.R_SetClipRect( const clipRect_t *clipRect );
void clgi.R_ClearStyle( void );

// Outline Stroke API
void clgi.R_SetStroke( const uint32_t color, const float thickness );
void clgi.R_SetStrokeThickness( const float thickness );
void clgi.R_SetStrokeThickness4( const float top, const float right, const float bottom, const float left );
void clgi.R_SetStrokeColors4( const uint32_t top, const uint32_t right, const uint32_t bottom, const uint32_t left );
void clgi.R_SetStrokeEx( const uint32_t color, const float thickness, const uint32_t flags );
void clgi.R_SetStroke4Ex( const uint32_t colors[4], const float thickness[4], const uint32_t flags );

// Outer Glow / Shadow API
void clgi.R_SetOuterGlow( const uint32_t color, const float radius );
void clgi.R_SetOuterGlowRadius4( const float top, const float right, const float bottom, const float left );
void clgi.R_SetOuterGlowColors4( const uint32_t top, const uint32_t right, const uint32_t bottom, const uint32_t left );
void clgi.R_SetOuterGlowEdges( const uint32_t edge_mask );
void clgi.R_SetOuterGlowEx( const uint32_t color, const float radius, const uint32_t flags );
void clgi.R_SetOuterGlow4Ex( const uint32_t colors[4], const float radii[4], const uint32_t flags );

// Inner Glow API
void clgi.R_SetInnerGlow( const uint32_t color, const float radius );
void clgi.R_SetInnerGlowRadius4( const float top, const float right, const float bottom, const float left );
void clgi.R_SetInnerGlowColors4( const uint32_t top, const uint32_t right, const uint32_t bottom, const uint32_t left );
void clgi.R_SetInnerGlowEx( const uint32_t color, const float radius, const uint32_t flags );

// Corner Radius API
void clgi.R_SetCornerRadius( const float radius );
void clgi.R_SetCornerRadius4( const float top_left, const float top_right, const float bottom_right, const float bottom_left );
```

---

### 8.4 MTSDF Font & Text Functions

```cpp
// Font Registration & Metrics
qhandle_t clgi.R_RegisterFontTTF( const char *path, const float pixel_height );
float     clgi.Font_StringWidthTTF( const qhandle_t font, const char *text );
float     clgi.Font_StringWidthTTF_N( const qhandle_t font, const char *text, const size_t maxlen );
float     clgi.Font_StringWidthTTF_Ex( const qhandle_t font, const char *text, const size_t maxlen, const float extra_spacing );
float     clgi.Font_GetHeightTTF( const qhandle_t font );
float     clgi.R_GetFontEffectSpacing( void );

// Text Drawing
const int32_t clgi.R_DrawString( const int32_t x, const int32_t y, const int32_t flags, const size_t maxChars, const char *str, const qhandle_t font );
void          clgi.R_DrawChar( const int32_t x, const int32_t y, const int32_t flags, const int32_t ch, const qhandle_t font );
```

---

### 8.5 3D World Scene & Entity Functions

```cpp
// Scene Execution & PVS
void clgi.R_RenderFrame( refdef_t *fd );
void clgi.V_CalculateLocalPVS( const vec3_t viewOrigin );

// Entities & Models
const qhandle_t clgi.R_RegisterModel( const char *name );
const qhandle_t clgi.R_RegisterSkin( const char *name );
void            clgi.V_AddEntity( entity_t *ent );
void            clgi.V_AddParticle( particle_t *p );

// Decals
void clgi.R_AddDecal( decal_t *d );
void clgi.R_AddDecalMesh( const decal_mesh_vertex_t *vertices, const int32_t vertexCount, const vec3_t albedo, const float alpha, const uint32_t materialHash, const float lifeSeconds );
void clgi.R_ClearDecals( void );
```

---

### 8.6 3D Dynamic Lighting Functions

```cpp
void clgi.V_AddLight( const vec3_t org, float intensity, float r, float g, float b );
void clgi.V_AddSphereLight( const vec3_t org, float intensity, float r, float g, float b, float radius );
void clgi.V_AddSpotLight( const vec3_t org, const vec3_t dir, float intensity, float r, float g, float b, float width_angle, float falloff_angle );
void clgi.V_AddSpotLightTexEmission( const vec3_t org, const vec3_t dir, float intensity, float r, float g, float b, float width_angle, qhandle_t emission_tex );
void clgi.V_AddLightStyle( int style, float value );
```

---

### 8.7 3D World-Space Text & Debug Primitives

```cpp
// 3D Text
void clgi.R_DrawString3D( const vec3_t origin, const vec3_t angles, const float scale, const char *text, const qhandle_t font, const uint32_t color );
void clgi.R_DrawString3DOccluded( const vec3_t origin, const vec3_t angles, const float scale, const char *text, const qhandle_t font, const uint32_t color );
void clgi.R_DrawString3DNonOccluded( const vec3_t origin, const vec3_t angles, const float scale, const char *text, const qhandle_t font, const uint32_t color );

// Debug Primitives
void clgi.R_DrawDebugBox( const vec3_t mins, const vec3_t maxs, uint32_t color, const float thickness, const float outline_thickness, const uint16_t style_flags );
void clgi.R_DrawDebugLine( const vec3_t start, const vec3_t end, uint32_t color, const float thickness, const float outline_thickness, const uint16_t style_flags );
void clgi.R_DrawDebugArrow( const vec3_t start, const vec3_t end, float head_length, uint32_t color, const float thickness, const float outline_thickness, const uint16_t style_flags );
void clgi.R_DrawDebugSphere( const vec3_t center, float radius, uint32_t color, const float thickness, const float outline_thickness, const uint16_t style_flags );
void clgi.R_DrawDebugCapsule( const vec3_t start, const vec3_t end, float radius, uint32_t color, const float thickness, const float outline_thickness, const uint16_t style_flags );
void clgi.R_DrawDebugCylinder( const vec3_t start, const vec3_t end, float radius, uint32_t color, const float thickness, const float outline_thickness, const uint16_t style_flags );
```

---

### 8.8 Skeletal Model (IQM) API

```cpp
skm_bone_node_t*      clgi.SKM_GetBoneByName( const model_t *model, const char *boneName );
skm_bone_node_t*      clgi.SKM_GetBoneByNumber( const model_t *model, const int32_t boneNumber );
skm_transform_t*      clgi.SKM_PoseCache_AcquireCachedMemoryBlock( const uint32_t size );
const skm_transform_t* clgi.SKM_GetBonePosesForFrame( const model_t *model, const int32_t frame );
void                  clgi.SKM_LerpBonePoses( const model_t *model, const skm_transform_t *frameBonePoses, const skm_transform_t *oldFrameBonePoses, const float frontLerp, const float backLerp, skm_transform_t *outBonePose, const int32_t rootMotionBoneID, const int32_t rootMotionAxisFlags );
void                  clgi.SKM_ComputeLerpBonePoses( const model_t *model, const int32_t frame, const int32_t oldFrame, const float frontLerp, const float backLerp, skm_transform_t *outBonePose, const int32_t rootMotionBoneID, const int32_t rootMotionAxisFlags );
void                  clgi.SKM_RecursiveBlendFromBone( const skm_transform_t *addBonePoses, skm_transform_t *addToBonePoses, const skm_bone_node_t *boneNode, const skm_bone_node_t **excludeNodes, const int32_t numExcludeNodes, const double backLerp, const double fraction );
void                  clgi.SKM_TransformBonePosesLocalSpace( const skm_model_t *model, const skm_transform_t *relativeBonePose, const skm_bone_controller_t *boneControllers, float *pose_matrices );
void                  clgi.SKM_TransformBonePosesWorldSpace( const skm_model_t *model, const skm_transform_t *relativeBonePose, float *pose_matrices );
```

---

## 9. Elaborative Interrogation & Technical Deep-Dive

### Q1: Why not render TrueType vector curves directly on the GPU using loop-blinn or stencil-and-cover?
Direct vector evaluation requires tessellating path triangles on the CPU, uploading dynamic vertex buffers every frame, or executing complex quadratic Bézier root solvers per pixel. 
MTSDF transforms vector curves into a static, highly cacheable texture atlas. GPU rendering reduces to simple textured quad submission (`enqueue_stretch_pic`). The pixel shader executes exactly two texture taps and a single median instruction, delivering pure $O(1)$ constant-time vector rendering at over a thousand frames per second.

### Q2: Why does standard 3-channel MSDF fail for glows, and why is the 4th channel (Alpha) necessary?
MSDF intentionally segments distance fields so that channel discontinuities reconstruct sharp corners. However, calculating an isotropic glow requires the distance function to be smooth and radially symmetric in all directions away from the geometry:

$$\|\nabla d(p)\| = 1$$

MSDF channels do not satisfy this property far from corners. Channel values diverge, producing visible star-shaped spikes and dark directional seams. By adding true Euclidean distance to the 4th (Alpha) channel, MTSDF enables the shader to sample razor-sharp edge coverage from RGB while evaluating glowing halos and soft shadows from Alpha with zero artifacts.

### Q3: Why use a 48px reference height for the master atlas rather than 16px or 64px?
48px represents the optimal sweet spot for distance field numerical stability. At 48px, quadratic Bézier curves provide sufficient texel density to resolve complex serifs and tight diacritics without sub-pixel aliasing. Because distance fields scale cleanly both upward ($10\times$) and downward ($0.25\times$), a 48px master atlas easily spans tiny 10px HUD tooltips up to massive 120px title banners without requiring separate atlases.

### Q4: How does 3D text interact with HDR and Vulkan Path Tracing?
3D world text quads are submitted to the stretch-pic queue with world-space MVP transforms. When rendered into the RTX frame buffer, text fragments pass through the HDR tone-mapping and color space conversion pipeline:
```glsl
if ( spec_tone_mapping_hdr != 0 ) {
	accum_color.rgb *= ui_hdr_nits / 80.0;
	accum_color.rgb = apply_saturation_scale( accum_color.rgb, tm_hdr_saturation_scale * 0.01 );
}
```
This ensures in-game 3D text matches the physical luminance and color gamut of the surrounding path-traced lighting environment.

---

## 10. Console Variables & Diagnostic Commands

| CVar / Command | Type | Default | Description |
| :--- | :--- | :--- | :--- |
| `scr_font` | String | `"fonts/segoeui.ttf"` | Path to the active TrueType/OpenType font used for in-game UI. |
| `scr_scale` | Float | `1.0` | Global UI and HUD scale multiplier. Clamped via `R_ClampScale`. |
| `scr_alpha` | Float | `1.0` | Global UI opacity multiplier [0.0, 1.0]. |
| `scr_font_stroke` | Float | `0.0` | Global outline stroke thickness in pixels for TrueType font rendering. |
| `scr_font_stroke_color` | String | `"#333333"` | Hex color string for font outline strokes. |
| `r_debug_draw` | Integer | `0` | Master gate for 3D debug primitive rendering (0=Disabled, 1=Enabled). |
| `clg_flashlight` | Integer | `0` | Toggles player mounted path-traced flashlight spotlight. |
| `clg_flashlight_intensity` | Float | `400.0` | Intensity multiplier for the path-traced flashlight. |
| `clg_show_lights` | Integer | `0` | Visualizes dynamic light positions and radii with debug spheres. |
| `clg_draw2d` | Integer | `1` | Master toggle to enable/disable 2D HUD and canvas rendering. |
