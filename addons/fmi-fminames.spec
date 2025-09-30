#
# fmi-fminames.spec
#
# Ref. <https://fedoraproject.org/wiki/How_to_create_an_RPM_package>
#
%define DESC Location names database binding

Summary: %{DESC}
Name: fmi-fminames

# Note: Version number from release date (i.e. 2010-01-08 -> 10.1.8) 
#
# <1..N>.el5.fmi
#
# 1..N:     release number (of the version)
# el5:      "Enterprise Linux 5.0"
# fmi:      FMI
#
Version: 17.2.3
Release: 1%{dist}.fmi

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

BuildRequires:	lua-devel >= 5.1.4
Requires:       lua >= 5.1.4

# Note: MySQL++ 3.0.0 has API changes, making FmiNames work EITHER with 2.x or 3.0.
#       --AKa 10-May-10
#
BuildRequires:	smartmet-library-locus >= 17.2.3-1
BuildRequires:	mysql++-devel >= 3.1.0

%description
%{DESC}

#---
%prep
%setup -q -n %{name}

# Note: The same binary is used for both command line ('metqu') and server ('q3').
#       We make a symbolic link in the '-server' package to the general Lua loadable module.
#
%build
make TOOLS=tools

#---
# Note: We use the same file for both '-metqu' and '-q3' variants. Some packages may
#       need separate files (i.e. for blocking security risks in server use).
#       Copy is being made (and not a symbolic link) so that the server would not end
#       up requiring 'metqu' command line tool.
#
%install
rm -rf %{buildroot}
install -d %{buildroot}/usr/lib64/lua/5.1
install -m 664 lua51-fminames.so %{buildroot}/usr/lib64/lua/5.1/fminames.so
install -d %{buildroot}/usr/lib64/lua/5.1/q3server-addons
install -m 664 lua51-fminames.so %{buildroot}/usr/lib64/lua/5.1/q3server-addons/fminames.so

%clean
rm -rf %{buildroot}

# No files for the parent package (just for building)

#---
# Package for 'metqu' command line tool
#
%package metqu

Summary: %{DESC} (for Metqu command line tool)
Group: Development/Libraries

#Requires:   metqu-virtual >= package metqu-virtual is not installed metqu-virtual is not installed

%description metqu
%{DESC} (for Metqu command line tool)

%files metqu
%defattr(-,root,root)
%{_libdir}/lua/5.1/fminames.so


#---
# Package for 'q3' server
#
%package q3

Summary: %{DESC} (for q3 server)
Group: Development/Libraries

#
Requires:   fmi-q3-virtual

# Name until 25-Nov-10
#
Obsoletes:     fmi-q3-addon-fminames

%description q3
%{DESC} (for q3 server)

%files q3
%defattr(-,root,root)
%{_libdir}/lua/5.1/q3server-addons/fminames.so


#---
%changelog
* Fri Feb 3 2017 <mikko.visa@fmi.fi>
- Rebuild, GitHub packages, cleanup
* Wed Nov 24 2010 <asko.kauppi@fmi.fi>
- Packaging revise (each addon as a separate package).

