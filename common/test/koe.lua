--
-- KOE.LUA
--
-- MetQu standalone-käytön testaus
--
local DATA_PATH= os.getenv("DATA_PATH") or ""

require "metqu"

local r,err= raw( DATA_PATH.."/*_ecmwf_skandinavia_pinta240h_3.sqd", { params=T } )
assert(r,err)

LOG( concat(r.params, " ") )
LOG( concat(r.levels, " ") )
LOG( concat(r.times, " ") )

--
-- Mitä parametreja uuteen laitetaan?
--
local params2={ "CAPE", "FlMinB:1234" }
for _,v in ipairs(r.params) do
    params2[#params2+1]= v
end

--
-- Toinen raw, jota voidaan kirjoittaa (otetaan vain yksi validtime ja level mukaan)
--
local r2,err= raw( r, { times= r.times[1], levels= r.levels[1], params= params2 } )
assert(r2,err)

LOG( concat(r2.params, " ") )
LOG( concat(r2.times, " ") )
LOG( concat(r2.levels, " ") )

--
-- Modaa sitä ja palauta lopputulos
--
for _,vt in ipairs(r2.times) do
    for _,lev in ipairs(r2.levels) do

        -- Reduce the T param by one
        --
        local r2_T= r2.grid{ time=vt, level=lev }[T]
        r2_T= r2_T - 1    -- reduce whole matrix by one (changes will remain in 'r2')
    end
end

local g,err= r2.grid{}
assert( g,err )
assert( g.FlMinB )
LOG( g.FlMinB )

r2.write( "out_xxx.sqd" )

--
-- Re-open the file and show its params (did we get the named 'FlMinB:1234' right?)
--
local r3= raw( "out_xxx.sqd" )
LOG( concat(r3.params, " ") )

LOG("Done.")

