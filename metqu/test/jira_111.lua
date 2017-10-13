require "metqu"

local MODEL_QUERYDATA_MASK= "../../../data/iv5_data/mallipinta/2010*.sqd"
local SURFACE_QUERYDATA_MASK= "../../../data/iv5_data/pinta/2010*.sqd"

local r,err= raw( MODEL_QUERYDATA_MASK ) 
assert( r,err ) 

DUMP(r.params)
assert( r.has_param("WVEC") )

--
local r2,err= raw( SURFACE_QUERYDATA_MASK ) 
assert( r2,err ) 

local m_rr= r2{ time= r2.times[37] }.RR

for p,v in points(m_rr) do
    if v>0 then
        print( "r2:", p.x, p.y, v )
    end
end 
DUMP( r2.params )

local r3,err= raw( r2, { sqd_producer= 240,
                         -- 
                         projection= "stereographic,20,90,60:16,30,50,60",    -- not same as 'r2.projection'
                         --
                         -- gridsize= xy(12,23)   -- not same as 'r2.gridsize'
                } ) 
assert(r3,err)

local m_rr3= r3{ time= r3.times[37] }.RR

local sick= false

for p,v in points(m_rr3) do
    if v>5.0 then
        print( "r3:", p.x, p.y, v )
        sick= true
    end
end 

--print( "Min and max before projection:", min(m2), max(m2) )
--print( "Min and max after projection:", min(m3), max(m3) )

--assert( min(m3) >= min(m2) )
--assert( max(m3) <= max(m2) )

if sick then
    error "Concerningly high values were detected."
else
    print "OK:)"
end
