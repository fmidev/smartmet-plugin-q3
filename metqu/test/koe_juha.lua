--
-- KOE_NAN.LUA
--
-- Testataan NAN:in kirjoittamista SQD-tiedostoon (pitäisi mennä 32700:ksi)
--
-- Testataan myös ääkkösellisten parametrinimien meno Latin-1-enkoodauksella.
--
require "metqu"

local r,err= raw( "IV5/data/pinta/*.sqd" )
assert(r,err)

print( table.concat(r.params,"\n") )
