--
-- TRACK_PROXY.LUA
--
-- Help function for 'Track.cpp'. This is needed by (and only by) the C side
-- and is therefore initialized earlier than 'prepare.lua'.
--
-- Note: 'assert.xxx', 'proto' etc. are NOT available when loading this code, 
--      but they will be available when executing the function.
--

-- Lookup for track keys that have special handling
--
local key_lookup= { ["origintimes"]=true, ["archorigintimes"]=true, ["allorigintimes"]=true, ["runs"]=true }

--
-- matrix|matrix2d = trackproxy_index( track_ud, str|jday_ud|int )
--
-- Convert "dot notation" to "call notation", using globals for validtime, 
-- origintime, projection and gridsize.
--
local function trackproxy_index( track, key )
    proto( "TrackProxy, string|jday|int", track, key )
    
    -----
    -- HIR.origintimes  ->  { jday [, ...] }
    -- HIR.runs         ->  run_interval_secs (0: unregular runs)
    --
    if key_lookup[key] then
        return track(key)
    end

    -----
    -- HIR[ jday_ud|time_str|int ]
    --   -> 
    -- HIR{ origintime=<what's within brackets>, 
    --      times= _G["validtime"],
    --      ground=true,
    --    }{ 
    --      time=_G["validtime"],
    --      projection=_G["projection"], 
    --      gridsize=_G["gridsize"],
    --      ground= true,
    --    }
    --
    if tonumber(key) or type.jday(key) then   -- number or numeric string or JDay userdata
        local r,err= track{ origintime= key,
                            times= validtime,
                            ground= true,
                          }
        if not r then
            error(err,2)
        end

        -- Note: We explicitly use the 'projection', 'validtime' and 'gridsize' globals
        --      here, to be correct in both ways of the 'GLOBAL_DEFAULTS_IN_GRID'
        --      configuration option.
        --
        local g,err= r{ projection= projection,
                        gridsize= gridsize,
                        time= validtime,
                        ground= true,
                      }
        assert(g,err)   -- We've checked time and level would be there above (or if 'validtime'==nil
                        -- there's always a default time. Grid should always be found.
        return g        
    end
    
    if type.string(key) then

        -----
        -- HIR["hPa:NNN"|"flight:NNN"|"hybrid:NNN"|"ground"]    DISCONTINUED NOTION!
        --
        local lt= key:match( "^(%w+):[%d%.]+$" )
        if (key=="ground") or (lt=="hPa") or (lt=="hpa") or (lt=="flight") or (lt=="hybrid") then
            error "Use of HIR['hpa:NNN'] and similar discontinued. Use HIR{hpa=NNN} instead."
        end

        -----
        -- HIR.param
        -- HIR[param_str]
        --   -> 
        -- HIR{ origintime=_G["origintime"] or true,  -- default: last run that has requested level
        --      ground=true,
        --      params= {param_str},
        --    }{ 
        --      time=_G["validtime"],
        --      projection=_G["projection"], 
        --      gridsize=_G["gridsize"],
        --      level="ground",
        --    }[param_str]
        --
        -- Note: We DO NOT specify validtime or param in finding the matching data;
        --      we just want the LAST data which has ANY ground data.
        --
        local param= key

        local vt= rawget(_G,"validtime")

        -- TBD: Is it meaningful NOT to limit with validtime already in selection of sqd
        --      or to bring also it in as soon as possible?
        --
        local r,err= track{ origintime= rawget( _G, "origintime" ) or true,  -- default: last ground data
                            -- times NOT given, by design
                            ground= true,
                            params= {param},
                          }
        if not r then error(err,2) end
    
        local g,err= r{ time= vt,
                        projection= rawget(_G,"projection"),
                        gridsize= rawget(_G,"gridsize"),
                        ground= true,
                      }
        if not g then error(err,2) end
    
        return g[param]
    end
    
    error( "Bad index for Track: "..type(key), 2 )
end

return trackproxy_index
