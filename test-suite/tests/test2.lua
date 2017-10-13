--[[
-- description: Test '.origintimes'
--]]

local ots= EC.origintimes

assert( #ots>0 )

for i=1,#ots-1 do
    assert( ots[i] > ots[i+1] )     -- decending order (latest is [1])
end

-- Negative origintimes
--
assert( EC{ origintime=-1 }.origintime == ots[2] )

return "OK"

--[[ok:
OK
]]
