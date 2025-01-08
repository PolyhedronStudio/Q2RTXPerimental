-- Lua 'require' Module Table.
local entities = {}

-- Add a test function to the entities.
function entities:for_each_entity( entities, callbackFunction )
    -- -- Ensure we have a table.
    -- if type(entities) ~= "table" then
    --     error( "expected 'entities' to be of type 'table'!" )
    -- end
    -- -- Ensure we got a callback function.
    -- if type(callbackFunction) ~= "function" then
    --     error( "expected 'callbackFunction' to be of type 'function'!" )
    -- end
    -- Iterate over the entities.
    for entityKey,entityValue in pairs(entities) do
        callbackFunction( entityKey, entityValue )
    end
end

-- Return the Lua 'require' Module Table.
return entities