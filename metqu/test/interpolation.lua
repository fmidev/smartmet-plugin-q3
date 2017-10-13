--
--
require "metqu"

local r,err=raw( "../../../data/*.sqd", { params=WD } ) 
assert(r,err)

local t1= r.times[1]
local t2= r.times[2]

local dt= (t2-t1)/10    -- step in hours

-- must use 'while' (not 'for') because 't1','t2' are jday (not numbers)
-- 
local t= t1
while t<=t2 do
    print(t)
    local m= r{ time=t, gridsize=xy(5,5) }.WD
    print(m)

    t= t+dt
end
