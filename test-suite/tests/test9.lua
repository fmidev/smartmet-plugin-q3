--[[
-- description: Advektio
--
-- projection:  stereographic,20,90,60:6,51.3,49,70.2
-- gridsize:    5,6
-- decimals:    3
-- validtime:   TODAY12
--]]

local g= HIR{ params={U,V,T} }()
return adv(g.T) * 1e5

-- NOTE: "Outo muuttuja laskussa: ADV" - q2 does not seem to have such
--[[q2:
    RESULT= ADV(T_HIR)
]]
