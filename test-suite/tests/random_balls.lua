--[[
-- description: Basic drawing (random circles on transparent bg)
-- image:       true
--]]

require 'newcairo'
local cs,cr= newcairo.surface( 600,200 )

local w,h= cs.width, cs.height

for i=1,20 do
    local r= random(90)
    local cx,cy= r+random(w-2*r), r+random(h-2*r)
    cr.circle( cx,cy, r )
      .set_source_rgba( random(), random(), random(), random() )
      .fill()
end

return cs
