#
# Makefile
#
help:
	@echo "Usage:"
	@echo "    make rpm        Create all RPM packages"
	@echo "    make clean      Clean RPM packages and subdirectories"
	@echo ""


# Package names
#
Q3_PACKAGE =        smartmet-plugin-q3
STRICT_PACKAGE =    lua-strict


# We build the RPMs locally. Makefile creates this directory unless already there.
#
RPMBUILD=$(HOME)/rpmbuild
UNAME_P = $(shell uname -p)

# The Makefile is run with '_PACKAGE=' to build a certain package. 
#
_PACKAGE=

ifneq "$(_PACKAGE)" ""
  SPEC      = $(_PACKAGE).spec
  SOURCE_DIR = $(RPMBUILD)/SOURCES/$(_PACKAGE)
  SOURCE_TGZ = $(SOURCE_DIR).tgz
  VERSION   := $(shell grep "^Version:" $(SPEC) | cut -d\  -f 2)
  RELEASE   := $(shell grep "^Release:" $(SPEC) | cut -d\  -f 2)
  #
  # This section defines, what goes in the source tarballs (paths based in '../')
  #
  # 06-Feb-2012 MHe/PKi: Added addons
  #
  ifeq "$(_PACKAGE)" "$(Q3_PACKAGE)"
    SOURCES=addons common tools server fonts packaging/*.conf packaging/*.html
  endif
  
  RPM_MASK = $(RPMBUILD)/RPMS/$(UNAME_P)/$(_PACKAGE)*-$(VERSION)-$(RELEASE).$(UNAME_P).rpm
endif

#---
# We need to make a copy of the stuff, in order to get the tar top level
# directory name right. There ought to be an easier way, but I doubt
# (tried symbolic links; tar does not follow them).   --AKa 8-Jan-2010 
#
# 06-Feb-2012 MHe/PKi: Excluding .spec files too; otherwise build error 'second %prep'
#
_rpm:
	@mkdir -p $(RPMBUILD)/{RPMBUILD,RPMS,SOURCES,SPECS,SRPMS,TEMP}
	-[ -f $(SOURCE_TGZ) ] && rm $(SOURCE_TGZ)
	-rm -rf $(SOURCE_DIR)
	mkdir -p $(SOURCE_DIR)
ifeq "$(_PACKAGE)" "$(STRICT_PACKAGE)"
	cp ../addons/newcairo/tools/strict.lua $(SOURCE_DIR)/
else
	(cd .. && \
	tar --exclude=".*" --exclude="xcode" --exclude="q3_xcode" --exclude="_Unused_" \
		--exclude="*.so" --exclude="*.o" --exclude="newcairo/examples"  \
		--exclude="core*" --exclude="out/*" --exclude="tmp/*" \
		--exclude="obj/*" --exclude="*.spec" \
	    -c $(SOURCES) | tar -C $(SOURCE_DIR) -x \
	)
	(cd ../addons/newcairo && \
	   tar --exclude=".*" -c tools | tar -C $(SOURCE_DIR) -x \
	)
endif
	cp $(_PACKAGE).spec $(SOURCE_DIR)/
	tar -C $(RPMBUILD)/SOURCES -czf $(SOURCE_TGZ) $(_PACKAGE)
	#rm -rf $(SOURCE_DIR)
	#
	TAR_OPTIONS=--wildcards \
	rpmbuild --define "_topdir $(RPMBUILD)" \
				 --define "_tmppath $(RPMBUILD)/TEMP" \
	         -v -ta $(SOURCE_TGZ)
	@echo ""
	@ls -al --color=tty $(RPM_MASK)
	@echo ""
	rpm -qlp $(RPM_MASK)
	@echo ""
	@echo "*** $(_PACKAGE) - packaging done ***"
	@echo ""

#---
rpm:
	$(MAKE) q3_rpm
#	$(MAKE) strict_rpm
	@echo ""
	@echo "*** All RPMs done ***"
	@echo ""

q3_rpm:
	$(MAKE) _rpm _PACKAGE=$(Q3_PACKAGE)

strict_rpm:
	$(MAKE) _rpm _PACKAGE=$(STRICT_PACKAGE)

clean:
	-rm -rf $(RPMBUILD)
