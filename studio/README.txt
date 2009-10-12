Required standard packages
  e2fsprogs-devel
  libjpeg
  libjpeg-devel
  unixODBC
  unixODBC-devel
  psqlODBC
  postgresql-8.1.5-13

Extra packages produced by the Ingex team for convenience
  codecs-for-ffmpeg
  ffmpeg-h264-aac
  libodbc++
  dvsdriver

To make the ffmpeg RPM...

Put patches and ffmpeg-0.5.tar.bz2 in /usr/src/packages/SOURCES

> rpmbuild -bb ffmpeg-DNxHD-h264-aac.spec


