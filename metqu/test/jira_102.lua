--
-- https://jira.fmi.fi:8443/browse/BRAINSTORM-102
--
require "metqu"

local r,err=raw("../../../data/iv5_data/mallipinta/*.fqd") 
assert(r,err)

DUMP( r.params, r.source )
    --
    -- geom:3
    -- ilmanpaine:1
    -- N:79

-- 'params="N:79"' or 'params=":79"' takes a real param only (not one from combo)
-- 'params="N"' takes either a real or one from combo
--
local r,err=raw("../../../data/iv5_data/mallipinta/*.fqd", { params="N" }) 
assert(r,err)

DUMP( r.params )
print( r.source )

print "OK"
