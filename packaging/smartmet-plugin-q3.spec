%define DIRNAME q3
%define SPECNAME smartmet-plugin-%{DIRNAME}
Summary: SmartMet q3 plugin
Name: %{SPECNAME}
Version: 20.8.14
Release: 1.el7.fmi
License: MIT
Group: SmartMet/Plugins
URL: https://github.com/fmidev/smartmet-plugin-q3
Vendor: Finnish Meteorological Institute
Source: %{name}.tgz
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

BuildRequires:	scons
BuildRequires:	gcc-c++ >= 4.8.5
BuildRequires:	libstdc++-devel >= 4.8.5
BuildRequires:  proj-devel >= 4.8.0
BuildRequires:	lua-devel >= 5.1.4
BuildRequires:  boost169-devel
BuildRequires:	bzip2-devel >= 1.0.6
BuildRequires:	libpng-devel >= 1.5.13
BuildRequires:	libjpeg-turbo-devel >= 1.2.90
BuildRequires:  smartmet-library-tron >= 20.4.18
BuildRequires:	smartmet-library-spine-devel >= 20.8.11
Requires:       proj >= 4.8.0
Requires:       lua >= 5.1.4
Requires:       bzip2-libs >= 1.0.6
Requires:       libpng >= 1.5.13
Requires:       libjpeg-turbo >= 1.2.90
Requires:       smartmet-library-newbase >= 20.6.16
Requires:       smartmet-library-spine >= 20.8.11
Requires:       smartmet-server >= 20.8.10
Obsoletes:      fmi-q3-lib
Obsoletes:      fmi-q3-config
Obsoletes:      fmi-q3-brainstorm

%description
SmartMet q3 plugin

%prep
%setup -q -n %{SPECNAME}

%build
echo "#define RPM_VERSION \"%{version}-%{release}\"" >> common/include/Versions.h
(cd server && make SCONS_FLAGS=-j4 release)

%install
rm -rf %{buildroot}
install -d %{buildroot}/usr/lib64
install -m 775 server/libfmi-q3.so %{buildroot}/usr/lib64/
install -d %{buildroot}/etc/smartmet/plugins
install -m 664 packaging/q3plugin.conf %{buildroot}/etc/smartmet/plugins/ 
install -d %{buildroot}/usr/share/q3plugin
install -m 664 packaging/testbed.html %{buildroot}/usr/share/q3plugin/ 
install -d %{buildroot}/usr/share/q3plugin/fonts
install -m 664 fonts/*.ttf %{buildroot}/usr/share/q3plugin/fonts/ 
install -d %{buildroot}/usr/share/smartmet/plugins
install -m 775 server/q3.so %{buildroot}/usr/share/smartmet/plugins/
install -d %{buildroot}/etc/smartmet/plugins

%clean
rm -rf %{buildroot}

%files
%defattr(-,root,root)
%{_libdir}/libfmi-q3.so
%{_datadir}/q3plugin/testbed.html
%{_datadir}/q3plugin/fonts/*.ttf
%{_datadir}/smartmet/plugins/*.so
%{_sysconfdir}/smartmet/plugins/*.conf
%config(noreplace) %{_sysconfdir}/smartmet/plugins/q3plugin.conf

%changelog
* Fri Aug 14 2020 Pertti Kinnia <pertti.kinnia@fmi.fi> - 20.8.14-1.el7.fmi
- dataquery(): use 'SoundingLevel' leveltype if leveltype is not provided (defaulting to 'Ground') and track has sounding data; BS-1892
* Mon May 18 2020 Pertti Kinnia <pertti.kinnia@fmi.fi> - 20.5.18-1.el7.fmi
- Use default precision (query parameter 'decimals') 0 instead of -1; exponential presentation causes data loss e.g. for WeatherNumber
* Fri May  8 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.5.8-1.el7.fmi
- Disabled LOG_OK, LOG_DEBUG, LOG_STAT and LOG_TIMING output
* Sat Apr 18 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.4.18-1.el7.fmi
- Upgraded to Boost 1.69
* Thu Apr 17 2020 Pertti Kinnia <pertti.kinnia@fmi.fi> - 20.4.17-1.el7.fmi
- Fixed bug with sounding data and missing parameters in query when removing return data having missing pressure value (BS-1819)
* Thu Apr  2 2020 Pertti Kinnia <pertti.kinnia@fmi.fi> - 20.4.2-1.el7.fmi
- Skip duplicate metadata for sounding data (both hpa=850 and sounding=true matches); BS-1812
* Tue Feb  4 2020 Pertti Kinnia <pertti.kinnia@fmi.fi> - 20.2.4-1.el7.fmi
- Added underscore to character set used to match/split track names for combined tracks (BRAINSTORM-1749)
- New release version
* Mon Feb  3 2020 Pertti Kinnia <pertti.kinnia@fmi.fi> - 20.2.3-1.el7.fmi
- Fix to ground data query, level data could be returned instead when it was never than ground data (BRAINSTORM-1741)
* Wed Nov 20 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.11.20-1.el7.fmi
- Repackaged due to newbase ABI changes
* Thu Oct 31 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.10.31-1.el7.fmi
- Rebuilt due to newbase API/ABI changes
* Thu Sep 26 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.9.26-1.el7.fmi
- Repackaged due to ABI changes
* Tue Apr  2 2019 Pertti Kinnia <pertti.kinnia@fmi.fi> - 19.4.2-1.fmi
- Using ProbabilityThunderstorm2 in weather number calculation if ProbabilityThunderstorm is not available (BRAINSTORM-1557)
* Thu Mar 21 2019 Pertti Kinnia <pertti.kinnia@fmi.fi> - 19.3.21-1.fmi
- Fixed missing value handling in weathernumber calculation
- Fixed GDalArea projection creation (BRAINSTORM-1541, LENTOSAA-1122, PAK-1288)
* Tue Mar  5 2019 Pertti Kinnia <pertti.kinnia@fmi.fi> - 19.3.5-1.fmi
- Fixed weathernumber calculation with native projection data (BS-1489)
- Fixed weathernumber thunder probability classification (BS-1491)
* Mon Dec 17 2018 Pertti Kinnia <pertti.kinnia@fmi.fi> - 18.12.17-1.fmi
- Fixed bug in dataquery().imagequery(), did not pass (empty) locations to querydata()
* Wed Dec  5 2018 Pertti Kinnia <pertti.kinnia@fmi.fi> - 18.12.5-1.fmi
- dataquery() returns data for given locations (if any), and returns data as a matrix (instead of a table) when querying a single parameter (for convenience only)
* Tue Nov 27 2018 Pertti Kinnia <pertti.kinnia@fmi.fi> - 18.11.27-1.fmi
- Added weathernumber calculation (PAK-1288)
* Mon Oct  1 2018 Pertti Kinnia <pertti.kinnia@fmi.fi> - 18.10.1-1.fmi
- Catch exceptions thrown when loading querydata time and level information (BS-1375)
* Thu Sep 27 2018 Pertti Kinnia <pertti.kinnia@fmi.fi> - 18.9.27-1.fmi
- Fixed crash when loading broken querydata file (unhandled exception raised by newbase)
* Thu Jun 21 2018 Pertti Kinnia <pertti.kinnia@fmi.fi> - 18.6.21-1.fmi
- rpm build changed to enable debuginfo generation (BS-1206)
* Fri Jun  1 2018 Pertti Kinnia <pertti.kinnia@fmi.fi> - 18.6.1-1.fmi
- Added raw index '.mt_relative_uv' and metadata query field 'relativeuvs' for obtaining wind U/V component reference
* Wed May 23 2018 Pertti Kinnia <pertti.kinnia@fmi.fi> - 18.5.23-1.fmi
- Repackaged due to newbase ABI fix
* Wed May 23 2018 Pertti Kinnia <pertti.kinnia@fmi.fi> - 18.5.23-1.fmi
- Use model's U/V reference information to control whether U and V are rotated when reprojecting
* Mon Apr 16 2018 Pertti Kinnia <pertti.kinnia@fmi.fi> - 18.4.16-1.fmi
- New release version (boost 1.66, newbase fixes)
* Tue Jan  9 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.1.9-1.el7.fmi
- Fix to GDAL projection handling
* Thu Sep 28 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.9.28-1.el7.fmi
- Fixed version number to be of form YY.MM.DD
* Thu Apr 27 2017 <mikko.visa@fmi.fi>
- Remove traces of climateguide stuff, was never used
* Tue Apr 25 2017 <mikko.visa@fmi.fi>
- Lots of internal code changes and cleanup, see BRAINSTORM-822
* Thu Apr 20 2017 <mikko.visa@fmi.fi>
- Forcing wiping for data loaded/referenced by repetitive metadata queries (BRAINSTORM-784)
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
