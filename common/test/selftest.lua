--
-- SELFTEST.LUA
--
-- Tests various aspects of Metqu standalone (command line) tool.
--
local DATA_PATH= os.getenv("DATA_PATH") or ""

require "metqu"

local PROJECTION="stereographic,20,90,60:6,51.3,49,70.2"

--
-- Print all features available in the global namespace
--
--[[
for k,v in pairs(_G) do
    print( k,v )
end
--]]

local SIZE= xy(50,60)   -- xy(10,20)
local SIZE_N= SIZE.x * SIZE.y

--
-- Accessing data
--
local r,err= raw( DATA_PATH.."/*_hirlam_skandinavia_pinta.sqd", { params=T } )
assert(r,err)   -- show error if data wasn't found

assert( type.Raw(r) )   -- latest raw data

local r2= raw( DATA_PATH.."/*_hirlam_skandinavia_pinta.sqd", { origintime="20110101010000" --[["TODAY0"]] } )
assert(r2==nil)  -- nothing today


-- 
--[[ Matrix calculations ]]
--

local m= matrix(SIZE)
assert( type.Matrix(m) )
assert( type.MatrixPos( m.size ) )
assert( m.size == SIZE )

-- Check that we get XS*YS loops and all is NAN
--
local count=0
for p,v in points(m) do
    --print(p,v,p.x,p.y)
    count= count+1
    assert( isnan(v) )
end
assert( count == SIZE_N )

-- Fill 'm' with raising numbers
--
count=0
for p,_ in points(m) do
    m[p]= count
    assert( m[p]==count )
    count= count+1
end
assert( count == SIZE_N )

count=0
for p,v in points(m) do
    assert( v==count )
    count= count+1
end
assert( count == SIZE_N )


---=== Helpers ===---

local function print_m(m)
    for p,v in points(m) do
        print( "["..(p.x).." "..(p.y).."]", v )
    end
end

local function assert_m( m, m2, func )
    for p,v in points(m2) do
        local v_check= func(m[p])
        local mag= abs(v-v_check) / abs(v)      -- magnitude of error
        
        --print( mag )
        if mag > 1e-6 then
            print( p.x, p.y, v, v_check )
            assert(false)
        end
    end
end



---=== Matrix operations ===---

--
-- Matrix add
--
do
    assert_m( m, m+m, function(a) return a+a end )
    assert_m( m, m+3, function(a) return a+3 end )
    assert_m( m, 4+m, function(a) return a+4 end )
end

--
-- Matrix sub
--
do
    assert_m( m, m-m, function(a) return 0 end )
    assert_m( m, m-3, function(a) return a-3 end )
    assert_m( m, 4-m, function(a) return 4-a end )
end


--
-- Matrix multiply
--
do
    assert_m( m, m*m, function(a) return a*a end )
    assert_m( m, m*3, function(a) return a*3 end )
    assert_m( m, 4*m, function(a) return 4*a end )
end

--
-- Matrix divide
--
do
    assert_m( m, m/m, function(a) return 1 end )
    assert_m( m, m/3, function(a) return a/3 end )
end

--
-- Matrix sqrt, unm, abs
--
do
    assert_m( m, sqrt(m), function(a) return sqrt(a) end )
    assert_m( m, -m, function(a) return -a end )
    assert_m( m, abs(m), function(a) return abs(a) end )
end

--
-- Matrix min/max
--
do
    local a= min(m)
    print(a)
    assert( a==0 )
    local b= max(m)
    print(b)
    assert( b==2999 )
end



--
-- Performance & multiple operations test
--
--[[
do
    local t0= os.clock()
    local mm
    for d=1,1e5 do
        --mm= 2*m
        --mm= m*m + sqrt(m)
        mm= abs(m)
    end
print( os.clock() - t0 )
    --assert_m( m, mm, function(a) return a*a + sqrt(a) end )
    assert_m( m, mm, function(a) return abs(a) end )
end
]]



--
-- Raw SQD access
--
do
    print( "Origintime", r.origintime )
    print( "Source", r.source )
    print( "Size", r.gridsize.x, r.gridsize.y )
    
    for i,v in ipairs(r.levels) do
        print( "Level "..i, v )
    end
    
    for i,v in ipairs(r.times) do
        print( "Time "..i, v )
    end
    
    for i,v in ipairs(r.params) do
        print( "Param "..i, v )
    end
end


--
-- Matrix opening
--
--[==[ DISABLED (API CHANGES) AKa 29-Dec-2009
do
    local vt= r.times[1]    -- at least one valid time should exist
    assert(vt)

    -- Native gridsize
    --
    for loop=1,2 do
        local m_native
        if loop==1 then
            -- Using globals
            --
            validtime= vt
            projection= nil
            gridsize= nil
            m_native= HIR.WS  -- native size matrix (373,288)
        else
            -- Using functional interface
            --
            m_native= HIR{ param=WS, validtime=vt }.grid{}.WS
        end

        assert( m_native )
        assert( m_native.size == r.size )

        for p,v in points(m_native) do
            --print( p.x, p.y, v )
        end
    end

    -- Interpolated gridsize
    --
    for loop=1,2 do
        local m_50_60
        if loop==1 then
            -- Using globals
            --
            validtime= vt
            projection= PROJECTION
            gridsize= xy(50,60)
            m_50_60= HIR.WS  -- interpolated matrix
        else
            -- Using functional interface
            --
            m_50_60= HIR{ param=WS, validtime=vt }.
                        grid{ gridsize=xy(50,60), projection=PROJECTION }.WS
        end
    
        assert( m_50_60 )
        print( m_50_60.size.x, m_50_60.size.y )
        assert( m_50_60.size == xy(50,60) )

--[[
        for p,v in points(m_50_60) do
            print( p.x, p.y, v )
        end
--]]
    end
end
--]==]


