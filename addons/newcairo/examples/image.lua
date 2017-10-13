--
-- Adopted from: http://www.cairographics.org/samples/
--
local fn_in,fn_out= ...
assert(fn_in, "no input filename")
assert(fn_out, "no output filename")

require "newcairo"

local PI= math.pi

local image,w,h= newcairo.surface( fn_in )

-- Note: Intentionally using 'w' for both dimensions (same as 
-- cairographics.org)
--
local cs,cr= newcairo.surface( w,w, { filename=fn_out } )

cr	.translate( 128,128 )
	.rotate( 45*PI/180 )
	.scale( 256/w, 256/h )
	.translate( -0.5*w, -0.5*h )
	--
	.set_source_surface(image, 0, 0)
	.paint()
