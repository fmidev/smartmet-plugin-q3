--[[
-- description: Alueiden väritys
--
-- image:       true
-- projection:  stereographic,20,90,60:6,51.3,49,70.2
-- gridsize:    50,60
-- validtime:   NOW
--]]

require 'newcairo'

local x_max,y_max= gridsize.x-1, gridsize.y-1
local scale= 600/x_max

local cs,cr= newcairo.surface( x_max*scale,y_max*scale )

cr .save().set_source_rgb(1,1,1).paint().restore()

-- Convert so we can draw with matrix coordinates
--
scale_to_grid( cr, gridsize, 0,0, cs.width, cs.height )

local m= HIR.T
local step= 2
local smooth= 0.5

--local LBZ= labelizer( cs.width,cs.height )
cr .set_source_rgb(1,0.8,0.2)
   .select_font_face('Serif', 'normal', 'normal')
	.set_font_size(8)

local low   -- { curve, ... }
for val= -30,30, step do
    local tmp= min( 1.0, abs(val)/30 )   -- 0.0 .. 1.0
    local rgb= (val<0) and { tmp/2, tmp/2, tmp } or
               (val==0) and { 0.3, 0.3, 0.3 } or
                           { tmp, tmp/2, tmp/2 }
    low= low or { contour( m, val, smooth ) }
    local high= { contour( m, val, smooth ) }
    contour_path_for_fill( cr, unpack(low) )
    contour_path_for_fill( cr, unpack(high) )
    cr.set_source_rgb( unpack(rgb) )
      .fill()

    -- Plot the numbers each 10m
    --[[ if val%10==0 then
      LBZ.label( cr, val, unpack(low) )  -- add labels
      LBZ.ban( unpack(low) )  -- don't allow future text to overlap this line
                              -- (but DO allow overlaps of the 2,4,6,8 lines)
    end ]]

    low= high
end

cr .identity_matrix()  -- back to regular coordinates
   .set_source_rgb(0.9,0.8,0.2)
   .select_font_face('Serif', 'normal', 'normal')
	.set_font_size(24)
   .move_to( cs.width*0.1, cs.height-30 )
	.show_text( "HIR.T "..os.date("!%Y-%m-%d %H:%M (UTC)", validtime.epoch) )

return cs
