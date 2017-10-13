--
-- Adopted from: http://www.cairographics.org/samples/
--
local fn= ...
assert(fn, "no output filename")

require "newcairo"

local PI= math.pi

local cs,cr= newcairo.surface( 256,256, { filename=fn } )

local pat= newcairo.pattern_linear(0.0, 0.0, 0.0, 256.0)
                .add_color_stop_rgba(1, 0, 0, 0, 1)
                .add_color_stop_rgba(0, 1, 1, 1, 1)

cr	.rectangle(0, 0, 256, 256)
  	.set_source(pat)
  	.fill()

local pat2= newcairo.pattern_radial(115.2, 102.4, 25.6, 102.4,  102.4, 128.0)
                .add_color_stop_rgba(0, 1, 1, 1, 1)
                .add_color_stop_rgba(1, 0, 0, 0, 1)

cr	.set_source(pat2)
  	.arc(128.0, 128.0, 76.8, 0, 2*PI)
  	.fill()
