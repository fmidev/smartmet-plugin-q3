--
-- Adopted from: http://www.cairographics.org/samples/
--
local fn_in,fn_out= ...
assert(fn_in, "no input filename")
assert(fn_out, "no output filename")

require "newcairo"

local PI= math.pi
local image,w,h= newcairo.surface( fn_in )    -- read the image

local cs,cr= newcairo.surface( 256,256, { filename=fn_out } )

cr	.arc( 128, 128, 76.8, 0, 2*PI )
    .clip()
    .new_path()     -- current path is not consumed by 'clip()'
    --
    .scale( 256/w, 256/h )
    .set_source_surface( image, 0,0 )
    .paint()
