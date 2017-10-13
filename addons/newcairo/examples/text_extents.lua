--
-- Adopted from: http://www.cairographics.org/samples/
--
-- lua arc.lua output.png
--
-- NOTE: The output of this sample may vary slightly from that on the
--       cairographics.org website. Spacing between characers is platform
--       specific. However the red line will always follow the text outline.
--
local fn= ...
assert(fn, "no output filename")

require "newcairo"

local PI= math.pi

local cs,cr= newcairo.surface( 256,256, { filename=fn } )

local utf8= "cairo"

cr	.select_font_face("Sans", "normal", "normal")
  	.set_font_size(100)

local ext= cr.text_extents( utf8 )  -- after the setting of the font type & size
local x,y= 25,150

cr	.move_to(x,y).show_text(utf8)
	-- draw helping lines
	.set_source_rgba(1, 0.2, 0.2, 0.6)
	.set_line_width(6)
	.arc(x, y, 10, 0, 2*PI)
	.fill()
	.move_to(x,y)
	.rel_line_to(0, -ext.height)
	.rel_line_to(ext.width, 0)
	.rel_line_to(ext.x_bearing, -ext.y_bearing)
	.stroke()
