# $Id: ingex-studio.spec,v 1.3 2009/01/29 07:36:58 stuart_hc Exp $
Summary: Ingex Studio applications
Name: ingex-studio
Version: 2.1.0
Release: 1
License: GPL
Group: System Environment/Daemons
Source: ingex-%{version}.tar.gz
Source1: AAF-src-1.1.3.tar.gz
Url: http://ingex.sourceforge.net
BuildRoot: %{_tmppath}/%{name}-root
BuildRequires: libodbc++ unixODBC-devel

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

Source exported with "cvs export -r V2_1_0 -d ingex-%{version} ingex", then packaged with "tar czf ingex-%{version}.tar.gz ingex-%{version}".

%package -n ingex-player
License:        GPL
Summary:        Ingex player application for playing MXF files
Group:          System Environment/Daemons

%description -n ingex-player
Ingex player application for playing MXF files produced by the Index studio system.

%package -n ingex-gui
License:        GPL
Summary:        Graphical user interface for controller Ingex studio system
Group:          System Environment/Daemons
%description -n ingex-gui
Graphical user interface for controller Ingex studio system.

%prep
rm -rf $RPM_BUILD_ROOT

# unpack the source
%setup -q -c -n build
%setup -q -T -D -a 1 -n build

%build
cd AAF-src-1.1.3 && make install && make examples && cd ..

cd ingex-%{version} && make AAFSDKINSTALL=`pwd`/../AAF-src-1.1.3/%{AAF_arch_dir} AAFSDK=`pwd`/../AAF-src-1.1.3

%install
mkdir -p $RPM_BUILD_ROOT/usr/local/bin
cd ingex-%{version}
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
/usr/%{arch_lib}
%attr(-,wwwrun,www) /srv/www
/usr/local/bin/InfoDumper
/usr/local/bin/MXFDump
/usr/local/bin/Naming_Service
/usr/local/bin/ProdAutoDatabase.sql
/usr/local/bin/Recorder
/usr/local/bin/RecorderClient
/usr/local/bin/add_basic_config.sh
/usr/local/bin/avidmxfinfo
/usr/local/bin/capture.sh
/usr/local/bin/convert_audio
/usr/local/bin/cpfs
/usr/local/bin/create_aaf
/usr/local/bin/create_bamzooki_user.sh
/usr/local/bin/create_prodautodb.sh
/usr/local/bin/delayed_network_mounts
/usr/local/bin/directors-quad.sh
/usr/local/bin/disk_rw_benchmark
/usr/local/bin/dvs_dummy
/usr/local/bin/dvs_sdi
/usr/local/bin/dvs_setup
/usr/local/bin/icons/ingexHD.xpm
/usr/local/bin/icons/ingexSD.xpm
/usr/local/bin/ingex.conf
/usr/local/bin/keep_trying_mount
/usr/local/bin/make_dirs.sh
/usr/local/bin/media_transfer.pl
/usr/local/bin/nexus_multicast
/usr/local/bin/nexus_save
/usr/local/bin/nexus_stats
/usr/local/bin/nexus_web
/usr/local/bin/nexus_xv
/usr/local/bin/play_multicast.sh
/usr/local/bin/playout
/usr/local/bin/quad-split.sh
/usr/local/bin/record
/usr/local/bin/routerlogger
/usr/local/bin/run_client.sh
/usr/local/bin/run_ingex.sh
/usr/local/bin/run_ingex1.sh
/usr/local/bin/run_ingex2.sh
/usr/local/bin/run_ingex3.sh
/usr/local/bin/run_recorder.sh
/usr/local/bin/run_routerlogger.sh
/usr/local/bin/run_transcode.sh
/usr/local/bin/set_mode.sh
/usr/local/bin/startIngex.sh
/usr/local/bin/start_ingex.sh
/usr/local/bin/start_multicast.sh
/usr/local/bin/stopIngex.sh
/usr/local/bin/stop_multicast.sh
/usr/local/bin/system_info_web
/usr/local/bin/testgen
/usr/local/bin/transcode_avid_mxf
/usr/local/bin/wl_config.sql
/usr/local/bin/writeavidmxf
/usr/local/bin/xferclient.pl
/usr/local/bin/xferserver.pl


%files -n ingex-player
%defattr(-,root,root)
/usr/local/bin/player

%files -n ingex-gui
%defattr(-,root,root)
/usr/local/bin/ingexgui
/usr/local/bin/run_ingexgui.sh

%changelog
* Fri Dec 12 2008 Stuart Cunningham <stuart_hc@users.sourceforge.net>
- First official release - version 2.1.0

* Sat Nov 01 2008 Stuart Cunningham <stuart_hc@users.sourceforge.net> 2008-11-01
- Complete build of studio with all executables, script and WebIngex files

* Tue Sep 16 2008 Stuart Cunningham <stuart_hc@users.sourceforge.net> 2008-09-16
- IBC 2008 snapshot

* Tue Feb 13 2007 Stuart Cunningham <stuart_hc@users.sourceforge.net> 2007-11-03
- Initial version
