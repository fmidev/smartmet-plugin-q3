#
# fmi-areamask.spec
#
# Packaging for Centos 5.4
#
# Ref. <https://fedoraproject.org/wiki/How_to_create_an_RPM_package>
#
%define DESC Location limits for Finnish areas

Summary: %{DESC}
Name: fmi-areamask

# Note: Version number from release date (i.e. 2010-01-08 -> 10.1.8) 
#
# <1..N>.el5.fmi
#
# 1..N:     release number (of the version)
# el5:      "Enterprise Linux 5.0"
# fmi:      FMI
#
Version: 24.3.14
Release: 1.el8.fmi

License: FMI
Group: Development/Libraries
Vendor: Finnish Meteorological Institute
Packager: mikko.visa@fmi.fi

Source: %{name}.tgz
Buildroot: %{_tmppath}/%{name}-root

Requires:       lua >= 5.3.4

%description
%{DESC}

#---
%prep
%setup -q -n %{name}

%global debug_package %{nil}

# Just Lua files - no compilation required
#
%build

#---
# Note: We use the same file for both '-metqu' and '-q3' variants.
#       Copy is being made (and not a symbolic link) so that the server would not end
#       up requiring 'metqu' command line tool.
#
%install
rm -rf %{buildroot}
install -d %{buildroot}/usr/share/lua/5.3/areamask/
install -m 664 *.lua %{buildroot}/usr/share/lua/5.3/areamask/
install -d %{buildroot}/usr/share/lua/5.3/q3server-addons/areamask/
install -m 664 *.lua %{buildroot}/usr/share/lua/5.3/q3server-addons/areamask/

%clean
rm -rf %{buildroot}

%files
%{_datadir}/lua/5.3/areamask/*.lua
%{_datadir}/lua/5.3/q3server-addons/areamask/*.lua

# No files for the parent package (just for building)

#---
# Package for 'metqu' command line tool
#
#%package metqu

#Summary: %{DESC} (for Metqu command line tool)
#Group: Development/Libraries

#Requires:   metqu-virtual

#%description metqu
#%{DESC} (for Metqu command line tool)

#%files metqu
#%defattr(-,root,root)
#%{_datadir}/lua/5.1/areamask/*.lua


#---
# Package for 'q3' server
#
%package q3

Summary: %{DESC} (for q3 server)
Group: Development/Libraries

#
Requires:   fmi-q3-virtual

%description q3
%{DESC} (for q3 server)

%files q3
%defattr(-,root,root)
%{_datadir}/lua/5.3/q3server-addons/areamask/*.lua


#---
%changelog
* Fri Feb 3 2017 <mikko.visa@fmi.fi>
- Rebuild, GitHub packages and cleanup
* Wed Nov 24 2010 <asko.kauppi@fmi.fi>
- Packaging revise (each addon as a separate package).

