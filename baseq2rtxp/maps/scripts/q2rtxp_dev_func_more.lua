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
--    func_rotating map stuff:
--
--
-----------------------------------------------------------------------------
--
--
--
function TargetsLeftHoloGram_ToggleState( delaySeconds, lightsOn, other, activator ) 
    -- Use them.
    local useTargetType = EntityUseTargetType.ON
    local useTargetValue = 1
    if ( lightsOn ~= true ) then
        useTargetType = EntityUseTargetType.OFF
        useTargetValue = 0
    end
    -- Turn toggle the targets left light and holgoram entities.
    entities:UseTargetDelay( Game.GetEntityForTargetName( "light_targetsleft" ), other, activator, useTargetType, useTargetValue, delaySeconds )
    entities:UseTargetDelay( Game.GetEntityForTargetName( "targetsleftcounter0" ), other, activator, useTargetType, useTargetValue, delaySeconds )
    entities:UseTargetDelay( Game.GetEntityForTargetName( "targetsleftcounter1" ), other, activator, useTargetType, useTargetValue, delaySeconds )
    entities:UseTargetDelay( Game.GetEntityForTargetName( "wall_hologram0" ), other, activator, useTargetType, useTargetValue, delaySeconds )
    entities:UseTargetDelay( Game.GetEntityForTargetName( "wall_hologram1" ), other, activator, useTargetType, useTargetValue, delaySeconds )
end

-----------------------------------------------------------------------------
--
-----------------------------------------------------------------------------
function button_func_rotating_toggle_OnSignalIn( self, signaller, activator, signalName, signalArguments )
    -- Done handling signal.
    return true
end
function button_func_rotating_continuous_OnSignalIn( self, signaller, activator, signalName, signalArguments )
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
        rangetarget_close = Media.PrecacheSound( "maps/targetrange/rangetarget_close.wav" ),
        rangetarget_open = Media.PrecacheSound( "maps/targetrange/rangetarget_open.wav" ),
        rangetarget_pain = Media.PrecacheSound( "maps/targetrange/rangetarget_pain.wav" ),

        newround = Media.PrecacheSound( "maps/targetrange/newround.wav" )
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
