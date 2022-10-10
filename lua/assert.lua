--
-- ASSERT.LUA
--
-- This is in a separate source file for copyright reasons.
--
-- Converting the Lua 'assert' function into a namespace table (without
-- breaking compatibility with the basic 'assert()' calling).
--
-- This module allows shorthand use s.a. 'assert.table()' for asserting 
-- variable types, and is also being used by luaSub constraints system
-- (-sjacket) for testing function parameter & return types.
--
-- All in all, a worthy code and could be part of Lua future versions.
--
-- Note: the 'assert' table is available for your own assertions, too. Just add
--       more functions s.a. 'assert.myobj()' to check for custom invariants. 
--       They will then be available for constraints check, too.
--
-- Usage:   assert.int(12)  -> 12
--          assert.int(4.2, "Value not an integer")  -> error with given message
--          assert.table(a)   -> table or error
--          assert.userdata(a)  -> 'a' or error
--          ..
--
-- From:    luaSub 0.46 (MIT licensed)
--          http://luaforge.net/frs/?group_id=311
--
-- Author:  <akauppi@gmail.com>
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

-- Global changes:
--      'assert' redefined, in a backwards compatible way
--
-- Module functions:
--      none

assert( type(assert) == "function" )    -- system assert function

local math_floor= assert(math.floor)

local function is_int(v)
    return type(v)=="number" and math_floor(v)==v
end

-----
-- Integer range: INT_MIN..INT_MAX
--
local function try_maxint( n )
    return (n > n-1) and n-1   -- false when outside the integer range 
end

local INT_MAX=
    try_maxint( 2^64 ) or
    try_maxint( 2^63-1 ) or
    try_maxint( 2^32 ) or
    try_maxint( 2^31-1 ) or
    try_maxint( 2^24 ) or     -- float (24-bit mantissa)
    assert( false )

local INT_MIN= -(INT_MAX+1)


---=== assert.*() ===---

local at_msg= "type assertion error"  -- TBD: better messages, about that exact situation
local av_msg= "value assertion error"

-- void= _assert( val [, msg_str [, lev_uint]] )
--
-- Note: Unlike regular 'assert', this does not return the 'val'; upper level 
--       compatibility functions do.
--
local function _assert( cond, msg, lev ) 

    -- Original 'assert' provides no level override, so we use 'error'.
    --
    if not cond then
        error( msg or "assertion failed!", (lev or 1)+1 )
    end

--[[ 
    Through us:    
lua: ttt.lua:5: assertion failed!
stack traceback:
        [C]: in function 'error'
        scripts/assert.lua:84: in function '_assert'
        scripts/assert.lua:269: in function 'assert'
        ttt.lua:5: in main chunk
        [C]: ?

    Plain Lua:
lua: ttt.lua:5: assertion failed!
stack traceback:
        [C]: in function 'assert'
        ttt.lua:5: in main chunk
        [C]: ?
        
    So the first line is 'right' but stack trace does include us. Cannot be
    avoided?
]]
end

-- Note: following code uses the new assert by purpose, since it provides
--       a level override (original doesn't)
--
local function assert_v_gen( v0 )
    return function(v,msg,lev) 
                _assert( v == v0, msg or av_msg, (lev or 1)+1 )
                return v
           end
end
local function assert_t_gen( str )
    return function(v,msg,lev) 
                _assert( type(v) == str, msg or at_msg, (lev or 1)+1 )
                return v
           end
end
local function assert_t2_gen( re_str )
    -- 
    -- Be ready for extended 'type()' returning "userdata:XXX" or "table:XXX"
    --
    -- Userdata specific functions can be added to 'assert.xxx' (we want to keep the
    -- "value,message,level" calling convention for all asserts, so userdata subtype
    -- names would not fit in here).
    --
    return function(v,msg,lev) 
                -- TBD: 'type()' returning two values is for subtypes (see 'type.lua')
                --      This should work ok with normal 'type', too, if subtypes are
                --      not mentioned.
                --
                _assert( type(v):match(re_str), msg or at_msg, (lev or 1)+1 )
                return v
           end
end

--
assert= {
    -- Allow use of levels also for the plain 'assert()'; good for custom
    -- assert function makers.
    --
    __call= function(_,v,msg,lev)     -- plain 'assert()' (compatibility)
            _assert( v, msg, (lev or 1)+1 )
            return v
        end,

    -- Hopefully, Lua will allow use of 'nil', 'function' and other reserved 
    -- words as table shortcuts in the future (5.1 does not). 
    --
    ["nil"]= assert_v_gen( nil ),
    bool= assert_t_gen "boolean",       -- alias
    boolean= assert_t_gen "boolean",
    ["function"]= assert_t_gen "function",
    string= assert_t_gen "string",
    number= assert_t_gen "number",
    
    userdata= assert_t2_gen "^userdata",   -- regexp
    table= assert_t2_gen "^table",         -- regexp

    char= function( v, msg, lev )
        _assert( type(v)=="string" and #v==1, msg or at_msg, (lev or 1)+1 )
        return v
    end,

    int= function( v, msg, lev )
        _assert( is_int(v), msg or at_msg, (lev or 1)+1 )
        return v
    end,

    uint= function( v, msg, lev )
        _assert( is_int(v) and v>=0, msg or at_msg, (lev or 1)+1 )
        return v
    end,
    
    ['true']= assert_v_gen( true ),
    ['false']= assert_v_gen( false ),

    any= function( v, msg, lev )
        _assert( v ~= nil, msg or av_msg, (lev or 1)+1 )
        return v
    end,

    -- ...
}
setmetatable( assert, assert )


-----    
-- void= assert.fails( function [,err_msg_str] )
--
-- Special assert function, to make sure the call within it fails, and gives a 
-- specific error message. Used in unit testing.
--
function assert.fails( func, err_msg )
    --
    assert["function"]( func, "Expected a function", 2 )

    local st,err= pcall( func )
    if st then
        _assert( false, "Block expected to fail, but didn't.", 2 )

    else
        if err_msg then
            -- 'err': "file.lua:5: xxx"
            --
            local true_err= string.match( err, "^.-%:%d+%:%s(.*)$" )
            _assert(true_err)

            if true_err ~= err_msg then
                _assert( false, "Failed with wrong error message: \n"..
                           "'"..true_err.."'\n"..
                           "expected: '"..err_msg.."'", 2 )
            end
        end
    end
end


-----    
-- void= assert.failsnot( function )
--
-- Similar to 'assert.fails' but expects the code to survive.
--
function assert.failsnot( func )
    --
    assert["function"]( func, "Expected a function", 2 )

    local st,err= pcall( func )
    if not st then
        _assert( false, "Block expected NOT to fail, but did."..
                        (err and "\n\tError: '"..err.."'" or ""), 2 )
    end
end


-----    
-- void= assert.nilerr( function [,err_msg_str] )
--
-- Expects the function to return with 'nil,err' failure code, with
-- optionally error string matching. Similar to --> 'assert.fails()'
--
function assert.nilerr( func, err_msg )
    --
    assert["function"]( func, "Expected a function", 2 )

    local v,err= func()
    _assert( v==nil, "Expected to return nil, but didn't: "..tostring(v), 2 )
    _assert( (not err_msg) or (err==err_msg),
                    "Failed with wrong error message: \n"..
                    "'"..err.."'\n"..
                    "expected: '"..err_msg.."'", 2 )
end


-- Sanity check
--
assert( true )
assert.fails( function() assert( false ) end )
assert.fails( function() assert( nil ) end )

