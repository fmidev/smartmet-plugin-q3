SUBNAME = q3
SPEC = smartmet-plugin-$(SUBNAME)
INCDIR = smartmet/plugins/$(SUBNAME)

REQUIRES = geos gdal cairo configpp

include $(shell echo $${PREFIX-/usr})/share/smartmet/devel/makefile.inc

FLAGS += -Wno-variadic-macros -Wno-narrowing -mfpmath=sse -msse2

# Compiler options

DEFINES = -DUNIX -D_REENTRANT -DSMOOTH_AND_STRETCH -DUSE_NEWBASE -DUSE_TRON -DUSE_UNSTABLE_GEOS_CPP_API

INCLUDES += $(shell pkg-config --cflags-only-I luajit)

LIBS += -L$(libdir) \
	$(REQUIRED_LIBS) \
	-lsmartmet-newbase \
	-lsmartmet-spine \
	-lsmartmet-tron \
	$(shell pkg-config --libs luajit)

# What to install

LIBFILE = $(SUBNAME).so

# Compilation directories

vpath %.cpp $(SUBNAME)
vpath %.h $(SUBNAME)

# The files to be compiled

SRCS = $(wildcard $(SUBNAME)/*.cpp)
HDRS = $(wildcard $(SUBNAME)/*.h)
OBJS = $(patsubst %.cpp, obj/%.o, $(notdir $(SRCS)))

LUAS = $(wildcard lua/*.lua)
LCHS = $(patsubst %.lua, q3/%.lch, $(notdir $(LUAS)))

INCLUDES := -I$(SUBNAME) $(INCLUDES)

.PHONY: test rpm

# The rules

all: objdir $(LIBFILE)
debug: all
release: all
profile: all

$(LIBFILE): $(LCHS) $(OBJS)
	$(CXX) $(LDFLAGS) -shared -rdynamic -o $(LIBFILE) $(OBJS) $(LIBS)
	@echo Checking $(LIBFILE) for unresolved references
	@if ldd -r $(LIBFILE) 2>&1 | c++filt | grep ^undefined\ symbol |\
	                grep -Pv ':\ __(?:(?:a|t|ub)san_|sanitizer_)'; \
	then \
	        rm -v $(LIBFILE); \
	        exit 1; \
	fi

clean: 
	rm -f $(LIBFILE) *~ $(SUBNAME)/*~ */*.lch
	rm -rf obj

format:
	clang-format -i -style=file $(SUBNAME)/*.h $(SUBNAME)/*.cpp

install:
	@mkdir -p $(plugindir)
	$(INSTALL_PROG) $(LIBFILE) $(plugindir)/$(LIBFILE)

objdir:
	@mkdir -p $(objdir)

rpm: clean $(SPEC).spec
	rm -f $(SPEC).tar.gz # Clean a possible leftover from previous attempt
	tar -czvf $(SPEC).tar.gz --exclude packaging --exclude-vcs --transform "s,^,$(SPEC)/," *
	rpmbuild -tb $(SPEC).tar.gz
	rm -f $(SPEC).tar.gz

.SUFFIXES: $(SUFFIXES) .cpp .lua

obj/%.o: %.cpp $(LCHS)
	$(CXX) $(CFLAGS) $(INCLUDES) -c -MD -MF $(patsubst obj/%.o, obj/%.d, $@) -MT $@ -o $@ $<

q3/%.lch: lua/%.lua
	luajit -b $< - | luajit bin2c.lua -o $@

test:
	cd test && $(MAKE) test

-include $(wildcard obj/*.d)
