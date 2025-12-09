# Wiki Migration Guide

## Overview

This document explains the wiki migration that has been prepared for the Q2RTXPerimental repository. All changes have been committed locally and are ready to be pushed to the GitHub wiki.

## What Was Done

### 1. Backed Up Existing Wiki Content
The existing GitHub wiki had two pages:
- `Home.md` - Basic project introduction
- `Documentation.md` - Feature documentation

These have been preserved as:
- `Old-Wiki-Home.md`
- `Old-Wiki-Documentation.md`

### 2. Migrated Comprehensive Documentation
All 33 markdown files from the `/wiki/` directory have been copied to the wiki repository:

#### Vanilla Quake 2 Foundation (7 pages)
- `Vanilla-Game-Module-Architecture.md` - Client-server separation and DLL interface
- `Vanilla-Entity-System.md` - Entity system basics
- `Vanilla-BSP-Format.md` - Map structure and entity placement
- `Vanilla-Networking.md` - Networking fundamentals
- `Vanilla-Physics.md` - Physics and movement
- `Vanilla-Weapons.md` - Weapon system implementation
- `Vanilla-AI.md` - AI and pathfinding basics

#### Q2RTXPerimental Architecture (4 pages)
- `Architecture-Overview.md` - High-level system design
- `Server-Game-Module.md` - Server-side game logic (svgame)
- `Client-Game-Module.md` - Client-side prediction and rendering (clgame)
- `Shared-Game-Module.md` - Shared code between client and server

#### Entity System (6 pages)
- `Entity-System-Overview.md` - Entity architecture overview
- `Entity-Base-Class-Reference.md` - svg_base_edict_t complete reference
- `Creating-Custom-Entities.md` - Step-by-step tutorial with examples
- `Entity-Lifecycle.md` - Entity spawn, think, and lifecycle
- `Entity-Networking.md` - Entity synchronization to clients
- `Entity-Save-Load.md` - Persistence and serialization

#### Temporary Entity System (4 pages)
- `Temp-Entity-Overview.md` - What temp entities are and when to use them
- `Temp-Entity-Event-Types.md` - Complete event type reference
- `Using-Temp-Entities.md` - How to spawn temp entities
- `Custom-Temp-Entities.md` - Adding new temp entity types

#### Advanced Topics (5 pages)
- `Signal-IO-System.md` - Entity communication system
- `UseTargets-System.md` - Interactive entity system
- `Game-Modes.md` - Custom game mode development
- `Lua-Scripting.md` - Lua integration
- `Skeletal-Animation.md` - SKM animation system

#### API Reference (5 pages)
- `API-Entity-Types.md` - Entity type constants
- `API-Entity-Flags.md` - Entity flag reference
- `API-Entity-Events.md` - Entity event system
- `API-Spawn-Flags.md` - Map editor spawn flags
- `API-Means-Of-Death.md` - Damage type constants

#### Meta Pages (3 pages)
- `Home.md` - Updated main wiki page with comprehensive navigation
- `Getting-Started.md` - Build instructions and development setup
- `README.md` - Documentation structure overview

### 3. Created Navigation Structure

#### `_Sidebar.md`
Created a comprehensive sidebar navigation organized into categories:
- Main (Home, Getting Started)
- Vanilla Quake 2 (7 pages)
- Architecture (4 pages)
- Entity System (6 pages)
- Temp Entities (4 pages)
- Advanced Topics (5 pages)
- API Reference (5 pages)
- Old Wiki (2 backup pages)

#### `_Footer.md`
Added footer with:
- Links to GitHub repository
- Discord community links
- Version information reference

## Current Status

### ✅ Completed
- All wiki content backed up
- All 33 new pages copied and ready
- Navigation sidebar created
- Footer created
- All changes committed to local git repository at `/home/runner/work/Q2RTXPerimental/wiki-repo`

### ⏳ Pending
- Push changes to GitHub wiki repository (requires authentication)

## How to Complete the Migration

You have three options to complete the push:

### Option 1: Run the Push Script (Recommended)
```bash
./push-wiki-migration.sh
```

This script will:
- Verify all changes are ready
- Show a summary of what will be pushed
- Attempt to push to the GitHub wiki
- Provide troubleshooting steps if authentication fails

### Option 2: Manual Push from wiki-repo Directory
```bash
cd /home/runner/work/Q2RTXPerimental/wiki-repo
git status
git log origin/master..master  # Review commits
git push origin master
```

### Option 3: Clone and Copy Manually
If authentication is problematic, you can:

1. Clone the wiki repository in a separate location:
```bash
git clone https://github.com/PolyhedronStudio/Q2RTXPerimental.wiki.git my-wiki
```

2. Copy all markdown files:
```bash
cp /home/runner/work/Q2RTXPerimental/wiki-repo/*.md my-wiki/
```

3. Commit and push:
```bash
cd my-wiki
git add -A
git commit -m "Migrate comprehensive documentation from /wiki/ directory"
git push origin master
```

## Verifying the Migration

After pushing, visit your wiki to verify:
- https://github.com/PolyhedronStudio/Q2RTXPerimental/wiki

You should see:
1. ✅ New comprehensive Home page with navigation
2. ✅ Sidebar navigation on the left with all categories
3. ✅ All 33 new documentation pages accessible
4. ✅ Old wiki content preserved in Old-Wiki-* pages
5. ✅ Footer with links on every page

## Troubleshooting

### Authentication Issues
If you encounter authentication errors:

1. **Using HTTPS**: Configure credentials
   ```bash
   git config --global credential.helper store
   git push origin master
   # Enter your GitHub username and personal access token
   ```

2. **Using GitHub CLI**:
   ```bash
   gh auth login
   # Follow the prompts to authenticate
   cd /home/runner/work/Q2RTXPerimental/wiki-repo
   git push origin master
   ```

3. **Using SSH**: Add SSH key and update remote
   ```bash
   cd /home/runner/work/Q2RTXPerimental/wiki-repo
   git remote set-url origin git@github.com:PolyhedronStudio/Q2RTXPerimental.wiki.git
   git push origin master
   ```

### Changes Already Pushed
If the push says "Everything up-to-date", the migration may have already completed. Check your wiki on GitHub.

### Merge Conflicts
If someone else edited the wiki during migration:
```bash
cd /home/runner/work/Q2RTXPerimental/wiki-repo
git pull origin master
# Resolve any conflicts
git push origin master
```

## Repository Structure After Migration

```
wiki-repo/
├── _Sidebar.md                        # Navigation menu
├── _Footer.md                         # Footer with links
├── Home.md                            # Main wiki page (updated)
├── Getting-Started.md                 # Build and setup guide
├── README.md                          # Documentation overview
├── Old-Wiki-Home.md                   # Backup of original Home
├── Old-Wiki-Documentation.md          # Backup of original Documentation
│
├── Vanilla-*.md                       # 7 Vanilla Quake 2 pages
├── Architecture-Overview.md           # Architecture pages
├── Client-Game-Module.md
├── Server-Game-Module.md
├── Shared-Game-Module.md
│
├── Entity-*.md                        # 6 Entity system pages
├── Creating-Custom-Entities.md
│
├── Temp-Entity-*.md                   # 4 Temp entity pages
├── Using-Temp-Entities.md
├── Custom-Temp-Entities.md
│
├── Signal-IO-System.md               # Advanced topics
├── UseTargets-System.md
├── Game-Modes.md
├── Lua-Scripting.md
├── Skeletal-Animation.md
│
└── API-*.md                          # 5 API reference pages
```

## Summary

The wiki migration is **ready to push** with:
- **38 files total** (33 new pages + 2 backups + 3 meta files)
- **20,864+ lines of documentation** added
- **Fully organized navigation** structure
- **Old content preserved** for reference

Run `./push-wiki-migration.sh` to complete the migration!
