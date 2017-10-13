--
-- RAJAT.LUA
--
-- Convert SVGPATH areamasks to Lua 'latlon(num,num)' array syntax.
--
-- For SVG path source data see:
--      /smartmet/share/textgendata/maps/sonera/
--
-- Usage:
--  LUA_PATH="tools/?.lua;./?.lua" \
--      lua -lstrict tools/rajat.lua /smartmet/share/textgendata/maps/sonera/uusimaa.svg
--
require "svgpath"

local fn= select(1,...)
if not fn then
    error "No .svgpath filename"
end

local paths= svgpath.convert(fn)

-- By prefixing 'return' we can read the data construct simply by requiring the file.
--
local out= io.stdout

out:write( "-- "..fn.."\n" )
out:write "local f= latlon\n"
out:write "return {\n"

for _,path in ipairs(paths) do
    out:write "   {"

    for i,v in ipairs(path) do
        local lon,lat= unpack(v)
        assert( lon and lat )
    
        if (i-1)%5 == 0 then
            out:write "\n\t"
        end
    
        -- SVG input data seems to have four digits of accuracy
        --
        out:write( string.format( "f(%.4f,%.4f),", lat,lon ) )
    end
    out:write "\n   },\n"
end

out:write "}\n"
