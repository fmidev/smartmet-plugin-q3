--
-- CROSS.LUA                            Copyright 2009-2010, Ilmatieteen laitos
--
-- Cross section utility functions.
--
-- This file is compiled into the Q3 engine, just like 'q3.lua' is.
-- Keeping it in a separate source file is merely "making a point" of it
-- not being 100% essential (it could also be placed as a 'require'-loadable
-- addon on the server disk).
--

local bind= assert( select(1,...) )     -- C side exports

-- Functions for creating C++ side objects
--
local new_ScalarMatrix= assert( bind.new_ScalarMatrix )
local nan_matrix= assert( bind._nan_matrix )

bind=nil
local table_insert= table.insert

-- 
-- { [val [, ...]] }= wrap( [val | { val, ... }] )
--
-- Wrap the given values in a table. If no value is given, returns an empty table.
-- If a single value is given, returns it wrapped in a table. If a table is given,
-- returns the table unchanged.
--
local function wrap( a )
    return type.table(a) and a or {a}
end

--
-- [{ { level_type_str [, value_num] } [, ...] }]= levels_in_order( [levels_tbl] )
--
-- levels_tbl: { [ground,]
--               [hpa= { num, ... },]
--               [hybrid= { uint, ... },]
--               [height= { num, ... },]    (if enabled by C++ side)
--              }
--
-- This function defines the order of levels returned by 'cross()'.
--
-- Returns nothing (nil) if there were no levels defined (never returns an empty table).
--    
-- 21-Sep-2011 PKi: If using newbase, (currently) support for one level type only
--    
local function levels_in_order(t,use_newbase)
    if not t then return end

    local ll= {}
    local gr=0 local hp=0 local fl=0 local hy=0 local he=0
    if t.ground then
        ll[#ll+1]= {"ground",true}
        gr=1
    end
    for _,v in ipairs( wrap(t.hpa) ) do
        ll[#ll+1]= {"hpa",v}
        hp=1
    end
    -- 27-Sep-2011 PKi: Flight level support
    for _,v in ipairs( wrap(t.flight) ) do
        ll[#ll+1]= {"flight",v}
        fl=1
    end
    for _,v in ipairs( wrap(t.hybrid) ) do
        ll[#ll+1]= {"hybrid",v}
        hy=1
    end
    for _,v in ipairs( wrap(t.height) ) do
        ll[#ll+1]= {"height",v}
        he=1
    end

    if (use_newbase and ((gr+hp+fl+hy+he) > 1)) then
        error("Must give only one of: 'ground', 'hybrid', 'hpa', 'flight', 'height'",2)
    end

    return ll[1] and ll or nil
end

--
-- matrix_ud|matrix2d_ud = cross( track_proxy_ud, param_str, latlon_ud, {time [, ...]} ,levels_tbl )
-- matrix_ud|matrix2d_ud = cross( track_proxy_ud, param_str, {latlon_ud [, ...]}, time ,levels_tbl )
-- matrix_ud|matrix2d_ud = cross( track_proxy_ud, param_str, {latlon_ud [, ...]}, {time [, ...]} ,levels_tbl )
--
-- matrix_ud|matrix2d_ud = cross( raw_ud, param_str, latlon_ud, {time [, ...]} [, levels_tbl] )
-- matrix_ud|matrix2d_ud = cross( raw_ud, param_str, {latlon_ud [, ...]}, time [, levels_tbl] )
-- matrix_ud|matrix2d_ud = cross( raw_ud, param_str, {latlon_ud [, ...]}, {time [, ...]} [, levels_tbl] )
--
-- time:    jday_ud|time_str
-- levels: { 
--       [ground=true,]
--       [hpa=number|{number,...},]
--       [hybrid=uint|{uint,...},]
--       [height=number|{number,...},]  (if enabled by C++ side)
--       [flight=uint|{uint,...},]
--      }
--
-- Returns: 
--      Matrix with levels in the Y axis and cross section progress on the X axis.
--
-- Note:
--      Default for defining no levels is to make a cross section of _all_ the available levels.
--      This can only be done when 'raw' is the first parameter (the levels available in a track
--      are not a fixed set).
--
-- 21-Sep-2011 PKi: If new parameter 'use_q3' is false or not given, using newbase
-- 05-Mar-2015 PKi: If new parameter 'flightroute' is true, returning one value per location, time and level
--
function cross( a, param, loc, time, levels, flightroute, use_q3 )

    local levels_proto= 
           "{ ground=[true],"..
           "   hpa=[number|{number,...}],"..
           "   hybrid=[uint|{uint,...}],"..
           "   height=[number|{number,...}],"..
           "   flight=[uint|{uint,...}],"..
           "}"

    -- 17-Feb-2012 PKi: TrackProxy not available in metqu
    --
    if type.Raw(a) then
        proto( "Raw, string, latlon|{latlon, ...}, jday|time_str|{jday|time_str, ...}, ["..levels_proto.."]",
                a, param, loc, time, levels )
    else
        proto( "TrackProxy, string, latlon|{latlon, ...}, jday|time_str|{jday|time_str, ...}, ["..levels_proto.."]",
                a, param, loc, time, levels )
    end

    flightroute = ((type(flightroute) == "boolean") and flightroute) or false
    local use_newbase = ((use_q3 == nil) or (type(use_q3) ~= "boolean")) or (not use_q3)

    -- [val|NAN]= func( level_type_str, level_value_num|true, latlon_ud, jday_ud )
    --
    -- Hides the track/raw difference; provides data to the matrix-filling loop
    --
    -- 17-Feb-2012 PKi: TrackProxy not available in metqu
    --
    local func= (not type.Raw(a)) and
        function( lt, lv, latlon, t, cross )
            -- 07-Oct-2011 PKi: Tracks do not support height=number|table; require height=true instead
            local V
            if (lt == "height") then
                V = true
            else
                V = lv
            end
            -- 12-Mar-2012 PKi: Do not require the times to exist in data (missing values returned)
            local r,err= a{ [lt]=V, params={param}, origintime=true }   -- any origintime
            if r then
                -- 21-Sep-2011 PKi: Use newbase ?
                if (use_newbase) then
                    local m,e= r{ [lt]=lv, time=t, location=latlon, [cross]=true }
                    -- 11-Oct-2011 PKi: Report the error if set
                    if ((m == nil) and (e ~= nil)) then
                        error(e)
                    end
                    return (m and m[param]) or nil
                end

                -- 28-Sep-2011 PKi: Added nil checks to avoid runtime errors
                local g= r{ [lt]=lv, time=t }
                local m= (g and g[param]) or nil
                return (m and m[latlon]) or nil
            end
            --return nil    -- no data
        end
    or
        function( lt, lv, latlon, t, cross )
            -- 21-Sep-2011 PKi: Use newbase ?
            if (use_newbase) then
                local m,e= a{ [lt]=lv, time=t, location=latlon, [cross]=true }
                local mp
                -- 11-Oct-2011 PKi: Report the error if set
                if (not m) then
                    error(e and e or "Unknown error when fetching data")
                end
                mp = m[param]
                if (not mp) then
                    error("Unknown parameter: " .. param, 0)
                end
                return mp
            end
	
            -- 28-Sep-2011 PKi: Added nil checks to avoid runtime errors
            local g= a{ [lt]=lv, time=t }
            local m= (g and g[param]) or nil
            return (m and m[latlon]) or nil
        end
    
    -- Note: The order in which we form 'levels' defines the order in which the Y axis is filled
    --      (which matters if the caller asked for more than one kind of levels).
    --
    local ll= levels_in_order(levels,use_newbase)        -- { { lt_str [,lv_num] }, ... }
    
    -- If no levels defined, default is (for Raw) all levels
    --
    if not ll then
        if not type.Raw(a) then
            error( "Levels must be explicitly provided when using 'cross()' for a track (not for a particular data source)", 2 )
        end
        
        -- Note: SQD files always contain ONLY ground, hybrid, or pressure levels ONLY.
        --       We don't stick to this restriction in q3, though. Data sources can have any
        --       combindation of these.
        --
        --       The levels are returned to the caller in the 'ground,hybrid,pressure' order,
        --       but within these groups we stick to the order given by '.levels'.
        --
        -- 21-Sep-2011 PKi: If using newbase, (currently) support for one level type only
        --                  (call with multiple types results in error() in levels_in_order())
        --      
        local tmp= {}
        for _,v in ipairs(a.levels) do      -- "ground", "hybrid:nnn", "hPa:nnn"
            local lt,lv= v:match("^(.-):([%d.]+)$")
            if not lt then
                tmp[v]= true   -- "ground"
            else
                if lt=="hPa" then lt="hpa" end      -- work with lower case internally
                if not tmp[lt] then
                    tmp[lt]= {}
                end
                table_insert( tmp[lt], tonumber(lv) )     -- retain the order from '.levels'
            end
        end

        local r= a

--DUMP(tmp)            -- i.e. '{ hpa= { 1000, 900, ..., 300 } }'
        ll= levels_in_order( tmp,use_newbase )
        assert( ll, "Data with no levels?" )
    end

    -- Quietly allow also both fixed loc AND time
    -- (better than giving an error message)
    --

    local fixed_loc= ((not type.table(loc)) or (#loc == 1))
    local fixed_time= ((not type.table(time)) or (#time == 1))

    if ((not fixed_loc) and (not fixed_time) and (#loc ~= #time)) then
        error( "cross() needs an equal number of locations and times", 2 )
    end

    local width= (((not fixed_loc) and #loc) or (fixed_time and 1) or #time)
    local height= #ll

    -- Note: We don't know yet if 'param' provides a scalar or a vector; initialize 'm' once
    --      we know.
    --
    local m

    local one_loc= ((type.table(loc) and loc[1]) or loc)
    local one_time= ((type.table(time) and time[1]) or time)

    local jd= fixed_time and (jday(one_time) or error( "Bad time: "..tostring(one_time), 2 ))
    local latlon= fixed_loc and one_loc

	if string.match(param,"^%d+$") then param= ":"..param end

    -- 21-Sep-2011 PKi: If using newbase (default), load levels and times into tables
    --                  for loading cross via single call to newbase.

    if (use_newbase) then
        local lt = ll[1][1]
        local lvVec = ((lt == "ground") and true) or {}
        local jdVec = {}

        if (type(lvVec) == "table") then
            for i,pair in ipairs(ll) do
                lvVec[i] = pair[2]
            end
        end

        if not fixed_time then
            for j=1,width do
                jdVec[j] = jday(time[j]) or error( "Bad time: "..tostring(time[j]), 2 )
            end
        else
            jdVec[1] = jd
        end

        m= func( lt, lvVec, loc, jdVec, flightroute and "flightroute" or "cross" )
    else

    for i,pair in ipairs(ll) do
        local lt,lv= pair[1], pair[2] or true

        for j=1,width do
            if not fixed_time then
                jd= jday(time[j]) or error( "Bad time: "..tostring(time[j]), 2 )
            end
            if not fixed_loc then
                latlon= loc[j]
            end

            -- Get the data at particular level, location and time
            --
--DUMP( {lt,lv,latlon,jd} )
            local v= func( lt, lv, latlon, jd )
            if v then
                if not m then
                    m= nan_matrix( xy(width,height), v )   -- scalar, vector xy or vector polar
                end
                m[xy(j-1,i-1)]= v
            end
        end
    end
    end	-- if use newbase

    return m or new_ScalarMatrix( xy(width,height), nan )
end
