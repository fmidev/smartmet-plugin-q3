--[[
-- description: Testataan eri painepintadatojen käyttö
--
-- projection:  stereographic,20,90,60:6,51.3,49,70.2
-- gridsize:    5,6
-- decimals:    3
-- validtime:   TODAY12
--]]

--[[
    NOTE: Q2 is unable to give interpolated levels (850 is ok, 823 is NOT)
          Causes "proxy upstream error. code 2"
    
    NOTE: The returned values are different. Gathering data with 'hpa={850,823}' causes
         the hybrid data ("mallipinta") to be used. Also 'hpa=823' does this (for best
         interpolation of pressure data). 'hpa=850', however matches pre-calculated
         pressure data ("painepinta") and gives slightly different results. 
         This is normal. -- AKa 11-May-10
]]

local r,err= HIR{ hpa={850,823}, params={T} }
assert(r,err)

return r{ hpa=850 }.T - r{ hpa=823 }.T
       --, HIR{hpa=850}.T - HIR{hpa=823}.T

--[[q2:
    RESULT= T_HIR_850-T_HIR_823
]]
