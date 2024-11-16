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
----    Target(s) Range Implementations:
----
----
----------------------------------------------------------------------
-- XXL Target:
function TargetXXL_OnSignalIn( self, signaller, activator, signalName, signalArguments )
    if ( signalName == "OnKilled" ) then
       Core.DPrint( "Killed TargetXXL :-)\n" ) 
    end
end
-- XL Target:
function TargetXL_OnSignalIn( self, signaller, activator, signalName, signalArguments )
    if ( signalName == "OnKilled" ) then
        Core.DPrint( "Killed TargetXL :-)\n" ) 
     end
end
-- L Target:
function TargetL_OnSignalIn( self, signaller, activator, signalName, signalArguments )
    if ( signalName == "OnKilled" ) then
        Core.DPrint( "Killed TargetL :-)\n" ) 
     end
end
-- Target:
function Target_OnSignalIn( self, signaller, activator, signalName, signalArguments )
    if ( signalName == "OnKilled" ) then
        Core.DPrint( "Killed Target :-)\n" ) 
     end
end

-- Button that Toggles On/Off
function button_toggle_targetrange_OnSignalIn( self, signaller, activator, signalName, signalArguments )
    if ( signalName == "OnPressed" ) then
        Core.DPrint( "Pressed toggle target range button\n" )
        Game.UseTarget( Game.GetEntityForTargetName( "train_target_xxl" ), signaller, activator, ENTITY_USETARGET_TYPE_TOGGLE, 1 )
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
    if ( signalName == "OnPressed" ) then
        -- Get entity
        local entityWareHouseDoor01 = Game.GetEntityForLuaName( "WareHouseDoor01" )
        -- Determine its move state.
        local doorMoveState = Game.GetPushMoverState( entityWareHouseDoor01 )
        -- Only SignalOut a "DoorOpen" when the elevator is NOT moving.
        if ( doorMoveState ~= PUSHMOVE_STATE_MOVING_DOWN and doorMoveState~= PUSHMOVE_STATE_MOVING_UP ) then
            -- Send the lock toggle signal.
            --Game.SignalOut( Game.GetEntityForLuaName( "HallElevatorDoor00" ), self, activator, "DoorLockToggle", { testInteger = 10, testString = "Hello World!", testNumber = 13.37 } )
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
