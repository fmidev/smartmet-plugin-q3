--[[
-- description: WD saa negatiivisia arvoja (pitäisi olla 0..360)
--              https://jira.fmi.fi:8443/browse/BRAINSTORM-119
--
-- projection:  stereographic,20,90,60:6,51.3,49,70.2
-- gridsize:    50,60
-- decimals:    2
-- validtime:   NOW
--]]

local m= HIR.WD

if min(m) < 0 then
    return "WD saa negatiivisia arvoja", m
end

return "ok"

--[[ok:
ok
]]
