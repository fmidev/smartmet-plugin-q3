--[[

Script contains some common functions which are handy with iv5 lua scripts.

@version 20.08.2009 
@author juha.vainola@fmi.fi

--]]


-- Does meter to foot convertion for given value.
--
-- @param meters type:Float Value (in meters) which is converted.
--
-- @return type:Float Converted value (in feet)
--
function metersToFeet(meters)
	return meters*3.2808399
end


--
-- { ... }= split_by_comma( str [, func] )
-- [any]= func(any)
--
-- Split 'str' into substrings, passing them through the filter 'func' (i.e. 'tonumber').
--
function split_by_comma( str, func )
    local t= {}
    func= func or function(x) return x end

    for w in str:gmatch("[^,]+") do
        t[#t+1]= func(w)
    end
    return t
end

--
-- {...} = merge_arrays( {...} [, {...}] )
--
-- Merges multiple arrays (tables with 1..N values) together.
--
function merge_arrays(...)
    local t= {}
    for i=1,select('#',...) do
        for _,v in ipairs( select(i,...) ) do
            t[#t+1]= v
        end
    end
    return t
end

-- 
-- { c,b,a }= array_reverse( { a,b,c} )
-- 
-- Returns a table (1..N) with it's values in reverse order.
-- 
function array_reverse(t)
    local ret= {}
    for i,v in ipairs(t) do
        ret[ #t-(i-1) ]= v
    end
    return ret
end

function in_table( e, t )
	for _,v in pairs(t) do
		if (v==e) then return true end
	end
	return false
end

function table_keys_in_sorted_order(t, func)
	local sorted_keys={}
	table.foreach (t, function (k) table.insert (sorted_keys, k) end )
	if(func==nil) then table.sort(sorted_keys)
	else table.sort(sorted_keys, func) end
	return sorted_keys	
end