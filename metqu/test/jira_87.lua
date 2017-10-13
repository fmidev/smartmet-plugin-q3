--
-- https://jira.fmi.fi:8443/browse/BRAINSTORMPLUGINS-87
--
require "metqu"

local r,err=raw("../../../data/iv5_data/pinta/*.sqd") 
assert(r,err)

-- "WIND" here means the vector WIND (not the SQD native WIND): causes parameters WS
-- and WD to exist in the created 'r2' (and to be copied from 'r').
--
local r2,err=raw(r, { sqd_producer= 0,
                      params="WIND" 
                }) 
assert(r2,err)

print "Original data:\n"
DUMP( { params=r.params, levels=r.levels } )

print "Copied data:\n"
DUMP( { params=r2.params, levels=r2.levels } )

-- Check the params we got
--
local got= {}
local should_get= { WIND=true, WS=true, WD=true }

for _,v in ipairs( r2.params ) do
    if not should_get[v] then
        error( "New Raw got param '"..v.."' (should not have)" )
    end
    got[v]= true
end

for k,_ in pairs(should_get) do
    if not got[k] then
        error( "New Raw should have gotten param '"..k.."' (but did not)" )
    end
end


local errors= 0

-- Huom: 
--     Myšs 'ipairs(r.times)' voisi kŠyttŠŠ - tŠmŠ 'grids_by_time' uudehko funktio antaa
--     sekŠ gridin ettŠ ajan samalla kertaa.    --AKa 3-Sep-10

for g,vt in grids_by_time(r, r.times[1] --[[alkuaika]], r.times[1] --[[loppuaika]]) do
    local g2= r2{time=vt}

    for pos in points(g.WIND) do
        local v= g.WIND[pos]
        local v2= g2.WIND[pos]
        
        --print( pos.x, pos.y, v.abs, v.deg, v2.abs, v2.deg, v==v2 and "ok" or "error!" )
        
        if (v2~=v) and not (isnan(v) and isnan(v2)) then 
            print( pos.x, pos.y, v.abs, v.deg, v2.abs, v2.deg, "error!" )
            errors= errors+1
        end
    end
end

if errors>0 then
    error( "ERROR: "..errors.." values were faulty!" )
else
    print( "OK :D" )
end


