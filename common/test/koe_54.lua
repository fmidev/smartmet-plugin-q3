--
-- KOE_54.LUA
--
require "metqu"

local s1,err= raw( "data/*_ecmwf_pinta.sqd", { params=T } )
assert(s1,err)

LOG( concat(s1.params, " ") )

local iv5Params={"FLCbBase","FLCbCover","FLMinBase","FLMaxBase"}
local s2= raw( s1, { params=iv5Params } )

LOG( concat(s2.params, " ") )

return s2.params

