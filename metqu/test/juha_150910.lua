require "metqu"

local NEW_PROJECTION= "stereographic,20,90,60:6,50.0,49,60.0"
local NEW_GS= xy(10,10)

--
-- bool= arrays_match( tbl, tbl )
--
local function arrays_match( a, b ) 
    if #a ~= #b then return false end
    
    for i=1,#a do
        if a[i] ~= b[i] then
            return false
        end
    end
    return true     -- all match    
end


local r,err=raw("../../../data/iv5_data/mallipinta/*.fqd") 
assert(r,err)

DUMP( {
    projection= r.projection,
    gridsize= r.sqd_gridsize,
    times= r.times,
    levels= r.levels,
    params= r.params,
} )

local r2,err= raw( r, { projection=NEW_PROJECTION, sqd_gridsize=NEW_GS, sqd_producer=123 } )
assert(r2,err)

DUMP( {
    projection_copy= r2.projection,
    gridsize_copy= r2.sqd_gridsize,
    times_copy= r2.times,
    levels_copy= r2.levels,
    params_copy= r2.params,
} )

assert( r.projection ~= r2.projection )
assert( r.sqd_gridsize ~= r2.sqd_gridsize )

assert( arrays_match( r.times, r2.times ) )
assert( arrays_match( r.levels, r2.levels ) )
assert( arrays_match( r.params, r2.params ) )

-- But writing the SQD out causes "name:id" in it
--
r2.write( "koe.sqd" )

local r3,err= raw( "koe.sqd" )
assert(r3,err)

DUMP( r3.params )

assert( arrays_match( r3.times, r.times ) )
assert( arrays_match( r3.levels, r.levels ) )
assert( arrays_match( r3.params, r.params ) )

print "OK :)"
