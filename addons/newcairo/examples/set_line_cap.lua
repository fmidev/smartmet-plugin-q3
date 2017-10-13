--
-- Adopted from: http://www.cairographics.org/samples/
--
local fn= ...
assert(fn, "no output filename")

require "newcairo"

local cs,cr= newcairo.surface( 256,256, { filename=fn } )

cr	.set_line_width(30)
	.set_line_cap("butt")  -- default
	.move_to(64, 50).line_to(64, 200)
	.stroke()
	--
	.set_line_cap("round")
	.move_to(128, 50).line_to(128, 200)
	.stroke()
	--
	.set_line_cap("square")
	.move_to(192, 50).line_to(192, 200)
	.stroke()
	-- draw helping lines
	.set_source_rgb(1, 0.2, 0.2)
	.set_line_width(2.56)
	.move_to(64, 50).line_to(64, 200)
	.move_to(128, 50).line_to(128, 200)
	.move_to(192, 50).line_to(192, 200)
	.stroke()
