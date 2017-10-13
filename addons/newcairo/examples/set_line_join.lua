--
-- Adopted from: http://www.cairographics.org/samples/
--
local fn= ...
assert(fn, "no output filename")

require "newcairo"

local cs,cr= newcairo.surface( 256,256, { filename=fn } )

cr	.set_line_width(40.96)
	.move_to(76.8, 84.48).rel_line_to(51.2, -51.2).rel_line_to(51.2, 51.2)
	.set_line_join("miter")   -- default
	.stroke()
	--
	.move_to(76.8, 161.28).rel_line_to(51.2, -51.2).rel_line_to(51.2, 51.2)
	.set_line_join("bevel")
	.stroke()
	--
	.move_to(76.8, 238.08).rel_line_to(51.2, -51.2).rel_line_to(51.2, 51.2)
	.set_line_join("round")
	.stroke()
