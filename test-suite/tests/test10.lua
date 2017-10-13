--[[
-- description: Poikkileikkaus
--
-- decimals: 3
--]]

local trange= time_range_mins(NOW+6,NOW+30,60)

-- We get 25x1 matrices which need to be merged to compare with q2 result (which is 25x2)
--
local a= cross( HIR, T, lonlat(21.4,62.1), trange, {ground=true} )
local b= cross( HIR, T, lonlat(25.8,71.2), trange, {ground=true} )

local xs= a.size.x
LOG(xs)

local m= matrix( xy( xs, 2 ) )

for x=0,xs-1 do
    m[ xy(x,0) ]= a[ xy(x,0) ]
    m[ xy(x,1) ]= b[ xy(x,0) ]
end
return m

--[[q2:
paramId=4
producerId=240
dataType=2
requestType=timeSerial
startTime=NOW+6
endTime=NOW+30
timeStep=60
latlon=21.4,62.1,25.8,71.2
]]
