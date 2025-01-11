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
    -- there are none yet.
    targetRange = {
        targetsAlive = 0,   -- How many targets are still alive and kicking?
        roundActive = false    -- True when the targetrange has been (re-)started and actively moving the targets.
    }
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
--    Target Range Implementation:
--
--
-----------------------------------------------------------------------------
-----------------------------------------------------------------------------
-- The following function is called upon by each individual target entity
-- that has a SignalIn. Saves us a lot of headache.
--
-- The function, implements the necessary responses to the various incoming
-- 'func_door_rotating' signals received by Signalling in order to imiate a
-- target range target behavior.
-----------------------------------------------------------------------------
-- L Target:
function Target_ProcessSignals( self, signaller, activator, signalName, signalArguments, displayName, targetName, trainTargetName, lightTargetName, lightBrushTargetName )
    --
    -- Play special pain audio:
    --
    if ( signalName == "OnPain" ) then
        -- Play speciual 'pain' sound effect.
        Media.Sound( self, SoundChannel.VOICE, mapMedia.sound.rangetarget_pain, 1.0, SoundAttenuation.NORMAL, 0.0 )
        -- Done handling signal.
        return true
    --
    -- It just got killed, stop the train track, 'open' the door, notify players, and update score.
    --
    elseif ( signalName == "OnKilled" ) then
        -- Decrement number of targets alive count, only if we're the team master that is being signalled.
        if ( self.teamMaster == self.targetName ) then
            mapStates.targetRange.targetsAlive = mapStates.targetRange.targetsAlive - 1
            -- Set score counter frame.
            local scoreCounterEntityA = Game.GetEntityForTargetName( "targetsleftcounter0" )
            local scoreCounterEntityB = Game.GetEntityForTargetName( "targetsleftcounter1" )
            -- Change texture of 'animslight' its frame to first, so it turns white-ish indicating this target is killable.
            scoreCounterEntityA.state.frame = mapStates.targetRange.targetsAlive
            scoreCounterEntityB.state.frame = mapStates.targetRange.targetsAlive
        end

        -- Notify of the kill.
        Game.Print( PrintLevel.NOTICE, "Shot down the \"" .. displayName .. "\" target! Only #".. mapStates.targetRange.targetsAlive .. " targets remaining!\n" )
        -- Stop the train for this target.
        Game.UseTarget( Game.GetEntityForTargetName( trainTargetName ), signaller, activator, EntityUseTargetType.OFF, 0 )
        -- Play special 'opening' sound effect.
        Media.Sound( self, SoundChannel.VOICE, mapMedia.sound.rangetarget_open, 1.0, SoundAttenuation.IDLE, 0.0 )

        -- Change texture of 'animslight' its frame to second, so it turns red indicating this target is dead.
        -- Get LightBrush Entity.
        local lightBrushEntity = Game.GetEntityForTargetName( lightBrushTargetName )
        -- Change texture of 'animslight' its frame to first, so it turns white-ish indicating this target is killable.
        lightBrushEntity.state.frame = 1

        -- Get target entity
        --local targetEntity = Game.GetEntityForTargetName( targetName )
        -- Apply a 'GIB' effect to signify respawn.
        --targetEntity.state.effects = EntityEffects.GIB

        -- Turn on all lights for the target range if we killed all 4 targets.
        if ( mapStates.targetRange.targetsAlive <= 0 ) then
            -- Turn on all lights for the target range.
            local targetRangeLights = Game.GetEntitiesForTargetName( "light_ceil_range" )
            -- Iterate over the matching targetname light entities.
            for targetRangeLightKey,targetRangeLight in pairs(targetRangeLights) do
                -- Turn on the light for this target.
                Game.UseTarget( targetRangeLight, self, activator, EntityUseTargetType.ON, 1 )
            end
            -- Turn off the targets left light
            Game.UseTarget( Game.GetEntityForTargetName( "light_targetsleft" ), signaller, activator, EntityUseTargetType.OFF, 0 )
            -- Switch off the target lights. (Round ended.)
            Game.UseTargets( Game.GetEntityForTargetName( "light_target_xxl" ), signaller, activator, EntityUseTargetType.OFF, 0 )
            Game.UseTargets( Game.GetEntityForTargetName( "light_target_xl" ), signaller, activator, EntityUseTargetType.OFF, 0 )
            Game.UseTargets( Game.GetEntityForTargetName( "light_target_l" ), signaller, activator, EntityUseTargetType.OFF, 0 )
            Game.UseTargets( Game.GetEntityForTargetName( "light_target" ), signaller, activator, EntityUseTargetType.OFF, 0 )

            -- The target range isn't in an active round anymore.
            mapStates.targetRange.roundActive = false
            -- Just make sure it is 0.
            mapStates.targetRange.targetsAlive = 0

            -- Get target range button entity.
            local targetRangeButtonEntity = Game.GetEntityForTargetName( "button_toggle_targetrange" )
            -- Enable the button for reuse.
            targetRangeButtonEntity.useTargetFlags = targetRangeButtonEntity.useTargetFlags - EntityUseTargetFlags.DISABLED
            -- Signal it to be unpressed again.
            Game.SignalOut( targetRangeButtonEntity, signaller, activator, "ButtonUnPress", {} )
            -- Adjust frame to match it being pressable again, signalling red animation style that target range is inactive.
            targetRangeButtonEntity.state.frame = 0

            -- Turn on the HoloGram text displays.
            Game.UseTarget( Game.GetEntityForTargetName( "targetsleftcounter0" ), self, activator, EntityUseTargetType.OFF, 0 )
            Game.UseTarget( Game.GetEntityForTargetName( "targetsleftcounter1" ), self, activator, EntityUseTargetType.OFF, 0 )
            Game.UseTarget( Game.GetEntityForTargetName( "wall_hologram0" ), self, activator, EntityUseTargetType.OFF, 0 )
            Game.UseTarget( Game.GetEntityForTargetName( "wall_hologram1" ), self, activator, EntityUseTargetType.OFF, 0 )
        end
        -- Done handling signal.
        return true
    --
    -- If the target is engaging in "Closing", it means a new range round has begun:
    --
    elseif ( signalName == "OnClose" ) then
        -- Get target entity
        local targetEntity = Game.GetEntityForTargetName( targetName )
        -- Apply a 'TELEPORTER' effect to signify respawn.
        targetEntity.state.effects = EntityEffects.TELEPORTER
        -- Turn on the light for this target.
        Game.UseTarget( Game.GetEntityForTargetName( lightTargetName ), signaller, activator, EntityUseTargetType.ON, 1 )
        -- Play speciual closing sound effect.
        Media.Sound( self, SoundChannel.VOICE, mapMedia.sound.rangetarget_close, 1.0, SoundAttenuation.IDLE, 0.0 )
        -- Done handling signal.
        return true
    --
    -- A new round has begun, so the target got told to "close", turn on the train.
    --
    elseif ( signalName == "OnClosed") then
        -- Turn off the targets left light
        Game.UseTarget( Game.GetEntityForTargetName( "light_targetsleft" ), signaller, activator, EntityUseTargetType.ON, 1 )
        -- Get LightBrush Entity.
        local lightBrushEntity = Game.GetEntityForTargetName( lightBrushTargetName )
        -- Change texture of 'animslight' its frame to first, so it turns white-ish indicating this target is killable.
        lightBrushEntity.state.frame = 0
        -- Get target entity
        local targetEntity = Game.GetEntityForTargetName( targetName )
        -- Reset effects.
        targetEntity.state.effects = EntityEffects.NONE
        -- Turn on the train for this target.
        Game.UseTarget( Game.GetEntityForTargetName( trainTargetName ), signaller, activator, EntityUseTargetType.ON, 1 )
        -- Done handling signal.
        return true
    --
    -- It started opening.
    --
    elseif ( signalName == "OnOpen" ) then
        -- Done handling signal.
        return true
    --
    -- It finished the 'open movement' and arrived at destination:
    --
    elseif ( signalName == "OnOpened" ) then
        -- Get target entity
        local targetEntity = Game.GetEntityForTargetName( targetName )
        -- Reset effects.
        targetEntity.state.effects = EntityEffects.NONE
        -- Done handling signal.
        return true
    end
    -- Done handling signal.
    return true
end

----------------------------------------------------------------------
--  Range Targets SignalIn:
--
--  All rely on Target_ProcessSignals.
----------------------------------------------------------------------
-- XXL Target:
function TargetXXL_OnSignalIn( self, signaller, activator, signalName, signalArguments )
    return Target_ProcessSignals( self, signaller, activator, signalName, signalArguments, "XXL", "t_target_xxl", "train_target_xxl", "light_target_xxl", "light_brush_target_xxl" )
end
-- XL Target:
function TargetXL_OnSignalIn( self, signaller, activator, signalName, signalArguments )
    return Target_ProcessSignals( self, signaller, activator, signalName, signalArguments, "XL", "t_target_xl", "train_target_xl", "light_target_xl", "light_brush_target_xl" )
end
-- L Target:
function TargetL_OnSignalIn( self, signaller, activator, signalName, signalArguments )
    return Target_ProcessSignals( self, signaller, activator, signalName, signalArguments, "L", "t_target_l", "train_target_l", "light_target_l", "light_brush_target_l" )
end
-- S Target:
function Target_OnSignalIn( self, signaller, activator, signalName, signalArguments )
    return Target_ProcessSignals( self, signaller, activator, signalName, signalArguments, "S", "t_target", "train_target", "light_target", "light_brush_target_s" )
end

----------------------------------------------------------------------
----    Target Range Start Button SignalIn:
----------------------------------------------------------------------
function button_toggle_targetrange_OnSignalIn( self, signaller, activator, signalName, signalArguments )
    -- If the button is pressed.
    if ( signalName == "OnPressed" ) then
        -- Multiple targets left:
        if ( mapStates.targetRange.targetsAlive > 1 ) then
            -- Notify players.
            Game.Print( PrintLevel.NOTICE, "Round Active, #" .. mapStates.targetRange.targetsAlive .. " targets remaining!\n" )
        -- One target left:
        elseif ( mapStates.targetRange.targetsAlive == 1 ) then
            -- Notify players.
            Game.Print( PrintLevel.NOTICE, "Round Active, last target remaining!\n" )
        -- All targets dead, (re-)activate range course.
        else
            -- Notify players.
            Game.Print( PrintLevel.NOTICE, "New Round Started! Kill all 4 targets as quickly as you can!\n" )
            -- Play speciual 'restart' sound effect.
            Media.Sound( signaller, SoundChannel.ITEM, mapMedia.sound.newround, 1.0, SoundAttenuation.NORMAL, 0.0 )
            -- Signal all targetrange lane targets to "close" again, effectively reactivating our target range course.
            Game.SignalOut( Game.GetEntityForTargetName( "t_target_xxl" ), signaller, activator, "DoorClose", {} )
            Game.SignalOut( Game.GetEntityForTargetName( "t_target_xl" ), signaller, activator, "DoorClose", {} )
            Game.SignalOut( Game.GetEntityForTargetName( "t_target_l" ), signaller, activator, "DoorClose", {} )
            Game.SignalOut( Game.GetEntityForTargetName( "t_target" ), signaller, activator, "DoorClose", {} )
            -- Engage all func_train into moving again.
            Game.UseTargets( Game.GetEntityForTargetName( "train_target_xxl" ), signaller, activator, EntityUseTargetType.ON, 1 )
            Game.UseTargets( Game.GetEntityForTargetName( "train_target_xl" ), signaller, activator, EntityUseTargetType.ON, 1 )
            Game.UseTargets( Game.GetEntityForTargetName( "train_target_l" ), signaller, activator, EntityUseTargetType.ON, 1 )
            Game.UseTargets( Game.GetEntityForTargetName( "train_target" ), signaller, activator, EntityUseTargetType.ON, 1 )
            -- Switch on the target lights. (New round has begun.)
            Game.UseTargets( Game.GetEntityForTargetName( "light_target_xxl" ), signaller, activator, EntityUseTargetType.ON, 1 )
            Game.UseTargets( Game.GetEntityForTargetName( "light_target_xl" ), signaller, activator, EntityUseTargetType.ON, 1 )
            Game.UseTargets( Game.GetEntityForTargetName( "light_target_l" ), signaller, activator, EntityUseTargetType.ON, 1 )
            Game.UseTargets( Game.GetEntityForTargetName( "light_target" ), signaller, activator, EntityUseTargetType.ON, 1 )

            -- Reset targets alive count.
            mapStates.targetRange.targetsAlive = 4
            -- Reset round into active mode.
            mapStates.targetRange.roundActive = true

            -- Turn off all lights for the target range.
            local targetRangeLights = Game.GetEntitiesForTargetName( "light_ceil_range" )
            -- This is used just as for testing purposes of the 'require' functionality.
            -- The implementation resides in /maps/scripts/utilities/entities.lua
            -- entities:for_each_entity(
            --     targetRangeLights, 
            --     function( entityKey, entityValue )
            --         -- Let there be a delay before actually triggering. Otherwise things look too instant/snappy.
            --         entityValue.delay = 4.0
            --         -- Turn on the light for this target.
            --         Game.UseTarget( entityValue, self, activator, EntityUseTargetType.OFF, 0 )
            --         -- Reset the delay back to 0.
            --         entityValue.delay = 0
            --     end
            -- )
            -- This is how you'd normally do it.
            -- -- Iterate over the matching targetname light entities.
            for targetRangeLightKey,targetRangeLight in pairs(targetRangeLights) do
                -- Turn off the light for this target.
                Game.UseTarget( targetRangeLight, self, activator, EntityUseTargetType.OFF, 0 )
            end

            -- Set its frame to 'orange' state.
            self.state.frame = 4
            -- Disable the button from being reused.
            self.useTargetFlags = self.useTargetFlags + EntityUseTargetFlags.DISABLED

            -- Turn on the HoloGram text displays.
            Game.UseTarget( Game.GetEntityForTargetName( "targetsleftcounter0" ), self, activator, EntityUseTargetType.ON, 1 )
            Game.UseTarget( Game.GetEntityForTargetName( "targetsleftcounter1" ), self, activator, EntityUseTargetType.ON, 1 )
            Game.UseTarget( Game.GetEntityForTargetName( "wall_hologram0" ), self, activator, EntityUseTargetType.ON, 1 )
            Game.UseTarget( Game.GetEntityForTargetName( "wall_hologram1" ), self, activator, EntityUseTargetType.ON, 1 )
        end

        -- Set score counter frame.
        local scoreCounterEntityA = Game.GetEntityForTargetName( "targetsleftcounter0" )
        local scoreCounterEntityB = Game.GetEntityForTargetName( "targetsleftcounter1" )
        -- Change texture of 'animslight' its frame to first, so it turns white-ish indicating this target is killable.
        scoreCounterEntityA.state.frame = mapStates.targetRange.targetsAlive
        scoreCounterEntityB.state.frame = mapStates.targetRange.targetsAlive
    end
    return true
end



----------------------------------------------------------------------
--
--
--    WareHouse:
--
--
----------------------------------------------------------------------
----------------------------------------------------------------------
-- The WareHouseLockingButton SignalIn:
----------------------------------------------------------------------
function WareHouseLockingButton_OnSignalIn( self, signaller, activator, signalName, signalArguments )
    -- Open the doors.
    if ( signalName == "OnPressed" or signalName == "OnUnPressed" ) then
        -- Get the teamed up door entity
        local entityWareHouseDoor00 = Game.GetEntityForLuaName( "WareHouseDoor00" )
        -- Determine its move state.
        local doorMoveState = Game.GetPushMoverState( entityWareHouseDoor00 )
        -- Only SignalOut a "DoorLockToggle" when the doors are NOT moving.
        if ( doorMoveState ~= PushMoveState.MOVING_DOWN and doorMoveState ~= PushMoveState.MOVING_UP ) then
            -- Send the lock toggle signal.
            Game.SignalOut( entityWareHouseDoor00, self, activator, "DoorLockToggle", {} )
        end
    end
    -- Done Signalling.
    return true
end
----------------------------------------------------------------------
-- The Doors SignalIn:
----------------------------------------------------------------------
function WareHouseDoor00_OnSignalIn( self, signaller, activator, signalName, signalArguments )
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
