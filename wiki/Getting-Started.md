# Getting Started with Q2RTXPerimental Development

This guide will walk you through setting up your development environment and creating your first mod for Q2RTXPerimental.

## Prerequisites

Before you begin, ensure you have the following installed:

### Operating System Requirements

| Platform | Minimum Version |
|----------|----------------|
| Windows  | Windows 7 x64 (Windows 10 recommended) |
| Linux    | Ubuntu 16.04 x86_64 or aarch64 (or binary compatible distributions) |

### Required Software

| Software | Minimum Version | Download Link |
|----------|----------------|---------------|
| Git | 2.15 | [https://git-scm.com/downloads](https://git-scm.com/downloads) |
| CMake | 3.8 | [https://cmake.org/download/](https://cmake.org/download/) |
| Vulkan SDK | 1.2.162 | [https://www.lunarg.com/vulkan-sdk/](https://www.lunarg.com/vulkan-sdk/) |
| GPU Driver (NVIDIA) | 460.82+ | [https://www.geforce.com/drivers](https://www.geforce.com/drivers) |
| GPU Driver (AMD) | 21.1.1+ | [https://www.amd.com/en/support](https://www.amd.com/en/support) |

### Development Tools

- **Windows**: Visual Studio 2019 or later (recommended) or Visual Studio Code with C++ extension
- **Linux**: GCC 7+ or Clang 6+ with standard build tools

## Cloning the Repository

1. Open a terminal or command prompt
2. Clone the repository with all submodules:

```bash
git clone --recursive https://github.com/PolyhedronStudio/Q2RTXPerimental.git
cd Q2RTXPerimental
```

If you forgot the `--recursive` flag, initialize submodules manually:

```bash
git submodule update --init --recursive
```

## Setting Up Game Assets

The engine requires game asset packages to run:

1. Create a `baseq2` directory in the repository root if it doesn't exist:
   ```bash
   mkdir baseq2
   ```

2. Download or copy the following required files into `baseq2/`:
   - `blue_noise.pkz` - Blue noise textures for path tracing
   - `q2rtx_media.pkz` - Q2RTX media assets
   - `pak0.pak` and `pak1.pak` - Original Quake II game data files

3. For the Q2RTXPerimental-specific content, also include:
   - `pak2.pak` (if available) - Additional Q2RTXPerimental assets

**Note**: The `.pkz` files can be found in [Q2RTX releases](https://github.com/NVIDIA/Q2RTX/releases). The original `pak` files come from your Quake II installation or from [GOG](https://www.gog.com/game/quake_ii_quad_damage).

## Building the Engine

### Step 1: Create Build Directory

The build directory **must** be named `build` under the repository root (required for shader build rules):

```bash
mkdir build
cd build
```

### Step 2: Configure CMake

#### Windows (Visual Studio)

```bash
cmake .. -G "Visual Studio 16 2019" -A x64
```

Or for Visual Studio 2022:
```bash
cmake .. -G "Visual Studio 17 2022" -A x64
```

#### Linux

```bash
cmake .. -DCMAKE_BUILD_TYPE=Release
```

**Important**: Only 64-bit builds are supported.

### Step 3: Build

#### Using CMake Command Line

```bash
cmake --build . --config Release
```

#### Using Visual Studio (Windows)

1. Open `Q2RTXPerimental.sln` in Visual Studio
2. Select "Release" configuration
3. Build → Build Solution (Ctrl+Shift+B)

#### Using Make (Linux)

```bash
make -j$(nproc)
```

### Step 4: Run the Engine

After building, the executable will be in:
- **Windows**: `build/Release/q2rtxp.exe`
- **Linux**: `build/q2rtxp`

Run it from the repository root directory:

```bash
# Windows
.\build\Release\q2rtxp.exe

# Linux
./build/q2rtxp
```

## Project Structure

Understanding the repository layout:

```
Q2RTXPerimental/
├── baseq2rtxp/          # Game data directory for Q2RTXPerimental
├── src/
│   ├── baseq2rtxp/      # Game module source code
│   │   ├── clgame/      # Client game module
│   │   ├── svgame/      # Server game module
│   │   └── sharedgame/  # Shared code between client and server
│   ├── client/          # Engine client code
│   ├── server/          # Engine server code
│   ├── refresh/         # Rendering/graphics code
│   └── common/          # Common engine utilities
├── inc/                 # Public header files
├── extern/              # External dependencies (submodules)
├── build/               # Build output directory (create this)
├── wiki/                # This documentation
└── CMakeLists.txt       # Main CMake configuration
```

## Creating Your First Mod

Now that you have the engine built, let's create a simple custom entity as your first mod.

### Understanding the Game Modules

Q2RTXPerimental uses a modular architecture:

- **Server Game (svgame)**: Authoritative game logic, physics, AI
- **Client Game (clgame)**: Client-side prediction, rendering, effects
- **Shared Game (sharedgame)**: Code shared between client and server

For this tutorial, we'll create a simple custom entity on the server side.

### Example: Creating a Simple Light Entity

Let's create a custom light entity that pulses in brightness:

#### 1. Create the Entity Header File

Create `src/baseq2rtxp/svgame/entities/misc/svg_misc_pulselight.h`:

```cpp
#pragma once

#include "svgame/entities/svg_base_edict.h"

/**
 * @brief A simple pulsing light entity
 * 
 * This entity demonstrates basic entity creation with a Think callback.
 * The light will pulse in brightness over time.
 */
class svg_misc_pulselight_t : public svg_base_edict_t {
public:
    // Constructor
    svg_misc_pulselight_t() = default;
    virtual ~svg_misc_pulselight_t() = default;

    // Required: Spawn function
    virtual void Spawn() override;

    // Think callback for pulsing logic
    void PulseThink();

private:
    float baseLight = 200.0f;      // Base light intensity
    float pulseSpeed = 2.0f;       // Pulse speed in cycles per second
    float pulseAmount = 100.0f;    // How much to vary the light
};

// Register the entity type with the spawn system
DECLARE_EDICT_SPAWN_INFO(misc_pulselight, svg_misc_pulselight_t);
```

#### 2. Create the Entity Implementation File

Create `src/baseq2rtxp/svgame/entities/misc/svg_misc_pulselight.cpp`:

```cpp
#include "svg_misc_pulselight.h"
#include "svgame/svg_main.h"

// Register this entity with the spawn system
DEFINE_EDICT_SPAWN_INFO(misc_pulselight, svg_misc_pulselight_t);

/**
 * @brief Spawn the pulse light entity
 */
void svg_misc_pulselight_t::Spawn() {
    // Call base class spawn first
    svg_base_edict_t::Spawn();

    // Set entity properties
    s.entityType = ET_GENERAL;  // Generic entity type
    solid = SOLID_NOT;          // No collision
    movetype = MOVETYPE_NONE;   // Doesn't move

    // Read spawn parameters from entity dictionary
    if (entityDictionary.contains("light")) {
        baseLight = std::stof(entityDictionary["light"]);
    }
    if (entityDictionary.contains("speed")) {
        pulseSpeed = std::stof(entityDictionary["speed"]);
    }
    if (entityDictionary.contains("amount")) {
        pulseAmount = std::stof(entityDictionary["amount"]);
    }

    // Set up model if specified
    if (entityDictionary.contains("model")) {
        gi.SetModel(edict, entityDictionary["model"].c_str());
    }

    // Link into world
    gi.LinkEntity(edict);

    // Set up Think callback
    SetThinkCallback(&svg_misc_pulselight_t::PulseThink);
    nextThinkTime = level.time + FRAMETIME;
}

/**
 * @brief Update light intensity in a pulsing pattern
 */
void svg_misc_pulselight_t::PulseThink() {
    // Calculate current light value using sine wave
    float time = level.time.count() * pulseSpeed;
    float pulse = std::sin(time * M_PI * 2.0f);
    float currentLight = baseLight + (pulse * pulseAmount);

    // Update light style (would require light system integration)
    // For this example, we just demonstrate the pattern
    s.renderEffects = (currentLight > baseLight) ? EF_HYPERBLASTER : 0;

    // Schedule next think
    nextThinkTime = level.time + FRAMETIME;
}
```

#### 3. Add to CMake Build

Edit `src/baseq2rtxp/svgame/CMakeLists.txt` (or the appropriate build file) to include your new files:

```cmake
# Add to the svgame source list
set(SVGAME_SOURCES
    # ... existing files ...
    entities/misc/svg_misc_pulselight.cpp
    entities/misc/svg_misc_pulselight.h
    # ... rest of files ...
)
```

#### 4. Rebuild the Engine

```bash
cd build
cmake --build . --config Release
```

#### 5. Use in a Map

In your map editor (like TrenchBroom), you can now place an entity with:
- **classname**: `misc_pulselight`
- **light**: 200 (base brightness)
- **speed**: 2.0 (pulses per second)
- **amount**: 100 (pulse intensity variation)

### Next Steps

Now that you've created your first custom entity, explore more advanced topics:

- [Entity System Overview](Entity-System-Overview.md) - Learn about the entity architecture
- [Creating Custom Entities](Creating-Custom-Entities.md) - More advanced entity creation techniques
- [Server Game Module](Server-Game-Module.md) - Understanding server-side game logic
- [Client Game Module](Client-Game-Module.md) - Adding client-side effects

## Development Tips

### Hot Reloading

Q2RTXPerimental supports hot-reloading of game modules during development:

1. Start the engine with the console enabled: `+set developer 1`
2. After rebuilding game DLLs, reload them with: `game_restart`

This saves time by not requiring a full engine restart.

### Debugging

#### Visual Studio (Windows)

1. Set `q2rtxp` as the startup project
2. Set breakpoints in your entity code
3. Press F5 to start debugging

#### GDB (Linux)

```bash
gdb ./build/q2rtxp
(gdb) run
# Reproduce issue
(gdb) bt  # Show backtrace when crash occurs
```

### Console Commands

Useful console commands for development:

- `developer 1` - Enable developer mode with extra logging
- `map <mapname>` - Load a specific map
- `give all` - Give all weapons and items (single player)
- `god` - Toggle god mode
- `noclip` - Toggle no-clip mode (fly through walls)
- `spawn <classname>` - Spawn an entity at your location
- `sv_cheats 1` - Enable cheats (required for some commands)

## Troubleshooting

### Common Build Issues

**CMake can't find Vulkan SDK**
- Ensure Vulkan SDK is installed and `VULKAN_SDK` environment variable is set
- Restart your terminal/IDE after installing Vulkan SDK

**Submodule errors**
```bash
git submodule update --init --recursive --force
```

**Shader compilation errors**
- Ensure you created the `build` directory at the repository root
- Check that `glslangValidator` is available in your PATH

### Common Runtime Issues

**"Couldn't load pics/colormap.pcx"**
- Ensure `pak0.pak` and `pak1.pak` are in the `baseq2` directory

**Black screen or no rendering**
- Verify GPU drivers are up to date
- Check Vulkan SDK is properly installed
- Ensure your GPU supports Vulkan 1.2+

**Game crashes on startup**
- Check console output for error messages
- Verify all required `.pkz` files are present
- Try running with `+set developer 1` for more verbose logging

## Getting Help

If you encounter issues:

1. Check existing [GitHub Issues](https://github.com/PolyhedronStudio/Q2RTXPerimental/issues)
2. Join the [Discord server](https://discord.gg/6Qc6wfmFMR) for real-time help
3. Review the [Q2RTX documentation](https://github.com/NVIDIA/Q2RTX) for engine-level issues

---

**Next**: [Architecture Overview](Architecture-Overview.md) - Learn about Q2RTXPerimental's architecture
