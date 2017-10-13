--
-- Test that levels come out in the right height order
--
require "metqu"

local r,err=raw("../../../data/201009090317_mbehirlam_mallipinta.sqd") 
assert(r,err)

DUMP( r.params, r.source )
    --
    -- geom:3
    -- ilmanpaine:1
    -- N:79

DUMP( r.levels )

for g,lev in grids_by_level(r) do
    print( lev, g.Z[xy(0,0)] )
end

