#
# make [PREFIX=path] [SCONS_FLAGS=-j4]
#
LUA51_METQU_SO=lua51-metqu.so
LUA51_METQU_NOSQD_SO=lua51-metqu_nosqd.so

SSE=1

objdir = obj

# The rules
#
SCONS_FLAGS += objdir=$(objdir)

#--
all: debug

release $(LUA51_METQU_SO): SConstruct
	scons $(SCONS_FLAGS) sse=$(SSE) $(LUA51_METQU_SO)

debug:
	scons $(SCONS_FLAGS) sse=$(SSE) debug=1 $(LUA51_METQU_SO)

# Variant without Newbase (or other FMI) dependencies
#
release-nosqd $(LUA51_METQU_NOSQD_SO): SConstruct
	scons $(SCONS_FLAGS) sse=$(SSE) $(LUA51_METQU_NOSQD_SO)

clean:
	-rm -f $(LUA51_METQU_SO) $(LUA51_METQU_NOSQD_SO) *~ source/*~ include/*~
	-rm -rf $(objdir)

run: $(LUA51_METQU_SO)
	LUA_CPATH="./lua51-?.so" \
      lua -e "require 'metqu'" -i

run-nosqd: $(LUA51_METQU_NOSQD_SO)
	LUA_CPATH="./lua51-?_nosqd.so" \
      lua -e "require 'metqu'" -i

test:
	cd test && make test

echo:
	@echo $(SSE)

#--
.PHONY: test
