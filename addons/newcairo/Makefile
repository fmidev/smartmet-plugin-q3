#
# Makefile 
#
# make [server] VARIANT=release|debug CFLAGS=...
# make test
#
TOOLS=tools

BIN2C=$(TOOLS)/bin2c.lua
LUANEW_PATH=$(TOOLS)/luanew

SRCS=$(wildcard source/*.cpp)

FULL_OBJS=$(addprefix obj/1/,$(notdir $(SRCS:.cpp=.o)))
SERVER_OBJS=$(addprefix obj/2/,$(notdir $(SRCS:.cpp=.o)))

LUANEW_SRCS=$(wildcard $(LUANEW_PATH)/source/*.cpp)
LUANEW_OBJS=$(addprefix obj/,$(notdir $(LUANEW_SRCS:.cpp=.o)))

FULL_SO=lua53-newcairo.so
SERVER_SO=lua53-newcairo-server.so

CFLAGS=-Wall -W -fPIC -std=c++11

VARIANT=debug
ifeq "$(VARIANT)" "debug"
  CFLAGS+=-O0 -g
else
  CFLAGS+=-O2
endif

UBUNTU=$(shell uname -a | grep Ubuntu | wc -l)
   # 0 or 1

# Centos and RHEL have 'pkg-config lua'
# Ubuntu and Debian have 'pkg-config lua5.3'
#
ifneq "$(shell pkg-config --exists lua5.3 && echo 1)" ""
  CFLAGS+=$(shell pkg-config --cflags lua5.3)
  LIBS+=$(shell pkg-config --libs lua5.3)
else
  CFLAGS+=$(shell pkg-config --cflags lua)
  LIBS+=$(shell pkg-config --libs lua)
endif

CFLAGS+=$(shell pkg-config --cflags cairo)
LIBS+=$(shell pkg-config --libs cairo)

CFLAGS+=$(shell pkg-config --cflags freetype2)
LIBS+=$(shell pkg-config --libs freetype2)

CUSTOM_FONT_PATH=$(shell pwd)/fonts

ifeq "$(shell uname -s)" "Darwin"
  SHARED=-bundle
  LUA=lua53 -lstrict
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
all: $(FULL_SO) $(SERVER_SO)
	LUA_CPATH=./lua53-?.so \
	  $(LUA) -e "require 'newcairo'; print(newcairo.version)"

# 'standalone' is depricated name for "not server"
#
full standalone: $(FULL_SO)

server: $(SERVER_SO)

$(FULL_SO): $(FULL_OBJS) $(LUANEW_OBJS)
	g++ -o $@ $(SHARED) $(LIBS) $^

$(SERVER_SO): $(SERVER_OBJS) $(LUANEW_OBJS)
	g++ -o $@ $(SHARED) $(LIBS) $^

obj/%.lch: lua/%.lua
	luac -o delme $< && lua $(BIN2C) delme -o $@
	@rm delme

obj/1/NewCairo.o: obj/newcairo.lch

obj/2/NewCairo.o: obj/newcairo.lch

obj/1/%.o: source/%.cpp include/*.hpp
	@-mkdir -p obj/1
	g++ -c -o $@ $(CFLAGS) -I $(LUANEW_PATH)/include -I include -I obj $<

obj/2/%.o: source/%.cpp include/*.hpp
	@-mkdir -p obj/2
	g++ -c -o $@ $(CFLAGS) -DSANDBOX -DCUSTOM_FONT_PATH=\"$(CUSTOM_FONT_PATH)\" -I $(LUANEW_PATH)/include -I include -I obj $<

#-- 
# LuaNew and Proto sources + precompiled Lua chunk
#
obj/%.o: $(LUANEW_PATH)/source/%.cpp obj/proto.lch
	@-mkdir -p obj
	g++ -c -o $@ $(CFLAGS) -I $(LUANEW_PATH)/include -I obj $<

obj/%.lch: $(LUANEW_PATH)/lua/%.lua
	luac -o delme $< && lua $(BIN2C) delme -o $@
	@rm delme

clean:
	-rm -rf obj/* out/*

#--
# Tests from 'examples/*.lua' -> 'out/*.png'
#
EXAMPLES=$(wildcard examples/*.lua)
ALL_OUT_=$(addprefix out/,$(notdir $(EXAMPLES:.lua=.png)))
ALL_OUT=$(ALL_OUT_:test3.png=test3.pdf)
ROMEDALEN=examples/data/romedalen.png

test:  $(ALL_OUT)

test1: out/arc.png

out/clip_image.png: examples/clip_image.lua $(FULL_SO) $(ROMEDALEN)
	LUA_CPATH=./lua53-?.so \
	  $(LUA) $< $(ROMEDALEN) $@

out/image.png: examples/image.lua $(FULL_SO) $(ROMEDALEN)
	LUA_CPATH=./lua53-?.so \
	  $(LUA) $< $(ROMEDALEN) $@

out/imagepattern.png: examples/imagepattern.lua $(FULL_SO) $(ROMEDALEN)
	LUA_CPATH=./lua53-?.so \
	  $(LUA) $< $(ROMEDALEN) $@

out/%.png: examples/%.lua $(FULL_SO)
	LUA_CPATH=./lua53-?.so \
	  $(LUA) $< $@

out/%.pdf: examples/%.lua $(FULL_SO)
	LUA_CPATH=./lua53-?.so \
	  $(LUA) $< $@

echo:
	@echo $(CUSTOM_FONT_PATH)
