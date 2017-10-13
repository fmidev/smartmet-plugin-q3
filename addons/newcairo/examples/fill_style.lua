--
-- Adopted from: http://www.cairographics.org/samples/
--
local fn= ...
assert(fn, "no output filename")

require "newcairo"

local PI= math.pi

local cs,cr= newcairo.surface( 256,256, { filename=fn } )

cr	.set_line_width(6)
	.rectangle(12, 12, 232, 70)
	.new_sub_path()
		.arc(64, 64, 40, 0, 2*PI)
	.new_sub_path()
        .arc_negative(192, 64, 40, 0, -2*PI)
	--
	.set_fill_rule("even_odd")
	.set_source_rgb(0, 0.7, 0).fill_preserve()
	.set_source_rgb(0, 0, 0).stroke()
	--
	.translate(0, 128)
	.rectangle(12, 12, 232, 70)
	.new_sub_path().arc(64, 64, 40, 0, 2*PI)
	.new_sub_path().arc_negative(192, 64, 40, 0, -2*PI)
	--
	.set_fill_rule("winding")
	.set_source_rgb(0, 0, 0.9).fill_preserve()
	.set_source_rgb(0, 0, 0).stroke()
