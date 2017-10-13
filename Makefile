#
# Makefile
#
help:
	@echo "Usage:"
	@echo "    make run [PORT=808x] [VALGRIND=valgrind|\"valgrind --leak-check=full\"] [NICE=nice] [VARIANT=debug|release]"
	@echo "    make clean"
	@echo ""
	@echo "    make rpm        Create all RPM packages"
	@echo ""
	@echo "    make check      Run 'jslint' of JavaScript files"
	@echo ""


#---=== Debug runs ===---

PORT=8091
VALGRIND=
NICE=

# Note: $VARIANT only affects 'make run' (not rpm builds)
#
VARIANT=debug
    # release
    # debug

SERVER_PATH=server
ADDONS_PATH=addons
RUN_PATH=run

SMARTMETD_RUNNING := $(shell ps -U $(USER) | grep smartmetd)

CUSTOM_FONT_PATH=$(shell pwd)/fonts

TMP=tmp

Q3_PACKAGE=smartmet-plugin-q3

Q3_VER := $(shell grep "^Version:" packaging/$(Q3_PACKAGE).spec | cut -d\  -f 2)
Q3_REV := $(shell grep "^Release:" packaging/$(Q3_PACKAGE).spec | cut -d\  -f 2)

#---
# Run the server (and keep running...)
#
# 8080:         SmartMet Server service installed on the machine (via RPM)
#
# 8091:         "make run" and "make bs_run" (development)
#
# Note: Using 8088 on 'crash.fmi.fi' has funny effects. It seems to redirect to 8081
#       (maybe some system config doing this?). Not good for 'make run'.
#
# Note: Cannot have the makefile do 'ulimit -c unlimited' automatically. Tried.
#
# Deletes 'core.*' so we only get the latest core (if the server crashes)
#
# This is for the DEVELOPMENT VERSION ONLY.
#
# NOTE: When using 'gdb' backtrace, you MUST BE IN THE DIRECTORY OF THE CORE
#       DUMP FILE. Otherwise, you won't get the debug symbols!    --AKa 25-Feb-10
#
#       In other words:
#           make run
#               ..craassh BOOM!..
#           gdb server/q3server core.*
#               ...
#           bt
#           (you should see the stack trace with function names, source files, line numbers)
#
bs_run: $(RUN_PATH)/temp/run.conf already_running_check
	install -d $(RUN_PATH)/plugins
	-rm $(RUN_PATH)/plugins/*
	$(MAKE) bs_compile
	ln -s `pwd`/$(SERVER_PATH)/q3.so $(RUN_PATH)/plugins/q3.so
	cp `pwd`/$(SERVER_PATH)/libfmi-q3.so $(RUN_PATH)/temp/
	@echo ""
	@echo "NOTE: To have core dumps created, run 'ulimit -c unlimited'"
	@echo "      Then (if crashed) 'gdb /usr/sbin/smartmetd core.NNN' to load gdb"
	@echo "      Then (in gdb) 'bt' to show back trace"
	@echo ""
	-rm core.*
	$(NICE) $(VALGRIND) LD_LIBRARY_PATH=$(RUN_PATH)/temp \
	   /usr/sbin/smartmetd -c $(RUN_PATH)/smartmetd.conf -L $(RUN_PATH) -p $(PORT) -v

# Create a test variant of the production "model" 'q3server.conf'
#
# Note: 'addons/?.lua' is there for 'areamask.xxx' -> 'addons/areamask/xxx.lua'
#
$(RUN_PATH)/temp/run.conf: packaging/q3server.conf Makefile
	cat $< \
	   | sed -e "s/^log=.*/log=stderr/" \
	   | sed -e "s+^testbed=.*+testbed=packaging/testbed.html+" \
	   | sed -e "s+^package_path=.*+package_path=addons/?.lua;addons/?/?.lua+" \
	   | sed -e "s+^package_cpath=.*+package_cpath=addons/?/lua51-?-server.so;addons/?/lua51-?.so;addons/fmi/?/lua51-?.so+" \
	> $@

clean-run:
	-rm -f $(RUN_PATH)/plugins/* $(RUN_PATH)/engines/* $(RUN_PATH)/temp/*

# Check that no SmartMet Server instance is running in the background. There's some weirdness
# if this is not done: a zombie can run at the background taking up a certain port and NOT
# giving an error if we try to again use the port. Weird. To prevent this, we try to catch
# such issues first hand.   -- AKa 2010
#
already_running_check:
ifneq "$(SMARTMETD_RUNNING)" ""
	@echo "SmartMet Server already running - kill this process manually:"
	@echo ""
	@echo "$(SMARTMETD_RUNNING)"
	@echo ""
	@false
endif

bs_compile:
	(cd $(SERVER_PATH) && $(MAKE) bs_$(VARIANT))
	$(MAKE) addons_compile

addons_compile:
	(cd $(ADDONS_PATH)/newcairo && $(MAKE) server VARIANT=$(VARIANT) CUSTOM_FONT_PATH=$(CUSTOM_FONT_PATH))
	(cd $(ADDONS_PATH)/fminames && $(MAKE) VARIANT=$(VARIANT))

clean:
	cd server && $(MAKE) clean
	cd addons/newcairo && $(MAKE) clean
	cd addons/fminames && $(MAKE) clean
	$(MAKE) clean-run

RPMBUILD=$(HOME)/rpmbuild
UNAME_P = $(shell uname -p)
    # x86_64

RPMS_PATH=$(HOME)/rpmbuild/RPMS/$(UNAME_P)

AREAMASK_Q3_RPM=    $(wildcard $(RPMS_PATH)/fmi-areamask-q3-*)

FMINAMES_Q3_RPM=    $(wildcard $(RPMS_PATH)/fmi-fminames-q3-*)

NEWCAIRO_Q3_RPM=    $(wildcard $(RPMS_PATH)/lua-newcairo-q3-*)
NEWCAIRO_RPM=       $(filter-out $(NEWCAIRO_Q3_RPM), $(wildcard $(RPMS_PATH)/lua-newcairo-*))

Q3_SMARTMETD_RPM=  $(wildcard $(RPMS_PATH)/smartmet-plugin-q3)

STRICT_RPM=         $(wildcard $(RPMS_PATH)/lua-strict-*)

# Note: We need to clear '$(RPMBUILD)' each time. Otherwise 'msg' will show not only the
#       latest versions. Knowing the right versions (from studying the '.spec' files) is
#       possible but elaborate.
rpm:
	-rm -rf $(RPMBUILD)
	(cd packaging && $(MAKE) rpm)
	(cd addons && $(MAKE) rpm)
	$(MAKE) msg

# Note: '$(HOSTNAME)' is a system env.var. (i.e. "crash.fmi.fi")
#
msg:
	@echo ""
	@echo "** Update Q3 SmartMet Server plugin (port 8080) by **"
	@echo ""
	@echo "  systemctl stop smartmetd"
	@echo "  sudo rpm -Uvh $(Q3_SMARTMETD_RPM) \\"
	@echo "              $(Q3_LIB_RPM) \\"
	@echo "              $(Q3_CONFIG_RPM)"
	@echo "  systemctl start smartmetd"
	@echo ""
	@echo "** Update server extensions by (any, none or all of these) **"
	@echo ""
	@echo "  sudo rpm -U $(NEWCAIRO_Q3_RPM)"
	@echo "  sudo rpm -U $(FMINAMES_Q3_RPM)"
	@echo "  sudo rpm -U $(AREAMASK_Q3_RPM)"
	@echo ""
	@echo "** To test the server **"
	@echo ""
	@echo "  open 'http://$(HOSTNAME):8080/q3' (SmartMet Server plugin)"
	@echo ""
	@echo "  You should see a testbed form and be able to edit & run scripts."
	@echo ""


echo:
	@echo "$(Q3_LIB_RPM)"

format:
	clang-format -i -style=file server/include/*.h server/source/*.cpp

#---
.PHONY: run compile clean-run clean echo


