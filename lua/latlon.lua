--
-- LATLON.LUA                                     Copyright 2009, Ilmatieteen laitos
--
-- Parsing latlon index strings
--

local abs= assert( math.abs )

--
-- [deg]= parse_latlon( latlon_str, is_positive_bool )
--
-- Parse a latitude or longitude string, return nil if problems with the syntax.
--
local function parse_latlon( str, is_positive )
    proto( "string, bool", str, is_positive )

    local v= tonumber(str)  -- "24.6" etc.
    if v then
        return is_positive and v or -v
    end

    -- If seconds are given, minutes are without decimals
    --
    local deg, mins, secs= str:match( "^(%d+)°(%d+)[']([%d.]+)[\"]$" )
    if not deg then
        -- If no seconds, minutes can have decimals
        deg, mins= str:match( "^(%d+)°([%d.]+)[']$" )
        secs= 0
    end

    deg= tonumber(deg)
    mins= tonumber(mins)
    secs= tonumber(secs)
    
    if deg and mins and secs and (mins<60) and (secs<60) then
        local v= deg + (mins/60) + (secs/3600)
        return is_positive and v or -v
    end
end

assert( abs( parse_latlon( "60°12'13.03\"", true ) - 60.203619444444 ) < 0.00001 )
assert( abs( parse_latlon( "24°57'38.02\"", true ) - 24.960561111111 ) < 0.00001 )
assert( abs( parse_latlon( "24°57'38.02\"", true ) - 24.960561111111 ) < 0.00001 )

assert( abs( parse_latlon( "60°12.2'", true ) - 60.203333333333 ) < 0.00001 )
assert( abs( parse_latlon( "24°57.6'", true ) - 24.96 ) < 0.00001 )

assert( parse_latlon( "xxx", true ) == nil )

--
-- [lat_num, lon_num]= f( latlon_str )
-- 
-- i.e. "60°12′13.03″ N 24°57′38.02″ E"
--      "60°12.2′ N 24°57.6′ E"
--      "60.2N 24.9E"
--      "60N24E"
--
return function( s )
    proto( "string", s )

    local lat,ns, lon,ew= s:match( "^(.-)%s*([NS])%s*(.-)%s*([EW])$" )
    if lat then
        lat= parse_latlon( lat, ns=="N" )
        lon= parse_latlon( lon, ew=="E" )    -- east is positive
        
        if lat and lon then
            return lat, lon
        end
    end
    -- return nil
end
