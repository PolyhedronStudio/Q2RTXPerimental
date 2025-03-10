----------------------------------------------------------------------
---- Map State Variables - These are stored for save/load game states.
----------------------------------------------------------------------
-- mapStates = {
--     -- there are none yet.
-- }
-- mapMedia = {
--     -- Filled by precaching.
--     sound = {}
-- }

----------------------------------------------------------------------
-- The Continuous Use Button.
----------------------------------------------------------------------
function ButtonLight03_OnSignalIn( self, signaller, activator, signalName, signalArguments )
    -- Turn on the light when the 'press' has finished, and we're about to be 'holding' it pressed.
    if ( signalName == "OnContinuousUnPressed" ) then
        -- Get light entity.
        local lightEntity = Game.GetEntityForTargetName( "button_light03" )
        -- Turn it off.
        Game.UseTarget( lightEntity, signaller, activator, EntityUseTargetType.SET, 0 )
        Core.DPrint( "ButtonLight03_OnSignalName " .. signalName .. " turned OFF the Light!\n" )
    elseif ( signalName == "OnContinuousPressed" ) then
        -- Get light entity.
        local lightEntity = Game.GetEntityForTargetName( "button_light03" )
        -- Turn it on.
        Game.UseTarget( lightEntity, signaller, activator, EntityUseTargetType.SET, 1 )
        Core.DPrint( "ButtonLight03_OnSignalName " .. signalName .. " turned ON the Light!\n" )
    elseif ( signalName == "OnContinuousPress" or signalName == "OnContinuousUnPress" ) then
        --Core.DPrint( "ButtonLight03_OnSignalName " .. signalName .. "\n" )
    end

    -- Finished here.
    return true
end



----------------------------------------------------------------------
----    Map CallBack Hooks:
----------------------------------------------------------------------
--[[
--  Precache all map specific related media.
]]--
function OnPrecacheMedia()
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
    Core.DPrint( "OnClientExitLevel: A client connected with entityID(#" .. clientEntity.number .. ")\n")
    --Core.DPrint( "OnClientExitLevel: A client connected with entityID(#" .. clientEntity .. ")\n")
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
