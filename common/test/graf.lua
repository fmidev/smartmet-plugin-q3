--
-- GRAF.LUA
--
-- Grafiikkaa SQD-tiedostojen pohjalta
--

require "metqu"
require "svgcore"

local r,err= raw( "data/*_ecmwf_pinta.sqd", { params=T } )
assert(r,err)

--LOG( concat(r.params, " ") )

local m= r.grid{ gridsize=xy(10,10) }.T

local doc= svg.document{
    size={ 400,600 },
    title= "Matrix SVG example",
    ["xmlns:xlink"]= "http://www.w3.org/1999/xlink",
}

local function func( xy, wh, val )
    if isnan(val) then return end

    local x,y= unpack(xy)
    local w,h= unpack(wh)
    local cx,cy= x+w/2, y+h/2
    local r= min( w/2, h/2 )
    local color= (val>0) and "red" or "blue"
    local _end= (180/20)*val
    --
    return { svg.circle{ {cx,cy}, (abs(val)/20)*r, fill= (val>0) and "red" or "blue" },
             --svg.circle{ {cx,cy}, 1, fill="black" },
             svg.text{ {cx-r/2,cy-r/6}, fill="blue", string.format("%.1f",val) }
           }
end

local vt_str= time2str(validtime)

doc[#doc+1]= svg.matrix{ {20,20}, {360,540}, func, m }
doc[#doc+1]= svg.text{ {50,570}, fill="blue", "T: "..vt_str }

-- Standard ':write' method has been modified to be able to print objects
-- s.a. 'doc' (having 'output' metatable).
--
local f, err= io.open( "out.svg", "w" )
assert( f, err )

f:write( doc )
