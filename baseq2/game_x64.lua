-- Requirement for FFI interaction(Required in each file that needs its access).
local ffi = require( "ffi" )

-- Try and get access to Com_LPrintf
ffi.cdef[[
    typedef enum {
        PRINT_ALL,          // general messages
        PRINT_TALK,         // print in green color
        PRINT_DEVELOPER,    // only print when "developer 1"
        PRINT_WARNING,      // print in yellow color
        PRINT_ERROR,        // print in red color
        PRINT_NOTICE        // print in cyan color
    } print_type_t;

    void Com_LPrintf( print_type_t type, const char *fmt, ... );
]]
ffi.C.Com_LPrintf( ffi.C.PRINT_WARNING, "Hello from Lua using Com_LPrintf here is a vararg(1=%d, 2=%d\n", ffi.new("int", 10), ffi.new("int", 20 ) )
