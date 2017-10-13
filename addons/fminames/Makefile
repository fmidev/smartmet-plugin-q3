#
# Makefile 
#
# make VARIANT=release|debug CFLAGS=...
# make test
#

# 'TOOLS=tools' provided by RPM packaging script (local copy of 'tools')
#
TOOLS=../newcairo/tools

BIN2C=$(TOOLS)/bin2c.lua

+SRCS=$(wildcard source/*.cpp) #,$(TOOLS)/luanew/source/Proto.cpp
+OBJS=$(addprefix obj/,$(notdir $(SRCS:.cpp=.o))) #../../server/obj/Proto.os

CFLAGS=-Wall -W -fPIC -std=c++11

VARIANT=debug
ifeq "$(VARIANT)" "debug"
  CFLAGS+=-O0 -g
else
  CFLAGS+=-O2
endif

FMINAMES_SO=lua51-fminames.so

UBUNTU=$(shell uname -a | grep Ubuntu | wc -l)
   # 0 or 1

# Centos and RHEL have 'pkg-config lua'
# Ubuntu and Debian have 'pkg-config lua5.1'
#
# Linking to Lua libs.
#
ifneq "$(shell pkg-config --exists lua5.1 && echo 1)" ""
  CFLAGS+=$(shell pkg-config --cflags lua5.1)
  LIBS+=$(shell pkg-config --libs lua5.1)
else
  CFLAGS+=$(shell pkg-config --cflags lua)
  LIBS+=$(shell pkg-config --libs lua)
endif

# System FmiNames
#
CFLAGS+=-I /usr/include/smartmet/locus -I $(TOOLS)/luanew/include 
FMINAMES_A=/usr/lib64/libsmartmet-locus.so

ifeq "$(shell uname -s)" "Darwin"
  SHARED=-bundle
  LUA=lua51 -lstrict
else
  SHARED=-shared
  LUA=lua -lstrict
endif

# Centos and OS X (fink) have 'strict.lua' available from the Lua package.
# Ubuntu has not.
#
ifeq "$(UBUNTU)" "1"
  LUA := LUA_PATH=$(TOOLS)/?.lua $(LUA)
endif

#--
all: $(FMINAMES_SO)

# Note: target '$^' must be *before* FmiNames library for linking to succeed    --AKa 16-Nov-10
#
$(FMINAMES_SO): $(OBJS)
	g++ -o $@ $(SHARED) $(LIBS) $^ $(FMINAMES_A)

obj/%.lch: lua/%.lua
	luac -o delme $< && lua $(BIN2C) delme -o $@
	@rm delme

obj/FmiNames.o: obj/fminames.lch

obj/%.o: source/%.cpp
	g++ -c -o $@ $(CFLAGS) -I obj $<

clean:
	-rm -rf *.so obj/*

test:  $(FMINAMES_SO) test.lua
	LUA_CPATH=./lua51-?.so \
	  $(LUA) test.lua

echo:
	@echo $(OBJS)
