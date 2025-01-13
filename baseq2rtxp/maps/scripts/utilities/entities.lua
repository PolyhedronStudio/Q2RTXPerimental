-- Lua 'require' Module Table.
local entities = {}

-- Add a test function to the entities.
function entities:for_each_entity( entities, callbackFunction )
    -- -- Ensure we have a table.
    if type(entities) ~= "table" then
         error( "expected 'entities' to be of type 'table'!" )
    end
    -- Ensure we got a callback function.
    if type(callbackFunction) ~= "function" then
        error( "expected 'callbackFunction' to be of type 'function'!" )
    end
    -- Iterate over the entities.
    for entityKey,entityValue in pairs(entities) do
        callbackFunction( entityKey, entityValue )
    end
end

--
-- UseTarget, but with a delay time in seconds.
--
-- Will store the entity's original delay, override it with a new one, call upon UseTargets, and reset delay to the original value.
--
function entities:UseTargetDelay( targetEntity, otherEntity, activatorEntity, useTargetType, useTargetValue, delaySeconds )
    -- -- Ensure we have a table.
    if targetEntity == nil then
         error( "targetEntity == nil" )
    end
    -- Store original delay.
    local originalDelay = targetEntity.delay
    -- Set temporary delay.
    targetEntity.delay = delaySeconds
    -- Use the target entity.
    local useTargetResult = Game.UseTarget( targetEntity, otherEntity, activatorEntity, useTargetType, useTargetValue )
    -- Reset its deilay.
    targetEntity.delay = 0.0
    -- Return results of UseTarget.
    return useTargetResult
end

--
-- UseTarget, but with a delay time in seconds.
--
-- Will store the entity's original delay, override it with a new one, call upon UseTargets, and reset delay to the original value.
--
function entities:UseTargetsDelay( targetEntity, otherEntity, activatorEntity, useTargetType, useTargetValue, delaySeconds )
    -- -- Ensure we have a table.
    if targetEntity == nil then
         error( "targetEntity == nil" )
    end
    -- Store original delay.
    local originalDelay = targetEntity.delay
    -- Set temporary delay.
    targetEntity.delay = delaySeconds
    -- Use the target entity.
    local useTargetResult = Game.UseTargets( targetEntity, otherEntity, activatorEntity, useTargetType, useTargetValue )
    -- Reset its deilay.
    targetEntity.delay = 0.0
    -- Return results of UseTarget.
    return useTargetResult
end

-- Return the Lua 'require' Module Table.
return entities