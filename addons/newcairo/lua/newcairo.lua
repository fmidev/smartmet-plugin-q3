--
-- NEWCAIRO.LUA                            Copyright 2010, Ilmatieteen laitos
--
-- Upper level chunk of NewCairo binding. Baked into the binary module and
-- called like this (by the binding):
--
--  = chunk( private_binding_tbl, module_name_str [, METQU_bool] )
--
local m, mod_name, METQU= ...

-- lua5.1 ==> 5.3: For some reason global 'type' (table is set by type.lua) is a
-- function (global function not replaced by the table) and thus type.xxx is not
-- available. Using 'typetable' global/reference set by type.lua
--
-- TODO: Should fix the root of the problem though
--
local type= typetable

assert( type(m)=="table" )
assert( type(mod_name)=="string" )

local m_image_surface=  assert( m.image_surface )
local m_image_surface_from_png= METQU and assert(m.image_surface_from_png)
local m_pdf_surface=    m.pdf_surface   -- nil if no PDF support
local m_svg_surface=    m.svg_surface   -- nil if no SVG support

local m_pattern_for_surface= assert( m.pattern_for_surface )

require "proto"

assert( proto.CairoSurface )
assert( proto.CairoContext )
assert( proto.CairoPattern )
assert( proto.CairoMatrix )

-- 
-- Calling 'module' (without 'package.seeall' flag) makes all our globals vanish.
-- Take them into aliases first.
--
-- Note: We can _only_ call 'module' from Lua function, not from Lua C API (it seems).
--
local assert=       assert
local type=         type
local tostring=     tostring
local pairs=        pairs
local ipairs=       ipairs
local select=       select
local rawset=       rawset

local math_floor=   math.floor
local table_concat= table.concat

local PRINT=        print   -- for occasional debugging only

local proto=        assert( proto )

module(mod_name)

local G= _ENV
assert(G)

-- Copy 'm' fields (most of them) to the module namespace
--
for k,v in pairs(m) do
    -- "image_surface[_from_png]", "pdf_surface", "svg_surface" are dealt differently
    --
    if not k:match("^[^_]+_surface") then
        rawset( G, k,v )
    end
    --PRINT(k)
end

--
-- Various assert helpers
--
local function assert_number(v)
    assert( type(v)=="number" )
end

local function assert_table_or_nil(v)
    assert( v==nil or type(v)=="table" )
end

local function type_string(v)
    return type(v)=="string"
end


---=== Tools ===---

--
-- [str]= DUMP( v_any [,indent_uint] )
--
-- Debugging stack dump
--
local function DUMP( v, indent )
    indent= indent or 0
    local indent_str= ("  "):rep(indent)
    local arr={}

    if type(v)=="table" then
        arr[1]= "{\n"
        -- First 1..n
        for i,vv in ipairs(v) do
            arr[#arr+1]= indent_str..i..": "..DUMP( vv, indent+1 ).."\n"
        end
        for k,vv in pairs(v) do
            if type(k)~="number" or (math_floor(k)~=k) or (k<=0) then
                arr[#arr+1]= indent_str..tostring(k)..": "..DUMP( vv, indent+1 ).."\n"
            end
        end
        arr[#arr+1]= indent_str.."}"

    elseif type(v)=="number" then
        arr[1]= v
    elseif type(v)=="string" then
        arr[1]= "\""..v.."\""
    else
        arr[1]= tostring(v)
    end
    
    local s= table_concat(arr)
    if indent>0 then
        return s
    else
        LOG(s)  -- topmost level
    end
end


---=== 'newcairo.*' ===---

version= assert( m.VERSION )    -- i.e. "1.8.8"

--
-- cs,cr= surface( w_number, h_number, [{ filename=[str], format=[str], version=[str], background=[color_str] }] )
--
--      '.filename' for command line tool only
--      '.version' for SVG format only: "1.1" or "1.2" (default)
--      '.format':
--              "argb32" (default), "rgb24", "a8", "a1" (image formats)
--              "svg"
--              "pdf"
--      '.background': paint the background with given color
--
--      cr:     The Cairo drawing context (alternatively, you may use 'cs.context').
--
-- Note: 'w' and 'h' are allowed to be numbers (though C API for image surface takes integers
--      only, for obvious reason). This is uniform with PDF/SVG surface usage and -most importantly-
--      allows scaling of bitmap size by calculations.
--
-- cs,w_uint,h_uint= surface( fn_str )      -- METQU only (reads from a PNG file)
--
function surface( ... )
    local args=select('#',...)

    if METQU and args==1 then
        proto( "str", ... )
        local cs= m_image_surface_from_png(...)
        return cs, cs.width, cs.height
    end

    if METQU then
        proto( "num, num, [{ filename=[str], format=[str], version=[str] }]", ... )
    else
        proto( "num, num, [{ format=[str], version=[str] }]", ... )
    end

    local w,h,opt= ...
    w= math_floor(w) -- lua5.3: width and height must converted to integer
    h= math_floor(h)
    local format= opt and opt.format or nil
    local METQU_opt_filename= METQU and (opt and opt.filename) or nil

    local cs,err
    
    if format=="pdf" then
        cs,err= m_pdf_surface(w,h, METQU_opt_filename)
    elseif format=="svg" then
        local version= opt and opt.version or nil
        cs,err= m_svg_surface(w,h,version, METQU_opt_filename)
    else
        cs,err= m_image_surface(w,h,format, METQU_opt_filename)
    end

    if not cs then
        error(err,2)
    end

    --
    -- Initialize the surface to fully transparent background.
    --
    -- The Cairo default is black (0 binary) and black stroke, which essentially
    -- requires the user to do initialization every time.
    --
    local cr= cs.context

    --
    -- Paint the background with given color (by default transparent)
    --
    local background= opt and opt.background
    if background then
        cr.save().set_source_rgb(background).paint().restore()
    end

    return cs, cr
end



--[[
if false then
    -- Paint checkered using a pattern
    
    local c1,c2= 0.6,0.8
    local box_pixels= 20

    local cs2= m_image_surface( box_pixels*2, box_pixels*2 )
    cs2.context        
        .set_source_rgb(c1,c1,c1).paint()   -- paint with one shade
        .rectangle( 0,0, box_pixels, box_pixels )
        .rectangle( box_pixels, box_pixels, box_pixels, box_pixels )
        .set_source_rgb(c2,c2,c2)
        .fill()

    cr.set_source( m_pattern_for_surface(cs2).set_extend("repeat") ).paint()
end
]]
