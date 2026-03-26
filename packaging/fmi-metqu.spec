#
# fmi-metqu.spec
#
# Metqu command line tool for running q3 scripts locally + addon packages
#
%define DESC Meteorological scripting (command line tool)

Summary: %{DESC}
Name: fmi-metqu

# Note: Version number from release date (i.e. 2010-01-08 -> 10.1.8) 
#
# <1..N>.el5.fmi
#
# 1..N:     release number (of the version)
# el5:      "Enterprise Linux 5.0"
# fmi:      FMI
#
Version: 18.1.9
Release: 1%{?dist}.fmi

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
BuildRequires:	gcc-c++ >= 4.8.2
BuildRequires:	libstdc++-devel >= 4.8.2

BuildRequires:  proj-devel >= 4.8.0
Requires:       proj >= 4.8.0

BuildRequires:	lua-devel >= 5.1.4
Requires:       lua >= 5.1.4

Requires:       lua-strict
Requires:       libsmartmet-newbase >= 15.4.15-1

#---
# NEWBASE
#       >= 10.3.26-1 to have a bugfix for handling malformed hybrid data (see FMI Jira#77)
#       >= 9.4.7-1 to avoid const casts.  --AKa 7-Apr-2009
#
# Note: FMI 'smartmet' libraries are linked to statically (convention by KAP developers).
#       This is why we don't give any runtime 'Requires' packages for them (since no
#       runtime files are required).    --AKa 22-Feb-10
#
BuildRequires:  boost-devel >= 1.55.0

BuildRequires:	bzip2-devel >= 1.0.6
Requires:	bzip2-libs >= 1.0.6

BuildRequires:	libpng-devel >= 1.5.13
Requires:	libpng >= 1.5.13

BuildRequires:	libjpeg-turbo-devel >= 1.2.90
Requires:	libjpeg-turbo >= 1.2.90

# Name until 5-May-10
#
Obsoletes:      fmi-q3-standalone

# This is a marker that can be used for "having some Metqu installed", in case
# we do 'metqu-nosql' package (excluding Newbase, thus allowing for open sourcing)
# in the future.    AKa 25-Nov-10
#
Provides:       metqu-virtual

%description
%{DESC}

#---
%prep
%setup -q -n %{name}

%build
echo "#define RPM_VERSION \"%{version}-%{release}\"" >> common/include/Versions.h
(cd metqu && make SCONS_FLAGS=-j4 release)

#---
# Installs: 'metqu' command for launching Lua with the 'metqu' library preloaded
#
# Note: The destination for CentOS Lua addons can be found by:
#       'lua -e "require 'a'"'  
#       (see the error message for 'a.so' that was not found)
#
# Note: Using '-l strict' is a good deed with Lua (catches some spelling mistakes).
#       CentOS does not seem to have 'strict.lua' preinstalled so we dare to place
#       it in!
#
%install
rm -rf %{buildroot}
install -d %{buildroot}/usr/lib64/lua/5.1
install -d %{buildroot}/usr/bin
install -m 664 metqu/lua51-metqu.so %{buildroot}/usr/lib64/lua/5.1/metqu.so
install -m 775 packaging/metqu %{buildroot}/usr/bin/

%clean
rm -rf %{buildroot}

%files
%defattr(-,root,root)
%{_bindir}/metqu
%{_libdir}/lua/5.1/metqu.so


#---
%changelog
* Tue Jan  9 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.1.9-1.el7.fmi
- Fix to GDAL area handling
* Wed Apr 22 2015 Mikko Visa <mikko.visa@fmi.fi>
- utilities.lua fix #2, BRAINSTORM-466
* Tue Apr 21 2015 Mikko Visa <mikko.visa@fmi.fi>
- utilities.lua fix, BRAINSTORM-466
* Mon Apr 20 2015 Mikko Visa <mikko.visa@fmi.fi>
- Rebuild with dynamically linked newbase
* Mon May 5 2014 Mikko Visa <mikko.visa@fmi.fi>
- See fmi-q3 changelog for details
* Tue Feb 4 2014 Mikko Visa <mikko.visa@fmi.fi>
- LENTOSAA-835 Humidity fixes
- Take surface x/y ratio into account when setting contour fill pattern transformation matrix's scale
- Set contour fill pattern transformation matrix's scale
* Tue Jan 14 2014 <pertti.kinnia@fmi.fi>
- BRAINSTORM-320 + few bug fixes in building
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
* Tue Dec 21 2010 <asko.kauppi@fmi.fi>
- CRITICAL FIX: Jira 120 (time interpolation on pressure/flight surfaces)
- Internal changes in the parameter name classes (ApiScalarParam introduced; role of NA_Param now clear)
* Fri Dec 17 2010 <asko.kauppi@fmi.fi>
- CRITICAL FIX: Jira 126 (projections of newly created data).
* Fri Dec 10 2010 <asko.kauppi@fmi.fi>
- CRITICAL FIX: Jira 124 (binary output = SmartMet integration).
- FIX: Jira 121 fixed (problems with levels - one-line fix).
* Thu Dec 9 2010 <asko.kauppi@fmi.fi>
- FIX: Jira 120 (use of flight levels).
* Wed Dec 8 2010 <asko.kauppi@fmi.fi>
- CRITICAL FIX: Jira 119 (range of degree interpolation).
* Mon Nov 29 2010 <asko.kauppi@fmi.fi>
- Added '-nosqd' package.
- Removed 'strict.lua' from the Metqu installation.
* Thu Nov 25 2010 <asko.kauppi@fmi.fi>
- Packaging revise (and name change).
* Tue Nov 23 2010 <asko.kauppi@fmi.fi>
- Pressure level interpolation fixed - ready to start testing again.
- Addons packaged separately.
* Fri Nov 19 2010 <asko.kauppi@fmi.fi>
- ATZ function replacing height levels
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
- Fix to creation of SQD files (was creating "X:n" parameter names, only "X" should be there)
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
- Feature: JSONP server interface added (no effect to command line)
* Fri Sep 3 2010 <asko.kauppi@fmi.fi>
- BUGFIX (not in Jira): handling of pressure levels, hybrid levels (when interpolating pressure data)
- CRITICAL BUGFIX: WD, WS, WIND and other derived parameter handling fixed (was messed up in June!).
- BUGFIX (Jira#87): copying SQD data with combo parameters (i.e. WIND).
* Tue Jun 22 2010 <asko.kauppi@fmi.fi>
- BUGFIX: 32700 in freshly created SQD files (was: nan)
- BUGFIX: Storing of parameter names in SQD files as Latin-1 (was: UTF-8)
* Mon Jun 21 2010 <asko.kauppi@fmi.fi>
- 'make full' instead of 'make standalone' (internal building change)
* Thu Jun 3 2010 <asko.kauppi@fmi.fi>
- Version number update.
* Wed May 5 2010 <asko.kauppi@fmi.fi>
- Separated command line tool to a whole NEW PACKAGE FILE (mirroring changes in the build system)
- Older (long) changelog removed.
- OPEN: How to sync changes affecting both q3server and metqu (need to modify both changelogs?)
