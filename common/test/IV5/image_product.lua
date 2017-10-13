--[[

Script contains operations which are executed inside q3-engine. Script creates images from surface-querydata showing 
certain parameters. Used querydata-files are configured in ../q3.conf-file.

@version 17.07.2009 
@author juha.vainola@fmi.fi

--]]

local MY_PATH= arg[0]:match( "^(.+)/[^/]+$" ) or ""

-- Read some common lua-functions...
--
dofile(MY_PATH.."common_functions.lua")

--
--
-- GLOBALS BEGIN
--
--

-- type:Boolean When this is set to true then only tests (test_image_product.lua) are run.
--
local RUN_TESTS=false

-- type:String Filepath (and filemask) to a directory where input querydata is located
--
QUERYDATA_MASK="data/pinta/*.sqd"

-- type:String Filepath to a directory where the produced images are written
--
local OUTPUT_DIR="data/out/"

-- type:String String which is appended in the filename of the produced images.
--
local OUTPUT_FILE_NAME="CLB"

-- type:String Filepath to an image which acts as the background of the produced images
--
BG_IMAGE="data/meri_ranta_raja_isot_jarvet_ranta.ClB.gif"

-- type:int Width of the produced images
--
local WIDTH=375

-- type:int Height of the produced images
--
local HEIGHT=416

-- type:Table Contains names of the parameters whose data is drawn on the images.
--
PARAMETERS={"FLMinBase:1003"}

-- type:Table Contains colors (drawn in the image) grouped according to parameter-values.
--
IMAGE_COLORS={["valueColor"]={[{0,2}]="153,51,102",[{2,5}]="166,51,255",[{5,10}]="130,92,255",[{10,15}]="120,122,255",[{15,50}]="122,176,255",[{50,100}]="51,204,255",[{100,200}]="153,224,255",[{200,300}]="153,255,255",[{300,}]="153,255,255"}}

local TIMESTAMP_FORMAT="%Y%m%d%H"

COMBINED_PARAMETER={}

-- Parse command line arguments...
--
for _,argumentString in ipairs(arg) do
	local key,value= argumentString:match( "^(.-)=(.+)$" )
	
	if(key=="output_dir") then
		OUTPUT_DIR=value
	elseif(key=="bg_image") then
		BG_IMAGE=value
	elseif(key=="width") then
		WIDTH=tonumber(value)
	elseif(key=="height") then
		HEIGHT=tonumber(value)
	elseif(key=="parameters") then
		PARAMETERS=split_by_comma(value)
	elseif(key=="combined_parameter") then
		COMBINED_PARAMETER=split_by_comma(value)
	elseif key:match( "^(.+):(.+)$" ) then
		local colorIndex,index= key:match( "^(.+):(.+)$" )
		if(IMAGE_COLORS[colorIndex]==nil) then IMAGE_COLORS[colorIndex]={} end
		IMAGE_COLORS[colorIndex][split_by_comma(index)]=value
	elseif(key=="timestamp_format") then
		TIMESTAMP_FORMAT=value
	elseif(key=="output_file_name") then
		OUTPUT_FILE_NAME=value
	elseif(key=="run_tests") then
		RUN_TESTS=value
	elseif key=="avimerge_querydata_mask" then
		QUERYDATA_MASK=value
	else
		error("Unknown command line argument: "..argumentString)
	end
end


--
--
-- FUNCTIONS BEGIN
--
--


-- Selects color (which is used to render value on image) according to given value.
-- 
-- @param value type:Float
--
-- @return type:String
--
function selectColor(value)
	
	for _,paramLimits in pairs(IMAGE_COLORS) do
		for limits,col in pairs(paramLimits) do
			local min,max=tonumber(limits[1]),tonumber(limits[2])		
			if(max==nil and min<=value) then return col
			elseif(min<=value and max>value) then return col end
		end
	end
	return nil
end

function selectMultiColor(parameterValues)
	local color
	local index=999	
	
	for param,value in pairs(parameterValues) do		
		local i=0
		local sortedLimits=table_keys_in_sorted_order(IMAGE_COLORS[param],function (a,b) if(a[1]<b[1]) then return true else return false end end)	 
		for _,limits in ipairs(sortedLimits) do
			local col=IMAGE_COLORS[param][limits]
			local min,max=tonumber(limits[1]),tonumber(limits[2])			
			if(max==nil and min<=value and i<=index) then 
				color=col
				index=i
				break
			elseif(min<=value and max>value and i<index) then 
				color=col
				index=i
				break	
			end
			i=i+1
		end				
	end
	return color
end

function drawSingleCircle(cr,pos,colors)
	
	if colors~=nil then
	    colors=split_by_comma(colors)
		cr.circle( pos.x,pos.y, 0.7 ).set_source_rgb( colors[1]/255,colors[2]/255,colors[3]/255 ).fill()
	end
end

function drawIsoCurves(cr,matrix)

	local step= 2
	
	for val= floor(min(matrix)/step-1)*step, max(matrix), step do
	    draw_for_stroke( cr, matrix.isocurves(val) )
	    cr.set_dash( {1,1} ).set_source_rgb(BLACK).stroke()
	end
end


function drawCircles(cr,matrix)
	
	for pos,value in points(matrix) do			
		
		if not isnan(value) then	      		
	      	drawSingleCircle(cr,pos,selectColor(value))
		end
	end
end

function drawCirclesFromMultipleMatrixes(cr,matrixes)
	
	local firstParam,firstMatrix
	for p,m in pairs(matrixes) do firstParam=p; firstMatrix=m; break end
	for pos,_ in points(firstMatrix) do
		local paramValues={}	
		for p,m in pairs(matrixes) do			
			if not isnan(m[pos]) then paramValues[p]=m[pos] end
		end
		if next(paramValues) ~= nil then drawSingleCircle(cr,pos,selectMultiColor(paramValues)) end		
	end
end

function drawLines(cr,matrix)
	for pos,value in points(matrix) do		
		if (value>0) then
			if(value==1) then
				drawSingleCircle(cr,pos,selectColor(0))	      	  	
			end
			cr.move_to(pos.x-0.7,pos.y)
			cr.set_source_rgb(1,1,1).set_line_width(0.15).line_to(pos.x+0.7,pos.y).stroke()			
		end
		
	end
end

function drawOriginTime(cr,originTime)
	cr.identity_matrix()  -- back to regular coordinates
	   .set_source_rgb(0,0,0)
	   .select_font_face('Serif', 'normal', 'normal')
	  .set_font_size(12)
	   .move_to( 10, 15 ) 
	   .show_text( "OriginTime: "..os.date("!%Y%m%d%H",originTime.epoch))
end

function drawParameter(cr,parameter,matrix)
	
	if(parameter=="FOG") then
		drawLines(cr,matrix)
	--elseif (parameter=="P") then
		--drawIsoCurves(cr,matrix)
	else
		drawCircles(cr,matrix)
	end	
end

function drawCombinedParameter(cr,matrixes)
	drawCirclesFromMultipleMatrixes(cr,matrixes)
end

function drawSingleImage(filename,grid,originTime)

	local matrix=grid[array_merge(PARAMETERS,COMBINED_PARAMETER)[1]]
	local xs,ys= matrix.size.x, matrix.size.y
	
	local image,w,h=newcairo.surface(BG_IMAGE)	
	local pat=newcairo.pattern_for_surface(image)
	local cs,cr= newcairo.surface( WIDTH,HEIGHT)
	local cs2,cr2= newcairo.surface( WIDTH,HEIGHT, {filename=filename} )
	   
	cr .save().set_source(pat).paint_with_alpha(1).restore()
	   -- convert so we can draw with matrix coordinates
	   --
	   .scale(WIDTH/xs,-HEIGHT/ys)       -- y grows from down to up
	   .translate( 0.5, -ys+0.5 )  -- origin is bottom left
	
	if(next(PARAMETERS) ~= nil) then
		for _,param in ipairs(PARAMETERS) do
			drawParameter(cr,param,grid[param])
		end
	end
	if(next(COMBINED_PARAMETER) ~= nil) then
		local matrixes={}
		for _,param in ipairs(COMBINED_PARAMETER) do
			matrixes[param]=grid[param]
		end
		drawCombinedParameter(cr,matrixes)
	end
	
	drawOriginTime(cr,originTime)
	
	local pat2=newcairo.pattern_for_surface(cs)
	cr2.set_source(pat).paint().set_source(pat2).paint_with_alpha(0.9)

end

function createImagesWithCairo(surfaceSqd)
	local originTime=surfaceSqd.origintime
	for grid,time in grids_by_time(surfaceSqd) do	
LOG( os.date("%d.%m.%Y %H",time.epoch) )
        local filename=OUTPUT_DIR..os.date(TIMESTAMP_FORMAT,time.epoch).."_"..OUTPUT_FILE_NAME..".png"
		drawSingleImage(filename,grid,originTime)
	end
	
end


--
--
-- MAIN BEGIN
--
--

do

	require "metqu"
	--require "svgcore"
	require "newcairo"
	
	if(RUN_TESTS) then
		dofile(MY_PATH.."/../../test/test_image_product.lua")
		return 0
	end
	
	local surfaceSqd, err= raw( QUERYDATA_MASK )
	assert( surfaceSqd, err )
	
	createImagesWithCairo(surfaceSqd)
	return 0

end
