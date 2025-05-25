#
#
#	Game: BaseQ2
#
#
# BaseQ2 SharedGame
SET(SRC_BASEQ2_SHAREDGAME
	baseq2/sharedgame/sg_gamemode.cpp
	baseq2/sharedgame/sg_misc.cpp
	baseq2/sharedgame/sg_muzzleflashes_monsters.c
	baseq2/sharedgame/sg_pmove.cpp
	baseq2/sharedgame/sg_pmove_slidemove.cpp
)
SET(HEADERS_BASEQ2_SHAREDGAME
	baseq2/sharedgame/sg_cmd_messages.h
	baseq2/sharedgame/sg_gamemode.h
	baseq2/sharedgame/sg_local.h
	baseq2/sharedgame/sg_misc.h
	baseq2/sharedgame/sg_muzzleflashes.h
	baseq2/sharedgame/sg_muzzleflashes_monsters.h
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
	baseq2rtxp/sharedgame/sg_skm.cpp
	baseq2rtxp/sharedgame/sg_skm_rootmotion.cpp
	baseq2rtxp/sharedgame/sg_usetarget_hints.cpp
)
SET(HEADERS_BASEQ2RTXP_SHAREDGAME
	baseq2rtxp/sharedgame/sg_cmd_messages.h
	baseq2rtxp/sharedgame/sg_gamemode.h
	baseq2rtxp/sharedgame/sg_local.h
	baseq2rtxp/sharedgame/sg_misc.h
	baseq2rtxp/sharedgame/sg_muzzleflashes.h
	baseq2rtxp/sharedgame/sg_pmove.h
	baseq2rtxp/sharedgame/sg_pmove_slidemove.h
	baseq2rtxp/sharedgame/sg_qtag_memory.hpp
	baseq2rtxp/sharedgame/sg_qtag_string.hpp
	baseq2rtxp/sharedgame/sg_shared.h
	baseq2rtxp/sharedgame/sg_shared_fwd.h
	baseq2rtxp/sharedgame/sg_skm.h
	baseq2rtxp/sharedgame/sg_skm_rootmotion.h
	baseq2rtxp/sharedgame/sg_tempentity_events.h
	baseq2rtxp/sharedgame/sg_time.h
	baseq2rtxp/sharedgame/sg_usetarget_hints.h
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
	baseq2rtxp/clgame/clg_events.cpp
	baseq2rtxp/clgame/clg_input.cpp
	baseq2rtxp/clgame/clg_local_entities.cpp
	baseq2rtxp/clgame/clg_gamemode.cpp
	baseq2rtxp/clgame/clg_hud.cpp
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
	
	baseq2rtxp/clgame/hud/clg_hud_usetargethint.cpp

	baseq2rtxp/clgame/local_entities/clg_local_env_sound.cpp	
	baseq2rtxp/clgame/local_entities/clg_local_misc_entities.cpp

	baseq2rtxp/clgame/packet_entities/clg_packet_et_beam.cpp
	baseq2rtxp/clgame/packet_entities/clg_packet_et_generic.cpp
	baseq2rtxp/clgame/packet_entities/clg_packet_et_gib.cpp
	baseq2rtxp/clgame/packet_entities/clg_packet_et_item.cpp
	baseq2rtxp/clgame/packet_entities/clg_packet_et_monster.cpp
	baseq2rtxp/clgame/packet_entities/clg_packet_et_player.cpp
	baseq2rtxp/clgame/packet_entities/clg_packet_et_pusher.cpp
	baseq2rtxp/clgame/packet_entities/clg_packet_et_spotlight.cpp

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
	baseq2rtxp/clgame/clg_events.h
	baseq2rtxp/clgame/clg_hud.h
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

	baseq2rtxp/clgame/hud/clg_hud_usetargethint.h
)
# BaseQ2RTXP ServerGame
SET(SRC_BASEQ2RTXP_SVGAME
	# SharedGame API Bindings
	baseq2rtxp/sharedgame/game_bindings/sg_binding_svgame.cpp

	# ServerGame
	baseq2rtxp/svgame/svg_ai.cpp
	baseq2rtxp/svgame/svg_chase.cpp
	baseq2rtxp/svgame/svg_clients.cpp
	baseq2rtxp/svgame/svg_commands_game.cpp
	baseq2rtxp/svgame/svg_commands_server.cpp
	baseq2rtxp/svgame/svg_combat.cpp
	baseq2rtxp/svgame/svg_edicts.cpp
	baseq2rtxp/svgame/svg_edict_pool.cpp
	baseq2rtxp/svgame/svg_gamemode.cpp
	baseq2rtxp/svgame/svg_game_client.cpp
	baseq2rtxp/svgame/svg_game_items.cpp
	baseq2rtxp/svgame/svg_game_locals.cpp
	baseq2rtxp/svgame/svg_level_locals.cpp
	baseq2rtxp/svgame/svg_lua.cpp
	baseq2rtxp/svgame/svg_main.cpp
	baseq2rtxp/svgame/svg_misc.cpp
	baseq2rtxp/svgame/svg_monster.cpp
	baseq2rtxp/svgame/svg_physics.cpp
	baseq2rtxp/svgame/svg_save_pointers.cpp
	baseq2rtxp/svgame/svg_save.cpp
	baseq2rtxp/svgame/svg_signalio.cpp
	baseq2rtxp/svgame/svg_spawn.cpp
	baseq2rtxp/svgame/svg_stepmove.cpp
	baseq2rtxp/svgame/svg_target.cpp
	baseq2rtxp/svgame/svg_trigger.cpp
	baseq2rtxp/svgame/svg_utils.cpp
	baseq2rtxp/svgame/svg_weapon.cpp

	baseq2rtxp/svgame/entities/func/svg_func_areaportal.cpp
	baseq2rtxp/svgame/entities/func/svg_func_button.cpp
	#baseq2rtxp/svgame/entities/func/svg_func_breakable.cpp
	#baseq2rtxp/svgame/entities/func/svg_func_conveyor.cpp
	baseq2rtxp/svgame/entities/func/svg_func_door.cpp
	baseq2rtxp/svgame/entities/func/svg_func_door_rotating.cpp
	#baseq2rtxp/svgame/entities/func/svg_func_door_secret.cpp
	#baseq2rtxp/svgame/entities/func/svg_func_killbox.cpp
	#baseq2rtxp/svgame/entities/func/svg_func_object.cpp
	baseq2rtxp/svgame/entities/func/svg_func_plat.cpp
	baseq2rtxp/svgame/entities/func/svg_func_rotating.cpp
	#baseq2rtxp/svgame/entities/func/svg_func_timer.cpp
	#baseq2rtxp/svgame/entities/func/svg_func_train.cpp
	#baseq2rtxp/svgame/entities/func/svg_func_wall.cpp
	baseq2rtxp/svgame/entities/func/svg_func_water.cpp
	baseq2rtxp/svgame/entities/func/svg_func_entities.cpp

	baseq2rtxp/svgame/entities/info/svg_info_notnull.cpp
	baseq2rtxp/svgame/entities/info/svg_info_null.cpp
	baseq2rtxp/svgame/entities/info/svg_info_player_coop.cpp
	baseq2rtxp/svgame/entities/info/svg_info_player_deathmatch.cpp
	baseq2rtxp/svgame/entities/info/svg_info_player_intermission.cpp
	baseq2rtxp/svgame/entities/info/svg_info_player_start.cpp

	baseq2rtxp/svgame/entities/light/svg_light_light.cpp
	baseq2rtxp/svgame/entities/light/svg_light_spotlight.cpp

	baseq2rtxp/svgame/entities/misc/svg_misc_explobox.cpp
	baseq2rtxp/svgame/entities/misc/svg_misc_teleporter.cpp
	baseq2rtxp/svgame/entities/misc/svg_misc_teleporter_dest.cpp

	baseq2rtxp/svgame/entities/monster/svg_monster_testdummy_puppet.cpp
	
	baseq2rtxp/svgame/entities/path/svg_path_corner.cpp

	#baseq2rtxp/svgame/entities/target/svg_target_character.cpp

	baseq2rtxp/svgame/entities/trigger/svg_trigger_always.cpp
	baseq2rtxp/svgame/entities/trigger/svg_trigger_counter.cpp
	baseq2rtxp/svgame/entities/trigger/svg_trigger_elevator.cpp
	baseq2rtxp/svgame/entities/trigger/svg_trigger_gravity.cpp
	baseq2rtxp/svgame/entities/trigger/svg_trigger_hurt.cpp
	baseq2rtxp/svgame/entities/trigger/svg_trigger_multiple.cpp
	baseq2rtxp/svgame/entities/trigger/svg_trigger_once.cpp
	baseq2rtxp/svgame/entities/trigger/svg_trigger_push.cpp
	baseq2rtxp/svgame/entities/trigger/svg_trigger_relay.cpp

	baseq2rtxp/svgame/entities/svg_base_edict.cpp
	baseq2rtxp/svgame/entities/svg_item_edict.cpp
	baseq2rtxp/svgame/entities/svg_player_edict.cpp
	baseq2rtxp/svgame/entities/svg_pushmove_edict.cpp
	baseq2rtxp/svgame/entities/svg_worldspawn_edict.cpp
	baseq2rtxp/svgame/entities/svg_entities_pushermove.cpp

	baseq2rtxp/svgame/lua/svg_lua_corelib.cpp
	baseq2rtxp/svgame/lua/svg_lua_gamelib.cpp
	baseq2rtxp/svgame/lua/svg_lua_medialib.cpp

	baseq2rtxp/svgame/lua/gamelib/svg_lua_gamelib_entities.cpp
	baseq2rtxp/svgame/lua/gamelib/svg_lua_gamelib_pushmovers.cpp
	baseq2rtxp/svgame/lua/gamelib/svg_lua_gamelib_signalio.cpp
	baseq2rtxp/svgame/lua/gamelib/svg_lua_gamelib_usetargets.cpp

	baseq2rtxp/svgame/lua/medialib/svg_lua_medialib_sound.cpp

	baseq2rtxp/svgame/lua/usertypes/svg_lua_usertype_edict_state_t.cpp
	baseq2rtxp/svgame/lua/usertypes/svg_lua_usertype_edict_state_t.hpp
	baseq2rtxp/svgame/lua/usertypes/svg_lua_usertype_edict_t.cpp
	baseq2rtxp/svgame/lua/usertypes/svg_lua_usertype_edict_t.hpp

	baseq2rtxp/svgame/save/svg_save_field_descriptor.cpp
	baseq2rtxp/svgame/save/svg_save_funcptr_instance.cpp
	baseq2rtxp/svgame/save/svg_save_read_context.cpp
	baseq2rtxp/svgame/save/svg_save_write_context.cpp

	baseq2rtxp/svgame/monsters/svg_mmove.cpp
	baseq2rtxp/svgame/monsters/svg_mmove_slidemove.cpp

	baseq2rtxp/svgame/player/svg_player_animation.cpp
	baseq2rtxp/svgame/player/svg_player_client.cpp
	baseq2rtxp/svgame/player/svg_player_events.cpp
	baseq2rtxp/svgame/player/svg_player_hud.cpp
	baseq2rtxp/svgame/player/svg_player_move.cpp
	baseq2rtxp/svgame/player/svg_player_obituary.cpp
	baseq2rtxp/svgame/player/svg_player_spawn.cpp
	baseq2rtxp/svgame/player/svg_player_trail.cpp
	baseq2rtxp/svgame/player/svg_player_view.cpp
	baseq2rtxp/svgame/player/svg_player_weapon.cpp

	baseq2rtxp/svgame/weapons/svg_weapon_fists.cpp
	baseq2rtxp/svgame/weapons/svg_weapon_pistol.cpp
)
SET(HEADERS_BASEQ2RTXP_SVGAME
	baseq2rtxp/svgame/svg_chase.h
	baseq2rtxp/svgame/svg_clients.h
	baseq2rtxp/svgame/svg_combat.h
	baseq2rtxp/svgame/svg_edict_pool.h
	baseq2rtxp/svgame/svg_game_client.h
	baseq2rtxp/svgame/svg_game_items.h
	baseq2rtxp/svgame/svg_game_locals.h
	baseq2rtxp/svgame/svg_level_locals.h
	baseq2rtxp/svgame/svg_local.h
	baseq2rtxp/svgame/svg_local_fwd.h
	baseq2rtxp/svgame/svg_lua.h
	baseq2rtxp/svgame/svg_misc.h
	baseq2rtxp/svgame/svg_pushmove_info.h
	baseq2rtxp/svgame/svg_save.h
	baseq2rtxp/svgame/svg_signalio.cpp
	baseq2rtxp/svgame/svg_trigger.h
	baseq2rtxp/svgame/svg_utils.h
	baseq2rtxp/svgame/svg_usetargets.h
	baseq2rtxp/svgame/svg_weapons.h

	baseq2rtxp/svgame/lua/svg_lua_callfunction.hpp
	baseq2rtxp/svgame/lua/svg_lua_signals.hpp
	
	baseq2rtxp/svgame/lua/svg_lua_gamelib.hpp
	baseq2rtxp/svgame/lua/svg_lua_medialib.hpp

	baseq2rtxp/svgame/entities/func/svg_func_areaportal.h
	baseq2rtxp/svgame/entities/func/svg_func_button.h
	baseq2rtxp/svgame/entities/func/svg_func_breakable.h
	baseq2rtxp/svgame/entities/func/svg_func_conveyor.h
	baseq2rtxp/svgame/entities/func/svg_func_door.h
	baseq2rtxp/svgame/entities/func/svg_func_door_rotating.h
	#baseq2rtxp/svgame/entities/func/svg_func_door_secret.h
	baseq2rtxp/svgame/entities/func/svg_func_killbox.h
	baseq2rtxp/svgame/entities/func/svg_func_object.h
	baseq2rtxp/svgame/entities/func/svg_func_plat.h
	baseq2rtxp/svgame/entities/func/svg_func_rotating.h
	baseq2rtxp/svgame/entities/func/svg_func_timer.h
	baseq2rtxp/svgame/entities/func/svg_func_train.h
	baseq2rtxp/svgame/entities/func/svg_func_wall.h
	baseq2rtxp/svgame/entities/func/svg_func_water.h
	baseq2rtxp/svgame/entities/func/svg_func_entities.h

	baseq2rtxp/svgame/entities/info/svg_info_player_start.h
	baseq2rtxp/svgame/entities/info/svg_info_notnull.h
	baseq2rtxp/svgame/entities/info/svg_info_null.h

	baseq2rtxp/svgame/entities/light/svg_light_light.h
	baseq2rtxp/svgame/entities/light/svg_light_spotlight.h

	baseq2rtxp/svgame/entities/monster/svg_monster_testdummy.h
	
	baseq2rtxp/svgame/entities/misc/svg_misc_explobox.h
	baseq2rtxp/svgame/entities/misc/svg_misc_teleporter.h
	baseq2rtxp/svgame/entities/misc/svg_misc_teleporter_dest.h

	baseq2rtxp/svgame/entities/path/svg_path_corner.h
	
	#baseq2rtxp/svgame/entities/target/svg_target_character.h

	baseq2rtxp/svgame/entities/trigger/svg_trigger_always.h
	baseq2rtxp/svgame/entities/trigger/svg_trigger_counter.h
	baseq2rtxp/svgame/entities/trigger/svg_trigger_elevator.h
	baseq2rtxp/svgame/entities/trigger/svg_trigger_gravity.h
	baseq2rtxp/svgame/entities/trigger/svg_trigger_hurt.h
	baseq2rtxp/svgame/entities/trigger/svg_trigger_multiple.h
	baseq2rtxp/svgame/entities/trigger/svg_trigger_once.h
	baseq2rtxp/svgame/entities/trigger/svg_trigger_push.h
	baseq2rtxp/svgame/entities/trigger/svg_trigger_relay.h

	baseq2rtxp/svgame/entities/typeinfo/svg_edict_typeinfo.h

	baseq2rtxp/svgame/entities/svg_base_edict.h
	baseq2rtxp/svgame/entities/svg_item_edict.h
	baseq2rtxp/svgame/entities/svg_player_edict.h
	baseq2rtxp/svgame/entities/svg_worldspawn_edict.h
	baseq2rtxp/svgame/entities/svg_entities_pushermove.h

	baseq2rtxp/svgame/player/svg_player_client.h
	baseq2rtxp/svgame/player/svg_player_hud.h
	baseq2rtxp/svgame/player/svg_player_move.h
	baseq2rtxp/svgame/player/svg_player_trail.h
	baseq2rtxp/svgame/player/svg_player_weapon.h
	baseq2rtxp/svgame/player/svg_player_view.h

	baseq2rtxp/svgame/save/svg_save_callback_global.h
	baseq2rtxp/svgame/save/svg_save_callback_member.h
	baseq2rtxp/svgame/save/svg_save_field_descriptor.h
	baseq2rtxp/svgame/save/svg_save_funcptr_instance.h
	baseq2rtxp/svgame/save/svg_save_read_context.h
	baseq2rtxp/svgame/save/svg_save_write_context.h

	baseq2rtxp/svgame/monsters/svg_mmove.h
	baseq2rtxp/svgame/monsters/svg_mmove_slidemove.h
)



#
#
#	Client
#
#
SET(SRC_CLIENT
	client/cl_ascii.cpp
	client/cl_console.cpp
	client/cl_cinematic.cpp
	client/cl_clientgame.cpp
	client/cl_crc.cpp
	client/cl_demo.cpp
	client/cl_download.cpp
	client/cl_effects.cpp
	client/cl_entities.cpp
	client/cl_input.cpp
	client/cl_keys.cpp
	client/cl_locs.cpp
	client/cl_main.cpp
	#client/cl_newfx.cpp
	#client/cl_null.cpp
	client/cl_parse.cpp
	client/cl_precache.cpp
	client/cl_predict.cpp
	client/cl_refresh.cpp
	client/cl_screen.cpp
	#client/cl_tent.cpp
	client/cl_view.cpp
	client/cl_world.cpp
	client/ui/_scoreboard.cpp
	client/ui/demos.cpp
	client/ui/editor_rmaterial.cpp
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
	client/cl_http.cpp
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
	refresh/model_iqm.c

	server/sv_commands.cpp
	server/sv_entities.cpp
	server/sv_init.cpp
	server/sv_main.cpp
	server/sv_models.cpp
	server/sv_send.cpp
	server/sv_game.cpp
	server/sv_user.cpp
	server/sv_world.cpp
	server/sv_save.cpp
) 

SET(HEADERS_SERVER
	#server/server.h

	server/sv_server.h
	server/sv_commands.h
	server/sv_entities.h
	server/sv_init.h
	server/sv_main.h
	server/sv_models.h
	server/sv_send.h
	server/sv_game.h
	server/sv_user.h
	server/sv_world.h
	server/sv_save.h
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
	common/modelcache.cpp
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

	common/skeletalmodels/cm_skm.cpp
	common/skeletalmodels/cm_skm_posecache.cpp
	common/skeletalmodels/cm_skm_configuration.cpp
)
SET(HEADERS_COMMON
	#common/net/chan.h
	#common/net/Q2RTXPerimentalNetChan.h
	common/net/inet_ntop.h
	common/net/inet_pton.h
	common/net/win.h

	${CMAKE_SOURCE_DIR}/inc/common/async.h
	${CMAKE_SOURCE_DIR}/inc/common/bsp.h
	${CMAKE_SOURCE_DIR}/inc/common/cmd.h
	${CMAKE_SOURCE_DIR}/inc/common/collisionmodel.h
	${CMAKE_SOURCE_DIR}/inc/common/common.h
	${CMAKE_SOURCE_DIR}/inc/common/cvar.h
	${CMAKE_SOURCE_DIR}/inc/common/error.h
	${CMAKE_SOURCE_DIR}/inc/common/field.h
	${CMAKE_SOURCE_DIR}/inc/common/fifo.h
	${CMAKE_SOURCE_DIR}/inc/common/files.h
	${CMAKE_SOURCE_DIR}/inc/common/halffloat.h
	${CMAKE_SOURCE_DIR}/inc/common/huffman.h
	${CMAKE_SOURCE_DIR}/inc/common/intreadwrite.h
	${CMAKE_SOURCE_DIR}/inc/common/math.h
	${CMAKE_SOURCE_DIR}/inc/common/mdfour.h
	${CMAKE_SOURCE_DIR}/inc/common/messaging.h
	${CMAKE_SOURCE_DIR}/inc/common/modelcache.h
	${CMAKE_SOURCE_DIR}/inc/common/prompt.h
	${CMAKE_SOURCE_DIR}/inc/common/protocol.h
	${CMAKE_SOURCE_DIR}/inc/common/sizebuf.h
	${CMAKE_SOURCE_DIR}/inc/common/tests.h
	${CMAKE_SOURCE_DIR}/inc/common/utils.h
	${CMAKE_SOURCE_DIR}/inc/common/zone.h

	${CMAKE_SOURCE_DIR}/inc/common/skeletalmodels/cm_skm.h
	${CMAKE_SOURCE_DIR}/inc/common/skeletalmodels/cm_skm_posecache.h
	${CMAKE_SOURCE_DIR}/inc/common/skeletalmodels/cm_skm_configuration.h
)


#
#
# Refresh GL:
#
#
SET(SRC_REFRESH
	refresh/images.c
	refresh/models.c
	refresh/model_sp2_json.cpp
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
	# <Q2RTXP>: WID: TODO: For some reason it won 't find them anyhow, also, list is outdated.'
	#shared/client/cl_game.h
	#shared/config.h
	#shared/config_cpp.h
	#shared/endian.h
	#shared/info_strings.h
	#shared/util/util_list.h
	#shared/platform.h
	#shared/shared.h
	#shared/shared_cpp.h
	#shared/string_utilities.h
	#inc/shared/server/sv_game.h
)
SET(SRC_SHARED
	shared/info_strings.cpp
	shared/math.cpp
	shared/shared.cpp
	shared/string_utilities.cpp
)
if (MSVC)
	# MSVC specific
	SET(SRC_SHARED
		${SRC_SHARED}
		${CMAKE_CURRENT_SOURCE_DIR}/cpp.hint
	)
endif()


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

set(SRC_TOOLS_FTEQW_IQMTOOL
	tools/fteqw-iqmtool/iqm.cpp
	tools/fteqw-iqmtool/iqm.h
	tools/fteqw-iqmtool/util.h
)