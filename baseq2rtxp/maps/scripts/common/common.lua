-- Require entities utilities.
require( "utilities/entities" )
-- Lua 'require' Module Table.
local common = {}

-- Add a test function to the entities.
function common:test( someStr )
    Core.DPrint( "common:test(\""..someStr.."\")\n" )
end

-- Return the Lua 'require' Module Table.
return common