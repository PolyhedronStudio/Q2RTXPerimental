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
----    HallWay Elevator Implementations:
----
----
----------------------------------------------------------------------
----------------------------------------------------------------------
-- The Buttons
----------------------------------------------------------------------
function HallElevatorButtonOutside_OnSignalIn( self, signaller, activator, signalName, signalArguments )
    -- Open the doors.
    if ( signalName == "OnPressed" ) then
        -- Determine its move state.
        local elevatorMoveState = Game.GetPushMoverState( Game.GetEntityForLuaName( "HallElevator" ) )
        -- Only SignalOut a "Open" when the elevator is NOT moving.
        if ( elevatorMoveState ~= PUSHMOVE_STATE_MOVING_DOWN and elevatorMoveState ~= PUSHMOVE_STATE_MOVING_UP ) then
            -- Send open signal, as a toggle, so it remains opened  until further actioning.
            Game.SignalOut( Game.GetEntityForLuaName( "HallElevatorDoor00" ), self, activator, "Open", { testInteger = 10, testString = "Hello World!", testNumber = 13.37 } )
        end
    end
    return true
end
function HallElevatorButtonInside_OnSignalIn( self, signaller, activator, signalName, signalArguments )
    -- Close the doors.
    if ( signalName == "OnPressed" ) then
        -- Get elevator and door move states.
        local elevatorMoveState = Game.GetPushMoverState( Game.GetEntityForLuaName( "HallElevator" ) )
        local doorMoveState = Game.GetPushMoverState( Game.GetEntityForLuaName( "HallElevatorDoor00" ) )
        -- Only fire SignalOuts for closing/openingthe door IF the elevator is NOT moving.
        if ( elevatorMoveState == PUSHMOVE_STATE_TOP or elevatorMoveState == PUSHMOVE_STATE_BOTTOM ) then
            -- Open the door if closed:
            if ( doorMoveState == PUSHMOVE_STATE_MOVING_DOWN or doorMoveState == PUSHMOVE_STATE_BOTTOM ) then
                -- Send close signal, as a toggle, so it remains closed until further actioning.
                Game.SignalOut( Game.GetEntityForLuaName( "HallElevatorDoor00" ), self, activator, "Open" )
            elseif ( doorMoveState == PUSHMOVE_STATE_MOVING_UP or doorMoveState == PUSHMOVE_STATE_TOP ) then
                -- Send close signal, as a toggle, so it remains closed until further actioning.
                Game.SignalOut( Game.GetEntityForLuaName( "HallElevatorDoor00" ), self, activator, "Close" )
            end
        end
    end
    return true
end
----------------------------------------------------------------------
-- The Doors 
----------------------------------------------------------------------
function HallElevatorDoor00_OnSignalIn( self, signaller, activator, signalName, signalArguments )
    -- Acquire the number of the inside elevator func_button.
    local ent_HallElevator_InsideButton = Game.GetEntityForLuaName( "HallElevatorButtonInside" )
    DEBUG_ITERATE_TABLE( signalArguments )
    -- Closed by inside button?.
    if ( signalName == "OnClosed" and signaller == ent_HallElevator_InsideButton ) then
        -- Acquire the number of the func_plat hallway elevator
        local entity_HallElevator = Game.GetEntityForLuaName( "HallElevator" )
        -- Determine its move state.
        local elevatorMoveState = Game.GetPushMoverState( entity_HallElevator )
        -- It is not moving anymore, thus, we can toggle our hallway elevator doors.
        if ( elevatorMoveState ~= PUSHMOVE_STATE_MOVING_DOWN and elevatorMoveState ~= PUSHMOVE_STATE_MOVING_UP ) then
        --if ( elevatorMoveState ~= PUSHMOVE_STATE_MOVING_DOWN and elevatorMoveState ~= PUSHMOVE_STATE_MOVING_UP ) then
            Game.UseTarget( entity_HallElevator, signaller, activator, ENTITY_USETARGET_TYPE_TOGGLE, 1 )
        else
            Core.DPrint( "Well that's fucked eh? The elevatorMoveState(" .. elevatorMoveState .. ")\n" )
            -- TODO: WID: Maybe play some silly audio or display a message here. It is an example so...
        end
    end
    -- Return true.
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
