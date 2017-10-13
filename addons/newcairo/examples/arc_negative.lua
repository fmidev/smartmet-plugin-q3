--
-- Adopted from: http://www.cairographics.org/samples/
--
-- lua arc_negative.lua output.png
--
local fn= ...
assert(fn, "no output filename")

require "newcairo"

local PI= math.pi
local xc,yc= 128,128
local radius= 100
local angle1= 45 * (PI/180)    -- angles are specified in radians
local angle2= 180 * (PI/180)

local cs,cr= newcairo.surface( 256,256, { filename=fn } )

cr	.set_line_width(10)
    .arc_negative(xc, yc, radius, angle1, angle2)
    .stroke()
    -- draw helping lines
    .set_source_rgba(1, 0.2, 0.2, 0.6)
    .set_line_width(6.0)
    --
    .arc(xc, yc, 10, 0, 2*PI)
    .fill()
    --
    .arc(xc, yc, radius, angle1, angle1)
    .line_to(xc, yc)
    .arc(xc, yc, radius, angle2, angle2)
    .line_to(xc, yc)
    .stroke()
