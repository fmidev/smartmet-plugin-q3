--
-- PETRIVUORIO.LUA
--
-- Luetaan Petrin tekemää raakadataa tiedostosta ja luodaan siitä SQD.
--
-- Käyttö:
--      lua -lstrict petrivainio.lua input.dat output.sqd
--
local input_fn= select(1,...) 
local output_fn= select(2,...)

if not (input_fn and output_fn) then
    error( "Käyttö: input_dat output_sqd" )
end

require "metqu"

--
-- Syötteen formaatti:
--[[
origintime: 
20050101000000
times: 
19750115000000 19750215000000 19750315000000 [..snap..] 20851015000000 20851115000000 20851215000000 
params:
433 434
gridsize:
52 45
projection:
WGS84 18.875 59.375 0.25 0.25
data:
-0.4 -0.4 -0.4 -0.4 -0.4 -0.4 -0.4 -0.4 -0.4 -0.4 -0.4 -0.4 [...]   -- 52*45 values (whole matrix)
-1.5 -1.6 -1.6 -1.6 -1.6 -1.7 -1.7 -1.8 -1.8 -1.9 -1.9 -1.9 [...]
]]

local f,err= io.open( input_fn )
assert(f,err)

-- 
-- Luetaan kriittiset mitat sisään (datan alkuun saakka)
--
local ot        -- jday
local times= {}
local params= {}
local xs, ys
local levels= { "ground" }
local projection

for n=1,11 do   -- rivinro
    local line= f:read()    -- read a line, newline removed
    assert( line, "Rikkinäinen faili!" )

    if n%2==1 then
        assert( line:match("^%w+:%s*$"), "Outo rivi: "..line )  -- ollaanpas sitä tarkkoja
    else
        if n==2 then    -- origintime
            ot= jday(line) or error( "Huono origintime: "..line )
        elseif n==4 then    -- times
            for v in line:gmatch( "%d+" ) do
                times[#times+1]= jday(v) or error( "Huono time: "..v )
            end
        elseif n==6 then    -- params
            for v in line:gmatch( "%d+" ) do
                params[#params+1]= ":"..v   -- i.e. ":433"
            end
        elseif n==8 then    -- gridsize
            xs, ys= line:match( "^(%d+) (%d+)$" )
            assert( xs, "Outo gridsize: "..line )
            xs= tonumber(xs); ys= tonumber(ys)  -- ovat muuten merkkijonoja

        elseif n==10 then   -- projection
            -- TBD: projektion sovittaminen Newbasen käyttämään formaattiin (TARKISTA!!!)
            --
            local lon,lat, lon_step,lat_step= line:match( "^WGS84 ([%d.]+) ([%d.]+) ([%d.]+) ([%d.]+)$" )  -- spacen kohdalla voisi käyttää '%s+' tai ' +'
            assert( lon, "Outo projektio: "..line )
 
            projection= "stereographic,20,90,60:"..lon..","..lat..","..(lon+(xs-1)*lon_step)..","..(lat+(ys-1)*lat_step)
                --
                -- i.e. "stereographic,20,90,60:6,51.3,49,70.2"
        end
    end
end

--
-- Luodaan SQD-data annetuilla mitoilla
--
local r,err= raw( { producer= "X:1",   -- TBD: mitä tuottajanumeroa käytetään
                    origintime= ot, 
                    times= times,
                    levels= levels,
                    projection= projection,
                    gridsize= xy(xs,ys),
                    params= params,
                } )
assert(r,err)


--
-- Luetaan data SQD:hen
--
local param_i= 0
local level_i= 0
local time_i= 0

local n=0   -- lines read of the data part
local g     -- grid-objekti (vaihtuu levelin tai ajan vaihtuessa)

local n_check= #times * #levels * #params   -- expected number of data lines

print ""
while true do
    local line= f:read()
    if not line then
        break
    end

    -- Show progress
    --
    if (n%17 == 0) then
        io.stdout:write( string.format("\rLine %d/%d (%.1f%%)", n+1,n_check, ((n+1)/n_check)*100 ) )
        io.stdout:flush()
    end

    if n % (#levels * #params) == 0 then
        time_i= time_i+1
        level_i= 1
        param_i= 1
        g= nil
    elseif n % (#params) == 0 then
        level_i= level_i+1
        param_i= 1
        g= nil
    else
        param_i= param_i+1
    end

    assert( times[time_i] )
    assert( levels[level_i] )
    assert( params[param_i] )    

    if not g then
        g= r.grid{ time=times[time_i], level=levels[level_i] }
    end
    local m= g[ params[param_i] ]        

    -- Each line has all the values of a whole matrix (growing S->N, W->E)
    --
    local i=0
    for v in line:gmatch( "%S+" ) do    
        v= tonumber(v) or error( "Non-numeric data on line "..n..": "..v )

        local x,y= floor(i/ys), i%ys
        assert( x>=0 and x<xs )
        assert( y>=0 and y<ys, "Internal error: "..x.." "..y.." "..i )

        m[ xy(x,y) ]= v
        i= i+1
    end
    assert( i==xs*ys, "Bad data on line "..n.." ("..i.." != "..xs*ys.."): "..line )
    
    n= n+1
end
print ""

assert( n == n_check , "Väärä määrä dataa: "..n.." != "..n_check )

r.write( output_fn )

print "done :)"

