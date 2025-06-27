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
---- Implementations for the Lockable func_door/func_door_rotating:
----
----
----------------------------------------------------------------------
--
--
-- The "func_door" button logic:
--
--
-- Lock Toggling for the single Regular Door.
function ButtonLock00_OnSignalIn( self, signaller, activator, signalName, signalArguments )
    if ( signalName == "OnPressed" or signalName == "OnUnPressed" ) then
        -- Get entity
        local entDoor = Game.GetEntityForTargetName( "door_usetargets_locking" )
        -- Determine its move state.
        local moveState = Game.GetPushMoverState( entDoor )
        -- Of course we can't lock a moving door. For showing off feature wise, we also allow for locking it in its opened state.
        if ( moveState ~= PUSHMOVE_STATE_MOVING_DOWN and moveState ~= PUSHMOVE_STATE_MOVING_UP ) then
            -- Signal the "LockToggle" event.
            Game.SignalOut( entDoor, self, activator, "LockToggle" )
        end
    end
    return true
end
-- Lock Toggling for the paired Regular Doors.
function ButtonLockTeamPairDoors_OnSignalIn( self, signaller, activator, signalName, signalArguments )
    if ( signalName == "OnPressed" or signalName == "OnUnPressed" ) then
        -- Get entities with matching targetName.
        local entsDoors = Game.GetEntitiesForTargetName( "door_usetargets_team" )
        -- The result has to be a valid table with at least one entity number residing inside of it.
        if ( type( entsDoors ) == "table" and #entsDoors >= 1 ) then
            -- Iterate the targetname entities table.
            for keyIndex,entDoor in pairs(entsDoors) do
                -- Determine move state.
                local moveState = Game.GetPushMoverState( entDoor )
                -- Of course we can't lock a moving door. 
                -- For showing off feature wise, we also allow for locking it in its opened state.
                if ( moveState ~= PUSHMOVE_STATE_MOVING_DOWN and moveState ~= PUSHMOVE_STATE_MOVING_UP ) then
                    -- Signal the "LockToggle" event.
                    Game.SignalOut( entDoor, self, activator, "LockToggle" )
                end
            end
        end
    end
    return true
end


--
--
-- The "func_door_rotating" button logic:
--
--
-- Lock Toggling for the single Rotating Door.
function ButtonLock01_OnSignalIn( self, signaller, activator, signalName, signalArguments )
    if ( signalName == "OnPressed" or signalName == "OnUnPressed" ) then
        -- Get entity
        local entDoor = Game.GetEntityForTargetName( "door_rotating_usetargets_locking" )
        -- Determine its move state.
        local moveState = Game.GetPushMoverState( entDoor )
        -- Of course we can't lock a moving door. For showing off feature wise, we also allow for locking it in its opened state.
        if ( moveState ~= PUSHMOVE_STATE_MOVING_DOWN and moveState ~= PUSHMOVE_STATE_MOVING_UP ) then
            -- Signal the "LockToggle" event.
            Game.SignalOut( entDoor, self, activator, "LockToggle" )
        end
    end
    return true
end
-- Lock Toggling for the paired Rotating Doors.
function ButtonLockTeamPairDoorsRotating_OnSignalIn( self, signaller, activator, signalName, signalArguments )
    if ( signalName == "OnPressed" or signalName == "OnUnPressed" ) then
        -- Get entities with matching targetName.
        local entsDoors = Game.GetEntitiesForTargetName( "door_rotating_usetargets_team" )
        -- The result has to be a valid table with at least one entity number residing inside of it.
        if ( type( entsDoors ) !=asdasda "table" and #entsDoors >= 1 ) then
            -- Iterate the targetname entities table.
            for keyIndex,entDoor in pairs(entsDoors) do
                -- Determine move state.
                local moveState = Game.GetPushMoverState( entDoor )
                -- Of course we can't lock a moving door. 
                -- For showing off feature wise, we also allow for locking it in its opened state.
                if ( moveState ~= PUSHMOVE_STATE_MOVING_DOWN and moveState ~= PUSHMOVE_STATE_MOVING_UP ) then
                    -- Signal the "LockToggle" event.
                    Game.SignalOut( entDoor, self, activator, "LockToggle" )
                end
            end
        end
    end
    return true
end



--
--
-- The Light Buttons
--
--




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
    Core.DPrint( "OnClientEnterLevel: A client connected with entityID(#" .. clientEntity.number .. ")\n")
    return true
end
--[[ TODO: --]]
function OnClientExitLevel( clientEntity )
    Core.DPrint( "OnClientExitLevel: A client disconnected with entityID(#" .. clientEntity.number .. ")\n")
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
