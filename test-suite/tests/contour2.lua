--[[
-- description: Käyrästys
--
-- image:       true
-- projection:  stereographic,20,90,60:6,51.3,49,70.2
-- gridsize:    50,60
-- validtime:   TODAY12
--]]

require 'newcairo'

local NONE= "none"
local RED= "red"
local YELLOW= "yellow"
local WHITE= "white"
local BLACK= "black"
local GREEN= "green"
local BLUE= "rgba(0,0,255,128)"
local BLACKDASH= "rgb(0,0,0)[1,1]"

local x_max,y_max= gridsize.x-1, gridsize.y-1
local scale= 500/x_max

local cs,cr= newcairo.surface( x_max*scale,y_max*scale )

-- Convert so we can draw with matrix coordinates
--
scale_to_grid( cr, gridsize, 0,0, cs.width, cs.height )

local m= HIR.T
local step= 2

-- Get contourcollector
local cc=contourcollector()

--
-- Contouring configuration (defaults set in contour.lua)
--
-- ContourRange
-- ContourList
-- ContourFillRange
-- ContourLabel
--   ContourLabelColor = "def"
--   ContourFontHeight = 20
--   ContourLabelStrategy {ContourLabelsHorizontal,ContourLabelsTilted} = ContourLabelsHorizontal
-- ContourLabelBox
--   ContourLabelBoxFillColor = "none"
--   ContourLabelBoxLineColor = "none"
--   ContourLabelBoxLineWidth = 1
-- ContourSmoothFactor = 5.2
--
-- Labelizer configuration (defaults loaded from q3 configuration file)
--
-- LimitAlongLine = 10
-- LimitDirect = 10
-- AllowOverlappingLines = false
-- AllowOverlappingLabels = false
-- DampenCornersLimitDeg = 8.0
-- DampenCorners = 0.8
-- BoostHorizontalLimitDeg = 3.0
-- BoostHorizontal = 1.5
-- CurveOptimizationFactor = 10
--

-- Tron smoothing factor; length 3, degree 2
cc:config(ContourSmoothFactor,3.2)

--
-- Fills
--
-- Fill from min to -4 with BLUE
-- (32700 is taken as min(m)/max(m) when step/rounding is given)
-- (lo range in rounded by step)
cc:config(ContourFillRange,BLUE,32700,-4,2)
-- Store the fill
cc:contourpaths( cr, m )

-- Fill from -4 to -2 with light blue
cc:config(ContourFillRange,"rgba(0,0,255,50)",-4,-2)
-- Store the fill
cc:contourpaths( cr, m )

-- Fill from 5 to max(m) with light red
-- (32700 is taken as min(m)/max(m) when step/rounding is given)
-- (hi range in rounded by step)
cc:config(ContourFillRange,"rgba(255,0,0,50)",5,32700,2)
-- Store the fill
cc:contourpaths( cr, m )

--
-- Contours
--
-- Min distance to another label
cc:config(LimitDirect,5)
cc:config(LimitAlongLine,5)

-- BLUE contours, linewidth 1, stepping from min(m) to -8 by 2
-- (32700 is taken as min(m)/max(m))
-- (lo range in rounded by step)
--
cc:config(ContourRange,BLUE,1,32700,-8,2)
-- RED labels, height 20, tilted
cc:config(ContourLabel,RED,20,ContourLabelsTilted)
-- Store the contours
cc:contourpaths( cr, m )

-- BLUE contours having linewidth 3 for listed values (-4 and -2)
cc:config(ContourList,BLUE,3,-4,-2)
-- GREEN labels
cc:config(ContourLabelColor,GREEN)
-- YELLOW label boxes surrounded by RED lines with width 1
cc:config(ContourLabelBox,YELLOW,RED,1)
-- Label height 10
cc:config(ContourFontHeight,10)
-- Labels horizontally
cc:config(ContourLabelStrategy,ContourLabelsHorizontal)
-- Store the contours
cc:contourpaths( cr, m )

-- BLACK dashded contour having linewidth 2 for value 0
cc:config(ContourList,BLACKDASH,2,0)
-- No label nor box
cc:config(ContourLabelColor,NONE)
-- Store the contour
cc:contourpaths( cr, m )

-- RED contours, linewidth 1, stepping from 1 to max(m) by 2
-- (32700 is taken as min(m)/max(m))
-- (hi range in rounded by step)
cc:config(ContourRange,RED,1,1,32700,2)
-- WHITE labels
cc:config(ContourLabelColor,WHITE)
-- RED label boxes surrounded by BLACK lines with width 1
cc:config(ContourLabelBoxFillColor,RED)
cc:config(ContourLabelBoxLineColor,BLACK)
cc:config(ContourLabelBoxLineWidth,1)
-- Store the contours
cc:contourpaths( cr, m )

-- BLUE dashed contour having linewidth 4 for value -6 using Q2 style
-- contour descriptor string
cc:contourpaths( cr, m, "2 1 -6 rgba(0,0,255,128)[1,1] none 4 3.2 2 20 red none none 0")

-- Stroke and labelize (true)
cc:stroke(cr,true)

cr .identity_matrix()  -- back to regular coordinates
   .set_source_rgb(0.1,0.1,0.1)
   .select_font_face('Serif', 'normal', 'normal')
	.set_font_size(24)
   .move_to( cs.width*0.1, cs.height-30 )
	.show_text( "HIR.T "..os.date("!%Y-%m-%d %H:%M (UTC)", validtime.epoch) )

return cs
