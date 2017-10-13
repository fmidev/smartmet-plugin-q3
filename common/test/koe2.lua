--
-- KOE2.LUA
--
-- Bugzilla-testaukseen
--

local function merge_arrays(...)
    local t={}
    for i=1,select('#',...) do
        for _,v in ipairs(select(i,...)) do
            t[#t+1]= v
        end
    end
    return t
end


local surfaceSqd= SQD{}
assert( surfaceSqd )


local CLOUD_LAYER_COVER_PARAMS={"FL1Cover","FL2Cover","FL3Cover","FL4Cover","FL5Cover","FL6Cover","FL7Cover","FL8Cover"}

local surfaceRaw= raw( surfaceSqd, { params= merge_arrays( CLOUD_LAYER_COVER_PARAMS, surfaceSqd.params ) } )

return surfaceRaw
