--[[
-- description: Käyrästys
--
-- image:       true
-- projection:  stereographic,20,90,60:6,51.3,49,70.2
-- gridsize:    50,60
-- validtime:   TODAY12
--]]

require 'newcairo'

local RED= "ff1010"
local BLUE= "1010ff"
local BLACK= "000000"

local x_max,y_max= gridsize.x-1, gridsize.y-1
local scale= 500/x_max

local cs,cr= newcairo.surface( x_max*scale,y_max*scale )

-- Convert so we can draw with matrix coordinates
--
scale_to_grid( cr, gridsize, 0,0, cs.width, cs.height )

local m= HIR.T
local step= 2

for val= floor(min(m)/step-1)*step, max(m), step do
    contour_path_for_stroke( cr, contour(m,val) )
    if val==0 then
      cr.set_dash( {1,1} ).set_source_rgb(BLACK)
    else
      cr.set_dash({}).set_source_rgb( val<0 and BLUE or RED )
    end
    cr.stroke()
end

cr .identity_matrix()  -- back to regular coordinates
   .set_source_rgb(0.1,0.1,0.1)
   .select_font_face('Serif', 'normal', 'normal')
	.set_font_size(24)
   .move_to( cs.width*0.1, cs.height-30 )
	.show_text( "HIR.T "..os.date("!%Y-%m-%d %H:%M (UTC)", validtime.epoch) )

return cs
