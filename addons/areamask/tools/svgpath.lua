--
-- SVGPATH.LUA                     Copyright (c) 2009, Ilmatieteen laitos
--
-- Tools for handling SVG path files (s.a. /smartmet/share/textgendata/maps/sonera/suomi.svg)
--
-- Note: Such files are NOT valid SVG (they are not XML, and don't open up in SVG editors).
--      They are, however, valid SVG path constructs = what's inside <path d="..."/>.
--
-- Usage:
--      require "svgpath"
--      local t= svgpath.convert( filename )
--

module( "svgpath", package.seeall )        -- 'package.seeall' lets us see the globals

--
-- { { {lon,lat}, ... } [, ...] }= svgpath.convert( filename_str )
--
-- Converts a text file in the form "M lon lat L lon lat ... [Z]" to Lua syntax.
--
--  o There may be comment lines, starting with '#' (ignored)
--  o Actual data starts with "\"M" (hyphen + M) and ends with another hyphen
--  o The data may be one path, or multiple (each on their own line)
--  o Each path may (or may not) be closed with a "Z" at the end
--
function convert( fn )

    local text= {}      -- non-comment lines (also preceding and ending hyphen removed)

    for line in io.lines(fn) do
        if line:sub(1,1)~="#" then
            -- eat away optional hyphen at start and end 
            local s= line:match( "^\"?(.+)\"?$" )
            text[#text+1]= s
        end
    end

    --
    -- { lon_num, lat_num }= lonlat( lon_str, lat_str )
    --
    local function lonlat(lon,lat)
        return { tonumber(lon), tonumber(lat) }
    end

    local paths= {}
    local suomi_svg     -- non-svg path syntax found in 'suomi.svg'

    for i,s in ipairs(text) do
--print( "!!"..s:sub(-7).."!!" )
        local a1,b1, mid, z= s:match( "^\M%s*([%d.]+)%s+([%d.]+)%s+(L.+)(Z?)$" )
        if not a1 then
            -- 'suomi.svg' has faulty syntax, this is for trying to deal with it
            --
            a1,b1, mid, z= s:match( "^\M([%d.]+),([%d.]+) L(.+)$" )
            if a1 then
                suomi_svg= true
            else
                error( "Bad syntax in: "..fn )
            end
        end

        local t= { lonlat(a1,b1) }

        local pattern= (not suomi_svg) and "L ([%d.]+) ([%d.]+)"    -- normal
                                        or "([%d.]+),([%d.]+)"      -- suomi.svg

        for a,b in mid:gmatch( pattern ) do
            t[#t+1]= lonlat(a,b)
        end
        assert( #t>1, "Only one coordinate in the file?" )
    
        paths[#paths+1]= t
    end
    
    assert( #paths>=1, "No paths in the file?" )
    return paths
end

