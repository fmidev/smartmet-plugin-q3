--[[
-- description: Testataan väliaikaismuuttujia
--
-- projection:  stereographic,20,90,60:6,51.3,49,70.2
-- gridsize:    5,6
-- decimals:    3
-- validtime:   TODAY12
--]]

local x= HIR.T - HIR.DP
local y = HIR.P / HIR.WS
local z = HIR.N * 1.1
return x * y + z


--[[q2:
    Var x = T_HIR - DP_HIR
    Var y = P_HIR / WS_HIR
    Var z = N_HIR * 1.1
    Result = x * y + z
]]
