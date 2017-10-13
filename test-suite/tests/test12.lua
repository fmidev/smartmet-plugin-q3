--[[
-- description: Aikasarja yhteen pisteeseen
--
-- Note: 'latlon' really means lonlat, for Q2
--
-- decimals:    3
--]]

return cross( EC, T, latlon("62.7572N 25.9542E"), time_range_h( NOW, NOW+6, 3 ), {ground=true} )


--[[q2:
paramId=4
producerId=240
dataType=2
latlon=25.9542,62.7572
requestType=timeSerial
startTime=NOW
endTime=NOW+6
timeStep=180
]]
