--
-- Adopted from: http://www.cairographics.org/samples/
--
local fn= ...
assert(fn, "no output filename")

require "newcairo"

local x0,y0= 25.6, 25.6
local rect_w,rect_h= 204.8, 204.8
local radius= 102.4

local cs,cr= newcairo.surface( 256,256, { filename=fn } )

local x1,y1= x0+rect_w, y0+rect_h

if rect_w/2<radius then
    if rect_h/2<radius then
        cr.move_to( x0, (y0+y1)/2 )
          .curve_to( x0, y0, x0, y0, (x0+x1)/2, y0 )
          .curve_to( x1, y0, x1, y0, x1, (y0+y1)/2 )
          .curve_to( x1, y1, x1, y1, (x1+x0)/2, y1 )
          .curve_to( x0, y1, x0, y1, x0, (y0+y1)/2 )
    else
        cr.move_to( x0, y0+radius )
          .curve_to( x0, y0, x0, y0, (x0+x1)/2, y0 )
          .curve_to( x1, y0, x1, y0, x1, y0+radius )
          .line_to( x1, y1-radius )
          .curve_to( x1, y1, x1, y1, (x1+x0)/2, y1 )
          .curve_to( x0, y1, x0, y1, x0, y1-radius )
    end
else
    if rect_h/2<radius then
        cr.move_to( x0, (y0+y1)/2 )
          .curve_to( x0, y0, x0, y0, x0+radius, y0 )
          .line_to( x1-radius, y0 )
          .curve_to( x1, y0, x1, y0, x1, (y0+y1)/2 )
          .curve_to( x1, y1, x1, y1, x1-radius, y1 )
          .line_to( x0+radius, y1 )
          .curve_to( x0, y1, x0, y1, x0, (y0+y1)/2 )
    else
        cr.move_to( x0, y0+radius )
          .curve_to( x0, y0, x0, y0, x0+radius, y0 )
          .line_to( x1-radius, y0 )
          .curve_to( x1, y0, x1, y0, x1, y0+radius )
          .line_to( x1, y1-radius )
          .curve_to( x1, y1, x1, y1, x1-radius, y1 )
          .line_to( x0+radius, y1 )
          .curve_to( x0, y1, x0, y1, x0, y1-radius )
    end
end

cr	.close_path()
	.set_source_rgb(0.5, 0.5, 1)
	.fill_preserve()
	.set_source_rgba(0.5, 0, 0, 0.5)
	.set_line_width(10)
	.stroke()
