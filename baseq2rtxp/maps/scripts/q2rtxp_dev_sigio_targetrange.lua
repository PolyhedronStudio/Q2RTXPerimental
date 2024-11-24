----------------------------------------------------------------------
---- Map State Variables - These are stored for save/load game states.
----------------------------------------------------------------------
mapStates = {
    -- there are none yet.
}
mapMedia = {
    -- Filled by precaching.
    sound = {}
}

----------------------------------------------------------------------
-- Target Logic:
----------------------------------------------------------------------
-- L Target:
function Target_ProcessSignals( self, signaller, activator, signalName, signalArguments, onKilledMessage, targetName, trainTargetName, lightTargetName )
    local entityUseType = ""
    local entityUseValue = 0
    local entityTargetName = ""

    --[[
    -- Handles its death, turns off the train track and the light.
    ]]--
    if ( signalName == "OnPain" ) then
        -- Play speciual 'opening' sound effect.
        local res = Media.Sound( self, CHAN_VOICE, mapMedia.sound.rangetarget_pain, 1.0, ATTN_NORM, 0.0 )
        Core.DPrint( "res="..res.."\n" )
        Core.DPrint( "OnPain! kick="..signalArguments["kick"].." damage="..signalArguments["damage"].."\n" )
        return
    -- It just got killed, stop the train track, notify, and add score.
    elseif ( signalName == "OnKilled" ) then
        -- Notify of the kill.
        Core.DPrint( onKilledMessage )
        -- Stop the train for this target.
        localentityUseType = "ENTITY_USETARGET_TYPE_OFF"
        entityUseValue = 0
        entityTargetName = trainTargetName
        Game.UseTarget( Game.GetEntityForTargetName( trainTargetName ), signaller, activator, ENTITY_USETARGET_TYPE_OFF, 0 )
        -- Play speciual 'opening' sound effect.
        Media.Sound( self, CHAN_VOICE, mapMedia.sound.rangetarget_open, 1.0, ATTN_IDLE, 0.0 )
        return
    -- It finished the 'open movement' and arrived at destination:
    elseif ( signalName == "OnOpened" ) then
        -- Turn off the light for this target.
        entityUseType = "ENTITY_USETARGET_TYPE_OFF"
        entityUseValue = 0
        entityTargetName = lightTargetName
        Game.UseTarget( Game.GetEntityForTargetName( lightTargetName ), signaller, activator, ENTITY_USETARGET_TYPE_OFF, 0 )
        return
    --[[
    -- If the target is engaging in "Closing", it means a new range round has begun.
    -- The following turns on the light, revives the 'door', and reactivates he train
    -- track when it has reached 'closed' state.
    ]]--
    -- A new round has begun, so the target got told to "close", turn on the light so we can see it close.
    elseif ( signalName == "OnClose" ) then
        -- Turn on the light for this target.
        entityUseType = "ENTITY_USETARGET_TYPE_ON"
        entityUseValue = 1
        entityTargetName = lightTargetName
        Game.UseTarget( Game.GetEntityForTargetName( lightTargetName ), signaller, activator, ENTITY_USETARGET_TYPE_ON, 1 )
        -- Play speciual closing sound effect.
        Media.Sound( self, CHAN_VOICE, mapMedia.sound.rangetarget_close, 1.0, ATTN_IDLE, 0.0 )
        return
    -- A new round has begun, so the target got told to "close", turn on the train.
    elseif ( signalName == "OnClosed") then
        -- Turn on the train for this target.
        entityUseType = "ENTITY_USETARGET_TYPE_ON"
        entityUseValue = 1
        entityTargetName = trainTargetName
        Game.UseTarget( Game.GetEntityForTargetName( trainTargetName ), signaller, activator, ENTITY_USETARGET_TYPE_ON, 1 )
        return
    --elseif ( signalName == "DoorClose") then
    --elseif ( signalName == "DoorOpen") then
    end

    -- Debugging
    if ( type(signalName) == "string" and signalName ~= "" ) then
        Core.DPrint( "[Unhandled:"..signalName.."] Game.GetEntityForTargetName("..entityTargetName.."), signaller(#"..signaller.."), activator(#"..activator.."), "..entityUseType..","..entityUseValue.."\n" )
    else
        Core.DPrint( "[Unhandled and InvalidSignalType] Game.GetEntityForTargetName("..entityTargetName.."), signaller(#"..signaller.."), activator(#"..activator.."), "..entityUseType..","..entityUseValue.."\n" )
    end
end

----------------------------------------------------------------------
----
----
----    Target(s) Range Implementations:
----
----
----------------------------------------------------------------------
-- XXL Target:
function TargetXXL_OnSignalIn( self, signaller, activator, signalName, signalArguments )
    Target_ProcessSignals( self, signaller, activator, signalName, signalArguments, "Killed the XXL target!\n", "t_target_xxl", "train_target_xxl", "light_target_xxl" )
end
-- XL Target:
function TargetXL_OnSignalIn( self, signaller, activator, signalName, signalArguments )
    Target_ProcessSignals( self, signaller, activator, signalName, signalArguments, "Killed the XL target!\n", "t_target_xl", "train_target_xl", "light_target_xl" )
end
-- L Target:
function TargetL_OnSignalIn( self, signaller, activator, signalName, signalArguments )
    Target_ProcessSignals( self, signaller, activator, signalName, signalArguments, "Killed the L target!\n", "t_target_l", "train_target_l", "light_target_l" )
end
-- Target:
function Target_OnSignalIn( self, signaller, activator, signalName, signalArguments )
    Target_ProcessSignals(self, signaller, activator, signalName, signalArguments, "Killed the S target!\n", "t_target", "train_target", "light_target" )
end

-- Button that Toggles On/Off
function button_toggle_targetrange_OnSignalIn( self, signaller, activator, signalName, signalArguments )
    if ( signalName == "OnPressed" ) then
        Core.DPrint( "Pressed toggle target range button\n" )

        -- -- Turn back on the train tracks for moving the targets with.
        Game.UseTarget( Game.GetEntityForTargetName( "train_target_xxl" ), signaller, activator, ENTITY_USETARGET_TYPE_ON, 1 )
        Game.UseTarget( Game.GetEntityForTargetName( "train_target_xl" ), signaller, activator, ENTITY_USETARGET_TYPE_ON, 1 )
        Game.UseTarget( Game.GetEntityForTargetName( "train_target_l" ), signaller, activator, ENTITY_USETARGET_TYPE_ON, 1 )
        Game.UseTarget( Game.GetEntityForTargetName( "train_target" ), signaller, activator, ENTITY_USETARGET_TYPE_ON, 1 )

        -- Signal all targetrange lane targets to "close" again, effectively reactivating our target range course.
        Game.SignalOut( Game.GetEntityForTargetName( "t_target_xxl" ), signaller, activator, "DoorClose" )
        Game.SignalOut( Game.GetEntityForTargetName( "t_target_xl" ), signaller, activator, "DoorClose" )
        Game.SignalOut( Game.GetEntityForTargetName( "t_target_l" ), signaller, activator, "DoorClose" )
        Game.SignalOut( Game.GetEntityForTargetName( "t_target" ), signaller, activator, "DoorClose" )

        -- -- Turn on all the lights again.
        Game.UseTarget( Game.GetEntityForTargetName( "light_target_xxl" ), signaller, activator, ENTITY_USETARGET_TYPE_ON, 1 )
        Game.UseTarget( Game.GetEntityForTargetName( "light_target_xl" ), signaller, activator, ENTITY_USETARGET_TYPE_ON, 1 )
        Game.UseTarget( Game.GetEntityForTargetName( "light_target_l" ), signaller, activator, ENTITY_USETARGET_TYPE_ON, 1 )
        Game.UseTarget( Game.GetEntityForTargetName( "light_target" ), signaller, activator, ENTITY_USETARGET_TYPE_ON, 1 )
    end
end



----------------------------------------------------------------------
----
----
----    WareHouse Locked Doors Implementations:
----
----
----------------------------------------------------------------------
----------------------------------------------------------------------
-- The Buttons
----------------------------------------------------------------------
function WareHouseLockingButton_OnSignalIn( self, signaller, activator, signalName, signalArguments )
    -- Open the doors.
    if ( signalName == "OnPressed" or signalName == "OnUnPressed" ) then
        -- Get entity
        local entityWareHouseDoor01 = Game.GetEntityForLuaName( "WareHouseDoor01" )
        -- Determine its move state.
        local doorMoveState = Game.GetPushMoverState( entityWareHouseDoor01 )

        -- Only SignalOut a "DoorOpen" when the elevator is NOT moving.
        if ( doorMoveState ~= PUSHMOVE_STATE_MOVING_DOWN and doorMoveState~= PUSHMOVE_STATE_MOVING_UP ) then
            -- Send the lock toggle signal.
            Game.SignalOut( entityWareHouseDoor01, self, activator, "DoorLockToggle" )
        end
    end
    return true
end




----------------------------------------------------------------------
----
----
----    Map CallBack Hooks:
----
----
----------------------------------------------------------------------
--[[
--  Precache all map specific related media.
]]--
function OnPrecacheMedia()
    mapMedia.sound = {
        rangetarget_close = Media.PrecacheSound( "maps/targetrange/rangetarget_close.wav" ),
        rangetarget_open = Media.PrecacheSound( "maps/targetrange/rangetarget_open.wav" ),
        rangetarget_pain = Media.PrecacheSound( "maps/targetrange/rangetarget_pain.wav" )
    }
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
    Core.DPrint( "OnClientEnterLevel: A client connected with entityID(#" .. clientEntity .. ")\n")
    return true
end
--[[ TODO: --]]
function OnClientExitLevel( clientEntity )
    Core.DPrint( "OnClientEnterLevel: A client disconnected with entityID(#" .. clientEntity .. ")\n")
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
   return true
end
