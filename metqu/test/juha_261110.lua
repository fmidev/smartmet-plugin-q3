require "metqu"

local PRODUCER_NAME=241

local r,err= raw( "../../../data/iv5_data/mallipinta/*.fqd", { params=N } )
assert(r,err)

local r2,err= raw( r, { sqd_producer= PRODUCER_NAME,    -- this makes it SQD format
                        sqd_gridsize= xy(2,2),
                        --
                        --projection= r.projection,
                        times={ r.times[2] }, 
                        params= r.params,
                } )
assert(r2,err)
       
DUMP( {
    times= r2.times,
    levels= r2.levels,
    params= r2.params,
} )

r2.write( "out.sqd" )

--
print "\nOK :)"
