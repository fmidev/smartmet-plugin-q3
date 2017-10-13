--
-- Adopted from: http://www.cairographics.org/samples/
--
-- lua text_align_center.lua output.png
--
local fn= ...
assert(fn, "no output filename")

require "newcairo"

local PI= math.pi

local cs,cr= newcairo.surface( 256,256, { filename=fn } )

local utf8= "cairo"

cr  .select_font_face("Sans", "normal", "normal")
    .set_font_size(52)

local ext= cr.text_extents( utf8 )  -- after the setting of the font type & size
local x= 128-(ext.width/2 + ext.x_bearing)
local y= 128-(ext.height/2 + ext.y_bearing)

cr  .move_to(x, y).show_text(utf8)
    -- draw helping lines
    .set_source_rgba(1, 0.2, 0.2, 0.6)
    .set_line_width(6)
    .arc(x, y, 10, 0, 2*PI)
    .fill()
    .move_to(128, 0)
    .rel_line_to(0, 256)
    .move_to(0, 128)
    .rel_line_to(256, 0)
    .stroke()
