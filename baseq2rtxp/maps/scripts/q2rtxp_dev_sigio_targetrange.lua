----------------------------------------------------------------------
---- Map State Variables - These are stored for save/load game states.
----------------------------------------------------------------------
mapStates = {
    -- there are none yet.
    targetRange = {
        targetsAlive = 0,   -- How many targets are still alive and kicking?
        roundActive = false    -- True when the targetrange has been (re-)started and actively moving the targets.
    }
}
mapMedia = {
    -- Filled by precaching.
    sound = {}
}
-- Debug Function, iterates the signalArguments table.
function DEBUG_ITERATE_TABLE( table )
    if ( type( table ) == "table" ) then
        Core.DPrint( "  DEBUG_ITERATE_TABLE: table = {\n" )
        for key, value in pairs(table) do
            Core.DPrint( "      [" .. key .. "] => [" .. value .. "]\n" )
        end
        Core.DPrint( "}\n" )
    else
    --    Core.DPrint( "DEBUG_ITERATE_TABLE: NO TABLE FOUND\n" )
    end
end
----------------------------------------------------------------------
-- Target Logic:
----------------------------------------------------------------------
-- L Target:
function Target_ProcessSignals( self, signaller, activator, signalName, signalArguments, displayName, targetName, trainTargetName, lightTargetName, lightBrushTargetName )
    --[[
    -- Handles its death, turns off the train track and the light.
    ]]--
    if ( signalName == "OnPain" ) then
        -- Play speciual 'pain' sound effect.
        Media.Sound( self, SoundChannel.VOICE, mapMedia.sound.rangetarget_pain, 1.0, SoundAttenuation.NORMAL, 0.0 )
        -- Done handling signal.
        return true
    -- It just got killed, stop the train track, notify, and add score.
    elseif ( signalName == "OnKilled" ) then
        -- Notify of the kill.
        Game.Print( PrintLevel.NOTICE, "Shot down the \"" .. displayName .. "\" target! Only #".. mapStates.targetRange.targetsAlive .. " targets remaining!\n" )

        -- Decrement number of targets alive count, only if we're the team master that is being signalled.
        if ( self.teamMaster == self.targetName ) then
            mapStates.targetRange.targetsAlive = mapStates.targetRange.targetsAlive - 1
        end

        -- Stop the train for this target.
        Game.UseTarget( Game.GetEntityForTargetName( trainTargetName ), signaller, activator, EntityUseTarget.OFF, 0 )
        
        -- Play special 'opening' sound effect.
        Media.Sound( self, SoundChannel.VOICE, mapMedia.sound.rangetarget_open, 1.0, SoundAttenuation.IDLE, 0.0 )

        -- Change texture of 'animslight' its frame to second, so it turns red indicating this target is dead.
        -- Get LightBrush Entity.
        local lightBrushEntity = Game.GetEntityForTargetName( lightBrushTargetName )
        -- Change texture of 'animslight' its frame to first, so it turns white-ish indicating this target is killable.
        lightBrushEntity.state.frame = 1

        -- Turn on the light for this target.
        --Game.UseTarget( lightBrushEntity, signaller, activator, EntityUseTarget.ON, 1 )

        -- Turn on all lights for the target range if we killed all 4 targets.
        if ( mapStates.targetRange.targetsAlive <= 0 ) then
            -- Turn on all lights for the target range.
            local targetRangeLights = Game.GetEntitiesForTargetName( "light_ceil_range" )
            for targetRangeLightKey,targetRangeLight in pairs(targetRangeLights) do
                -- Turn on the light for this target.
                Core.DPrint( "UseTarget on light entity #".. targetRangeLight.number .. ", EntityUseTarget.ON\n" )
                Game.UseTarget( targetRangeLight, self, activator, EntityUseTarget.ON, 1 )
            end

            -- Switch off the target lights. (Round ended.)
            Game.UseTargets( Game.GetEntityForTargetName( "light_target_xxl" ), signaller, activator, EntityUseTarget.OFF, 0 )
            Game.UseTargets( Game.GetEntityForTargetName( "light_target_xl" ), signaller, activator, EntityUseTarget.OFF, 0 )
            Game.UseTargets( Game.GetEntityForTargetName( "light_target_l" ), signaller, activator, EntityUseTarget.OFF, 0 )
            Game.UseTargets( Game.GetEntityForTargetName( "light_target" ), signaller, activator, EntityUseTarget.OFF, 0 )

            -- The target range isn't in an active round anymore.
            mapStates.targetRange.roundActive = false
            mapStates.targetRange.targetsAlive = 0
        end
        -- Done handling signal.
        return true
    -- It finished the 'open movement' and arrived at destination:
    elseif ( signalName == "OnOpened" ) then
        -- Turn off the light for this target.
        --Game.UseTarget( Game.GetEntityForTargetName( lightTargetName ), signaller, activator, EntityUseTarget.OFF, 0 )
        -- Done handling signal.
        return true
    --[[
    -- If the target is engaging in "Closing", it means a new range round has begun.
    -- The following turns on the light, revives the 'door', and reactivates he train
    -- track when it has reached 'closed' state.
    ]]--
    -- A new round has begun, so the target got told to "close", turn on the light so we can see it close.
    elseif ( signalName == "OnClose" ) then
        -- Turn on the light for this target.
        Game.UseTarget( Game.GetEntityForTargetName( lightTargetName ), signaller, activator, EntityUseTarget.ON, 1 )
        -- Play speciual closing sound effect.
        Media.Sound( self, SoundChannel.VOICE, mapMedia.sound.rangetarget_close, 1.0, SoundAttenuation.IDLE, 0.0 )
        -- Done handling signal.
        return true
    -- A new round has begun, so the target got told to "close", turn on the train.
    elseif ( signalName == "OnClosed") then
        -- Get LightBrush Entity.
        local lightBrushEntity = Game.GetEntityForTargetName( lightBrushTargetName )
        -- Change texture of 'animslight' its frame to first, so it turns white-ish indicating this target is killable.
        lightBrushEntity.state.frame = 0
        -- Turn off the light for this target.
        --Game.UseTarget( lightBrushEntity, signaller, activator, EntityUseTarget.OFF, 0 )

        -- Let it glow? lol.
        self.state.effects = EntityEffects.COLOR_SHELL
        self.state.renderFx = RenderFx.SHELL_GREEN

        -- Turn on the train for this target.
        Game.UseTarget( Game.GetEntityForTargetName( trainTargetName ), signaller, activator, EntityUseTarget.ON, 1 )
        -- Done handling signal.
        return true
    elseif ( signalName == "OnOpen" ) then
        -- Do nothing here.
    --elseif ( signalName == "DoorClose") then
    --elseif ( signalName == "DoorOpen") then
    else
        -- Nothing.
    end
    -- Done handling signal.
    return true
end

----------------------------------------------------------------------
----    Range Target(s) Implementations:
----------------------------------------------------------------------
-- XXL Target:
function TargetXXL_OnSignalIn( self, signaller, activator, signalName, signalArguments )
    return Target_ProcessSignals( self, signaller, activator, signalName, signalArguments, "XXL", "t_target_xxl", "train_target_xxl", "light_target_xxl", "light_brush_target_xxl" )
end
-- XL Target:
function TargetXL_OnSignalIn( self, signaller, activator, signalName, signalArguments )
    return Target_ProcessSignals( self, signaller, activator, signalName, signalArguments, "XL", "t_target_xl", "train_target_xl", "light_target_xl", "light_brush_target_xl" )
end
-- L Target:
function TargetL_OnSignalIn( self, signaller, activator, signalName, signalArguments )
    return Target_ProcessSignals( self, signaller, activator, signalName, signalArguments, "L", "t_target_l", "train_target_l", "light_target_l", "light_brush_target_l" )
end
-- S Target:
function Target_OnSignalIn( self, signaller, activator, signalName, signalArguments )
    return Target_ProcessSignals( self, signaller, activator, signalName, signalArguments, "S", "t_target", "train_target", "light_target", "light_brush_target_s" )
end

-- Button that resets the range.
function button_toggle_targetrange_OnSignalIn( self, signaller, activator, signalName, signalArguments )
    if ( signalName == "OnPressed" ) then
        -- Multiple targets left:
        if ( mapStates.targetRange.targetsAlive > 1 ) then
            Game.Print( PrintLevel.NOTICE, "Round Active, #" .. mapStates.targetRange.targetsAlive .. " targets remaining!\n" )
        -- One target left:
        elseif ( mapStates.targetRange.targetsAlive == 1 ) then
            Game.Print( PrintLevel.NOTICE, "Round Active, last target remaining!\n" )
        -- Proceed (re-)activating range:
        else
            Game.Print( PrintLevel.NOTICE, "New Round Started! Kill all 4 targets as quickly as you can!\n" )

            -- Play speciual 'restart' sound effect.
            Media.Sound( signaller, SoundChannel.ITEM, mapMedia.sound.newround, 1.0, SoundAttenuation.NORMAL, 0.0 )

            -- Signal all targetrange lane targets to "close" again, effectively reactivating our target range course.
            Game.SignalOut( Game.GetEntityForTargetName( "t_target_xxl" ), signaller, activator, "DoorClose", {} )
            Game.SignalOut( Game.GetEntityForTargetName( "t_target_xl" ), signaller, activator, "DoorClose", {} )
            Game.SignalOut( Game.GetEntityForTargetName( "t_target_l" ), signaller, activator, "DoorClose", {} )
            Game.SignalOut( Game.GetEntityForTargetName( "t_target" ), signaller, activator, "DoorClose", {} )

            -- Engage all func_train into moving again.
            Game.UseTargets( Game.GetEntityForTargetName( "train_target_xxl" ), signaller, activator, EntityUseTarget.ON, 1 )
            Game.UseTargets( Game.GetEntityForTargetName( "train_target_xl" ), signaller, activator, EntityUseTarget.ON, 1 )
            Game.UseTargets( Game.GetEntityForTargetName( "train_target_l" ), signaller, activator, EntityUseTarget.ON, 1 )
            Game.UseTargets( Game.GetEntityForTargetName( "train_target" ), signaller, activator, EntityUseTarget.ON, 1 )

            -- Switch on the target lights. (New round has begun.)
            Game.UseTargets( Game.GetEntityForTargetName( "light_target_xxl" ), signaller, activator, EntityUseTarget.ON, 1 )
            Game.UseTargets( Game.GetEntityForTargetName( "light_target_xl" ), signaller, activator, EntityUseTarget.ON, 1 )
            Game.UseTargets( Game.GetEntityForTargetName( "light_target_l" ), signaller, activator, EntityUseTarget.ON, 1 )
            Game.UseTargets( Game.GetEntityForTargetName( "light_target" ), signaller, activator, EntityUseTarget.ON, 1 )

            -- Reset targets alive count.
            mapStates.targetRange.targetsAlive = 4
            -- Reset round into active mode.
            mapStates.targetRange.roundActive = true

            -- Turn off all lights for the target range.
            local targetRangeLights = Game.GetEntitiesForTargetName( "light_ceil_range" )
            for targetRangeLightKey,targetRangeLight in pairs(targetRangeLights) do
                -- Turn off the light for this target.
                Core.DPrint( "UseTarget on light entity #".. targetRangeLight.number .. " EntityUseTarget.OFF\n" )
                Game.UseTarget( targetRangeLight, self, activator, EntityUseTarget.OFF, 0 )
            end
        end
    end
    return true
end



----------------------------------------------------------------------
-- WareHouse Locked Doors Implementations:
----------------------------------------------------------------------
----------------------------------------------------------------------
-- The Buttons
----------------------------------------------------------------------
function WareHouseLockingButton_OnSignalIn( self, signaller, activator, signalName, signalArguments )
    -- Open the doors.
    if ( signalName == "OnPressed" or signalName == "OnUnPressed" ) then
        -- Get entity
        local entityWareHouseDoor00 = Game.GetEntityForLuaName( "WareHouseDoor00" )
        Core.DPrint( "entityWareHouseDoor00(#"..entityWareHouseDoor00.number..")\n")

        -- Determine its move state.
        local doorMoveState = Game.GetPushMoverState( entityWareHouseDoor00 )

        -- Only SignalOut a "DoorOpen" when the elevator is NOT moving.
        if ( doorMoveState ~= PushMoveState.MOVING_DOWN and doorMoveState ~= PushMoveState.MOVING_UP ) then
            -- Send the lock toggle signal.
            Game.SignalOut( entityWareHouseDoor00, self, activator, "DoorLockToggle", {} )
        end
    end
    return true
end

----------------------------------------------------------------------
-- The Doors
----------------------------------------------------------------------
function WareHouseDoor00_OnSignalIn( self, signaller, activator, signalName, signalArguments )
    -- Open the doors.
    if ( signalName == "OnPressed" or signalName == "OnUnPressed" ) then
        -- -- Get entity
        -- local entityWareHouseDoor01 = Game.GetEntityForLuaName( "WareHouseDoor01" )
        -- -- Determine its move state.
        -- local doorMoveState = Game.GetPushMoverState( entityWareHouseDoor01 )

        -- -- Only SignalOut a "DoorOpen" when the elevator is NOT moving.
        -- if ( doorMoveState ~= PUSHMOVE_STATE_MOVING_DOWN and doorMoveState~= PUSHMOVE_STATE_MOVING_UP ) then
        --     -- Send the lock toggle signal.
        --     Game.SignalOut( entityWareHouseDoor01, self, activator, "DoorLockToggle" )
        -- end
    end
    return true
end



----------------------------------------------------------------------
----    Map CallBack Hooks:
----------------------------------------------------------------------
--[[
--  Precache all map specific related media.
]]--
function OnPrecacheMedia()
    mapMedia.sound = {
        rangetarget_close = Media.PrecacheSound( "maps/targetrange/rangetarget_close.wav" ),
        rangetarget_open = Media.PrecacheSound( "maps/targetrange/rangetarget_open.wav" ),
        rangetarget_pain = Media.PrecacheSound( "maps/targetrange/rangetarget_pain.wav" ),

        newround = Media.PrecacheSound( "maps/targetrange/newround.wav" )
    }
    return true
end
--[[ TODO: --]]
function OnBeginMap()
    return true
end
--[[ TODO: --]]
function OnExitMap()
    return true
end



----------------------------------------------------------------------
----
----
----    Client CallBack Hooks:
----
----
----------------------------------------------------------------------
--[[ TODO: --]]
function OnClientEnterLevel( clientEntity )
    Core.DPrint( "OnClientEnterLevel: A client connected with entityID(#" .. clientEntity.number .. ")\n")
    --Core.DPrint( "OnClientEnterLevel: A client connected with entityID(#" .. clientEntity .. ")\n")
    return true
end
--[[ TODO: --]]
function OnClientExitLevel( clientEntity )
    Core.DPrint( "OnClientEnterLevel: A client disconnected with entityID(#" .. clientEntity.number .. ")\n")
    --Core.DPrint( "OnClientEnterLevel: A client disconnected with entityID(#" .. clientEntity .. ")\n")
    return true
end



----------------------------------------------------------------------
----
----
----    Frame CallBack Hooks:
----
----
----------------------------------------------------------------------
--[[ TODO: --]]
function OnBeginServerFrame()
    return true
end
--[[ TODO: --]]
function OnEndServerFrame()
    return true
end
--[[ TODO: --]]
function OnRunFrame( frameNumber )
    --Core.DPrint( "framenumber = " .. frameNumber .. "\n" )
    return true
end
