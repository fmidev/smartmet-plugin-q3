--
-- GAUSS.LUA                            Copyright 2009-2010, Ilmatieteen laitos
--
-- Statistics utility functions.
--
-- This file is compiled into the Q3 engine, just like 'q3.lua' is.
-- Keeping it in a separate source file is merely "making a point" of it
-- not being 100% essential (it could also be placed as a 'require'-loadable
-- addon on the server disk).
--

local bind= assert( select(1,...) )     -- C side exports

-- Functions for creating C++ side objects
--
local q3_count= assert( bind._count )

bind=nil
local table_insert= table.insert

-- lua5.1 ==> 5.3: For some reason global 'type' (table is set by type.lua) is a
-- function (global function not replaced by the table) and thus type.xxx is not
-- available. Using 'typetable' global/reference set by type.lua
--
-- TODO: Should fix the root of the problem though
--
local type= typetable

--
-- uint= count( m )
-- uint, uint [, ...]= count( m, num [, ...] )
--
-- Count the number of values in 'm' that are <= 'num'. If no limits are given, counts
-- the total number of non-NAN values in 'm'.
--
-- Returns the counts for each range. I.e. '10' will return two values (count of <= 10
-- and count of >10). '10,15' will return three values (<=10, 10<x<=15, and >15).
--
-- The limits are expected to be given in growing order.
--
function count( m, ... )
    proto( "Matrix,[number],...", m, ... )

    if select('#',...)==0 then
        -- no ranges, just count all non-NAN values
        --
        return q3_count(m)  -- SSE optimized
    else
        local limits= {...}
        local counts= {}
        local rest= 0   -- the last slot (higher than last limit)

        for i=1,#limits do
            counts[i]= 0
        end

        for _,v in points(m) do
            if not isnan(v) then
                rest= rest+1
                for i,limit in ipairs(limits) do
                    if v <= limit then
                        counts[i]= counts[i]+1
                        rest= rest-1
                        break
                    end
                end
            end
        end
        counts[#counts+1]= rest
        return unpack(counts)
    end
end

--
-- num|nan= abs_deviation( matrix_ud [,central_num] )
--
-- Average absolute deviation (by default MEAN absolute deviation)
--
-- There are multiple varieties of "absolute deviation".
-- See http://en.wikipedia.org/wiki/Average_deviation
--
function abs_deviation( m, central )
    proto( "Matrix,[number]", m, central )

    -- Returns NAN if the matrix has no valid values
    --
    return avg( abs( m - (central or avg(m)) ) )
end

--
-- num|nan= std_variance( matrix_ud [,central_num] )
--
-- Estimate of standard variance (square of deviation) based on the sampled
-- approach (dived by 'n-1'). Any grid data are samples by nature.
--
-- See http://en.wikipedia.org/wiki/Standard_deviation
--
function std_variance( m, central )
    proto( "Matrix,[number]", m, central )

    local n= q3_count(m)    -- number of non-nan values
    
    if n==0 then
        return nan
    elseif n==1 then
        return 0    -- avoid division by zero (just one value so deviation is zero)
    else
        return sum(( m - (central or avg(m)) )^2) / (n-1)
    end
end

--
-- num|nan= std_deviation( matrix_ud [,central_num] )
--
-- Square root of standard variance.
--
function std_deviation( ... )
    return sqrt( std_variance(...) )
end


---=== Gauss distribution functions ===---
--
-- The external interface is done via 'gauss()' objects, which have the
-- various functions available as their methods. This way we only need
-- to check validity of parameters once, and we get less namespace
-- pollution. Also, we can implement the local functions for the 
-- normal distribution only (mean=0, deviation=1).
--
-- Note: Calculation of the gauss distribution cumulative probabilities
--      (the cdf) cannot be performed by standard mathematical functions. 
--      We cache the cdf of normal Gauss distribution to allow speedy
--      usage (if the script uses 'gauss()' once, it's bound to be using
--      it a lot more).   --AKa 14-Jul-10
--
-- Ref. http://en.wikipedia.org/wiki/Normal_distribution
--
do 
    -- Constants used
    --
    local per_sqrt_2pi= 1.0/sqrt(2.0*pi)

    --
    -- cdf= { [0]=0.5, num, ... }
    --
    -- 'cdf' is an array of increasing probabilities, describing the right side of 
    -- a Gauss distribution curve.
    --
    local cdf
    local cdf_step= 0.1     -- 'cdf' accuracy
    local cdf_last= 4.0     -- end of the expected use range (normalized)
                            -- (4.0 corresponds probability of 0.99995188448116)

    --
    -- num= normal_pdf( normalized_num )
    --
    local function normal_pdf( x )
        return per_sqrt_2pi * exp( -(x^2)/2.0 )
    end

--[[
    --
    -- num= normal_erf( normalized_num )
    --
    -- The "error function" is closely related to CDF (see below). The ERF cannot
    -- be calculated exactly by regular functions; estimations or numerical integrations
    -- are what we can do (alas, this applies to CDF as well).
    --
    local function normal_erf( x )
        local a= 0.140012   -- (8*(pi-3)) / (3*pi*(4-pi))
        local sign= (x>0) and 1 or (x<0) and -1 or 0
        local x2= x^2
        return sign * sqrt( 1 - exp( -x2 * ((4/pi + a*x2) / (1+a*x2)) ) )
    end

    --
    -- num= normal_cdf_by_erf( normalized_num )
    --
    -- Returns the probability (0.0..1.0) of values being below the 'v_normalized'
    -- level, according to given Gaussian distribution.
    --
    -- "cdf" stands for Cumulative Distribution Function
    -- (ref. http://en.wikipedia.org/wiki/Cumulative_distribution_function)
    --
    local per_sqrt_2= 1 / sqrt(2)
    local function normal_cdf_by_erf( x )
        local ret= 0.5 * (1 + normal_erf(x*per_sqrt_2))
        assert( ret>=0 and ret<=1 )     -- internal sanity check
        return ret
    end
]]

    --
    -- { [0]=0.5, ... }= normal_cdf_cache()
    --
    -- Returns the array to be placed in 'cdf' for calculation of Gauss probabilities. 
    -- Called once per each script only (when first time using 'gauss()').
    --
    local function normal_cdf_cache()
        -- Abramowitz-Stegun polynomial:
        --      ".. approximation for Φ(x) with the absolute error |ε(x)| < 7.5·10−8"
        --
        local ret= { [0]= 0.5 }   -- the distribution is symmetrical

        for x=cdf_step, cdf_last, cdf_step do
            local t= 1 / (1+0.2316419*x)
            local cdf_x= 1- normal_pdf(x) * (0.319381530*t - 0.356563782*t^2 + 1.781477937*t^3 - 1.821255978*t^4 + 1.330274429*t^5)

            assert( cdf_x>=0 and cdf_x<=1 )     -- internal sanity checks
            assert( cdf_x > ret[#ret] )

            ret[#ret+1]= cdf_x       
        end
        return ret
    end

    -- 
    -- num= normal_cdf( normalized_num )
    --
    -- Returns the probability of values being < 'x' based on a normal Gauss distribution.
    --
    local function normal_cdf( x )
        if x<0 then
            return 1-normal_cdf(-x)     -- distribution is symmetric
        end

        -- Find the closest precalculated value (we don't do interpolations between them; 
        -- if the accuracy is too low, reduce 'cdf_step' to get more values in the cache.
        --
        local i= floor( x/cdf_step + 0.5 )
        return cdf[i] or 1  -- give 1 for values beyond the calculated cache
    end
    
    --
    -- num= normal_limit( prob_num )
    --
    -- Returns a normalized value of the Gauss distribution that has _at least_ 'prob' probability
    -- of values below it. In other words, the returned value is a higher limit estimate.
    --
    local function normal_limit( prob )
        if prob<0.5 then
            return -normal_limit(1-prob)    -- distribution is symmetric
        end

        -- Loop through the probabilities cached in 'cdf'
        --
        for i=0,#cdf do
            if cdf[i] >= prob then
                return i*cdf_step
            end
        end
        return inf  -- 'prob' was bigger than what the cache had
    end

    -- Selftest
    --
    do
        cdf= normal_cdf_cache()
--LOG(normal_limit(0.5))
--LOG(normal_limit(0.7))  -- 0.6
--LOG(normal_limit(0.4))  -- -0.3
        assert( normal_limit(0.5)==0 )  -- 0.0
        assert( normal_limit(0.7)>0 )   -- 0.6
        assert( normal_limit(0.4)<0 )   -- -0.3
    end

    -- 
    -- g= gauss( mean_num, deviation_num )
    --
    function gauss( mean, deviation )
        proto( "number,number", mean, deviation )

        if deviation<=0 then
            error( "Deviation cannot be <= 0: "..deviation, 2 )
        end

        if not cdf then
            cdf= normal_cdf_cache()  -- one time only
            assert(cdf)
        end

        -- 'mean', 'deviation' etc. are remembered as upvalues of each returned object
        --        
        local s= deviation
        return {
            --
            -- num= g.prob( num )
            -- matrix= g.prob( matrix )
            --
            prob= function(v)
                    proto("number|Matrix",v)

                    if type.Matrix(v) then
                        return foreach( (v-mean)/s, normal_cdf )
                    else
                        return normal_cdf( (v-mean)/s )
                    end
                end,

            -- 
            -- limit_num= g.limit( prob_num )
            --
            -- Returns an estimate of the value providing given probability _below_ it
            -- on the Gaussian distribution.
            --
            limit= function(prob)
                    proto("number",prob)
                    if prob<0 or prob>100 then
                        error( "Probability out of range (0..1): "..prob, 2 )
                    end
                    return mean + normal_limit(prob)*s
                end,
                
            -- 
            -- { [0.0]=0.5, ... }= g.dump()
            --
            -- For debugging only. Returns the internal cache table.
            --
            dump= function() 
                return cdf
            end
        }
    end
end

--
-- prc_val|prc_matrix= crit_percent( data_num|matrix, bias_num, deviation_num, x_num )
--
-- Analyses the given value or matrix (an observation or an estimate), for the 
-- probability that the real world value would in fact exceed the 'crit' level.
--
-- 'bias' is the known model bias, which is simply subtracted from the values,
-- before analysis.
--
-- 'deviation' is a standard deviation (sigma) that defines the width of the
-- Gaussian distribution that percentages are calculated from. 
--
-- Returns percentages (0.0 .. 100.0) of the change of _exceeding_ the 'crit'
--      level in real world.
--
function crit_percent( v, bias, deviation, x )
    proto( "number|Matrix,number,number,number", v,bias,deviation,x )

    -- Create one Gaussian distribution around zero (we give it the relative
    -- difference between 'x'-'v').
    --
    local gd= gauss(0,deviation)

    return 100 * (1-gd.prob(x - (v-bias)))   -- works for both scalars and matrices
end


--
-- val|matrix= crit_limit( data_num|matrix, bias_num, deviation_num, prc_num )
--
-- Returns the limits where probability of actual values being above them
-- is 'prc'.
--
function crit_limit( v, bias, deviation, prc )
    proto( "number|Matrix,number,number,number", v,bias,deviation,prc )

    local gd= gauss(0,deviation)

    return (v-bias)+gd.limit(1-prc/100)
end

