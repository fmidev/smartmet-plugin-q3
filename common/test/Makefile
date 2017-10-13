#
# make test[NN] selftest [VALGRIND=valgrind]
#
# NOTE: To have core dumps generated, run 'ulimit -c unlimited' on Linux
#
LUA51_METQU_SO=../lua51-metqu.so
VARIANT=release
    # debug
    
VALIDTIME=
PROJ=stereographic,20,90,60:6,51.3,49,70.2
GRIDSIZE=
SCRIPT=
SQD=
OUTPUT=
VALGRIND=
KILLTIME=-k 100
NOKILL=-k 0

ADDONS_PATH=../../addons
DATA_PATH=../../../data

# If running this from tags, add one more directory level
#
ifneq "$(findstring /tags/,$(shell pwd))" ""
  DATA_PATH:=../$(DATA_PATH)
endif

KOE_LUA=no_koe_lua

all test:
	$(MAKE) selftest

# Note: Use '-v' (verbose) to get LOG output.
#
test0:
	$(MAKE) _test VALIDTIME=20090218120000 GRIDSIZE=5,6 \
	               "SCRIPT=return HIR.T" 

test1:
	$(MAKE) _test VALIDTIME=20090218120000 GRIDSIZE=50,60 \
	               "SCRIPT=return HIR.WS-EC.WS, 22"

koe54:
	$(MAKE) _koe KOE_LUA=koe_54.lua

koe:
	$(MAKE) _koe KOE_LUA=koe.lua

_koe: $(LUA51_METQU_SO) $(KOE_LUA)
	@echo ""
	@echo "NOTE: To have core dumps created, run 'ulimit -c unlimited'"
	@echo "      Then (if crashed) 'gdb lua core.NNN' to load gdb"
	@echo "      Then (in gdb) 'bt' to show back trace"
	@echo ""
	@echo "      Position of core file can be changed by:"
	@echo "      'echo /tmp/core > /proc/sys/kernel/core_pattern' (as su)"
	@echo ""
	LUA_CPATH="../lua51-?.so" \
	LUA_PATH="../?.lua" \
	DATA_PATH=$(DATA_PATH) \
	  lua -lstrict $(KOE_LUA)

no_koe_lua:
	false

graf: $(LUA51_METQU_SO) graf.lua
	LUA_CPATH="../lua51-?.so" \
	LUA_PATH="$(ADDONS_PATH)/?/?.lua;../?.lua" \
	DATA_PATH=$(DATA_PATH) \
	  lua -lstrict graf.lua 

#---
require: $(LUA51_METQU_SO)
	LUA_CPATH="../lua51-?.so" \
	LUA_PATH="../?.lua" \
	  lua -lstrict -e "require 'metqu'"

#---
ANNAKAISA_SQD=../../ecmwf_skandinavia_avimerge.sqd

annakaisa: $(LUA51_METQU_SO) $(ANNAKAISA_SQD)
	LUA_CPATH="../lua51-?.so" \
	LUA_PATH="../?.lua" \
	  lua -lstrict ../../packaging/metqu annakaisa.lua $(ANNAKAISA_SQD)

#---
iv5: $(LUA51_METQU_SO)
	(cd .. && make standalone-$(VARIANT))
	cd IV5 && \
	LUA_CPATH="../../lua51-?.so" \
 	LUA_PATH="../../?.lua;?.lua" \
      lua -lstrict avimerge_reworked.lua

#---
iv5_image: $(LUA51_METQU_SO)
	(cd .. && make standalone-$(VARIANT))
	(cd ../../addons/newcairo && make lua51-newcairo.so)
	cd IV5 && \
	LUA_CPATH="../../lua51-?.so;../../../addons/?/lua51-?.so" \
 	LUA_PATH="../../?.lua;?.lua" \
      lua -lstrict image_product.lua

iv5_pinnat: $(LUA51_METQU_SO)
	(cd .. && make standalone-$(VARIANT))
	cd IV5 && \
	LUA_CPATH="../../lua51-?.so;../../../addons/?/lua51-?.so" \
 	LUA_PATH="../../?.lua;?.lua" \
      lua -lstrict pintojen_jarjestys.lua

#---
INPUT_DAT=../../skenaario_a1b.dat
OUTPUT_SQD=output.sqd

petri: $(LUA51_METQU_SO) $(INPUT_DAT)
	LUA_CPATH="../lua51-?.so" \
	LUA_PATH="../?.lua" \
	  lua -lstrict ../../packaging/metqu petri.lua $(INPUT_DAT) $(OUTPUT_SQD)
	ls -al $(OUTPUT_SQD)

	
# Open a command line prompt with 'require "q3"' loadable
#
run: $(LUA51_METQU_SO)
	LUA_CPATH="../lua51-?.so" \
	LUA_PATH="../?.lua" \
	DATA_PATH=$(DATA_PATH) \
	  lua 
	
test2:
	-rm output2.sqd
	$(MAKE) _test OUTPUT="-o output2.sqd" VALIDTIME=20090218120000 GRIDSIZE=50,60 \
	   "SCRIPT=return HIR.WS-EC.WS, 22"
	test ! -f output2.sqd && false
	ls -al output2.sqd

#
# Premade selftests
#
selftest: selftest.lua $(LUA51_METQU_SO)
	LUA_CPATH="../lua51-?.so" \
	LUA_PATH="../?.lua" \
	DATA_PATH=$(DATA_PATH) \
	   lua -lstrict $<

selftest_valgrind:
	$(MAKE) selftest KILLTIME="-k 0" VALGRIND=valgrind

# Note: The $(SQD) parameter is _intended_ to be expanded here. Instead of "*.sqd"
#       'q3' gets all the SQD filenames. Each of the filename parameters
#       is an entity, and only one (latest) file of each entity is used in 
#       data fetching. This means, giving '*.sqd' is same as "the most recent
#       SQD file, and only that". This is a bit confusing, I know, but dates
#       to the way configuration files for Q2 Engine are. For now.
#           --AKa 18-Feb-2009
#
#_test: $(Q3_EXE)
#	echo $(SCRIPT) | \
#	   $(Q3_EXE) $(CONF) $(OUTPUT) validtime=$(VALIDTIME) projection=$(PROJ) gridsize=$(GRIDSIZE) - -v
#	@echo ""

.PHONY: test _test test0 test1 test_shebang

# IMPORTANT: use of 'standalone-debug' or 'standalone-release' below is ESSENTIAL!
#
$(LUA51_METQU_SO): ../source/*.cpp ../lua/*.lua ../include/*.h ../SConstruct ../Makefile ../../tools/luanew/lua/*.lua ../../tools/luanew/source/* ../../tools/luanew/include/*
	cd .. && $(MAKE) standalone-$(VARIANT)

clean:
	cd .. && $(MAKE) clean

echo:    
	echo xxx
