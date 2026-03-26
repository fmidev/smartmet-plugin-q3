#
# make [SCONS_FLAGS=...] [release|debug]
# make clean
#
LIB_SO = libfmi-q3.so
BS_PLUGIN_SO = q3.so

objdir = obj

SCONS_FLAGS += objdir=$(objdir)

#---
all release:
	scons $(SCONS_FLAGS) $(LIB_SO) $(BS_PLUGIN_SO)

debug:
	scons debug=1 $(SCONS_FLAGS) $(LIB_SO) $(BS_PLUGIN_SO)

#---
bs_release:
	scons -j7 $(SCONS_FLAGS) $(BS_PLUGIN_SO)

bs_debug:
	scons debug=1 $(SCONS_FLAGS) $(BS_PLUGIN_SO)

#---
clean:
	-rm -f $(LIB_SO) $(BS_PLUGIN_SO) *~ source/*~ include/*~
	-rm -rf $(objdir)

#--
.PHONY: clean debug release
