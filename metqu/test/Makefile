#
# make test[NN] selftest [VALGRIND=valgrind]
#
# NOTE: To have core dumps generated, run 'ulimit -c unlimited' on Linux
#
# USING GBD:
#   $ ulimit -c unlimited       # enables core dumping
#   $ make jira_85              # or any test case
#   ... core dumped ...
#   $ cd IV5                    # or where-ever the core was created (you MUST be in that directory for gdb to be happy!)
#   $ gdb lua core.xxx          # the particular core file
#   (gdb) bt                    # shows the stack trace ("back trace")
#
LUA51_METQU_SO=../lua51-metqu.so
VARIANT=debug
    # release
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

NEWCAIRO_TOOLS=$(ADDONS_PATH)/newcairo/tools

# If running this from tags, add one more directory level
#
ifneq "$(findstring /tags/,$(shell pwd))" ""
  DATA_PATH:=../$(DATA_PATH)
endif

KOE_LUA=no_koe_lua

TEST_FILES=$(wildcard jira_*.lua) $(wildcard juha_*.lua) interpolation.lua

help:
	@echo ""
	@echo "make all                    - make all selftests"
	@echo "make jira_nnn|juha_nnn|...  - run a certain test (from '<target>.lua')"
	@echo ""

test all:
	$(MAKE) $(TEST_FILES:.lua=)

#---
require: $(LUA51_METQU_SO)
	LUA_CPATH="../lua51-?.so" \
	LUA_PATH="$(NEWCAIRO_TOOLS)/?.lua" \
	  lua -lstrict -e "require 'metqu'"

#---
ANNAKAISA_SQD=../../ecmwf_skandinavia_avimerge.sqd

annakaisa: $(LUA51_METQU_SO) $(ANNAKAISA_SQD)
	LUA_CPATH="../lua51-?.so" \
	LUA_PATH="$(NEWCAIRO_TOOLS)/?.lua" \
	  lua -lstrict ../../packaging/metqu annakaisa.lua $(ANNAKAISA_SQD)

#---
missing_heights:
	-rm core.*
	(cd .. && make $(VARIANT))
	LUA_CPATH="../lua51-?.so" \
 	LUA_PATH="$(NEWCAIRO_TOOLS)/?.lua;?.lua" \
      lua -lstrict missing_heights.lua "/smartmet/brainstorm/ecmwf/mallipinta/ecmwf_mallipinta_*.sqd"

#---
iv5:
	(cd .. && make $(VARIANT))
	cd IV5 && \
	LUA_CPATH="../../lua51-?.so" \
 	LUA_PATH="../$(NEWCAIRO_TOOLS)/?.lua;?.lua" \
      lua -lstrict avimerge_reworked.lua

#---
iv5_image:
	(cd .. && make $(VARIANT))
	(cd ../../addons/newcairo && make lua51-newcairo.so)
	cd IV5 && \
	LUA_CPATH="../../lua51-?.so;../../../addons/?/lua51-?.so" \
 	LUA_PATH="../$(NEWCAIRO_TOOLS)/?.lua;?.lua" \
      lua -lstrict image_product.lua

iv5_pinnat:
	(cd .. && make $(VARIANT))
	cd IV5 && \
	LUA_CPATH="../../lua51-?.so;../../../addons/?/lua51-?.so" \
 	LUA_PATH="../$(NEWCAIRO_TOOLS)/?.lua;?.lua" \
      lua -lstrict pintojen_jarjestys.lua

levels:
	(cd .. && make $(VARIANT))
	LUA_CPATH="../lua51-?.so;../../addons/?/lua51-?.so" \
 	LUA_PATH="$(NEWCAIRO_TOOLS)/?.lua;?.lua" \
      lua -lstrict levels.lua

tostring:
	(cd .. && make $(VARIANT))
	LUA_CPATH="../lua51-?.so;../../addons/?/lua51-?.so" \
 	LUA_PATH="$(NEWCAIRO_TOOLS)/?.lua;?.lua" \
      lua -lstrict tostring.lua

#---
# jira_xxx
# juha_xxx
# interpolation
#
$(TEST_FILES:.lua=):
	(cd .. && make $(VARIANT))
	LUA_CPATH="../lua51-?.so" \
 	LUA_PATH="$(NEWCAIRO_TOOLS)/?.lua;?.lua" \
      lua -lstrict $@.lua

	
# Open a command line prompt with 'require "q3"' loadable
#
run: $(LUA51_METQU_SO)
	LUA_CPATH="../lua51-?.so" \
	LUA_PATH="$(NEWCAIRO_TOOLS)/?.lua" \
	DATA_PATH=$(DATA_PATH) \
	  lua 

$(LUA51_METQU_SO): ../Makefile ../SConstruct ../include/* ../source/* ../../common/include/* ../../common/source/* ../../common/lua/*
	cd .. && $(MAKE) $(VARIANT)

clean:
	cd .. && $(MAKE) clean

echo:    
	echo $(OUTPUT_SQD)

.PHONY: clean echo
