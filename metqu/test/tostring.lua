require "metqu"

local MODEL_QUERYDATA_MASK= "../../../data/iv5_data/mallipinta/2010*.sqd"

local r,err= raw( MODEL_QUERYDATA_MASK ) 
assert( r,err ) 

print( r.T )
