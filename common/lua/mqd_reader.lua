--
-- MQD_READER.LUA                               Copyright 2009-2010, Ilmatieteen laitos
--
-- Code for parsing the header portion of MQD files (helps 'MQD_Data.cpp').
--

-- Format is like Lua table / globals syntax, but strings don't have hyphens.
--
-- Data contents is similar to SQD but no producer code is stored (store it in filename
-- if required).
--
-- MQD binary data layout is way more efficient than SQD layout (which has time running
-- in innermost loop, and is not 16-byte aligned for SSE). This is one of the reasons for
-- using MQD (another is becoming detached from Newbase library, completely).
--
-- Revised: 21-Oct-2010 AKa
--

--[[ 
--MQD 1.0
-- ..comment lines..
--
origintime= yyyymmddhhmmss
times= { yyyymmddhhmmss [, ...] }
levels= { str [, ...] }                 -- ground | NNN | hPa:XXX
params= { name_str [unit_str][, ...] }  -- name | "name with spaces" | "name with ,"
projection= str                         -- Newbase and/or Proj4 projection strings
]]

-- Times are stored as UTC.
-- The whole header is stored with UTF8 encoding (important for parameter names and units).
--
-- Unit strings are expected not to have spaces in them. Further metaparameters can be added
-- similarily after these (by making providing of unit compulsory). 
--
-- In the beginning of the binary area, there's a block of metadata for each "tile". These
-- are in order, but the actual data (pointed to from these) may be out of order (allowing
-- packing aligned blocks first and non-aligned last).
--
--      Data storage type:
--          0: float           (nan=missing)
--          1: boolean 0..1    (2=missing)
--          2: half byte 0..14 (15=missing)
--          3: byte 0..254     (255=missing)
--          4: uint16 0..65534 (65535=missing)
--      Interpolation method:
--          0: no interpolation (nearest point)
--          1: linear
--          2: linear deg
--      Gridsize (0x0 for empty tile)
--      Compressed block size (0: no compression)
--      Offset of actual data, from binary block start (0 for empty tile)
--
-- Use of several data types allows us to get concise files without tying parameters artifically
-- together (into bitmaps). This is a storage optimization only; in the 'NA_Data' API all values
-- are represented as float, NAN marking a missing value.
--

local io_open= assert( io.open )

local string_find= assert( string.find )


---=== Helpers ===---

--
-- num|str= parse_single( str )
--
-- Return a single number/string entry.
--
local function parse_single(s)

    local tmp= tonumber(s)
    if tmp then
        return tmp  -- numeric
    end
    
    -- If 's' has quotes around it, remove them
    --
    return s:match( "^\"(.+)\"$" ) or s
end


---=== Public function ===---

--
-- tbl, pos_uint= mqd_header( fn_str, end_marker_str )
--
-- 'pos_uint' is the file position after header portion (first offset where binary data
--          can start, if properly aligned)
--
local function mqd_header( fn, end_marker )

    local f= io_open( fn, "r" )
    local tbl= {}

     for line in f:lines() do 
        -- Line with the end marker is not processed
        --
        if string_find( line, end_marker ) then
            break
        end

        if line:find( "^%s*%-%-" ) then
            -- skip comment lines
        else
            -- key= num
            -- key= str
            -- key= { a, b, .. }
            --
            local key,value= line:match( "^([%a]+)=%s*(.-)%s*$" )   -- no white space around 'value'

            if not key then
                -- Empty lines are just skipped
                --
                if not line:find( "^%s*$" ) then
                    error( "Bad line: "..line )     -- bad syntax
                end
            else
                -- Is it an array?
                --
                local tmp= value:match( "^{%s*(.-)%s*}$" )
                if tmp then
                    local arr= {}
                    for v in tmp:gmatch( "([^,]+),?%s*") do
                       arr[#arr+1]= parse_single(v)
                    end
                    tbl[key]= arr
                else
                    tbl[key]= parse_single(value)
                end
            end
        end
    end

    local pos,err= f:seek()     -- current position without changing it ("cur",0)
    if not pos then
        error(err)
    end

    f:close()   -- would also happen automatically by GC

    return tbl, pos
end

--
return mqd_header

