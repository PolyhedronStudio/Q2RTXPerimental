#
#
#	Game: BaseQ2
#
#
# BaseQ2 SharedGame
SET(SRC_BASEQ2_SHAREDGAME
	baseq2/sharedgame/sg_gamemode.cpp
	baseq2/sharedgame/sg_misc.cpp
	baseq2/sharedgame/sg_pmove.cpp
	baseq2/sharedgame/sg_pmove_slidemove.cpp
)
SET(HEADERS_BASEQ2_SHAREDGAME
	baseq2/sharedgame/sg_cmd_messages.h
	baseq2/sharedgame/sg_gamemode.h
	baseq2/sharedgame/sg_local.h
	baseq2/sharedgame/sg_misc.h
	baseq2/sharedgame/sg_pmove.h
	baseq2/sharedgame/sg_pmove_slidemove.h
	baseq2/sharedgame/sg_shared.h
	baseq2/sharedgame/sg_time.h
)
#	BaseQ2 ClientGame
SET(SRC_BASEQ2_CLGAME
	# SharedGame API Bindings
	baseq2/sharedgame/game_bindings/sg_binding_clgame.cpp

	# ClientGame
	baseq2/clgame/clg_client.cpp
	baseq2/clgame/clg_eax.cpp
	baseq2/clgame/clg_effects.cpp
	baseq2/clgame/clg_entities.cpp
	baseq2/clgame/clg_input.cpp
	baseq2/clgame/clg_local_entities.cpp
	baseq2/clgame/clg_gamemode.cpp
	baseq2/clgame/clg_main.cpp
	baseq2/clgame/clg_packet_entities.cpp
	baseq2/clgame/clg_parse.cpp
	baseq2/clgame/clg_precache.cpp
	baseq2/clgame/clg_predict.cpp
	baseq2/clgame/clg_temp_entities.cpp
	baseq2/clgame/clg_screen.cpp
	baseq2/clgame/clg_view.cpp

	baseq2/clgame/effects/clg_fx_classic.cpp
	baseq2/clgame/effects/clg_fx_dynamiclights.cpp
	baseq2/clgame/effects/clg_fx_footsteps.cpp
	baseq2/clgame/effects/clg_fx_lightstyles.cpp
	baseq2/clgame/effects/clg_fx_muzzleflash.cpp
	baseq2/clgame/effects/clg_fx_muzzleflash2.cpp
	baseq2/clgame/effects/clg_fx_particles.cpp
	baseq2/clgame/effects/clg_fx_new.cpp
	
	baseq2/clgame/local_entities/clg_local_env_sound.cpp	
	baseq2/clgame/local_entities/clg_local_misc_entities.cpp

	baseq2/clgame/temp_entities/clg_te_beams.cpp
	baseq2/clgame/temp_entities/clg_te_explosions.cpp
	baseq2/clgame/temp_entities/clg_te_lasers.cpp
	baseq2/clgame/temp_entities/clg_te_railtrails.cpp
	baseq2/clgame/temp_entities/clg_te_sustain.cpp
)
SET(HEADERS_BASEQ2_CLGAME
	baseq2/clgame/clg_client.h
	baseq2/clgame/clg_eax.h
	baseq2/clgame/clg_effects.h
	baseq2/clgame/clg_entities.h
	baseq2/clgame/clg_input.h
	baseq2/clgame/clg_local.h
	baseq2/clgame/clg_local_entities.h
	baseq2/clgame/clg_packet_entities.h
	baseq2/clgame/clg_parse.h
	baseq2/clgame/clg_precache.h
	baseq2/clgame/clg_predict.h
	baseq2/clgame/clg_screen.h
	baseq2/clgame/clg_temp_entities.h
	baseq2/clgame/clg_view.h

	baseq2/clgame/local_entities/clg_local_env_sound.h
	baseq2/clgame/local_entities/clg_local_entity_classes.h
)
# BaseQ2 ServerGame
SET(SRC_BASEQ2_SVGAME
	# SharedGame API Bindings
	baseq2/sharedgame/game_bindings/sg_binding_svgame.cpp

	# SererGame
	baseq2/svgame/g_ai.cpp
	baseq2/svgame/g_chase.cpp
	baseq2/svgame/g_cmds.cpp
	baseq2/svgame/g_combat.cpp
	baseq2/svgame/g_func.cpp
	baseq2/svgame/g_gamemode.cpp
	baseq2/svgame/g_items.cpp
	baseq2/svgame/g_main.cpp
	baseq2/svgame/g_misc.cpp
	baseq2/svgame/g_monster.cpp
	baseq2/svgame/g_phys.cpp
	baseq2/svgame/g_ptrs.cpp
	baseq2/svgame/g_save.cpp
	baseq2/svgame/g_spawn.cpp
	baseq2/svgame/g_spotlight.cpp
	baseq2/svgame/g_svcmds.cpp
	baseq2/svgame/g_target.cpp
	baseq2/svgame/g_trigger.cpp
	baseq2/svgame/g_turret.cpp
	baseq2/svgame/g_utils.cpp
	baseq2/svgame/g_weapon.cpp
	baseq2/svgame/m_actor.cpp
	baseq2/svgame/m_berserk.cpp
	baseq2/svgame/m_boss2.cpp
	baseq2/svgame/m_boss3.cpp
	baseq2/svgame/m_boss31.cpp
	baseq2/svgame/m_boss32.cpp
	baseq2/svgame/m_brain.cpp
	baseq2/svgame/m_chick.cpp
	baseq2/svgame/m_flipper.cpp
	baseq2/svgame/m_float.cpp
	baseq2/svgame/m_flyer.cpp
	baseq2/svgame/m_gladiator.cpp
	baseq2/svgame/m_gunner.cpp
	baseq2/svgame/m_hover.cpp
	baseq2/svgame/m_infantry.cpp
	baseq2/svgame/m_insane.cpp
	baseq2/svgame/m_medic.cpp
	baseq2/svgame/m_move.cpp
	baseq2/svgame/m_mutant.cpp
	baseq2/svgame/m_parasite.cpp
	baseq2/svgame/m_soldier.cpp
	baseq2/svgame/m_supertank.cpp
	baseq2/svgame/m_tank.cpp
	baseq2/svgame/p_client.cpp
	baseq2/svgame/p_hud.cpp
	baseq2/svgame/p_trail.cpp
	baseq2/svgame/p_view.cpp
	baseq2/svgame/p_weapon.cpp
)
SET(HEADERS_BASEQ2_SVGAME
	baseq2/svgame/g_local.h
	baseq2/svgame/g_save.h

	baseq2/svgame/m_actor.h
	baseq2/svgame/m_berserk.h
	baseq2/svgame/m_boss2.h
	baseq2/svgame/m_boss31.h
	baseq2/svgame/m_boss32.h
	baseq2/svgame/m_brain.h
	baseq2/svgame/m_chick.h
	baseq2/svgame/m_flipper.h
	baseq2/svgame/m_float.h
	baseq2/svgame/m_flyer.h
	baseq2/svgame/m_gladiator.h
	baseq2/svgame/m_gunner.h
	baseq2/svgame/m_hover.h
	baseq2/svgame/m_infantry.h
	baseq2/svgame/m_insane.h
	baseq2/svgame/m_medic.h
	baseq2/svgame/m_mutant.h
	baseq2/svgame/m_parasite.h
	baseq2/svgame/m_player.h
	baseq2/svgame/m_rider.h
	baseq2/svgame/m_soldier.h
	baseq2/svgame/m_supertank.h
	baseq2/svgame/m_tank.h
)



#
#
#	Game: BaseQ2RTXP
#
#
# BaseQ2RTXP SharedGame
SET(SRC_BASEQ2RTXP_SHAREDGAME
	baseq2rtxp/sharedgame/sg_gamemode.cpp
	baseq2rtxp/sharedgame/sg_misc.cpp
	baseq2rtxp/sharedgame/sg_pmove.cpp
	baseq2rtxp/sharedgame/sg_pmove_slidemove.cpp
)
SET(HEADERS_BASEQ2RTXP_SHAREDGAME
	baseq2rtxp/sharedgame/sg_cmd_messages.h
	baseq2rtxp/sharedgame/sg_gamemode.h
	baseq2rtxp/sharedgame/sg_local.h
	baseq2rtxp/sharedgame/sg_misc.h
	baseq2rtxp/sharedgame/sg_pmove.h
	baseq2rtxp/sharedgame/sg_pmove_slidemove.h
	baseq2rtxp/sharedgame/sg_shared.h
	baseq2rtxp/sharedgame/sg_time.h
)
#	BaseQ2RTXP ClientGame
SET(SRC_BASEQ2RTXP_CLGAME
	# SharedGame API Bindings
	baseq2rtxp/sharedgame/game_bindings/sg_binding_clgame.cpp

	# ClientGame
	baseq2rtxp/clgame/clg_client.cpp
	baseq2rtxp/clgame/clg_eax.cpp
	baseq2rtxp/clgame/clg_effects.cpp
	baseq2rtxp/clgame/clg_entities.cpp
	baseq2rtxp/clgame/clg_input.cpp
	baseq2rtxp/clgame/clg_local_entities.cpp
	baseq2rtxp/clgame/clg_gamemode.cpp
	baseq2rtxp/clgame/clg_main.cpp
	baseq2rtxp/clgame/clg_packet_entities.cpp
	baseq2rtxp/clgame/clg_parse.cpp
	baseq2rtxp/clgame/clg_precache.cpp
	baseq2rtxp/clgame/clg_predict.cpp
	baseq2rtxp/clgame/clg_temp_entities.cpp
	baseq2rtxp/clgame/clg_screen.cpp
	baseq2rtxp/clgame/clg_view.cpp

	baseq2rtxp/clgame/effects/clg_fx_classic.cpp
	baseq2rtxp/clgame/effects/clg_fx_dynamiclights.cpp
	baseq2rtxp/clgame/effects/clg_fx_footsteps.cpp
	baseq2rtxp/clgame/effects/clg_fx_lightstyles.cpp
	baseq2rtxp/clgame/effects/clg_fx_muzzleflash.cpp
	baseq2rtxp/clgame/effects/clg_fx_muzzleflash2.cpp
	baseq2rtxp/clgame/effects/clg_fx_particles.cpp
	baseq2rtxp/clgame/effects/clg_fx_new.cpp
	
	baseq2rtxp/clgame/local_entities/clg_local_env_sound.cpp	
	baseq2rtxp/clgame/local_entities/clg_local_misc_entities.cpp

	baseq2rtxp/clgame/temp_entities/clg_te_beams.cpp
	baseq2rtxp/clgame/temp_entities/clg_te_explosions.cpp
	baseq2rtxp/clgame/temp_entities/clg_te_lasers.cpp
	baseq2rtxp/clgame/temp_entities/clg_te_railtrails.cpp
	baseq2rtxp/clgame/temp_entities/clg_te_sustain.cpp
)
SET(HEADERS_BASEQ2RTXP_CLGAME
	baseq2rtxp/clgame/clg_client.h
	baseq2rtxp/clgame/clg_eax.h
	baseq2rtxp/clgame/clg_effects.h
	baseq2rtxp/clgame/clg_entities.h
	baseq2rtxp/clgame/clg_input.h
	baseq2rtxp/clgame/clg_local.h
	baseq2rtxp/clgame/clg_local_entities.h
	baseq2rtxp/clgame/clg_packet_entities.h
	baseq2rtxp/clgame/clg_parse.h
	baseq2rtxp/clgame/clg_precache.h
	baseq2rtxp/clgame/clg_predict.h
	baseq2rtxp/clgame/clg_screen.h
	baseq2rtxp/clgame/clg_temp_entities.h
	baseq2rtxp/clgame/clg_view.h

	baseq2rtxp/clgame/local_entities/clg_local_env_sound.h
	baseq2rtxp/clgame/local_entities/clg_local_entity_classes.h
)
# BaseQ2RTXP ServerGame
SET(SRC_BASEQ2RTXP_SVGAME
	# SharedGame API Bindings
	baseq2rtxp/sharedgame/game_bindings/sg_binding_svgame.cpp

	# SererGame
	baseq2rtxp/svgame/weapons/p_weapon_fists.cpp
	baseq2rtxp/svgame/weapons/p_weapon_pistol.cpp

	baseq2rtxp/svgame/g_ai.cpp
	baseq2rtxp/svgame/g_chase.cpp
	baseq2rtxp/svgame/g_cmds.cpp
	baseq2rtxp/svgame/g_combat.cpp
	baseq2rtxp/svgame/g_func.cpp
	baseq2rtxp/svgame/g_gamemode.cpp
	baseq2rtxp/svgame/g_items.cpp
	baseq2rtxp/svgame/g_main.cpp
	baseq2rtxp/svgame/g_misc.cpp
	baseq2rtxp/svgame/g_monster.cpp
	baseq2rtxp/svgame/g_phys.cpp
	baseq2rtxp/svgame/g_ptrs.cpp
	baseq2rtxp/svgame/g_save.cpp
	baseq2rtxp/svgame/g_spawn.cpp
	baseq2rtxp/svgame/g_spotlight.cpp
	baseq2rtxp/svgame/g_svcmds.cpp
	baseq2rtxp/svgame/g_target.cpp
	baseq2rtxp/svgame/g_trigger.cpp
	baseq2rtxp/svgame/g_turret.cpp
	baseq2rtxp/svgame/g_utils.cpp
	baseq2rtxp/svgame/g_weapon.cpp
	baseq2rtxp/svgame/m_actor.cpp
	baseq2rtxp/svgame/m_berserk.cpp
	baseq2rtxp/svgame/m_boss2.cpp
	baseq2rtxp/svgame/m_boss3.cpp
	baseq2rtxp/svgame/m_boss31.cpp
	baseq2rtxp/svgame/m_boss32.cpp
	baseq2rtxp/svgame/m_brain.cpp
	baseq2rtxp/svgame/m_chick.cpp
	baseq2rtxp/svgame/m_flipper.cpp
	baseq2rtxp/svgame/m_float.cpp
	baseq2rtxp/svgame/m_flyer.cpp
	baseq2rtxp/svgame/m_gladiator.cpp
	baseq2rtxp/svgame/m_gunner.cpp
	baseq2rtxp/svgame/m_hover.cpp
	baseq2rtxp/svgame/m_infantry.cpp
	baseq2rtxp/svgame/m_insane.cpp
	baseq2rtxp/svgame/m_medic.cpp
	baseq2rtxp/svgame/m_move.cpp
	baseq2rtxp/svgame/m_mutant.cpp
	baseq2rtxp/svgame/m_parasite.cpp
	baseq2rtxp/svgame/m_soldier.cpp
	baseq2rtxp/svgame/m_supertank.cpp
	baseq2rtxp/svgame/m_tank.cpp
	baseq2rtxp/svgame/p_client.cpp
	baseq2rtxp/svgame/p_hud.cpp
	baseq2rtxp/svgame/p_trail.cpp
	baseq2rtxp/svgame/p_view.cpp
	baseq2rtxp/svgame/p_weapon.cpp
)
SET(HEADERS_BASEQ2RTXP_SVGAME
	baseq2rtxp/svgame/g_local.h
	baseq2rtxp/svgame/g_save.h

	baseq2rtxp/svgame/m_actor.h
	baseq2rtxp/svgame/m_berserk.h
	baseq2rtxp/svgame/m_boss2.h
	baseq2rtxp/svgame/m_boss31.h
	baseq2rtxp/svgame/m_boss32.h
	baseq2rtxp/svgame/m_brain.h
	baseq2rtxp/svgame/m_chick.h
	baseq2rtxp/svgame/m_flipper.h
	baseq2rtxp/svgame/m_float.h
	baseq2rtxp/svgame/m_flyer.h
	baseq2rtxp/svgame/m_gladiator.h
	baseq2rtxp/svgame/m_gunner.h
	baseq2rtxp/svgame/m_hover.h
	baseq2rtxp/svgame/m_infantry.h
	baseq2rtxp/svgame/m_insane.h
	baseq2rtxp/svgame/m_medic.h
	baseq2rtxp/svgame/m_mutant.h
	baseq2rtxp/svgame/m_parasite.h
	baseq2rtxp/svgame/m_player.h
	baseq2rtxp/svgame/m_rider.h
	baseq2rtxp/svgame/m_soldier.h
	baseq2rtxp/svgame/m_supertank.h
	baseq2rtxp/svgame/m_tank.h
)



#
#
#	Client
#
#
SET(SRC_CLIENT
	client/ascii.cpp
	client/console.cpp
	client/cin.cpp
	client/clgame.cpp
	client/crc.cpp
	client/demo.cpp
	client/download.cpp
	client/effects.cpp
	client/entities.cpp
	client/input.cpp
	client/keys.cpp
	client/locs.cpp
	client/main.cpp
	#client/newfx.cpp
	#client/null.cpp
	client/parse.cpp
	client/precache.cpp
	client/predict.cpp
	client/refresh.cpp
	client/screen.cpp
	#client/tent.cpp
	client/view.cpp
	client/world.cpp
	client/ui/_scoreboard.cpp
	client/ui/demos.cpp
	client/ui/menu.cpp
	client/ui/playerconfig.cpp
	client/ui/playermodels.cpp
	client/ui/script.cpp
	client/ui/servers.cpp
	client/ui/ui.cpp
	client/sound/dma.c
	client/sound/al.c
	client/sound/main.c
	client/sound/mem.c
	client/sound/ogg.c
	client/sound/qal/fixed.c
#	client/sound/qal/dynamic.cpp
)

SET(SRC_CLIENT_HTTP
	client/http.cpp
)

SET(HEADERS_CLIENT
	client/cl_client.h
	client/ui/ui.h
	client/sound/sound.h
	client/sound/qal/dynamic.h
	client/sound/qal/fixed.h
)


#
#
# Server
#
#
SET(SRC_SERVER
	server/commands.cpp
	server/entities.cpp
	server/init.cpp
	server/main.cpp
	server/send.cpp
	server/svgame.cpp
	server/user.cpp
	server/world.cpp
	server/save.cpp
)

SET(HEADERS_SERVER
	server/server.h
)


#
#
# Common
#
#
SET(SRC_COMMON
	common/async.c
	common/bsp.cpp
	common/cmd.cpp
	common/collisionmodel/cm_areaportals.cpp
	common/collisionmodel/cm_boxleafs.cpp
	common/collisionmodel/cm_entities.cpp
	common/collisionmodel/cm_hull_boundingbox.cpp
	common/collisionmodel/cm_hull_octagonbox.cpp
	common/collisionmodel/cm_materials.cpp
	common/collisionmodel/cm_pointcontents.cpp
	common/collisionmodel/cm_trace.cpp
	common/collisionmodel.cpp
	common/common.cpp
	common/cvar.cpp
	common/error.cpp
	common/field.cpp
	common/fifo.cpp
	common/files.cpp
	common/huffman.cpp
	common/math.cpp
	common/mdfour.cpp
	common/messaging.cpp
	#common/pmove.cpp
	common/prompt.cpp
	common/sizebuf.cpp
#	common/tests.cpp
	common/utils.cpp
	common/zone.cpp

	common/messaging/ParseDeltaEntityState.cpp
	common/messaging/ParseDeltaPlayerState.cpp
	common/messaging/ParseDeltaUserCommand.cpp

	common/messaging/WriteDeltaEntityState.cpp
	common/messaging/WriteDeltaPlayerState.cpp
	common/messaging/WriteDeltaUserCommand.cpp

	common/net/chan.cpp
	common/net/Q2RTXPerimentalNetChan.cpp
	common/net/net.cpp
)
SET(HEADERS_COMMON
	#common/net/chan.h
	#common/net/Q2RTXPerimentalNetChan.h
	common/net/inet_ntop.h
	common/net/inet_pton.h
	common/net/win.h
)


#
#
# Refresh GL:
#
#
SET(SRC_REFRESH
	refresh/images.c
	refresh/models.c
	refresh/model_iqm.c
	refresh/stb/stb.c
)
SET(SRC_GL
	refresh/gl/draw.c
	refresh/gl/hq2x.c
	refresh/gl/texture.c
	refresh/gl/main.c
	refresh/gl/mesh.c
	refresh/gl/models.c
	refresh/gl/sky.c
	refresh/gl/state.c
	refresh/gl/surf.c
	refresh/gl/tess.c
	refresh/gl/world.c
	refresh/gl/qgl.c
	refresh/gl/legacy.c
	refresh/gl/shader.c
)
SET(HEADERS_GL
	refresh/gl/arbfp.h
	refresh/gl/gl.h
)


#
#
# Shared
#
#
SET(HEADERS_SHARED
	#shared/clgame.h
	#shared/config.h
	#shared/config_cpp.h
	#shared/endian.h
	#shared/info_strings.h
	#shared/util_list.h
	#shared/platform.h
	#shared/shared.h
	#shared/shared_cpp.h
	#shared/string_utilities.h
	#inc/shared/svgame.h

	#inc/shared/math/qray_math.h
	#inc/shared/math/qray_matrix4x4.h
	#inc/shared/math/qray_quaternion.h
	#inc/shared/math/qray_utils.h
	#inc/shared/math/qray_vector2.h
	#inc/shared/math/qray_vector3.h
	#inc/shared/math/qray_vector4.h
)
SET(SRC_SHARED
	shared/info_strings.cpp
	shared/math.cpp
	shared/m_flash.c
	shared/shared.cpp
	shared/string_utilities.cpp
)


#
#
# Linux
#
#
SET(SRC_LINUX
	unix/hunk.c
	unix/system.c
	unix/tty.c
)
SET(SRC_LINUX_CLIENT
	unix/sound/sdl.c
	unix/video.cpp
)


#
#
# Windows
#
#
SET(SRC_WINDOWS
	#windows/ac.cpp
	windows/debug.cpp
	windows/hunk.cpp
	windows/system.cpp
)
SET(SRC_WINDOWS_CLIENT
	windows/wave.c
	unix/sound/sdl.c
	unix/video.cpp
)
SET(HEADERS_WINDOWS
	#windows/wgl.h
	#windows/glimp.h
	windows/client.h
	windows/threads/threads.h
)


#
#
# Refresh VKPT:
#
#
SET(SRC_VKPT
	refresh/vkpt/asvgf.c
	refresh/vkpt/bloom.c
	refresh/vkpt/bsp_mesh.c
	refresh/vkpt/draw.c
	refresh/vkpt/fog.c
	refresh/vkpt/cameras.c
	refresh/vkpt/freecam.c
	refresh/vkpt/fsr.c
	refresh/vkpt/main.c
	refresh/vkpt/material.c
	refresh/vkpt/matrix.c
	refresh/vkpt/mgpu.c
	refresh/vkpt/models.c
	refresh/vkpt/path_tracer.c
	refresh/vkpt/physical_sky.c
	refresh/vkpt/precomputed_sky.c
	refresh/vkpt/profiler.c
	refresh/vkpt/shadow_map.c
	refresh/vkpt/textures.c
	refresh/vkpt/tone_mapping.c
	refresh/vkpt/transparency.c
	refresh/vkpt/uniform_buffer.c
	refresh/vkpt/vertex_buffer.c
	refresh/vkpt/vk_util.c
	refresh/vkpt/buddy_allocator.c
	refresh/vkpt/device_memory_allocator.c
	refresh/vkpt/god_rays.c
	refresh/vkpt/conversion.c
)
SET(HEADERS_VKPT
	refresh/vkpt/vkpt.h
	refresh/vkpt/vk_util.h
	refresh/vkpt/buddy_allocator.h
	refresh/vkpt/device_memory_allocator.h
	refresh/vkpt/fog.h
	refresh/vkpt/cameras.h
	refresh/vkpt/material.h
	refresh/vkpt/physical_sky.h
	refresh/vkpt/precomputed_sky.h
	refresh/vkpt/conversion.h
)
set(SRC_SHADERS
	refresh/vkpt/shader/animate_materials.comp
	refresh/vkpt/shader/god_rays_filter.comp
	refresh/vkpt/shader/god_rays.comp
	refresh/vkpt/shader/bloom_composite.comp
	refresh/vkpt/shader/bloom_blur.comp
	refresh/vkpt/shader/bloom_downscale.comp
	refresh/vkpt/shader/compositing.comp
	refresh/vkpt/shader/checkerboard_interleave.comp
	refresh/vkpt/shader/asvgf_atrous.comp
	refresh/vkpt/shader/asvgf_gradient_atrous.comp
	refresh/vkpt/shader/asvgf_gradient_img.comp
	refresh/vkpt/shader/asvgf_gradient_reproject.comp
	refresh/vkpt/shader/asvgf_lf.comp
	refresh/vkpt/shader/asvgf_taau.comp
	refresh/vkpt/shader/asvgf_temporal.comp
	refresh/vkpt/shader/instance_geometry.comp
	refresh/vkpt/shader/normalize_normal_map.comp
	refresh/vkpt/shader/tone_mapping_histogram.comp
	refresh/vkpt/shader/tone_mapping_curve.comp
    refresh/vkpt/shader/tone_mapping_apply.comp
	refresh/vkpt/shader/physical_sky.comp
	refresh/vkpt/shader/physical_sky_space.comp
	refresh/vkpt/shader/shadow_map.vert
	refresh/vkpt/shader/sky_buffer_resolve.comp
	refresh/vkpt/shader/stretch_pic.frag
	refresh/vkpt/shader/stretch_pic.vert
	refresh/vkpt/shader/final_blit_lanczos.frag
	refresh/vkpt/shader/final_blit.vert
	refresh/vkpt/shader/fsr_easu_fp16.comp
	refresh/vkpt/shader/fsr_easu_fp32.comp
	refresh/vkpt/shader/fsr_rcas_fp16.comp
	refresh/vkpt/shader/fsr_rcas_fp32.comp
)
set(SRC_RT_SHADERS
	refresh/vkpt/shader/primary_rays.rgen
	refresh/vkpt/shader/direct_lighting.rgen
	refresh/vkpt/shader/indirect_lighting.rgen
	refresh/vkpt/shader/path_tracer.rchit
	refresh/vkpt/shader/path_tracer.rmiss
	refresh/vkpt/shader/path_tracer_masked.rahit
	refresh/vkpt/shader/path_tracer_particle.rahit
	refresh/vkpt/shader/path_tracer_sprite.rahit
	refresh/vkpt/shader/path_tracer_beam.rahit
	refresh/vkpt/shader/path_tracer_beam.rint
	refresh/vkpt/shader/path_tracer_explosion.rahit
	refresh/vkpt/shader/reflect_refract.rgen
)