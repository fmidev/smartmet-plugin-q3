--
-- JSON.LUA                                     Copyright 2009, Ilmatieteen laitos
--
-- Provides a function for filtering query return values, mainly converting
-- any tables to JSON format strings (but we could do also other such filtering here).
--
-- Reference:
--      <http://www.json.org/>
--

-- 
-- Store Lua functions that may get disabled or renamed, before they do.
--
local table_concat= assert( table.concat )
local table_maxn= assert( table.maxn )
local math_floor= assert( math.floor )

-- lua5.1 ==> 5.3: For some reason global 'type' (table is set by type.lua) is a
-- function (global function not replaced by the table) and thus type.xxx is not
-- available. Using 'typetable' global/reference set by type.lua
--
-- TODO: Should fix the root of the problem though
--
local type= typetable

--
-- str= json_string( str|num )
--
-- Return a JSON string representation of a Lua string or number,
-- with proper escapes and hyphenation.
--
local function json_string(s)

    if not tonumber(s) then -- numbers or numerical string need no escapes
        -- Escapes:
        --      \ -> \\
        --      " -> \"
        --      / -> \/
        --      \b -> \\b
        --      \f -> \\f
        --      \n -> \\n
        --      \r -> \\r
        --      \t -> \\t
        --      unicode char -> \uXXXX      (not implemented)
        --
        local lookup= {
            ["\b"]= "\\b",
            ["\f"]= "\\f",
            ["\n"]= "\\n",
            ["\r"]= "\\r",
            ["\t"]= "\\t",
        }

        s= s:gsub( "[\\\"/\b\f\n\r\t]",
            function(c)     -- given each match at a time -> return the replacement
                return lookup[c] or "\\"..c
            end
        )
    end

    return "\""..s.."\""
end


--
-- [str]= json_value( v [,recursion_lookup_tbl] )
--
-- Returns a string presenting the Lua value 'v', in JSON syntax,
--      or nothing if 'v' is not presentable in JSON (just skip the entry)
--
-- Note: Arrays and objects are checked against recursion (i.e. _G._G._G..).
--      In a case of recursion, the recursive branch is given value 'null'
--      (it will still be mentioned in the output).
--
local function json_value( v, recursion_lookup )
    local tv= type(v)

    -- JSON values:
    --  string
    --  number
    --  object
    --  array
    --  true
    --  false
    --  null
    --
    if tv=="string" then
        return json_string(v)

    elseif tv=="number" then
        -- 05-Jan-2012 PKi: Output nan as "null"
        return isnan(v) and "null" or tostring(v)  -- number

    elseif tv=="boolean" then
        return tostring(v)  -- "true" | "false"

    elseif v==nil then
        return "null"   -- can exist in arrays with holes

    elseif tv=="table" then
        if not recursion_lookup then
            recursion_lookup= {}   -- first level
        elseif recursion_lookup[v] then
            return "null"   -- placeholder for the recursive instance
        end
        recursion_lookup[v]= true

        -- Lua table can be either a JSON array, or object (map)
        --
        -- Note: We COULD make tables with holes use the array notation, but this turned
        --      out to be a BAD idea. Returning tables with i.e. timestamps as table keys
        --      would create around 1.2 BILLION 'nil' entries on the way. Too much.
        --      We could use array notation for relatively small sparse tables, though.
        --      -- AKa 18-Mar-10
        --
        local is_object= false
        local last_n= 0

        for k,_ in pairs(v) do
            if type(k)=="number" and math_floor(k)==k and k>0 then    -- array key
                if k>last_n then
                    last_n= k
                end
            else
                is_object= true
                break   -- we won't need 'last_n'
            end
        end

        -- Check if we should output a sparse array in object format (or as an array with 'null's)
        --
        if not is_object then
            -- Note: DON'T trust of '#v' giving the last continuous array index. It may, or may not
            --      (it is defined that way). Use 'table.maxn()'.
            --
            if table_maxn(v)<last_n and last_n>1000 then     -- i.e. having time stamps as table indices
                is_object= true
            end
        end

        -- A collection of strings that are concatenated together (this is the preferred way
        -- to do such in Lua)
        --
        local ss= {}

        if is_object then
            -- JSON object: { string:value [, string:value [, ...]] }
            --
            for k,vv in pairs(v) do
                -- Anything convertible to a string will do as a key. Numbers are converted; if there are
                -- two keys both represented with the same string, behaviour is undefined (one of them will
                -- prevail).
                --
                ss[#ss+1]= json_string( tostring(k) )..":"..json_value(vv, recursion_lookup)
            end
        else
            -- JSON array: \[ value [, value [, ...]] \]    (\[ means the bracket is really there)
            --
            for i=1,last_n do
                ss[#ss+1]= json_value(v[i], recursion_lookup)    -- some values may be nil
            end
        end

        local pre= is_object and "{ " or "[ "
        local post= is_object and " }" or " ]"

        return pre..table_concat( ss, ", " )..post

    else
        -- Userdata, functions, coroutines (type "thread")
        --
        -- Note: JSON has no concept of such objects. To be strictly JSON compliant we must
        --      add hyphens around the returned values (making them into strings).
        --
        
        -- If the '__tostring()' metamethod has been defined for this class, use it for the  
        -- output (i.e. times will become meaningfully formatted this way).
        --
        -- Note: A class may have '__tostring()' defined but the function might not return
        --      anything (i.e. an undefined 'JDay' date)
        --
        local mt= getmetatable(v)
        if mt and mt.__tostring then
            local s= tostring(v)
            if s then
                return json_string(s)
            end
        end

        -- Removing "userdata:" from class names s.a. "userdata:Raw"
        --
        -- CURRENTLY NO HYPHENS ARE ADDED - the output is NOT JSON if functions, userdata
        -- or coroutine handles are in it (but this is more clear to read in debugging).
        --
        return tv:gsub("^userdata:","",1)
    end
end

return json_value
