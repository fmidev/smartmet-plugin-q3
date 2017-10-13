--
-- FMINAMES.LUA                            Copyright 2010, Ilmatieteen laitos
--
-- Lua part baked into 'lua51-fminames.so' binding.
--
-- Usage:
--      require "fminames"
--      local ll= latlon( "Helsinki, Finland" )
--      print( ll.lat, ll.lon )
--

--
-- [lon_num, lat_num]= f( str )
--
local f= ...

local latlon_orig= rawget( _G, "latlon" )   -- original 'latlon' function (if any)

--
-- latlon_ud= latlon( str|... )
--
function latlon( ... )

    local a= select(1,...)
    
    if type(a)=="string" then

        -- Note: Convert "Inari, Inari" -> "Inari,Inari"
        --               "New York,    US" -> "New York,US"
        --
        -- Database connection data must be set in configuration
        --
        local dbhost = addonconfigvalue('fminames.dbhost')
        local dbuser = addonconfigvalue('fminames.dbuser')
        local dbpass = addonconfigvalue('fminames.dbpass')
        local dbname = addonconfigvalue('fminames.dbname')
        local dbport = addonconfigvalue('fminames.dbport')

        if (not(dbhost and dbuser and dbpass and dbname and dbport and
                dbhost~="" and dbuser~="" and dbpass~="" and dbname~="" and dbport~=""))
        then
            error( "Database connection data (dbhost, dbuser, dbpass, dbname or dbport) missing" )
        end
        if (not(dbport:match("^%d+$")))
        then
            error( "Database connection port (dbport) is not numeric" )
        end

        a:gsub(",%s+",",")

        local lon,lat= f( dbhost, dbuser, dbpass, dbname, dbport, a )
        if lon then
            if latlon_orig then
                return latlon_orig( lat, lon )  -- create a latlon object
            else
                return { lon=lon, lat=lat }
            end
        end
    end

    if latlon_orig then
        return latlon_orig(...)     -- forward to original function
    else
        error( "Bad parameter (expected location name): ".. type(a) )
    end
end

function addonconfigvalue( a )

    if type( a )=="string" then
        if ( a:match('^(%w+)%.(%w+)$') ) then
            return getaddonsetting( a )
        else
            error( "Bad addon setting name (expected addon.name): ".. a )
        end
    else
        error( "Bad parameter (expected addon setting name): ".. type(a) )
    end

end
