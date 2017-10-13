--[[
-- description: Drawing with a mask
-- image:       true
--]]

require 'newcairo'
local cs,cr= newcairo.surface( 400,200 )

local linpat= newcairo.pattern_linear(0, 0, 100, 100)
                .add_color_stop_rgb( 0, 0, 0.3, 0.8 )
                .add_color_stop_rgb( 1, 0, 0.8, 0.3 )

local radpat= newcairo.pattern_radial(50, 50, 25, 50, 50, 80)
                .add_color_stop_rgba( 0, 0, 0, 0, 1)
                .add_color_stop_rgba(1, 0, 0, 0, 0)

cr.translate( 50,50 )
    .set_source(linpat)
    .rectangle(100,10,80,80)
    .mask(radpat)
    .fill()

return cs

