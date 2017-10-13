--
-- CONFIG.LUA
--
-- This code gets the configuration for a Q3 engine, and returns a number
-- of 'TrackerBase' objects keeping track of the configured data.
--
-- Parameters:
--  1: configuration string, or "@filename"
--
proto( "string", ... )

---=== Helpers ===---

--
-- [str]= DUMP( v_any [,indent_uint] )
--
-- Debugging interface to get table constructs on the LOG.
--
local function DUMP( v, indent )
    indent= indent or 0
    local indent_str= string.rep("  ",indent)
    local arr={}

    if type(v)=="table" then
        arr[1]= "{\n"
        -- First 1..n
        for i,vv in ipairs(v) do
            arr[#arr+1]= indent_str..i..": "..DUMP( vv, indent+1 ).."\n"
        end
        for k,vv in pairs(v) do
            if type(k)~="number" or (math.floor(k)~=k) or (k<=0) then
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
    
    local s= table.concat(arr)
    if indent>0 then
        return s
    else
        LOG(s)  -- topmost level
    end
end


--
-- The format resembles Lua but is not. This is intentional.
-- The configuration is parsed linewise, and anything after a hash is taken
-- to be a comment (no hashes even within strings are allowed, currently).
--
--[==[
# comments
#
[refresh=[Xh][Ymin][Zsec]
[wiping=[Xh][Ymin][Zsec]]
[archwiping=[Xh][Ymin][Zsec]]
[metawiping=[Xh][Ymin][Zsec]]
[rootdir=str]
#[cache=str]
[runs=[Xh][Ymin][Zsec]
[healthcheck([Xh][Ymin][Zsec])=str]
[...]

track {
    [runs=[Xh][Ymin][Zsec]]
    [refresh=[Xh][Ymin][Zsec]]
    [rootdir=str]
    { fn_filter_str [refresh=[Xh][Ymin][Zsec]] [wiping=[Xh][Ymin][Zsec]] [metawiping=[Xh][Ymin][Zsec]] }
      ...
}

-->

{
[track]= {
    runs=secs_uint, 
    { fn_filter_abs, refresh=secs_uint, wiping=secs_uint, metawiping=secs_uint }
    ...
    },
...
},
cache_str,
{ x.x= str [, ... ] }
]==]
local conf_s= ...

-- Read configuration from file if '@...'
--
local fn= conf_s:match("^@(.+)$")
if fn then
    local f= io.open(fn,"r")
    if not f then
        error( "Unable to read configuration: "..fn )
    end
    conf_s= f:read'*a'  -- read the whole file
end

--
-- iterator_f= lines(str)      -- set up iteration
-- [str, line_uint]= iterator_f()
--
-- Iteration for reading the conf linewise, removing comments and skipping
-- empty lines.
--
local function lines(str)
    -- Note: 'lines_raw' and 'lines_n' are part of the returned closure; they are
    --       different for each iterator (NOT shared like C 'static' vars are)
    --
    local iter= string.gmatch(str, "[^\n]+")
    local line_n= 0

    return function() 
        while true do
            local s= iter()    -- next raw line (or 'nil' for end)
            if not s then
                return  -- end iteration
            end
            line_n= line_n+1
            
            s= s:gsub( "#.*$", "" )     -- remove line-end comment (if any)
            s= s:match( "^%s*(.-)%s*$" )    -- trim out space at front and back

            if s~="" then
                return s, line_n
            end
            -- otherwise next line
        end
    end
end

--
-- [num_secs]= h_min_sec( str )
--
-- Read a time value and return it in seconds (or 'nil' for not a valid format).
-- 
-- str: "[Xh][Ymin][Zsec]"
-- 
local function h_min_sec( v, secs )
    secs= secs or 0

    -- Note: Lua regex'es are not as powerful as i.e. Perl - we cannot get this
    --      done on one cycle (specifically: '?' cannot be applied to groups).
    --
    local num,unit,tail= v:match("^(%d+)(%D+)(.*)$")     -- i.e. "1h", "5min" or "20sec"
    if num then
        num= tonumber(num)
        local secs
        if unit=="h" then
            secs= 3600*num
        elseif unit=="min" then
            secs= 60*num
        elseif unit=="sec" then
            secs= num
        else
            return  --nil (not according to the format)
        end

        if tail=="" then
            return secs  -- end of recursion
        else
            local secs_tail= h_min_sec( tail )
            if secs_tail then   -- did tail have proper syntax
                return secs+secs_tail
            end
        end
    end
    -- return nil (no match)
end

-- Some self-tests (can be disabled)
do
    assert( h_min_sec("5min") == 5*60 )
    assert( h_min_sec("20sec") == 20 )
    assert( h_min_sec("1h20sec") == 3600+20 )
    assert( h_min_sec("-20sec") == nil )
end


--
local params= { [""]={} }
    --
    -- [""]: { [refresh=,] ... } default params
    -- [name_str]: { [refresh=,] ... } params for certain track

local scope  -- last track name (s.a. 'HIR' in 'HIR={'; nil for none)

-- 02-Dec-2011 PKi: Labelizer configuration defaults
-- 20-Dec-2011 PKi: TBD: Fields and defaults should be taken from Q3Engine::labelCfgFlds[] ...
--
local LabelizerCfg= {
    limitalongline= 500.0, limitdirect= 40.0,
    allowoverlappinglabels= false, allowoverlappinglines= false,
    dampencornerslimitdeg= 8.0, dampencorners= 0.8,
    boosthorizontallimitdeg= 3.0, boosthorizontal= 1.5,
    curveoptimizationfactor= 10,
    decimals= 1
}

local AddonCfg= {}

--
-- Loop the file, line by line (each entry is supposed to be on a single line)
--
for s,line_n in lines(conf_s) do
    local k2     -- argument within paranthesis of the key

    local k,v= s:match( "^([%w_%.]+)%s*=%s*(.*)$" )    -- i.e. "run = 6h"
    if not k then
        k,k2,v= s:match( "^([%w_]+)%((.-)%)%s*=%s*(.*)$" )  -- i.e. "healthcheck(20min)=..."
        if not k then
            k= s:match( "^([%w_]+)%s*{$" )  -- i.e. "HIR {"
        end
    end

    if v then
        if k2 then
            -- "healthcheck()=..." only for the global scope
            --
            if scope then
                error( "Line "..line_n..": must be within global scope: "..s, 2 )
            end
            params[""][k]= params[""][k] or {}
            local t= params[""][k]
            if type(t)~="table" then
                error( "Line "..line_n..": '"..k.."' used both with and without parameter.", 2 )
            end

            local k2_secs= h_min_sec(k2)
            if not k2_secs then
                error( "Line "..line_n..": bad time value '"..k2.."' (expected '[Xh][Ymin][Zsec]')", 2 )
            end

            -- If there's two lines with same seconds, merge them
            --
            t[k2_secs]= t[k2_secs] and (t[k2_secs].." "..v) or v

        -- 02-Dec-2011 PKi: Labelizer configuration
        --
        elseif (scope == "LabelizerCfg") then
            local lk=string.lower(k)

            if (type(LabelizerCfg[lk]) == type(0)) then
                v=tonumber(v)
            elseif (type(LabelizerCfg[lk]) == type(true)) then
                local tf= {} tf["true"]= true tf["false"]= false tf["err"]= "err"
                v=tf[v:lower():match("^%s*true%s*$") or v:lower():match("^%s*false%s*$") or "err"]
            end

            if (type(LabelizerCfg[lk]) == type(v)) then
                LabelizerCfg[lk]= v
            else
                error( "Line "..line_n..": Invalid key ("..k..") or value within scope '"..scope.."'", 2 )
            end

        else
            -- Plain "key=val"
            --
            -- Convert time strings to seconds, all others passed as-is. 
            -- Numberical strings are converted to numbers.
            --
            -- Addon configuration settings are named <addon>.<setting>, e.g. fminames.dbuser.
            -- They are allowed in global scope only
            --
            local addonsetting= k:match("^%w+%.%w+") and true or false;

            if scope and (k=="cache" or addonsetting) then
                error( "Line "..line_n..": must be within global scope: "..s, 2 )
            end

            if (addonsetting) then
                AddonCfg[k]= h_min_sec(v) or tonumber(v) or v
            else
                params[scope or ""][k]= h_min_sec(v) or tonumber(v) or v
            end
        end

    elseif k then   -- k {
        if scope then
            error( "Line "..line_n..": Cannot start scope '"..k.."' within scope '"..scope.."'", 2 )
        end

        scope= k
            
        -- Allow appending to same scope later on in the config file (two 'HIR={' blocks)
        --
        if not params[scope] then
            params[scope]= setmetatable( {}, { __index= params[""] } )    -- look here for non-existing keys
        end

    elseif s=="}" then
        if not scope then
            error( "Line "..line_n..": Cannot stop a scope (there's none open)", 2 )
        end
        scope= nil
    else
        -- { filemask [refresh=Xh[Ymin]] }
        --
        -- Note: We CAN deprecate the filemask specific properties feature (the 'refresh=' and other such)
        --      completely. In the documentation, only properties to the track level are described, or
        --      used in the production config.  --AKa 17-Mar-10
        --
        local mask,tail= s:match( "^{%s*(.-)%s+(.*)}$" )
        if not mask then
            error( "Line "..line_n..": Bad configuration line '"..s.."'", 2 )
        end
        
        if not scope then
            error( "Line "..line_n..": entries need a scope (i.e. 'HIR {')", 2 )
        end
        assert( params[scope] )

        local rootdir= params[""].rootdir

        -- Apply 'rootdir' if given and the mask is not starting with '/' or '~/'
        --
        if rootdir and (not mask:match("^\~?/")) then
            mask= (rootdir.."/"..mask):gsub("//+","/")     -- multiple slashes to one
        end
        
        -- Missing fields from scope (and then from globals)
        --
        local entry= setmetatable( { mask }, { __index= params[scope] } )
        
        -- Go through each 'xxx=yyy' pair in the tail
        --
        for k,v in string.gmatch(tail,"(%w+)=(%w+)") do
            entry[k]= h_min_sec(v) or tonumber(v) or v
        end

        table.insert( params[scope], entry )
    end
end

-- Salvage some default params but take '[""]' away from the returned table
-- (entries still point to it for defaults via '__index' metamethod)
--
-- Also return global settings to be used as defaults when loading include files
--
local cache= params[""].cache
local healthcheck= params[""].healthcheck or {}
AddonCfg["include.runs"]= params[""].runs
AddonCfg["include.refresh"]= params[""].refresh
AddonCfg["include.rootdir"]= params[""].rootdir
AddonCfg["include.wiping"]= params[""].wiping
AddonCfg["include.metawiping"]= params[""].metawiping
AddonCfg["include.archwiping"]= params[""].archwiping
params[""]= nil

-- TEMPORARILY DISABLED. If we don't have archive extraction to disk, we don't need this.
--  --AKa 7-May-10
--
-- assert( cache, "'cache' config must be given" )

-- If cache name has '$(xxx)' in it, replace with environment variable value
--
if cache then
    cache= cache:gsub( "%$%((.-)%)", 
        function(s) 
            return os.getenv(s) or ""     -- if nothing, eats up the mention (like Make)
        end )
else
	cache= "/var/smartmet/archivecache/.q3_cache"
end

-- 02-Dec-2011 PKi: Returning Labelizer configuration too
--
return params, cache, healthcheck, LabelizerCfg, AddonCfg
