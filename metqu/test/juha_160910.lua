require "metqu"

local r,err=raw("../../../data/iv5_data/mallipinta/*.fqd") 
assert(r,err)

DUMP( {
    times= r.times,
    levels= r.levels,
    params= r.params,
} )

--[[
for g,vt in grids_by_time(r) do
    LOG( tostring(vt), tostring(not isnan(max(g.N))) )
end
--]]

--
local r2,err= raw( r, { sqd_producer=123 } )  -- copy (writeable)
assert(r2,err)
    
for g,vt in grids_by_time(r2) do
    g.N= 123        -- test we can write it like this
    g["N:79"]= 345  -- and like this (same thing)
    LOG( tostring(vt) )
end
r2.write( "out.sqd" )

--
print "\nOK :)"
