--
-- PROTO.LUA                            Copyright 2010, Ilmatieteen laitos
--
-- Help feature to check call parameters at runtime.
--
-- This code is based on Asko Kauppi's 'assert.lua' (copyright below)
--
--[[
/******************************************************************************
* Copyright (C) 2006-08, Asko Kauppi.
*
* Permission is hereby granted, free of charge, to any person obtaining
* a copy of this software and associated documentation files (the 
* "Software"), to deal in the Software without restriction, including   
* without limitation the rights to use, copy, modify, merge, publish,
* distribute, sublicense, and/or sell copies of the Software, and to
* permit persons to whom the Software is furnished to do so, subject to   
* the following conditions:
*
* The above copyright notice and this permission notice shall be
* included in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
******************************************************************************/
]]--

local assert=       assert
local type=         type
local tonumber=     tonumber
local pairs=        pairs
local ipairs=       ipairs
local tostring=     tostring
local tonumber=     tonumber
local setmetatable= setmetatable
local error=        error
local select=       select
local unpack=       unpack

local math_floor=   assert(math.floor)
local math_abs=     assert(math.abs)
local table_concat= assert(table.concat)

-- Note: 'io' is not available in server mode
--
local io_stderr=    rawget(_G,"io") and io.stderr   -- 'io' not available in sandboxed use (i.e. server)
local LOG=          rawget(_G,"LOG")                -- available in 'q3' server

module "proto"

local m= _ENV  -- my namespace which 'module' did
assert( type(m)=="table" )

-----
-- Proto syntax:
--
--  constraint:
--      number, numerical, uint, int
--      bool, true, false
--      string
--      table                           any table (works same as "{}" constraint)
--      function
--      range(a,b)                      values a<=x<=b (of any type)
--      { field=constraint [, ...] }    table with certain fields
--      { constraint, ... }             array of certain fields
--
--      [constraint]                    the constraint is optional
--      constraint1|...                 any of 2 or more constraints (only one of which can have 
--                                      '()' or '{}' and must be be last)
--
-- Multiple constraints:
--      constraint1[,constraint2[,...]]     -- there may be spaces after the comma
--
-- Note:
--      With tables, fields other than those requested are allowed (and ignored).
--      Especially, tables prototyped as 'array' can still have non
--
-- The proto system uses 'proto.*' functions to actually do the assertion.
-- This allows introduction of custom prototypes simply by adding their check
-- functions as 'proto.mytype'.
-- 
-----


---=== Helpers ===---

local function ith(i)
    return i..(({ "st", "nd", "rd" })[i%10] or "th")
end

--
-- DEBUG( ... )
--
local DEBUG= LOG or function(...)
    io_stderr:write( "DEBUG >>> ", ... )
    io_stderr:write "\n"
end


--
-- constraint_func, tail_str= constraint( str, [ends_with_str], lev_uint )
--
-- Extract the first constraint from 'str' and evaluate it into a function.
--
-- 'tail':      Remaining string after the extracted constraint (starting with ','
--              if more constraints; "" if done).
--
-- 'ends_with': "%]" or "%)"
--              required to be after the constraint being extracted
--              (eaten away, before a possible comma).
--
-- Observe:
--      "\[constraint\]"                optional constraint
--      "{ [constraint,] [field=constraint] [, ...] }"  table of constraints
--      "range(10,20)"                  constraint with variables
--
local function constraint( str, ends_with, lev )
    assert( str ~= "" )

    local c= str:sub(1,1)
    ends_with= ends_with or ""

    if c=="[" then
        local first,tail= constraint( str:sub(2), "%]", lev+1 )
        assert(first)
        
        return function(v)
            if v==nil then return true end  -- the optional part
            return first(v)     -- actual
        end, tail

    elseif c=="{" then
        local str_full= str     -- for error message
        str= str:sub(2)

        -- We expect either:    "constraint[, ...]}"
        --                      "field= constraint[, ...]}"
        --                      "[int]= constraint[, ...]}"
        --                      "...}"  further fields (if any) are of the last unkeyed constraint
        --                              (must be the end of the table constraint string)
        --                      "}"
        --
        local tbl= {}   -- key -> constraint_f

        local array_first_index     -- Apply this constraint to anything beyond it

        while str:sub(1,1)~="}" do
            -- Tail is expected to have something, if not 'field=' will
            -- be treated as the key (and an error message given).
            --
            local key,tail= str:match( "^([%w_]+)=(.+)$" )  -- field= constraint
            if not key then
                key,tail= str:match( "^%[(.-)%]=(.+)$" )    -- [int]= constraint
                key= tonumber(key)  -- makes anything non-numerical go 'nil'
            end
            if key then
                str= tail
            end
            
            -- Subconstraint will end with ',' or '}' (not eaten)
            --
            tbl[key or (#tbl+1)], str= constraint( str, nil, lev+1 )

            str= str:gsub("^,","")  -- remove ',' from the start of tail (can also be '}')

            -- If we are ending with "...}", make that into a special constraint
            --
            local tail= str:match( "^%.%.%.(}.*)$" )
            if tail then
                array_first_index= #tbl     -- last array constraint that was given
                if array_first_index==0 and (not tbl[0]) then   -- '{ [0]=string, ... }' is a valid constraint
                    error( "Bad constraint (no array part to repeat): "..str_full, lev )
                end
                str= tail   -- starts with '}'
                break
            end
        end
        
        local tail= str:match( "^}"..ends_with.."(.*)$" )
        if not tail then
            -- 'ends_with' was not there
            error( "Bad constraint (no "..ends_with:sub(-1).."): "..str_full, lev )
        end

        -- 'tbl' has all constraints
        --
        return function(v)
            if type(v)~="table" then return false end

            -- Check constraints in 'tbl' but let 'v' have unannounced fields
            -- ("duck is a duck even with a coat on").
            --
            for k,f in pairs(tbl) do
                if not f(v[k]) then
                    return false, "item '"..k.."' not matching ("..tostring(v[k])..")"
                end
            end

            -- If ", ..." at the end of the constraint, check the tail for being like
            -- the last constraint before it was (i.e. "{ [string], ... }")
            --
            if array_first_index then
                local f= tbl[array_first_index]
                for i=array_first_index+1, #v do
                    if not f(v[i]) then
                        return false, "item "..i.." not matching ("..tostring(v[i])..")"
                    end
                end
            end

            return true -- ok
        end, tail

    else
        -- Regular constraints (i.e. "uint", "range(2,10)")

        -- Is there '(' before a comma
        --
        local first,params,tail= str:match( "^([^,]-)%((.-)%)"..ends_with.."(.*)$" )

        if not first then
            -- Split 'str' at '|' for alternative constraints.
            --
            -- Note: Only the LAST alternative may contain '(' ')' '{' '}'.
            --
            -- i.e. "uint|string|{uint|string,...}"
            --
            local alts= {}

            while true do
                local a,b= str:match( "^([%w_]+)|(.+)$" )
                if a then
                    alts[#alts+1]= constraint( a, nil, lev+1 )    -- no tail
                    str= b
                elseif alts[1] then
                    -- Last alternative, which may be a table or functional (s.a. 'range()'),
                    -- for which we must recall 'constraint'.
                    --
                    local tail
                    alts[#alts+1], tail= constraint( str, ends_with, lev+1 )
                    return function(v)
                        for _,ff in ipairs(alts) do
                            if ff(v) then return true end   -- any one match is enough
                        end
                        return false
                    end, tail
                else
                    break
                end
            end

            -- Note: Must take two different paths, based of whether 'ends_with' is 
            --      used or not (wasn't able to merge them into one functional regex).
            --
            if ends_with=="" then
                first,tail= str:match( "^([^,}]+)(.*)$" )     
            else
                first,tail= str:match( "^([^,}]-)"..ends_with.."(.*)$" )
            end
            if not first then
                error( "Bad constraint (#2): '"..str.."'", lev )
            end
        end

        local f= m[first]
        if not f then
            error( "Bad constraint (#3): '"..first.."'", lev )
        end

        if params then
            -- Params is split into a table as well (i.e. "2,5" -> '{ 2,5 }')
            --
            local params_tbl= {}
            for w in params:gmatch("[^,]+") do
                params_tbl[#params_tbl+1]= w
            end
            f= f( unpack(params_tbl) )  -- generate the actual constraint function
        end
        
        return f,tail
    end
end


--
-- { proto_func [, ...], [-1..N]= constraint_str, tail=[true] }= proto_funcs( str )
--
local function proto_funcs( s )
    local tbl= {}
    local lev= 3   -- level to throw errors at

    while s~="" do
        local f, tail= constraint(s, nil, lev+1)
        local i= #tbl+1
        tbl[i]= f

        -- Mark our constraint string to 'tbl[-i]' for error messages
        --
        local head= s:sub(1,#s-#tail)
        if head:sub(-1)==',' then
            head= head:sub(1,-2)    -- remove terminating ','
        end        
        tbl[-i]= head

        assert(tail)    -- starts with ',' (or at least "")
        s= tail:gsub("^,","")
        
        if s=="..." then    -- ending with ", ..." (more of the last is allowed)
            tbl.tail= true
            break
        end
    end
    return tbl
end


---=== Proto constraints ===---

local function num_is_int(v)
    return math_abs( math_floor(v)-v ) < 1e-9
end

--
-- bool= proto.uint(v)
--
function uint(v)
    return type(v)=="number" and num_is_int(v) and (v>=0)
end

--
-- bool= proto.int(v)
--
function int(v)
    return type(v)=="number" and num_is_int(v)
end

--
-- bool= proto.bool(v)
--
function bool(v)
    return type(v)=="boolean"
end

m["true"]= function(v)  -- 'true' is a reserved word
    return v==true
end

m["false"]= function(v) -- 'false' is a reserved word
    return v==false
end

--
-- bool= proto.num[ber](v)
--
-- Note: We are strict that 'v' must be a number. Either convert numerical strings
--      in application script via 'tonumber()' or use the 'numerical' constraint.
--
function number(v)
    return type(v)=="number"
end
num= number     -- alias

--
-- bool= proto.numerical(v)
--
-- Note: Relaxed number interpretation, number or numerical string (that Lua would
--      automatically convert to a number) will do.
--
function numerical(v)
    return tonumber(v) and true or false
end

--
-- bool= proto.string(v)
--
function string(v)
    return type(v)=="string"
end
str= string     -- alias

--
-- bool= proto.function(v)
--
m["function"]= function(v)      -- 'function' is a reserved word
    return type(v)=="function"
end

--
-- bool= proto.table(v)
--
function table(v)
    return type(v)=="table"
end

--
-- bool= proto.any(v)
-- bool= proto["*"](v)
--
-- Anything but not 'nil'. To allow even nil, use "[any]".
-- Or: "*", "[*]".
--
function any(v)
    return v~=nil
end
m["*"]= any

--
-- func= proto.range( a, b [, ...] )
-- bool= func(v)
--
-- Note: Range can be used with any comparable class (numbers, strings or
--      user classes with '<', '>' operators defined.
--
function range( a,b, ...)
    if select('#',...)>0 then
        error( "Bad constraint (range takes two params)", 2 )
    end

    return function(v)
            return (v>=a) and (v<=b)
        end
end


---=== Proto handling (public API) ===---

-----
-- cache[ proto_str ] --> { [1..n]= proto_function }
--
local cache= {}

--
-- ...= proto( m_tbl, str, ... )
--
-- Match the other params to the prototype expressed in 'proto' string.
-- If the types don't match, an error is thrown (the call returns only on matching types).
--
-- Returns the '...' parameters fed to the function. This is useful i.e. for use
--      of the 'proto' system with syntax modifiers.
--
-- TBD: If there were a way to get all the parameters of the calling function, _without_
--      explicitly providing them, it would be marvellous. They are a continuous range
--      on the stack, so we should be able to do that. Or not.
--
local function mt_call( _, s, ... )
    local lev= 3    -- 3 shows the place in user's script (one level above 'proto()' call)

    -- Remove _all_ whitespace from the proto string
    --
    s= s:gsub( "%s+", "" )

    local protos= cache[s]  -- have we encountered it earlier?
    if not protos then
        protos= proto_funcs(s)
        cache[s]= protos
    end

    -- Handle parameters in order (leave tail last); most logical for the users
    --
    for i,f in ipairs(protos) do
        local v= select(i,...)
        local ok,details= f(v)
        if not ok then
            local msg= "Bad "..ith(i).. " parameter: "..tostring(v)
            if details then
                msg= msg.." ("..details..")"
            else
                msg= msg.." ('"..protos[-i].."' expected)"
            end
            error( msg, lev )
        end
    end

    local have_n= select('#',...)
    local want_n= #protos

    -- Allow explicity 'nil's in the top params (Lua does differ between them slightly,
    -- but we can consider missing params and 'nil' the same).
    --
    for k=have_n,want_n+1,-1 do
        if select(k,...)==nil then
            have_n= have_n-1    -- ignore such tailing nil (like it wasn't there)
        else
            break
        end
    end
    
    -- If 'protos.tail'==true, any number of parameters (with the constraint of the last one)
    -- is allowed.
    --
    if protos.tail then
        local f= protos[want_n]     -- last constraint
        for i=want_n+1, have_n do
            local v= select(i,...)
            if not f(v) then
                error( "Bad "..ith(i).." parameter: "..tostring(v), lev )
            end
        end
    elseif have_n > want_n then
        error( "Too many parameters: "..have_n.." > "..want_n, lev )
    end
    
    return ...      -- param passthrough on success
end

setmetatable( m, { __call= mt_call } )


-- 
-- Selftests
--
do
    -- Sample that failed in real world
    --
    -- Note: Don't use any spaces, 'mt_call()' strips them of constraints before passing
    --       to 'constraint()'.
    --
    local c= constraint( "{xxx=uint}", nil, 0 )
    assert(c)

    c= constraint( "[{a=[uint|true],"..
                      "b=[uint|{uint,...}],"..
                      "c=[string|{string,...}],"..
                      "d=[string|{string,...}]"..
                    "}]", nil, 0 )
    assert(c)

    -- Also this failed in real world
    --
    -- NOTE: DO _NOT_ USE SPACES. (it said so above, but... DO NOT!)
    --
    c= constraint( "{times=[uint|string|{uint|string,...}]}", nil, 0 )
    assert(c)

    c= constraint( "[{times=[uint|string|{uint|string,...}]}]", nil, 0 )
    assert(c)
end
