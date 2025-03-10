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
--    func_breakable map stuff:
--
--
--
-----------------------------------------------------------------------------
function FuncExplosives00_OnSignalIn( self, signaller, activator, signalName, signalArguments )
    -- Notify players.
    Game.Print( PrintLevel.NOTICE, "FuncExplosives(number:#"..self.number..") received Signal(\""..signalName.."\") with a damage of("..signalArguments.damage..")...\n" )
    -- Done handling signal.
    return true
end

-----------------------------------------------------------------------------
--  SignalIn: button_func_wall_02
-----------------------------------------------------------------------------
function button_func_wall_02_OnSignalIn( self, signaller, activator, signalName, signalArguments )
    -- Get wall target entity.
    local wallEntity = Game.GetEntityForTargetName( self.target )

    -- Toggle..
    if ( signalName == "OnPressed" ) then
        -- Signal.
        Game.SignalOut( wallEntity, signaller, activator, "Toggle", {} )
    end
    -- Toggle..
    if ( signalName == "OnUnPressed" ) then 
        -- Signal.
        Game.SignalOut( wallEntity, signaller, activator, "Toggle", {} )
    end
    
    -- Done handling signal.
    return true
end
-----------------------------------------------------------------------------
--  SignalIn: button_func_wall_03
-----------------------------------------------------------------------------
function button_func_wall_03_OnSignalIn( self, signaller, activator, signalName, signalArguments )
    -- Get wall target entity.
    local wallEntity = Game.GetEntityForTargetName( self.target )

    -- Show.
    if ( signalName == "OnPressed" ) then
        -- Signal.
        Game.SignalOut( wallEntity, signaller, activator, "Show", {} )
    end
    -- Hide.
    if ( signalName == "OnUnPressed" ) then 
        -- Signal.
        Game.SignalOut( wallEntity, signaller, activator, "Hide", {} )
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
    Core.DPrint( "OnClientExitLevel: A client connected with entityID(#" .. clientEntity.number .. ")\n")
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
    return true
end
