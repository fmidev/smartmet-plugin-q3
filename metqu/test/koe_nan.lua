--
-- KOE_NAN.LUA
--
-- Testataan NAN:in kirjoittamista SQD-tiedostoon (pitäisi mennä 32700:ksi)
--
-- Testataan myös ääkkösellisten parametrinimien meno Latin-1-enkoodauksella.
--
require "metqu"

local ot= "20100621000000"
local vt= "20100621010203"

local r,err= raw( nil, { 
                    producer="xxx:12",
                    origintime=ot, 
                    times={vt}, 
                    ground=true, 
                    params= { "Äääkkölä:123", "Jäätävä söde:456" },
                    projection= "stereographic,20,90,60:6,51.3,49,70.2",
                    gridsize= xy(10,10),
                } )
assert(r,err)

local m= r{ ground=true }[":123"]

for pos,v in points(m) do
    --LOG( m[pos] )
    --m[pos]= nan
end

r.write( "out_nan.sqd" )
