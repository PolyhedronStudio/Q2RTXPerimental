#pragma once

#include "shared/math/qm_vector3_dp.h"

class svg_monster_base_t;

/**
*	@brief	Start a bounded opt-in capture for one active monster slot/incarnation.
*	@note	Console command: sv nav_corner_capture <entityNum> <frameCount> [fullRun:0|1], range 1..4096.
**/
void Nav_Corner_CaptureCommand();

/**
*	@brief	Dump a frozen capture outside the measured movement loop.
*	@note	Console command: sv nav_corner_report <entityNum> [output.jsonl]; reports JSON frame records.
**/
void Nav_Corner_ReportCommand();

/**
*	@brief	Record one realized movement update when its entity is selected for capture.
*	@param	monster	Active mover; retained data contains no entity pointers.
*	@param	startOrigin	BBox origin before authoritative movement.
*	@param	commandVelocity	Velocity issued before authoritative movement.
*	@param	blockedMask	Final physics flags, retained separately from direct clip counters.
*	@note	Fixed 120-frame contact ring; optional full-run/route storage is prepared by the console command.
**/
void Nav_Corner_RecordMovement( const svg_monster_base_t &monster, const Vector3DP &startOrigin,
	const Vector3DP &commandVelocity, const int32_t blockedMask );

/**
*	@brief	Read-only native traversal/strict-clearance query for an active monster profile.
*	@note	Console: sv nav_corner_probe <entity> <sx sy sz> <ex ey ez> [requireHeight:0|1], all coordinates feet.
**/
void Nav_Corner_ProbeCommand();

/**
*	@brief	Collision-checked monster placement for opt-in native engine stair/path fixtures.
*	@note	Console: sv nav_corner_place <entity> <feet x y z>; fails without a clear supported hull.
**/
void Nav_Corner_PlaceCommand();

/**
*	@brief	Set an exact supported target for an isolated singleton crowd fixture.
*	@note	Console: sv nav_corner_goal <entity> <feet x y z>; normal movement/steering remains active.
**/
void Nav_Corner_GoalCommand();

/**
*	@brief	Inspect exact route generation and full hull validation without installing a route.
*	@note	Console: sv nav_corner_route <entity> <goal feet x y z>; reports every retained link and source obligation.
**/
void Nav_Corner_RouteCommand();

/**
*	@brief	Set an explicit collision-checked native profile, policy, yaw and speed for one fixture actor.
*	@note	Console: sv nav_corner_profile <entity> <radius> <shape:1|2> <minZ> <maxZ> <step> <drop> <normalZ> <yaw> <speedScale>.
**/
void Nav_Corner_ProfileCommand();

/**
*	@brief	Select opt-in causal observation: normal, geometry hold, direct frozen, blend frozen, separation frozen, normal replans.
*	@note	Console: sv nav_corner_mode <entity> <mode:0..5>; production default is zero.
**/
void Nav_Corner_ModeCommand();

/**
*	@brief	Initialize both game RNGs from an explicit fixture seed before map entities spawn.
*	@note	sv_nav_fixture_seed defaults to zero, retaining ordinary time-based initialization.
**/
void Nav_Corner_InitRandomSeed();

/**
*	@brief	Invalidate selected capture sampling before a new navigation/map lifetime reuses identities.
*	@note	Retained evidence can still be reported with capture_complete=false.
**/
void Nav_Corner_InvalidateCapture();

/**
*	@brief	Install a fully validated bounded authored fixture route using the live movement contract.
*	@note	Console: sv nav_corner_replay <entity> <3..16 feet points: x y z ...>.
*			Fixture provenance is explicit; generation is isolated, while release and native physics remain active.
**/
void Nav_Corner_ReplayCommand();

/**
*	@brief	Reset or report a fixed-storage whole-crowd physical movement audit.
*	@note	Console: sv nav_corner_audit <crowd> <reset|report>; no allocations during movement sampling.
**/
void Nav_Corner_AuditCommand();
/**
*	@brief	Accumulate every selected crowd member's realized movement and admission checks.
*	@param	monster	Live owner after authoritative movement.
*	@param	startOrigin	Exact bbox origin before the command.
*	@param	commandVelocity	Original velocity before native resolution.
**/
void Nav_Corner_AuditMovement( const svg_monster_base_t &monster, const Vector3DP &startOrigin, const Vector3DP &commandVelocity );

/**
*	@brief	Set an explicit seeded local-player inspection camera for native renderer fixtures.
*	@note	Console: sv nav_corner_camera <bbox x y z> <pitch yaw>; requires an active local client and cheats.
**/
void Nav_Corner_CameraCommand();
/**
*	@brief	Enable or disable an opt-in route/trajectory/native-hull/candidate visual overlay.
*	@note	Console: sv nav_corner_overlay <0|1>; uses already copied capture evidence only.
**/
void Nav_Corner_OverlayCommand();
/**
*	@brief	Draw the selected fixture's bounded copied route, realized trail and steering evidence.
*	@note	Disabled by default; no generation, clearance query or actor state mutation occurs.
**/
void Nav_Corner_DebugDraw();
