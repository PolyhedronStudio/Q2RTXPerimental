----------------------------------------------------------------------
---- Map State Variables - These are stored for save/load game states.
----------------------------------------------------------------------
mapStates = {
    -- there are none yet.
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
----
----
----    WareHouse Locked Doors Implementations:
----
----
----------------------------------------------------------------------
----------------------------------------------------------------------
-- The Buttons
----------------------------------------------------------------------
function ButtonLock00_OnSignalIn( self, signaller, activator, signalName, signalArguments )
    -- Open the doors.
    if ( signalName == "OnPressed" ) then
        -- Get entity
        local entDoor = Game.GetEntityForTargetName( "door_usetargets_locking" )
        -- Determine its move state.
        local moveState = Game.GetPushMoverState( entDoor )
        -- Of course we can't lock a moving door. For showing off feature wise, we also allow for locking it in its opened state.
        if ( moveState ~= PUSHMOVE_STATE_MOVING_DOWN and moveState ~= PUSHMOVE_STATE_MOVING_UP ) then
            -- Signal the "DoorLockToggle" event.
            Game.SignalOut( entDoor, self, activator, "DoorLockToggle" )
        end
    end
    return true
end

function ButtonLockTeamPairDoors_OnSignalIn( self, signaller, activator, signalName, signalArguments )
    -- Open the doors.
    if ( signalName == "OnPressed" ) then
        -- Get entity
        local entsDoors = Game.GetEntitiesForTargetName( "door_usetargets_team" )
        
        -- No results at all.
        if ( type( entsDoors ) ~= "table" )
            return false
        end
        if ( #entsDoors >= 1 ) then
            --Core.DPrint( "entsDoors = " .. type(entsDoors) .. "value = "..entsDoors.."\n")
            DEBUG_ITERATE_TABLE( entsDoors )
            for keyIndex,entDoor in pairs(entsDoors) do
                -- Determine its move state.
                local moveState = Game.GetPushMoverState( entDoor )
                -- Of course we can't lock a moving door. For showing off feature wise, we also allow for locking it in its opened state.
                if ( moveState ~= PUSHMOVE_STATE_MOVING_DOWN and moveState ~= PUSHMOVE_STATE_MOVING_UP ) then
                    -- Signal the "DoorLockToggle" event.
                    Game.SignalOut( entDoor, self, activator, "DoorLockToggle" )
                end
            end
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
