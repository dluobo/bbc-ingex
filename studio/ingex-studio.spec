Summary: Ingex Studio capture process and recorder
Name: ingex-studio-recorder
Version: 1
Release: 1
License: GPL
Group: System Environment/Daemons
Source: ingex-cvs-2007-11-03.tar.gz
Url: http://ingex.sourceforge.net
BuildRoot: %{_tmppath}/%{name}-root
BuildRequires: libodbc++ unixODBC-devel
Provides: dvs_sdi Recorder

%description
Capture process and Recorder for the Ingex Studio SDI recording system.
Source exported with "cvs export -D 2007-11-03 -d ingex-cvs-2007-11-03 ingex".

%prep
rm -rf $RPM_BUILD_ROOT

# unpack the source
%setup -q -n ingex-cvs-2007-11-03

%build
# common library is built first (some tools depend on DVS SDK)
# FIXME: Makefile searches for SDK version under $HOME/sdk...
# this should use the path from the dvs rpm
cd common && make && cd ..

# dvs_sdi depends on the DVS driver SDK
# FIXME: fix DVS SDK path here too
cd studio/capture && make all
cd ..

# Build all directories using stand-alone makefiles
for dir in mxfwriter
do
	(cd $dir && make)
done

# Build all directories using .mpc files (ACE build environment)
export ACE_ROOT=/usr
export TAO_ROOT=/usr
for dir in processing/offline common
do
	(cd $dir && /usr/share/ace/bin/mwc.pl && make)
done

# The CORBA build needs some include fixups after the makefile generation
cd ace-tao
/usr/share/ace/bin/mwc.pl
perl -p -i -e 's,^include...TAO_ROOT.,include /usr/include/makeinclude,' `grep '^include...TAO_ROOT' -r . -I -l`
# routerlogger doesn't build so edit it from the makefile
perl -p -i -e 's/\s*routerlogger.*//; s/\t\@cd routerlog.*//; s/\.PHONY:$//' GNUmakefile
export TAO_ROOT=/usr/include/makeinclude
make
cd ..

%install
mkdir -p $RPM_BUILD_ROOT/usr/local/bin
cp common/tools/{disk_rw_benchmark,convert_audio} $RPM_BUILD_ROOT/usr/local/bin
cp common/tools/dvs_hardware/{record,playout} $RPM_BUILD_ROOT/usr/local/bin

cp studio/capture/{dvs_sdi,testgen,nexus_save,nexus_stats,nexus_xv} $RPM_BUILD_ROOT/usr/local/bin

cp	studio/ace-tao/Recorder/Recorder \
	studio/ace-tao/Recorder/run_* \
	studio/ace-tao/RecorderClient/RecorderClient \
	studio/ace-tao/RecorderClient/run_client.sh	$RPM_BUILD_ROOT/usr/local/bin

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
/usr/local/bin/

%changelog
* Tue Feb 13 2007 Stuart Cunningham <stuart_hc@users.sourceforge.net> 2007-11-03
- Initial version
