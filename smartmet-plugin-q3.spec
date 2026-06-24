%define DIRNAME q3
%define SPECNAME smartmet-plugin-%{DIRNAME}
Summary: SmartMet q3 plugin
Name: %{SPECNAME}
Version: 26.6.24
Release: 1%{?dist}.fmi
License: MIT
Group: SmartMet/Plugins
URL: https://github.com/fmidev/smartmet-plugin-q3
Vendor: Finnish Meteorological Institute
Source0: %{name}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

# https://fedoraproject.org/wiki/Changes/Broken_RPATH_will_fail_rpmbuild
%global __brp_check_rpaths %{nil}

%if 0%{?rhel} && 0%{rhel} < 9
%define smartmet_boost boost169
%else
%define smartmet_boost boost
%endif

BuildRequires: rpm-build
BuildRequires: gcc-c++
BuildRequires: make
BuildRequires: proj97-devel >= 9.5.1
BuildRequires: luajit-devel >= 2.1.0
BuildRequires: %{smartmet_boost}-devel
BuildRequires: bzip2-devel >= 1.0.6
BuildRequires: libpng-devel >= 1.5.13
BuildRequires: libjpeg-turbo-devel >= 1.2.90
BuildRequires: smartmet-library-tron-devel >= 26.2.4
BuildRequires: smartmet-library-spine-devel >= 26.6.24
Requires: proj97 >= 9.5.1
Requires: luajit >= 2.1.0
Requires: bzip2-libs >= 1.0.6
Requires: libpng >= 1.5.13
Requires: libjpeg-turbo >= 1.2.90
Requires: smartmet-library-newbase >= 26.6.24
Requires: smartmet-library-tron >= 26.2.4
Requires: smartmet-library-spine >= 26.6.24
Requires: smartmet-server >= 26.6.24
Obsoletes: fmi-q3-lib
Obsoletes: fmi-q3-config
Obsoletes: fmi-q3-brainstorm

%description
SmartMet q3 plugin

%prep
%setup -q -n %{SPECNAME}

%build
echo "#define RPM_VERSION \"%{version}-%{release}\"" >> q3/Versions.h
make %{_smp_mflags}

%install
rm -rf %{buildroot}
install -d %{buildroot}%{_datadir}/smartmet/plugins
install -m 775 q3.so %{buildroot}%{_datadir}/smartmet/plugins/
install -d %{buildroot}%{_sysconfdir}/smartmet/plugins
install -m 664 cnf/q3plugin.conf %{buildroot}%{_sysconfdir}/smartmet/plugins/
install -d %{buildroot}%{_datadir}/q3plugin/fonts
install -m 664 fonts/*.ttf %{buildroot}%{_datadir}/q3plugin/fonts/

%clean
rm -rf %{buildroot}

%files
%defattr(-,root,root)
%{_datadir}/q3plugin/fonts/*.ttf
%{_datadir}/smartmet/plugins/*.so
%{_sysconfdir}/smartmet/plugins/*.conf
%config(noreplace) %{_sysconfdir}/smartmet/plugins/q3plugin.conf

%changelog
* Wed Jun 24 2026 Mika Heiskanen <mika.heiskanen@fmi.fi> - 26.6.24-1.fmi
- Mass rebuild
- Use PROJ 9.7
* Wed Apr  2 2026 Mika Heiskanen <mika.heiskanen@fmi.fi> - 25.9.30-1.fmi
- Refactored build to use Makefile instead of scons
- Merged all source into single q3/ directory
- Switched from lua to luajit for 2x performance
- No longer builds separate libfmi-q3.so library
* Tue Sep 30 2025 Mika Heiskanen <mika.heiskanen@fmi.fi> - 25.9.30-1.fmi
- Repackaged due to ABI changes
* Tue Sep  2 2025 Andris Pavēnis <andris.pavenis@fmi.fi> - 25.9.2-2
- Repackage due to smartmet-library-spine ABI changes
* Tue Apr 29 2025 Pertti Kinnia <pertti.kinnia@fmi.fi> - 25.4.29-1.el8.fmi
- Added tron library runtime dependency and fixed build dependency to devel package
* Mon Mar  3 2025 Pertti Kinnia <pertti.kinnia@fmi.fi> - 25.3.3-1.el8.fmi
- Repackaged due to geos/proj/gdal update
* Thu Jan  9 2025 Pertti Kinnia <pertti.kinnia@fmi.fi> - 25.1.9-1.el8.fmi
- Fixed cross() to call error() on nil result since instead of calling luaL_error() fetching data for missing parameter now returns nil matrix (BRAINSTORM-3104)
* Wed Jan  8 2025 Pertti Kinnia <pertti.kinnia@fmi.fi> - 25.1.8-1.el8.fmi
- Do not call luaL_error() on missing parameter since for some reason (some stack issue ?) it crashes with luajit/rhel8 (BRAINSTORM-3099)
- Added open/init for luajit library (and for the rest of the libs from luajit's lib_init.c)
* Tue Dec  3 2024 Pertti Kinnia <pertti.kinnia@fmi.fi> - 24.12.3-1.el8.fmi
- Using luajit. Changes made to master (rhel7) source and pushed to branch master-PAK-4164-luajit
* Wed Jan 31 2024 Pertti Kinnia <pertti.kinnia@fmi.fi> - 24.1.31-1.el7.fmi
- Allow missing point data coordinates (PAK-3114). Updated geos and gdal include paths
* Thu Jan 19 2023 Pertti Kinnia <pertti.kinnia@fmi.fi> - 23.1.19-1.el7.fmi
- areamask queries were slow in production. Removed lua gc settings (more aggressive, now back to using defaults) should they affect througput (BRAINSTORM-2522)
* Mon Jan 16 2023 Pertti Kinnia <pertti.kinnia@fmi.fi> - 23.1.16-1.el7.fmi
- MAXZ speedup by fetching Z outside grids_by_level loop (BRAINSTORM-2515). Some aviation lua files (listed in issue) were also updated; gridsize was not set, default 50x50 was used
* Fri Nov 25 2022 Pertti Kinnia <pertti.kinnia@fmi.fi> - 22.11.25-1.el7.fmi
- Use r,err (nil,err) return when searching for matching data and no data is found. Call to luaL_error results to longjmp() to be called (at least when lua is build with c -compiler) and destructors are not called
* Thu Nov 10 2022 Pertti Kinnia <pertti.kinnia@fmi.fi> - 22.11.10-1.el7.fmi
- Set lua gc pause and step multiplier, still constant growth in memory usage
* Tue Nov  8 2022 Pertti Kinnia <pertti.kinnia@fmi.fi> - 22.11.8-1.el7.fmi
- Fixed memory leak (BRAINSTORM-1460) and added number_to_keep config setting (BRAINSTORM-2426)
* Wed Oct 19 2022 Pertti Kinnia <pertti.kinnia@fmi.fi> - 22.10.19-1.el7.fmi
- Do not log use of track aliases, too much output. Logging must be controlled by a config setting or by a query which sets logging on (or off) if alias usage needs to be logged
* Fri Oct  7 2022 Pertti Kinnia <pertti.kinnia@fmi.fi> - 22.10.7-1.el7.fmi
- Support for multiple track aliases; BRAINSTORM-2257
- Logging track alias when track is used, not when initializing the query
* Wed Aug 24 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.8.24-1.el7.fmi
- Fixed resolution calculation in SQD_Data
* Thu May  5 2022 Pertti Kinnia <pertti.kinnia@fmi.fi> - 22.5.5-1.el7.fmi
- Added track alias handling to forward queries to another track (e.g. HIR to MEPS); BRAINSTORM-2257
- Added geos/geom/Coordinate.inl include for contouring (otherwise undefined symbols). Updated geos and gdal include paths
* Fri Oct 15 2021 Pertti Kinnia <pertti.kinnia@fmi.fi> - 21.10.15-1.el7.fmi
- Added more exception handling for exceptions thrown by newbase (BRAINSTORM-2180)
* Tue Aug 31 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.8.31-1.el7.fmi
- Repackaged due to Spine ABI changes
* Tue Aug 17 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.8.17-1.el7.fmi
- Use the new shutdown API
* Mon Jun 21 2021 Pertti Kinnia <pertti.kinnia@fmi.fi> - 21.6.21-1.el7.fmi
- Catch std::exception thrown by newbase and coordinate transformation (BS-2092)
- nuke LatLon object if it's constructor throws due to invalid coordinates (BS-2100); otherwise crashes in lua gc
* Wed May 12 2021 Pertti Kinnia <pertti.kinnia@fmi.fi> - 21.5.12-1.el7.fmi
- Upgrade to proj72 and geos39. Newbase and tron api changes.
* Wed Nov 11 2020 Pertti Kinnia <pertti.kinnia@fmi.fi> - 20.11.11-1.el7.fmi
- Take leveltype into account when using relative origintime; BS-1973
* Fri Aug 21 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.8.21-1.el7.fmi
- Upgrade to fmt 6.2
* Fri Aug 14 2020 Pertti Kinnia <pertti.kinnia@fmi.fi> - 20.8.14-1.el7.fmi
- dataquery(): use 'SoundingLevel' leveltype if leveltype is not provided (defaulting to 'Ground') and track has sounding data; BS-1892
* Mon May 18 2020 Pertti Kinnia <pertti.kinnia@fmi.fi> - 20.5.18-1.el7.fmi
- Use default precision (query parameter 'decimals') 0 instead of -1; exponential presentation causes data loss e.g. for WeatherNumber
* Fri May  8 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.5.8-1.el7.fmi
- Disabled LOG_OK, LOG_DEBUG, LOG_STAT and LOG_TIMING output
* Sat Apr 18 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.4.18-1.el7.fmi
- Upgraded to Boost 1.69
* Thu Apr  2 2020 Pertti Kinnia <pertti.kinnia@fmi.fi> - 20.4.2-1.el7.fmi
- Skip duplicate metadata for sounding data (both hpa=850 and sounding=true matches); BS-1812
* Tue Feb  4 2020 Pertti Kinnia <pertti.kinnia@fmi.fi> - 20.2.4-1.el7.fmi
- Added underscore to character set used to match/split track names for combined tracks (BRAINSTORM-1749)
* Mon Feb  3 2020 Pertti Kinnia <pertti.kinnia@fmi.fi> - 20.2.3-1.el7.fmi
- Fix to ground data query, level data could be returned instead when it was never than ground data (BRAINSTORM-1741)
* Wed Nov 20 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.11.20-1.el7.fmi
- Repackaged due to newbase ABI changes
* Thu Oct 31 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.10.31-1.el7.fmi
- Rebuilt due to newbase API/ABI changes
* Thu Sep 26 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.9.26-1.el7.fmi
- Repackaged due to ABI changes
* Fri Jun  1 2018 Pertti Kinnia <pertti.kinnia@fmi.fi> - 18.6.1-1.fmi
- Added raw index '.mt_relative_uv' and metadata query field 'relativeuvs' for obtaining wind U/V component reference
