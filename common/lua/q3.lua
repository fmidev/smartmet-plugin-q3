--
-- Q3.LUA                                   Copyright 2009, Ilmatieteen laitos
--
-- Lua side of Q3 bindings. This is executed after 'prepare.lua'.
--

-- TBD: Move 'if METQU' stuff to a separate file, 'metqu/lua/metqu.lua'

-- C side exports
--
local bind, UTC, METQU= ...

-- Functions for creating C++ side objects
--
local new_ApiMatrix= assert( bind.new_ApiMatrix )   -- pushes a scalar or vector matrix
local new_ScalarMatrix= assert( bind.new_ScalarMatrix )
local new_VectorMatrix_xy= assert( bind.new_VectorMatrix_xy )

local new_MatrixPos= assert( bind.new_MatrixPos )
local new_SubMatrix= assert( bind.new_SubMatrix )

local new_Vector_xy= assert( bind.new_Vector_xy )
local new_Vector_polar= assert( bind.new_Vector_polar )

local nan_matrix= assert( bind._nan_matrix )

local new_Raw_ro, new_Raw_rw
if METQU then
    new_Raw_ro= assert( bind.new_Raw_ro )
    new_Raw_rw= assert( bind.new_Raw_rw )
end

local _latlon= assert( bind.latlon )

distance_km= assert( bind.distance_km )

-- Other C++ side functions
--
local _points_iterator= assert( bind._points_iterator )

local _areamask= assert( bind._areamask )

local _subm_set_pos= assert( bind._subm_set_pos )
local _subm_set_radius= assert( bind._subm_set_radius )

-- Cairo may be disabled in the compilation
--
local _cairo_write= bind._cairo_write

local _parse_jday= assert( bind._parse_jday )

local _LONLAT= assert( bind.LONLAT )

bind=nil

-- lua5.1 ==> 5.3: For some reason global 'type' (table is set by type.lua) is a
-- function (global function not replaced by the table) and thus type.xxx is not
-- available. Using 'typetable' global/reference set by type.lua
--
-- TODO: Should fix the root of the problem though
--
local type= typetable

assert( type(type)=="table" )
assert( type(assert)=="table" )

--local debug_getinfo= assert( debug.getinfo )

local table_concat= assert( table.concat )
local table_remove= assert( table.remove )

local string_rep=   assert( string.rep )

local math_exp=     assert( math.exp )
local math_pow=     assert( math.pow )

--
-- Small filter to pass all values via 'tonumber()'
--
function tonumber_all(...)
    local t= {}
    for i=1,select('#',...) do
        -- NOTE: Vital to have the 'select(i,...)' in extra paranthesis,
        --      to take only the FIRST (i:th) member. Without it, the whole
        --      tail will follow (and mess up 'tonumber()')
        --
        t[i]= tonumber( (select(i,...)) )
    end
    return unpack(t)
end

--
-- Calculate values for NOW and TODAY globals
--
-- NOW: current UTC time, rounded to closest hour
-- TODAY: current UTC date (0:00)
--
NOW= _parse_jday(UTC)    -- copy
    --
    if NOW.min>=30 then
        NOW= NOW+1   -- add one hour
    end
    NOW.min= 0
    NOW.sec= 0

TODAY= _parse_jday(UTC)  -- copy
    --
    TODAY.hour= 0
    TODAY.min= 0
    TODAY.sec= 0

--
-- Modify user-provided string values to appropriate type ('JDay' and 'MatrixPos')
--
do
    --
    -- [jday_ud]= parse( str )
    --
    -- Prepare for "NOW[+-]xx" and "TODAY[+-]xx" from URL.
    --
    local function parse( str )

        -- Try if it's a regular 'YYYYMMDDHHMM[SS]' string or a number (we get 'nil' if not such)
        --
        local jd= _parse_jday(str)
        if jd then
            return jd   -- done
        end

        -- Can be 'NOW[+-][nn]' or 'TODAY[+-][nn]'
        --
        local jd= NOW
        local nn= str:match( "^NOW(.*)$" )
        if not nn then
            jd= TODAY
            nn= str:match( "^TODAY(.*)$" )
        end

        return (nn=="" and jd)              -- plain 'NOW' or 'TODAY'
            or (tonumber(nn) and jd+nn)     -- 'nn' in hours
            or nil                          -- caller gives an error
    end
    
    -- Note: Use 'rawget()' to not raise a problem in command line mode if '-lstrict' is used.
    --
    -- Note: Use 'tostring()'; in command line mode, the values can be anything.
    --
    local s= rawget(_G,"validtime")
    if s then
        s= tostring(s)
        validtime= parse(s) or error( "Bad validtime: "..s, 2 )
    end
    
    s= rawget(_G,"origintime")
    if s then
        s= tostring(s)
        origintime= parse(s) or error( "Bad origintime: "..s, 2 )
    end
end

if rawget(_G,"gridsize") then
    local x,y= tonumber_all( gridsize:match("^(%d+),(%d+)$") )
    if (not x) or (x<=0) or (y<=0) then
        error( "Bad 'gridsize': "..gridsize )
    end
    gridsize= new_MatrixPos(x,y)
--
-- 16-Nov-2011 PKi: Missing gridsize seems to cause crash; no matter projection force Q2 (50,50) default
--                  as a temporary fix (native gridsize not applicable here; it would be preferable)
--
-- elseif rawget(_G,"projection") then
--
else
    -- Q2 had (50,50) as default gridsize (if we have projection only)
    --
    rawset( _G, "gridsize", new_MatrixPos(50,50) )
end


--
-- str= parameter_types(...)
--
local function parameter_types(...)
    local t= {}
    for i=1,select('#',...) do
        t[#t+1]= type(select(i,...))
    end
    return table_concat(t, " ")
end 
 
--
-- ...= split_by_underscore( str )
--
-- There is no 'string.split()' in Lua. There could be.
--
local function split_by_underscore( s )
    local ret= {}
    for w in s:gmatch("[^_]+") do
        ret[#ret+1]= w
    end
    return unpack(ret)
end
local __t= { split_by_underscore("1_2","_") }
assert( __t[1]=="1" and __t[2]=="2" and #__t==2 )

--
-- str= unwrap_table(tbl)
--
-- Note: no support for subtables (not needed for our use)
--
local function unwrap_table(t)
    local t2= {}
    for k,v in pairs(t) do
        local s
        if type.string(v) then
            s= "\""..v.."\""
        else
            s= tostring(v)
        end
        t2[#t2+1]= tostring(k).."="..s
    end
    return "{ "..table_concat(t2,", ").." }"
end
local __s= unwrap_table( { a=12, b="c" } )
assert( __s=="{ a=12, b=\"c\" }" or __s=="{ b=\"c\", a=12 }" )

--
-- ...= skip_nils( ... )
--
-- Filter 0..n parameters, returning them except for nils (order remains).
--
local function skip_nils(...)
    local t= {}
    for i=1,select('#',...) do
        local v= select(i,...)
        if v~=nil then
            t[#t+1]= v
        end
    end
    return unpack(t)
end


---=== Array help functions ===---
--
-- These are exposed to scripts, to help them handle arrays of parameters, levels etc.

--
-- { ... }= array_merge( any [, ...] )
--
-- Merges the parameters together, into an array.
-- If parameters are tables, they are unpacked _one_ level only
-- (not recursively).
--
-- Returns an array of 1..N values
--
function array_merge( ... )
    local t= {}
    for i=1,select('#',...) do
        local v= select(i,...)
        if type(v)=="table" then
            for _,vv in ipairs(v) do
                t[#t+1]= vv
            end
        else
            t[#t+1]= v
        end
    end
    return t
end

--
-- {...}= array_remove( {...} [, ...] )
--
-- Removes given values from an array. The original array remains untouched;
-- a second array object is returned.
--
function array_remove( t, ... )
    --
    -- Create lookup of the values to be removed
    --
    local skip_these={}
    for i=1,select('#',...) do
        skip_these[ select(i,...) ]= true
    end

    local t2= {}
    for _,v in ipairs(t) do
        if not skip_these[v] then
            t2[#t2+1]= v
        end
    end
    return t2
end

--
-- {...}= array_reverse( {...} )
--
-- Returns an array {Ê1, ..., N } in reversed order { N, ..., 1 }
--
local function array_reverse( tbl )
    local n= #tbl
    local ret= {}

    for i=1,n do
        ret[i]= tbl[n+1-i]
    end
    return ret
end


---=== ... ===---

--
-- latlon_ud= latlon( [projection_str, gs_pos_ud,] pos_ud )
-- latlon_ud= latlon( lat_num, lon_num )
-- latlon_ud= latlon( str )          -- i.e. "60.2N 50.7E"
-- latlon_ud= latlon( vector2d_ud )  -- '.x' as longitude, '.y' as latitude
-- latlon_ud= latlon( latlon_ud )    -- just make a copy
--
-- Handle some cases here, rest in C++.
--
-- Note: Do NOT use 'proto()' here. Area masks use latlon constructors heavily.
--
function latlon( ... )
    local n= select('#',...)
    if n==0 then
        proto("number|string|Vector|latlon|MatrixPos",...)  -- gives an error message
    end

    if n==1 and type.MatrixPos(...) then
        local proj= _G["projection"] or error( "No 'projection' global", 2 )
        local gs= _G["gridsize"] or error( "No 'gridsize' global", 2 )
        return _latlon( proj, gs, ... )

    elseif n==1 and type.Vector(...) then
        local v= select(1,...)
        return _latlon( v.y, v.x )

    else    
        return _latlon(...)
    end
end


--
-- m2_lon_lat= LONLAT( [projection_str] [,gridsize_pos] )
--
function LONLAT( proj, gs )
    proto( "[string],[MatrixPos]", proj, gs )
    
    proj= proj or _G["projection"] or error( "No default 'projection')" )
    gs= gs or _G["gridsize"] or error( "No default 'gridsize')" )

    return _LONLAT( proj, gs )
end


--
-- m2_xsize_ysize= GSIZE( [projection_str] [,gridsize_pos] )
--
function GSIZE( proj, gs )
    proto( "[string],[MatrixPos]", proj, gs )
    
    proj= proj or _G["projection"] or error( "No default 'projection')" )
    gs= gs or _G["gridsize"] or error( "No default 'gridsize')" )

    -- Get latlons of all the grid points and then calculate the distances
    --    
    local LL= _LONLAT( proj, gs )

    local mx= new_ScalarMatrix( gs, nil, proj )   -- no init
    local my= new_ScalarMatrix( gs, nil, proj )   -- no init
    
    -- Collect the sizes from 'pos' to 'pos'+(1,1). Edges are set to 0.
    --
    local dx= xy(1,0)
    local dy= xy(0,1)

    -- Note: We depend on the looping order to be positive (x: 0..gs.x-1; y: 0..gs.y-1)
    --
    for pos,ll in points(LL) do
        local ll_pos= latlon( LL[pos] )
        if pos.x<gs.x-1 then
            mx[pos]= distance_km( ll_pos, latlon(LL[pos+dx]) )
        elseif pos.x>0 then
            mx[pos]= mx[pos-dx]
        else
            mx[pos]= 0  -- width 1
        end
        if pos.y<gs.y-1 then
            my[pos]= distance_km( ll_pos, latlon(LL[pos+dy]) )
        elseif pos.y>0 then
            my[pos]= my[pos-dy]
        else
            my[pos]= 0  -- height 1
        end
    end

    return new_VectorMatrix_xy( mx, my )
end


--
-- xy= window_size_for_radius( LONLAT_vectormatrix, r_km_num )
--
-- Calculate the smallest grid window size (-X,-Y)..(X,Y) that covers an 'r_km'
-- radius anywhere within the projection area.
--
local function window_size_for_radius( LL, r_km )

    -- Start with (-1,-1)..(1,1) and grow it if some corner is not fitting in.
    --
    -- Note: On certain projections, we can possibly do this more clever, but 
    --      in generic (any) projections, we must go through all the points.
    --
    -- Suggestion: Place this to be part of the C++ 'NA_Proj' API, instead
    --      (allows easy optimizations for projection types that would know
    --      how to short cut).
    --
    local sz_x, sz_y= 1,1

    local gs= LL.size

    local function dist_less_than( ll0, ll_other )
        local km= distance_km( ll0, ll_other )
        return km <= r_km
    end

    for y0=0,gs.y-1 do
        for x0=0,gs.x-1 do
            local ll0= latlon( LL[ xy(x0,y0) ] )

            -- Grow x both ways
            --
            while true do
                local left_inside= (x0-sz_x >= 0)
                local right_inside= (x0+sz_x < gs.x)

                if (left_inside and dist_less_than( ll0, latlon(LL[ xy(x0-sz_x,y0) ] ))) or
                   (right_inside and dist_less_than( ll0, latlon(LL[ xy(x0+sz_x,y0) ] ))) then
                    sz_x= sz_x+1
                else
                    break   -- 'sz_x' fits both left and right
                end
            end

            -- Grow y both ways
            --
            while true do
                local down_inside= (y0-sz_y >= 0)
                local up_inside= (y0+sz_y < gs.y)

                if (down_inside and dist_less_than( ll0, latlon(LL[ xy(x0,y0-sz_y) ] ))) or
                   (up_inside and dist_less_than( ll0, latlon(LL[ xy(x0,y0+sz_y) ] ))) then
                    sz_y= sz_y+1
                else
                    break   -- 'sz_y' fits both down and up
                end
            end
        end
    end

LOG( "Found out window size ("..sz_x..","..sz_y..") to cover radius "..r_km.."km." )

    return new_MatrixPos(sz_x, sz_y)
end


--
-- matrix_ud | matrix2_ud= matrix( [matrixpos_ud,] [param_str,] [value_num|vector] )
--
-- DEPRECATED:
--      matrix_ud | matrix2_ud= matrix( size_x_uint, size_y_uint [,param_str] [,value_num|vector] )
--
-- Creates a new work matrix.
--
-- 'param' defines the units and scalar/vector property (if given).
-- 'value' defines the initial value for the matrix (NAN if not given).
--
-- If no size is given, global 'gridsize' is used.
--
-- TBD: Also projection should be given as a parameter ('new_ApiMatrix()' has it but we don't pass
--      on the possibility to our callers, always providing default projection matrices).
--
function matrix(...)
    -- Check for the deprecated way
    --
    if type(select(1,...))=="number" and type(select(2,...))=="number" then
        error( "'matrix( x,y, ... )' variant is deprecated, please use 'matrix( xy(x,y), ... )' instead." )
        
        -- Or allow by this:
        --
        --return matrix( xy(select(1,...), (select(2,...))), select(3,...) )
    end

    -- Note: Not using 'proto()' at all since the position of the parameters may vary.

    local arg= {...}
    local gs, param, value

    if type.MatrixPos( arg[1] ) then    
        gs= arg[1]
        table_remove( arg, 1 )
    end

    if type.string( arg[1] ) then
        param= arg[1]
        table_remove( arg, 1 )
    end

    if type.number(arg[1]) or type.Vector(arg[1]) then
        value= arg[1]
        table_remove( arg, 1 )
    end

    if #arg>0 then
        error( "Bad parameters: ".. parameter_types(...) )
    end

    if not gs then
        gs= rawget(_G,"gridsize") or error( "Global 'gridsize' not defined", 2 )
    end

    return new_ApiMatrix( gs, value or nan, param )
end

--
-- [jday_ud]= jday( [yyyymmddhhmmss_num] )
--
-- Convert a time number or string (s.a. "20100329112233") to a 'JDay' object. 
-- Usually not required (done automatically when using times).
--
-- Passes 'nil' through; this is important for allowing easy use of 'jday(_G["validtime"])'
-- (if 'validtime' is changed in the script it may be a string, not jday).
--
function jday( a )
    if a==nil then return nil end
    return _parse_jday(a) or error( "Bad date value: '"..tostring(a).."'" )
end

--
-- mi_ud= xy( x_int, y_int )
--
xy= assert( new_MatrixPos )

--
-- vector_ud= vector_xy( x_num, y_num )
--
vector_xy= assert( new_Vector_xy )

--
-- vector_ud= vector_polar( abs_num, deg_num )
--
vector_polar= assert( new_Vector_polar )

--
-- iterator_func, m_ud [, mi_ud] = points( m_ud )
-- iterator_func, m2_ud [, mi_ud] = points( m2_ud )
--
-- pos_xy, v|vector= iterator_func( m, [mi_xy] )
--
-- iterator_func2, m_ud [, mi_ud] = points( m_ud, { [window=xy,] [range_km=num,] } )
-- iterator_func2, m2_ud [, mi_ud] = points( m2_ud { [window=xy,] [range_km=num,] } )
--
-- pos_xy, subm_ud= iterator_func2( m, [mi_xy] )
--
-- Matrix iteration. Glues applications to the C++ side iteration, following
-- Lua iterator conventions (see Lua 5.1 manual, section 2.4.5).
--
-- In the first variant, points of 'm' or 'm2' are iterated.
--
-- In the second variant (used for filtering), 'window' gridsize and/or 'range_km'
-- define the scope for each point and its surroundings.
--
-- Note: This function is called only once per loop structure. After this,
--       it's the C++ side iteration function that gets called repeatedly.
--
-- Usage:
--      for pos,v|vector in points(m) do
--          ...
--      end
--
--      for pos,subm in points( m, { range_km=2 } ) do    -- 2 km radius
--          ...
--      end 
--
function points( m, opt )
    proto( "Matrix|VectorMatrix, [{ window=[MatrixPos], range_km=[number] }]", m, opt )

    if not opt then
        return _points_iterator, m, nil     -- first round: 'mi' is nil

    else
        local window= opt.window
        local range_km= opt.range_km
        local sm

        if (not window) and (not range_km) then
            error( "Either '.window' or '.range_km' must be given", 2 )

        elseif window and (not range_km) then
            sm= new_SubMatrix( m, window )
        else
            local LL
            
            if range_km then
                if not m.projection then
                    error( "Use of '.range_km' requires the matrix to have a projection.", 2 )
                end
        
                LL= _LONLAT( m.projection, m.size )
                    --
                    -- .x: longitudes
                    -- .y: latitudes
            end

            -- If window size hasn't been given, calculate big enough to always contain the
            -- given radius.
            --
            if not window then
                window= window_size_for_radius( LL, range_km )
            end

            -- Make one 'SubMatrix' object and move it around during the looping.
            --
            sm= new_SubMatrix( m, window )
            if range_km then
                _subm_set_radius( sm, range_km, LL )
            end
        end

        local pos      -- position of iteration for C++ side iterator (initially 'nil')

        -- 
        -- [pos,submatrix_ud]= func()
        -- 
        -- Note: We keep everything in the closure variables, Lua iteration API 'state' and 
        --       'var' are not used.
        -- 
        return function()
            pos= _points_iterator( m, pos )
            if not pos then
                return  -- nil (ends the iteration)
            end
            
            _subm_set_pos(sm,pos)
            return pos,sm
        end
    end
end

--
-- m_ud|m2_ud= foreach( m[2]_ud, func )
-- num|vector_ud= func( num|vector_ud )
--
-- m_ud|m2_ud= foreach( m[2]_ud [,window_size_xy] [,r_km_num] ,func2 )
-- num|vector_ud= func2( m[2]_sub_ud )
--
-- Alternative way of iteration. The given function is called per each coordinate
-- and its return values collected to an implicitly created matrix, which is returned.
--
-- Note: The first return value defines, whether the returned matrix will carry
--      scalars or vectors. It is possible to iterate a scalar matrix and return
--      a vector matrix, or vice versa.
--
-- If 'window_size' and/or 'r_km' are given, 'func2' will be called with
-- a submatrix, giving the function a "peekhole" window to the actual matrix (used
-- i.e. for weighted averaging).
--
-- If 'r_km' is given, only grid points within (and including) that distance
-- are available in the submatrix. If only 'r_km' is given, a suitable window
-- size is automatically calculated.
--
function foreach( m, ... )
    local window_size, r_km, func

    local n= select('#',...)
    if n==1 then
        proto( "Matrix|VectorMatrix, function", m, ... )
        func= ...
    elseif n==2 then
        if type.MatrixPos(select(1,...)) then
            proto( "Matrix|VectorMatrix, MatrixPos, function", m, ... )
            window_size, func= ...
        else
            proto( "Matrix|VectorMatrix, number, function", m, ... )
            r_km, func= ...
        end
    else
        --expecting n==3
        proto( "Matrix|VectorMatrix, MatrixPos, number, function", m, ... )
        window_size, r_km, func= ...
    end

    local m_ret     -- 'nil' until we know if it's supposed to be 'Matrix' or 'VectorMatrix'

    if (not window_size) and (not r_km) then
        for pos,v in points(m) do
            local vv= func(v)
            if vv then
                if not m_ret then
                    m_ret= nan_matrix(m.size, vv) or error( "'foreach' expected number or vector; got "..type(vv), 2 )
                end
                m_ret[pos]= vv
            end
        end
    else
        for pos,sm in points(m, { window=window_size, range_km=r_km }) do
            local vv= func(sm)
            if vv then
                if not m_ret then
                    m_ret= nan_matrix(m.size, vv) or error( "'foreach' expected number or vector; got "..type(vv), 2 )
                end
                m_ret[sm.center]= vv
            end
        end
    end
    return m_ret
end


--
-- iter_f= grids_by_level( raw_ud [,{ [time=jday_ud|time_str] [,reverse=bool] }] )
-- 
-- Usage:
--      for grid,lt_str[,lv_num] in grids_by_level(r) do ... end                      -- iterate from ground up
--      for grid,lt_str[,lv_num] in grids_by_level(r, {reverse=true}) do ... end      -- iterate downwards
--
-- 'lt':    "ground"|"hybrid"|"hpa"
-- 'lv':    value of the level (not provided for 'ground')
--
-- Iterate through all the levels in the raw data. Normally, iteration gives the levels rising
-- from ground upwards (reducing pressure).
--
-- 'time': If given, selects the time point at which to iterate. By default, the 
--         value of 'validtime' global defines this.
--
-- 'reverse': Iterate levels from up to ground (increasing pressure).
--
function grids_by_level(r, opt)
    proto( "Raw,[ {time=[jday|time_str], reverse=[bool]} ]", r, opt )
    opt= opt or {}

    local vt= opt.time
    local reverse= opt.reverse
    
    local levels= r.levels
    assert( levels and levels[1] )  -- always at least one level exists

    if reverse then
        levels= array_reverse(levels)
    end
    local n= 1   -- upvalue of the closure to be returned

    return function()
        local lev= levels[n]
        if not lev then
            return  -- end of looping
        else
            n= n+1
            local lt,lv= lev:match("^(.-):(.+)$")
            if lt then
                lv= tonumber(lv)
            else
                lt= lev     -- "ground"
                lv= nil     -- (is already, but...)
            end

            -- Note: Also "hPa" should work as key as well as "hpa".
            --
            local g,err= r{ [lt]=lv or true, time=vt }
            assert(g,err)

            return g, lt, lv
        end
    end
end


--
-- iter_f= grids_by_time( raw_ud [,jday|time_str [,jday|time_str]]
--                [,{ ground=[true]
--                    | hybrid=[uint]
--                    | hpa=[number]
--                  }] )
--
-- [grid, jday_ud]= iter_f()
-- 
-- Usage:
--      for g,vt in grids_by_time(r) do ... end
--
-- Iterate through all the validtimes in the raw data (or a specific range),
-- optionally limited to certain levels.
--
-- If no level specifiers are given, the first level in the data is iterated over
-- (not all the levels).
--
function grids_by_time(r, ...)

    local n= select('#',...)
    local has_opt= (n>0) and type.table(select(n,...))
    local time_params= has_opt and (n-1) or n

    proto( "Raw"..string_rep( ",[jday|time_str]", time_params )..","..
            "[{ ground=[true],"..
            "   hybrid=[uint],"..
            "   hpa=[number],"..
            "}]", r, ... )

    local jd_start, jd_end
    
    if time_params>0 then
        jd_start= select(2,...)
        if time_params>1 then
            jd_end= select(3,...)
        end
    end

    local opt= has_opt and (select(n,...)) or {}

    local opt_ground= opt.ground
    local opt_hybrid= opt.hybrid
    local opt_hpa= opt.hpa

    local times= r.times
    assert( times and times[1] )

    local n=1   -- upvalue of the closure to be returned
    return function()
        while true do
            local jd= times[n]
            if not jd then
                return  -- end of looping
            else
                n= n+1
                if (jd_start and (jd<jd_start)) or (jd_end and (jd>jd_end)) then
                    -- nothing; next
                else
                    return r{ time=jd, ground=opt_ground, hybrid=opt_hybrid, hpa= opt_hpa }, jd
                end
            end
        end
    end
end


--
-- Checking if a number or vector is NAN
--
function isnan(v)
    -- Don't use 'proto()' - this can be called in tight inner loops.

    if type.number(v) then
        return v~=v
    elseif type.Vector(v) then
        return v.isnan
    else
        -- Using 'proto' here to give an error is completely fine
        --
        proto( "number|Vector", v )
        assert(false)
    end
end
assert( isnan(0/0) )
assert( not isnan(0) )

-- either component being NAN makes the vector NAN (this could be otherways too, at least
-- for polar vectors)
--
assert( isnan( vector_xy(0/0,123) ) )
assert( isnan( vector_xy(123,0/0) ) )
--[[
assert( isnan( vector_polar(0/0,123) ) )
assert( isnan( vector_polar(123,0/0) ) )
]]

--
-- 'inf' instead of 'huge' for the global name for infinity ('math.huge' remains for METQU)
--
inf= 1/0

-- Note: To test if a number is NAN, use 'isnan()'. "x==nan" will always be false (NAN is not equal
--      to anything, not even itself).
--
nan= 0/0

--
-- Following names can be used as such, with or without quotes
-- (this eases use of common params, s.a. '{ param=T }').
--
-- This list is similar to that in 'SQD_Tools.cpp' of "standard params".
--
local globals_by_name= {
    -- Standard param names (see 'SQD_Tools.cpp')
    --
    T=true, 
    P=true, 
    Z=true, 
    THETAW=true, 
    DP=true, 
    RH=true, 
    W=true,
    RRCON= true,
    RRLAR= true,
    CAPE= true,
    KIND= true,
    POP= true,
    TKE= true,
    PSEUDOSATEL= true,
    LRAD= true,
    SRAD= true,
    VIS= true,
    AVIVIS= true,
    VERVIS= true,
    MIST= true,
    TIMESTEP= true,

    -- WeatherAndCloudiness derivatives
    --
    N= true,
    CL= true,
    CM= true,
    CH= true,
    RR= true,
    PRET= true,
    PREF= true,
    FOG= true,
    HSADE= true,
    HESSAA= true,
    THUND= true,

    -- TotalWind derivatives
    --
    WIND=true,  -- vector
    UV=true,    -- vector
    WD=true, 
    WS=true, 
    GUST= true,
    U=true, 
    V=true, 
    WVEC=true, 

    -- 16-Dec-2011 PKi: Labelizer configuration parameters (for contourcollector:config())
    --
    LimitAlongLine= true,
    LimitDirect= true,
    AllowOverlappingLines= true,
    AllowOverlappingLabels= true,
    DampenCornersLimitDeg= true,
    DampenCorners= true,
    BoostHorizontalLimitDeg= true,
    BoostHorizontal= true,
    CurveOptimizationFactor= true,

    -- 22-Dec-2011 PKi: Contouring configuration parameters (for contourcollector:config())
    --
    ContourRange= true,
    ContourList= true,
    ContourFillRange= true,
    ContourLabel= true,
    ContourLabelColor= true,
    ContourFontHeight= true,
    ContourLabelStrategy= true,
    ContourLabelsHorizontal= true,
    ContourLabelsTilted= true,
    ContourLabelBox= true,
    ContourLabelBoxFillColor= true,
    ContourLabelBoxLineColor= true,
    ContourLabelBoxLineWidth= true,
    ContourSmoothFactor= true,
    Decimals= true
} 

local globals_that_can_carry_nil= {
    validtime= true,
    projection= true,
    gridsize= true,
    --
    RPM_VERSION= true,  -- allows asking it even if it's not been set
    RPM_PACKAGE= true,
}

-- Without this, Lua command line will cause us an error
--
if METQU then
    globals_that_can_carry_nil["_PROMPT"]= true
    globals_that_can_carry_nil["_PROMPT2"]= true
end

--
-- This gets called when there's no global variable by given name ('key')
--
local function G_mt_index( t, key )

    -- TODAYnn and NOWnn variables
    --
    -- Note: We could remove these, and use 'TODAY+nn', 'NOW+nn' only (which work now)
    --      --AKa 28-May-10
    --
    if key:match("^NOW%d+$") or key:match("^TODAY%d+$") then
        error( "'NOWnn' and 'TODAYnn' discontinued. Use 'NOW+nn' or 'TODAY+nn' instead." )
    end        
        --[[ Was:
        local nn= key:match("^NOW(%d+)$")
        if nn then
            return NOW + tonumber(nn)
        else
            nn= key:match("^TODAY(%d+)$")
            if nn then
                return TODAY + tonumber(nn)
            end
        end
        ]]

    if globals_by_name[key] then
        return key  -- value is the string itself
    end

    -- Certain globals are allowed to be read, even if they're nil
    -- (C++ code does this and the user might want to do it as well;
    -- these are NOT typing mistakes so we can let them be read).
    --
    if globals_that_can_carry_nil[key] then
        return nil
    end

    -- Other keys will cause an error
    error( "Global variable '"..tostring(key).."' does not exist", 2 )
end

-- If we're in METQU mode and '-strict' has been enabled, the globals metatable is
-- indeed set (but we can overwrite its '__index' member).
--
local current_G_mt= getmetatable(_G)
if current_G_mt then
    current_G_mt.__index= G_mt_index
else
    setmetatable(_G, { __index=G_mt_index })
end

--
-- For METQU only:
--
-- Creating a new raw object (read-write):
--
-- Note: Either 'r' (template) or the options table must be given.
--
-- If 'sqd_producer' and/or 'sqd_gridsize' is given, creates an SQD format object.
-- Otherwise, an MQD object.
--
-- [raw_ud] [,err_str]= raw( [r_ud,] [{ 
--                      [origintime= jday_ud|time_str,]
--                      [times= jday_ud|time_str | { jday_ud|time_str [, ...] },]
--                      [ground= true,]
--                      [hpa= number|{number,...},]
--                      [hybrid= uint|{uint,...},]
--                      [params= str|{ str [, ...] },]
--                      [projection= str,]
--
--                      -- SQD format params:
--                      [sqd_producer= uint,] 
--                      [sqd_gridsize= xy,]
--                      [sqd_combo19= bool,]
--                      [sqd_combo326= bool,]
--
--                      -- MQD format params:
--          }] )
--
-- Opening an existing file (SQD or MQD):
--
-- [raw_ud] [,err_str]= raw( fn_mask_str, [{
--                      [origintime= jday_ud|time_str,]
--                      [times= jday_ud|time_str | { jday_ud|time_str [, ...] },]
--                      [ground= true,]
--                      [hpa= number|{number,...},]
--                      [hybrid= uint|{uint,...},]
--                      [params= str|{ str [, ...] },]
--          }] )
--
if METQU then
    function raw( ... )
        local r,opt,fn_mask

        local t1= type( select(1,...) )

        if t1=="string" then
            proto( "string, [{"..
                        "origintime=[jday|time_str],"..
                        "times=[jday|time_str|{jday|time_str,...}],"..
                        "ground=[true],"..
                        "hpa=[number|{number,...}],"..
                        "hybrid=[uint|{uint,...}],"..
                        "params=[string|{string,...}],"..
                    "}]", ... )
            fn_mask,opt= ...
        
        else
            -- Note: Testing of 't1' should be like this to give 'Raw, [{...}]" (the most
            --       generic prototype) in case unknown params were fed to the function.
            --
            local a,b
            if t1=="table" then
                a,b= "", ""
            else
                a,b= "Raw, [", "]"
            end

            proto( a.."{"..
                        "origintime=[jday|time_str],"..
                        "times=[jday|time_str|{jday|time_str,...}],"..
                        "ground=[true],"..
                        "hpa=[number|{number,...}],"..
                        "hybrid=[uint|{uint,...}],"..
                        "params=[string|{string,...}],"..
                        "projection=[string],"..
                        --
                        "sqd_producer=[uint],"..
                        "sqd_gridsize=[MatrixPos],"..
                        "sqd_combo19=[bool],"..
                        "sqd_combo326=[bool],"..
                    "}"..b, ... )
                    
            if t1=="userdata:Raw" then
                r,opt= ...
            else
                opt= ...
            end
        end

        -- 
        -- When params can be presented in a table, make sure they are (helps keep C++ side simpler)
        --
        local opt2= {}

        if opt then
            local cover_these= { hpa=true, hybrid=true, params=true, times=true }
            for k,v in pairs(opt) do
                opt2[k]= (cover_these[k] and (not type.table(v))) and {v} or v
            end
        end
    
        if fn_mask then
            return new_Raw_ro( fn_mask, opt2 )
        else
            return new_Raw_rw( r, opt2 )
        end
    end
end

--
-- m_masked|m2_masked= areamask( m_source|m2_source, [{ invert=true },] { latlon, ... } [, ...] )
-- m_mask= areamask( [projection_str, gridsize_pos, [{ value=[num], invert=[true] }],] { latlon, ... } [, ...] )
--
-- Returns (without 'm_source'):
--      A mask matrix with values:
--          'value' inside the given area (default: 1.0)
--          'value'/2 at precise edge points of the area
--          'nan' outside the area
--
-- Returns (with 'm_source'):
--      Mask applied to the source data so that all data inside (AND edges) is kept
--      and data outside of the mask is NAN. If the matrix does not have a projection
--      info, global 'projection' is used.
--
function areamask( ... )

    if type.Matrix(select(1,...)) then
        local maybe_opt= select(2,...)
        local opt= type.table(maybe_opt) and (#maybe_opt==0) and maybe_opt  -- table or false
    
        proto( "Matrix|VectorMatrix, "..
                (opt and "{ invert=true }" or "")..
                "{latlon,...}, ...", ... )

        local m_source= select(1,...)
        local skip= opt and 2 or 1
        
        local proj= m_source.projection or rawget(_G,"projection") or error( "No projection", 2 )
        local invert= opt and opt.invert

        local mask= areamask( proj, m.size, { value=0.0, invert=invert }, select(skip+1,...) )
            --
            -- edge and area to keep has 0, area to leave out has 'nan'

        return m_source + mask  -- makes anything 'outside' into 'nan'
    end

    local proj,gs,value,invert
    local skip

    if type.string(select(1,...)) then
        local maybe_opt= select(3,...)
        local opt= type.table(maybe_opt) and (#maybe_opt==0) and maybe_opt  -- table or false
        
        proto( "string,MatrixPos,"..
               (opt and "{ value=[number], invert=[true] }," or "")..
               "{latlon,...},...", ... )
        proj,gs= ...
        value= opt and opt.value or 1.0
        invert= opt and opt.invert or false

        skip= opt and 3 or 2
    else
        proto( "{latlon,...},...", select(1,...) )
        value= 1.0
        invert= false
        skip= 0
    end

    proj= proj or rawget(_G,"projection") or error( "No projection", 2 )
    gs= gs or rawget(_G,"gridsize") or error( "No gridsize", 2 )

    return _areamask( proj, gs, value, invert, select(skip+1,...) )
end


--
-- latlon_ud= lonlat( lon_num, lat_num )
--
function lonlat( lon, lat )
    proto( "number, number", lon, lat )
    return latlon( lat, lon )   -- swap the params
end


--
-- In METQU mode, modify ':write()' to support our 'output' metatable objects
-- (i.e. SVG output).
--
-- Note: All opened files seem to have the same metatable in Lua 5.1(.4) so changes
--      to that of 'io.stdout' (which is always open) will affect custom file 
--      objects as well.
--
--[==[ DISABLED (replacing output with tostring metamethod)   AKa 11-Nov-10
if METQU then
    local mt= getmetatable(io.stdout)
    assert( mt )

--[[  Metatable members:

setvbuf	function: 0x73db5a0
lines	function: 0x73db4b0
write	function: 0x73db7d0
close	function: 0x73d77e0
flush	function: 0x73d77b0
__gc	function: 0x73db830
read	function: 0x73db5f0
seek	function: 0x73db540
__index	table: 0x73db380
__tostring	function: 0x73db650
]]
    local write_orig= assert( mt.write )

    -- "Writes the value of each of its arguments to the file. The arguments must be 
    -- strings or numbers" [Lua 5.1 ref.] - or objects with 'output' metamethod.
    --
    mt.write= function( file, ... )
        for i=1,select('#',...) do
            local v= select(i,...)
            local mt= getmetatable(v)
            local mt_output= mt and mt.output

            if mt_output then
                local mime= mt_output( v, function(...) write_orig( file, ... ) end )
            else
                write_orig( file, v )
            end
        end
    end
end
]==]

