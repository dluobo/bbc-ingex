%define redhat		%(test ! -f /etc/redhat-release ; echo $?)
%define redhat		%(test ! -f /etc/fedora-release ; echo $?)
%define mandrake	%(test ! -f /etc/mandrake-release ; echo $?)
%define suse		%(test ! -f /etc/SuSE-release ; echo $?)

Summary: DVS driver and utilities for DVS SDI I/O cards
Name: dvsdriver
Version: 2.7p57
Release: 1
License: GPL
Group: System Environment/Daemons
Source: sdk%{PACKAGE_VERSION}.tar.gz
#Source1: dvsdriver_extras.tar.gz
#Source2: sdk%{PACKAGE_VERSION}_x86_64.tar.gz
Url: http://private.dvs.de/integrator/centaurus/beta/software/linux/sdk%{PACKAGE_VERSION}.tar.gz
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
Includes userdef.ref to enable DVS hardware to perform PALFF mode.

%prep
rm -rf $RPM_BUILD_ROOT
# setup unpacks the tarball and cd's into it
%ifarch x86_64
%setup -q -n sdk%{PACKAGE_VERSION}_x86_64 -b 2
%setup -q -n sdk%{PACKAGE_VERSION}_x86_64 -T -D -a 1
%else
%setup -q -n sdk%{PACKAGE_VERSION}
#%setup -q -n sdk%{PACKAGE_VERSION} -T -D -a 1
%endif


%build
cd linux/driver
./driver_create
cd ../../development/examples
make

%install
# get and check $INGEX_DIR environment variable
%define INGEX_DIR %(echo $INGEX_DIR)
if [ "$INGEX_DIR" = "" ]
then
	echo "Error: INGEX_DIR environment variable not set"
	exit 1
fi
export INGEX_ARCHIVE_DIR=$INGEX_DIR/archive


%ifarch x86_64
mkdir -p $RPM_BUILD_ROOT/usr/lib64
cp linux/lib/libdvsoem.a $RPM_BUILD_ROOT/usr/lib64
%else
mkdir -p $RPM_BUILD_ROOT/usr/lib
cp linux/lib/libdvsoem.a $RPM_BUILD_ROOT/usr/lib
%endif
mkdir -p $RPM_BUILD_ROOT/usr/lib/dvs
mkdir -p $RPM_BUILD_ROOT/usr/bin
mkdir -p $RPM_BUILD_ROOT/usr/include
cp linux/bin/* $RPM_BUILD_ROOT/usr/bin
cp development/header/*.h $RPM_BUILD_ROOT/usr/include

mkdir -p $RPM_BUILD_ROOT/usr/lib/dvs
for file in mkdev driver_load driver_unload default.ref dvsdriver.ko sdcsrmux.pld sdpixmux.pld dvsdebug.ko
do
	cp linux/driver/$file $RPM_BUILD_ROOT/usr/lib/dvs
done
# userdef.ref gives PALFF capability
cp $INGEX_ARCHIVE_DIR/bbc-internal/dvs/userdef.ref $RPM_BUILD_ROOT/usr/lib/dvs


mkdir -p $RPM_BUILD_ROOT/etc/init.d
cp $INGEX_ARCHIVE_DIR/spec/dvs_setup $RPM_BUILD_ROOT/etc/init.d

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
* Mon Jun 9 2008 Philip de Nier %{PACKAGE_VERSION}
- Changes for new ingex archive structure

* Wed Oct 31 2007 Philip de Nier %{PACKAGE_VERSION}
- Updates for archive preservation system on SuSE 10.3

* Tue Feb 13 2007 Stuart Cunningham <stuart_hc@users.sourceforge.net> %{PACKAGE_VERSION}
- Initial version
