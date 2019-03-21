--
-- UTILITIES.LUA                            Copyright 2009-2010, Ilmatieteen laitos
--
-- Add-on global functions. These are not an integral part of Q3 core
-- (within 'q3.lua') but things that are deemed useful for user scripts.
--
-- This file is compiled into the Q3 engine, just like 'q3.lua' is.
-- Keeping it in a separate source file is merely "making a point" of it
-- not being 100% essential (it could also be placed as a 'require'-loadable
-- addon on the server disk).
--

local bind= assert( select(1,...) )     -- C side exports

-- Functions for creating C++ side objects
--
local new_ScalarMatrix= assert( bind.new_ScalarMatrix )
local new_VectorMatrix_xy= assert( bind.new_VectorMatrix_xy )
local _parse_jday= assert( bind._parse_jday )

bind=nil
local table_insert= assert( table.insert )
local math_pow=     assert( math.pow )
local math_exp=     assert( math.exp )

local WeatherNumberParam= ":1382"

---=== Helpers ===---

--
-- [1..n]= array_index( tbl, val )
--
-- Returns the index of 'val' within 'tbl' (indexed parts, [1..#tbl]) or nothing if not there.
--
local function array_index( tbl, val )
    for i,v in ipairs(tbl) do
        if v==val then
            return i
        end
    end
    -- return nil
end

--
-- num= interpolate( a_num, b_num, f_num )
-- vector= interpolate( a_vector, b_vector, f_num )
--
-- Interpolate a value between 'a' and 'b' (including the end points), at 'f' (0.0 .. 1.0).
--
-- Either 'a' and/or 'b' may be NAN.
--
local function interpolate( a,b, f )
    assert( f>=0.0 and f<=1.0 )

    if f==0.0 then
        return a    -- 'b' can be NAN
    elseif f==1.0 then
        return b    -- 'a' can be NAN
    else
        -- This should work for both numbers and vectors. Gives NAN if either 'a' or 'b' is NAN.
        --
        return (1-f)*a + f*b        -- same as 'a + (b-a)*f'
    end
end


---=== Time functions ===---

--
-- { jday_ud [, ...] }= time_range_h( jday_ud|time_str, jday_ud|time_str, step_h_num )
-- { jday_ud [, ...] }= time_range_mins( jday_ud|time_str, jday_ud|time_str, step_mins_num )
--
-- Creates a range of timestamps, usable for 'cross' and other functions.
--
-- First param is the start of the range. Second is the end time. They can be given
-- either as JDay values, or "YYYYMMDDHHMMSS", as usual for times.
--
-- The returned time range has even steps throughout it. This means 'end' will only
-- be included in the returned range if the length of the range is a multiple of 'step'.
--
function time_range_h( t_start, t_end, h_step )
    proto( "jday|time_str,jday|time_str,number", t_start, t_end, h_step )

    local jd_start= _parse_jday(t_start) or error( "Bad time: "..t_start, 2 )
    local jd_end= _parse_jday(t_end) or error( "Bad time: "..t_end, 2 )

    if jd_end < jd_start then
        error( "Negative time range not allowed", 2 )
    end

    local tt= {}
    local jd= jd_start

    while jd<=jd_end do
        tt[#tt+1]= jd
        jd= jd+h_step       -- addition to 'jday_ud' is in hours (for 'NOW[-+]nn' to work)
    end
    return tt
end

function time_range_mins( t_start, t_end, mins_step )
    return time_range_h( t_start, t_end, mins_step/60.0 )
end


---=== Grad, adv, div, lap, rot ===---

--
-- m2_ud= grad( m_ud )
--
-- Calculate gradient of scalar matrix 'm' and return it as a matrix of vectors.
--
-- Returned units: 'unit_of_input/m'
--
-- Coefficients used in the calculations are:
--
--                            v-- calculating non-edge values
--      |--.--.--.       .--.--.--.       .--.--.--|
--      |-3|+4|-1|  ...  |-1| 0|+1|  ...  |+1|-4|+3|
--      |--`--`--`       `--`--`--`       `--`--`--|
--        ^--     calculating edge values      --^
--
-- Edge calculation is used also when the material has holes.
--
do
    local coeff_edge= { -3, 4, -1 }     -- negated at right edge

    local function grad_part( a,b,c,d,e )   -- 'c' is the center value
        if isnan(c) then
            return c    -- keep as NAN
        elseif isnan(b) then
            if isnan(d) or isnan(e) then
                return 0    -- missing values on both sides = our trend is even
            end
            -- left edge
            return coeff_edge[1]*c + coeff_edge[2]*d + coeff_edge[3]*e

        elseif isnan(d) then
            if isnan(a) then
                return 0    -- 'b' and 'c' are surrounded by NANs
            end
            -- right edge
            return -( coeff_edge[1]*c + coeff_edge[2]*b + coeff_edge[3]*a )

        else
            -- usual case (middle of material, no nans)
            return (-b)+d
        end
    end

    local dx,dy= xy(1,0), xy(0,1)

    function grad( m )
        proto( "Matrix", m )

        local xs,ys= m.size.x, m.size.y

        if xs<3 or ys<3 then
            error( "Matrix too small (min 3x3)", 2 )
        end

        local m2_x= new_ScalarMatrix( m.size, nil, m.projection ) -- uninitialized
        local m2_y= new_ScalarMatrix( m.size, nil, m.projection )

        -- Gradient is calculated based on a window of 5 values up/down and left/right
        -- around the current position. If nearest values exist, only a window of 3 values
        -- is actually used (but we always provide the five, to handle holes easily).
        --
        for pos in points(m) do
            m2_x[pos]= grad_part( m[pos-2*dx], m[pos-dx], m[pos], m[pos+dx], m[pos+2*dx] ) / 2
            m2_y[pos]= grad_part( m[pos-2*dy], m[pos-dy], m[pos], m[pos+dy], m[pos+2*dy] ) / 2
        end

        -- Member-wise division of components 
        --
        local GS= GSIZE( m.projection, m.size ) * 1000  -- GSIZE is in km
        return new_VectorMatrix_xy( m2_x/GS.x, m2_y/GS.y )
    end
end


--
-- num= adv( m_ud )
--
-- Calculate change of i.e. temperature caused by wind.
--
-- Note: 'm' must be from a grid that also provides U and V parameters.
--
-- Returned unit: 'unit_of_input/s'
--
function adv( m )
    proto( "Matrix", m )

    local a= grad(m)
    local b= m.grid.UV
    return a.x*b.x + a.y*b.y
end


--
-- m_ud= div( m_ud )
--
-- Divergence
--
function div( m )
    proto( "Matrix", m )
    local m2= grad(m)
    return m2.x + m2.y
end


--
-- m_ud= lap( m_ud )
--
-- "Laplace / nabla" (TBD: better reference about what this is?)
--
do
    local function lap_part( m, pos, xy_now, xy_last, step )
        if xy_now==0 then
            pos= pos+step   -- first two values at the edge are same
        elseif xy_now==xy_last then
            pos= pos-step   -- last two values at the edge are same
        end
        return m[pos-step] -2*m[pos] + m[pos+step]
    end

    local dx,dy= xy(1,0), xy(0,1)

    function lap( m )
        proto( "Matrix", m )

        local size_x,size_y= m.size.x, m.size.y

        if size_x<3 or size_y<3 then
            error( "Matrix too small (needs min 3x3)", 2 )
        end

        local m2_x= new_ScalarMatrix( m.size, nil, m.projection )  -- uninitialized
        local m2_y= new_ScalarMatrix( m.size, nil, m.projection )
        
        for pos in points(m) do
            m2_x[pos]= lap_part( m, pos, pos.x, size_x-1, dx )
            m2_y[pos]= lap_part( m, pos, pos.y, size_y-1, dy )
        end
        
        local tmp= GSIZE( m.projection, m.size )^2
        return new_VectorMatrix_xy( m2_x/tmp, m2_y/tmp )    -- memberwise division
    end
end


--
-- m_ud= rot( m2_ud )
--
-- Rotor operator
--
do
    local function rot_part( m, pos, xy_now, xy_last, step )
        if xy_now==0 then
            return (-3)*m[pos] + 4*m[pos+step] - m[pos+2*step]
        elseif xy_now==xy_last then
            return 3*m[pos] - 4*m[pos+step] + m[pos+2*step]
        else
            return m[pos+step] - m[pos-step]
        end
    end

    local dx,dy= xy(1,0), xy(0,1)

    function rot( m2 )
        proto( "VectorMatrix", m2 )

        local size_x,size_y= m2.size.x, m2.size.y

        if size_x<3 or size_y<3 then
            error( "Matrix too small (needs min 3x3)", 2 )
        end

        -- Generate the x and y components only once (each '.x'
        -- would cause a separate matrix creation within the loop)
        --
        local mx,my= m2.x, m2.y

        local a= new_ScalarMatrix( m2.size, nil, m2.projection )    -- uninitialized
        local b= new_ScalarMatrix( m2.size, nil, m2.projection )

        for pos in points(m2) do
            -- Values and stepping are crossed; one comes from X, 
            -- the other from Y.
            --
            a[pos]= rot_part( my, pos, pos.x, size_x-1, dx )
            b[pos]= rot_part( mx, pos, pos.y, size_y-1, dy )
        end

        local GS= GSIZE( m2.projection, m2.size )
        return a/(GS.x*2) + b/(GS.y*2)
    end
end


--
-- val_m|val_m2, height_m = MAXZ( raw, param_str, [h_start_num], [h_end_num], [gs] )
--
-- 'h_start' is the starting height (in meters); defaults to lowest level
-- 'h_end' is the ending height (in meters); defaults to highest level
--
-- Sample use:
--
--    local r,err= track{ height=true, params={WS[,Z]} }   -- origintime default affects
--    assert( r,err )
--
--    local m_WS, m_h= MAXZ( r, WS, 0, 5000 )     -- maximum wind speed and their heights (0..5000m)
--
-- Uses globals:
--  'validtime', 'projection', 'gridsize'
--
function MAXZ( r, param, h_start, h_end, gs )
    proto( "Raw, string, [number], [number], [MatrixPos]", r, param, h_start, h_end, gs )

    -- Note: 'r.params' provides the standard names, so we can check for existance of 'Z'
    --       (regardless of its native name, i.e. 'Z-korkeus:2').
    --
    if not array_index( r.params, "Z" ) then
        error( "Data does not provide 'Z' parameter (has "..concat(r.params,", ")..")", 2 )
    end

    -- Grid size for the matrix to be returned
    --
    -- Note: Non-SQD format data may have dynamic grid size; each time, level, param combo 
    --       potentially having different grid size (but same projection).
    --
    if not gs then
        gs= _G["gridsize"] or r.sqd_gridsize or error( "Gridsize must be provided" )
    end

	local mx_max= matrix(gs,param) -- maximum values (initially NANs); may be matrix of vectors
	local m_h= matrix(gs,"Z")      -- heights of maximum values (-''-)

    for g in grids_by_level(r, {gridsize=gs}) do   -- validtime, projection defaults affect
        for pos,v in points(g[param]) do
            assert( type.number(v) )

            local h= g.Z[pos]

            if ((not h_start) or (h>=h_start)) and
               ((not h_end) or (h<=h_end)) and
               (isnan(mx_max[pos]) or v>mx_max[pos]) then
                mx_max[pos]= v
                m_h[pos]= h
            end
        end
    end
    return mx_max, m_h
end


--
-- m|vm= ATZ( raw, param_str, height_num, [gridsize] )  
--
-- Interpolates the value of 'param' at certain fixed 'height'.
--
-- 'raw' is expected to have both the required 'param' but also the "Z" (height) parameter.
-- 
-- Note: This function replaces the need for 'height' level calculations, as performed by Newbase.
--
-- Sample use:
--
--    local r,err= track{ height=true, params={WS[,Z]} }   -- origintime default affects
--    if not r then
--        error(err)  -- no match from the given track
--    end
--    local m_WS= ATZ( r, WS, 120 )     -- wind speed at given height
--
-- Uses globals:
--  'validtime', 'projection', 'gridsize'
--
function ATZ( r, param, z_value, gs )
    proto( "Grid, string, number, [MatrixPos]", g, param, z_value, gs )

    -- Note: Don't use 'r.params' to check for Z. Params are listed there in the "native syntax",
    --       i.e. "Z-korkeus:2" for what is a valid Z.
    --
    if not r.has_param("Z") then
        error( "Data does not provide 'Z' parameter (has "..concat(r.params,", ")..")", 2 )
    end

    if not gs then
        gs= _G["gridsize"] or r.sqd_gridsize or error( "Gridsize must be provided" )
    end

    -- Going throught the levels of 'g' (with provided gridsize) we find the steps where param 'Z'
    -- crosses the value 'z_value'. For such, we interpolate an estimate for the actual value of
    -- requested 'param' at 'z_value' height.
    
	local m_param_at_z= matrix(gs,param)      -- so far calculated values (initially NANs); may be matrix of vectors

    local ma_param  -- param (as a matrix) at earlier level
    local ma_z      -- height (as a matrix) at earlier level

    for g in grids_by_level(r, {gridsize=gs}) do   -- validtime, projection defaults affect
    
        local mb_param= g[param]    -- param at current level
        local mb_z= g.Z             -- height at current level

        if ma_param then    -- on 2..nth level (have a lower level to compare with)
            for pos,v in points(g[param]) do
                if isnan( m_param_at_z[pos] ) then
                    -- Still not calculated this point

                    local za= ma_z[pos]
                    local zb= mb_z[pos]
    
                    if isnan(za) then
                        if z_value==zb then
                            m_param_at_z[pos]= mb_param[pos]
                        end
                        
                    elseif isnan(zb) then
                        if z_value==za then
                            m_param_at_z[pos]= ma_param[pos]
                        end
    
                    else
                        assert( za <= zb )       -- levels should have been provided in rising order (from the ground up)
    
                        if (z_value<za) or (z_value>zb) then
                            -- skip to next value
                        else
                            local f= (z-za) / (zb-za)   -- 0.0 .. 1.0 from 'za' to 'zb'
                                
                            m_param_at_z[pos]= interpolate( ma_param[pos], mb_param[pos], f )
                        end
                    end
                end        
            end
        end
        
        ma_param= mb_param
        ma_z= mb_z
    end
end


---=== LONLAT family functions ===---

--
-- m_num= CORIOLISFACTOR( [projection_str] [,gridsize_pos] )
--
-- Returns: The Coriolis frequency 'f', with which an otherwise resting air mass
--      will perform a full circle, due to the Coriolis meta force. 
--
-- Note: In Finnish, the frequency is called "coriolistekijä" (coriolis co-factor).
--
-- Unit: 1/s
--
-- Ref: http://en.wikipedia.org/wiki/Coriolis_effect
--
function CORIOLISFACTOR( proj, gs )
    proto( "[string],[MatrixPos]", proj, gs )

    -- Formula given by Janne Ylläsjärvi (3-Jun-2010)
    --
    return 2*7.292e-5*sin( LONLAT(proj,gs).y * (pi/180) )
end


---=== Flight level conversion ===---

--
-- hpa_num= fl_hpa( uint )
--
-- Convert a flight level value (~ 100*ft) to pressure level (hPa).
--
--[[
    PALT <= 36089ft: hpa = 1013.251013,25*(1 - (10^-6)*6,8756*(PALT))^5,2559 
    PALT > 36089ft: hpa = 226.32exp-((PALT-36089)/20805) 

    PALT on lentopinta jalkoina eli FL150 -> PALT = 15000 [ft]
    tulos on paine [hPA/mbar]
]]
--
function fl_hpa( fl )
    local palt= fl*100

    local hpa
    
    if palt <= 36089 then
        hpa= 1013.251013 * math_pow( (1 - (6.8756e-6 * palt)), 5.2559 )
    else
        hpa= 226.32 * math_exp( -((palt-36089)/20805.0) )
    end

    LOG( "Flight level conversion: "..fl.." -> "..hpa.." hPa" )
    return hpa
end


---=== 03-Jan-2012 PKi: UTF-8 conversion (used for weather symbols) ===---
---
--- Bit operations
---
function lshift(x, by)
    return x * (2 ^ by)
end
function rshift(x, by)
    return math.floor(x / (2 ^ by))
end
---
--- Convert from character code (number 0-255) to UTF-8 string
---
function chr2utf8str(v)
    --
    -- Return nonvalid input as is and code <= 127 as one char string
    --
    if ((not type.number(v)) or (v<0) or (v>255)) then
        return v
    elseif (v<=127) then
        return string.char(v)
    end
    --
    -- Return code 128<= c <=255 (bits yyyyyxxxxxx) as two char (c1,c2) string
    --
    local c1= 0xc0   		-- 1'st byte 110yyyyy
    local c2= 0x80			-- 2'nd byte 10xxxxxx

    local v1= rshift( v, 6 )	-- yyyyy
    local v2= v-lshift( v1, 6 )	-- xxxxxx

    return string.char(c1+v1)..string.char(c2+v2)
end

---=== Track metadata ===---

--
-- Check if property/field is selected for output
--
local function isfieldselected(query,field)
	return query[field] and true or false
end

--
-- Concatenate strings
--
local function concatenate(...)
	local ret= " "

	for i=1,select('#',...) do
       	local str= select(i,...)

		if type(str)=="number" then str= tostring(str) end

		if type(str)=="string" then
			ret= ret..str.." "
		end

	end

	return ret~=" " and ret or ""
end

--
-- Return table of strings matching pattern
--
local function matchpattern(pattern,...)
	local ret= {}

	for i=1,select('#',...) do
       	local str= select(i,...)

		if type(str)=="number" then str= tostring(str) end

		if type(str)=="string" then
			str= string.match(str,pattern)

			if str then
				ret[#ret+1]= str
			end
		end

	end

	return ret
end

--
-- Get querydata for given track and data selection (level type, parameters and origintime) criterias
--
local function getraw(track,targs)
	-- params is empty if querying only calculated parameter(s)
	--
	if targs.params and #targs.params==0 then
		targs.params= nil
	end

	return track(targs)
end

--
-- Return metadata
--
local function trackmetadata(query,args)

	local ret_origintimes= isfieldselected(query,"origintimes")
	local ret_files= isfieldselected(query,"files")
	local ret_loadtimes= isfieldselected(query,"loadtimes")
	local ret_filetimes= isfieldselected(query,"filetimes")
	local ret_idents= isfieldselected(query,"idents")
	local ret_grids= isfieldselected(query,"grids")
	local ret_levels= isfieldselected(query,"levels")
	local ret_timeranges= isfieldselected(query,"timeranges")
	local ret_parameters= isfieldselected(query,"parameters")
	local ret_descriptions= isfieldselected(query,"descriptions")
	local ret_interpolations= isfieldselected(query,"interpolations")
	local ret_precisions= isfieldselected(query,"precisions")
	local ret_projections= isfieldselected(query,"projections")
	local ret_wkts= isfieldselected(query,"wkts")
	local ret_dataids= isfieldselected(query,"dataids")
	local ret_datanames= isfieldselected(query,"datanames")
	local ret_locations= isfieldselected(query,"locations")
	local ret_relativeuvs= isfieldselected(query,"relativeuvs")

	if args==nil then args= {} end

	local tracks= args.names or mt_tracknames
	local leveltypes= args.leveltypes and matchpattern("^%s*(%a*):?",unpack(args.leveltypes)) or {}
	local paramnames= args.parameters and #args.parameters>0 and args.parameters or {}
	local paramids= args.parameters and matchpattern("(:?%d+)$",unpack(args.parameters)) or {}

	local queryleveltypes

	if #leveltypes>0 then
		queryleveltypes= {}

		for lt=1,#leveltypes do
			if type(leveltypes[lt])~="string" then
				error("Invalid level type '"..type(leveltypes[lt]).."', string expected")
			elseif leveltypes[lt]== "Ground" then
				queryleveltypes["ground"]= true
			elseif leveltypes[lt]== "PressureLevel" then
				queryleveltypes["hpa"]= 850
			elseif leveltypes[lt]== "HybridLevel" then
				queryleveltypes["hybrid"]= true
			elseif leveltypes[lt]== "SoundingLevel" then
				queryleveltypes["sounding"]= true
			else
				error("Unknown level type '"..leveltypes[lt].."', 'Ground', 'PressureLevel', 'HybridLevel' or 'SoundingLevel' expected")
			end
		end
	else
		queryleveltypes= { ground= true, hpa= 850, hybrid= true, sounding= true }
	end

	local ret= {}

	for t=1,#tracks do
  		local track=rawget( _G, tracks[t] )

  		if track==nil then error( "Could not get track with name '"..tracks[t].."'" ) end

		local origintimes= args.origintimes or track.origintimes

		if #origintimes>0 then
			local n= 0

			for ot=1,#origintimes do for k,v in pairs(queryleveltypes) do
				local targs= { [k]= v, origintime= origintimes[ot], metaquery= true }
				local status,r= pcall(getraw,track,targs)
				local matchingdata= status and r or false

				if matchingdata and (#paramnames>0 or #paramids>0) then
					--
					-- Match parameter name
					--
					matchingdata= false

					if #paramids>0 then
						--
						-- Match parameter number (n or :n) 
						--
						local paramdescs= concatenate(unpack(r.mt_paramdescriptions))

						for p=1,#paramids do
							if string.match(paramdescs,(string.sub(paramids[p],1,1)==":" and "" or ":")..paramids[p].." ")~=nil then
								matchingdata= true
								break
							end
						end
					end

					if matchingdata==false and #paramnames>0 then
						local params= concatenate(unpack(r.mt_paramidents))

						for p=1,#paramnames do
							if type(paramnames[p])=="string" and string.match(params," "..(paramnames[p].." "))~=nil then
								matchingdata= true
								break
							end
						end
					end
				end

				if matchingdata then
					local gs= r.sqd_gridsize

					if #ret==0 or ret[#ret].name~=tracks[t] then
						ret[#ret+1]= {}
						ret[#ret].name= tracks[t]

						if ret_origintimes then ret[#ret].origintimes= {} end
						if ret_files then ret[#ret].files= {} end
						if ret_loadtimes then ret[#ret].loadtimes= {} end
						if ret_filetimes then ret[#ret].filetimes= {} end
						if ret_idents then ret[#ret].idents= {} end
						if gs and ret_grids then ret[#ret].grids= {} end
						if ret_levels then ret[#ret].levels= {} end
						if ret_timeranges then ret[#ret].timeranges= {} end
						if ret_parameters then ret[#ret].parameters= {} end
						if ret_descriptions then ret[#ret].descriptions= {} end
						if ret_interpolations then ret[#ret].interpolations= {} end
						if ret_precisions then ret[#ret].precisions= {} end
						if gs and ret_projections then ret[#ret].projections= {} end
						if gs and ret_wkts then ret[#ret].wkts= {} end
						if (not gs) and ret_dataids then ret[#ret].dataids= {} end
						if (not gs) and ret_datanames then ret[#ret].datanames= {} end
						if (not gs) and ret_locations then ret[#ret].locations= {} end
						if ret_relativeuvs then ret[#ret].relativeuvs= {} end
					end

					n= n+1

					if ret_origintimes then ret[#ret].origintimes[n]= origintimes[ot] end
					if ret_files then ret[#ret].files[n]= r.source end
					if ret_loadtimes then ret[#ret].loadtimes[n]= r.mt_loadtime end
					if ret_filetimes then ret[#ret].filetimes[n]= r.mt_modificationtime end
					if ret_idents then ret[#ret].idents[n]= r.sqd_producer end

					if gs and ret_grids then
						ret[#ret].grids[n]= {}
						ret[#ret].grids[n].gridsize= gs
						ret[#ret].grids[n].dx= r.mt_dx
						ret[#ret].grids[n].dy= r.mt_dy
					end

					if ret_levels then
						ret[#ret].levels[n]= {}
						ret[#ret].levels[n].leveltype=r.mt_leveltype
						ret[#ret].levels[n].levels= { (string.gsub( (string.gsub( (string.gsub( concat(r.levels,","), "hPa:", "")), "hybrid:", "")), "sounding:", "")) }
					end

					if ret_timeranges then
						local times= r.times

						ret[#ret].timeranges[n]= {}
						ret[#ret].timeranges[n].first= times[1]
						ret[#ret].timeranges[n].last= times[#times]
						ret[#ret].timeranges[n].count = #times
						ret[#ret].timeranges[n].step = r.mt_timestep
					end

					if ret_parameters then ret[#ret].parameters[n]= r.mt_paramidents end
					if ret_descriptions then ret[#ret].descriptions[n]= r.mt_paramdescriptions end
					if ret_interpolations then ret[#ret].interpolations[n]= r.mt_interpolations end
					if ret_precisions then ret[#ret].precisions[n]= r.mt_precisions end

					if gs and ret_projections then ret[#ret].projections[n]= r.projection end
					if gs and ret_wkts then ret[#ret].wkts[n]= r.mt_wkt end

					if (not gs) and ret_dataids then ret[#ret].dataids[n]= r.dataids end
					if (not gs) and ret_datanames then ret[#ret].datanames[n]= r.datanames end
					if (not gs) and ret_locations then ret[#ret].locations[n]= r.locations end

					if ret_relativeuvs then ret[#ret].relativeuvs[n]= r.mt_relative_uv end
				end
			end end
  		end
	end

	return ret
end

--
-- Exclude fields from a query
--
local function exclude(q,...)
	for i=1,select('#',...) do
       	local field= select(i,...)

		if type(field)=="string" and type(q[field])=="boolean" then
			q[field]= nil
		end
	end

	return q
end

--
-- Include fields to a query
--
local function include(queryall,q,...)
	for i=1,select('#',...) do
		local field= select(i,...)

		if type(field)=="string" and type(queryall[field])=="boolean" then
			q[field]= true
		else
			error("Unknown include field '"..field.."'")

		end
	end

	return q
end

--
-- Query returning all except point data specific fields by default
--
local function getquery_all(args)
	local q= {
		--
		-- Note: 'names' field is not bool indicating it cannot be excluded from query
		--
		names= "",
		origintimes= true,
		files= true,
		loadtimes= true,
		filetimes= true,
		idents= true,
		grids= true,
		levels= true,
		timeranges= true,
		parameters= true,
		descriptions= true,
		interpolations= true,
		precisions= true,
		projections= true,
		wkts= true,
		dataids= false,
		datanames= false,
		locations= false,
		relativeuvs= false
	}

	local query= args.includes and include(getquery_all({}),q,unpack(args.includes)) or q

	return args.excludes and exclude(query,unpack(args.excludes)) or query,args
end

--
-- Query returning just track names and origin times by default
--
local function getquery_tracks(args)
	local q= {
		--
		-- Note: 'names' field is not bool indicating it cannot be excluded from query
		--
		names= "",
		origintimes= true
	}

	local query= args.includes and include(getquery_all({}),q,unpack(args.includes)) or q

	return args.excludes and exclude(query,unpack(args.excludes)) or query,args
end

--
-- Query for additional metadata for point data
--
local function getquery_addmeta(includes, soundingdata)
	local q= {
		dataids= false, getdataids= function(data) return data.dataids end, timedataids= false,
		datanames= false, getdatanames= function(data) return data.datanames end, timedatanames= false,
		locations= false, getlocations= function(data) return data.locations end, timelocations= false
	}

	if (soundingdata) then
		q.pressures= false q.getpressures= function(data) return data.P end q.timepressures= true
	end

	local meta= includes and include(q,q,unpack(includes)) or nil
	local func

	for k,v in pairs(meta) do
		if meta[k] then
			if func==nil then func= {} end
			func[k]= { f= q["get"..k], timef= q["time"..k] }
		end
	end

	if (soundingdata and func.pressures and (not func.dataids)) then
		--
		-- 'sdataids' include is used to get the stations (number of them) when 'dataids' is not included by the query
		--
		q.getsdataids= q.getdataids q.timesdataids= false
		func.sdataids= { f= q["getsdataids"], timef= q["timesdataids"] }
	end

	return meta and func or nil
end

--
-- Get 'key=value' setting (e.g. "param = contour descriptor"), or for 'getdata' mode, the data matrix to be used.
-- For 'getdata', if the input string contains lua code, execute it and return it's return value; otherwise try to
-- load the data matrix from the input string having format 'nx;ny;x1y1_val,x2y1_val,...'
--
local function getdatafromstring(arg,tbl,getdata)
	local ret= tbl or {}
	local n= 0

	if getdata and type.Matrix(arg) then
		ret.parameters= { "data" }
		ret.gridsize= arg.size
		ret.data= arg

		return ret
	end

	if type(arg)=="string" then
		if getdata then
			--
			-- Check for data; nx,ny;x1y1_val,x2y1_val,...
			--
     		local nx,ny,pos= tonumber_all(string.match(arg,"^(%d+),(%d+);()"))

			if nx~=nil and nx>0 and ny~=nil and ny>0 and pos~=nil and pos>0 then
				-- An empty field or value 32700 is taken as missing value
				--
				local data= ","..string.gsub(string.sub(arg,pos),",,",",32700,")..","
				repeat data= string.gsub(data,",,",",32700,") until not string.find(data,",,")
				data= string.gsub(data,",",",,")

				ret.parameters= { "data" }
				ret.gridsize= xy(nx,ny)
				ret.data= new_ScalarMatrix( ret.gridsize, nil, "" )

				local x,y= 0,0

				for v in string.gmatch(data,",([-]?%d+[.]?%d*),") do
					if y>=ny then error("Too much data") end

					ret.data[xy(x,y)]= tonumber(v)

					x= x+1
					if x>=nx then
						x=0
						y= y+1
					end
				end

				if y<ny then error("Not enough data") end

				return ret
			else
				--
				-- Execute lua code
				--
				local f,e= loadstring(arg)

				if f then return getdatafromstring(f(),{},true) end

				error("Invalid macro: "..e)
			end
		end

		--
		-- Get key=value pairs (despite the loop, just one expected)
		--
		for k, v in string.gmatch(arg, "([%w%p%d%-_]+)%s*=%s*([%w%p%d%-_%s]+)") do
			if ret[k]==nil then ret[k]= {}
			--
			-- Parameter might already have a contour (e.g. '["4"]="1 0 0 ..."')
			--
			elseif type(ret[k])=="string" then ret[k]= { ret[k] }
			end

			ret[k][#ret[k]+1]= v

			n= n+1
		end
	end

	return ret,n
end

--
-- Check metadata query parameters. Parameters are returned as a table of tables
--
local function checkmqargs(args)
	if args~=nil then
		if type(args)=="table" then
			--
			-- Parameter names, in plural for tables
			--
			-- e.g. trackquery({name="HIR",include="levels"})
			--		trackquery({names={"HIR","EC"},includes={"levels","grids"}})
			--
			local fields= { "name", "origintime", "leveltype", "parameter", "include", "exclude" }

			for field=1,#fields do
				local flds= fields[field].."s"
				local fld= fields[field]

		   		if args[flds]~=nil then
		   			if type(args[flds])~="table" then
						error("Invalid query parameter '"..flds.."', table expected")
					end
				elseif args[fld]~=nil then
					if type(args[fld])=="number" then args[fld]= tostring(args[fld]) end

					if type(args[fld])=="string" then
						args[flds]= { args[fld] }
					else
						error("Invalid query parameter '"..fld.."', string expected")
					end
				end
			end
		else
			error("Invalid query parameter type '"..type(args).."', table expected")
		end

		return args
	end

	return {}
end

--
-- Metadata queries
--
function metadataquery()
	--
	-- Query returning all fields by default
	--
	local function query(args) return trackmetadata(getquery_all(checkmqargs(args))) end

	--
	-- Query returning only track names and origin times by default
	--
	local function trackquery(args) return trackmetadata(getquery_tracks(checkmqargs(args))) end

	return {
		query = query,
		trackquery = trackquery
	}
end

--
-- Return table of data parameter numbers
--
local function getparamtable(args)
	local ret= {}
	local metadata= nil
	local parameters

	if type(args.parameters)=="string" then
		parameters= { args.parameters }
	elseif type(args.parameters)=="table" then
		parameters= args.parameters
	end

	if type(parameters)=="table" then
		for p=1,#parameters do
			--
			-- Extract number from parameters name when available.
			-- Otherwise search enumerated names
			-- 
			local paramid= matchpattern(":?(%d+)$",parameters[p])
			local pid= #paramid>0 and paramid[1] or _G["getparamid"](parameters[p])
			ret[#ret+1]= pid and (":"..pid) or parameters[p]
		end

		return ret
	end

	error("getparamtable(): internal:  'parameters' type is '"..type(args.parameters).."', a string, a number or a table of strings/numbers expected")
end

--
-- Add querydata to dataset
--
local function addqd2ds(dataset,status,r,name,origintime,time)
	local data= status and r or nil
	local index

	if data then
		for d=1,#dataset do
			if dataset[d].data and dataset[d].data.source==data.source then index= d break end
		end
	end

	if index==nil then
		index= #dataset+1

		dataset[index]= {}
		dataset[index].name= name
		dataset[index].data= data
		dataset[index].origintime= origintime
		dataset[index].times= {}
	end

	-- Store the validtime to be taken from this query data
	
	if time then dataset[index].times[#dataset[index].times+1]= time end
end

--
-- Return dataset containing raw objects (or nil's when not available) for given leveltype and each given track and origin/validtime, covering given parameters
--
local function getdataset(args)
	local targs= args.leveltype or {}
	local params= getparamtable(args)
	local ret= { parameters= params }
	local times= args.times or { 0 }
	local status,r

	-- Ignore calculated parameter weathernumber when selecting data
	--
	local queryparams = {}

	for p=0,#params do
		if params[p]~=WeatherNumberParam then
			queryparams[p]= params[p]
		end
	end

	for n=1,#args.names do
		local track= rawget( _G, args.names[n] )

  		if track==nil then
			error( "Could not get track with name '"..args.names[n].."'" )
		end

		if ret.datas==nil then ret.datas= {} end

		if args.origintimes~=nil and #args.origintimes>0 then
			for t=1,#args.origintimes do
				targs.origintime= args.origintimes[t]

				-- At first 'param' round search for querydata containing all requested parameters;
				-- if not available, search parameter by parameter until found.
				--
				-- Note: When data is found, values (well empty for missing parameters) for all requested parameters
				--		 are taken from it. Selecting multiple data's within 'param' loop would result in sort of messed
				-- 		 up output; first there would be block of values for some and missing values for some of the parameters,
				--		 then another (additional) block with other/changed values for some and values for some other parameters
				--		 that were missing in previous block and so on). If such could happen depends on track configuration,
				--		 but we don't let it happen.
				--
				-- 		 And, by picking up just one/first available data, track updates (new querydatas) meanwhile looping
				--		 don't affect us. 

				for p=0,#params do
					targs.params= p==0 and queryparams or (params[p]~=WeatherNumberParam and {params[p]}) or {}

-- print("** GET track "..args.names[n].." ot "..targs.origintime)
					status,r= pcall(getraw,track,targs)

					if (status and r) or p==0 then
						if status and r and p>0 then
							-- Ignore the nil data
							--
-- print("** NIL data "..args.names[n].." ot "..targs.origintime)
							table.remove(ret.datas)
						end

						addqd2ds(ret.datas,status,r,args.names[n],targs.origintime)

						if (status and r) or #params==1 then
							break
						end
					end

					p= p+1
				end
			end
		else
			for t=1,#times do
				if args.times then targs.times= times[t] end

				for p=0,#params do
					targs.params= p==0 and queryparams or (params[p]~=WeatherNumberParam and {params[p]}) or {}

-- Runtime error: attempt to concatenate field 'origintime' (a nil value)
-- print("** GET track "..args.names[n].." ot "..targs.origintime)
					status,r= pcall(getraw,track,targs)

					if (status and r) or p==0 then
						if status and r and p>0 then
-- Runtime error: attempt to concatenate field 'origintime' (a nil value)
-- print("** NIL data "..args.names[n].." ot "..targs.origintime)
							table.remove(ret.datas)
						end

						addqd2ds(ret.datas,status,r,args.names[n],targs.origintime,targs.times)

						if (status and r) or #params==1 then
							break
						end
					end

					p= p+1
				end
			end
		end
	end

	return ret
end

--
-- Return default validtime for querydata
--
local function getdefaulttime(data)
	--
	-- Default validtime is the first time for gridded (e.g. forecasts) and last time for nongrid (e.g. observations) querydata
	--
	if data and #data.times>0 then
		return { data.times[data.sqd_gridsize and 1 or #data.times] }
	end

	return { NOW }
end

--
-- Check if table contains given time
--
local function datatime(times,time)
	-- If origintime was given, times were not stored; using the selected data for all requested validtimes 
	--
	if #times==0 then return true end

	for t=1,#times do
		if times[t]== time then return true end
	end

	return false
end

--
-- Calculate weathernumber
--
-- For some reason calcweathernumber can't successfully call back getgrid; using copy of it
--
local function getgrid2(raw,param)
	return raw and raw[param] or error("No raw")
end
local function calcweathernumber(raw,locations)
        -- Matrix filled with missing value, used for missing parameters
        --
	if _G["gridsize"]==nil or _G["projection"]==nil then
		error("calcweathernumber: gridsize and projection must be set")
	else
		if type(_G["projection"])=="boolean" then
			-- Native projection
			--
			rawset (_G, "projection", raw["raw"]["projection"])
		end
		if type(_G["gridsize"])=="boolean" then
			-- Native gridsize
			--
			rawset (_G, "gridsize", raw["raw"]["sqd_gridsize"])
		end
	end

	local kFloatMissing= 32700.0
	local mmissing= new_ScalarMatrix(_G["gridsize"],kFloatMissing,_G["projection"])

	-- Get input data
	--
	local TotalCloudCover= ":79"
	local Precipitation1h= ":353"
	local PotentialPrecipitationForm= ":1226"
	local PrecipitationForm= ":57"
	local PotentialPrecipitationType= ":1235"
	local PrecipitationType= ":56"
	local ProbabilityThunderstorm= ":260"
	local FogIntensity= ":327"

	local status1,g1= pcall(getgrid2,raw,TotalCloudCover)
	local mcloudcover= status1 and g1 or mmissing
	local status2,g2= pcall(getgrid2,raw,Precipitation1h)
	local mrain= status2 and g2 or mmissing
	local status3,g3= pcall(getgrid2,raw,PotentialPrecipitationForm)
        if not status3 then
		status3,g3= pcall(getgrid2,raw,PrecipitationForm)
	end
	local mrform= status3 and g3 or mmissing
	local status4,g4= pcall(getgrid2,raw,PotentialPrecipitationType)
	if not status4 then
		status4,g4= pcall(getgrid2,raw,PrecipitationType)
	end
	local mrtype= status4 and g4 or mmissing
	local status5,g5= pcall(getgrid2,raw,ProbabilityThunderstorm)
	local mthunder= status5 and g5 or mmissing
	local status6,g6= pcall(getgrid2,raw,FogIntensity)
	local mfog= status6 and g6 or mmissing

	-- Calculate weathernumber

	local gs= #locations.locations==0 and _G["gridsize"] or xy(#locations.locations,1)
	local weathernumber= matrix(gs,nil,_G["projection"])
	local version= 1
	local cloud_class= 0		-- not available yet

	local thunder_limit1= 30
	local thunder_limit2= 60

	local rain_limit1= 0.025
	local rain_limit2= 0.04
	local rain_limit3= 0.4
	local rain_limit4= 1.5
	local rain_limit5= 2
	local rain_limit6= 4
	local rain_limit7= 7

	local cloud_limit1= 7
	local cloud_limit2= 20
	local cloud_limit3= 33
	local cloud_limit4= 46
	local cloud_limit5= 59
	local cloud_limit6= 72
	local cloud_limit7= 85
	local cloud_limit8= 93

	local loc= 1
	for pos,v in points(weathernumber) do
		local mpos= #locations.locations==0 and pos or locations.locations[loc]
		local n= mcloudcover[mpos]
		local n_class= 9	-- missing

		if isnan(n) or n==kFloatMissing then
			n_class= 9
		elseif n<cloud_limit1 then
			n_class= 0
		elseif n<cloud_limit2 then
			n_class= 1
		elseif n<cloud_limit3 then
			n_class= 2
		elseif n<cloud_limit4 then
			n_class= 3
		elseif n<cloud_limit5 then
			n_class= 4
		elseif n<cloud_limit6 then
			n_class= 5
		elseif n<cloud_limit7 then
			n_class= 6
		elseif n<cloud_limit8 then
			n_class= 7
		else
			n_class= 8
		end

		local rain= mrain[mpos]
		local rain_class= 9	-- missing

		if isnan(rain) or rain==kFloatMissing then
			rain_class= 9
		elseif rain<rain_limit1 then
			rain_class= 0
		elseif rain<rain_limit2 then
			rain_class= 1
		elseif rain<rain_limit3 then
			rain_class= 2
		elseif rain<rain_limit4 then
			rain_class= 3
		elseif rain<rain_limit5 then
			rain_class= 4
		elseif rain<rain_limit6 then
			rain_class= 5
		elseif rain<rain_limit7 then
			rain_class= 6
		else
			rain_class= 7
		end

		local rform= mrform[mpos]
		local rform_class= (isnan(rform) or (rform==kFloatMissing)) and 9 or math.floor(rform)

		local rtype= mrtype[mpos]
		local rtype_class= (isnan(rtype) or (rtype==kFloatMissing)) and 9 or math.floor(rtype)

		local thunder= mthunder[mpos]
		local thunder_class= 9	-- missing

		if isnan(thunder) or thunder==kFloatMissing then
			thunder_class= 9
		elseif thunder<thunder_limit1 then
			thunder_class= 0
		elseif thunder<thunder_limit2 then
			thunder_class= 1
		else
			thunder_class= 2
		end

		local fog= mfog[mpos]
		local fog_class = (isnan(fog) or (fog==kFloatMissing)) and 9 or math.floor(fog)

		weathernumber[pos]= 10000000 * version +
				    1000000 * thunder_class +
				    100000 * rform_class +
				    10000 * rtype_class +
				    1000 * rain_class +
				    100 * fog_class +
				    10 * n_class +
				    cloud_class

		loc= loc+1
	end

	return weathernumber
end

local function locationdata(m,locations)
	if not type.Matrix(m) then
		error "locationdata: No data"
	elseif #locations.locations==0 then
		error "locationdata: No locations"
	end

	local lm= new_ScalarMatrix(xy(#locations.locations,1), nil, m.projection)
	local loc= 1

	for pos,_ in points(lm) do
		lm[pos]= m[locations.locations[loc]]
		loc= loc+1
	end

	return lm
end

--
-- Get grid for given parameter
--
local function getgrid(raw,param,locations)
	if param==WeatherNumberParam then
		return calcweathernumber(raw,locations)
	elseif raw then
		return #locations.locations>0 and locationdata(raw[param],locations) or raw[param]
	else
		error("No raw")
	end
end

--
-- Return data
--
local function querydata(args,locations)
	if type.Matrix(args.data) then
		--
		-- Return given data, no query
		--
		return { args.data }
	end

	local ret= {}
	local dataset= getdataset(args)
	local levels= args.levels or {}

	for d=1,#dataset.datas do
		local times= args.times or getdefaulttime(dataset.datas[d].data)

		for p=1,#dataset.parameters do
			if #levels>0 then
				for l=1,#levels do
					if dataset.datas[d].data==nil then
						ret[#ret+1]= {}
						if args.requiredata then return ret end
					else
						local gargs= {}
						gargs[args.leveltypetype]= levels[l]

						for t=1,#times do
							if datatime(dataset.datas[d].times,times[t]) then
								gargs.time= times[t]
								local status,g= pcall(getgrid,dataset.datas[d].data(gargs),dataset.parameters[p],locations)
								ret[#ret+1]= status and g or {}

								if args.requiredata and type(ret[#ret])=="table" then return ret end
							end
						end
					end
				end
			elseif dataset.datas[d].data==nil then
				ret[#ret+1]= {}
				if args.requiredata then return ret end
			else
				local gargs= {}

				for t=1,#times do
					if datatime(dataset.datas[d].times,times[t]) then
						gargs.time= times[t]
						local status,g= pcall(getgrid,dataset.datas[d].data(gargs),dataset.parameters[p],locations)
						ret[#ret+1]= status and g or {}

						if args.requiredata and type(ret[#ret])=="table" then return ret end
					end
				end
			end
		end
	end

	if args.includes then
		-- Get metadata (i.e. station id, name and/or location for point data, and/or pressures for sounding data)
		--
		-- ret.data= data
		--
		-- ret.dataids= {{dataids}}
		-- ret.datanames= {{datanames}}
		-- ret.locations= {{locations}}
		-- ret.pressures= {{pressures}}
		--
		ret= { data= ret }
		local gargs= {}

		for d=1,#dataset.datas do
			for k,inc in pairs(args.includes) do
				if inc.f then
					if ret[k]==nil then ret[k]= {} end

					if dataset.datas[d].data==nil then
						ret[k][#ret[k]+1]= {}
					elseif not inc.timef then
						ret[k][#ret[k]+1]= inc.f(dataset.datas[d].data)
					else
						local times= args.times or getdefaulttime(dataset.datas[d].data)

						for t=1,#times do
							gargs.time= times[t]
							local status,r= pcall(getraw,dataset.datas[d].data,gargs)
							ret[k][#ret[k]+1]= status and inc.f(r) or {}
						end
					end
				end
			end
		end

		if ret.pressures then
			--
			-- Remove missing sounding pressure values and corresponding data values; they are of no use without pressure/height information
			-- (well height could exist in the data, but we don't use/check it).
			--
			-- If there's no pressure values (data's type is table, not matrix), there's no or other data values either; returning an empty
			-- table (common for all stations).
			--
			-- Otherwise the original matrix (common for all stations) is replaced by a table of station data matrixes
			--
			-- 'sdataids' include is used to get the stations (number of them) when 'dataids' is not included by the query
			--
--DUMP(ret)
			local ntimes= args.times and #args.times or 1

			for i=1,#ret.pressures do
				local m= ret.pressures[i]
				local dataids= ret.dataids or ret.sdataids
				local ndataids= dataids[i] and #dataids[i] or 0
				local si= {}
				local nv=0
				local nx= type(m)=="table" and 0 or m.size.x-1
				local j

				if ndataids>0 then
					for j=0,nx do
						if m[xy(j,0)]~=32700 then
							local s= (j%ndataids)+1
							if si[s]==nil then si[s]= {} end
--print("Pick s="..s.." n="..(#si[s]+1).." idx="..j)
							si[s][#si[s]+1]= j
							nv= nv+1
						end
					end
				end

				nx= nx+1

				if nv<=nx then
					if nv==0 then
						ret.pressures[i]= {}
					else
						local spm= {}

						for s=1,ndataids do
							spm[s]= {}

							nx= si[s] and #si[s] or 0

							if nx>0 then
								local pm= matrix(xy(nx,1))
								j=0

								for n=1,nx do
									pm[xy(j,0)]= m[xy(si[s][n],0)]
									j= j+1
								end

								spm[s]= pm
							end
						end

						ret.pressures[i]= spm

						for p=1,#dataset.parameters do
							local sdm= {}

							for s=1,ndataids do
								sdm[s]= {}

								nx= si[s] and #si[s] or 0

								if nx>0 then
									local dm= matrix(xy(nx,1))
									j=0

									for n=1,nx do
										dm[xy(j,0)]= ret.data[((p-1)*ntimes) + i][xy(si[s][n],0)]
										j= j+1
									end

									sdm[s]= dm
								end
							end

							ret.data[((p-1)*ntimes) + i]= sdm
						end
					end
				end
			end
		end

		if ret.sdataids then ret.sdataids=nil end
	end

	return (#locations.locations>0 and #ret==1) and ret[1] or ret
end

--
-- Return cross section data
--
local function querycross(dqargs,cqargs)
	local ret= {}

	--
	-- Abort data selection loop on first empty result
	--
	dqargs.requiredata= true

	local dataset,d= getdataset(dqargs)
	local levels= dqargs.levels or { true }
	local times= dqargs.times or { NOW }

	for d=1,#dataset.datas do
		local raw= dataset.datas[d].data
		if not type.Raw(raw) then error(dataset.datas[d].name.."["..dataset.datas[d].origintime.."]: no data") end

		for p=1,#dqargs.parameters do
			for l=1,#levels do
				for t=1,#times do
					local gargs= {}
					gargs[dqargs.leveltypetype or "ground"]= dqargs.leveltypetype and cqargs.flightroute and levels or levels[l]

					ret[#ret+1]= {}

					local status,g= pcall(getgrid,raw(gargs),dqargs.parameters[p])

					if status and g and type(g)~="table" then
						ret[#ret]= cross(raw, dqargs.parameters[p], cqargs.locations, (cqargs.flightroute or cqargs.timecross) and times or times[t], gargs, cqargs.flightroute)
					end

					if cqargs.flightroute or cqargs.timecross then break end
				end

				if cqargs.flightroute then break end
			end
		end
	end

	return ret
end

--
-- Get and configure contourcollector
--
local function getcc(ccargs)
	local cc= contourcollector()
	local defaults= { Decimals= 0 }

	if ccargs~=nil then
		for k,v in pairs(ccargs) do
			cc:config(k,v)
			defaults[k]= nil
		end
	end

	for k,v in pairs(defaults) do
		if v~=nil then
			cc:config(k,v)
		end
	end

	return cc
end

--
-- Return image
--
local function queryimage(dqargs,lqargs,pqargs)
	require 'newcairo'

	--
	-- Get and configure contourcollector
	--
	local cc= getcc(pqargs.labelsettings)

	--
	-- Abort data query loop on first empty result
	--
	dqargs.requiredata= true

	local datas,d= querydata(dqargs,lqargs),1
	local gs= type.Matrix(datas[1]) and datas[1].size or gridsize
	local x_max,y_max= gs.x-1, gs.y-1
	local xscale= pqargs.size[1]/x_max
	local yscale= pqargs.size[2]/y_max
	local levels= dqargs.levels or { 0 }
	local times= dqargs.times or { NOW }

	local cs,cr= newcairo.surface( x_max*xscale,y_max*yscale )

	scale_to_grid( cr, gs, 0,0, cs.width, cs.height )

	for p=1,#dqargs.parameters do
		for l=1,#levels do
			for t=1,#times do
				if type(datas[d])~="table" and pqargs.contours[dqargs.parameters[p]] then
					if type(pqargs.contours[dqargs.parameters[p]])=="string" then pqargs.contours[dqargs.parameters[p]]= { pqargs.contours[dqargs.parameters[p]] } end

					for c=1,#pqargs.contours[dqargs.parameters[p]] do
						cc:contourpaths( cr, datas[d], pqargs.contours[dqargs.parameters[p]][c])
					end
				else
					error("Unknown parameter '"..dqargs.parameters[p].."' (no data)")
				end

				d= d+1
			end
		end
	end

	cc:stroke(cr,true)

	return cs
end

--
-- Check data query time parameter and return table of time(s)
--
local function checktimes( times )
	if #times==3 and type(times[3])=="number" then
		local f

		if times[3]%60==0 then
			f= time_range_h
			times[3]= times[3]/60
		else
			f= time_range_mins
		end

		if type(times[2])=="number" then
    		local jd_start= _parse_jday(times[1]) or error( "Bad time: "..times[1], 2 )
			return f( jd_start, jd_start + ((times[2]-1) * times[3]), times[3] )
		else
			return f( unpack(times) )
		end
	end

	local ret= {}

	for t=1,#times do
		ret[#ret+1]= _parse_jday(times[t]) or error( "Bad time: "..times[t], 2 )
	end

	return ret
end

--
-- Check location parameters. Parameters are returned as a table of tables
--
local function checklqargs(lqargs)
	if lqargs==nil then
		lqargs= {}
		lqargs.locations= {}

		return lqargs
	end

	if type(lqargs)~="table" then
		error("Invalid arguments, (table) expected")
	end

	-- Parameter names, in plural for tables
	--
	-- e.g. {locations={"62.7572N 25.9542E","62.8113N 25.9401E"}}
	--
	local fields= { "location" }

	for field=1,#fields do
		local fld= fields[field]
		local flds= fld.."s"

		if lqargs[fld]~=nil then
			if type(lqargs[fld])=="string" then
				lqargs[flds]= { latlon(lqargs[fld]) }
			elseif fld~="location" or not type.LatLon(lqargs[fld]) then error("Invalid query parameter '"..fld.."', string expected")
			else lqargs[flds]= { lqargs[fld] }
			end
		elseif flds and lqargs[flds]~=nil then
			if type(lqargs[flds])~="table" then
				error("Invalid query parameter '"..flds.."', table expected")
			end
		end
	end

	if lqargs.locations~=nil then
		for l=1,#lqargs.locations do if type(lqargs.locations[l])=="string" then lqargs.locations[l]= latlon(lqargs.locations[l]) end end
	else
		lqargs.locations= {}
	end

	return lqargs
end

--
-- Check data query parameters. Parameters are returned as tables of tables
--
local function checkdqargs(args,lqargs)
	if args==nil then
		error("No query args, datastring or a table expected")
	end

	if type(args)=="string" then
		--
		-- Get data as a table
		--
		args= getdatafromstring(args,{},true)
	end

	if lqargs==nil then
		lqargs= { locations= {} }
	end

	if not type.Matrix(args.data) then
		if type(args)=="table" then
			--
			-- Parameter names, in plural for tables
			--
			-- e.g. dataquery({name="HIR",parameter=4})
			-- 		dataquery({names={"HIR","EC"},parameters={4,13}})
			--
			local fields= { "leveltype", "gridsize", "projection", "decimals", "name", "origintime", "parameter", "level", "time", "include" }

			for field=1,#fields do
				local fld= fields[field]
				local flds= field>4 and fld.."s" or nil

				if args[fld]~=nil then
					if type(args[fld])=="number" then args[fld]= tostring(args[fld]) end

					if type(args[fld])=="string" then
						if fld=="leveltype" then
							if args[fld]=="Ground" then
								args.leveltype= nil
							else
								local leveltype= {}

								if args[fld]=="PressureLevel" then args.leveltypetype="hpa" leveltype[args.leveltypetype]= 850
								elseif args[fld]=="HybridLevel" then args.leveltypetype="hybrid" leveltype[args.leveltypetype]= true
								elseif args[fld]=="SoundingLevel" then args.leveltypetype="sounding" leveltype[args.leveltypetype]= true
								else error("Unknown leveltype '"..args[fld].."'")
								end

								args.leveltype= leveltype
							end
						end

						if flds then
							args[flds]= { args[fld] }
						end
					elseif not ((fld=="gridsize" and (type.MatrixPos(args[fld]) or type(args[fld])=="boolean")) or
								(fld=="projection" and type(args[fld])=="boolean") or
								(fld=="time" and type.jday(args[fld]))
							   ) then
						error("Invalid query parameter '"..fld.."', string expected")
					elseif (fld=="time") then
						args[flds]= { args[fld] }
					end
				elseif flds and args[flds]~=nil then
					if type(args[flds])~="table" then
						error("Invalid query parameter '"..flds.."', table expected")
					end
				end
			end
		else
			error("Invalid query parameter type '"..type(args).."', table expected")
		end

		if args.names==nil then error("Query track(s)/producer(s) missing") end

		if args.leveltype==nil or args.leveltypetype=="sounding" then args.levels=nil
		elseif args.levels==nil or #args.levels==0 then error("Query level(s) missing")
		else
			for l=1,#args.levels do
				if type(args.levels[l])~="number" then
					local level= tonumber(args.levels[l])

					if type(args.levels[l])~="string" or tostring(level)~=args.levels[l] then
						error("Invalid level '"..args.levels[l].."', number expected")
					end

					args.levels[l]= level
				end
			end
		end

		if args.parameters==nil or #args.parameters==0 then
			error("Query parameter(s) missing")
		else
			for p=1,#args.parameters do args.parameters[p]= tostring(args.parameters[p]) end
		end

		if args.times then args.times= checktimes(args.times) end

		if args.projection then rawset( _G, "projection", args.projection ) end

		if args.decimals then
			if string.match(args.decimals,"^%d+$") then
				args.decimals= tonumber(args.decimals)
				setdecimals( args.decimals )
			else
				error("Invalid decimals, decimals=n expected")
			end
		end

		if args.includes then args.includes= getquery_addmeta(args.includes, args.leveltypetype=="sounding") end

		lqargs= checklqargs(lqargs)
	end

	if args.gridsize==true then
		rawset( _G, "gridsize", true )
	elseif args.gridsize then
		local gridsize

		if type.MatrixPos(args.gridsize) then gridsize= args.gridsize
		else
		 	args.gridsize= { tonumber_all(string.match(args.gridsize,"^(%d+),(%d+)$")) }
			if #args.gridsize~=2 then error("Invalid gridsize, gridsize=\"x,y\" expected") end

			gridsize= xy(unpack(args.gridsize))
		end

		rawset( _G, "gridsize", gridsize)
	end

	return args,lqargs
end

--
-- Load unattached/unnamed contour descriptors from numerical contour table indexes
--
local function attachcontours(dqargs,pqargs)
	-- To attach unnamed contour descriptors (if any) to the parameters, there must be just one parameter
	-- or equal number of unnamed contour descriptors and parameters in total or having no contour descriptors.
	--
	-- The unnamed contour descriptors are attached to the parameters in the order of appearance
	--
	if #pqargs.contours==0 then return end

	-- First collect named contour descriptors, and store the indexes of unnamed contour descriptors if there are
	-- more than 1 parameter (if there is just 1 parameter, attach the unnamed contour descriptors right away)

	local unnamed= {}

	for c=1,#pqargs.contours do
		local t,n= getdatafromstring(pqargs.contours[c],pqargs.contours)

		if n==0 then
			if #dqargs.parameters==1 then
				if pqargs.contours[dqargs.parameters[1]]==nil then pqargs.contours[dqargs.parameters[1]]= {} end
				pqargs.contours[dqargs.parameters[1]][#pqargs.contours[dqargs.parameters[1]]+1]= pqargs.contours[c]
			else
				unnamed[#unnamed+1]= c
			end
		end
	end

	if #unnamed==0 then return end

	if #unnamed~=#dqargs.parameters then
		-- Count parameters having no contour descriptors
		--
		local n= 0

		for p=1,#dqargs.parameters do
			if pqargs.contours[dqargs.parameters[p]]==nil then n= n+1 end
		end

		if n~=#unnamed then
			error("Can't attach "..#unnamed.." unnamed contour descriptor(s) to "..#dqargs.parameters.." parameters, of which "..n.." having no contour descriptors")
		end
	end

	local pnext= 1

	for c=1,#unnamed do
		local pattach

		for p=pnext,#dqargs.parameters do
			if #unnamed==#dqargs.parameters or pqargs.contours[dqargs.parameters[p]]==nil then
				pqargs.contours[dqargs.parameters[p]]= { pqargs.contours[unnamed[c]] }
				pattach= p

				break
			end
		end

		if pattach==nil then
			error("Ran out of parameters when searching for a parameter for an unnamed contour descriptor")
		end

		pnext= pattach+1
	end
end

--
-- Check cross section query parameters. Parameters are returned as tables of tables
--
local function checkcqargs(dqargs,cqargs,crosstype)
	if type(dqargs)~="table" or type(cqargs)~="table" then
		error("Invalid arguments, (table,table) expected")
	end

	--
	-- Currently cqargs carries only locations, so it can be passed as is to checkdqargs
	--
	dqargs,cqargs= checkdqargs(dqargs,cqargs)

	if crosstype then cqargs[crosstype]= true end

	if #cqargs.locations==0 then
		error("cross: Location(s) missing")
	end

	if cqargs.flightroute or cqargs.timecross then

		local levelcnt= cqargs.flightroute and dqargs.levels and #dqargs.levels or #cqargs.locations
		local timecnt= dqargs.times and #dqargs.times or #cqargs.locations

		if cqargs.flightroute and levelcnt==1 then
			for l=2,#cqargs.locations do dqargs.levels[l]= dqargs.levels[1] end
			levelcnt= #dqargs.levels
		end

		if timecnt==1 then
			for t=2,#cqargs.locations do dqargs.times[t]= dqargs.times[1] end
			timecnt= #dqargs.times
		end

		if (cqargs.flightroute and #cqargs.locations~=levelcnt) or #cqargs.locations~=timecnt then
			if cqargs.flightroute then
				error("Equal number of locations, levels and times required for flightroute")
			else
				error("Equal number of locations and times required for timecross")
			end
		end
	end

	return dqargs,cqargs
end

--
-- Check picture query parameters. Parameters are returned as tables of tables
--
local function checkpqargs(dqargs,pqargs)
	if dqargs==nil or type(pqargs)~="table" then
		error("Invalid arguments, (datastring|table,table) expected")
	end

	-- Store/return empty locations (lqargs) too, to be passed on to queryimage() (and further to querydata())
	--
	dqargs,lqargs= checkdqargs(dqargs)

	--
	-- Parameter names, in plural for tables
	--
	-- e.g. picturequery({name="HIR",parameter=4},{size="800,600",contour="1 0 0 ..."})
	-- 		picturequery({names={"HIR","EC"},parameters={4,13}},{size="800,600",contours={"1 0 0 ...","1 0 0 ..."}})
	--
	local fields= { "size", "contour", "labelsetting" }

	for field=1,#fields do
		local fld= fields[field]
		local flds= field>1 and fld.."s" or nil

		if fld~="labelsetting" and pqargs[fld]~=nil then
			if type(pqargs[fld])=="string" then
				if fld=="size" then
		 			pqargs.size= { tonumber_all(string.match(pqargs.size,"^(%d+),(%d+)$")) }
					if #pqargs.size~=2 then error("Invalid picture size, size=\"x,y\" expected") end
				elseif #dqargs.parameters==1 then
					pqargs.contours= {}
					pqargs.contours[dqargs.parameters[1]]= { pqargs.contour }
				else
					error("'contours' (a table of contours) must be given when more than one parameter is given")
		 		end
			else
				error("Invalid query parameter '"..fld.."', string expected")
			end
		elseif flds and pqargs[flds]~=nil then
			if type(pqargs[flds])~="table" then
				error("Invalid query parameter '"..flds.."', table expected")
			end
		end
	end

	if pqargs.size==nil then
		error("Picture size missing")
	elseif pqargs.contours==nil then
		error("Contour descriptor(s) missing")
	end

	--
	-- Note:
	--
	-- Contour descriptors for parameters given with number, ":number" or "name:number" must be
	-- given as "key = val" or [key] = "val" settings; e.g. setting "4"="1 0 0 ..." can't be used).
	--
	-- Parameter id's (given as a number too) are stored as strings, and the corresponding contour table
	-- keys are strings as well. Explicit numerical keys should not be used, they are supposed to store
	-- unattached/unnamed contour descriptors.
	--
	-- Attach the yet unattached ("4=1 0 0 ...") and unnamed (plain "1 0 0 ...") contour descriptors
	-- from numerical table indexes.
	--
	-- To attach unnamed contour descriptors (if any) to the parameters, there must be just one parameter
	-- or equal number of unnamed contour descriptors and parameters in total or having no contour descriptors
	--
	attachcontours(dqargs,pqargs)

	--
	-- Check each parameter has at least 1 contour descriptor
	--
	for p=1,#dqargs.parameters do
		if pqargs.contours[dqargs.parameters[p]]==nil then
			error("Contour descriptor(s) missing for parameter '"..dqargs.parameters[p].."'")
		end
	end

	return dqargs,lqargs,pqargs
end

--
-- Data/image queries
--
function dataquery()
	--
	-- Query returning data
	--
	local function query(args,lqargs) return querydata(checkdqargs(args,lqargs)) end

	--
	-- Querys returning cross section data
	--
	local function crossquery(dqargs,cqargs) return querycross(checkcqargs(dqargs,cqargs)) end
	local function timecrossquery(dqargs,cqargs) return querycross(checkcqargs(dqargs,cqargs,"timecross")) end
	local function flightroutequery(dqargs,cqargs) return querycross(checkcqargs(dqargs,cqargs,"flightroute")) end

	--
	-- Query returning image
	--
	local function imagequery(dqargs,pqargs) return queryimage(checkpqargs(dqargs,pqargs)) end

	return {
		query = query,
		cross = crossquery,
		timecross = timecrossquery,
		flightroute = flightroutequery,
		picture = imagequery,
		image = imagequery
	}
end

--
-- Q2 smarttools PEEKXY, PEEKXY3
--
local function PEEK(m,xoffset,yoffset,distance)
    local peekpos= distance and
        function(m,m2,gridpos,xoffset,yoffset)
			return peekxy(m2,m,gridpos,xoffset,yoffset)
		end
	  or
        function(m,m2,gridpos,xoffset,yoffset)
			return peekxy(m2,m,gridpos,xy(xoffset,yoffset))
		end

	if distance then
		proto( "Matrix,number,number",m,xoffset,yoffset)
	else
		proto( "Matrix,int,int",m,xoffset,yoffset)
	end

	if xoffset==0 and yoffset==0 then return m end

	if m.grid==nil then error( "No grid for matrix" ) end

	local param= m.param
	if param==nil then error( "Matrix data parameter is unknown" ) end

	local pr= projection
	local gs= gridsize

	-- Get native grid matrix
	--
	projection= true
	gridsize= true

	local mnative= m.grid.raw(m.level or {})[param]

	-- Get output matrix
	--
	-- Note: By passing true as 4. parameter the returned matrix could be "repeeked"
	--		 (it has 'grid' property set). As of now this adds no functionality,
	--		 and the feature is experimental (lua gc / memory issues ?). Use/test if needed
	-- 
	local mpeek= new_ScalarMatrix(gs,m,m.projection,false)

	projection= pr
	gridsize= gs

	-- Peek
	--
	local xs= m.size.x
	local ys= m.size.y
	local nan= m[xy(xs,ys)]

	for x=0,xs-1 do
		for y=0,ys-1 do
			local gpos= xy(x,y)
			local ppos= peekpos(m,mnative,gpos,xoffset,yoffset)

 			mpeek[gpos]= ppos and mnative[ppos] or nan
		end
	end

	return mpeek
end
function PEEKXY(m,xoffset,yoffset)
	return PEEK(m,xoffset,yoffset,false)
end
function PEEKXY3(m,xoffsetkm,yoffsetkm)
	return PEEK(m,xoffsetkm,yoffsetkm,true)
end
