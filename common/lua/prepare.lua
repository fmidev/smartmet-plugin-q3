--
-- PREPARE.LUA                              Copyright 2009, Ilmatieteen laitos
--
-- Preparations for a Lua state to be used in Q3 Plugin. This code is baked
-- into the Q3 Plugin binary via 'luac'.
--

-- C side exports
--
local bind, METQU, package_path, package_cpath= ...

local matrix_min= assert( bind._min )
local matrix_max= assert( bind._max )
local matrix_avg= assert( bind._avg )
local matrix_sum= assert( bind._sum )
local matrix_count= assert( bind._count )
local _parse_jday= assert( bind._parse_jday )
local LOG_ONE_UP= assert( bind.LOG_ONE_UP )

-- lua5.1 ==> 5.3: For some reason global 'type' (table is set by type.lua) is a
-- function (global function not replaced by the table) and thus type.xxx is not
-- available. Using 'typetable' global/reference set by type.lua
--
-- TODO: Should fix the root of the problem though
--
local type= typetable

assert( type(type)=="table" )
assert( type(assert)=="table" )

testraw= bind.testraw

--
-- Lua 5.0 features depricated in 5.1 (we don't need them)
--
if not METQU then
    math.mod= nil       -- now 'math.fmod'
    string.gfind= nil   -- now 'string.gmatch'
    table.setn= nil
    table.getn= nil
    loadlib= nil
    table.foreach= nil
    table.foreachi= nil
    gcinfo= nil
end

assert( package )
assert( table )
if not METQU then
    assert( io==nil )
end

local os_orig= assert( os )
local os_time= assert( os.time )
assert( string )

-- By making these into locals we make sure user changes won't affect us.
--
local math_floor= assert( math.floor )
local string_rep= assert( string.rep )
local table_concat= assert( table.concat )

-- Extension modules (at least 'svgcore') will appreciate 'debug.getinfo'
--
assert( debug and debug.getinfo )

if not METQU then
    coroutine= nil      -- not needed (wouldn't be harmful, though)
    dofile= nil
    load= nil
    loadfile= nil
end

--
-- [str]= DUMP( any [, ...] )
--
-- Debugging interface to get table constructs (and anything) on the LOG.
--
function DUMP( ... )
    --
    -- str= DUMP_ONE( any, indent_uint )
    --
    local function DUMP_ONE(v, indent)
        local indent_str= string_rep("  ",indent)

        if type(v)=="table" then
            local arr= { "{\n" }
            -- First 1..n
            for i,vv in ipairs(v) do
                arr[#arr+1]= indent_str..i..": "..DUMP_ONE(vv,indent+1).."\n"
            end
            for k,vv in pairs(v) do
                if type(k)~="number" or (math_floor(k)~=k) or (k<=0) then
                    arr[#arr+1]= indent_str..tostring(k)..": "..DUMP_ONE(vv,indent+1).."\n"
                end
            end
            arr[#arr+1]= indent_str.."}"

            return table_concat(arr)
    
        elseif type(v)=="number" then
            return v
        elseif type(v)=="string" then
            return "\""..v.."\""
        else
            return tostring(v)
        end
    end
    
    local ss={}
    for i=1,select('#',...) do
        ss[#ss+1]= DUMP_ONE( select(i,...), 0 )
    end
    LOG_ONE_UP( table_concat(ss,'\n') )  -- report the level that called us
end


-- 
-- Prototype additions to non-C++ custom types
--
assert( proto )

--
-- Time can be "YYYYMMDDHHMM[SS]" (either string or number)
--
function proto.time_str(v) 
    return _parse_jday(v) ~= nil    -- use C++ side to do actual check
end

--
-- Time as seconds since epoch (usually 1970 Jan 1st UTC)
--
proto.epoch_uint= proto.uint

-- 
-- Only selected parts of 'os.*' are left through
--
if not METQU then
    os= {
        clock=      os_orig.clock,
        date=       os_orig.date,
        difftime=   os_orig.difftime,
        time=       os_orig.time,
    }
end

local skip_in_math= {
    huge= true      -- don't put it to global namespace (we get 'inf')
}

-- 
for k,v in pairs(math) do
    local bind_func= bind[k]
    if bind_func then
        -- Craft a little wrapper around each 'abs()', 'ceil()' etc. that will
        -- divert to either matrix operations or the original 'math.abs()' etc.
        --
        _G[k]= function(...)
            if type.Matrix(select(1,...)) then
                return bind_func(...)
            else
                return v(...)
            end
        end
    elseif not skip_in_math[k] then
        _G[k]= v    -- pi, random etc.
    end
end

--
-- matrix_ud|num= min|max( matrix_ud|num|nil, matrix_ud|num|nil [, ...] )
-- num, pos_ud [,meta_num] = min|max( matrix_ud )
--
-- Returns a _member_wise_ minimum or maximum of a number of matrices and/or
-- scalars (with pure scalars, works almost as 'math.min' and 'math.max' in
-- plain Lua except that ORDER OF PARAMETERS MUST NEVER AFFECT THE RESULT.
-- With Lua, 'min(nan,0)' and 'min(0,nan)' can be different).
--
-- 'nil' parameters are allowed (though Lua stock 'min/max' does not) to make
-- collection of min/max to a storage that is initially 'nil' easier.
--
-- With just one matrix parameter, reduces the minimum/maximum of that matrix,
-- and provides the location information + possible meta value of it (a meta
-- value i.e. indicates which height the value was originally from).
--
-- NAN numeric values are treated like 'nil', i.e. simply ignored (again, this
-- is not how plain Lua 'min'/'max' does but this is more predictable).
--
local function gen_minmax( orig_f, matrix_f )
    assert( orig_f and matrix_f )
    
    -- Do a wrapper to filter out bad arguments and to guarantee a matrix is
    -- the first parameter (C++ side functions expect that)
    --
    return function(...)
        local res   -- result so far (nil/num/matrix_ud)

        local n= select('#',...)
        
        if (n==1) and type.Matrix(select(1,...)) then   -- allow "userdata:xxx"
            return matrix_f(...)      -- reduce to a number
        end

        local res_m, res_v  -- separate collectors for min/max matrix and scalar

        for i=1,n do
            local v= select(i,...)
            if type.Matrix(v) then
                res_m= (not res_m) and v or matrix_f(res_m,v)    -- all matrices' min/max
            elseif v==nil or isnan(v) then
                -- skip it
            elseif tonumber(v) then
                res_v= (not res_v) and v or orig_f(res_v,v)      -- all scalars' min/max
            else
                error( "Bad parameter #"..i.." to min/max: "..type(v), 2 )
            end
        end

        if res_m and res_v then
            return matrix_f(res_m,res_v)    -- matrix needs to be first
        else
            return res_m or res_v or nan    -- never return 'nil'
        end
    end
end

min= gen_minmax( math.min, matrix_min )
max= gen_minmax( math.max, matrix_max )

--
-- matrix_ud|num= sum( matrix_ud|nil, [, ...] )
-- num = sum( matrix_ud )
-- num = sum( num [, ...] )
--
-- Similar to min/max, returns the sum of matrices location-wise, or the sum
-- of all non-nan members of a single matrix.
--
-- As in min/max, 'nil' parameters are allowed to make use of the function
-- easy.
--
function sum(...)
    if type.number(select(1,...)) then
        proto( "number,...", ... )

        local n= select('#',...)
        local sum= 0
        for i=1,n do 
            local v= select(i,...)
            if not isnan(v) then
                sum= sum+v
            end
        end
        return sum
    else
        return matrix_sum(...)
    end
end

--
-- matrix_ud|num|NAN= avg( matrix_ud|nil, [, ...] )
-- num|NAN = avg( matrix_ud )
-- num|NAN = avg( num [, ...] )
--
-- Similar to min/max, returns the average of matrices location-wise, or the
-- average of all non-nan members of a single matrix.
--
-- As in min/max, 'nil' parameters are allowed to make use of the function
-- easy.
--
function avg(...)
    if type.number(select(1,...)) then
        return sum(...) / count(...)
    else
        return matrix_avg(...)
    end
end

--
-- matrix_ud|uint= count( matrix_ud|nil, [, ...] )
-- uint = count( matrix_ud )
-- uint= count( num [, ...] )
--
-- Similar to min/max, returns the count of non-NAN values in the matrices.
--
-- As in min/max, 'nil' parameters are allowed to make use of the function
-- easy.
--
function count(...)
    if type.number(select(1,...)) then
        proto( "number,...", ... )

        local n= select('#',...)
        local count= 0
        for i=1,n do 
            if not isnan(select(i,...)) then
                count= count+1
            end
        end
        return count
    else
        return matrix_count(...)
    end
end


-- '__pow' metamethod affects '^' but not the global 'pow()' or 'sqrt()' functions
--
function pow(a,b) return a ^ b end
function sqrt(a) return a ^ 0.5 end

-- 'math.deg' and 'math.rad' are simple conversions; this will make them work
-- on matrices and numbers alike.
--
local RADIANS_PER_DEGREE= (math.pi/180.0)

function rad(x) return x*RADIANS_PER_DEGREE end
function deg(x) return x/RADIANS_PER_DEGREE end

--
-- package.*
--
package.loadlib= nil    -- all package loading via 'require'

-- Restrict where 'require' fetches the addons (in METQU, Lua default feature
-- of LUA_PATH and LUA_CPATH env.vars applies)
--
if not METQU then
    package.path= package_path
    package.cpath= package_cpath
end

--
-- Make sure 'package' table is read-only (not allowing change of .path or
-- .cpath; security issue)
--
-- Note: Lua proxy tables are empty tables, which have metamethods taking
--       care of read/write events. If a value were to inserted in the proxy,
--       accessing it would bypass the metamethod processing.
--
local package_orig= assert( package )

package= setmetatable( {}, {
    __index= function( _, k )
        return package_orig[k]
    end,
    __newindex= function( _, k, v )
        error( "package table is read-only", 2 )    -- level that did the write
    end
} )


--
-- str= concat( tbl [, sep [, i [,j]]] )
--
-- Like 'table.concat()' but also handles non-string parameters (via 'tostring()')
--
function concat( t1, ... )
    local t2={}
    for i,v in ipairs(t1) do
        t2[i]= tostring(v)
    end
    return table_concat(t2,...)
end


--
-- tbl= flatten( tbl [, ...] )
--
-- Given multiple tables, merges their fields. 
--
-- Array entries (1..n) are appended, in the order they occur within the tables, and their subtables,
-- and so on (this function flattens any depth). 
--
-- Non-array entries (i.e. string keys) are merged, with the last entry overruling earlier ones.
--
function flatten(...)
    local t={}

    local flatten_to_t; flatten_to_t= function(tt)     -- allows self-recursiveness; 't' is an upvalue
        assert( type(tt)=="table" )
        
        -- Handle array entries
        --
        for _,v in ipairs(tt) do
            t[#t+1]= v
        end
        
        -- Handle non-array entries
        --
        for k,v in pairs(tt) do
            if (type(k)~="number") or (floor(k)~=k) or (k<=0) then
                t[k]= v
            end
        end
    end

    for i=1,select('#',...) do
        local v= select(i,...)
        assert( type(v)=="table" )
        flatten_to_t(v)
    end
    return t
end


--
-- Make 'print' be able to output matrices (in the command line use)
--
--[[ NOT NEEDED, using '__tostring' metamethod makes this automatic
if METQU then
    local print_orig= assert(print)
    
    print= function(...) 
        local args= {...}
        for i,v in ipairs(args) do
            if type.Matrix(v) then
                -- exchange the matrix to its contents as a string
                --
                args[i]= tostring(v)
            end
        end
        return print_orig( unpack(args) )
    end
end
]]


