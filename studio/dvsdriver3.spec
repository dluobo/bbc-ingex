%define redhat		%(test ! -f /etc/redhat-release ; echo $?)
%define redhat		%(test ! -f /etc/fedora-release ; echo $?)
%define mandrake	%(test ! -f /etc/mandrake-release ; echo $?)
%define suse		%(test ! -f /etc/SuSE-release ; echo $?)

Summary: DVS driver and utilities for DVS SDI I/O cards
Name: dvsdriver
Version: 3.2.14.3
Release: 1
License: GPL
Group: System Environment/Daemons
Source: sdk%{version}.tar.gz
Source1: dvs_setup
Url: http://private.dvs.de/integrator/centaurus/beta/software/linux/sdk%{version}.tar.gz
BuildRoot: %{_tmppath}/%{name}-root
%if %{suse}
BuildRequires: kernel-source
%else
BuildRequires: kernel-devel
%endif
PreReq: /sbin/chkconfig /sbin/service
Provides: svram

%description
DVS driver, developer includes, library and example programs.

%prep
rm -rf $RPM_BUILD_ROOT
# setup unpacks the tarball and cd's into it
%setup -q -n sdk%{version}

%build
%ifarch x86_64
%define arch_dir x86_64
%else
%define arch_dir x86
%endif

cd linux-%{arch_dir}/driver
./driver_create

%install
%ifarch x86_64
mkdir -p $RPM_BUILD_ROOT/usr/lib64
cp linux-%{arch_dir}/lib/libdvsoem.a $RPM_BUILD_ROOT/usr/lib64
%else
mkdir -p $RPM_BUILD_ROOT/usr/lib
cp linux-%{arch_dir}/lib/libdvsoem.a $RPM_BUILD_ROOT/usr/lib
%endif
mkdir -p $RPM_BUILD_ROOT/usr/lib/dvs
mkdir -p $RPM_BUILD_ROOT/usr/bin
mkdir -p $RPM_BUILD_ROOT/usr/include
cp linux-%{arch_dir}/bin/{svram,cmodetst,dmaloop,dmaspeed,dpxio,dvsinfo,jackloop,overlay} $RPM_BUILD_ROOT/usr/bin
cp development/header/*.h $RPM_BUILD_ROOT/usr/include

mkdir -p $RPM_BUILD_ROOT/usr/lib/dvs
for file in mkdev driver_load driver_unload default.ref dvsdriver.ko sdcsrmux.pld sdpixmux.pld dvsdebug.ko
do
	cp linux-%{arch_dir}/driver/$file $RPM_BUILD_ROOT/usr/lib/dvs
done

mkdir -p $RPM_BUILD_ROOT/etc/init.d
cp $RPM_SOURCE_DIR/dvs_setup $RPM_BUILD_ROOT/etc/init.d

%clean
rm -rf $RPM_BUILD_ROOT

%post
# remove old version if any
/sbin/chkconfig --del dvs_setup 2>/dev/null || true
# add boot-time script
/sbin/chkconfig --add dvs_setup

%preun
if [ "$1" = "0" ]; then
	/sbin/service dvs_setup stop > /dev/null 2>&1
	/sbin/chkconfig --del dvs_setup
fi
exit 0

%files
%defattr(-,root,root)
%ifarch x86_64
/usr/lib64/
%endif
/usr/lib/
/usr/include/
/usr/bin/
/etc/init.d/dvs_setup

%changelog
* Mon Nov 03 2008 Stuart Cunningham <stuart_hc@users.sourceforge.net> 3.2.14.3
- Updates for sdk3.2.14.3
* Wed Oct 31 2007 Philip de Nier %{PACKAGE_VERSION}
- Updates for archive preservation system on SuSE 10.3
* Tue Feb 13 2007 Stuart Cunningham <stuart_hc@users.sourceforge.net> %{PACKAGE_VERSION}
- Initial version
