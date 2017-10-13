#
# Makefile
#
# Ref: Creating RPMs
#      <http://www.gurulabs.com/downloads/GURULABS-RPM-LAB/GURULABS-RPM-GUIDE-v1.0.PDF>
#
help:
	@echo "Usage:"
	@echo "    make rpm        Create all RPM packages"
	@echo ""

NEWCAIRO_PACKAGE = lua-newcairo
NEWCAIRO_SOURCES = newcairo

FMINAMES_PACKAGE = fmi-fminames
FMINAMES_SOURCES = fminames

AREAMASK_PACKAGE = fmi-areamask
AREAMASK_SOURCES = areamask

# We build the RPMs locally. Makefile creates this directory unless already there.
#
RPMBUILD=$(HOME)/rpmbuild
UNAME_P = $(shell uname -p)
    # x86_64

# These are given values by 'make rpm' and alike
#
_PACKAGE=
_SOURCES=

ifneq "$(_PACKAGE)" ""
  SPEC      = $(_PACKAGE).spec
  SOURCE_DIR = $(RPMBUILD)/SOURCES/$(_PACKAGE)
  SOURCE_TGZ = $(SOURCE_DIR).tgz
  VERSION   := $(shell grep "^Version:" $(SPEC) | cut -d\  -f 2)
  RELEASE   := $(shell grep "^Release:" $(SPEC) | cut -d\  -f 2)
  
  ifeq "$(_PACKAGE)" "$(NEWCAIRO_PACKAGE)"
    RPM_METQU  = $(RPMBUILD)/RPMS/$(UNAME_P)/$(_PACKAGE)-$(VERSION)-$(RELEASE).$(UNAME_P).rpm
  else
    RPM_METQU  = $(RPMBUILD)/RPMS/$(UNAME_P)/$(_PACKAGE)-metqu-$(VERSION)-$(RELEASE).$(UNAME_P).rpm
  endif
  RPM_Q3     = $(RPMBUILD)/RPMS/$(UNAME_P)/$(_PACKAGE)-q3-$(VERSION)-$(RELEASE).$(UNAME_P).rpm
endif

#---
# We need to make a copy of the stuff, in order to get the tar top level
# directory name right. There ought to be an easier way, but I doubt
# (tried symbolic links; tar does not follow them).   --AKa 8-Jan-2010 
#
_rpm:
	@mkdir -p ~/rpmbuild/{BUILD,RPMS,SOURCES,SPECS,SRPMS,TEMP}
	-[ -f $(SOURCE_TGZ) ] && rm $(SOURCE_TGZ)
	-rm -rf $(SOURCE_DIR)
	mkdir -p $(SOURCE_DIR)
	#-[ -f $(_SOURCES)/Makefile ] && (cd $(_SOURCES) && $(MAKE) clean)
	(cd $(_SOURCES) && \
	   tar --exclude=".*" --exclude="_Unused_" --exclude="*.so" --exclude="*.o" --exclude="core*" \
	       --exclude="update-areamasks" --exclude="obj/*" --exclude="out/*" \
	    	-c * | tar -C $(SOURCE_DIR) -x \
	)
	(cd newcairo && \
	   tar --exclude=".*" -c tools | tar -C $(SOURCE_DIR) -x \
	)
	cp $(_PACKAGE).spec $(SOURCE_DIR)/
	tar -C $(RPMBUILD)/SOURCES -czf $(SOURCE_TGZ) $(_PACKAGE)
	#rm -rf $(SOURCE_DIR)
	#
	TAR_OPTIONS=--wildcards \
    	rpmbuild --define "_topdir $(RPMBUILD)" \
				 --define "_tmppath $(RPMBUILD)/TEMP" \
    	         -v -ta $(SOURCE_TGZ)
	@echo ""
ifneq "$(RPM_METQU)" ""
	  @ls -al --color=tty $(RPM_METQU)
	  @echo ""
	  rpm -qlp $(RPM_METQU)
	  @echo ""
endif
ifneq "$(RPM_Q3)" ""
	  @ls -al --color=tty $(RPM_Q3)
	  @echo ""
	  rpm -qlp $(RPM_Q3)
	  @echo ""
endif
	@echo "*** $(_PACKAGE) - packaging done ***"
	@echo ""

#---
rpm:
	$(MAKE) newcairo_rpm
	$(MAKE) fminames_rpm
	$(MAKE) areamask_rpm
	@echo ""
	@echo "*** All RPMs done ***"
	@echo ""

newcairo_rpm:
	$(MAKE) _rpm _PACKAGE=$(NEWCAIRO_PACKAGE) _SOURCES=$(NEWCAIRO_SOURCES)

fminames_rpm:
	$(MAKE) _rpm _PACKAGE=$(FMINAMES_PACKAGE) _SOURCES=$(FMINAMES_SOURCES)

areamask_rpm:
	$(MAKE) _rpm _PACKAGE=$(AREAMASK_PACKAGE) _SOURCES=$(AREAMASK_SOURCES)

clean:
	-rm -rf $(RPMBUILD)

echo:
	@echo "$(RPM)"
	@echo "$(RPM_SERVER)"

#---
.PHONY: 



