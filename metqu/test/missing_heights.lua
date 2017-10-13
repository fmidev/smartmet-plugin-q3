--
-- MISSING_HEIGHTS.LUA
--
-- Miksi tietyissä SQD-tiedostoissa näyttää olevan vain "tyhjää" korkeustietoa (parametrit 2 ja 3).
--
-- Käyttö:
--      lua -lstrict missing_heights.lua tiedosto.sqd
--
local fn= select(1,...) or error( "Anna SQD-tiedoston nimi parametrina" )

require "metqu"

local r,err= raw(fn, { gridsize=true })     -- use native gridsize for this data
assert(r,err)

LOG( concat(r.params, " ") )
LOG( concat(r.levels, " ") )
LOG( concat(r.times, " ") )

local done= false
local n= 0

for g,lev in grids_by_level( r ) do

    --LOG(lev)
    for pos,v in points(g.Z) do
        if not isnan(v) then
            LOG( pos.x, pos.y, v )
            done= true
            break
        end
        n= n+1
    end
    
    if done then break end
end

LOG( n.." NANs" )
