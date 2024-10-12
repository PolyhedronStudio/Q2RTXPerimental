----------------------------------------------------------------------
---- Map State Variables - These are stored for save/load game states.
----------------------------------------------------------------------
mapStates = {
    -- there are none yet.
}

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
function HallElevatorButtonOutside_OnSignalIn( self, signaller, activator, signalName )
    -- Open the doors.
    if ( signalName == "OnPressed" ) then
        -- Determine its move state.
        local elevatorMoveState = Game.GetPushMoverState( Game.GetEntityForLuaName( "HallElevator" ) )
        -- Only SignalOut a "DoorOpen" when the elevator is NOT moving.
        if ( elevatorMoveState ~= PUSHMOVE_STATE_MOVING_DOWN and elevatorMoveState ~= PUSHMOVE_STATE_MOVING_UP ) then
            -- Send open signal, as a toggle, so it remains opened  until further actioning.
            Game.SignalOut( Game.GetEntityForLuaName( "HallElevatorDoor00" ), self, activator, "DoorOpen", { testInteger = 10, testString = "Hello World!", testNumber = 13.37 } )
        end
    end
    return true
end
function HallElevatorButtonInside_OnSignalIn( self, signaller, activator, signalName )
    -- Close the doors.
    if ( signalName == "OnPressed" ) then
        -- Get elevator and door move states.
        local elevatorMoveState = Game.GetPushMoverState( Game.GetEntityForLuaName( "HallElevator" ) )
        local doorMoveState = Game.GetPushMoverState( Game.GetEntityForLuaName( "HallElevatorDoor00" ) )
        -- Only fire SignalOuts for closing/openingthe door IF the elevator is NOT moving.
        if ( elevatorMoveState == PUSHMOVE_STATE_TOP or elevatorMoveState == PUSHMOVE_STATE_DOWN ) then
            -- Open the door if closed:
            if ( doorMoveState == PUSHMOVE_STATE_MOVING_DOWN or doorMoveState == PUSHMOVE_STATE_BOTTOM ) then
                -- Send close signal, as a toggle, so it remains closed until further actioning.
                Game.SignalOut( Game.GetEntityForLuaName( "HallElevatorDoor00" ), self, activator, "DoorClose" )
            elseif ( doorMoveState == PUSHMOVE_STATE_MOVING_UP or doorMoveState == PUSHMOVE_STATE_TOP ) then
                -- Send close signal, as a toggle, so it remains closed until further actioning.
                Game.SignalOut( Game.GetEntityForLuaName( "HallElevatorDoor00" ), self, activator, "DoorOpen" )
            end
        end
    end
    return true
end
----------------------------------------------------------------------
-- The Doors 
----------------------------------------------------------------------
function HallElevatorDoor00_OnSignalIn( self, signaller, activator, signalName )
    -- Acquire the number of the inside elevator func_button.
    local insideButtonEntity = Game.GetEntityForLuaName( "HallElevatorButtonInside" )
    -- Closed by inside button?.
    if ( signalName == "OnClosed" and signaller == insideButtonEntity ) then
        -- Acquire the number of the func_plat hallway elevator
        local hallElevatorEntity = Game.GetEntityForLuaName( "HallElevator" )
        -- Determine its move state.
        local elevatorMoveState = Game.GetPushMoverState( hallElevatorEntity )
        -- -- It is not moving anymore, thus, we can toggle our hallway elevator doors.
        if ( elevatorMoveState ~= PUSHMOVE_STATE_MOVING_DOWN and elevatorMoveState ~= PUSHMOVE_STATE_MOVING_UP ) then
            Game.UseTarget( hallElevatorEntity, signaller, activator, ENTITY_USETARGET_TYPE_TOGGLE, 1 )
        else
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
