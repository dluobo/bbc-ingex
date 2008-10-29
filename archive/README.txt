                               Ingex Archive
                                July 2008


This is the Ingex Archive software. The Ingex Archive software is used in the
BBC Information and Archives' D3 Preservation project. The BBC Archives holds
around 360.000 programmes on D3 tape. The D3 tape format has become obsolete and
the D3 Preservation project aims to transfer around 100.000 of the D3 tapes to
files over a 6 year period.

The 4fsc digital composite PAL video data on the D3 tape is converted to digital
component video using the BBC transform PAL decoder. The video data with VITC, 4
digital audio tracks and LTC embedded in the VBI are combined in an SDI signal
which is input into the machine running the Ingex Archive Recorder. The cue and
timecode analogue audio tracks are optional and can be embedded alongside the
digital audio tracks after analogue-to-digital conversion.

The MXF file format is used to store the data from the D3 tapes. It includes
8-bit uncompressed UYVY 4:2:2 video data, 20-bit 48kHz PCM audio data, VITC and
LTC timecodes and metadata such as D3 VTR playback errors, Photosensitive
Epilepsy (PSE) analysis results, descriptive and identification information. The
MXF files are stored on LTO-3 tapes. An LTO-3 tape stores around 5 hours of
content and therefore 1 LTO-3 tape replaces 10 half-hour D3 tapes.

The Ingex Archive Recorder and Tape Export services run on a server machine with
a 2TB disk cache attached to it. The Recorder service captures the SDI data and
writes the data to an MXF file. At the same time it creates a 3Mbps MPEG-2
browse copy file and analyses the video for PSE failures. A Tape Export service
transfers the MXF files onto an LTO tape. The Recorder and Tape Export services
are controlled using a web browser running on a client PC.

The MXF files are checked in the Quality Check stage to ensure that they
faithfully represent the data on the D3 tape. An operator uses the QC Player
application to extract the MXF files from an LTO and writes to a local disk
cache. The player displays the video on the PC monitor and an SDI monitor. A
report is generated at the end of the quality check process. It includes
programme start/end information and marks set by the operator indicating where
audio/video faults occur and whether these faults require a retransfer.



1. Hardware requirements


1.1 Recorder / Tape Export services

The Recorder requires a server machine that supports simultaneous browse copy
encoding (3Mbps) and PSE analysis. The Recorder writes data to disk at a rate of
21MBps per second. The transfer to LTO-3 tape requires a disk read rate of
between 30MBps and 80MBps and the transfer can take place whilst recording or
playing back an MXF file.

A DVS Centaurus capture card (http://www.dvs.de) is used to capture the SDI
signal. The DVS card's SDI input is used to capture the video, audio and
timecode data and the card's SDI output is used to monitor the capture and for
playback. The newer Centaurus II card has not (yet) being tested in the system.

A D3 VTR is used to playback the tapes and a Digibeta VTR is used to record onto
a Digibeta tape in parallel to the SDI capture in the recorder. The D3 and
Digibeta VTRs are controlled by the recorder using the RS-422 serial port
interface. A set of serial-to-USB converters are used on the recorder side to
utilise the available USB ports.

A LTO-3 compatible tape drive is required. 

A barcode scanner is required to scan the D3 tape barcodes. Any barcode scanner
can be used to fill in the barcode fields in the web pages on the client PC. An
alternative is to use a server-side barcode scanner - see
src/barcode-scanner/BarcodeScanner.cpp for a list of supported barcode scanners.

A Contour Design ShuttlePro V2 (http://www.contourdesign.com/shuttlepro/)
jog-shuttle controller is used to control the playback and chunking in the
Recorder client's web browser.


1.2 Quality Check

The Quality Check station requires a PC that supports reading from an LTO drive
to disk at a rate of between 30MBps and 80MBps. Playback of the MXF file
requires at least 21MBps and more is required to allow smooth seeking. Playback
and transfer from LTO can happen simultaneously.

A DVS Centaurus or Centaurus II capture card can be used to playback the MXF
file to an SDI monitor.

A Contour Design ShuttlePro V2 (http://www.contourdesign.com/shuttlepro/)
jog-shuttle controller is used to control the MXF playback and set quality check
marks.

A LTO-3 compatible tape drive is required. 



2. OS Requirements

The Recorder, Tape Export and Quality Check stations run 32-bit openSUSE 10.3
Linux with the KDE desktop environment. Install the latest updates from the
openSUSE online update repository.



3. External software dependencies

The software depends on a number of external libaries:

- Database server and libraries, PostgreSQL, version >= 8.1
    - http://www.postgresql.org/
    - install the following packages using YaST: postgresql, postgresql-contrib,
      postgresql-devel, postgresql-libs, postgresql-server
- PostgreSQL client library, libpqxx, version >= 2.6.9
    - http://pqxx.org/development/libpqxx/
    - install the following packages using YaST: libpqxx and libpqxx-devel
- Embedded web server, shttpd, snapshot 2007/08/27
    - http://shttpd.sourceforge.net
    - download the snapshot, make and install; a RPM spec file is also available
      in the spec/ directory
    - note that the API has changed with more recent versions
- A/V encoder library, FFmpeg, snapshot 2007/10/04
    - http://ffmpeg.mplayerhq.hu/
    - download the snapshot, make and install
    - any working version should be ok
- Web library, libcurl
    - http://curl.haxx.se/
    - install the following packages using YaST: libcurl4, libcurl-devel
- SDI capture library, DVS SDK, version 2.7p57 
    - http://www.dvs.de/
    - the PALFF video raster mode is required to allow access to the VITC and
      LTC lines embedded in the VBI
- Harding FPA Photo Sensitivity Epilepsy (PSE) analysis module
    - http://www.hardingfpa.com/pse.html
    - A dummy module can used if this software is not available.
- barcode - version 0.98-330
    - generates barcode 
- ImageMagick, version 6.3.5.10-2.2
    - converts barcode into image
- Miscellaneous
    - install the C/C++ development, Linux kernel development and Perl
      development software patterns using YaST
    - install the following packages using YaST: libuuid1, libuuid-devel,
      kernel-source



4. Ingex software dependencies

The archive software depends on the following Ingex libraries:

- ingex/common
    - libcommon.a
- ingex/libMXF
    - libMXF.a in ingex/libMXF/lib
    - libMXFReader.a in ingex/libMXF/examples/reader
    - libwritearchivemxf.a in ingex/libMXF/examples/archive/write
    - libd3mxfinfo.a in ingex/libMXF/examples/archive/info
- ingex/player/ingex_player
    - player
    - qc_player



5. Directory contents

The contents of the directories are as follows:

* bbc-internal/ (only available in the BBC)
    Contains database scripts for loading BBC D3 tape information into the
    database and a script to export data for each recording. The DVS
    user raster file provides the PALFF video raster mode. The Harding FPA
    module provides Photosensitive Epilepsy analysis.
* config/
    The configuration file templates for the Recorder and Tape Export services.
    The ingex.cfg.channel file is used for the recorder 'channels' installed at
    the BBC. The CHANNEL_NAME is filled in when the RPM is installed. The second
    configuration file, ingex.cfg.testbed, is used for the testbed installation.
    The third configuration file, ingex.cfg.test, is used for testing the
    Recorder and Tape Export applications on a development PC that doesn't have
    a DVS card or VTR access.
* database/
    Contains the database schema for the Recorder and Tape Export applications.
    Example scripts for creating and dropping the database are provided along
    with a set of test D3 tape information.
* jogshuttle-settings
    Contains the Contour Design ShuttlePro V2 settings file and macros for use
    in the Windows client machines which control the Recorder application
* kde-desktop/
    KDE desktop shortcuts for the Recorder, Tape Export and Quality Check 
    applications.
* scripts/
    Scripts to start the Recorder, Tape Export and server-side barcode scanner
    applications. Also includes a script to extract files from an LTO and
    scripts to rewind and reset an LTO tape.
* spec/
    RPM specifications files and associated installation files
* src/
    Source code - see section 5.1 below
* test/
    Test setups for running the Recorder, Tape Export and Quality Check
    applications
* web/
    web pages etc. - see section 5.2 below


5.1. Source code

The src/ directory contains the following sub-directories:
* barcode-scanner/
    Classes for reading barcodes from a barcode scanner device. Also includes a
    commandline utility for reading the output from a barcode scanner and
    writing to 1 or more FIFO (named pipe) files. This utility allows a single
    barcode scanner to be used on the server side for both the Recorder and Tape
    Export applications.
* browse-encoder/
    Classes for creating MPEG-2 browse copy files from the uncompressed video
    and audio data
* cache/
    Class to manage files created by the Recorder and provide information about
    the contents of the disk cache to both the Recorder and Tape Export
    applications.
* capture/
    Library to capture SDI using the DVS card, generate a browse copy file and
    analyse the video for PSE failures.
* convert-binary-tc/
    The timecode file lists the control, VITC and LTC timecode associated
    with each frame ingested. The D3 Preservation project started with a
    binary encoding (SMPTE 12M) for the timecode file. This was changed to a
    text encoding and this utility allows the binary encoded files to be
    converted.
* database/
    Classes for loading, saving and deleting information from the Recorder and
    Tape Export's PostgreSQL database.
* general/
    General purpose classes
* generate-browse/
    This utility can be used to generate the browse copy file set (MPEG-2 file,
    timecode text file, and source information text file) from a D3 MXF file.
    The utility generates the same set of browse copy files as the recorder.
* http/
    Wraps the shttpd library's API in a C++ class and is used for communicating
    with the client's web browser
* pse/
    Defines the PSE analysis interface and provides a simple PSE implementation.
    Also provides a class to write PSE reports.
* qc-report/
    Quality Check commandline utility used by the QC player to generate QC and
    PSE reports for a QC'ed MXF file
* recorder/
    The Recorder server application
* tape-export/
    The LTO Tape Export server application
* tape-io/
    Library for doing tape input/output
* vtr/
    Library for controlling the VTR through using the RS-422 serial port


5.2. Web pages

The Recorder, Tape Export and the VTR test applications are controlled using a
web browser. The pages, scripts, images and styles for each application can be
found in the web/content directory. The web/content/recorder/ directory is for
the Recorder application, the web/content/tape directory is for the Tape Export
application and the web/content/vtrtest is for the VTR test application. The
web/svg directory contains the SVG files which were used to create the images
for the web pages.



6. Building

The build uses a number of environment variables:
- BBC_ARCHIVE_DIR
    This is the location of the bbc-internal/ directory. The dummy PSE module is
    used if this environment variable is not set
- USE_SV_DUMMY
    Set this variable to link in the dummy DVS capture in the Recorder
    application. Colourbars and dummy values for the other essence data are
    passed to the recorder.
- DVS_DIR
    The directory of the DVS SDK version 2.7p57. The SDK is assumed to be
    located at $(HOME)/sdk2.7p57 if this variable is not set.

Change into the src/ directory and build the Ingex dependencies
        make ingex-deps
and then build the Ingex Archive software
        make

The executables generated by the build are located in the source directories.
The Recorder application executable is src/recorder/ingex_recorder and the Tape
Export application executable is src/tape-export/ingex_tape_export.

The object files, libraries and executables of the Ingex dependencies can be
removed by running
        make clean-ingex-deps
The same can be done for the Ingex Archive software
        make clean


The Ingex dependencies have their own build system and requirements. The main
thing to check is that the player in ingex/player/ingex_player builds using the
same DVS SDK as the Recorder application.



8. Database installation

The Recorder and Tape Export applications use a PostgreSQL database. Install the
PostgreSQL software modules listed in section 3 on a separate and shared
database server or on the Recorder / Tape Export server. 

The postgres system user's home directory can be changed from the default
/var/lib/pgsql to /home/postgres to allow it to be located on a different disk
setup with better I/O performance. Open YaST and select Security and Users and
User Management. Open Set Filter and select System Users. Select the postgres
user and change the home directory.

Set the access privileges to allow access to the database from the Recorder and
Tape Export servers. For example, for an isolated network 10.100.x.x edit
/home/postgres/data/pg_hba.conf as root user and add the following lines and
comment out the existing lines:
        local   all         all                               trust
        host    all         all         127.0.0.1/32          trust
        host    all         all         ::1/128               trust
        host    all         all         10.100.0.0/16         trust

Edit the postgresql configuration file, /home/postgres/data/postgresql.conf, as
root user and set to allow access from all servers on the network
        listen_addresses = '*'
Make a couple of changes to make it easier to import data without causing tons
of warnings
        backslash_quote = on
        escape_string_warning = off
        standard_conforming_strings = off

Restart the database service
    sudo /sbin/service postgresql restart

Create a database using the script in the database/ directory
    ./create_example.sh
This will create a 'ingexrecorderdb' database from IngexRecorderDb.sql and
populate it with test D3 source information from test_d3_source.sql.

You can test access by running this command in a terminal
    psql -U ingex -d ingexrecorderdb -c "SELECT * FROM Version"
which should return
    ver_identifier | ver_version
    ----------------+-------------
                1 |           1
    (1 row)
Run it with the option "-h <hostname>" (fill in the hostname of the database server)
on a different machine to test network access.



9 Recorder and Tape Export Installation


9.1 Creating the RPMs

The spec/ directory contains the ingex_archive.spec RPM specification file for
creating an RPM for the Recorder and Tape Export applications. This file can be
used to create an installation RPM as follows (`pwd`/.. equals the ingex/
directory path)
    INGEX_DIR=`pwd`/.. rpmbuild -ba spec/ingex_archive.spec

The ingex_archive RPM depends on the dvs_driver RPM. To compile the DVS SDK you
need to configure the Linux kernel sources first
    cd /usr/src/linux
    sudo make cloneconfig
    sudo make scripts/mod/

Copy the DVS SDK source, sdk2.7p57.tar.gz, to /usr/src/packages/SOURCES/.
Build the DVS SDK and create the installation RPM (`pwd`/.. equals the ingex/
directory path)
    INGEX_DIR=`pwd`/.. rpmbuild -ba spec/dvsdriver27p57.spec


9.1 Installing the RPMs

Install the dvs_driver RPM as follows,
    sudo rpm -U dvsdriver-2.7p57-1.i586.rpm
and then install the ingex_archive RPM,
    sudo rpm -U ingex_archive-0.9.6-1.i586.rpm

Edit the Ingex Archive setup script, /etc/init.d/ingex_setup, and modify or
comment out the lines that set the read-ahead buffer size for the /dev/sdb1
block device. This setting is specific to the BBC installation and is not
generally useful and it is better to comment it out if you are unsure.

Restart the Ingex Archive setup
    sudo /sbin/service ingex_archive restart


9.2. Recorder client jog/shuttle settings

A Contour Design ShuttlePro V2 controller is used to control the playback and
chunking in the Recorder client's web browser. The HTML file,
jogshuttle/IngexConfidenceReplay.html, shows the mapping of the buttons and
jog/shuttle knob.

Install the latest driver version (eg. version 2.6.17) from
http://www.contourdesign.com/shuttlepro/shuttle_downloads.htm. The driver
version must include the Key Composer functionality.

The jog-shuttle device driver generates keyboard events for each button press or
turn of the jog/shuttle knob. The device configuration allows specific events to
be associated with the active application. The target application for the
recorder is the Firefox web browser running on the Recorder client PC. The
keyboard events are picked up by the web page's javascript code (see
web/content/recorder/scripts/replay_control.js) and passed on to the server
using a XMLHTTPRequest.

The jog-shuttle needs to be configured to send the appropriate keyboard events
to the Firefox web browser application. The configuration uses macros to send
multiple keyboard events for most of the buttons and jog/shuttle turns; some
buttons use single keystrokes to allow the javascript code to distinguish
between key up and key down events. Unfortunately these macros are not exported
as part of the jog-shuttle preferences file and they must be manually copied to
the "C:\Document and Settings\All Users\Application Data\Contour Design\Macros"
directory; note that the "Application Data" directory is hidden. The macros are
in the jogshuttle/Macros/ directory. Open the jog-shuttle control panel after
copying the files and you should see a series of "ingex..." macros in the Key
Composer tab.

The settings file can be now imported by opening the jog-shuttle control panel
and selecting "Options" -> "Import Settings" and then select the settings file
jogshuttle/IngexConfidenceReplay.pref. You should now see the settings for the
Firefox application matching that shown in jogshuttle/IngexConfidenceReplay.html
and used in web/content/recorder/scripts/replay_control.js.



10. Quality Check installation


10.1 Jog-shuttle event device permissions

The jog-shuttle controller is accessed through an event device
/dev/input/eventxxx. The player needs to have read permission for the event device
and by default a non-root user does not have read permission.

Edit the udev configuration file, /etc/udev/rules.d/50-udev-default.rules, as
root user and change the mode in the line 
    KERNEL=="mouse*|mice|event*", NAME="input/%k", MODE="0640"
to 0644
    KERNEL=="mouse*|mice|event*", NAME="input/%k", MODE="0644"

Unplug the jog-shuttle controller and restart the udev system
    sudo /sbin/service boot.udev restart
Plug the controller back in and check that there is at least one device (which
is probably the jog-shuttle) can be read by non-root users
    sudo ls -l /dev/input/event*


10.2 Creating the RPMs

The spec/ directory contains the qc_player.spec RPM specification file for
creating an RPM for the Quality Check station. This file can be used to create
an installation RPM as follows (`pwd`/.. equals the ingex/ directory path)
    INGEX_DIR=`pwd`/.. rpmbuild -ba spec/qc_player.spec

The qc_player RPM depends on the dvs_driver RPM. See section 9.1 for how to
build and create the dvs_driver RPM.


10.3 Creating the installation directories

A number of directories must be created manually for the installation:
    /home/qc
    /home/qc/cache
    /home/qc/reports
    /home/qc/reports/logs
These directories can be mounted on different drives. For example, the cache/
directory is where the MXF files from the LTO are stored and should be mounted
on a fast (RAIDed) disk cache. The reports/ directory could be mounted on a
shared network drive for central storage of the QC reports.


10.4 Installing the RPMs

Install the dvs_driver RPM as follows,
    sudo rpm -U dvsdriver-2.7p57-1.i586.rpm
and then install the qc_player RPM,
    sudo rpm -U qc_player-0.5.1-1.i586.rpm

Edit the QC setup script, /etc/init.d/qc_setup, and fill in the device name
(/dev/xxx) for the disk that holds the cache/ directory. Note that this change
in the block device read-ahead setting is specific to the BBC installation and
is not generally useful and it is better to comment it out if you are unsure.

Restart the QC setup,
    sudo /sbin/service qc_setup restart



11. Testing

The test/ directory contains scripts to create a setup suitable for testing the
Recorder, Tape Export and Quality Check applications. 


11.1 Recorder and Tape Export

The Tape Export application currently requires a LTO tape device (dev/nst0) to
be present.

The Recorder software must be built with the USE_SV_DUMMY environment variable
set to allow the software to be run without a DVS capture card. Eg. run
    cd src/recorder && touch main.cpp && USE_SV_DUMMY=1 make
to rebuild the recorder with the dummy DVS capture module.

The database must be setup on the local machine to run the test application.
Remember to start the database server, eg.
    sudo /sbin/service postgresql start

Run
    ./prepare_test.sh 
in the test/recorder/ directory to create a set of directories for testing the
Recorder and Tape Export applications. 

The script also copies over the config/ingex.cfg.test configuration file to
ingex.cfg. The configuration file has a directory setup matching that created by
prepare_test.sh. It defines the database host as 'localhost', defines the use
of a dummy D3 and Digibeta VTR and plays back file to the PC monitor rather than
the SDI monitor.

Start the Recorder service
    ./run_recorder.sh

You should see something similar to the following:
    25 16:10:39.334008 Info:  Starting recorder 'pilot340'...
    25 16:10:39.334162 Info:  Version: 0.9.6 (build Jun 25 2008)
    25 16:10:39.334335 Info:  Configuration: browse_dir='/home/philipn/...
... ETC ...
    25 16:10:39.339247 Info:  Opened Postgres database connection to
'ingexrecorderdb' running on 'localhost'
    25 16:10:39.339889 Info:  HTTP server is listening on port 9000
    25 16:10:39.348406 Recorder version: date=Jun 25 2008 12:21:24
    25 16:10:39.356510 capture card opened: video raster 720x592
    25 16:10:39.356691 capture thread id = 2841668496
    25 16:10:39.356802 store thread id = 2833275792
    25 16:10:39.356906 Info:  Initialised the capture system
    25 16:10:39.357032 Warn:  Opened dummy D3 vtr control 1
    25 16:10:39.357067 Warn:  Opened dummy Digibeta vtr control 2
    25 16:10:39.359536 Info:  Loaded recorder 'pilot340' data from database
    25 16:10:39.359588 Info:  Cache directory is
'/home/philipn/project/ap/ingex/archive/test/recorder/cache/'
    25 16:10:39.359606 Info:  Browse directory is
'/home/philipn/project/ap/ingex/archive/test/recorder/browse/'
    25 16:10:39.359623 Info:  PSE reports directory is
'/home/philipn/project/ap/ingex/archive/test/recorder/pse/'
    25 16:10:39.359807 Info:  Created cache creating directory 'creating'
    25 16:10:39.360529 Info:  Loaded cache
'/home/philipn/project/ap/ingex/archive/test/recorder/cache/' data from
        database
    25 16:10:39.400461 Info:  Loaded 7 cache items from the database

If the Recorder software was not compiled with the USE_SV_DUMMY environment
variable set then you will see error messages similar to the following:
    25 16:08:48.839170 card 0: SV_ERROR_DEVICENOTFOUND, The device is not found.
    25 16:08:48.839200 ERROR: Failed to initialise the DVS SDI Ingex capture
system, near Recorder.cpp:131
    25 16:08:48.839471 ERROR: Recorder exception: Failed to initialise the DVS
SDI Ingex capture system


Start the Tape Export service:
    ./run_tape_export.sh

You should see something similar to the following:
    25 16:11:36.921578 Info:  Starting tape export for recorder 'channel1'...
    25 16:11:36.921648 Info:  Version: 0.9.5 (build Jun 20 2008)
    25 16:11:36.921735 Info:  Configuration: browse_dir='/home/philipn/...
    ... ETC ...
    25 16:11:36.925546 Info:  Opened Postgres database connection to
'ingexrecorderdb' running on 'localhost'
    25 16:11:36.929566 Info:  HTTP server is listening on port 9001
    25 16:11:36.929889 Info:  Tape device: vendor='HP', model='Ultrium 3-SCSI',
revision='G54D'
    25 16:11:39.359588 Info:  Cache directory is
'/home/philipn/project/ap/ingex/archive/test/recorder/cache/'
    25 16:11:39.360529 Info:  Loaded cache
'/home/philipn/project/ap/ingex/archive/test/recorder/cache/' data from
        database
    25 16:11:36.939789 Info:  Loaded 7 cache items from the database


11.2 Quality Check

The quality check test script can be used to test the player without a DVS card
for playing out to an SDI monitor.

Run
    ./prepare_test.sh 
in the test/quality-check/ directory to create a set of directories for
testing.

Run
    ./run_qc.sh
to start the quality check player.

You should see a player window showing "bouncing balls".

You will see a "Device access failed" message of there is no LTO tape drive
connected 



12. Software authors

Philip de Nier <philipn@users.sourceforge.net>
Stuart Cunningham <stuart_hc@users.sourceforge.net>
Jim Easterbrook <easter@users.sourceforge.net>

