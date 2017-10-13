--[[
-- description: Testaa vektorisuureiden paikkansapitävyys
--
-- decimals:    3
--]]

local r,err= HIR{ params={"UV","WIND"} }
assert(r,err)

function eq( m1, m2 )
    local diff= m1-m2
    return (min(diff)==0) and (max(diff)==0)
end

-- '.params' tulee sisältää nämä kaikki:
--      "WIND", "WD", "WS"
--      "UV", "U", "V"
--
local params_lookup= {}
for _,v in ipairs( r.params ) do
    params_lookup[v]= true
end

assert( params_lookup.WIND )
assert( params_lookup.WD )
assert( params_lookup.WS )
assert( params_lookup.UV )
assert( params_lookup.U )
assert( params_lookup.V )

local g= r()    -- default level, time, projection and gridsize

assert( eq( g.WIND.abs, g.WS ) )
assert( eq( g.WIND.deg, g.WD ) )

assert( eq( g.UV.x, g.U ) )
assert( eq( g.UV.y, g.V ) )

return "ok"

--[[ok:
ok
]]
