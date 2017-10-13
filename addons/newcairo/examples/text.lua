--
-- Adopted from: http://www.cairographics.org/samples/
--
-- lua text.lua output.png
--
-- Note: Bold text effect seems not to be working on OS X (10.6, cairo 1.8.8 
--       via fink). It does work flawlessly on Linux.
--
local fn= ...
assert(fn, "no output filename")

require "newcairo"

local PI= math.pi

local cs,cr= newcairo.surface( 256,256, { filename=fn } )

cr  .select_font_face("Sans", "normal", "bold" )
    .set_font_size(90)
    .move_to(10, 135).show_text("Hello")
    .move_to(70, 165).text_path("void")
    .set_source_rgb(0.5, 0.5, 1)
    .fill_preserve()
    .set_source_rgb(0, 0, 0)
    .set_line_width(2.56)
    .stroke()
    --
    -- draw helping lines
    .set_source_rgba(1, 0.2, 0.2, 0.6)
    .arc(10, 135, 5.12, 0, 2*PI)
    .close_path()
    .arc(70, 165, 5.12, 0, 2*PI)
    .fill()
