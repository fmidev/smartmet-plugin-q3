require "metqu"

local PARAM= PRET     -- FOG, RR, PRET, anything combo

local r,err=raw("../../../data/iv5_data/pinta/*.sqd", {params=PARAM} ) 
assert(r,err)

--[[
DUMP( {
    times= r.times,
    levels= r.levels,
    params= r.params,
} )
]]

for g,vt in grids_by_time(r) do
    for p,v in points( g[PARAM] ) do

        print( tostring(vt), p.x, p.y, v )
        --
        -- Faulty values were 1e-36 .. 1e-34
                    
        if (v>0 and v<1e-10) then
            print( tostring(vt), p.x, p.y, v )
            error "Seems wrong"
        end
    end
end

print "OK:)"
