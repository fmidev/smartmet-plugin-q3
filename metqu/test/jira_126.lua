--
-- Jira #126
--
-- Projektion muuttaminen luotaessa uusi raw-olio (sqd-tiedosto) ei tunnu vaikuttavan dataan.
--
require "metqu"

local PROJECTION= "stereographic,20,90,60:17.033,59.113,38.786,69.257"
local PARAM= "T"  --":784"

local r= raw( "../../../data/iv5_data/pinta/201010010720_ecmwf_skandinavia_pinta240h_3.sqd" )

--DUMP( r.params )

local r2= raw( r, {times=r.times, projection=PROJECTION, sqd_producer=r.sqd_producer} )

assert( r.sqd_gridsize == r2.sqd_gridsize )

-- Tarkista, että parametrin arvot ovat erit 

local m= r{ ground=true, time= r.times[1] }[PARAM]
local m2= r2{ ground=true, time= r.times[1] }[PARAM]

local diffs= 0

for pos,v in points(m) do
    local v2= m2[pos]
    
    if v2 ~= v then
        LOG( "DIFFERENT: "..v.." "..v2 )
        diffs= diffs+1
    else
        LOG( "SAME: "..v )
    end
end

if diffs==0 then
    error( "No change by projection" )
else
    local all= m.size.x * m.size.y

    print( "OK: "..diffs.."/"..all.." values differed" )
end
