--[[

  AVIMERGE_REWORKED.LUA                   Copyright 2009, Ilmatieteen laitos

  Purpose:

  Input:

  Output:

  Usage:
    LUA_CPATH=... LUA_LPATH=... \
    lua [-lstrict] avimerge_reworked.lua 

  Changes:
    AKa 5-Oct-2009:         Revised, trying to see why it's slooooooow.
                                - using shorter variable names
    juha.vainola@fmi.fi     Basic work (version 17.07.2009


Algorhitm:

surface_sqd <- read surface sqd file
model_sqd <- read model sqd file
r <- create raw object according to surface_sqd with necessary IV5-parameters
loop every timestamp inside r
{
	insert FOG-data for timestamp from surface_sqd to r
	loop every level inside r for timestamp
	{
		loop every N-parameter value inside r for level and timestamp
		{
			loop every height of the FL*Cover parameters
			{
				if level < height 
					FL*Cover <- max(N, FL*Cover)
			}
		}
	}
	loop every point inside r for current time
	{
		previous_significant_FL <- nil
		loop every height of the FL*Cover parameters
		{
			if FL*Cover >= SIGNIFICANT_N_LIMIT
			{
				if previous_significant_FL==nil
					FLMinBase <- bottom height of the FL*Cover parameter
				FLMaxBase <- bottom height of the FL*Cover parameter
				previous_significant_FL <- FL*Cover
			}			
		}
	}	
}
loop every timestamp inside r
{
	loop every rain_precipitation, precipitation_form inside surface_sqd
	{
		loop every other limit of precipitation_form
		{
			if rain_precipitation < limit
				AVIVIS <- linear interpolation between correct limits
		}
	}
}
--]]

--local MY_PATH= arg[0]:match( "^(.+)/[^/]+$" ) or ""
--dofile( MY_PATH.."common_functions.lua" )

local arg= {...}
require "common_functions"

IV5_PARAMS= { "FLCbBase:1001","FLCbCover:1002","FLMinBase:1003","FLMaxBase:1004"--[[,"VERVIS"--]],"AVIVIS:1005" ,"FOG", "P"}


---=== Config ===---

-- Limit of "significant" TotalCloudCover-value. When calculating bottom and top heights of clouds only heights
-- which contain N-value greater than SIGNIFICANT_N_LIMIT-value are used.
--
SIGNIFICANT_N_LIMIT= 62.5

-- Top-heights of used cloud layers in hectofeet
--
CLOUD_LAYER_COVERS= {2,5,10,15,50,100,200,300}

-- Filename mask to model querydata files (input; hybrid levels).
--
MODEL_QUERYDATA_MASK= "data/mallipinta/*.fqd"     --"/home/vainola/data/iv5/ecmwf/mallipinta/*.sqd"

-- Filename mask to surface querydata files (input; ground level).
--
SURFACE_QUERYDATA_MASK= "data/pinta/*.sqd"          --"/home/vainola/data/iv5/ecmwf/pinta/*.sqd"

-- Output file
--
SQD_OUT= "data/avimerge/avi_ec.sqd"                           -- "/home/vainola/data/iv5/out/avi_ec.sqd"

-- Precipitation intensity (x-scale; 1st, 3rd,..) and visibility (y-scale; 2nd, 4th,..) points for 
-- every needed precipitation form. These points are used to calculate visibility according to 
-- precipitation intensity.
--
VISIBILITY_LIMITS={ [0]={0.5,10,4,4,10,0}, [1]={0,10,0.5,2,4,0.7,10,0}, [2]={0,10,0.05,4,0.1,2,0.2,0} }
    
-- Projection used in the produced querydata.
--
PROJECTION="stereographic,20,90,60:6,51.3,49,70.2"

-- Grid size used in the produced querydata.
--
GRID_SIZE={82,91}        

-- type:Boolean When this is set to true then only tests (test_avimerge.lua) are run.
--
RUN_TESTS=false

--
-- Read command line params to the config variables
--
for _,arg_n in ipairs(arg) do
    -- AKa: Paloittelun voi tehdä:
    --      local argument, value= argumentString:match( "^(.-)=(.+)$" )

    local key,val= arg_n:match( "^(.-)=(.+)$" )
    
	if key=="cloud_layers" then
		CLOUD_LAYER_COVERS= split_by_comma(val, tonumber)

	elseif key=="significant_value" and tonumber(val) then
		SIGNIFICANT_N_LIMIT=tonumber(val)

	elseif key=="sqd_out" then
		SQD_OUT=val

	elseif key=="projection" then
		PROJECTION=val

	elseif key=="grid_size" then
		GRID_SIZE=split_by_comma(val)

	elseif key:find("visibility_limits") then 
	   
	   local _,prec_form= key:match( "^(.+):(.+)$" )
	   
	   if not prec_form then
	       error( "Bad param: "..arg_n )
        end
        
        VISIBILITY_LIMITS[prec_form]= split_by_comma(val,tonumber)
		
--[[    Was like this:
		local precForm= keyargument:sub(argument:find(":")+1)
		VISIBILITY_LIMITS[precForm]=split_string(value,",")
		for i,v in ipairs(VISIBILITY_LIMITS[precForm]) do
            VISIBILITY_LIMITS[precForm][i]= tonumber(v)
        end
]]

	elseif key=="model_querydata_mask" then
		MODEL_QUERYDATA_MASK= val

	elseif key=="surface_querydata_mask" then
		SURFACE_QUERYDATA_MASK= val
	elseif(key=="run_tests") then
		RUN_TESTS=val
	else
		error("Unknown command line argument: "..arg_n)
	end
end

--
-- [1..8] -> parameter name
--
CLOUD_LAYER_COVER_PARAMS={}

for i=1,#CLOUD_LAYER_COVERS do
	CLOUD_LAYER_COVER_PARAMS[i]= "FL"..i.."Cover:101"..i
end



---=== Functions ===---

--
-- Linear interpolation between two xy-points.
--
function linearInterpolation( x, x1,y1, x2,y2 )
    local y= y1+(x-x1)*((y2-y1)/(x2-x1))
    return y
end

-- 
-- uint= getCloudLayerCoverBottomHeight( cloudLayerCoverParamIndex_uint )
--
-- cloudLayerCoverParamIndex: Index-number of current cloud layer cover parameter (1..n).
--
-- Returns bottom height of desired cloud layer cover (unit: hectofeet).
--
local function getCloudLayerCoverBottomHeight( param_index )

	return (param_index>1) and CLOUD_LAYER_COVERS[param_index-1] or 0
end

-- 
-- [FLCbBase_num, FlCbCover_num]= CbBaseAndCoverValues(...)
--
-- Calculates 'FLCbBase' and 'FLCbCover' according to FL1..8Cover-parameters.
--
function CbBaseAndCoverValues( m_cloud_layers, pos, param_index, previous_N )

	local v_N= m_cloud_layers[param_index][pos]
	local bottom= getCloudLayerCoverBottomHeight(param_index)
	if(isnan(v_N)) then bottom=nan end
	
	if isnan(previous_N) or v_N<previous_N then        
     	
        m_cloud_layers["FLCbBase:1001"][pos]= bottom
        m_cloud_layers["FLCbCover:1002"][pos]= v_N
        previous_N= v_N
	end
	 return previous_N
end

-- 
-- Sets FLMinBase and FLMaxBase only if FL1..8Cover-value exceeds SIGNIFICANT_N_LIMIT.
--
-- Returns: previous FL1..8Cover value which exceeded the SIGNIFICANT_N_LIMIT (to be thrown back
--          at us on the next call)
--
function setSignificantCloudBaseValues( m_cloud_layers, pos, param_index, previous_significant_FL )

	local v_FL= m_cloud_layers[param_index][pos]	   
	local bottom= getCloudLayerCoverBottomHeight(param_index)
	
	if v_FL >= SIGNIFICANT_N_LIMIT then 
		--
		-- First SIGNIFICANT_N_LIMIT exceed is used for FLMinBase
		if previous_significant_FL==nil then
            m_cloud_layers["FLMinBase:1003"][pos]= bottom
        end
        
        --
        -- Last SIGNIFICANT_N_LIMIT exceed is used for FLMinBase
        m_cloud_layers["FLMaxBase:1004"][pos]= bottom        
		previous_significant_FL= v_FL
	end
	
	return previous_significant_FL
end

-- Calculate FL1..8Cover parameter for current grid-point according to given
-- N-parameter and level height of model querydata.
--
function setFlCoverParamValues(m_cloud_layers,pos,level_height,v_N,max_height)
	local start=1
	if(level_height > max_height) then start=8 end
	
    for i=start, #CLOUD_LAYER_COVERS do
    	local cloud_layer_height=CLOUD_LAYER_COVERS[i]
        if level_height > max_height or level_height<cloud_layer_height then
            -- TBD: Using 'v_N' here feels a LOT like it could be done as a matrix
            --      operation after this block. Is it so?   --AKa 6-Oct-2009
			
			if(isnan(v_N) and isnan(m_cloud_layers[i][pos])) then
				m_cloud_layers[i][pos]=nan
			else				
				m_cloud_layers[i][pos]= max( v_N, m_cloud_layers[i][pos] )
			end            
            break
        end
    end
end

--
-- Calculates FLCbBase, FLCbCover, FLMinBase and FLMaxBase parameters for current grid-point according to 
-- FL1..8Cover parameters (calculated inside setCloudLayerValue). 
--
-- 'm_cloud_layers':    { matrix [, ...] }  -- 1..8 matrices of 'FL?Cover' values
--
function setCloudBaseValues( m_cloud_layers, pos )

	local previous_FL=nan
	local previous_significant_FL
		
	for i=1,#CLOUD_LAYER_COVERS do	
		previous_significant_FL= setSignificantCloudBaseValues( m_cloud_layers, pos, i, previous_significant_FL )
		--previous_FL= CbBaseAndCoverValues( m_cloud_layers, pos, i, previous_FL )
	end
end


-- 
-- Calculate VERVIS and AVIVIS parameters for current grid-point according to given RR and PREF values.
--
function setVisibilityValues( m_AVIVIS, pos, v_RR, v_PREF )

	if not VISIBILITY_LIMITS[tostring(v_PREF)] then
	   error( "Unknown precipitation form encountered: "..v_PREF)
    end

	local limits= VISIBILITY_LIMITS[tostring(v_PREF)]	
	
	if(v_RR>=limits[#limits-1]) then
		m_AVIVIS[pos]=0
		return
	end
	m_AVIVIS[pos]=limits[2]

    for i=1,#limits,2 do    -- in steps of 2 (x1,y1,x2,y2,...)
		local x=limits[i]
		if v_RR<x then
            if i<2 then
                m_AVIVIS[pos]= limits[2]    
            else                
                m_AVIVIS[pos]= linearInterpolation( v_RR, limits[i-2],limits[i-1], x,limits[i+1] )
            end
			break			
		end
	end	
end


-- 
-- Loops through model querydata's levels (for given time) and calculates cloud
-- layer parameters for every point. 
--
-- 'm_cloud_layers':    { matrix [, ...] }  -- 1..8 matrices for writing 'FL?Cover' values
--
function processModelLevels( m_cloud_layers, vt, model_sqd )

    local max_height= CLOUD_LAYER_COVERS[#CLOUD_LAYER_COVERS]
    
    for g in grids_by_level(model_sqd, {time=vt}) do
		
		local m_GeomHeight=g.geom
		local done= true
		
		for pos,v_N in points(g.N) do
			
			if(m_cloud_layers[1].size.x>pos.x and m_cloud_layers[1].size.y>pos.y) then
				
				local level_height= metersToFeet( m_GeomHeight[pos] )/100    -- hectofeet
			
				if level_height < max_height then
		               done= false     -- at least one point was within the limits
		        end
				setFlCoverParamValues(m_cloud_layers,pos,level_height,v_N,max_height)
			end			
		end		
		if done then break end
	end
end

--
-- = calcVisibilities( surface_sqd, raw )
--
-- Calculates visibility parameters (VERVIS, AVIVIS) and adds them to 'raw'.
--
--  surface_sqd: Data of a certain surface querydata file.
--  raw:         Surface querydata into which visibility parameters are inserted.
--
local function calcVisibilities( surface_sqd, r )

LOG( "Processing times with visibility-parameters:" )

    for _,vt in ipairs(r.times) do
        local g= r{time=vt}

        if(in_table(vt,surface_sqd.times)) then
        
	        local m_AVIVIS=g["AVIVIS:1005"]    -- proxy matrix to change 'r'
						
	LOG( os.date("%d.%m.%Y %H",vt.epoch) )
	
			local rain_g= surface_sqd{ time=vt }   -- native projection and gridsize
	
	        local rain_PREF= rain_g.PREF
	
			for pos,rain_v_RR in points(rain_g.RR) do
				local rain_v_PREF= rain_PREF[pos]
				if not isnan(rain_v_PREF) then
	                setVisibilityValues( m_AVIVIS, pos, rain_v_RR, rain_v_PREF )                
				else
					m_AVIVIS[pos]=VISIBILITY_LIMITS[next(VISIBILITY_LIMITS)][2]
				end
			end			
		end
	end
end

--
-- raw= calcCloudLayers( model_sqd, surface_sqd )
--
-- Returns a Raw object based on 'surface_sqd', with added parameters
-- (calculated):
--
--  FL1Cover:   ...
--      ...
--  FL8Cover:   ...
--
-- Every flight-cover gets a calculated TotalCloudCover-value. 
-- This value is chosen from model levels' (which are on the same height as the flight-cover) 
-- TotalCloudCover-values. The maximum TotalCloudCover-value is used.
--
-- Also these parameters are calculated (according to the FL1..8Cover):
--
--  FLCbBase:   Bottom height of the cloud layer which has the minimum value of N-parameter
--  FLCbCover:  Minimum value of N-parameter (inside the 8 cloud layers)
--  FLMinBase:  Bottom height of the first cloud layer whose N-parameter value exceeds SIGNIFICANT (>=SIGNIFICANT_N_LIMIT)
--  FLMaxBase:  Bottom height of the last cloud layer whose N-parameter value exceeds SIGNIFICANT (>=SIGNIFICANT_N_LIMIT)
-- 
local function calcCloudLayers( model_sqd, surface_sqd )

	local r= raw( surface_sqd, { 
	                   origintime=model_sqd.origintime,
	                   times= model_sqd.times, 
	                   projection= PROJECTION, 
	                   gridsize= xy(GRID_SIZE[1],GRID_SIZE[2]), 
	                   params= merge_arrays( CLOUD_LAYER_COVER_PARAMS, IV5_PARAMS )
	           } )
	       
LOG( "Processing times with cloud-parameters:" )

	for _,vt in ipairs( r.times ) do
        local g= r{time=vt}

LOG( os.date("%d.%m.%Y %H",vt.epoch) )
        -- Create proxy matrices that point to 'g'
        --
        local m_cloud_layers= {}    -- [1..8]: matrix proxy to 'g.FL?Cover' data

        -- Keep handles to them (1..8) available and passed on to 'processModelLevels' and below
        -- (this is a vital optimization; avoiding unnecessary creation of matrix proxys)
        --
    	for i,p in ipairs(CLOUD_LAYER_COVER_PARAMS) do
    		m_cloud_layers[i]= g[p]     -- creates a proxy matrix that modifies 'r'
        end

        -- Also these will be written (create proxy matrices)
        --
        for _,v in ipairs(IV5_PARAMS) do
            m_cloud_layers[v]= g[v]
        end
        
        -- Insert FOG- and P-data from original surface sqd -file
        --
        g.FOG=surface_sqd{time=vt}.FOG
        g.P=surface_sqd{time=vt}.P

		processModelLevels( m_cloud_layers, vt, model_sqd )

        -- Calculate FLCbBase, FLCbCover, FLMinBase and FLMaxBase parameters for every point.
        --
    	for pos,_ in points( m_cloud_layers[1] ) do
	       setCloudBaseValues( m_cloud_layers, pos )
	   end
	   
	   --if (true) then error() end
    end
    
    
	
	-- Interpolate raw-object to same resolution as original surface-querydata.
	-- 
	-- AKa: Eikö olisi helpointa toimia yksillä 'projection' ja 'gridsize' arvoilla, ja lyödä ne "seinään" globaaleissa?
	--
	return r
	--[[return raw( r, {
	               projection= surface_sqd.projection, 
	               gridsize= surface_sqd.gridsize, 
	           } )--]]
end

--
--
-- MAIN BEGIN
--
-- AKa: Renaming things so 'this_is' a variable and 'thisIs' a function.
--
do
	require "metqu"
	
	if(RUN_TESTS) then
		dofile(MY_PATH.."/../../test/test_avimerge.lua")
		return 0
	end

    -- Read SQD files
    --
    --local model_sqd, err= raw( "/fmi/data/querydata/ecmwf/skandinavia/mallipinta/*.sqd", {params=N} )
	local model_sqd, err= raw( MODEL_QUERYDATA_MASK, {params=N} )
	assert( model_sqd, err )    -- 'err' gives real reason why no data was found
	

	local surface_sqd, err= raw( SURFACE_QUERYDATA_MASK, {params={RR,PREF}} )
	assert( surface_sqd, err )

    -- Do real work
    --
	local r= calcCloudLayers( model_sqd, surface_sqd )
	--local r=raw("/fmi/data/querydata/ecmwf/skandinavia/IV5_testi/avimerge/*.sqd")

	calcVisibilities( surface_sqd, r )

	-- Print out some of the returned data... (just testing)
	--
	
	--[[local position
	for l,g in levels(model_sqd, {time=model_sqd.times[10]}) do
		local height= g.GeomHeight
		for pos,v_n in points(g.N) do
			print(metersToFeet( height[pos] )/100,v_n)		
			position=pos
			break
		end
	end

	for vt,g in times(r) do
		if (vt==model_sqd.times[10]) then
			for _,p in ipairs(merge_arrays( CLOUD_LAYER_COVER_PARAMS, IV5_PARAMS )) do
				print(p,g[p][position])
			end
			break
		end
	end	--]]
	
	--[[local count=0
	for validtime,grid in times(surface_sqd) do
	
		local rain_PREF=grid.PREF
		for pos,rain_v_RR in points(grid.RR) do
			print("PREF:",rain_PREF[pos],"RR:",rain_v_RR)
			position=pos
			break
		end
		if(count>9) then break end
		count=count+1
	end
	
	count=0
	for vt,g in times(r) do
		for _,p in ipairs({"AVIVIS"}) do
			print(p,g[p][position])
		end
		if(count>9) then break end
		count=count+1
	end
	
	if(true) then return 0 end--]]

LOG( "Writing "..SQD_OUT.." .." )
    r.write( SQD_OUT )

LOG( "Done." )
	return 0
	
end
