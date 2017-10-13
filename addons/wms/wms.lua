--
-- WMS.LUA                                  Copyright 2010, Ilmatieteen laitos
--
-- Code for handling WMS queries.
--
-- Ref. <http://www.opengeospatial.org/standards/wms>
--
--[[
map=/var/www/html/mapserver/mapfiles/radar_finland_1km_5min_b.map
SERVICE=WMS
VERSION=1.1.1
REQUEST=GetMap
SRS=epsg:2393
BBOX=3016850,6445445,3783538,7680553
format=image/png
styles=default
WIDTH=760
HEIGHT=1226
layers=tutka
EXCEPTIONS=INIMAGE
time=200908110630
]]


--[[
SERVICE=WMS
VERSION=1.1.0
REQUEST=GetMap
SRS=EPSG:4326&BBOX=-97.105,24.913,78.794,36.358
WIDTH=560
HEIGHT=350
LAYERS=AVHRR-09-27
STYLES=
FORMAT=image/png
BGCOLOR=0xFFFFFF
TRANSPARENT=TRUE
EXCEPTIONS=application/vnd.ogc.se_inimage
]]


--[[
SERVICE=WMS
VERSION=1.1.0
REQUEST=GetMap
SRS=EPSG:4326&BBOX=-97.105,24.913,78.794,36.358
WIDTH=560
HEIGHT=350
LAYERS=BUILTUPA_1M,COASTL_1M,POLBNDL_1M
STYLES=0XFF8080,0X101040,BLACK
FORMAT=image/png
BGCOLOR=0xFFFFFF
TRANSPARENT=TRUE
EXCEPTIONS=application/vnd.ogc.se_inimage
]]

local assert=   assert
local error=    assert(error)
local ipairs=   assert(ipairs)
local pairs=    assert(pairs)
local tostring= assert(tostring)
local setmetatable= assert(setmetatable)
local type=     assert(type)
local proto=    assert(proto)

local table_concat= assert(table.concat)

module "wms"


---=== Helpers ===---

--
-- str= xml( tbl [,indent_str] )
--
-- Convert 'tbl' to XML string. tag name is stored in [0].
--
local function xml( tbl, indent )
    --
    proto( "{ [0]=string, [table|string], ... }, [string]", tbl, indent )

    indent= indent or ""

    local tag= tbl[0]
    local out= { indent.."<"..tag }
    
    for k,v in pairs(tbl) do
        if type(k)~="number" then   -- tag arguments
            out[#out+1]= " "..k.."=\""..tostring(v).."\""
        end
    end
    
    if not tbl[1] then  -- quick close, no contents
        out[#out+1]= " />"
    else
        out[#out+1]= ">"

        -- Fill so that strings wouldn't get extra spaces or newlines around them
        -- (= do not change the data contents because of indentation)
        --
        local last_was_string= false

        for i,v in ipairs(tbl) do   -- goes 1..n
            local t= type(v)
                        
            if t=="string" or t=="number" then
                out[#out+1]= tostring(v)    -- straight after the '>'
                last_was_string= true
            elseif t=="table" then
                if last_was_string then
                    out[#out+1]= xml(v,"")    -- RECURSION
                else
                    out[#out+1]= "\n"..xml(v,indent.."\t")
                end
                last_was_string= false
            else
                error( "Unexpected type: ".. type(v) )
            end
        end
        if not last_was_string then
            out[#out+1]= "\n"..indent
        end
        out[#out+1]= "</"..tag..">"
    end
    
    return table_concat(out)
end

--
-- [{ [string] [, ...] }]= split( [str] )
--
-- Return the string 'str' split at commas.
--
local function split(str)
    if not str then return end

    local t= {}
    for w in str:gmatch("[^,]+") do
        t[#t+1]= w
    end
    return t
end


---=== GetCapabilities ===---

--
-- output_obj= GetCapabilities( {
--                  ...
--      } )
--
-- 'format':    mime type
--
-- 'update_sequence':   
--
local function GetCapabilities( opt )

    local format= opt.format or "text/xml"

    if format~="text/xml" then
        error( "Cannot provide output in: "..format )
    end

    -- 
    -- The XML schema to be returned is defined in "OpenGIS Web Map Service Implementation Specification"
    -- (see Ref.)
    --
--[[
    local t= {
        { [0]="element", name="WMS_Capabilities",
                        version="1.3.0",
                        updateSequence="",
            { [0]="wms:service" },
            { [0]="wms:capabilities" },
        }
    }
]]
    local t=
        { [0]="Service",
            { [0]="Name", "WMS" },      -- always like this
            { [0]="Title", "..human readable description of this WMS service.." },  -- TBD
            { [0]="OnlineResource", "" },   -- TBD
            { [0]="Abstract", "..more blah blah of the project.." },    -- TBD
            { [0]="KeyworkList" },   -- TBD
            { [0]="ContactInformation" },   -- TBD
            { [0]="Fees", "none" },     -- "none" is a reserved word
            { [0]="AccessConstraints", "none" },     -- "none" is a reserved word
            { [0]="Capability" },   -- TBD
            { [0]="LayerLimit" },   -- TBD
            { [0]="MaxWidth" },     -- TBD
            { [0]="MaxHeight" },    -- TBD
        }

-- [mime_str] [,filename_str]= getmetatable(obj).output( obj, stream_ud [,text_decimals_int] )
-- stream_ud( str )     -- to stream output

    local mt= {
        output= function( obj, stream )
            stream( [[
<?xml version="1.0" encoding="UTF-8"?>
<schema targetNamespace="http://www.opengis.net/wms"
        xmlns="http://www.w3.org/2001/XMLSchema" 
        xmlns:wms="http://www.opengis.net/wms" 
        xmlns:xlink="http://www.w3.org/1999/xlink" 
        elementFormDefault="qualified">

<import namespace="http://www.w3.org/1999/xlink" 
        schemaLocation="http://schemas.opengis.net/gml/2.1.2/xlinks.xsd"/>
]]..xml(t).."\n" )
            return format
        end
    }
    return setmetatable( {}, mt )
end


---=== GetMap ===---
--
-- Ref: "OpenGIS Web Map Service WMS Implementation Specification" (pdf) section 7.3 (GetMap)
--

--
-- output_obj= GetMap_1_3_0( {
--                  -- mandatory fields
--                  layers= string      -- "comma separated lis tof one or more map layers"
--                  styles= string      -- "comma separated list of one rendering style per requested layer" 
--                  crs= "namespace:identifier"    -- coordinate reference system
--                  bbox= "minx,miny,maxx,maxy"    -- bounding box in CRS units
--                  width= num          -- output width (pixels)
--                  height= num         -- output height (pixels)
--                  format= mime_str    -- output format (i.e. "image/png")
--
--                  -- optional fields
--                  transparent= "TRUE"|"FALSE"     -- background transparency (default: false)
--                  bgcolor= "0xrrggbb"             -- color of background (if not transparent) (default: white)
--                  exceptions= string              -- format of errors (default: "XML")
--                  time= time                      -- validtime
--                  elevation= elevation            -- elevation (shall we use this?)
--
--                  -- custom fields
--                  ...
--      } )
--
local function GetMap_1_3_0( opt )
        
    local ls_map= {}    -- [layer_str]= style_str

    -- Layers: "comma separated list of one or more map layers"
    -- Styles: "comma separated list of one rendering style per requested layer."
    --
    local layers= split( opt.layers )
    local styles= split( opt.styles )

    if (not layers) or (not layers[1]) then
        error( "No 'Layers' requested" )
    elseif (not styles) or (#styles ~= #layers) then
        error( "No 'Styles' or wrong number (must have one style for each layer): "..tostring(opt.styles) )
    end

    -- Merge layers and styles into one map (layer as key, style as value)
    --
    local ls_map= {}
    for i,v in ipairs(layers) do
        ls_map[v]= styles[i]
    end

    -- Crs: "namespace:identifier"
    --
    local crs= opt.crs
    if not crs then
        error( "No 'CRS' (coordinate reference system) provided." )
    end

    -- Bbox: four values in CRS units
    --
    -- By the 1.3.0 WMS standard, getting all the four values is mandatory.
    --
    local bb= split(opt.bbox)
    if (not bb) or (#bb ~= 4) then
        error( "No 'Bbox' or not four values: "..tostring(opt.bbox) )
    end

    -- Width:
    -- Height:
    --
    local width= tonumber( opt.width )
    local height= tonumber( opt.height )

    if (not width) or (not height) then
        error( "No 'width' or 'height' given, or not numeric: "..tostring(width).." "..tostring(height) )
    end

    -- Format:
    --
    local format= opt.format
    if not format then
        error( "No 'format' provided" )
    end

    -- Transparent: "TRUE"|"FALSE" (default)
    --
    local transparent= false
    if (opt.transparent:lower()=="true")
...

    -- 
    -- The XML schema to be returned is defined in "OpenGIS Web Map Service Implementation Specification"
    -- (see Ref.)
    --
--[[
    local t= {
        { [0]="element", name="WMS_Capabilities",
                        version="1.3.0",
                        updateSequence="",
            { [0]="wms:service" },
            { [0]="wms:capabilities" },
        }
    }
]]
    local t=
        { [0]="Service",
            { [0]="Name", "WMS" },      -- always like this
            { [0]="Title", "..human readable description of this WMS service.." },  -- TBD
            { [0]="OnlineResource", "" },   -- TBD
            { [0]="Abstract", "..more blah blah of the project.." },    -- TBD
            { [0]="KeyworkList" },   -- TBD
            { [0]="ContactInformation" },   -- TBD
            { [0]="Fees", "none" },     -- "none" is a reserved word
            { [0]="AccessConstraints", "none" },     -- "none" is a reserved word
            { [0]="Capability" },   -- TBD
            { [0]="LayerLimit" },   -- TBD
            { [0]="MaxWidth" },     -- TBD
            { [0]="MaxHeight" },    -- TBD
        }

-- [mime_str] [,filename_str]= getmetatable(obj).output( obj, stream_ud [,text_decimals_int] )
-- stream_ud( str )     -- to stream output

    local mt= {
        output= function( obj, stream )
            stream( [[
<?xml version="1.0" encoding="UTF-8"?>
<schema targetNamespace="http://www.opengis.net/wms"
        xmlns="http://www.w3.org/2001/XMLSchema" 
        xmlns:wms="http://www.opengis.net/wms" 
        xmlns:xlink="http://www.w3.org/1999/xlink" 
        elementFormDefault="qualified">

<import namespace="http://www.w3.org/1999/xlink" 
        schemaLocation="http://schemas.opengis.net/gml/2.1.2/xlinks.xsd"/>
]]..xml(t).."\n" )
            return format
        end
    }
    return setmetatable( {}, mt )
end


---=== OGC/WMS common ground ===---

--
-- ...= query( { 
--          service= "WMS",
--          version= "1.1.1"|"1.3.0"|...,   -- used for version negotiation
--          request= "GetCapabilities" | "GetMap" | "GetFeatureInfo",
--          format= "text/xml" | "image/png"     -- requested output MIME format
--          exceptions= "INIMAGE"           -- format for error messages (C++ side should handle this
--                                          -- and catch our errors)
--          ...
--        } )
--
-- Notes from the standard:
--  "Parameter names shall not be case sensitive, but parameter values shall be case sensitive."
--  (we've taken care of that in the C++ side)
--
function query( opt ) 
    
    assert( opt.service=="WMS", "Expected 'service=WMS'" )

    -- 'GetCapabilities' has version optional (asking our favourite instead)
    -- 'GetMap' must have a version given
    --
    --[[
    local a,b,c= (opt.version or ""):match( "%d+.%d+.%d+$" )
    local ver= a and ((a*100)+b)*100+c    -- i.e. "1.3.0" -> 13000
    ]]

    if opt.request=="GetCapabilities" then      -- mandatory for WMS
        return GetCapabilities( opt )

    elseif opt.request=="GetMap" then           -- mandatory for WMS
        -- Version check
        --
        local ver= opt.version
        if not ver then
            error( "'GetMap' request needs 'version=x.y.z' (i.e. \"1.3.0\")" )
        elseif ver ~= "1.3.0" then
            error( "Only 'GetMap' version 1.3.0 implemented (requested '"..ver.."')" )
        end
        
        return GetMap_1_3_0( opt )

    elseif opt.request=="GetFeatureInfo" then   -- optional
        return GetFeatureInfo(opt)
    else
        error( "Invalid WMS request: "..tostring(opt.request) )
    end
end

