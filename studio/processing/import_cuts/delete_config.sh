#!/bin/sh

psql -v ON_ERROR_STOP=yes -U bamzooki -d prodautodb -c "BEGIN; DELETE FROM Recorder; DELETE FROM RouterConfig; DELETE FROM MultiCameraCut; DELETE FROM MultiCameraClipDef; DELETE FROM ProxyDefinition; DELETE FROM SourceConfig; DELETE FROM RecordingLocation; END;"
