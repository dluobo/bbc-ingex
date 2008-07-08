#
# spec file for package shttpd (cvs snapshot)
#
# Copyright (c) 2007 BBC. Stuart Cunningham <stuart_hc@users.sourceforge.net>
#
Name:           shttpd
BuildRequires:  gcc-c++ krb5-devel postgresql-devel
URL:            http://shttpd.sourceforge.net/
Summary:        Simple HTTPD - lightweight, easy to use web server
Version:        20070828
Release:        1
License:        BEER-WARE license (permissive BSD-like)
Group:          Productivity/Networking/Web/Servers
Source0:        %name-%version.tar.bz2
BuildRoot:      %{_tmppath}/%{name}-%{version}-build

%description
SHTTPD is a lightweight, easy to use web server. Ideal for web
developers, web-based software demos (like PHP, Perl etc), quick
file sharing. It also can be used as a library that provides web
server functionality, to create web interface for C/C++ applications.
SHTTPD is licensed under the terms of beerware license.


Authors:
--------
    Sergey Lyubka (shttpd-general at lists dot sourceforge dot net)

%prep
%setup -q

%build
cd src
# Turn off SSL since it adds dependency on -ldl
make CFLAGS="-Wall -g -O2 -DNDEBUG -DNO_SSL" unix 

%install
mkdir -p %buildroot/%_bindir
cp src/shttpd %buildroot/%_bindir

mkdir -p %buildroot/%_libdir
cp src/libshttpd.a %buildroot/%_libdir

mkdir -p %buildroot/%_includedir
cp src/shttpd.h %buildroot/%_includedir

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,-)
%doc CHANGELOG COPYING README
%_bindir/%name
%_libdir/lib%name.*
%_includedir/%name.h

%changelog -n libpqxx
* Wed Sep  5 2007 - stuart_hc@users.sourceforge.net
- First release using cvs snapshot: 2007/08/27 16:11:20
