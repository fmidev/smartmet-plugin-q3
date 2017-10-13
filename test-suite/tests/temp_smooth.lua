--[[
-- description: Matrix drawing with smooth field output
--
-- image:       true
-- projection:  stereographic,20,90,60:6,51.3,49,70.2
-- gridsize:    50,60
-- validtime:   NOW
--]]

require 'newcairo'

local RED= "ff1010"
local BLUE= "1010ff"

local x_max,y_max= gridsize.x-1, gridsize.y-1
local scale= 500/x_max

local cs,cr= newcairo.surface( x_max*scale,y_max*scale )
cr.save().set_source_rgb(1,1,1).paint().restore()   -- background white

-- convert so we can draw with matrix coordinates
-- 
cr .scale(scale,-scale)    -- y grows from down to up
   .translate( 0, -y_max ) -- origin is bottom left

for pos,v in points(HIR.T) do
    if not isnan(v) then
    	cr	.circle( pos.x,pos.y, (abs(v)/30) )
      		.set_source_rgb( (v>0) and RED or BLUE )
       	.fill()
	end
end

cr .identity_matrix()  -- back to regular coordinates
   .set_source_rgb(0.1,0.1,0.1)
   .select_font_face('Serif', 'normal', 'normal')
	.set_font_size(24)
   .move_to( cs.width*0.1, cs.height-30 )
	.show_text( "HIR.T "..os.date("!%Y-%m-%d %H:%M (UTC)", validtime.epoch) )

return cs
