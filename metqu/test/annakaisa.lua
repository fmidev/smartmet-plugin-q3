--
-- ANNAKAISA.LUA
--
-- Avataan SQD ja katsotaan sen arvot. Etsitään kummallisuuksia.
--
-- Käyttö:
--      lua -lstrict annkaisa.lua tiedosto.sqd
--
local fn= select(1,...) or error( "Anna SQD-tiedoston nimi parametrina" )

require "metqu"

local r,err= raw(fn)
assert(r,err)

LOG( concat(r.params, " ") )
LOG( concat(r.levels, " ") )
LOG( concat(r.times, " ") )

--error ""    -- pysäytä tähän?

--
-- Etsi erityisen suuria ja pieniä (mutta ei nolla) arvoja (inf, eps)
--
if true then
    local v_max     -- aineiston suurin arvo
    local v_min     -- aineiston pienin
    local v_eps     -- aineiston lähimpänä nollaa (muttei nolla)

    for _,vt in ipairs(r.times) do
        for _,lev in ipairs(r.levels) do
            local g= r.grid{ time=tostring(vt), level=lev }
            for _,par in ipairs(r.params) do
                local m= g[par]
                v_max= max( v_max, max(m) )
                v_min= min( v_min, min(m) )

                for pos,v in points(g[par]) do
                    if (not isnan(v)) and v~=0 then  -- ohita nanit ja nollat
                        if (not v_eps) or (abs(v) < abs(v_eps)) then
                            v_eps= v    -- pidetään etumerkki mukana
                        end
                    end
                end
            end
        end
    end
    return v_min, v_max, v_eps      -- 0, 200, 0.10000000149012
end


--
local bag= {}

for _,vt in ipairs(r.times) do
    for _,lev in ipairs(r.levels) do
        local g= r.grid{ time=tostring(vt), level=lev }
        for _,par in ipairs(r.params) do
            bag[par]= g[par]    -- a matrix
        end
    end
end

-- Returning just 'bag' will not reveal the matrix internals.
-- Returning the matrices as their own lines will.
--
return 
    bag ["FL5Cover:1015"],
    bag ["FL3Cover:1013"],
    bag ["FLMaxBase:1004"],
    bag ["FLMinBase:1003"],
    bag ["FLCbBase:1001"],
    bag ["FL6Cover:1016"],
    bag ["FLCbCover:1002"],
    bag ["FL8Cover:1018"],
    bag ["AVIVIS:1005"],
    bag ["FL4Cover:1014"],
    bag ["FL2Cover:1012"],
    bag ["FL7Cover:1017"],
    bag ["FL1Cover:1011"]
