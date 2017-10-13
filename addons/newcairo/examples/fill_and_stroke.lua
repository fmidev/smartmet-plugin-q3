--
-- Adopted from: http://www.cairographics.org/samples/
--
local fn= ...
assert(fn, "no output filename")

require "newcairo"

local cs,cr= newcairo.surface( 256,256, { filename=fn } )

cr	.move_to(128.0, 25.6)
	.line_to(230.4, 230.4)
	.rel_line_to(-102.4, 0)
	.curve_to(51.2, 230.4, 51.2, 128.0, 128.0, 128.0)
	.close_path()
	--
	.move_to(64.0, 25.6)
	.rel_line_to(51.2, 51.2)
	.rel_line_to(-51.2, 51.2)
	.rel_line_to(-51.2, -51.2)
	.close_path()
	--
	.set_line_width(10)
	.set_source_rgb(0,0,1)    -- blue
	.fill_preserve()
	.set_source_rgb(0,0,0)    -- black
	.stroke()
