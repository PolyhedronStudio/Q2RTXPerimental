local common = require( "common/common")
local entities = require( "utilities/entities" )
----------------------------------------------------------------------
--
--
--    Map State Variables - These are stored for save/load game states:
--
--
----------------------------------------------------------------------
mapStates = {
}



-----------------------------------------------------------------------------
--
--
--    Stores references to precached resources for easy and efficient
--    access.
--
--
-----------------------------------------------------------------------------
mapMedia = {
    -- Filled by precaching.
    sound = {}
}

-----------------------------------------------------------------------------
--
--
--
--    func_explosive map stuff:
--
--
--
-----------------------------------------------------------------------------
function FuncExplosives00_OnSignalIn( self, signaller, activator, signalName, signalArguments )
    -- Notify players.
    Game.Print( PrintLevel.NOTICE, "FuncExplosives(number:#"..self.number..") received Signal(\""..signalName.."\")...\n" )
    -- Done handling signal.
    return true
end



-----------------------------------------------------------------------------
--
--
--
--    func_rotating map stuff:
--
--
--
-----------------------------------------------------------------------------
-----------------------------------------------------------------------------
--  SignalIn: func_rotating_00
-----------------------------------------------------------------------------
function func_rotating_00_OnSignalIn( self, signaller, activator, signalName, signalArguments )
    self.target = "hello"
    -- Notify players.
    Game.Print( PrintLevel.NOTICE, self.targetName .. " " .. self.target .. " received Signal(\""..signalName.."\")...\n" )

    -- Done handling signal.
    return true
end
-----------------------------------------------------------------------------
--  SignalIn: button_func_rotating00_lock
-----------------------------------------------------------------------------
function button_func_rotating00_lock_OnSignalIn( self, signaller, activator, signalName, signalArguments )
    -- Get rotator target entity.
    local rotatorEntity = Game.GetEntityForTargetName( "func_rotating_00" )
    local buttonToggleEntity = Game.GetEntityForTargetName( "button_func_rotating00_toggle" )

    -- Get toggle button entity.
    --local buttonEntity = Game.GetEntityForTargetName( "button_func_rotating00_toggle" )

    -- Lock the rotator from accelerating/decelearting, and lock its toggle button.
    if ( signalName == "OnPressed" ) then
        -- Notify players.
        Game.Print( PrintLevel.NOTICE, "Locking Rotator(\""..rotatorEntity.targetName.."\")...\n" )
        -- Lock Signal.
        Game.SignalOut( rotatorEntity, signaller, activator, "Lock", {} )

        -- Set its frame to 'orange' state.
        buttonToggleEntity.state.frame = 4
        -- Disable it from being used.
        buttonToggleEntity.useTargetFlags = buttonToggleEntity.useTargetFlags + EntityUseTargetFlags.DISABLED
        --
        --Game.SignalOut( buttonEntity, signaller, activator, "ButtonLock", {} )
    end
    -- UnLock the rotator from accelerating/decelearting, and unlock its toggle button.
    if ( signalName == "OnUnPressed" ) then 
        -- Notify players.
        Game.Print( PrintLevel.NOTICE, "Unlocking Rotator (\""..rotatorEntity.targetName.."\")...\n" )
        -- Unlock Signal.
        Game.SignalOut( rotatorEntity, signaller, activator, "Unlock", {} )

        -- Assign new frame determined by buttonMoveState state. (Since we either locked pressed or unpressed.)
        local buttonMoveState = Game.GetPushMoverState( buttonToggleEntity )
        if ( buttonMoveState ~= PushMoveState.TOP and buttonMoveState ~= PushMoveState.MOVING_UP ) then
            -- Set its frame to 'red' state.
            buttonToggleEntity.state.frame = 2
        else
            -- Set its frame to 'green' state.
            buttonToggleEntity.state.frame = 0
        end
        -- Enable it to be used again.
        buttonToggleEntity.useTargetFlags = buttonToggleEntity.useTargetFlags - EntityUseTargetFlags.DISABLED
    end
    
    -- Done handling signal.
    return true
end
-----------------------------------------------------------------------------
--  SignalIn: button_func_rotating00_toggle
-----------------------------------------------------------------------------
function button_func_rotating00_toggle_OnSignalIn( self, signaller, activator, signalName, signalArguments )
    -- Get rotator target entity.
    local rotatorEntity = Game.GetEntityForTargetName( "func_rotating_00" )
    -- Get toggle button entity.
    --local buttonEntity = Game.GetEntityForTargetName( "button_func_rotating00_toggle" )

    -- Toggle into accelerating.
    if ( signalName == "OnPressed" ) then
        -- Notify players.
        Game.Print( PrintLevel.NOTICE, "Toggled into Accelerating...\n" )
        -- Signal.
        Game.SignalOut( rotatorEntity, signaller, activator, "Accelerate", {} )
    end
    -- Toggle into decelerating.
    if ( signalName == "OnUnPressed" ) then 
        -- Notify players.
        Game.Print( PrintLevel.NOTICE, "Toggled into Decelerating...\n" )
        -- Signal.
        Game.SignalOut( rotatorEntity, signaller, activator, "Decelerate", {} )
    end
    
    -- Done handling signal.
    return true
end


----------------------------------------------------------------------
--
--
--    Map CallBack Hooks:
--
--
----------------------------------------------------------------------
--
--  Precache all map specific related media.
--
function OnPrecacheMedia()
    mapMedia.sound = {
        -- rangetarget_close = Media.PrecacheSound( "maps/targetrange/rangetarget_close.wav" ),
        -- rangetarget_open = Media.PrecacheSound( "maps/targetrange/rangetarget_open.wav" ),
        -- rangetarget_pain = Media.PrecacheSound( "maps/targetrange/rangetarget_pain.wav" ),

        -- newround = Media.PrecacheSound( "maps/targetrange/newround.wav" )
    }
    return true
end
-- TODO:
function OnBeginMap()
    return true
end
-- TODO:
function OnExitMap()
    return true
end



----------------------------------------------------------------------
--
--
--    Client CallBack Hooks:
--
--
----------------------------------------------------------------------
-- TODO:
function OnClientEnterLevel( clientEntity )
    Core.DPrint( "OnClientEnterLevel: A client connected with entityID(#" .. clientEntity.number .. ")\n")
    return true
end
-- TODO:
function OnClientExitLevel( clientEntity )
    Core.DPrint( "OnClientEnterLevel: A client disconnected with entityID(#" .. clientEntity.number .. ")\n")
    return true
end



----------------------------------------------------------------------
--
--
--    Frame CallBack Hooks:
--
--
----------------------------------------------------------------------
-- TODO:
function OnBeginServerFrame()
    return true
end
-- TODO:
function OnEndServerFrame()
    return true
end
-- TODO:
function OnRunFrame( frameNumber )
    --Core.DPrint( "framenumber = " .. frameNumber .. "\n" )
    return true
end
