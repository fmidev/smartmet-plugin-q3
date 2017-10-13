--[[
-- description: Poikkileikkaus
--
-- validtime:   NOW18
-- decimals:    3
--]]

local locs={ latlon(61.2,23.1), latlon(65.6,24.7), latlon"67.2N 26.9E" }

local levels={ 950, 900, 800, 700, 600, 500, 400, 300 }

return cross( HIR, T, locs, validtime, { hpa=levels } )

--[[q2:
requestType=crossSection
paramId=4
producerId=230
latlon=23.1,61.2,24.7,65.6,26.9,67.2
pressures=950,900,800,700,600,500,400,300
dataType=15
]]
