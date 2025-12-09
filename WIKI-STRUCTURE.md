# Wiki Migration - Visual Structure

## Before (Old Wiki)
```
GitHub Wiki
├── Home.md (9 lines - basic intro)
└── Documentation.md (42 lines - feature list)
```

## After (New Wiki)
```
GitHub Wiki
├── _Sidebar.md (54 lines - navigation menu)
├── _Footer.md (5 lines - links and version info)
├── Home.md (101 lines - comprehensive navigation hub)
├── Getting-Started.md (397 lines)
├── README.md (158 lines)
│
├── Old-Wiki-Home.md (9 lines - backup)
├── Old-Wiki-Documentation.md (42 lines - backup)
│
├── Vanilla Quake 2 Foundation (7 pages)
│   ├── Vanilla-Game-Module-Architecture.md (494 lines)
│   ├── Vanilla-Entity-System.md (617 lines)
│   ├── Vanilla-BSP-Format.md (822 lines)
│   ├── Vanilla-Networking.md (407 lines)
│   ├── Vanilla-Physics.md (1,090 lines)
│   ├── Vanilla-Weapons.md (853 lines)
│   └── Vanilla-AI.md (881 lines)
│
├── Q2RTXPerimental Architecture (4 pages)
│   ├── Architecture-Overview.md (462 lines)
│   ├── Server-Game-Module.md (704 lines)
│   ├── Client-Game-Module.md (646 lines)
│   └── Shared-Game-Module.md (624 lines)
│
├── Entity System (6 pages)
│   ├── Entity-System-Overview.md (596 lines)
│   ├── Entity-Base-Class-Reference.md (1,857 lines)
│   ├── Creating-Custom-Entities.md (782 lines)
│   ├── Entity-Lifecycle.md (603 lines)
│   ├── Entity-Networking.md (265 lines)
│   └── Entity-Save-Load.md (319 lines)
│
├── Temporary Entity System (4 pages)
│   ├── Temp-Entity-Overview.md (590 lines)
│   ├── Temp-Entity-Event-Types.md (751 lines)
│   ├── Using-Temp-Entities.md (629 lines)
│   └── Custom-Temp-Entities.md (578 lines)
│
├── Advanced Topics (5 pages)
│   ├── Signal-IO-System.md (633 lines)
│   ├── UseTargets-System.md (332 lines)
│   ├── Game-Modes.md (960 lines)
│   ├── Lua-Scripting.md (587 lines)
│   └── Skeletal-Animation.md (443 lines)
│
└── API Reference (5 pages)
    ├── API-Entity-Types.md (468 lines)
    ├── API-Entity-Flags.md (508 lines)
    ├── API-Entity-Events.md (434 lines)
    ├── API-Spawn-Flags.md (551 lines)
    └── API-Means-Of-Death.md (613 lines)
```

## Statistics

### Before
- **Pages**: 2
- **Lines**: 51
- **Size**: ~6 KB

### After
- **Pages**: 38 (33 new + 2 backups + 3 meta)
- **Lines**: 20,915 (20,864 new + 51 old)
- **Size**: ~500 KB

### Growth
- **Pages**: 19x increase (1,900%)
- **Lines**: 410x increase (41,000%)
- **Coverage**: Comprehensive documentation for entire engine

## Navigation Structure

The `_Sidebar.md` organizes all pages into logical categories:

1. **Main** (2 pages)
   - Quick access to home and getting started

2. **Vanilla Quake 2** (7 pages)
   - Foundation knowledge required for modding

3. **Architecture** (4 pages)
   - Q2RTXPerimental-specific design and modules

4. **Entity System** (6 pages)
   - Core gameplay programming

5. **Temp Entities** (4 pages)
   - Visual effects system

6. **Advanced Topics** (5 pages)
   - Specialized systems

7. **API Reference** (5 pages)
   - Quick reference for constants and enums

8. **Old Wiki** (2 pages)
   - Preserved original content

## Content Highlights

### Most Comprehensive Pages
1. Entity-Base-Class-Reference.md (1,857 lines)
2. Vanilla-Physics.md (1,090 lines)
3. Game-Modes.md (960 lines)
4. Vanilla-AI.md (881 lines)
5. Vanilla-Weapons.md (853 lines)

### Tutorial Pages
- Getting-Started.md - Development environment setup
- Creating-Custom-Entities.md - Complete entity creation tutorial
- Using-Temp-Entities.md - Visual effects guide

### Reference Pages
- All API-*.md files - Quick lookup for constants
- Entity-Base-Class-Reference.md - Complete class documentation

## What Users Will See

### Home Page
- Comprehensive navigation with emojis
- Quick links to major sections
- Introduction to Q2RTXPerimental
- Links to community resources

### Sidebar (on every page)
- Hierarchical navigation
- All 38 pages organized by category
- Quick access to any documentation

### Footer (on every page)
- Links to GitHub repository
- Discord community links
- Version information

## Backup Safety

The old wiki content is preserved in:
- `Old-Wiki-Home.md` - Original introduction
- `Old-Wiki-Documentation.md` - Feature documentation

These pages are accessible via:
- Sidebar under "Old Wiki" section
- Direct links from anywhere

## Ready to Deploy

✅ All content migrated
✅ Navigation structured
✅ Old content backed up
✅ Locally committed
⏳ Awaiting push to GitHub

Run `./push-wiki-migration.sh` to deploy!
