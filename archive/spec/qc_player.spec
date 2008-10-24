Name: qc_player
Summary: Archive Quality Check Player
Version: 0.5.2
Release: 1
License: GPL
Group: Productivity/Multimedia/Video/Players
Packager: Jerry Kramskoy <jerry.kramskoy@bbc.co.uk>
BuildRoot: %{_tmppath}/%{name}-root
Requires: dvsdriver shttpd codecs-for-ffmpeg ffmpeg-h264-aac barcode ImageMagick

%description
The Quality Check player allows playback of D3 Archive MXF files
and generates a QC and PSE report based in marks set by the user  

%pre
if [ ! -e /home/qc ]
then
        echo "Error: Quality Check base directory '/home/qc' does not exist"
        exit 1
fi
if [ ! -e /home/qc/cache ]
then
        echo "Error: Quality Check cache directory '/home/qc/cache' does not exist"
        exit 1
fi
if [ ! -e /home/qc/reports ]
then
        echo "Error: Quality Check report directory '/home/qc/reports' does not exist"
        exit 1
fi
if [ ! -e /home/qc/reports/logs ]
then
        echo "Error: Quality Check report logs directory '/home/qc/reports/logs' does not exist"
        exit 1
fi

%install
# get and check $INGEX_DIR environment variable
%define INGEX_DIR %(echo $INGEX_DIR)
if [ "$INGEX_DIR" = "" ]
then
	echo "Error: INGEX_DIR environment variable not set"
	exit 1
fi
export INGEX_ARCHIVE_DIR=$INGEX_DIR/archive


mkdir -p $RPM_BUILD_ROOT/usr/local/bin
mkdir -p $RPM_BUILD_ROOT/usr/lib
mkdir -p $RPM_BUILD_ROOT/etc/init.d

cp -a $INGEX_ARCHIVE_DIR/src/qc-report/qc_report $RPM_BUILD_ROOT/usr/local/bin
cp -a $INGEX_ARCHIVE_DIR/src/qc-report/qc_report_script.sh $RPM_BUILD_ROOT/usr/local/bin
cp -a $INGEX_DIR/player/ingex_player/player $RPM_BUILD_ROOT/usr/local/bin
cp -a $INGEX_DIR/player/ingex_player/qc_player $RPM_BUILD_ROOT/usr/local/bin
cp -a $INGEX_DIR/libMXF/examples/archive/info/d3_mxf_info $RPM_BUILD_ROOT/usr/local/bin

mkdir -p $RPM_BUILD_ROOT/home/qc/Desktop

cp -a $INGEX_DIR/player/ingex_player/qcplayer.xpm $RPM_BUILD_ROOT/home/qc/Desktop
cp -a "$INGEX_ARCHIVE_DIR/kde-desktop/QC Player.desktop" $RPM_BUILD_ROOT/home/qc/Desktop
cp -a "$INGEX_ARCHIVE_DIR/kde-desktop/QC Reports.desktop" $RPM_BUILD_ROOT/home/qc/Desktop
cp -a $INGEX_ARCHIVE_DIR/kde-desktop/qc_player_desktop.sh $RPM_BUILD_ROOT/home/qc/Desktop

cp -a $INGEX_ARCHIVE_DIR/spec/qc_setup $RPM_BUILD_ROOT/etc/init.d
cp -a $INGEX_ARCHIVE_DIR/spec/dvs.conf $RPM_BUILD_ROOT/etc


%post
/sbin/chkconfig --del qc_setup 2>/dev/null || true
/sbin/chkconfig --add qc_setup

%preun
if [ "$1" = "0" ]; then
	/sbin/service qc_setup stop > /dev/null 2>&1
	/sbin/chkconfig --del qc_setup
fi
exit 0


%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
/etc/init.d/qc_setup
/etc/dvs.conf
/usr/local/bin/
/usr/lib/
%defattr(775,archive,users)
/home/qc/Desktop


%changelog
* Wed Oct 8 2008 Jerry Kramskoy, BBC Research 0.5.2
- Add barcode images to qc report web page

* Mon Jun 9 2008 Philip de Nier, BBC Research 0.5.1
- Changes for new ingex archive structure

* Mon Jun 2 2008 Philip de Nier, BBC Research 0.5.0
- Extensive modification of the QC report layout
- Added control frame for updating QC comments via the player's embedded server 
- Added validation prior to user quiting
- Added validation for setting marks
- Added switching the active marks bar
- Review button is now used to switch the active marks bar
- Recording loaded session filename in the QC report
- Renamed the QC root directory and report sub directory and required that they
  are present before installation
- Setting write permissions for the users group so that stuff can be shared 
  between users on the same pc

* Fri Apr 25 2008 Philip de Nier, BBC Research 0.4.0
- restructured installation for low-noise QC stations
- added second progress bar for placing D3 VTR error and PSE failure marks
- added missing dvs configuration file dvs.conf
- added user/hostname information to qc session files and qc report
- added loaded session filename in qc session files and qc report
- firefox is opended with the qc report so that it can be printed out
- included the pse result in the qc report
- changed jog shuttle button mapping 

* Fri Feb 22 2008 Philip de Nier, BBC Research 0.3.1
- removed hard-coded content package size from d3_mxf_info and qc reports

* Mon Feb 18 2008 Philip de Nier, BBC Research 0.3.0
- lift limit of monitoring 4 audio channels to 8

* Wed Feb 13 2008 Philip de Nier, BBC Research 0.2.0
- rename the qc reports directory to 'qcreports'
- creating the qc cache directory
- added logging to file for qc report
- added KDE progress meter to feedback busy status to user
- added ingex_setup startup script, which includes the blockdev readahead for
  better read performance
- made progress bar wider for imrpoved mark resolution
- reformatted the pse report

* Wed Jan 6 2008 Philip de Nier, BBC Research 0.1.0
- First version


