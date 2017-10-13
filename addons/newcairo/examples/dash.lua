--
-- Adopted from: http://www.cairographics.org/samples/
--
local fn= ...
assert(fn, "no output filename")

require "newcairo"

local dashes= { 50,10, 10,10 }
local offset= -50

local cs,cr= newcairo.surface( 256,256, { filename=fn } )

cr	.set_dash( dashes, offset )
	.set_line_width(10)
	--
	.move_to(128.0, 25.6)
	.line_to(230.4, 230.4)
	.rel_line_to(-102.4, 0)
	.curve_to(51.2, 230.4, 51.2, 128.0, 128.0, 128.0)
	.stroke()
