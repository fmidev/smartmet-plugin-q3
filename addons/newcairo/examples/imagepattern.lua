--
-- Adopted from: http://www.cairographics.org/samples/
--
local fn_in, fn_out= ...
assert(fn_in, "no intput filename")     -- "data/romedalen.png"
assert(fn_out, "no output filename")

require "newcairo"

local PI= math.pi
local sqrt= math.sqrt

local image,w,h= newcairo.surface(fn_in)
local pat= newcairo.pattern_for_surface(image)
            .set_extend("repeat")
            .set_matrix( newcairo.matrix_scale(w/256*5, h/256*5) )

local cs,cr= newcairo.surface( 256,256, { filename=fn_out } )

cr	.translate(128.0, 128.0)
	.rotate(PI/4)
	.scale(1/sqrt(2), 1/sqrt(2))
	.translate(-128.0, -128.0)
	.set_source(pat)
	--
	.rectangle( 0, 0, 256.0, 256.0 )
	.fill()
