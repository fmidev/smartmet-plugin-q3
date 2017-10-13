#
# lua-strict.spec
#
%define DESC=Lua addon allowing '-lstrict' checking for uninitialized variable names.

Summary: %{DESC}
Name: lua-strict

# Note: Version number from release date (i.e. 2010-01-08 -> 10.1.8) 
#
# <1..N>.el5.fmi
#
# 1..N:     release number (of the version)
# el5:      "Enterprise Linux 5.0"
# fmi:      FMI
#
Version: 5.1
Release: 2.el7.fmi

License: MIT
Group: Development/Libraries
Vendor: PUC-Rio
Packager: mikko.visa@fmi.fi

Source: %{name}.tgz
Buildroot: %{_tmppath}/%{name}-root

Requires:       lua >= 5.1.4-4.1

%description
%{DESC}

#---
%prep
%setup -q -n %{name}

# Note: Nothing to compile.
#
%build

#---
# Note: The destination for CentOS Lua addons can be found by:
#       'lua -e "require 'a'"'  
#       (see the error message for 'a.so' that was not found)
#
%install
rm -rf %{buildroot}
install -d %{buildroot}/usr/share/lua/5.1
install -m 664 strict.lua %{buildroot}/usr/share/lua/5.1/

%clean
rm -rf %{buildroot}

%files
%defattr(-,root,root)
%{_datadir}/lua/5.1/strict.lua


#---
%changelog
* Mon Nov 29 2010 <asko.kauppi@fmi.fi>
- Made this into a separate package (out from 'fmi-metqu.spec')
