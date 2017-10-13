--
-- Adopted from: http://www.cairographics.org/samples/
--
local fn= ...
assert(fn, "no output filename")

require "newcairo"

local PI= math.pi

local cs,cr= newcairo.surface( 256,256, { filename=fn } )

cr	.arc( 128, 128, 76.8, 0, 2*PI )
    .clip()
    --
    .new_path()     -- current path is not consumed by 'clip()'
    .rectangle( 0,0, 256,256 )
    .fill()
    .set_source_rgb( 0,1,0 )
    .move_to( 0,0 )
      .line_to( 256,256 )
        .move_to( 256,0 )
          .line_to( 0,256 )
    .set_line_width(10)
    .stroke()

