--[[
-- description: Tuulivektorien piirto
--
-- image:       true
-- projection:  stereographic,20,90,60:6,51.3,49,70.2
-- gridsize:    50,60
-- validtime:   NOW
--]]

require 'newcairo'

--
-- Draw a wind flag at certain position
--
local function draw_flag(cr,pos,v)
  local rad= v.deg*(pi/180)
  local len= v.abs/10

  -- Do the moving and rotating by transformation; then return back to current transform
  --
  cr.save().translate(pos.x,pos.y).rotate(-rad+pi/2)
  cr.move_to(0,0).line_to(len,0)
  cr.restore()
end

local x_max,y_max= gridsize.x-1, gridsize.y-1
local scale= 400/x_max

local cs,cr= newcairo.surface( x_max*scale,y_max*scale )

cr.save().set_source_rgb(1,1,1).paint().restore()   -- background white

-- convert so we can draw with matrix coordinates
--
scale_to_grid( cr, gridsize, 0,0, cs.width, cs.height )

-- WIND is a vector having both direction (.deg) and force (.abs)
--
for pos,v in points(HIR.WIND) do
  if not isnan(v) then
    draw_flag(cr,pos,v)
  end
end

-- Stroke all flags at once (faster)
cr.set_source_rgb("442288").stroke()

return cs
