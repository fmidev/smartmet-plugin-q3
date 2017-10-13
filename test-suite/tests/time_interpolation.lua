--[[
-- description: Aika-interpoloinnin arvojen tulee olla vierettäisten tarkkojen arvojen sisällä
--
-- projection:  stereographic,20,90,60:6,51.3,49,70.2
-- gridsize:    5,6
-- decimals:    3
-- validtime:   TODAY12
--]]

local r,err= HIR()
assert(r,err)

local t0= r.times[1]
local t1= r.times[2] or error( "No two times in the data" )

-- WD ei sovi tähän, koska sen arvoalue on epäjatkuva
--
local param= "WS"

local m0= r { time=t0 }[param]
local m1= r { time=t1 }[param]

local lo= min( m0, m1 )
local hi= max( m0, m1 )

local dt= t1-t0     -- difference in hours

local arr= {}
local ms= {}

for h=0,dt,dt/10 do     -- step in hours
  m= r { time=t0+h }[param]
  ms[#ms+1]= m

  for pos,v in points(m) do
    if (v< lo[pos]) and (not isnan(v)) then
      arr[#arr+1]= { h, v, lo[pos] }
    end
    if (v> hi[pos]) and (not isnan(v)) then
      arr[#arr+1]= { h, v, hi[pos] }
    end
  end
end

if #arr==0 then
    return "ok"
else
    return arr
end

--[[ok:
ok
]]
