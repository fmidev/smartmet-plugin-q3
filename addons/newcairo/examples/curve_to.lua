--
-- Adopted from: http://www.cairographics.org/samples/
--
local fn= ...
assert(fn, "no output filename")

require "newcairo"

local x,y= 25.6, 128.0
local x1,y1= 102.4, 230.4
local x2,y2= 153.6, 25.6
local x3,y3= 230.4, 128.0

local cs,cr= newcairo.surface( 256,256, { filename=fn } )

cr	.move_to(x, y)
	.curve_to(x1, y1, x2, y2, x3, y3)
	--
	.set_line_width(10)
	.stroke()
	--
	.set_source_rgba(1, 0.2, 0.2, 0.6)
	.set_line_width(6.0)
	.move_to(x,y).line_to(x1,y1)
	.move_to(x2,y2).line_to(x3,y3)
	.stroke()
