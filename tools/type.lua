--
-- TYPE.LUA
--
-- This is in a separate source file for copyright reasons.
--
-- str= type()
-- bool= type.xxx()
--
-- Extension to the Lua 'type()' function, allowing testing for a certain type
-- by a single function call, also covering custom tests for particular userdata.
--
-- Usage:   type.int(12)    -> true
--          type.int(4.2)   -> false
--          type.table(a)   -> true/false
--          type.userdata(a,"Matrix")  -- must have subtype "Matrix"
--          ..
--
-- From:    luaSub 0.46 (MIT licensed)
--          http://luaforge.net/frs/?group_id=311
--
-- Author:  <akauppi@gmail.com>
--
--[[
/******************************************************************************
* Copyright (C) 2006-07, Asko Kauppi.
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

local old_type= type
assert( type(old_type) == "function", "'type' was: "..tostring(old_type) )


---=== type.*() ===---

-- Usage:
--      type.number(v)
--      <=>
--      type(v)=="number"
--
--      type.userdata(v,subtype)
--      <=>
--      old_type(v)=="userdata" and getmetatable(v).__type==subtype
--
-- Note: You can also add your own type checks into the 'type' table.
--       "type.int()" and "type.uint()" are supported directly.
--
local function new_type( v )
    local ot= old_type(v)
    local mt= getmetatable(v)
    if mt and mt.__type then
        return ot..":"..(mt.__type)
    end
    return ot
end

-- version for "table" and "userdata" that could have metatables
--
-- One can check either just the main type ('type.userdata(some)') or
-- also the subtype at the same time: 'type.userdata(some,"subtype")'
-- Extendible by multiple type names: 'type.userdata(some,"a","b","c")'
--
local function LTF_mt( str )
  return
    function(v,...)
        local ot= old_type(v)
        if ot ~= str then
            return false    -- main type something else
        else
            local n= select('#',...)
            if n==0 then
                return true -- no subtype required
            else
                -- Particular subtype (any of the ones named) required
                --
                local mt= getmetatable(v)
                local v_subtype= mt and mt.__type
                if v_subtype then
                    for i=1,n do
                        if v_subtype==select(i,...) then
                            return true     -- was of the right subtype
                        end
                    end
                end
                return false    -- wrong subtype
            end
        end
    end
end

type= {
  userdate= LTF_mt "userdata",
  table= LTF_mt "table",

  __call= function(_,...) return new_type(...) end,     -- system function

  -- Use the 'proto.*' type system for us, too
  --
  __index= function( _,k )
            local f= proto[k]
            if not f then
                error( "Unknown type: "..k )
            end
            return f    -- bool= f(v)   (just like we need it)
           end,
           
    __type= false
}
setmetatable( type, type )

-- lua5.1 ==> 5.3: For some reason global 'type' is a function e.g. in q3.lua
-- (not replaced by the type table) and thus type.xxx is not available. Simply setting
-- another global/reference to the table and using 'local type= typetable' where needed
--
-- TODO: Should properly fix the problem though
--
typetable= type

-- Sanity check
--
assert( type(5) == "number" )
assert( type(type) == "table" )

assert( type.number(5) )
assert( type.table(type) )

do
    local o= setmetatable( {}, { __type="Abc" } )
    assert( type.table(o) )
    assert( type.table(o,"Abc") )
    assert( not type.table(o,"Xxx") )
    assert( type.table(o,"Xxx","Abc") )
end

