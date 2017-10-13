--[[
-- description: Cairo text (regular)
-- image:       true
--]]

require 'newcairo'
local cs,cr= newcairo.surface( 400,200 --[[,{ format='svg' }]] )

cr .select_font_face('Sans', 'normal', 'bold')
   .set_font_size(90)
	.move_to(10,135).show_text('Hello')
	.move_to(70,165).text_path('fmi')
	.set_source_rgb(0.5, 0.5, 1)
   .fill_preserve()
	.set_source_rgb(0,0,0)
	.set_line_width(2.56)
	.stroke()

return cs

