--[[
-- description: Q2/Q3 datat eivät vastaa toisiaan RCR Hirlam-hauissa
--
-- projection:  stereographic,20,90,60:6,51.3,49,70.2
-- gridsize:    5,6
-- decimals:    3
-- validtime:   TODAY12
--]]

return HIR{ hpa=300 }.T

--[[
local r,err= HIR{ hpa=300 }
assert(r,err)

return r().T
]]

--[[q2:
RESULT= T_HIR_300
]]
