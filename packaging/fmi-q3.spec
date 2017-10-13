#
# fmi-q3.spec
#
# Packaging for RHEL/Centos
#
# Ref. <https://fedoraproject.org/wiki/How_to_create_an_RPM_package>
#
%define DESC Meteorological scripting server

Summary: %{DESC}
Name: fmi-q3

# Note: Version number from release date (i.e. 2010-01-08 -> 10.1.8) 
#
# <1..N>.el5.fmi
#
# 1..N:     release number (of the version)
# el5:      "Enterprise Linux 5.0"
# fmi:      FMI
#
Version: 17.2.3
Release: 1.el7.fmi

License: FMI
Group: Development/Libraries
Vendor: Finnish Meteorological Institute
Packager: mikko.visa@fmi.fi

Source: %{name}.tgz
Buildroot: %{_tmppath}/%{name}-root

#---
# Note: "The Requires and BuildRequires headers are optional. RPM will automatically
#        calculate dependencies for software as it builds the software."
#
BuildRequires:	scons >= 2.3.0
BuildRequires:	gcc-c++ >= 4.8.5
BuildRequires:	libstdc++-devel >= 4.8.5

BuildRequires:  proj-devel >= 4.8.0
BuildRequires:	lua-devel >= 5.1.4

#---
BuildRequires:  boost-devel >= 1.55.0
BuildRequires:	bzip2-devel >= 1.0.6
BuildRequires:	libpng-devel >= 1.5.13
BuildRequires:	libjpeg-turbo-devel >= 1.2.90

#---
# TRON
#       Required for making contours
#
BuildRequires:  smartmet-library-tron >= 17.1.31

#---
# Brainstorm
#       Required for compiling 'fmi-q3-brainstorm'
#
BuildRequires:	smartmet-library-spine-devel >= 17.2.1

#---
%description
%{DESC}

%prep
%setup -q -n %{name}

%build
echo "#define RPM_VERSION \"%{version}-%{release}\"" >> common/include/Versions.h
(cd server && make SCONS_FLAGS=-j4 release)

%install
rm -rf %{buildroot}
#
install -d %{buildroot}/usr/lib64
install -m 664 server/libfmi-q3.so %{buildroot}/usr/lib64/
install -d %{buildroot}/etc/q3server
install -m 664 packaging/q3server.conf %{buildroot}/etc/q3server/ 
cat packaging/q3server.conf \
	   | sed -e "s/^log=.*/log=syslog local1/" \
	> %{buildroot}/etc/q3server/q3server.conf
chmod 664 %{buildroot}/etc/q3server/q3server.conf
#
install -d %{buildroot}/usr/share/q3server
install -m 664 packaging/testbed.html %{buildroot}/usr/share/q3server/ 
#
install -d %{buildroot}/usr/share/q3server/fonts
install -m 664 fonts/*.ttf %{buildroot}/usr/share/q3server/fonts/ 
#
install -d %{buildroot}/usr/share/smartmet/plugins
install -m 664 server/q3plugin.so %{buildroot}/usr/share/smartmet/plugins/
install -d %{buildroot}/etc/smartmet/plugins
ln -s /etc/q3server/q3server.conf %{buildroot}/etc/smartmet/plugins/q3plugin.conf

%clean
rm -rf %{buildroot}


#---
# 'fmi-q3-lib'
#
# Server library
%package lib

Summary: %{DESC} (server library)
Group: Development/Libraries

Requires:       proj >= 4.8.0
Requires:       lua >= 5.1.4
Requires:	bzip2-libs >= 1.0.6
Requires:	libpng >= 1.5.13
Requires:	libjpeg-turbo >= 1.2.90
Requires:	smartmet-library-newbase >= 17.2.2

%description lib
%{DESC} (server library)

%files lib
%defattr(-,root,root)
%{_libdir}/libfmi-q3.so


#---
# 'fmi-q3-config'
%package config

Summary: %{DESC} (common files; configuration, fonts etc.)
Group: Development/Libraries

Requires:   fmi-q3-virtual

%description config
%{DESC} (common files; configuration, fonts etc.)

# NOTE: The '%config(noreplace)' keeps changes in a configuration file on an update.
#       Since we now have a separate 'config' subpackage, the admin can remove
#       'fmi-q3-xxx' packages but the config still remains. This is good. Really
#       removing the configuration package should also remove the (possibly modified)
#       configuration files.    --AKa 26-Nov-10
#
%files config
%defattr(-,root,root)
%config(noreplace) %{_sysconfdir}/q3server/*.conf
%{_datadir}/q3server/testbed.html
%{_datadir}/q3server/fonts/*.ttf


#---
# 'fmi-q3-brainstorm'
#
# Plugin for the FMI SmartMet Server architecture.
#
%package brainstorm

Summary: %{DESC} (SmartMet Server plugin)
Group: Development/Libraries

Requires(post): smartmet-server >= 12.11.7-1

Requires:   fmi-q3-lib >= 17.2.3
Requires:   fmi-q3-config >= 17.2.3
Requires:  smartmet-library-spine >= 17.2.1

Provides:   fmi-q3-virtual

%description brainstorm
%{DESC} (SmartMet Server plugin)

%files brainstorm
%defattr(-,root,root)
%{_datadir}/smartmet/plugins/*.so
%{_sysconfdir}/smartmet/plugins/*.conf

#---
%changelog
* Upcoming
- Load addon settings (e.g. fminames database connection data) from config file
- Forcing wiping for data loaded/referenced by repetitive metadata queries (BRAINSTORM-784)
- Sounding data support
* Fri Feb 3 2017 <mikko.visa@fmi.fi>
- Rebuild against Smartmet Server GitHub packages.
* Wed Dec 7 2016 <pertti.kinnia@fmi.fi>
- SWCEDITOR-961: dataquery().; added support for JDay time value (e.g. NOW, TODAY)
- Support for archived data
- Archive cache file e.g. cache=<filepath> can be given in config file as a global setting (not within a track definition)
- Default cache file is /var/smartmet/archivecache/.q3_cache
* Tue Sep 13 2016 <mikko.visa@fmi.fi>
- Fix BRAINSTORM-706 by PKinnia
* Wed Mar 23 2016 <pertti.kinnia@fmi.fi>
- Added q2 smarttools macros PEEKXY and PEEKXY3
- Fixed handling of height=true
* Mon Feb 29 2016 <pertti.kinnia@fmi.fi>
- Matrix latlon indexing uses the original (nonnative) newbase grid instead of creating a new grid for each call
* Tue Feb 23 2016 <pertti.kinnia@fmi.fi>
- Fixed other bugs (missing labels or labels too near each other) in contour label positioning
* Fri Feb 19 2016 <pertti.kinnia@fmi.fi>
- Fixed bug in calculating contour label positions/distances (used data/grid coordinates instead of px)
* Wed Feb 17 2016 <pertti.kinnia@fmi.fi>
- Using q2 contour smoothening
* Tue Oct 27 2015 Mikko Visa <mikko.visa@fmi.fi>
- Added dataquery().imagequery() aka dataquery().picturequery()
- SWCEDITOR-153; Return empty values for missing parameters
- Missing values are always returned for point data query with nonnative validtime (or level). Fixed the result matrix to contain equal number of missing values as the number of stations/locations addressed by the query; before the matrix's size was equal to the total number of stations/locations in the querydata.
* Thu Oct 15 2015 Mikko Visa <mikko.visa@fmi.fi>
- Fixed bug in contouring. Egdes were deleted before contours were extracted from the geometries; thus contours were errorneously generated to the grid edges
* Wed Oct 14 2015 Mikko Visa <mikko.visa@fmi.fi>
- tron api changed, using FmiBuilder
- Add WWI data to config file.
- added 'datanames' (i.e. station names) to the set of metadata fields to be queried for point data
- fixed reference to 'getparamid' global
- Add SYNOP obs to config file.
* Wed May 13 2015 Mikko Visa <mikko.visa@fmi.fi>
- BRAINSTORM-466 fix #4; metadataquery() now returns pressurelevel and hybridlevel datas for origintimes having ground data
* Thu May 7 2015 Mikko Visa <mikko.visa@fmi.fi>
- utilities.lua fix #3, BRAINSTORM-466
* Wed Apr 22 2015 Mikko Visa <mikko.visa@fmi.fi>
- utilities.lua fix #2, BRAINSTORM-466
* Tue Apr 21 2015 Mikko Visa <mikko.visa@fmi.fi>
- utilities.lua fix, BRAINSTORM-466
* Mon Apr 20 2015 Mikko Visa <mikko.visa@fmi.fi>
- Rebuild with dynamically linked newbase
* Wed May 7 2014 Mikko Visa <mikko.visa@fmi.fi>
- Remove unused soundingindex from configuration.
- Import latest config changes from production server to SVN.
- BRAINSTORM-333; edge -tarkastelun ohitus contouroinnin nopeuttamiseksi
- BRAINSTORM-334; contour labelien desimaalien asetus
- LENTOSAA-835 Suhteellisen kosteuden taustakuva ei piirry oikein Q3:lta
- Tron API changed
* Mon Feb 17 2014 Mikko Visa <mikko.visa@fmi.fi>
- BRAINSTORM-331 Request status code fix
- BRAINSTORM-332 Wind U/V parameters messed up
* Tue Feb 4 2014 Mikko Visa <mikko.visa@fmi.fi>
- LENTOSAA-835 Humidity fixes
- Take surface x/y ratio into account when setting contour fill pattern transformation matrix's scale
- Set contour fill pattern transformation matrix's scale
* Tue Jan 14 2014 <pertti.kinnia@fmi.fi>
- Brainstorm API with HTTP classes (BRAINSTORM-316), BRAINSTORM-320
* Thu Jan 10 2013 <mikko.visa@fmi.fi>
- BRAINSTORM-242 Build again new Brainstorm API + few bug fixes in building
* Fri Dec 14 2012 <mikko.visa@fmi.fi>
- BRAINSTORM-224 Do smoothing only once
- BRAINSTORM-225 Use tron hints
- BRAINSTORM-231 Corrected error in grid latlon-indexing
- BRAINSTORM-232 Q3 missing values
- BRAINSTORM-233 Height value query support
- BRAINSTORM-239 Q3 height parameter not matching newbase
* Tue Mar 27 2012 <mikko.visa@fmi.fi>
- Build against more recent newbase
* Wed Mar 14 2012 <pertti.kinnia@fmi.fi>
- Fix several issue: BRAINSTORM-220, BRAINSTORM-221, BRAINSTORM-222, BRAINSTORM-223
* Thu Mar 1 2012 <pertti.kinnia@fmi.fi>
- Use multithreaded version of newbase, several other bugfixes and dependency updates
* Tue Feb 14 2012 <pertti.kinnia@fmi.fi>
- Debug output release
* Thu Feb 9 2012 <pertti.kinnia@fmi.fi>
- Force cast of cairocontext -parameter to the right type
* Wed Feb 8 2012 <pertti.kinnia@fmi.fi>
- Still more bug fixes, see JIRA for more details.
* Fri Nov 18 2011 <pertti.kinnia@fmi.fi>
- Lots of bug fixes and optimizations, see JIRA for more details.
* Thu Dec 23 2010 <asko.kauppi@fmi.fi>
- CRITICAL FIX: loading of addon libraries (climateguide, fminames, newcairo) as Brainstorm plugin.
* Tue Dec 21 2010 <asko.kauppi@fmi.fi>
- CRITICAL FIX: Jira 120 (time interpolation on pressure/flight surfaces)
- Internal changes in the parameter name classes (ApiScalarParam introduced; role of NA_Param now clear)
* Fri Dec 17 2010 <asko.kauppi@fmi.fi>
- No change for q3 server.
* Fri Dec 10 2010 <asko.kauppi@fmi.fi>
- CRITICAL FIX: Jira 124 (binary output = SmartMet integration).
- Packaging fix: 'postun'->'preun' for fmi-q3-mongoose (should uninstall without hicks now).
- FIX: Jira 121 fixed (problems with levels - one-line fix).
* Thu Dec 9 2010 <asko.kauppi@fmi.fi>
- FIX: Jira 120 (use of flight levels).
* Wed Dec 8 2010 <asko.kauppi@fmi.fi>
- CRITICAL FIX: Jira 119 (range of degree interpolation).
* Thu Nov 25 2010 <asko.kauppi@fmi.fi>
- Revised packaging.
* Tue Nov 23 2010 <asko.kauppi@fmi.fi>
- Pressure level interpolation fixed - ready to start testing again.
- Addons packaged separately.
* Fri Nov 19 2010 <asko.kauppi@fmi.fi>
- ATZ function replacing height levels
* Thu Nov 18 2010 <asko.kauppi@fmi.fi>
- Package name change: 'fmi-q3server' -> 'fmi-q3'
- Tron (curve making) into separate 'fmicurves' addon
* Wed Nov 17 2010 <asko.kauppi@fmi.fi>
- MAJOR FIX: time interpolated values correctly calculated
* Tue Nov 16 2010 <asko.kauppi@fmi.fi>
- Fminames as an external, loadable module (not built-in).
* Tue Nov 2 2010 <asko.kauppi@fmi.fi>
- MAJOR INTERNAL REVISE. Interpolation now done using q3 (not Newbase).
* Fri Sep 17 2010 <asko.kauppi@fmi.fi>
- CRITICAL FIX: Jira 107 (problems reading derived params N, RR, FOG, ...)
* Thu Sep 16 2010 <asko.kauppi@fmi.fi>
- Fixed using param '.N' with material having stand-alone 'N:79'
* Wed Sep 15 2010 <asko.kauppi@fmi.fi>
- Cairo graphics to use ARGB32 format; background is transparent by default ('.background' option added to surface creation).
* Tue Sep 14 2010 <asko.kauppi@fmi.fi>
- Fixed 'grad' return values to be 'unit/m' (as in the documentation); was 'unit/km'
* Thu Sep 9 2010 <asko.kauppi@fmi.fi>
- Fixed Jira#102. Filtering material with 'params='N'' (when 'N:79' is a real param)
- Allowing 'N:79' etc. to exist either as real params or within bitfield combo.
* Wed Sep 8 2010 <asko.kauppi@fmi.fi>
- Ability to load TTF fonts on demand (i.e. Synop symbols)
- Fixed (Jira #98): NOW values when minutes were 30..59
* Tue Sep 7 2010 <asko.kauppi@fmi.fi>
- Fixed: interpolation of WD,WS (and other derivative parameters)
- Feature: JSONP server interface added
* Fri Sep 3 2010 <asko.kauppi@fmi.fi>
- BUGFIX (not in Jira): handling of pressure levels, hybrid levels (when interpolating pressure data)
- CRITICAL BUGFIX: WD, WS, WIND and other derived parameter handling fixed (was messed up in June!).
* Tue Jun 29 2010 <asko.kauppi@fmi.fi>
- MAJOR REVISE of internal parameter handling - testing needed.
- API CHANGE: '.params' and '.times' must now be given table values (was promoting to table automatically earlier) 
* Thu Jun 24 2010 <asko.kauppi@fmi.fi>
- CCCRP bug fix: returning of average data WITH times ({ {jday,val}, ... }); was just values
* Mon Jun 21 2010 <asko.kauppi@fmi.fi>
- CCCRP changes from Petri Vuorio.
- Changed 'STANDALONE' -> 'METQU' in compilation flags (internal change).
- No '/q3' required in the URL.
- Disabled access to server's file system (SAFETY ISSUE -- UPDATE TO THIS VERSION)
- Added 'addon-areamasks' subpackage
* Thu Jun 3 2010 <asko.kauppi@fmi.fi>
- CCCRP initial version ready for tests.
- Fixed SSE NAN-detection bug (failed optimization) + 'min()' and 'max()' when between matrix and scalar. 
* Thu May 27 2010 <asko.kauppi@fmi.fi>
- Cross sections new API & tested & documented
- jday+-number takes number as hours (was: seconds). Done for NOW+3 etc.
* Fri May 14 2010 <asko.kauppi@fmi.fi>
- Gaussian functions and 'crit_percent()', 'crit_limit()' added (for Annakaisa and Juha K.)
* Thu May 6 2010 <asko.kauppi@fmi.fi>
- Older (long) changelog removed.
