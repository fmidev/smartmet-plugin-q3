#
# lua-newcairo.spec
#
# Ref. <https://fedoraproject.org/wiki/How_to_create_an_RPM_package>
#
%define DESC Cairo binding for Lua 5.3

Summary: %{DESC}
Name: lua-newcairo

# Note: Version number from release date (i.e. 2010-01-08 -> 10.1.8) 
#
# <1..N>.el5.fmi
#
# 1..N:     release number (of the version)
# el5:      "Enterprise Linux 5.0"
# fmi:      FMI
#
Version: 24.8.8
Release: 1.el8.fmi

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
BuildRequires:	gcc-c++ >= 4.8.5
BuildRequires:	libstdc++-devel >= 4.8.5

BuildRequires:	lua-devel >= 5.3.4
Requires:       lua >= 5.3.4

BuildRequires:  cairo-devel >= 1.15.12
Requires:       cairo >= 1.15.12

%description
%{DESC}

#---
%prep
%setup -q -n %{name}

#%global debug_package %{nil}

# Note: Only one '%build' per RPM. We must build both variants at once.
#
%build
make full VARIANT=release
make server VARIANT=release CUSTOM_FONT_PATH=%{_datadir}/q3server/fonts

#---
# Note: The destination for CentOS Lua addons can be found by:
#       'lua -e "require 'a'"'  
#       (see the error message for 'a.so' that was not found)
#
#
%install
rm -rf %{buildroot}
install -d %{buildroot}/usr/lib64/lua/5.3
install -m 664 lua53-newcairo.so %{buildroot}/usr/lib64/lua/5.3/newcairo.so
install -d %{buildroot}/usr/lib64/lua/5.3/q3server-addons
install -m 664 lua53-newcairo-server.so %{buildroot}/usr/lib64/lua/5.3/q3server-addons/newcairo.so

%clean
rm -rf %{buildroot}

%files
%defattr(-,root,root)
%{_libdir}/lua/5.3/newcairo.so


#---
# Separate '-q3' (sandbox) subpackage (installs in server path).
#
%package q3

Summary: %{DESC} (for q3 server)
Group: Development/Libraries

%description q3
%{DESCRIPTION} (for q3 server)

%files q3
%defattr(-,root,root)
%{_libdir}/lua/5.3/q3server-addons/newcairo.so


#---
%changelog
* Thu Aug  8 2024 Pertti Kinnia <pertti.kinnia@fmi.fi> - 24.8.8-1.el8.fmi
- c++17 rebuild
* Thu Mar 14 2024 Pertti Kinnia <pertti.kinnia@fmi.fi> - 24.3.14-1.el8.fmi
- Repackaged for rhel8, lua related changes (rhel7 lua 5.1 vs 5.3); BRAINSTORM-2608
* Wed Oct 19 2022 <pertti.kinnia@fmi.fi>
- Catch cairo exception(s) when e.g surface is empty when writing surface to png stream; BRAINSTORM-2437
* Thu Apr 20 2017 <mikko.visa@fmi.fi>
- Rebuild, big changes in Q3
* Fri Feb 3 2017 <mikko.visa@fmi.fi>
- Rebuild, GitHub packages, bump cairo versions
* Fri Jan 23 2015 <mikko.visa@fmi.fi>
- RHEL7 rebuild
* Wed Nov 24 2010 <asko.kauppi@fmi.fi>
- Started the package (earlier by the name 'fmi-metqu-addon-newcairo')
