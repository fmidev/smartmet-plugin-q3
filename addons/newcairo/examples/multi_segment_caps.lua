--
-- Adopted from: http://www.cairographics.org/samples/
--
local fn= ...
assert(fn, "no output filename")

require "newcairo"

local cs,cr= newcairo.surface( 256,256, { filename=fn } )

cr	.move_to(50, 75).rel_line_to(150, 0)
	.move_to(50, 125).rel_line_to(150, 0)
	.move_to(50, 175).rel_line_to(150, 0)
	.set_line_width(30).set_line_cap("round")
	.stroke()
