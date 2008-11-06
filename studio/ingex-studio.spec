# $Id: ingex-studio.spec,v 1.2 2008/11/06 11:16:09 john_f Exp $
%define vYYYY 2008
%define vMM 11
%define vDD 01

Summary: Ingex Studio applications
Name: ingex-studio
Version: %{vYYYY}%{vMM}%{vDD}
Release: 1
License: GPL
Group: System Environment/Daemons
Source: ingex-cvs-%{version}.tar.gz
Source1: AAF-src-1.1.3.tar.gz
Url: http://ingex.sourceforge.net
BuildRoot: %{_tmppath}/%{name}-root
BuildRequires: libodbc++ unixODBC-devel
Provides: dvs_sdi Recorder ingexgui player

%ifarch x86_64
%define AAF_arch_dir AAFx86_64LinuxSDK/g++
%define arch_lib lib64
%else
%define AAF_arch_dir AAFi686LinuxSDK/g++
%define arch_lib lib
%endif

%description
Capture process (dvs_sdi/dvs_dummy) and Recorder for the Ingex Studio system.
Recorder is controlled using ingexgui, monitored using WebIngex from a browser
and MXF files can be reviewed using player.

Source exported with "cvs export -D %{vYYYY}%{vMM}%{vDD} -d ingex-cvs-%{version} ingex", then packaged with "tar czf ingex-cvs-%{version}.tar.gz ingex-cvs-%{version}".

%prep
rm -rf $RPM_BUILD_ROOT

# unpack the source
%setup -q -c -n build
%setup -q -T -D -a 1 -n build

%build
cd AAF-src-1.1.3 && make install && make examples && cd ..

cd ingex-cvs-%{version} && make AAFSDKINSTALL=`pwd`/../AAF-src-1.1.3/%{AAF_arch_dir} AAFSDK=`pwd`/../AAF-src-1.1.3

%install
mkdir -p $RPM_BUILD_ROOT/usr/local/bin
cd ingex-cvs-%{version}
cp common/tools/{disk_rw_benchmark,convert_audio} $RPM_BUILD_ROOT/usr/local/bin
cp common/tools/dvs_hardware/{record,playout} $RPM_BUILD_ROOT/usr/local/bin

cp studio/capture/{dvs_sdi,dvs_dummy,testgen,nexus_multicast,nexus_web,nexus_save,nexus_stats,nexus_xv,system_info_web,capture.sh} $RPM_BUILD_ROOT/usr/local/bin

cp	studio/ace-tao/Recorder/Recorder \
	studio/ace-tao/Recorder/run_* \
	studio/ace-tao/RecorderClient/RecorderClient \
	studio/ace-tao/RecorderClient/run_client.sh \
	studio/database/scripts/create_prodautodb.sh \
	studio/database/scripts/ProdAutoDatabase.sql \
	studio/database/scripts/wl_config.sql \
	studio/database/scripts/create_bamzooki_user.sh \
	studio/database/scripts/add_basic_config.sh \
		$RPM_BUILD_ROOT/usr/local/bin

cp	studio/processing/media_transfer/{cpfs,media_transfer.pl,xferclient.pl,xferserver.pl} \
	studio/processing/createaaf/create_aaf \
	studio/processing/offline/{transcode_avid_mxf,run_transcode.sh} \
	studio/ace-tao/Ingexgui/src/ingexgui \
	studio/ace-tao/routerlog/run_routerlogger.sh \
	studio/ace-tao/routerlog/routerlogger		$RPM_BUILD_ROOT/usr/local/bin

cp -pr	studio/scripts/* $RPM_BUILD_ROOT/usr/local/bin

(cd studio/web/WebIngex && sh ./install.sh $RPM_BUILD_ROOT/srv/www)

cp	libMXF/examples/writeavidmxf/writeavidmxf \
	libMXF/examples/avidmxfinfo/avidmxfinfo		$RPM_BUILD_ROOT/usr/local/bin

cp player/ingex_player/player $RPM_BUILD_ROOT/usr/local/bin

cp libMXF/test/MXFDump/MXFDump $RPM_BUILD_ROOT/usr/local/bin
cp ../AAF-src-1.1.3/%{AAF_arch_dir}/bin/debug/InfoDumper $RPM_BUILD_ROOT/usr/local/bin
mkdir -p $RPM_BUILD_ROOT/usr/%{arch_lib}
mkdir -p $RPM_BUILD_ROOT/usr/%{arch_lib}/aafext
cp ../AAF-src-1.1.3/%{AAF_arch_dir}/bin/debug/libcom-api.so $RPM_BUILD_ROOT/usr/%{arch_lib}
cp ../AAF-src-1.1.3/%{AAF_arch_dir}/bin/debug/aafext/*.so $RPM_BUILD_ROOT/usr/%{arch_lib}/aafext

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
/usr/local/bin/
/usr/%{arch_lib}
%attr(-,wwwrun,www) /srv/www

%changelog
* Sat Nov 01 2008 Stuart Cunningham <stuart_hc@users.sourceforge.net> 2008-11-01
- Complete build of studio with all executables, script and WebIngex files

* Tue Sep 16 2008 Stuart Cunningham <stuart_hc@users.sourceforge.net> 2008-09-16
- IBC 2008 snapshot

* Tue Feb 13 2007 Stuart Cunningham <stuart_hc@users.sourceforge.net> 2007-11-03
- Initial version
