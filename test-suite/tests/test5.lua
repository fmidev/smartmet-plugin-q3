--[[
-- description: Testataan eri laskuoperaattoreita, vakioita ja sulkuja
--
-- projection:  stereographic,20,90,60:6,51.3,49,70.2
-- gridsize:    5,6
-- decimals:    3
-- validtime:   TODAY12
--]]

return (HIR.T - HIR.DP) * HIR.P / HIR.WS + HIR.N * 1.1

--[[q2:
    RESULT=(T_HIR - DP_HIR) * P_HIR / WS_HIR + N_HIR * 1.1
]]
