
require "metqu"

local r,err= raw( "data/mallipinta/*.fqd" )
assert(r,err)


-- Aikaleimojen debuggaus
--
if true then
    for g,t in grids_by_time(r) do
        print( t, os.date("!%d.%m.%Y %H",t.epoch) )
    end
end


-- Pintojen järjestyksen debuggaus
--
if false then
    print( table.concat( r.levels, " " ) )

    for g,lt,lv in grids_by_level(r) do
        print(lt,lv)
    end
end


