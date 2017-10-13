--
-- CONTOUR.LUA                            Copyright 2009-2010, Ilmatieteen laitos
--
-- Cairo helper functions.
--
-- Note: To use these functions, the caller must 'require "newcairo"' to get a 'cr' 
--      context for us. We don't need to require that, though (this is duc[kt] typing).
--
-- TBD: This module is unfinished.  --AKa 31-Dec-2010
--

local bind= assert( select(1,...) )     -- C side exports

local contour_smoothen_one= assert( bind.contour_smoothen_one )
local calc_slants= assert( bind.calc_slants )

local table_sort=   assert( table.sort )
local table_remove= assert( table.remove )
local math_min=     assert( math.min )
local math_max=     assert( math.max )
local math_floor=   assert( math.floor )

bind=nil


---=== Helper functions ===---

assert( (-1)%9 == 8 )   -- testing behaviour of mod on negative integers



---=== Public functions ===---

--
-- cr= scale_to_grid( cr, gridsize_ud [, x0_num, y0_num, width_num, height_num] )
--
-- Change 'cr' scaling and transformations so that grid coordinates can be used for drawing.
--
-- In practise, this means not only scaling but swapping of the y axis direction (grid
-- coords grow from South to North; Cairo grows from up to down).
--
-- If the caller wishes to get back to the original (pixel based) coordinate system, he 
-- must use 'cr.save()' and 'cr.restore()' around the grid based drawing.
--
-- Line width is kept to the same native coordinate system size as what the caller has
-- preceding the transformation (in other words, the scaling is compensated).
--
-- If 'x0,y0' and 'width','height' are not given, filling the whole drawing surface is
-- expected.
--
function scale_to_grid( cr, gs, x0,y0, width,height )

    if x0 then
        proto( "CairoContext, MatrixPos, number, number, number, number", cr, gs, x0,y0, width,height )
    else
        proto( "CairoContext, MatrixPos", cr, gs, x0,y0, width,height )
        x0= 0
        y0= 0

        error "TBD: 'cr.get_target()' not implemented - use 0,0, cs.width, cs.height in your script"
        --[[
        local cs= cr.get_target()
        
        width= assert( cs.width )
        height= assert( cs.height )
        ]]
    end
    
    local fx= width/(gs.x-1)
    local fy= height/(gs.y-1)

    local line_width= assert( cr.info and cr.info.line_width )

    cr  .translate(x0,y0)           -- move origin to x0,y0
        .scale(fx,-fy)              -- scale and flip the y axis
        .translate( 0, -(gs.y-1) )  -- origin is bottom left

    -- Note: Whether we want to scale the line width here or in the application is arguable.
    --
    cr  .set_line_width( line_width / ((fx+fy)/2.0) )

    return cr
end

--
-- cr= contour_path_for_stroke( cr, [contour_ud [, ...]] )
--
-- Add the contour paths to the 'cr' Cairo context. Sections at edge of material
-- (or edge of holes) are skipped.
--
-- Note: The 'cr' transforms must be scaled by 'scale_to_grid' prior to this call.
--
function contour_path_for_stroke( cr, ... )
    proto( "CairoContext, [Contour], ...", cr, ... )

    for i=1,select('#',...) do
        local contour= select(i,...)
            --
            -- { {x=num, y=num, edge=bool} [, ...] }   (or similar userdata)

        local n= #contour

        local within_gap= false     -- if 'true', next operation is 'move_to()'
        local had_gap= false        -- if 'false', last point gets closed to the first

        local last

        for k= 1,n do
            local ck= contour[k]

            if not last then
--LOG( "Move to: "..(ck.x).." "..(ck.y) )
                cr.move_to( ck.x, ck.y )
                --cr.circle( ck.x, ck.y, 0.5 )

            elseif last.edge and ck.edge then
                within_gap= true
                had_gap= true
            else
                if within_gap then
                    cr.move_to( last.x, last.y )    -- start of new non-edge section
                    within_gap= false
                end
--LOG( "Line to: "..(ck.x).." "..(ck.y) )
                cr.line_to( ck.x, ck.y )
            end

            last= ck
        end

        if not had_gap then
            cr.close_path()
        end
    end
    
    return cr   -- chain further
end


--
-- 22-Nov-2011 PKi: contourcollector for collecting and drawing/filling/labeling contours
--
-- cc= contourcollector()
--
function contourcollector()
    --
    -- Contouring configuration parameter setters
    --
    -- Settings given by calls to config(param,val[,val,...]) are stored into configflds table.
    --
    -- Contour descriptor string is build using the data in configflds when contouring/filling
    -- (contourpaths()) is called
    --

    function _configErr( param, errMsg )
        error("config("..param.."): "..errMsg)
    end

    function _storeArg( obj, param, field, typeVal, value, ...)
        --
        -- Store parameter field's value into configflds.
        -- Check value against typeVal's type and min value (if given).
        --
        -- typeVal is string "color" for colors and table of valid values (2 strings) when called
        -- for label strategy; storing the index (1 or 2) + 1 of matching value (--> 2=horizontal, 3=tilted)
        --
        -- Minimum value and comparison flag (ge or gt; default ge) can optionally be given for numbers
        --
        local tt= type.table(typeVal)
        local tv= ((tt and typeVal[1]) or typeVal)
        local t= ((tt and type(typeVal[1])) or type(typeVal))
        local str= (t == type("string"))
        local minVal= (((select('#',...) > 0) and select(1,...)) or nil)
        local noteq= (((select('#',...) > 1) and select(2,...)) or nil)

        if (type(value) ~= t) then
            if (str and (tv == "color")) then
                t= typeVal
			else
            	_configErr( param, field..": "..t.." expected" )
            end
        elseif ((t == type(0)) and minVal and ((value < minVal) or (noteq and (value == minVal)))) then
            _configErr( param, field..": value >"..((noteq and " ") or "= ")..minVal.." expected" )
        elseif (tt) then
            if (not string.match(" "..table.concat(typeVal," ").." "," "..value.." ")) then
                _configErr( param, field..": one of "..table.concat(typeVal,",").." expected" )
            end

            value= (((value == typeVal[1]) and 2) or 3)
        elseif (str and value:match("%s")) then
            -- No spaces allowed; space is contour descriptor string's field separator
            --
            _configErr( param, field..": no spaces allowed" )
        end

        if (type.table(obj.configflds[field])) then
            local idx= #(obj.configflds[field])
            obj.configflds[field][idx+1]= value
        else
            obj.configflds[field]= value
        end

        return value
    end

    -- For setting mode (contouring or filling, contour levels) related parameters
    --
    function _setContourLevels(mode)
        return function( obj, param, nValues, ... )
            -- input:
            --   mode 1 (ControurRange): strokecolor,strokewidth,lo,hi,step
            --   mode 2 (ControurList): strokecolor,strokewidth,level,level,...
            --   mode 3 (FillRange): fillcolor,lo,hi[,step]
            --
            -- result (descriptor string):
            --   1 step 0 lo hi
            --   2 nLevels l1 l2 ... lN	nLevels is stored to step
            --   3 step 0 lo hi		if lo or hi is 32700, use min or max value rounded to step as range
            --   4 1 lo hi			currently 1 (stored to step) range only (only 1 fillcolor supported)
            --
            local lmode= mode

            if (mode ~= 2) then
                -- Stepped contour range (1) or aligned (3) or "as is" (4) fill range
                --
                if ((mode == 1) and (nValues ~= 5)) then
                    _configErr( param, "strokecolor,strokewidth,lorange,hirange,step expected" )
                elseif (mode == 3) then
                    if ((nValues ~= 3) and (nValues ~= 4)) then
                        _configErr( param, "fillcolor,lorange,hirange[,step] expected" )
                    end

                    if (nValues == 3) then
                        -- No step; mode 4
                        lmode= 4
                    end
                end

                local offset= (((mode == 3) and 0) or 1)

                --
                -- 10-Jan-2012 PKi: Accept 32700 (min value) as low range
                --
                local val= _storeArg( obj, param, "lo", 0, (select(2+offset,...)))
                local hi= select(3+offset,...)
                _storeArg( obj, param, "hi", 0, hi, ((val ~= 32700) and val) or (hi - 1), (mode == 3))
                _storeArg( obj, param, "step", 0, (((lmode ~= 4) and (select(4+offset,...))) or 1), 0)
            else
                -- List of contour levels (2)
                --
                if (nValues < 3) then
                    _configErr( param, "linecolor,linewidth,level[,level[,...]] expected" )
                end

                obj.configflds["step"]= nValues-2
                obj.configflds["levels"]= {}

                local i
                for i=3,nValues do
                    _storeArg( obj, param, "levels", 0, (select(i,...)))
                end
            end

            obj.configflds["mode"]= lmode

            -- Keep previous mode specific settings if available; otherwise set to default
            --
            local sc= (obj["strokecolor"] or "def")
            local sw= (obj["strokewidth"] or 2)
            local fc= (obj["fillcolor"] or "none")

            _storeArg( obj, param, "strokecolor", "color", (((mode <= 2) and select(1,...)) or sc) )
            _storeArg( obj, param, "strokewidth", 0, (((mode <= 2) and select(2,...)) or sw), 1 )
            _storeArg( obj, param, "fillcolor", "color", (((mode > 2) and select(1,...)) or fc) )
        end
    end

    -- For setting label or labelbox related parameters
    --
    function _setLabel(txtparam)
        return function( obj, param, nValues, ... )
            if ((nValues < 1) or (nValues > 3)) then
                if (txtparam) then
                    _configErr( param, "labelcolor[,fontheight[,labelstrategy]] expected" )
                else
                    _configErr( param, "boxfillcolor[,linecolor[,linewidth]] expected" )
                end
            end

            local p1= ((txtparam and "ContourLabelColor") or "ContourLabelBoxFillColor")
            obj.contourConfig[p1]( obj, p1, 1, select(1,...))

            if (nValues >= 2) then
                local p2= ((txtparam and "ContourFontHeight") or "ContourLabelBoxLineColor")
                obj.contourConfig[p2]( obj, p2, 1, select(2,...))

                if (nValues == 3) then
                    local p3= ((txtparam and "ContourLabelStrategy") or "ContourLabelBoxLineWidth")
                    obj.contourConfig[p3]( obj, p3, 1, select(3,...))
                end
            end
        end
    end

    -- For setting standalone contouring parameter
    --
    function _setContourParam( cfgname, typeVal, ...)
        local minVal= (((select('#',...) > 0) and select(1,...)) or nil)
        local noteq= (((select('#',...) > 1) and select(2,...)) or nil)

        return function( obj, param, nValues, ... )
            if (nValues ~= 1) then
                _configErr( param, "one parameter value expected" )
            end

            _storeArg( obj, param, cfgname, typeVal, (select(1,...)), minVal, noteq)
        end
    end

    -- Parameters settable thru config()
    --
    -- Contouring parameters
    --
    local _contourConfigFlds= {
         [ContourRange]= _setContourLevels(1),
         [ContourList]= _setContourLevels(2),
         [ContourFillRange]= _setContourLevels(3),
         [ContourLabel]= _setLabel(true),
         [ContourLabelColor]= _setContourParam( "labelcolor", "color" ),
         [ContourFontHeight]= _setContourParam( "fontheight", 0, 0, true),
         [ContourLabelStrategy]= _setContourParam( "labelstrategy", {ContourLabelsHorizontal,ContourLabelsTilted} ),
         [ContourLabelBox]= _setLabel(false),
         [ContourLabelBoxFillColor]= _setContourParam( "labelboxfillcolor", "color"),
         [ContourLabelBoxLineColor]= _setContourParam( "labelboxlinecolor", "color"),
         [ContourLabelBoxLineWidth]= _setContourParam( "labelboxlinewidth", 0 , 0 ),
         [ContourSmoothFactor]= _setContourParam( "smoothfactor", 0, 0)
    }
    --
    -- Labelizer parameters
    --
    local _labelConfigFlds= {
         [LimitAlongLine]= true,
         [LimitDirect]= true,
         [AllowOverlappingLines]= true,
         [AllowOverlappingLabels]= true,
         [DampenCornersLimitDeg]= true,
         [DampenCorners]= true,
         [BoostHorizontalLimitDeg]= true,
         [BoostHorizontal]= true,
         [CurveOptimizationFactor]= true,
         [Decimals]= true
    }

    -- Contourcollector; "static" fields (not cleared after stroke())
    --
    local cc= {
        object= true, contourConfig= _contourConfigFlds, labelConfig= _labelConfigFlds, configflds= {},
        matrix=nil, smatrix=nil, smoothing= { length=0, degree=0 }, tronhints= nil
    }

    -- Init; fields cleared after stroke()
    --
    function cc:_new()
        self.contours= {}
        self.contourvalues= {}
        self.contourdefs= {}
        self.labelizercfg= nil
        self.gridsize= nil

        return self
    end

    -- Reset
    --
    function cc:_reset()
        cc:_new()
    end

    -- Max # of levels evaluated from contour descriptor string (to guard stepped lo-hi ranges)
    --
    function cc:_maxlevels()
        return 1000
    end

    -- Return (level/contour) values as a table.
    --
    -- 'asc' controls whether each pair of subsequent values must have ascending order (lo/hi fill ranges).
    --
    function _pack( asc, ... )
        local t= {}
        local n, i
        local pn= nil

        for i=1,select('#',...) do
            n= tonumber((select(i,...)))

            if (asc and pn) then
                if ((pn >= n) and (pn ~= 32700)) then
                    return {}
                end

                pn= nil
            else
                pn= n
            end

            t[#t+1]= n
        end

        return t
    end

    -- Find min/max levels based on 'data', aligned at 'step'
    --
    function _findlimits( m, step, lo, hi )

        if (lo==32700) then
            local min= min(m)

            if (step>0.0) then
                local mod= fmod( abs(min), step )
                lo= ((mod==0.0) and min) or ((min>0.0) and (min+(step-mod))) or (min+mod)
                lo= math.floor((lo*10^3) + 0.5) / (10^3)
            else
                lo= min
            end
        end

        if (hi==32700) then
            local max= max(m)

            if (step>0.0) then
                local mod= fmod( abs(max), step )
                hi= ((mod==0.0) and max) or ((max>0.0) and (max-mod)) or (max-(step-mod))
                hi= math.floor((hi*10^3) + 0.5) / (10^3)
            else
                hi= max
            end
        end

        return lo,hi
    end

    -- Get contour drawing/filling/labeling definitions from Q2 style contour descriptor string
    --
    function _contoursettings( m, _contourdefs )
        --
        -- contourdefs:
        --
        -- 1 		            step 					zero_level(not used) 	lo 				hi
        -- 2 		            Nlevels					level1					..levelN
        -- 3 		            step 					zero_level(not used) 	lo 				hi
        -- 4 		            Nranges					lo1 					hi1	     		..loN	hiN
        -- stroke_color 	    fill_color 				stroke_width 			smooth_factor
        -- label_strategy 	    label_font_height 		label_text_color
        -- label_box_fill_color label_box_stroke_color	label_box_stroke_width
        --
        -- We only need descriptor's levels evaluated to load the contours and smooth_factor
        -- to call smoothing; for drawing/labeling the descriptor is just passed forward
        --
        -- 11-Jan-2012 PKi: Label color is checked too; if it is 'none', labels are not drawn
        --                  (otherwise invisible labels would appear as holes in the contours)
        --
        -- 12-Mar-2015 PKi: Added default drawing and labeling properties (now allowed to be missing)

        local cs= { levels= {},
					stroke_color= "black", fill_color= "black", stroke_width= "1",
					smoothing= { length= 0, degree= 0 },
					label_font_height= "16", label_text_color= "black", label_box_fill_color= "none", label_box_stroke_color= "none", label_box_stroke_width= "1",
					labelize= false,
					contourdefs= _contourdefs }

        -- Evaluate contour levels/values from mode 1 or 3 contour descriptor string (lo..hi using step)
        --
        function cs:_getlevels_mode13( mode, m )
            --
            -- 1|3 step zero_level(not used) lo hi
            --
            -- For mode 3 (fill) step is used only for finding the upper/lower limits (if lo/hi==32700).
            -- Within the lo..hi range step makes no sense.
            --
            local steps,los,his= self.contourdefs:match("^%d%s+(+?%d+%.?%d*)%s+[+-]?%d+%.?%d*%s+([+-]?%d+%.?%d*)%s+([+-]?%d+%.?%d*)")

            if (steps and los and his) then
                -- Use min/max values if requested
                --
				-- Note: Min/max value indicator (32700) is passed on as is when filling and step is zero (range values not rounded to step).
				--		 Step must be nonzero when contouring, and min/max value indicator is replaced by rounded data min/max value
				--
                local step=tonumber(steps) lo=tonumber(los) hi=tonumber(his)

                if (((lo==32700) or (hi==32700)) and step>0.0) then
                    local lom,him= _findlimits( m, step, lo, hi )

                    if (lo==32700) then
                        lo= lom
                        self.contourdefs= self.contourdefs:gsub("^(%d%s+[+]?%d+%.?%d*%s+[+-]?%d+%.?%d*%s+)(32700)%s","%1"..lo.." ")
                    end

                    if (hi==32700) then
                        hi= him
                        self.contourdefs= self.contourdefs:gsub("^(%d%s+[+]?%d+%.?%d*%s+[+-]?%d+%.?%d*%s+[+-]?%d+%.?%d*%s+)(32700)(%s*)","%1"..hi.."%3")
                    end
                end

                if (mode=="1") then
                    if ((lo<=hi) and (step>0)) then
                        local nmax=cc:_maxlevels()

                        if (((hi-lo)/step) > nmax) then
                            step= (hi-lo)/nmax
                        end

                        repeat
                            self.levels[#(self.levels)+1]= lo
                            lo= lo+step
                        until (lo>hi)
                    end
                elseif (lo<hi or lo==32700) then
                    self.levels= { lo,hi }
                end
            end

            -- Return the number of contour level/value definition parameters; used to determine
            -- smooth_factor position
            --
            return ((#(self.levels)>0) and 5) or 0
        end
 
        -- Evaluate contour levels/values from mode 2 or 4 contour descriptor string (list of levels/ranges)
        --
        function cs:_getlevels_mode24( mode, m )
            --
            -- 2 Nlevels level1 level2 .. levelN
            -- 4 Nranges lo1 hi1 lo2 hi2 .. loN hiN
            --
            local n= self.contourdefs:match("^%d%s+(%d+)%s")
            n= ((n and ((((mode=="4") and 2) or 1) * tonumber(n))) or 0)

            if (not (n>0)) then
	        	return 0
            end

			local leveldef= "([+-]?%d+%.?%d*)"
            self.levels= _pack( mode=="4", self.contourdefs:match("^%d%s+%d+%s+"..string.rep(leveldef.."%s+",n-1)..leveldef) )

            -- Return the number of contour level/value definition parameters; used to determine
            -- smooth_factor position
            --
            -- Return -1 to indicate error (lorng >= hirng) in fill range
            --
            return ((#(self.levels)>0) and (n+2)) or -1
        end

        -- Load smoothing length/degree and labelize t/f from contour descriptor string
        --
        -- 'pos' is the position of smoothing factor in the contour descriptor
        --
        -- smooth_factor: length[.degree], 0[.0] = no smoothing, 0.n = q3 (or q2) smoothing
        --
        -- 11-Jan-2012 PKi: Check if label color is 'none'; no labels drawn
        --
        -- 12-Mar-2015 PKi: Smoothing factor and labeling fields (starting with strategy) are
		--					now allowed to be missing; no smoothing/labeling for the contour.
		--
		--					Returns label font size position (or nil if n/a) in the contour descriptor
        --
        function cs:_getsmoothing(pos)
            local length,degree,strategy,labelfontheightpos= self.contourdefs:match("%s+(%d*)%.?(%d*)%s*(%d*)()",pos)

            if (length == nil) then length= "" end
            if (degree == nil) then degree= "" end

            if ((length == "") and (degree == "")) then
				-- No smoothing nor labeling for this contour
				--
                return nil
            end

            if (length ~= "") then
                self.smoothing.length= tonumber(length)
            end

            if (degree ~= "") then
                degree= tonumber(degree)

                if (degree>0) then
                    self.smoothing.degree= degree
                end
			else
				if (self.smoothing.length>0) then
					self.smoothing.degree= 2
				end
            end

            -- No labels if labeling fields are missing or color is 'none'
            --
			self.labelize= ((strategy ~= "") and (string.find(self.contourdefs,"%s+%d+%s+none",labelfontheightpos) == nil)) or false

            return labelfontheightpos
        end

        function cs:_setlabelingdefaults(pos)
			if (pos == nil) then
                error("Internal error: could not determine contour's label font size position; "..self.contourdefs)
			end

            local colordef= "%s+%w+%(?[%d,]*%)?%[?%d*,?%d*%]?"
			local fldpat= { "%s+%d+", self.label_font_height,
							colordef, self.label_text_color, colordef, self.label_box_fill_color, colordef, self.label_box_stroke_color,
							"%s+%d+", self.label_box_stroke_width }

			for pat=1,#fldpat,2 do
				if (pos ~= nil) then
					pos= self.contourdefs:match(fldpat[pat].."()",pos)
				end

				if (pos == nil) then
					self.contourdefs= self.contourdefs.." "..fldpat[pat+1]
				end
			end
		end

		-- Check for missing drawing properties (line and fill color, contour line width) and set their default values.
		-- Get smoothing length/degree and labeling t/f state.
		-- If labeling, check for missing labeling properties (label font size, font and box color, box line width) and set their default values
        --
        -- 'nlp' is number of descriptor string's contour level/value definition parameters
        --
        function cs:_setdrawingdefaults(nlp)
            -- Skip nlp fields.
            -- Colors are defined by name[\[dash,dash\]] or by rgb[a](r,g,b[,a])[\[dash,dash\]]
            --
			local leveldef= "[+-]?%d+%.?%d*"
            local pattern= "^"..string.rep(leveldef.."%s+",nlp-1)..leveldef.."()"
            local pos= self.contourdefs:match(pattern)

			if (pos == nil) then
                error("Internal error: could not determine contour's stroke color position; "..self.contourdefs)
			end

            local colordef= "%s+%w+%(?[%d,]*%)?%[?%d*,?%d*%]?"
			local fldpat= { colordef, self.stroke_color, colordef, self.fill_color, "%s+%d*%.?%d*", self.stroke_width }

			for pat=1,#fldpat,2 do
				if (pos ~= nil) then
					pos= self.contourdefs:match(fldpat[pat].."()",pos)
				end

				if (pos == nil) then
					self.contourdefs= self.contourdefs.." "..fldpat[pat+1]
				end
			end

			-- Get smoothing factor and labeling t/f state
			--
			if (pos) then
				pos= self:_getsmoothing(pos)
			end

			-- Check for missing labeling properties. Note: 'pos' (and 'labelize') was set by _getsmoothing() if 'labelize' is true
			--
			if (self.labelize) then
				self:_setlabelingdefaults(pos)
			end
		end

        -- Load contour levels/values, check/set missing drawing properties and get smoothing length/degree and labelize t/f from contour descriptor string
        --
        function cs:_load( m )
            local mode= self.contourdefs:match("^(%d)%s")
            local nlp=0

            if ((mode == "1") or (mode == "3")) then
                nlp= self:_getlevels_mode13( mode, m )
            elseif ((mode == "2") or (mode == "4")) then
                nlp= self:_getlevels_mode24( mode, m )
            end

            if (#(self.levels)<1) then
                local e= (((nlp == 0) and "couldn't evaluate levels") or "hirng > lorng expected")
                error("Invalid contour descriptor ("..e.."); "..self.contourdefs)
            end

            self:_setdrawingdefaults(nlp)

            return self.contourdefs,(mode == "3") or (mode == "4"),self.levels,self.smoothing,self.labelize
        end

        return cs:_load(m)
    end

    -- Check if function was called without colon notation (no 'self')
    --
    function cc:_callchk(obj)
        if (not (obj and obj.object)) then
            local info= debug.getinfo( 2, "n" )
            local func= (((info and info.name) or "func").."()")

            error(func.." must be called with colon notation (object:"..func..")")
        end
    end

    -- Store contour paths and value.
    -- Contour descriptor string is stored for set's 1'st contour only (when not nil) 
    --
    function cc:contour_path_for_stroke( cr, contourdefs, labelize, val, m, sm, th, ... )
        cc:_callchk(self)
        proto( "CairoContext, [ string ], bool, number, Matrix, Matrix, [ TronHints ], [ Contour ], ...", cr, contourdefs, labelize, val, m, sm, th, ... )

        -- 29-May-2012 PKi: Store the source and smoothened (if smoothening was requested) matrixes for upcoming calls
        --
        -- 05-Jun-2012 PKi: Store the tron hints too
        --
        self.matrix= m
        self.smatrix= sm
        self.tronhints= th

        local c= {}
        local n= select('#',...)

        for i=1,n do
            c[#c+1]= select(i,...)
        end

        if (n>0) then
            local idx= #(self.contours)+1

            self.contours[idx]= c

            -- 11-Jan-2012 PKi: Set value to nan if labels not drawn (label color is 'none')
            --
            self.contourvalues[idx]= (labelize and val) or (0/0)

            if (contourdefs) then
                self.contourdefs[idx]= contourdefs
            end
        end
    end

    -- q3/q2 contour smoothening (alternative to tron smoothening)
    --
	local function q3q2smoothening(q3smoothening,smooth_factor,...)
		ct= {}
		ct[#ct+1]= select(1,...)
		ct[#ct+1]= select(2,...)

		for i=3,select('#',...) do ct[#ct+1]= contour_smoothen_one(select(i,...),smooth_factor,q3smoothening) end

		return unpack((ct))
	end

    -- Get contour paths for 'val' or lo-hi value range
    --
    function cc:contour( m, val, ... )
        cc:_callchk(self)

        -- gridsize can't change
        --
        if (self.gridsize == nil) then
            self.gridsize = m.size
        elseif ((self.gridsize.x~=m.size.x) or (self.gridsize.y~=m.size.y)) then
            --
            -- Would just be ignored by cc:contour_path_for_stroke
            --
            -- return val,m,nil

            error("gridsize ("..self.gridsize.x..","..self.gridsize.y..") can't change")
        end

        -- If called directly from user script no smoothing (length=0) by default.
        -- Default degree is 2
        --
        -- 05-Jun-2012 PKi: New bool argument controlling the usage of tron hints
        --
        local smooth_length=0
        local smooth_degree=0
        local usehints=false
        local n= select('#',...)

        if (n>=1) then
            smooth_length= select(1,...)
            smooth_degree= (((n>=2) and select(2,...)) or ((smooth_length>0) and 2 or 0))
            usehints= (((n>=3) and select(3,...)) or usehints)
        end

        -- 29-May-2012 PKi: Pass the already smoothened matrix too if the source matrix and
        --                  smoothening factor has not been changed
        --
        local sm= (
                   ((self.matrix==m) and
                    (smooth_length==self.smoothing.length) and
                    (smooth_degree==self.smoothing.degree) and
                    self.smatrix
                   ) or
                   nil
                  )

        self.smoothing.length=smooth_length
        self.smoothing.degree=smooth_degree

        -- 05-Jun-2012 PKi: Pass tron hints too if they are to be used and the source matrix
        --                  has not been changed
        --
		-- 10-Jan-2014 PKi: Pass the tron hints only if the data will not be resmoothed (when the
		-- 					smoothed matrix is passed to contourer).
		--
		-- 					Earlier the old hints (instead of nil) were passed when resmoothing,
		--					and things got messed up. This is catched now on the c++ side too.
        --
		-- 03-Mar-2015 PKi: Use q3 (instead of tron) smoothening when zero length and nonzero degree
		--					(could use q2 smoothening instead by setting q3smoothening to false)
        --
		-- 16-Feb-2016 PKi: Use q2 smoothening
        --

        local tharg= (usehints) and
            function() return ((sm and self.tronhints) or nil) end
        or
            function() return end

		local lo_val,hi_val,ret_val

		if type(val)=="number" then
			-- Single value
			--
			lo_val= val
			ret_val= val
		elseif type(val)=="table" then
			-- Value range
			--
			lo_val= val.lo
			hi_val= val.hi
			ret_val= lo_val or hi_val
		end

		if (lo_val~=nil and type(lo_val)~="number") or (hi_val~=nil and type(hi_val)~="number") then
            error("numeric contour value or value range (.lo and .hi) expected")
		end

		if smooth_length>0 or smooth_degree>0 then
			return ret_val,m,q3q2smoothening(false,smooth_length+(smooth_degree/10),contour( m, lo_val, hi_val or 0/0, 0, 0, sm, tharg() ))
		else
			return ret_val,m,contour( m, lo_val, hi_val or 0/0, 0, 0, sm, tharg() )
		end
    end

    -- Build contour descriptor string from contour configuration table
    --
    function cc:_contourdescr(m)
        --
        -- 1					step 					zero_level(not used) 	lo 				hi
        -- 2					Nlevels					level1			     	..levelN
        -- 3					step 					zero_level(not used) 	lo 				hi
        -- 4					Nranges					lo1 					hi1	     		..loN	hiN
        -- stroke_color 	    fill_color 				stroke_width 			smooth_factor
        -- label_strategy 	    label_font_height		label_text_color
        -- label_box_fill_color label_box_stroke_color	label_box_stroke_width
        --
        -- config(ContourRange), config(ContourList) or config(ContourFillRange) must have been called
        --
        local mode= self.configflds["mode"]
        if (not mode) then
            error("config(): contouring mode (ContourRange, ContourList or ContourFillRange) not set")
        end

        -- 12-Mar-2012 PKi: If not smoothening, adjust fill hi range with data max value.
        --                  This is needed for fill to get the hi range contour if it's value
        --                  would exceed the max data value.
        --
        --                  The same check is made in c++ side after smoothing the data.
        --
        -- 10-Jan-2014 PKi: The check is made in c++ side only (whether smoothing or not)

        local smoothfactor= (self.configflds["smoothfactor"] or 5.2)
        local length,_= modf(smoothfactor)
        local hi= self.configflds["hi"]

        local zerolevel= ((((mode == 1) or (mode == 3)) and "0 ") or "")
        local levels= (
                       ((mode == 2) and table.concat(self.configflds["levels"]," ")) or
                       (self.configflds["lo"].." "..hi)
                      )

        local cdefs= self.configflds["mode"].." "..
                     self.configflds["step"].." "..
                     zerolevel..
                     levels.." "..
                     (self.configflds["strokecolor"] or "def").." "..
                     (self.configflds["fillcolor"] or "none").." "..
                     (self.configflds["strokewidth"] or 2).." "..
                     (self.configflds["smoothfactor"] or 5.2).." "..
                     (self.configflds["labelstrategy"] or 2).." "..
                     (self.configflds["fontheight"] or 20).." "..
                     (self.configflds["labelcolor"] or "def").." "..
                     (self.configflds["labelboxfillcolor"] or "none").." "..
                     (self.configflds["labelboxlinecolor"] or "none").." "..
                     (self.configflds["labelboxlinewidth"] or 1)

        return cdefs
    end

    -- Collect contour paths using Q2 style contour descriptor string given as parameter
    -- or constructed from preconfigured parameter values
    -- 
    function cc:contourpaths( cr, m, _contourdefs )
        cc:_callchk(self)
        proto( "CairoContext, Matrix, [string]", cr, m, _contourdefs )

        local fill,levels,smoothing,labelize
        _contourdefs,fill,levels,smoothing,labelize= _contoursettings( m, (_contourdefs or cc:_contourdescr(m)))

        -- 05-Jun-2012 PKi: Use tron hints
        --
        --                  TBD ?: make user configurable
        --
        --                  config(TronHints,on[,mincontourcnt=some default]
        --                  config(TronHints,off)

        local usehints= ((#levels>=1) or false)
		local range= fill

        for l=1,#levels do
            --
            -- For min-x (32700-x) and x-max (x-32700) fills pass range to contourer
            --
			local n=l
			local val

			if range and l<#levels then
				if levels[l]==32700 or levels[l+1]==32700 then
					n= l+1
					val= { lo=levels[l], hi=levels[n] }
				else
					range= false
				end
			else
				range= fill
			end

            --
            -- Note: Contour descriptor strings are stored for set's 1'st contour only
            --
            cc:contour_path_for_stroke( cr, ((l==1) and _contourdefs) or nil, labelize, cc:contour(m, val or levels[l], smoothing.length, smoothing.degree, usehints ) )

			if n==#levels then break end
        end
    end

    -- Set Labelizer configuration value
    --
    function cc:_labelcfg( param, v )
        if (not self.labelizercfg) then
            self.labelizercfg= LabelizerConfig()
        end

        local p= string.lower(param)
        local t= type(self.labelizercfg[p])

        if (t == type(v)) then
            self.labelizercfg[p]= v
        elseif (t) then
            _configErr( param, t.." expected" )
        else
            _configErr( param, "unknown parameter" )
        end
    end

    -- Set contouring/labeling configuration value. Labeling setting must have exactly
    -- one value, contouring setting can have multiple values depending on setting
    --
    function cc:config( param, ... )
        cc:_callchk(self)
        proto( "string,...", param )

        local n= select('#',...)

        if (self.contourConfig[param]) then
            self.contourConfig[param]( self, param, n, ... )
        elseif (self.labelConfig[param]) then
            if (n ~= 1) then
                _configErr( param, "one parameter value expected" )
            end

            cc:_labelcfg( param, select(1,...) )
        else
            _configErr( param, "invalid parameter" )
        end
    end

    -- Draw and (by default) labelize collected contours
    --
    function cc:stroke( cr, labelize )
        cc:_callchk(self)
        proto( "CairoContext, [ bool ]", cr, labelize )

        labelize= (((labelize == nil) and true) or labelize)

        if (#(self.contours)>0)
        then
            local labels= ((labelize and self.contourvalues) or {})
            drawcontours( cr, self.gridsize, self.labelizercfg or {}, self.contourdefs, labels, self.contours )
        end

        -- Reset collector (clear contours etc) after stroke
        --
        self:_reset()
    end

    return cc:_new()
end


--
-- cr= contour_path_for_fill( cr, [contour_ud [, ...]] )
--
-- Add the contour paths to the 'cr' Cairo context (all parts, also those at edge).
--
-- Note: The 'cr' transforms must be scaled with 'scale_to_grid()' prior to this call.
--
function contour_path_for_fill( cr, ... )
    proto( "CairoContext, [Contour], ...", cr, ... )

    for i=1,select('#',...) do
        local contour= select(i,...)
            --
            -- { {x=num, y=num, edge=bool} , ... }   (userdata with such fields)

        local c1= contour[1]
        cr.move_to( c1.x, c1.y )
          
        for k= 2,#contour do
            local ck= contour[k]
            cr.line_to( ck.x, ck.y )    -- draw all segments
        end

        cr.close_path()
    end

    return cr   -- chain further
end


--
-- contour_ud [, ...]= contour_smoothen( [smoothen_num], contour_ud [, ...] )
--
-- Create new, smoothened contours based on the 1..N given. Returns the same number of
-- contours, which are independent of the provided ones.
--
-- 'smoothen':  optional smoothening factor, 0.0..1.0 (0.3 gives pretty good results).
--
-- Note: This is just a wrapper to handle the multiple parameters case. Actual smoothening
--      is on the C++ side, but could just as well be here in Lua. TBD, later?
--
function contour_smoothen( ... ) 
    local smooth, skip

    if type.number(select(1,...)) then
        proto( "number,Contour,...", ... )
        smooth= select(1,...)
        skip= 1
    else
        proto( "Contour,...", ... )
        smooth= 0.3
        skip= 0
    end
    
    local ret= {}
    for i= skip+1,select('#',...) do
        ret[#ret+1]= contour_smoothen_one( select(i,...), smooth )
    end
    
    return unpack(ret)
end


---=== Labeling ===---

--
-- void= plot_labels( cr, contour_ud, label_str|num )
--
--[==[
local function plot_labels( cr, contour, label )

    label= tostring(label) or error "No label"

    -- contour: { {x=num, y=num, edge=bool} , ... }     (userdata with such fields)
    --
    for i=1,#contour do
        local p= contour[i]
        --[[
        cr .move_to( p.x, p.y )
	       .show_text( label )
	   ]]
    end
end
]==]


--
-- { index_uint, ... }= places_for_label( part_tbl )
--
-- part:    { p1, ..., closed=bool, bb={x=num,y=num,w=num,h=num}, slants_deg={ num, ... } } )
--
-- Return the indices of 'part' in the order of their preference as label position, when only the part
-- itself is being considered. The returned indices will then be tried in turn, and some of them may
-- cause labels to actually arise.
--
local function places_for_label( part )

    error "TBD"
    
    -- TBD: no consideration, yet
    
    local ret= {}
    for i=2,#part-1 do
        ret[#ret+1]= i
    end

    -- Note: Handle closed loops 
    -- Note: Don't allow ends of an unclosed part to be even mentioned
    
    return ret
end


--
-- void= remove_candidates( part_tbl, central_index_uint, dist_along_line_num, places_tbl )
--
-- Remove other indices to 'part' from 'places' if they are _along_the_line_ too close
-- to 'central_index' that just got a label.
--
local function remove_candidates( part, central_index, dist_along_line, places )

    error "TBD"
end


--
-- [{ p1, ..., closed=bool }, ...] = contour_parts( contour_ud [, start_index_uint] )
--
-- Extract the remaining individual parts out of 'contour', starting from 'start_index'
-- (default: 1).
--
local function contour_parts( contour, i )

    -- 'contour[i]' is the beginning of a section
    
    if not i then
        -- Outside call (not recursion); we have a fresh contour which might be closed (all in 
        -- one piece).
        --
        if not contour[#contour].edge then  -- if the last point is not an edge, we know it's a closed contour
            local part= { closed=true }
            for i=1,#contour do
                part[i]= contour[i]
            end
            return part
        end
        i= 1    -- ok, go the other way
    end

    -- Collect points until two subsequent points are at edge
    --
    local part= { closed=false }
    local p= contour[i]
    local in_gap= false

    while true do
        local p_next= contour[i+1]
        if not p_next then
            i= nil  -- don't dig deeper
            break
        end

        if in_gap then
            if not p_next.edge then
                break   -- 'i' is the beginning of the next section (still at edge, but 'p_next' isn't)
            end
        elseif p.edge and p_next.edge then
            -- at an edge. We have added 'p' already so simply pass points until the next section starts
            -- (if at all)
            --
            in_gap= true
        else
            part[ #part+1 ]= p
        end
        p= p_next
        i= i+1
    end

    -- 'part' has the last section (or '#part'==0 for nothing if an all-edge contour)
    --
    if #part==0 then    -- Must be an all-edge contour
        assert( i==nil )
        return  --nil
    end

    -- 'i' is the beginning of the next section or 'nil' for nothing more
    --
    -- 'part' has the collection of this section
    --
    if i then
        return part, contour_parts( contour, i )    -- tail recursion
    else
        return part
    end
end


--
-- bool= plot_label( cr, {x=num,y=num}, label_str, slant_deg_num, all_contours )
--
-- all_contours:    ...necessary info to guard against overlapping other contours than 'label'...
--
-- Returns:     'true' if the label was succesfully plotted.
--              'false' if there was something preventing it (overlapping other contours, mainly)
--
local function plot_label( cr, p, label, slant_deg, all_contours )

    error "TBD"
end


--
-- x_num, y_num, width_num, height_num= bounding_box( { p1, ... } )
--
-- Provides the bounding dimensions of the part.
--
local function bounding_box( part )

    local p= part[1]
    
    local x_min, y_min= p.x, p.y
    local x_max, y_max= p.x, p.y

    for i=2,#part do
        p= part[i]
        x_min= math_min(x_min,p.x)
        y_min= math_min(y_min,p.y)
        x_max= math_max(x_max,p.x)
        y_max= math_max(y_max,p.y)
    end
    
    return x_min,y_min, (x_max-x_min), (y_max-y_min)
end


--
-- void= contour_labels( cr, { { label=[str|num], [contour_ud, ...] } [, ...] }, opt )
--
-- opt: .dist_along_line    Minimum distance between labels (by their centers) along the same contour
--      .dist_direct        Minimum distance between two labels NOT on the same contour
--
-- Given one or more sets of contours, draws labels on those contours with a 'label' field given.
-- Current Cairo font size and other properties are being used.
-- 
-- Labels are not allowed to overlap other contours. If this check is not required by the application,
-- call this function individually for each contour.
--
-- Giving a contour set without a 'label' field exercises such contours as a limit to the labels
-- of other contours but does not create any label for such a set itself.
--     
function contour_labels( cr, t, opt ) 
    proto( "CairoContext,"..
           "{ { label=[string|number], [Contour], ... }, ... },"..
           "{ dist_along_line= [number],"..
             "dist_direct= [number],"..
           "}", cr, t, opt )

    local dist_along_line=  opt.dist_along_line
    local dist_direct=      opt.dist_direct

    local slant_limit_deg=  5.0 -- limit of what is still considered horizontal
    local slant= true           -- slant the labels (if not nearly horizontal)
                                -- 'false': no slanting, all labels are plotted horizontal

    local all_contours= {}      -- { [label_str|""]= { contour_ud, ... } }
                                -- 
                                -- Contours to be used as constraint for placing labels (labels shall not overlap the contours,
                                -- except their own)
    
    local label_parts= {}       -- { { {x=num,y=num}, ..., closed=bool, label=str, bb={x=num,y=num,w=num,h=num}, n_wish=num }, ... }
                                --
                                -- Contours to be labelled, sorted with ascending dimension (smallest contours first)
                                
    for _,contour_set in ipairs(t) do
        local label= tostring( contour_set.label )  -- make numeric labels string, just to keep things simple
        
        for __,contour in ipairs(contour_set) do
            
            local ac_key= label or ""   -- empty string as key to plain constrain contours
            if not all_contours[ac_key] then
                all_contours[ac_key]= { contour }
            else
                local tt= all_contours[ac_key]
                tt[#tt+1]= contour
            end

            if label then
                -- If 'contour' is in multiple pieces, deal with each of them as a separate entity.
                -- This simplifies things later on and allows i.e. the number of labels suggested for
                -- each part to be fair(er).
                --
                for ___,part in ipairs( { contour_parts(contour) } ) do
                    --
                    -- part: { p1, ... , closed=bool }
                    -- p1:   { x=num, y=num }           (actually 'EdgePoint' but we don't use the 'edge' field)

                    local x,y,w,h= bounding_box( part )
                    part.bb= { x=x, y=y, width=w, height=h }    -- will be needed later

                    -- Make the wish how many labels we'd like to get (integer part of these)
                    --
                    part.n_wish= (part.closed and (2*(w+h)) or (w+h)) / 30      -- TBD: tune (one label every N pixels)

                    part.label= label

                    label_parts[ #label_parts+1 ]= part
                end
            end
        end
    end

    table_sort( label_parts, function(a,b) return a.n_wish < b.n_wish end )

    -- Do a secondary Cairo surface for use by 'contour_label_one()' to find places where labels would overlap
    -- other contours.
    --
--[[
    local cs2,cr2= newcairo.surface( cs.width, cs.height )
        --
        -- make a similar one (also just another independent drawing context would do
        -- since we don't plan to actually draw onto 'cs2').
]]

    -- Draw labels for each labelled part, starting from the smallest (because the larger ones have more
    -- freedom to position their labels).
    --
    for _,part in ipairs(label_parts) do
        local label=    part.label
        local n_wish=   part.n_wish
        local bb=       part.bb

        if n_wish >= 1 then
            -- Prepare the 'cs2' and/or 'cr2' to be used for overlapping detection (of a label
            -- of this contour and the other contours).
            --
            --[[
            prepare_cs2( cr2, all_contours, label )
            ]]

            -- Count slants for all the points to be considered (excludes first and last if not a closed contour)
            -- This information is required by 'places_for_label'.
            --
            local slants_deg= calc_slants( part )
            part.slants_deg= slants_deg

            -- Find the best places for a label, considering the part itself only.
            --
            local places= places_for_label( part )
                --
                -- { index_uint, ... }  indices of 'part' points, from best to worst (without the actual ratings)

            -- Let's apply the given indices in order. If one is applied, it may reduce the number of
            -- remaining candidates if they are too near.
                       
            -- Note: avoid 'ipairs(places)' since we'll be removing entries from the array.
            --
            while( places[1] ) do
                local candidate_index= places[1]
                local p= part[candidate_index]

                if plot_label( cr, p, label, slant and slants_deg[candidate_index] or 0, all_contours ) then
                    -- Succeeded - one label actually plotted
                    --
                    n_wish= n_wish-1
                    if n_wish < 1 then
                        break   -- done enough, next contour
                    end
                    
                    table_remove( places, 1 )   -- dealt with that

                    -- Remove other indices that would be too close (along the line, not directly)
                    --
                    if dist_along_line then
                        remove_candidates( part, candidate_index, dist_along_line, places )
                    end
                end
            end
        end
    end
end


