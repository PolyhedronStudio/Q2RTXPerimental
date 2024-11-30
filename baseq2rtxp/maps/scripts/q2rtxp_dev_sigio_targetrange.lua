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
    --[[
    -- Handles its death, turns off the train track and the light.
    ]]--
    if ( signalName == "OnPain" ) then
        -- Play speciual 'pain' sound effect.
        Media.Sound( self, CHAN_VOICE, mapMedia.sound.rangetarget_pain, 1.0, ATTN_NORM, 0.0 )
        -- Done handling signal.
        return
    -- It just got killed, stop the train track, notify, and add score.
    elseif ( signalName == "OnKilled" ) then
        -- Notify of the kill.
        Core.DPrint( onKilledMessage ) -- TODO: Use actual prints, not developer prints :-P
        -- Stop the train for this target.
        Game.UseTarget( Game.GetEntityForTargetName( trainTargetName ), signaller, activator, ENTITY_USETARGET_TYPE_OFF, 0 )
        -- Play speciual 'opening' sound effect.
        Media.Sound( self, CHAN_VOICE, mapMedia.sound.rangetarget_open, 1.0, ATTN_IDLE, 0.0 )
        -- Done handling signal.
        return
    -- It finished the 'open movement' and arrived at destination:
    elseif ( signalName == "OnOpened" ) then
        -- Turn off the light for this target.
        Game.UseTarget( Game.GetEntityForTargetName( lightTargetName ), signaller, activator, ENTITY_USETARGET_TYPE_OFF, 0 )
        -- Done handling signal.
        return
    --[[
    -- If the target is engaging in "Closing", it means a new range round has begun.
    -- The following turns on the light, revives the 'door', and reactivates he train
    -- track when it has reached 'closed' state.
    ]]--
    -- A new round has begun, so the target got told to "close", turn on the light so we can see it close.
    elseif ( signalName == "OnClose" ) then
        -- Turn on the light for this target.
        Game.UseTarget( Game.GetEntityForTargetName( lightTargetName ), signaller, activator, ENTITY_USETARGET_TYPE_ON, 1 )
        -- Play speciual closing sound effect.
        Media.Sound( self, CHAN_VOICE, mapMedia.sound.rangetarget_close, 1.0, ATTN_IDLE, 0.0 )
        -- Done handling signal.
        return
    -- A new round has begun, so the target got told to "close", turn on the train.
    elseif ( signalName == "OnClosed") then
        -- Turn on the train for this target.
        Game.UseTarget( Game.GetEntityForTargetName( trainTargetName ), signaller, activator, ENTITY_USETARGET_TYPE_ON, 1 )
        -- Done handling signal.
        return
    elseif ( signalName == "OnOpen" ) then
        -- Do nothing here.
    --elseif ( signalName == "DoorClose") then
    --elseif ( signalName == "DoorOpen") then
    else
        -- Nothing.
    end
end

----------------------------------------------------------------------
----    Range Target(s) Implementations:
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
-- S Target:
function Target_OnSignalIn( self, signaller, activator, signalName, signalArguments )
    Target_ProcessSignals( self, signaller, activator, signalName, signalArguments, "Killed the S target!\n", "t_target", "train_target", "light_target" )
end

-- Button that resets the range.
function button_toggle_targetrange_OnSignalIn( self, signaller, activator, signalName, signalArguments )
    if ( signalName == "OnPressed" ) then
        Core.DPrint( "Range targets reset!\n" ) -- TODO: Use not developer print...
        -- Play speciual 'restart' sound effect.
        Media.Sound( self, CHAN_ITEM, mapMedia.sound.newround, 1.0, ATTN_IDLE, 0.0 )

        -- Signal all targetrange lane targets to "close" again, effectively reactivating our target range course.
        Game.SignalOut( Game.GetEntityForTargetName( "t_target_xxl" ), signaller, activator, "DoorClose", {} )
        Game.SignalOut( Game.GetEntityForTargetName( "t_target_xl" ), signaller, activator, "DoorClose", {} )
        Game.SignalOut( Game.GetEntityForTargetName( "t_target_l" ), signaller, activator, "DoorClose", {} )
        Game.SignalOut( Game.GetEntityForTargetName( "t_target" ), signaller, activator, "DoorClose", {} )
    end
end



----------------------------------------------------------------------
-- WareHouse Locked Doors Implementations:
----------------------------------------------------------------------
----------------------------------------------------------------------
-- The Buttons
----------------------------------------------------------------------
function WareHouseLockingButton_OnSignalIn( xself, signaller, activator, signalName, signalArguments )
    Core.DPrint( "xself(#"..xself.number..")\n")
    Core.DPrint( "signaller(#"..signaller.number..")\n")
    Core.DPrint( "activator(#"..activator.number..")\n")

    -- Open the doors.
    if ( signalName == "OnPressed" or signalName == "OnUnPressed" ) then
        -- Get entity
        local entityWareHouseDoor00 = Game.GetEntityForLuaName( "WareHouseDoor00" )
        Core.DPrint( "entityWareHouseDoor00(#"..entityWareHouseDoor00.number..")\n")

        -- Determine its move state.
        local doorMoveState = Game.GetPushMoverState( entityWareHouseDoor00 )

        -- Only SignalOut a "DoorOpen" when the elevator is NOT moving.
        if ( doorMoveState ~= PUSHMOVE_STATE_MOVING_DOWN and doorMoveState~= PUSHMOVE_STATE_MOVING_UP ) then
            -- Send the lock toggle signal.
            Game.SignalOut( entityWareHouseDoor00, xself, activator, "DoorLockToggle", {} )
        end
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
