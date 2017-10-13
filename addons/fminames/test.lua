--
-- TEST.LUA
--
require "fminames"

local ll= latlon('Helsinki')

print( ll.lon, ll.lat, type(ll) )

assert( tostring(ll.lon) == "24.934200286865" )
assert( tostring(ll.lat) == "60.175598144531" )

print "OK!"

