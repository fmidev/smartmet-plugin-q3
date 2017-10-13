--
-- taken from http://luaforge.net/projects/luacairo/
--
-- lua arc.lua output.png
--
local fn= ...
assert(fn, "no output filename")

require "newcairo"

local w,h= 320,240
local cs,cr= newcairo.surface( w,h, { filename=fn })

cr	.set_source_rgb(1, 1, 1)
    .paint()
    --
    .set_source_rgb(0, 0, 0)
    .select_font_face("Sans", "normal", "bold")
    .set_font_size(w/6)
    .move_to(0, h/4)
    .show_text("Hello cairo!")
    --
    .select_font_face("Sans", "normal", "normal")
    .set_font_size(w/8)
    .move_to(0, 3*h/4)
    .text_path("Lua calling...")
    .set_source_rgb(0.5, 0.5, 1)
    .fill_preserve()
    .set_source_rgb(0, 0, 0)
    .set_line_width(w/200)
    .stroke()
