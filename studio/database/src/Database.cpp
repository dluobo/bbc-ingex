/*
 * $Id: Database.cpp,v 1.12 2009/09/18 16:50:11 philipn Exp $
 *
 * Provides access to the data in the database
 *
 * Copyright (C) 2006  Philip de Nier <philipn@users.sourceforge.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <cassert>
#include <cstring>
#include <cstdlib>

#include "Database.h"
#include "Utilities.h"
#include "DBException.h"
#include "Logging.h"

using namespace std;
using namespace prodauto;
using namespace pqxx;



#define COND_NUM_PARAM(cond, value) \
    (cond ? value : 0), cond

#define COND_STR_PARAM(cond, value) \
    (cond ? value : ""), cond


#define START_WORK \
    try

#define END_WORK(func_name) \
    catch (const exception &ex) \
    { \
        PA_LOGTHROW(DBException, (func_name ": %s", ex.what())); \
    } \
    catch (const DBException &ex) \
    { \
        throw; \
    } \
    catch (...) \
    { \
        PA_LOGTHROW(DBException, (func_name ": Unknown exception")); \
    }


static const int g_compatibilityVersion = 8;

    
const char* const LOAD_REC_LOCATIONS_STMT = "load recording locations";
const char* const LOAD_REC_LOCATIONS_SQL =
" \
    SELECT \
        rlc_identifier, \
        rlc_name \
    FROM \
        RecordingLocation \
";

const char* const INSERT_REC_LOCATIONS_STMT = "insert recording location";
const char* const INSERT_REC_LOCATIONS_SQL =
" \
    INSERT INTO RecordingLocation \
    ( \
        rlc_identifier, \
        rlc_name \
    ) \
    VALUES \
    ($1, $2) \
";


const char* const LOAD_RECORDER_STMT = "load recorder";
const char* const LOAD_RECORDER_SQL =
" \
    SELECT \
        rer_identifier, \
        rer_name, \
        rer_conf_id \
    FROM Recorder \
    WHERE \
        rer_name = $1 \
";

const char* const LOAD_RECORDER_CONFIG_STMT = "load recorder config";
const char* const LOAD_RECORDER_CONFIG_SQL =
" \
    SELECT \
        rec_identifier, \
        rec_name \
    FROM RecorderConfig \
    WHERE \
        rec_recorder_id= $1 \
";

const char* const LOAD_RECORDER_PARAMS_STMT = "load recorder params";
const char* const LOAD_RECORDER_PARAMS_SQL =
" \
    SELECT \
        rep_identifier, \
        rep_name, \
        rep_value, \
        rep_type \
    FROM RecorderParameter \
    WHERE \
        rep_recorder_conf_id= $1 \
";

const char* const LOAD_RECORDER_INPUT_CONFIG_STMT = "load recorder input config";
const char* const LOAD_RECORDER_INPUT_CONFIG_SQL =
" \
    SELECT \
        ric_identifier, \
        ric_index, \
        ric_name \
    FROM RecorderInputConfig \
    WHERE \
        ric_recorder_conf_id = $1 \
";

const char* const LOAD_RECORDER_INPUT_TRACK_CONFIG_STMT = "load recorder input track config";
const char* const LOAD_RECORDER_INPUT_TRACK_CONFIG_SQL =
" \
    SELECT \
        rtc_identifier, \
        rtc_index, \
        rtc_track_number, \
        rtc_source_id, \
        rtc_source_track_id \
    FROM RecorderInputTrackConfig \
    WHERE \
        rtc_recorder_input_id = $1 \
";

const char* const INSERT_RECORDER_STMT = "insert recorder";
const char* const INSERT_RECORDER_SQL =
" \
    INSERT INTO Recorder \
    ( \
        rer_identifier, \
        rer_name, \
        rer_conf_id \
    ) \
    VALUES \
    ($1, $2, $3) \
";

const char* const INSERT_RECORDER_CONFIG_STMT = "insert recorder config";
const char* const INSERT_RECORDER_CONFIG_SQL =
" \
    INSERT INTO RecorderConfig \
    ( \
        rec_identifier, \
        rec_name, \
        rec_recorder_id \
    ) \
    VALUES \
    ($1, $2, $3) \
";

const char* const INSERT_RECORDER_PARAM_STMT = "insert recorder param";
const char* const INSERT_RECORDER_PARAM_SQL =
" \
    INSERT INTO RecorderParameter \
    ( \
        rep_identifier, \
        rep_name, \
        rep_value, \
        rep_type, \
        rep_recorder_conf_id \
    ) \
    VALUES \
    ($1, $2, $3, $4, $5) \
";

const char* const INSERT_RECORDER_INPUT_CONFIG_STMT = "insert recorder input config";
const char* const INSERT_RECORDER_INPUT_CONFIG_SQL =
" \
    INSERT INTO RecorderInputConfig \
    ( \
        ric_identifier, \
        ric_index, \
        ric_name, \
        ric_recorder_conf_id \
    ) \
    VALUES \
    ($1, $2, $3, $4) \
";

const char* const INSERT_RECORDER_INPUT_TRACK_CONFIG_STMT = "insert recorder input track config";
const char* const INSERT_RECORDER_INPUT_TRACK_CONFIG_SQL =
" \
    INSERT INTO RecorderInputTrackConfig \
    ( \
        rtc_identifier, \
        rtc_index, \
        rtc_track_number, \
        rtc_source_id, \
        rtc_source_track_id, \
        rtc_recorder_input_id \
    ) \
    VALUES \
    ($1, $2, $3, $4, $5, $6) \
";

const char* const UPDATE_RECORDER_STMT = "update recorder";
const char* const UPDATE_RECORDER_SQL =
" \
    UPDATE Recorder \
    SET rer_name = $1, \
        rer_conf_id = $2 \
    WHERE \
        rer_identifier = $3 \
";

const char* const UPDATE_ACTIVE_RECORDER_CONFIG_STMT = "update active recorder config";
const char* const UPDATE_ACTIVE_RECORDER_CONFIG_SQL =
" \
    UPDATE Recorder \
    SET rer_conf_id = $1 \
    WHERE \
        rer_identifier = $2 \
";

const char* const UPDATE_RECORDER_PARAM_STMT = "update recorder param";
const char* const UPDATE_RECORDER_PARAM_SQL =
" \
    UPDATE RecorderParameter \
    SET rep_name = $1, \
        rep_value = $2, \
        rep_type = $3, \
        rep_recorder_conf_id = $4 \
    WHERE \
        rep_identifier = $5 \
";

const char* const UPDATE_RECORDER_CONFIG_STMT = "update recorder config";
const char* const UPDATE_RECORDER_CONFIG_SQL =
" \
    UPDATE RecorderConfig \
    SET rec_name = $1, \
        rec_recorder_id = $2 \
    WHERE \
        rec_identifier = $3 \
";

const char* const UPDATE_RECORDER_INPUT_CONFIG_STMT = "update recorder input config";
const char* const UPDATE_RECORDER_INPUT_CONFIG_SQL =
" \
    UPDATE RecorderInputConfig \
    SET ric_index = $1, \
        ric_name = $2, \
        ric_recorder_conf_id = $3 \
    WHERE \
        ric_identifier = $4 \
";

const char* const UPDATE_RECORDER_INPUT_TRACK_CONFIG_STMT = "update recorder input track config";
const char* const UPDATE_RECORDER_INPUT_TRACK_CONFIG_SQL =
" \
    UPDATE RecorderInputTrackConfig \
    SET rtc_index = $1, \
        rtc_track_number = $2, \
        rtc_source_id = $3, \
        rtc_source_track_id = $4, \
        rtc_recorder_input_id = $5 \
    WHERE \
        rtc_identifier = $6 \
";

const char* const DELETE_RECORDER_STMT = "delete recorder";
const char* const DELETE_RECORDER_SQL =
" \
    DELETE FROM Recorder WHERE rer_identifier = $1 \
";


const char* const LOAD_SOURCE_CONFIG_STMT = "load source config";
const char* const LOAD_SOURCE_CONFIG_SQL =
" \
    SELECT \
        scf_identifier, \
        scf_name, \
        scf_type, \
        scf_spool_number, \
        scf_recording_location \
    FROM SourceConfig \
    WHERE \
        scf_identifier = $1 \
";

const char* const LOAD_NAMED_SOURCE_CONFIG_STMT = "load named source config";
const char* const LOAD_NAMED_SOURCE_CONFIG_SQL =
" \
    SELECT \
        scf_identifier, \
        scf_name, \
        scf_type, \
        scf_spool_number, \
        scf_recording_location \
    FROM SourceConfig \
    WHERE \
        scf_name = $1 \
";

const char* const LOAD_SOURCE_TRACK_CONFIG_STMT = "load source track config";
const char* const LOAD_SOURCE_TRACK_CONFIG_SQL =
" \
    SELECT \
        sct_identifier, \
        sct_track_id, \
        sct_track_number, \
        sct_track_name, \
        sct_track_data_def, \
        (sct_track_edit_rate).numerator, \
        (sct_track_edit_rate).denominator, \
        sct_track_length \
    FROM SourceTrackConfig \
    WHERE \
        sct_source_id = $1 \
";


const char* const INSERT_SOURCE_CONFIG_STMT = "insert source config";
const char* const INSERT_SOURCE_CONFIG_SQL =
" \
    INSERT INTO SourceConfig \
    ( \
        scf_identifier, \
        scf_name, \
        scf_type, \
        scf_spool_number, \
        scf_recording_location \
    ) \
    VALUES \
    ($1, $2, $3, $4, $5) \
";

const char* const INSERT_SOURCE_TRACK_CONFIG_STMT = "insert source track config";
const char* const INSERT_SOURCE_TRACK_CONFIG_SQL =
" \
    INSERT INTO SourceTrackConfig \
    ( \
        sct_identifier, \
        sct_track_id, \
        sct_track_number, \
        sct_track_name, \
        sct_track_data_def, \
        sct_track_edit_rate, \
        sct_track_length, \
        sct_source_id \
    ) \
    VALUES \
    ($1, $2, $3, $4, $5, ($6, $7), $8, $9) \
";

const char* const UPDATE_SOURCE_CONFIG_STMT = "update source config";
const char* const UPDATE_SOURCE_CONFIG_SQL =
" \
    UPDATE SourceConfig \
    SET scf_name = $1, \
        scf_type = $2, \
        scf_spool_number = $3, \
        scf_recording_location = $4 \
    WHERE \
        scf_identifier = $5 \
";

const char* const UPDATE_SOURCE_TRACK_CONFIG_STMT = "update source track config";
const char* const UPDATE_SOURCE_TRACK_CONFIG_SQL =
" \
    UPDATE SourceTrackConfig \
    SET sct_track_id = $1, \
        sct_track_number = $2, \
        sct_track_name = $3, \
        sct_track_data_def = $4, \
        sct_track_edit_rate = ($5, $6), \
        sct_track_length = $7, \
        sct_source_id = $8 \
    WHERE \
        sct_identifier = $9 \
";

const char* const DELETE_SOURCE_CONFIG_STMT = "delete source config";
const char* const DELETE_SOURCE_CONFIG_SQL =
" \
    DELETE FROM SourceConfig WHERE scf_identifier = $1 \
";


const char* const LOAD_ALL_ROUTER_CONFIGS_STMT = "load all router configs";
const char* const LOAD_ALL_ROUTER_CONFIGS_SQL =
" \
    SELECT \
        ror_identifier, \
        ror_name \
    FROM RouterConfig \
";

const char* const LOAD_ROUTER_CONFIG_STMT = "load router config";
const char* const LOAD_ROUTER_CONFIG_SQL =
" \
    SELECT \
        ror_identifier, \
        ror_name \
    FROM RouterConfig \
    WHERE \
        ror_name = $1 \
";

const char* const LOAD_ROUTER_INPUT_CONFIG_STMT = "load router input config";
const char* const LOAD_ROUTER_INPUT_CONFIG_SQL =
" \
    SELECT \
        rti_identifier, \
        rti_index, \
        rti_name, \
        rti_source_id, \
        rti_source_track_id \
    FROM RouterInputConfig \
    WHERE \
        rti_router_conf_id = $1 \
";

const char* const LOAD_ROUTER_OUTPUT_CONFIG_STMT = "load router output config";
const char* const LOAD_ROUTER_OUTPUT_CONFIG_SQL =
" \
    SELECT \
        rto_identifier, \
        rto_index, \
        rto_name \
    FROM RouterOutputConfig \
    WHERE \
        rto_router_conf_id = $1 \
";


const char* const LOAD_ALL_MC_CLIP_DEFS_STMT = "load all mc clip defs";
const char* const LOAD_ALL_MC_CLIP_DEFS_SQL =
" \
    SELECT \
        mcd_identifier, \
        mcd_name \
    FROM MultiCameraClipDef \
";

const char* const LOAD_MC_CLIP_DEF_STMT = "load mc clip def";
const char* const LOAD_MC_CLIP_DEF_SQL =
" \
    SELECT \
        mcd_identifier, \
        mcd_name \
    FROM MultiCameraClipDef \
    WHERE \
        mcd_name = $1 \
";

const char* const LOAD_MC_TRACK_DEF_STMT = "load mc track def";
const char* const LOAD_MC_TRACK_DEF_SQL =
" \
    SELECT \
        mct_identifier, \
        mct_index, \
        mct_track_number \
    FROM MultiCameraTrackDef \
    WHERE \
        mct_multi_camera_def_id = $1 \
";

const char* const LOAD_MC_SELECTOR_DEF_STMT = "load mc selector def";
const char* const LOAD_MC_SELECTOR_DEF_SQL =
" \
    SELECT \
        mcs_identifier, \
        mcs_index, \
        mcs_source_id, \
        mcs_source_track_id \
    FROM MultiCameraSelectorDef \
    WHERE \
        mcs_multi_camera_track_def_id = $1 \
";

const char* const INSERT_MC_CLIP_DEF_STMT = "insert mc clip def";
const char* const INSERT_MC_CLIP_DEF_SQL =
" \
    INSERT INTO MultiCameraClipDef \
    ( \
        mcd_identifier, \
        mcd_name \
    ) \
    VALUES \
    ($1, $2) \
";

const char* const INSERT_MC_TRACK_DEF_STMT = "insert mc track def";
const char* const INSERT_MC_TRACK_DEF_SQL =
" \
    INSERT INTO MultiCameraTrackDef \
    ( \
        mct_identifier, \
        mct_index, \
        mct_track_number, \
        mct_multi_camera_def_id \
    ) \
    VALUES \
    ($1, $2, $3, $4) \
";

const char* const INSERT_MC_SELECTOR_DEF_STMT = "insert mc selector def";
const char* const INSERT_MC_SELECTOR_DEF_SQL =
" \
    INSERT INTO MultiCameraSelectorDef \
    ( \
        mcs_identifier, \
        mcs_index, \
        mcs_source_id, \
        mcs_source_track_id, \
        mcs_multi_camera_track_def_id \
    ) \
    VALUES \
    ($1, $2, $3, $4, $5) \
";

const char* const UPDATE_MC_CLIP_DEF_STMT = "update mc clip def";
const char* const UPDATE_MC_CLIP_DEF_SQL =
" \
    UPDATE MultiCameraClipDef \
    SET mcd_name = $1 \
    WHERE \
        mcd_identifier = $2 \
";

const char* const UPDATE_MC_TRACK_DEF_STMT = "update mc track def";
const char* const UPDATE_MC_TRACK_DEF_SQL =
" \
    UPDATE MultiCameraTrackDef \
    SET mct_index = $1, \
        mct_track_number = $2, \
        mct_multi_camera_def_id = $3 \
    WHERE \
        mct_identifier = $4 \
";

const char* const UPDATE_MC_SELECTOR_DEF_STMT = "update mc selector def";
const char* const UPDATE_MC_SELECTOR_DEF_SQL =
" \
    UPDATE MultiCameraSelectorDef \
    SET mcs_index = $1, \
        mcs_source_id = $2, \
        mcs_source_track_id = $3, \
        mcs_multi_camera_track_def_id = $4 \
    WHERE \
        mcs_identifier = $5 \
";

const char* const DELETE_MC_CLIP_DEF_STMT = "delete mc clip def";
const char* const DELETE_MC_CLIP_DEF_SQL =
" \
    DELETE FROM MultiCameraClipDef WHERE mcd_identifier = $1 \
";


const char* const LOAD_ALL_MC_CUTS_STMT = "load all mc cuts";
const char* const LOAD_ALL_MC_CUTS_SQL =
" \
    SELECT \
        mcc_identifier, \
        mcc_multi_camera_track_def_id, \
        mcc_multi_camera_selector_index, \
        mcc_date::varchar, \
        mcc_position, \
        (mcc_edit_rate).numerator, \
        (mcc_edit_rate).denominator \
    FROM MultiCameraCut \
";

const char* const LOAD_MC_CUTS_STMT = "load mc cuts";
const char* const LOAD_MC_CUTS_SQL =
" \
    SELECT \
        mcc_identifier, \
        mcc_multi_camera_track_def_id, \
        mcc_multi_camera_selector_index, \
        mcc_date::varchar, \
        mcc_position, \
        (mcc_edit_rate).numerator, \
        (mcc_edit_rate).denominator \
    FROM MultiCameraCut \
    WHERE \
        mcc_date >= $1 AND \
        mcc_date <= $2 AND \
        mcc_multi_camera_track_def_id = $3 \
    ORDER BY mcc_date, mcc_position \
";

const char* const INSERT_MC_CUT_STMT = "insert mc cut";
const char* const INSERT_MC_CUT_SQL =
" \
    INSERT INTO MultiCameraCut \
    ( \
        mcc_identifier, \
        mcc_multi_camera_track_def_id, \
        mcc_multi_camera_selector_index, \
        mcc_date, \
        mcc_position, \
        mcc_edit_rate \
    ) \
    VALUES \
    ($1, $2, $3, $4, $5, ($6, $7)) \
";

const char* const UPDATE_MC_CUT_STMT = "update mc cut";
const char* const UPDATE_MC_CUT_SQL =
" \
    UPDATE MultiCameraCut \
        mcc_multi_camera_track_def_id = $1, \
        mcc_multi_camera_selector_index = $2, \
        mcc_date = $3, \
        mcc_position = $4, \
        mcc_edit_rate = ($5, $6) \
    WHERE \
        mcc_identifier = $7 \
";


const char* const LOAD_ALL_SERIES_STMT = "load all series";
const char* const LOAD_ALL_SERIES_SQL =
" \
    SELECT \
        srs_identifier, \
        srs_name \
    FROM Series \
";

const char* const LOAD_PROGRAMMES_IN_SERIES_STMT = "load programmes in series";
const char* const LOAD_PROGRAMMES_IN_SERIES_SQL =
" \
    SELECT \
        prg_identifier, \
        prg_name \
    FROM Programme, \
        Series \
    WHERE \
        prg_series_id = srs_identifier AND \
        srs_identifier = $1 \
";

const char* const INSERT_SERIES_STMT = "insert series";
const char* const INSERT_SERIES_SQL =
" \
    INSERT INTO Series \
    ( \
        srs_identifier, \
        srs_name \
    ) \
    VALUES \
    ($1, $2) \
";

const char* const UPDATE_SERIES_STMT = "update series";
const char* const UPDATE_SERIES_SQL =
" \
    UPDATE Series \
    SET srs_name = $1 \
    WHERE \
        srs_identifier = $2 \
";

const char* const DELETE_SERIES_STMT = "delete series";
const char* const DELETE_SERIES_SQL =
" \
    DELETE FROM Series WHERE srs_identifier = $1 \
";

const char* const LOAD_ITEMS_IN_PROGRAMME_STMT = "load items in programme";
const char* const LOAD_ITEMS_IN_PROGRAMME_SQL =
" \
    SELECT \
        itm_identifier, \
        itm_description, \
        itm_script_section_ref, \
        itm_order_index \
    FROM Item, \
        Programme \
    WHERE \
        itm_programme_id = prg_identifier AND \
        prg_identifier = $1 \
    ORDER BY itm_order_index \
";

const char* const INSERT_PROGRAMME_STMT = "insert programme";
const char* const INSERT_PROGRAMME_SQL =
" \
    INSERT INTO Programme \
    ( \
        prg_identifier, \
        prg_name, \
        prg_series_id \
    ) \
    VALUES \
    ($1, $2, $3) \
";

const char* const UPDATE_PROGRAMME_STMT = "update programme";
const char* const UPDATE_PROGRAMME_SQL =
" \
    UPDATE Programme \
    SET prg_name = $1, \
        prg_series_id = $2 \
    WHERE \
        prg_identifier = $3 \
";

const char* const DELETE_PROGRAMME_STMT = "delete programme";
const char* const DELETE_PROGRAMME_SQL =
" \
    DELETE FROM Programme WHERE prg_identifier = $1 \
";

const char* const UPDATE_ITEM_ORDER_INDEX_STMT = "update item order index";
const char* const UPDATE_ITEM_ORDER_INDEX_SQL =
" \
    UPDATE Item \
    SET itm_order_index = $1 \
    WHERE \
        itm_identifier = $2 \
";

const char* const LOAD_TAKES_IN_ITEM_STMT = "load takes in item";
const char* const LOAD_TAKES_IN_ITEM_SQL =
" \
    SELECT \
        tke_identifier, \
        tke_number, \
        tke_comment, \
        tke_result, \
        (tke_edit_rate).numerator, \
        (tke_edit_rate).denominator, \
        tke_length, \
        tke_start_position, \
        tke_start_date, \
        tke_recording_location \
    FROM Take, \
        Item \
    WHERE \
        tke_item_id = itm_identifier AND \
        itm_identifier = $1 \
    ORDER BY tke_number \
";

const char* const INSERT_ITEM_STMT = "insert item";
const char* const INSERT_ITEM_SQL =
" \
    INSERT INTO Item \
    ( \
        itm_identifier, \
        itm_description, \
        itm_script_section_ref, \
        itm_order_index, \
        itm_programme_id \
    ) \
    VALUES \
    ($1, $2, $3, $4, $5) \
";

const char* const UPDATE_ITEM_STMT = "update item";
const char* const UPDATE_ITEM_SQL =
" \
    UPDATE Item \
    SET itm_description = $1, \
        itm_script_section_ref = $2, \
        itm_order_index = $3, \
        itm_programme_id = $4 \
    WHERE \
        itm_identifier = $5 \
";

const char* const DELETE_ITEM_STMT = "delete item";
const char* const DELETE_ITEM_SQL =
" \
    DELETE FROM Item WHERE itm_identifier = $1 \
";

const char* const INSERT_TAKE_STMT = "insert take";
const char* const INSERT_TAKE_SQL =
" \
    INSERT INTO Take \
    ( \
        tke_identifier, \
        tke_number, \
        tke_comment, \
        tke_result, \
        tke_edit_rate, \
        tke_length, \
        tke_start_position, \
        tke_start_date, \
        tke_recording_location, \
        tke_item_id \
    ) \
    VALUES \
    ($1, $2, $3, $4, ($5, $6), $7, $8, $9, $10, $11) \
";

const char* const UPDATE_TAKE_STMT = "update take";
const char* const UPDATE_TAKE_SQL =
" \
    UPDATE Take \
    SET tke_number = $1, \
        tke_comment = $2, \
        tke_result = $3, \
        tke_edit_rate = ($4, $5), \
        tke_length = $6, \
        tke_start_position = $7, \
        tke_start_date = $8, \
        tke_recording_location = $9, \
        tke_item_id = $10 \
    WHERE \
        tke_identifier = $11 \
";

const char* const DELETE_TAKE_STMT = "delete take";
const char* const DELETE_TAKE_SQL =
" \
    DELETE FROM Take WHERE tke_identifier = $1 \
";


const char* const LOAD_TRANSCODES_WITH_STATUS_STMT = "load transcodes with status";
const char* const LOAD_TRANSCODES_WITH_STATUS_SQL =
" \
    SELECT \
        trc_identifier, \
        trc_source_material_package_id, \
        trc_dest_material_package_id, \
        trc_created::varchar, \
        trc_target_video_resolution_id, \
        trc_status_id \
    FROM Transcode \
    WHERE \
        trc_status_id = $1 \
    ORDER BY trc_created \
";

const char* const INSERT_TRANSCODE_STMT = "insert transcode";
const char* const INSERT_TRANSCODE_SQL =
" \
    INSERT INTO Transcode \
    ( \
        trc_identifier, \
        trc_source_material_package_id, \
        trc_dest_material_package_id, \
        trc_target_video_resolution_id, \
        trc_status_id \
    ) \
    VALUES \
    ($1, $2, $3, $4, $5) \
";

const char* const UPDATE_TRANSCODE_STMT = "update transcode";
const char* const UPDATE_TRANSCODE_SQL =
" \
    UPDATE Transcode \
    SET trc_source_material_package_id = $1, \
        trc_dest_material_package_id = $2,\
        trc_target_video_resolution_id = $3,\
        trc_status_id = $4 \
    WHERE \
        trc_identifier = $5 \
";

const char* const DELETE_TRANSCODE_STMT = "delete transcode";
const char* const DELETE_TRANSCODE_SQL =
" \
    DELETE FROM Transcode WHERE trc_identifier = $1 \
";

const char* const RESET_TRANSCODES_STMT = "reset transcodes";
const char* const RESET_TRANSCODES_SQL =
" \
    UPDATE Transcode \
    SET \
        trc_status_id = $1 \
    WHERE \
        trc_status_id = $2 \
";

const char* const DELETE_TRANSCODES_STMT = "delete transcodes";
const char* const DELETE_TRANSCODES_SQL =
" \
    DELETE FROM Transcode \
    WHERE \
        trc_created < now() - $1 ::interval AND \
        trc_status_id IN ($2) \
";


const char* const LOAD_OR_CREATE_PROJECT_NAME_STMT = "load or create project name";
const char* const LOAD_OR_CREATE_PROJECT_NAME_SQL =
" \
    SELECT load_or_create_project_name($1) \
";

const char* const LOAD_ALL_PROJECT_NAMES_STMT = "load all project names";
const char* const LOAD_ALL_PROJECT_NAMES_SQL =
" \
    SELECT \
        pjn_identifier, \
        pjn_name \
    FROM ProjectName \
";

const char* const DELETE_PROJECT_NAME_STMT = "delete project name";
const char* const DELETE_PROJECT_NAME_SQL =
" \
    DELETE FROM ProjectName WHERE pjn_identifier = $1 \
";


const char* const LOAD_SOURCE_PACKAGE_STMT = "load source package";
const char* const LOAD_SOURCE_PACKAGE_SQL =
" \
    SELECT \
        pkg_identifier, \
        pkg_uid, \
        pkg_name, \
        pkg_creation_date::varchar, \
        pkg_project_name_id, \
        pjn_name, \
        pkg_descriptor_id, \
        pkg_source_config_name \
    FROM Package \
        LEFT OUTER JOIN ProjectName ON (pkg_project_name_id = pjn_identifier) \
    WHERE \
        pkg_name = $1 AND \
        pkg_descriptor_id IS NOT NULL \
";

const char* const LOAD_PACKAGE_STMT = "load package";
const char* const LOAD_PACKAGE_SQL =
" \
    SELECT \
        pkg_identifier, \
        pkg_uid, \
        pkg_name, \
        pkg_creation_date::varchar, \
        pkg_project_name_id, \
        pjn_name, \
        pkg_descriptor_id, \
        pkg_source_config_name \
    FROM Package \
        LEFT OUTER JOIN ProjectName ON (pkg_project_name_id = pjn_identifier) \
    WHERE \
        pkg_identifier = $1 \
";

const char* const LOAD_PACKAGE_WITH_UMID_STMT = "load package with umid";
const char* const LOAD_PACKAGE_WITH_UMID_SQL =
" \
    SELECT \
        pkg_identifier, \
        pkg_uid, \
        pkg_name, \
        pkg_creation_date::varchar, \
        pkg_project_name_id, \
        pjn_name, \
        pkg_descriptor_id, \
        pkg_source_config_name \
    FROM Package \
        LEFT OUTER JOIN ProjectName ON (pkg_project_name_id = pjn_identifier) \
    WHERE \
        pkg_uid = $1 \
";

const char* const LOAD_PACKAGE_USER_COMMENTS_STMT = "load package user comments";
const char* const LOAD_PACKAGE_USER_COMMENTS_SQL =
" \
    SELECT \
        uct_identifier, \
        uct_name, \
        uct_value, \
        uct_position, \
        uct_colour \
    FROM UserComment \
    WHERE \
        uct_package_id = $1 \
    ORDER BY uct_position \
";

const char* const LOAD_ESSENCE_DESCRIPTOR_STMT = "load essence descriptor";
const char* const LOAD_ESSENCE_DESCRIPTOR_SQL =
" \
    SELECT \
        eds_identifier, \
        eds_essence_desc_type, \
        eds_file_location, \
        eds_file_format, \
        eds_video_resolution_id, \
        (eds_image_aspect_ratio).numerator, \
        (eds_image_aspect_ratio).denominator, \
        eds_audio_quantization_bits, \
        eds_spool_number, \
        eds_recording_location \
    FROM EssenceDescriptor \
    WHERE \
        eds_identifier = $1 \
";

const char* const LOAD_TRACKS_STMT = "load tracks";
const char* const LOAD_TRACKS_SQL =
" \
    SELECT \
        trk_identifier, \
        trk_id, \
        trk_number, \
        trk_name, \
        trk_data_def, \
        (trk_edit_rate).numerator, \
        (trk_edit_rate).denominator \
    FROM Track \
    WHERE \
        trk_package_id = $1 \
";

const char* const LOAD_SOURCE_CLIP_STMT = "load source clip";
const char* const LOAD_SOURCE_CLIP_SQL =
" \
    SELECT \
        scp_identifier, \
        scp_source_package_uid, \
        scp_source_track_id, \
        scp_length, \
        scp_position \
    FROM SourceClip \
    WHERE \
        scp_track_id = $1 \
";

const char* const LOCK_SOURCE_PACKAGES_STMT = "lock source packages";
const char* const LOCK_SOURCE_PACKAGES_SQL =
" \
    LOCK TABLE SourcePackageLock \
";

const char* const INSERT_PACKAGE_STMT = "insert package";
const char* const INSERT_PACKAGE_SQL =
" \
    INSERT INTO Package \
    ( \
        pkg_identifier, \
        pkg_uid, \
        pkg_name, \
        pkg_creation_date, \
        pkg_project_name_id, \
        pkg_descriptor_id, \
        pkg_source_config_name \
    ) \
    VALUES \
    ($1, $2, $3, $4, $5, $6, $7) \
";

const char* const UPDATE_PACKAGE_STMT = "update package";
const char* const UPDATE_PACKAGE_SQL =
" \
    UPDATE Package \
    SET pkg_uid = $1, \
        pkg_name = $2, \
        pkg_creation_date = $3, \
        pkg_project_name_id = $4, \
        pkg_descriptor_id = $5, \
        pkg_source_config_name = $6 \
    WHERE \
        pkg_identifier = $7 \
";

const char* const INSERT_PACKAGE_USER_COMMENT_STMT = "insert package user comment";
const char* const INSERT_PACKAGE_USER_COMMENT_SQL =
" \
    INSERT INTO UserComment \
    ( \
        uct_identifier, \
        uct_package_id, \
        uct_name, \
        uct_value, \
        uct_position, \
        uct_colour \
    ) \
    VALUES \
    ($1, $2, $3, $4, $5, $6) \
";

const char* const UPDATE_PACKAGE_USER_COMMENT_STMT = "update package user comment";
const char* const UPDATE_PACKAGE_USER_COMMENT_SQL =
" \
    UPDATE UserComment \
    SET uct_package_id = $1, \
        uct_name = $2, \
        uct_value = $3, \
        uct_position = $4, \
        uct_colour = $5 \
    WHERE \
        uct_identifier = $6 \
";

const char* const INSERT_ESSENCE_DESCRIPTOR_STMT = "insert essence descriptor";
const char* const INSERT_ESSENCE_DESCRIPTOR_SQL =
" \
    INSERT INTO EssenceDescriptor \
    ( \
        eds_identifier, \
        eds_essence_desc_type, \
        eds_file_location, \
        eds_file_format, \
        eds_video_resolution_id, \
        eds_image_aspect_ratio, \
        eds_audio_quantization_bits, \
        eds_spool_number, \
        eds_recording_location \
    ) \
    VALUES \
    ($1, $2, $3, $4, $5, ($6, $7), $8, $9, $10) \
";

const char* const UPDATE_ESSENCE_DESCRIPTOR_STMT = "update essence descriptor";
const char* const UPDATE_ESSENCE_DESCRIPTOR_SQL =
" \
    UPDATE EssenceDescriptor \
    SET eds_essence_desc_type = $1, \
        eds_file_location = $2, \
        eds_file_format = $3, \
        eds_video_resolution_id = $4, \
        eds_image_aspect_ratio = ($5, $6), \
        eds_audio_quantization_bits = $7, \
        eds_spool_number = $8, \
        eds_recording_location = $9 \
    WHERE \
        eds_identifier = $10 \
";

const char* const INSERT_TRACK_STMT = "insert track";
const char* const INSERT_TRACK_SQL =
" \
    INSERT INTO Track \
    ( \
        trk_identifier, \
        trk_id, \
        trk_number, \
        trk_name, \
        trk_data_def, \
        trk_edit_rate, \
        trk_package_id \
    ) \
    VALUES \
    ($1, $2, $3, $4, $5, ($6, $7), $8) \
";

const char* const UPDATE_TRACK_STMT = "update track";
const char* const UPDATE_TRACK_SQL =
" \
    UPDATE Track \
    SET trk_id = $1, \
        trk_number = $2, \
        trk_name = $3, \
        trk_data_def = $4, \
        trk_edit_rate = ($5, $6), \
        trk_package_id = $7 \
    WHERE \
        trk_identifier = $8 \
";

const char* const INSERT_SOURCE_CLIP_STMT = "insert source clip";
const char* const INSERT_SOURCE_CLIP_SQL =
" \
    INSERT INTO SourceClip \
    ( \
        scp_identifier, \
        scp_source_package_uid, \
        scp_source_track_id, \
        scp_length, \
        scp_position, \
        scp_track_id \
    ) \
    VALUES \
    ($1, $2, $3, $4, $5, $6) \
";

const char* const UPDATE_SOURCE_CLIP_STMT = "update source clip";
const char* const UPDATE_SOURCE_CLIP_SQL =
" \
    UPDATE SourceClip \
    SET scp_source_package_uid = $1, \
        scp_source_track_id = $2, \
        scp_length = $3, \
        scp_position = $4, \
        scp_track_id = $5 \
    WHERE \
        scp_identifier = $6 \
";


const char* const LOAD_SOURCE_UID_STMT = "load source uid";
const char* const LOAD_SOURCE_UID_SQL =
"\
    SELECT * \
    FROM SourceClip \
    WHERE \
        scp_source_package_uid = $1 \
";

const char* const DELETE_PACKAGE_STMT = "delete package";
const char* const DELETE_PACKAGE_SQL =
" \
    DELETE FROM Package WHERE pkg_identifier = $1 \
";

const char* const LOAD_REFERENCED_PACKAGE_STMT = "load referenced package";
const char* const LOAD_REFERENCED_PACKAGE_SQL =
" \
    SELECT \
        pkg_identifier \
    FROM \
        Package \
    WHERE \
        pkg_uid = $1 \
";

const char* const LOAD_MATERIAL_1_STMT = "load material 1";
const char* const LOAD_MATERIAL_1_SQL =
" \
    SELECT \
        pkg_identifier \
    FROM \
        Package \
    WHERE \
        pkg_creation_date >= $1 AND \
        pkg_creation_date < $2 AND \
        pkg_descriptor_id IS NULL \
";

const char* const LOAD_MATERIAL_2_STMT = "load material 2";
const char* const LOAD_MATERIAL_2_SQL =
" \
    SELECT \
        pkg_identifier \
    FROM \
        Package, \
        UserComment \
    WHERE \
        uct_package_id = pkg_identifier AND \
        uct_name = $1 AND \
        uct_value = $2 AND \
        pkg_descriptor_id IS NULL \
";

const char* const LOAD_VERSION_STMT = "load version";
const char* const LOAD_VERSION_SQL =
" \
    SELECT \
        ver_version \
    FROM \
        Version \
";

const char* const LOAD_ALL_RESOLUTION_NAMES_STMT = "load all resolution names";
const char* const LOAD_ALL_RESOLUTION_NAMES_SQL =
" \
    SELECT \
        vrn_identifier, \
        vrn_name \
    FROM VideoResolution \
";

const char* const LOAD_ALL_FILE_FORMAT_NAMES_STMT = "load all file format names";
const char* const LOAD_ALL_FILE_FORMAT_NAMES_SQL =
" \
    SELECT \
        fft_identifier, \
        fft_name \
    FROM FileFormat \
";

const char* const LOAD_ALL_TIMECODE_NAMES_STMT = "load all timecode names";
const char* const LOAD_ALL_TIMECODE_NAMES_SQL =
" \
    SELECT \
        tct_identifier, \
        tct_name \
    FROM TimecodeType \
";

const char* const LOAD_LOCATION_NAME_STMT = "load location name";
const char* const LOAD_LOCATION_NAME_SQL =
" \
    SELECT \
        rlc_name \
    FROM RecordingLocation \
    WHERE \
        rlc_identifier = $1 \
";






Database* Database::_instance = 0;
Mutex Database::_databaseMutex = Mutex();

void Database::initialise(string hostname, string dbname, string username, string password,
                          unsigned int initialConnections, unsigned int maxConnections)
{
    LOCK_SECTION(_databaseMutex);

    if (_instance != 0) {
        delete _instance;
        _instance = 0;
    }

    _instance = new Database(hostname, dbname, username, password, initialConnections, maxConnections);
}

void Database::close()
{
    LOCK_SECTION(_databaseMutex);

    delete _instance;
    _instance = 0;
}

Database* Database::getInstance()
{
    LOCK_SECTION(_databaseMutex);

    if (!_instance)
        PA_LOGTHROW(DBException, ("Database has not been initialised"));

    return _instance;
}


Database::Database(string hostname, string dbname, string username, string password,
                   unsigned int initialConnections, unsigned int maxConnections)
{
    _hostname = hostname;
    _dbname = dbname;
    _username = username;
    _password = password;
    _numConnections = 0;
    _maxConnections = maxConnections;
    
    assert(initialConnections > 0 && maxConnections >= initialConnections);

    try
    {
        // fill connection pool with initial set of connections
        unsigned int i;
        for (i = 0; i < initialConnections; i++)
            _connectionPool.push_back(openConnection(hostname, dbname, username, password));

        // check compatibility with database
        checkVersion();
    }
    catch (exception &ex)
    {
        // close connections
        vector<connection*>::iterator iter;
        for (iter = _connectionPool.begin(); iter != _connectionPool.end(); iter++) {
            try
            {
                delete (*iter);
            }
            catch (...) {}
        }

        PA_LOGTHROW(DBException, ("Failed to construct database object:\n%s", ex.what()));
    }
    catch (DBException& ex)
    {
        // close connections
        vector<connection*>::iterator iter;
        for (iter = _connectionPool.begin(); iter != _connectionPool.end(); iter++) {
            try
            {
                delete (*iter);
            }
            catch (...) {}
        }

        throw;
    }
    catch (...)
    {
        // close connections
        vector<connection*>::iterator iter;
        for (iter = _connectionPool.begin(); iter != _connectionPool.end(); iter++) {
            try
            {
                delete (*iter);
            }
            catch (...) {}
        }

        PA_LOGTHROW(DBException, ("Failed to construct database object"));
    }
}

Database::~Database()
{
    while (!_transactionsInUse.empty()) {
        try
        {
            // the side effect of this is that the connection is returned to _connectionPool
            // and the transaction is deleted from _transactionsInUse
            delete *_transactionsInUse.begin();
        }
        catch (...) {}
    }
    
    vector<connection*>::iterator iter;
    for (iter = _connectionPool.begin(); iter != _connectionPool.end(); iter++) {
        try
        {
            delete (*iter);
        }
        catch (...) {}
    }
}

Transaction* Database::getTransaction(string name)
{
    LOCK_SECTION(_connectionMutex);

    connection *conn = 0;
    
    try
    {
        // get connection from pool or open a new connection if none are available
        if (_connectionPool.empty()) {
            if (_transactionsInUse.size() >= static_cast<size_t>(_maxConnections))
                PA_LOGTHROW(DBException, ("No connections available in pool"));

            try
            {
                conn = openConnection(_hostname, _dbname, _username, _password);
            }
            catch (const exception &ex)
            {
                PA_LOGTHROW(DBException, ("Failed to open new database connection:\n%s", ex.what()));
            }
            catch (...)
            {
                PA_LOGTHROW(DBException, ("Failed to open new database connection"));
            }
        } else {
            conn = _connectionPool.front();
            _connectionPool.erase(_connectionPool.begin());
        }
        
        // create transaction using connection
        Transaction *transaction = new Transaction(this, conn, name);
        conn = 0; // transaction now owns it
        _transactionsInUse.push_back(transaction);

        return transaction;
    }
    catch (const exception &ex)
    {
        try
        {
            delete conn;
        }
        catch (...) {}

        PA_LOGTHROW(DBException, ("Failed to get database transaction:\n%s", ex.what()));
    }
    catch (...)
    {
        try
        {
            delete conn;
        }
        catch (...) {}

        PA_LOGTHROW(DBException, ("Failed to get database transaction"));
    }
}

map<long, string> Database::loadLiveRecordingLocations(Transaction *transaction)
{
    Transaction *ts = transaction;
    auto_ptr<Transaction> local_ts;
    if (!ts) {
        local_ts = auto_ptr<Transaction>(getTransaction("LoadLiveRecordingLocations"));
        ts = local_ts.get();
    }
    
    START_WORK
    {
        map<long, string> recording_locations;
        
        result res = ts->prepared(LOAD_REC_LOCATIONS_STMT).exec();

        result::size_type i;
        for (i = 0; i < res.size(); i++)
            recording_locations.insert(make_pair(readLong(res[i][0], 0), readString(res[i][1])));

        return recording_locations;
    }
    END_WORK("LoadLiveRecordingLocations")
}

long Database::saveLiveRecordingLocation(string name, Transaction *transaction)
{
    Transaction *ts = transaction;
    auto_ptr<Transaction> local_ts;
    if (!ts) {
        local_ts = auto_ptr<Transaction>(getTransaction("SaveLiveRecordingLocation"));
        ts = local_ts.get();
    }
    
    START_WORK
    {
        long next_database_id = loadNextId("rlc_id_seq", ts);
        
        ts->prepared(INSERT_REC_LOCATIONS_STMT)
            (next_database_id)
            (name).exec();

        if (!transaction)
            ts->commit();

        return next_database_id;
    }
    END_WORK("SaveLiveRecordingLocation")
}

Recorder* Database::loadRecorder(string name, Transaction *transaction)
{
    Transaction *ts = transaction;
    auto_ptr<Transaction> local_ts;
    if (!ts) {
        local_ts = auto_ptr<Transaction>(getTransaction("LoadRecorder"));
        ts = local_ts.get();
    }
    
    auto_ptr<Recorder> recorder(new Recorder());

    START_WORK
    {
        // load recorder

        result res1 = ts->prepared(LOAD_RECORDER_STMT)(name).exec();
        if (res1.empty())
            PA_LOGTHROW(DBException, ("Recorder with name '%s' does not exist", name.c_str()));

        recorder->wasLoaded(readId(res1[0][0]));
        recorder->name = readString(res1[0][1]);
        long active_recorder_config_id = readLong(res1[0][2], 0);


        // load recorder configurations

        res1 = ts->prepared(LOAD_RECORDER_CONFIG_STMT)(recorder->getDatabaseID()).exec();
        result::size_type i1;
        for (i1 = 0; i1 < res1.size(); i1++) {
            RecorderConfig *recorder_config = new RecorderConfig();
            recorder->setAlternateConfig(recorder_config);
            recorder_config->wasLoaded(readId(res1[i1][0]));
            recorder_config->name = readString(res1[i1][1]);

            if (recorder_config->getDatabaseID() == active_recorder_config_id)
                recorder->setConfig(recorder_config);


            // load recorder parameters

            result res2 = ts->prepared(LOAD_RECORDER_PARAMS_STMT)(recorder_config->getDatabaseID()).exec();

            result::size_type i2;
            for (i2 = 0; i2 < res2.size(); i2++) {
                RecorderParameter rec_param;
                rec_param.wasLoaded(readId(res2[i2][0]));
                rec_param.name = readString(res2[i2][1]);
                rec_param.value = readString(res2[i2][2]);
                rec_param.type = readEnum(res2[i2][3]);
                recorder_config->parameters.insert(make_pair(rec_param.name, rec_param));
            }

            // load input configurations

            res2 = ts->prepared(LOAD_RECORDER_INPUT_CONFIG_STMT)(recorder_config->getDatabaseID()).exec();

            for (i2 = 0; i2 < res2.size(); i2++) {
                RecorderInputConfig *recorder_input_config = new RecorderInputConfig();
                recorder_config->recorderInputConfigs.push_back(recorder_input_config);
                recorder_input_config->wasLoaded(readId(res2[i2][0]));
                recorder_input_config->index = readInt(res2[i2][1], 0);
                recorder_input_config->name = readString(res2[i2][2]);

                
                // load track configurations

                result res3 = ts->prepared(LOAD_RECORDER_INPUT_TRACK_CONFIG_STMT)
                                (recorder_input_config->getDatabaseID()).exec();

                result::size_type i3;
                for (i3 = 0; i3 < res3.size(); i3++) {
                    RecorderInputTrackConfig *recorder_input_track_config = new RecorderInputTrackConfig();
                    recorder_input_config->trackConfigs.push_back(recorder_input_track_config);
                    recorder_input_track_config->wasLoaded(readId(res3[i3][0]));
                    recorder_input_track_config->index = readInt(res3[i3][1], 0);
                    recorder_input_track_config->number = readInt(res3[i3][2], 0);
                    
                    long source_config_id = readLong(res3[i3][3], 0);
                    if (source_config_id != 0) {
                        recorder_input_track_config->sourceTrackID = readInt(res3[i3][4], 0);

                        recorder_input_track_config->sourceConfig =
                            recorder_config->getSourceConfig(source_config_id, recorder_input_track_config->sourceTrackID);
                        if (!recorder_input_track_config->sourceConfig) {
                            // load source config
                            recorder_input_track_config->sourceConfig = loadSourceConfig(source_config_id, ts);
                            recorder_config->sourceConfigs.push_back(recorder_input_track_config->sourceConfig);
                            if (!recorder_input_track_config->sourceConfig->getTrackConfig(
                                    recorder_input_track_config->sourceTrackID))
                            {
                                PA_LOGTHROW(DBException, ("Reference to non-existing track in recorder input track config"));
                            }
                        }
                    }
                }
            }
        }

        if (active_recorder_config_id != 0 && !recorder->hasConfig())
            PA_LOGTHROW(DBException, ("Recorder config with id '%ld' has incorrect link back "
                                      "to recorder '%s'", active_recorder_config_id, name.c_str()));
    }
    END_WORK("LoadRecorder")

    return recorder.release();
}

void Database::saveRecorder(Recorder *recorder, Transaction *transaction)
{
    Transaction *ts = transaction;
    auto_ptr<Transaction> local_ts;
    if (!ts) {
        local_ts = auto_ptr<Transaction>(getTransaction("SaveRecorder"));
        ts = local_ts.get();
    }
    
    START_WORK
    {
        long next_recorder_database_id;

        // save recorder
        
        if (!recorder->isPersistent()) {
            next_recorder_database_id = loadNextId("rer_id_seq", ts);

            ts->registerCommitListener(next_recorder_database_id, recorder);
            
            ts->prepared(INSERT_RECORDER_STMT)
                (next_recorder_database_id)
                (recorder->name)
                (COND_NUM_PARAM(recorder->hasConfig() && recorder->getConfig()->isPersistent(),
                                recorder->getConfig()->getDatabaseID())).exec();
        } else {
            next_recorder_database_id = recorder->getDatabaseID();
            
            ts->prepared(UPDATE_RECORDER_STMT)
                (recorder->name)
                (COND_NUM_PARAM(recorder->hasConfig() && recorder->getConfig()->isPersistent(),
                                recorder->getConfig()->getDatabaseID()))
                (next_recorder_database_id).exec();
        }


        // save recorder configs
        
        vector<RecorderConfig*>::const_iterator iter1;
        for (iter1 = recorder->getAllConfigs().begin(); iter1 != recorder->getAllConfigs().end(); iter1++) {
            RecorderConfig *recorder_config = *iter1;

            long next_recorder_config_database_id;

            if (!recorder_config->isPersistent()) {
                next_recorder_config_database_id = loadNextId("rec_id_seq", ts);
    
                ts->registerCommitListener(next_recorder_config_database_id, recorder_config);
                
                ts->prepared(INSERT_RECORDER_CONFIG_STMT)
                    (next_recorder_config_database_id)
                    (recorder_config->name)
                    (next_recorder_database_id).exec(); 
            } else {
                next_recorder_config_database_id = recorder_config->getDatabaseID();
    
                ts->prepared(UPDATE_RECORDER_CONFIG_STMT)
                    (recorder_config->name)
                    (next_recorder_database_id)
                    (next_recorder_config_database_id).exec();
            }


            // update the recorder if the recorder config is connected and is new
            if (recorder->hasConfig() && recorder_config == recorder->getConfig()) {
                ts->prepared(UPDATE_ACTIVE_RECORDER_CONFIG_STMT)
                    (next_recorder_config_database_id)
                    (next_recorder_database_id).exec();
            }


            // save the recorder parameters

            map<string, RecorderParameter>::iterator iter2;
            for (iter2 = recorder_config->parameters.begin(); iter2 != recorder_config->parameters.end(); iter2++) {
                RecorderParameter &rec_parameter = iter2->second; // using ref so that object in map is updated

                long next_parameter_database_id;

                if (!rec_parameter.isPersistent()) {
                    next_parameter_database_id = loadNextId("rep_id_seq", ts);
        
                    ts->registerCommitListener(next_parameter_database_id, &rec_parameter);
                    
                    ts->prepared(INSERT_RECORDER_PARAM_STMT)
                        (next_parameter_database_id)
                        (rec_parameter.name)
                        (rec_parameter.value)
                        (rec_parameter.type)
                        (next_recorder_config_database_id).exec(); 
                } else {
                    next_parameter_database_id = rec_parameter.getDatabaseID();

                    ts->prepared(UPDATE_RECORDER_PARAM_STMT)
                        (rec_parameter.name)
                        (rec_parameter.value)
                        (rec_parameter.type)
                        (next_recorder_config_database_id)
                        (next_parameter_database_id).exec();
                }
            }


            // save recorder input configs
            
            vector<RecorderInputConfig*>::const_iterator iter3;
            for (iter3 = recorder_config->recorderInputConfigs.begin();
                 iter3 != recorder_config->recorderInputConfigs.end(); iter3++)
            {
                RecorderInputConfig *input_config = *iter3;

                long next_input_config_database_id;

                if (!input_config->isPersistent()) {
                    next_input_config_database_id = loadNextId("ric_id_seq", ts);
        
                    ts->registerCommitListener(next_input_config_database_id, input_config);
                    
                    ts->prepared(INSERT_RECORDER_INPUT_CONFIG_STMT)
                        (next_input_config_database_id)
                        (input_config->index)
                        (input_config->name, !input_config->name.empty())
                        (next_recorder_config_database_id).exec();
                } else {
                    next_input_config_database_id = input_config->getDatabaseID();

                    ts->prepared(UPDATE_RECORDER_INPUT_CONFIG_STMT)
                        (input_config->index)
                        (input_config->name, !input_config->name.empty())
                        (next_recorder_config_database_id)
                        (next_input_config_database_id).exec();
                }


                // save recorder input track configs
                
                vector<RecorderInputTrackConfig*>::const_iterator iter4;
                for (iter4 = input_config->trackConfigs.begin();
                     iter4 != input_config->trackConfigs.end(); iter4++)
                {
                    RecorderInputTrackConfig *input_track_config = *iter4;

                    long next_input_track_config_database_id;

                    if (!input_track_config->isPersistent()) {
                        next_input_track_config_database_id = loadNextId("rtc_id_seq", ts);
            
                        ts->registerCommitListener(next_input_track_config_database_id, input_track_config);
                        
                        ts->prepared(INSERT_RECORDER_INPUT_TRACK_CONFIG_STMT)
                            (next_input_track_config_database_id)
                            (input_track_config->index)
                            (input_track_config->number)
                            (COND_NUM_PARAM(input_track_config->sourceConfig, input_track_config->sourceConfig->getID()))
                            (COND_NUM_PARAM(input_track_config->sourceConfig, input_track_config->sourceTrackID))
                            (next_input_config_database_id).exec();
                    } else {
                        next_input_track_config_database_id = input_track_config->getDatabaseID();

                        ts->prepared(UPDATE_RECORDER_INPUT_TRACK_CONFIG_STMT)
                            (input_track_config->index)
                            (input_track_config->number)
                            (COND_NUM_PARAM(input_track_config->sourceConfig, input_track_config->sourceConfig->getID()))
                            (COND_NUM_PARAM(input_track_config->sourceConfig, input_track_config->sourceTrackID))
                            (next_input_config_database_id)
                            (next_input_track_config_database_id).exec();
                    }
                }
            }
        }

        if (!transaction)
            ts->commit();
    }
    END_WORK("SaveRecorder")
}

void Database::deleteRecorder(Recorder *recorder, Transaction *transaction)
{
    Transaction *ts = transaction;
    auto_ptr<Transaction> local_ts;
    if (!ts) {
        local_ts = auto_ptr<Transaction>(getTransaction("DeleteRecorder"));
        ts = local_ts.get();
    }
    
    START_WORK
    {
        ts->registerCommitListener(0, recorder);
        
        // the database will cascade the delete down to the recorder input track configs
        // so we register additional listeners for those objects
        vector<RecorderConfig*>::const_iterator iter1;
        for (iter1 = recorder->getAllConfigs().begin(); iter1 != recorder->getAllConfigs().end(); iter1++) {
            RecorderConfig* recorder_config = *iter1;
            
            ts->registerCommitListener(0, recorder_config);

            map<string, RecorderParameter>::iterator iter2;
            for (iter2 = recorder_config->parameters.begin(); iter2 != recorder_config->parameters.end(); iter2++) {
                RecorderParameter &recParam = iter2->second; // using ref so that object in map is updated
                ts->registerCommitListener(0, &recParam);
            }

            vector<RecorderInputConfig*>::const_iterator iter3;
            for (iter3 = recorder_config->recorderInputConfigs.begin();
                 iter3 != recorder_config->recorderInputConfigs.end(); iter3++)
            {
                RecorderInputConfig* input_config = *iter3;
                ts->registerCommitListener(0, input_config);

                vector<RecorderInputTrackConfig*>::const_iterator iter4;
                for (iter4 = input_config->trackConfigs.begin(); iter4 != input_config->trackConfigs.end(); iter4++) {
                    RecorderInputTrackConfig *track_config = *iter4;
                    ts->registerCommitListener(0, track_config);

                }
            }
        }

        ts->prepared(DELETE_RECORDER_STMT)(recorder->getDatabaseID()).exec();

        if (!transaction)
            ts->commit();
    }
    END_WORK("DeleteRecorder")
}

SourceConfig* Database::loadSourceConfig(long database_id, Transaction *transaction)
{
    Transaction *ts = transaction;
    auto_ptr<Transaction> local_ts;
    if (!ts) {
        local_ts = auto_ptr<Transaction>(getTransaction("LoadSourceConfig"));
        ts = local_ts.get();
    }
    
    auto_ptr<SourceConfig> source_config(new SourceConfig());
    
    START_WORK
    {
        result res = ts->prepared(LOAD_SOURCE_CONFIG_STMT)(database_id).exec();
        if (res.empty())
            PA_LOGTHROW(DBException, ("Source config %ld does not exist in database", database_id));
        
        source_config->wasLoaded(readId(res[0][0]));
        source_config->name = readString(res[0][1]);
        source_config->type = readEnum(res[0][2]);
        if (source_config->type == TAPE_SOURCE_CONFIG_TYPE)
            source_config->spoolNumber = readString(res[0][3]);
        else // LIVE_SOURCE_CONFIG_TYPE
            source_config->recordingLocation = readLong(res[0][4], 0);


        // load track configurations

        res = ts->prepared(LOAD_SOURCE_TRACK_CONFIG_STMT)(source_config->getDatabaseID()).exec();

        result::size_type i;
        for (i = 0; i < res.size(); i++) {
            SourceTrackConfig *source_track_config = new SourceTrackConfig();
            source_config->trackConfigs.push_back(source_track_config);
            source_track_config->wasLoaded(readId(res[i][0]));
            source_track_config->id = readInt(res[i][1], 0);
            source_track_config->number = readInt(res[i][2], 0);
            source_track_config->name = readString(res[i][3]);
            source_track_config->dataDef = readEnum(res[i][4]);
            source_track_config->editRate = readRational(res[i][5], res[i][6]);
            source_track_config->length = readInt64(res[i][7], 0);
        }
    }
    END_WORK("LoadSourceConfig")

    return source_config.release();
}

SourceConfig* Database::loadSourceConfig(string name, bool assume_exists, Transaction *transaction)
{
    Transaction *ts = transaction;
    auto_ptr<Transaction> local_ts;
    if (!ts) {
        local_ts = auto_ptr<Transaction>(getTransaction("LoadSourceConfig(name)"));
        ts = local_ts.get();
    }
    
    auto_ptr<SourceConfig> source_config(new SourceConfig());
    
    START_WORK
    {
        result res = ts->prepared(LOAD_NAMED_SOURCE_CONFIG_STMT)(name).exec();
        if (res.empty()) {
            if (assume_exists)
                PA_LOGTHROW(DBException, ("Source config '%s' does not exist in database", name.c_str()))
            else
                return 0;
        }
        
        source_config->wasLoaded(readId(res[0][0]));
        source_config->name = readString(res[0][1]);
        source_config->type = readEnum(res[0][2]);
        if (source_config->type == TAPE_SOURCE_CONFIG_TYPE)
            source_config->spoolNumber = readString(res[0][3]);
        else // LIVE_SOURCE_CONFIG_TYPE
            source_config->recordingLocation = readLong(res[0][4], 0);


        // load track configurations

        res = ts->prepared(LOAD_SOURCE_TRACK_CONFIG_STMT)(source_config->getDatabaseID()).exec();

        result::size_type i;
        for (i = 0; i < res.size(); i++) {
            SourceTrackConfig *source_track_config = new SourceTrackConfig();
            source_config->trackConfigs.push_back(source_track_config);
            source_track_config->wasLoaded(readId(res[i][0]));
            source_track_config->id = readInt(res[i][1], 0);
            source_track_config->number = readInt(res[i][2], 0);
            source_track_config->name = readString(res[i][3]);
            source_track_config->dataDef = readEnum(res[i][4]);
            source_track_config->editRate = readRational(res[i][5], res[i][6]);
            source_track_config->length = readInt64(res[i][7], 0);
        }
    }
    END_WORK("LoadSourceConfig(name)")

    return source_config.release();
}

void Database::saveSourceConfig(SourceConfig *config, Transaction *transaction)
{
    Transaction *ts = transaction;
    auto_ptr<Transaction> local_ts;
    if (!ts) {
        local_ts = auto_ptr<Transaction>(getTransaction("SaveSourceConfig"));
        ts = local_ts.get();
    }
    
    START_WORK
    {
        long next_config_database_id;

        // insert
        if (!config->isPersistent()) {
            next_config_database_id = loadNextId("scf_id_seq", ts);

            ts->registerCommitListener(next_config_database_id, config);
            
            ts->prepared(INSERT_SOURCE_CONFIG_STMT)
                (next_config_database_id)
                (config->name)
                (config->type)
                (config->spoolNumber, config->type == TAPE_SOURCE_CONFIG_TYPE)
                (config->recordingLocation, config->type == LIVE_SOURCE_CONFIG_TYPE).exec();
        } else {
            next_config_database_id = config->getDatabaseID();

            ts->prepared(UPDATE_SOURCE_CONFIG_STMT)
                (config->name)
                (config->type)
                (config->spoolNumber, config->type == TAPE_SOURCE_CONFIG_TYPE)
                (config->recordingLocation, config->type == LIVE_SOURCE_CONFIG_TYPE)
                (next_config_database_id).exec();
        }


        // save source track configs
        
        vector<SourceTrackConfig*>::const_iterator iter;
        for (iter = config->trackConfigs.begin(); iter != config->trackConfigs.end(); iter++) {
            SourceTrackConfig *track_config = *iter;

            long next_track_config_database_id;

            if (!track_config->isPersistent()) {
                next_track_config_database_id = loadNextId("sct_id_seq", ts);
    
                ts->registerCommitListener(next_config_database_id, track_config);
                
                ts->prepared(INSERT_SOURCE_TRACK_CONFIG_STMT)
                    (next_track_config_database_id)
                    (track_config->id)
                    (track_config->number)
                    (track_config->name)
                    (track_config->dataDef)
                    (track_config->editRate.numerator)
                    (track_config->editRate.denominator)
                    (track_config->length)
                    (next_config_database_id).exec();
            } else {
                next_track_config_database_id = track_config->getDatabaseID();

                ts->prepared(UPDATE_SOURCE_TRACK_CONFIG_STMT)
                    (track_config->id)
                    (track_config->number)
                    (track_config->name)
                    (track_config->dataDef)
                    (track_config->editRate.numerator)
                    (track_config->editRate.denominator)
                    (track_config->length)
                    (next_config_database_id)
                    (next_track_config_database_id).exec();
            }
        }

        if (!transaction)
            ts->commit();
    }
    END_WORK("SaveSourceConfig")
}

void Database::deleteSourceConfig(SourceConfig *config, Transaction *transaction)
{
    Transaction *ts = transaction;
    auto_ptr<Transaction> local_ts;
    if (!ts) {
        local_ts = auto_ptr<Transaction>(getTransaction("DeleteSourceConfig"));
        ts = local_ts.get();
    }
    
    START_WORK
    {
        ts->registerCommitListener(0, config);

        // the database will cascade the delete down to the source track configs
        // so we register additional listeners for those objects
        vector<SourceTrackConfig*>::const_iterator iter;
        for (iter = config->trackConfigs.begin(); iter != config->trackConfigs.end(); iter++)
            ts->registerCommitListener(0, *iter);

        ts->prepared(DELETE_SOURCE_CONFIG_STMT)(config->getDatabaseID()).exec();

        if (!transaction)
            ts->commit();
    }
    END_WORK("DeleteSourceConfig")
}

vector<RouterConfig*> Database::loadAllRouterConfigs(Transaction *transaction)
{
    Transaction *ts = transaction;
    auto_ptr<Transaction> local_ts;
    if (!ts) {
        local_ts = auto_ptr<Transaction>(getTransaction("LoadAllRouterConfigs"));
        ts = local_ts.get();
    }
    
    VectorGuard<RouterConfig> all_router_configs;

    START_WORK
    {
        result res = ts->prepared(LOAD_ALL_ROUTER_CONFIGS_STMT).exec();

        result::size_type i;
        for (i = 0; i < res.size(); i++) {
            RouterConfig *router_config = loadRouterConfig(readString(res[i][1]));
            if (router_config)
                all_router_configs.get().push_back(router_config);
        }
    }
    END_WORK("LoadAllRouterConfigs")

    return all_router_configs.release();
}

RouterConfig* Database::loadRouterConfig(string name, Transaction *transaction)
{
    Transaction *ts = transaction;
    auto_ptr<Transaction> local_ts;
    if (!ts) {
        local_ts = auto_ptr<Transaction>(getTransaction("LoadRouterConfig"));
        ts = local_ts.get();
    }
    
    auto_ptr<RouterConfig> router_config(new RouterConfig());

    START_WORK
    {
        // load router config

        result res = ts->prepared(LOAD_ROUTER_CONFIG_STMT)(name).exec();
        if (res.empty())
            PA_LOGTHROW(DBException, ("Router config with name '%s' does not exist", name.c_str()));

        router_config->wasLoaded(readId(res[0][0]));
        router_config->name = readString(res[0][1]);


        // load input configurations

        res = ts->prepared(LOAD_ROUTER_INPUT_CONFIG_STMT)(router_config->getDatabaseID()).exec();

        result::size_type i;
        for (i = 0; i < res.size(); i++) {
            RouterInputConfig *router_input_config = new RouterInputConfig();
            router_config->inputConfigs.push_back(router_input_config);
            router_input_config->wasLoaded(readId(res[i][0]));
            router_input_config->index = readInt(res[i][1], 0);
            router_input_config->name = readString(res[i][2]);
            
            long source_config_id = readLong(res[i][3], 0);
            if (source_config_id != 0) {
                router_input_config->sourceTrackID = readInt(res[i][4], 0);

                router_input_config->sourceConfig = router_config->getSourceConfig(source_config_id,
                                                                                   router_input_config->sourceTrackID);
                if (!router_input_config->sourceConfig) {
                    // load source config
                    router_input_config->sourceConfig = loadSourceConfig(source_config_id, ts);
                    router_config->sourceConfigs.push_back(router_input_config->sourceConfig);
                    if (!router_input_config->sourceConfig->getTrackConfig(router_input_config->sourceTrackID))
                        PA_LOGTHROW(DBException, ("Reference to non-existing track in router input config"));
                }
            }
        }


        // load output configurations

        res = ts->prepared(LOAD_ROUTER_OUTPUT_CONFIG_STMT)(router_config->getDatabaseID()).exec();
        for (i = 0; i < res.size(); i++) {
            RouterOutputConfig *router_output_config = new RouterOutputConfig();
            router_config->outputConfigs.push_back(router_output_config);
            router_output_config->wasLoaded(readId(res[i][0]));
            router_output_config->index = readInt(res[i][1], 0);
            router_output_config->name = readString(res[i][2]);
        }
    }
    END_WORK("LoadRouterConfig")

    return router_config.release();
}

vector<MCClipDef*> Database::loadAllMultiCameraClipDefs(Transaction *transaction)
{
    Transaction *ts = transaction;
    auto_ptr<Transaction> local_ts;
    if (!ts) {
        local_ts = auto_ptr<Transaction>(getTransaction("LoadAllMultiCameraClipDefs"));
        ts = local_ts.get();
    }
    
    VectorGuard<MCClipDef> all_mc_clip_defs;

    START_WORK
    {
        // load clip def
        
        result res1 = ts->prepared(LOAD_ALL_MC_CLIP_DEFS_STMT).exec();

        result::size_type i1;
        for (i1 = 0; i1 < res1.size(); i1++) {
            MCClipDef *mc_clip_def = new MCClipDef();
            all_mc_clip_defs.get().push_back(mc_clip_def);

            mc_clip_def->wasLoaded(readId(res1[i1][0]));
            mc_clip_def->name = readString(res1[i1][1]);


            // load track defs

            result res2 = ts->prepared(LOAD_MC_TRACK_DEF_STMT)(mc_clip_def->getDatabaseID()).exec();
    
            result::size_type i2;
            for (i2 = 0; i2 < res2.size(); i2++) {
                MCTrackDef *mc_track_def = new MCTrackDef();
                uint32_t index = readInt(res2[i2][1], 0);
                mc_clip_def->trackDefs.insert(pair<uint32_t, MCTrackDef*>(index, mc_track_def));
                mc_track_def->wasLoaded(readId(res2[i2][0]));
                mc_track_def->index = index;
                mc_track_def->number = readInt(res2[i2][2], 0);


                // load selector defs

                result res3 = ts->prepared(LOAD_MC_SELECTOR_DEF_STMT)(mc_track_def->getDatabaseID()).exec();
        
                result::size_type i3;
                for (i3 = 0; i3 < res3.size(); i3++) {
                    MCSelectorDef *mc_selector_def = new MCSelectorDef();
                    uint32_t index = readInt(res3[i3][1], 0);
                    mc_track_def->selectorDefs.insert(pair<uint32_t, MCSelectorDef*>(index, mc_selector_def));
                    mc_selector_def->wasLoaded(readId(res3[i3][0]));
                    mc_selector_def->index = index;
                    long source_config_id = readLong(res3[i3][2], 0);
                    if (source_config_id != 0) {
                        mc_selector_def->sourceTrackID = readInt(res3[i3][3], 0);

                        mc_selector_def->sourceConfig = mc_clip_def->getSourceConfig(source_config_id,
                                                                                     mc_selector_def->sourceTrackID);
                        if (!mc_selector_def->sourceConfig) {
                            // load source config
                            mc_selector_def->sourceConfig = loadSourceConfig(source_config_id, ts);
                            mc_clip_def->sourceConfigs.push_back(mc_selector_def->sourceConfig);
                            if (!mc_selector_def->sourceConfig->getTrackConfig(mc_selector_def->sourceTrackID))
                                PA_LOGTHROW(DBException, ("Reference to non-existing track in multi-camera selector def"));
                        }
                    }
                }
            }
        }
    }
    END_WORK("LoadAllMultiCameraClipDefs")

    return all_mc_clip_defs.release();
}

MCClipDef* Database::loadMultiCameraClipDef(string name, bool assume_exists, Transaction *transaction)
{
    Transaction *ts = transaction;
    auto_ptr<Transaction> local_ts;
    if (!ts) {
        local_ts = auto_ptr<Transaction>(getTransaction("LoadMultiCameraClipDef"));
        ts = local_ts.get();
    }
    
    auto_ptr<MCClipDef> mc_clip_def(new MCClipDef());

    START_WORK
    {
        // load multi-camera clip

        result res1 = ts->prepared(LOAD_MC_CLIP_DEF_STMT)(name).exec();
        if (res1.empty()) {
            if (assume_exists)
                PA_LOGTHROW(DBException, ("Multi-camera clip with name '%s' does not exist", name.c_str()))
            else
                return 0;
        }

        mc_clip_def->wasLoaded(readId(res1[0][0]));
        mc_clip_def->name = readString(res1[0][1]);

        
        // load track defs

        result res2 = ts->prepared(LOAD_MC_TRACK_DEF_STMT)(mc_clip_def->getDatabaseID()).exec();

        result::size_type i2;
        for (i2 = 0; i2 < res2.size(); i2++) {
            MCTrackDef *mc_track_def = new MCTrackDef();
            uint32_t index = readInt(res2[i2][1], 0);
            mc_clip_def->trackDefs.insert(pair<uint32_t, MCTrackDef*>(index, mc_track_def));
            mc_track_def->wasLoaded(readId(res2[i2][0]));
            mc_track_def->index = index;
            mc_track_def->number = readInt(res2[i2][2], 0);


            // load selector defs

            result res3 = ts->prepared(LOAD_MC_SELECTOR_DEF_STMT)(mc_track_def->getDatabaseID()).exec();
    
            result::size_type i3;
            for (i3 = 0; i3 < res3.size(); i3++) {
                MCSelectorDef *mc_selector_def = new MCSelectorDef();
                uint32_t index = readInt(res3[i3][1], 0);
                mc_track_def->selectorDefs.insert(pair<uint32_t, MCSelectorDef*>(index, mc_selector_def));
                mc_selector_def->wasLoaded(readId(res3[i3][0]));
                mc_selector_def->index = index;
                long source_config_id = readLong(res3[i3][2], 0);
                if (source_config_id != 0) {
                    mc_selector_def->sourceTrackID = readInt(res3[i3][3], 0);

                    mc_selector_def->sourceConfig = mc_clip_def->getSourceConfig(source_config_id,
                                                                                 mc_selector_def->sourceTrackID);
                    if (!mc_selector_def->sourceConfig) {
                        // load source config
                        mc_selector_def->sourceConfig = loadSourceConfig(source_config_id, ts);
                        mc_clip_def->sourceConfigs.push_back(mc_selector_def->sourceConfig);
                        if (!mc_selector_def->sourceConfig->getTrackConfig(mc_selector_def->sourceTrackID))
                            PA_LOGTHROW(DBException, ("Reference to non-existing track in multi-camera selector def"));
                    }
                }
            }
        }
    }
    END_WORK("LoadMultiCameraClipDef")

    return mc_clip_def.release();
}

void Database::saveMultiCameraClipDef(MCClipDef *mc_clip_def, Transaction *transaction)
{
    Transaction *ts = transaction;
    auto_ptr<Transaction> local_ts;
    if (!ts) {
        local_ts = auto_ptr<Transaction>(getTransaction("SaveMultiCameraClipDef"));
        ts = local_ts.get();
    }
    
    START_WORK
    {
        // save clip def
        
        long next_clip_def_database_id;

        if (!mc_clip_def->isPersistent()) {
            next_clip_def_database_id = loadNextId("mcd_id_seq", ts);
            
            ts->registerCommitListener(next_clip_def_database_id, mc_clip_def);
            
            ts->prepared(INSERT_MC_CLIP_DEF_STMT)
                (next_clip_def_database_id)
                (mc_clip_def->name).exec();
        } else {
            next_clip_def_database_id = mc_clip_def->getDatabaseID();
            
            ts->prepared(UPDATE_MC_CLIP_DEF_STMT)
                (mc_clip_def->name)
                (next_clip_def_database_id).exec();
        }


        // save track defs
        
        map<uint32_t, MCTrackDef*>::const_iterator track_iter;
        for (track_iter = mc_clip_def->trackDefs.begin(); track_iter != mc_clip_def->trackDefs.end(); track_iter++) {
            MCTrackDef* track_def = track_iter->second;

            long next_track_def_database_id;

            if (!track_def->isPersistent()) {
                next_track_def_database_id = loadNextId("mct_id_seq", ts);
                
                ts->registerCommitListener(next_track_def_database_id, track_def);
                
                ts->prepared(INSERT_MC_TRACK_DEF_STMT)
                    (next_track_def_database_id)
                    (track_def->index)
                    (track_def->number)
                    (next_clip_def_database_id).exec();
            } else {
                next_track_def_database_id = track_def->getDatabaseID();

                ts->prepared(UPDATE_MC_TRACK_DEF_STMT)
                    (track_def->index)
                    (track_def->number)
                    (next_clip_def_database_id)
                    (next_track_def_database_id).exec();
            }


            // save selector defs

            map<uint32_t, MCSelectorDef*>::const_iterator sel_iter;
            for (sel_iter = track_def->selectorDefs.begin(); sel_iter != track_def->selectorDefs.end(); sel_iter++) {
                MCSelectorDef *sel_def = sel_iter->second;
    
                long next_selector_def_database_id;
    
                if (!sel_def->isPersistent()) {
                    next_selector_def_database_id = loadNextId("mcs_id_seq", ts);
                    
                    ts->registerCommitListener(next_selector_def_database_id, sel_def);
                    
                    ts->prepared(INSERT_MC_SELECTOR_DEF_STMT)
                        (next_selector_def_database_id)
                        (sel_def->index)
                        (COND_NUM_PARAM(sel_def->sourceConfig && sel_def->sourceConfig->isPersistent(),
                                        sel_def->sourceConfig->getDatabaseID()))
                        (COND_NUM_PARAM(sel_def->sourceConfig && sel_def->sourceConfig->isPersistent(),
                                        sel_def->sourceTrackID))
                        (next_track_def_database_id).exec();
                } else {
                    next_selector_def_database_id = sel_def->getDatabaseID();
                    
                    ts->prepared(UPDATE_MC_SELECTOR_DEF_STMT)
                        (sel_def->index)
                        (COND_NUM_PARAM(sel_def->sourceConfig && sel_def->sourceConfig->isPersistent(),
                                        sel_def->sourceConfig->getDatabaseID()))
                        (COND_NUM_PARAM(sel_def->sourceConfig && sel_def->sourceConfig->isPersistent(),
                                        sel_def->sourceTrackID))
                        (next_track_def_database_id)
                        (next_selector_def_database_id).exec();
                }
            }
        }

        if (!transaction)
            ts->commit();
    }
    END_WORK("SaveMultiCameraClipDef")
}

void Database::deleteMultiCameraClipDef(MCClipDef *mc_clip_def, Transaction *transaction)
{
    Transaction *ts = transaction;
    auto_ptr<Transaction> local_ts;
    if (!ts) {
        local_ts = auto_ptr<Transaction>(getTransaction("DeleteMultiCameraClipDef"));
        ts = local_ts.get();
    }
    
    START_WORK
    {
        ts->prepared(DELETE_MC_CLIP_DEF_STMT)(mc_clip_def->getDatabaseID()).exec();

        ts->registerCommitListener(0, mc_clip_def);
        
        // the database will cascade the delete down to the mc tracks and mc selectors
        // so we register additional listeners for those objects
        map<uint32_t, MCTrackDef*>::const_iterator track_iter;
        for (track_iter = mc_clip_def->trackDefs.begin(); track_iter != mc_clip_def->trackDefs.end(); track_iter++) {
            ts->registerCommitListener(0, track_iter->second);

            map<uint32_t, MCSelectorDef*>::const_iterator sel_iter;
            for (sel_iter = track_iter->second->selectorDefs.begin();
                 sel_iter != track_iter->second->selectorDefs.end(); sel_iter++)
            {
                ts->registerCommitListener(0, sel_iter->second);
            }
        }

        if (!transaction)
            ts->commit();
    }
    END_WORK("DeleteMultiCameraClipDef")
}

vector<MCCut*> Database::loadAllMultiCameraCuts(Transaction *transaction)
{
    Transaction *ts = transaction;
    auto_ptr<Transaction> local_ts;
    if (!ts) {
        local_ts = auto_ptr<Transaction>(getTransaction("LoadAllMultiCameraCuts"));
        ts = local_ts.get();
    }
    
    VectorGuard<MCCut> all_mc_cuts;

    START_WORK
    {
        result res = ts->prepared(LOAD_ALL_MC_CUTS_STMT).exec();
        
        result::size_type i;
        for (i = 0; i < res.size(); i++) {
            MCCut *mc_cut = new MCCut();
            all_mc_cuts.get().push_back(mc_cut);

            mc_cut->wasLoaded(readId(res[i][0]));
            mc_cut->mcTrackId = readLong(res[i][1], 0);
            mc_cut->mcSelectorIndex = readInt(res[i][2], 0);
            mc_cut->cutDate = readDate(res[i][3]);
            mc_cut->position = readInt64(res[i][4], 0);
            mc_cut->editRate = readRational(res[i][5], res[i][6]);
        }
    }
    END_WORK("LoadAllMultiCameraCuts")

    return all_mc_cuts.release();
}

vector<MCCut*> Database::loadMultiCameraCuts(MCTrackDef *mc_track_def, Date start_date, int64_t start_timecode,
                                             Date end_date, int64_t end_timecode, Transaction *transaction)
{
    Transaction *ts = transaction;
    auto_ptr<Transaction> local_ts;
    if (!ts) {
        local_ts = auto_ptr<Transaction>(getTransaction("LoadAllMultiCameraCuts"));
        ts = local_ts.get();
    }
    
    long mc_track_def_id = 0;
    if (mc_track_def)
        mc_track_def_id = mc_track_def->getDatabaseID();

    VectorGuard<MCCut> mc_cuts;

    START_WORK
    {
        result res = ts->prepared(LOAD_MC_CUTS_STMT)
            (writeDate(start_date))
            (writeDate(end_date))
            (mc_track_def_id).exec();
        
        result::size_type i;
        for (i = 0; i < res.size(); i++) {
            MCCut *mcCut = new MCCut();

            mcCut->wasLoaded(readId(res[i][0]));
            mcCut->mcTrackId = readId(res[i][1]);
            mcCut->mcSelectorIndex = readInt(res[i][2], 0);
            mcCut->cutDate = readDate(res[i][3]);
            mcCut->position = readInt64(res[i][4], 0);
            mcCut->editRate = readRational(res[i][5], res[i][6]);

            if ((mcCut->cutDate == start_date && mcCut->position < start_timecode)
                || (mcCut->cutDate == end_date && mcCut->position >= end_timecode))
            {
                // Reject cuts not in correct timecode range
                delete mcCut;
            }
            else
            {
                mc_cuts.get().push_back(mcCut);
            }
        }
    }
    END_WORK("LoadAllMultiCameraCuts")

    return mc_cuts.release();
}

void Database::saveMultiCameraCut(MCCut *mc_cut, Transaction *transaction)
{
    Transaction *ts = transaction;
    auto_ptr<Transaction> local_ts;
    if (!ts) {
        local_ts = auto_ptr<Transaction>(getTransaction("SaveMultiCameraCut"));
        ts = local_ts.get();
    }
    
    START_WORK
    {
        long next_database_id;

        if (!mc_cut->isPersistent()) {
            next_database_id = loadNextId("mcc_id_seq", ts);

            ts->registerCommitListener(next_database_id, mc_cut);
            
            ts->prepared(INSERT_MC_CUT_STMT)
                (next_database_id)
                (mc_cut->mcTrackId)
                (mc_cut->mcSelectorIndex)
                (writeDate(mc_cut->cutDate))
                (mc_cut->position)
                (mc_cut->editRate.numerator)
                (mc_cut->editRate.denominator).exec();
        } else {
            next_database_id = mc_cut->getDatabaseID();

            ts->prepared(UPDATE_MC_CUT_STMT)
                (mc_cut->mcTrackId)
                (mc_cut->mcSelectorIndex)
                (writeDate(mc_cut->cutDate))
                (mc_cut->position)
                (mc_cut->editRate.numerator)
                (mc_cut->editRate.denominator)
                (next_database_id).exec();
        }

        if (!transaction)
            ts->commit();
    }
    END_WORK("SaveMultiCameraCut")
}

vector<Series*> Database::loadAllSeries(Transaction *transaction)
{
    Transaction *ts = transaction;
    auto_ptr<Transaction> local_ts;
    if (!ts) {
        local_ts = auto_ptr<Transaction>(getTransaction("LoadAllSeries"));
        ts = local_ts.get();
    }
    
    VectorGuard<Series> all_series;

    START_WORK
    {
        result res = ts->prepared(LOAD_ALL_SERIES_STMT).exec();
        
        result::size_type i;
        for (i = 0; i < res.size(); i++) {
            Series *series = new Series();
            all_series.get().push_back(series);

            series->wasLoaded(readId(res[i][0]));
            series->name = readString(res[i][1]);
        }
    }
    END_WORK("LoadAllSeries")

    return all_series.release();
}

void Database::loadProgrammesInSeries(Series *series, Transaction *transaction)
{
    Transaction *ts = transaction;
    auto_ptr<Transaction> local_ts;
    if (!ts) {
        local_ts = auto_ptr<Transaction>(getTransaction("LoadProgrammesInSeries"));
        ts = local_ts.get();
    }
    
    // remove existing in-memory programmes first
    series->removeAllProgrammes();

    START_WORK
    {
        result res = ts->prepared(LOAD_PROGRAMMES_IN_SERIES_STMT)(series->getDatabaseID()).exec();
        
        result::size_type i;
        for (i = 0; i < res.size(); i++) {
            Programme *programme = series->createProgramme();

            programme->wasLoaded(readId(res[i][0]));
            programme->name = readString(res[i][1]);
        }
    }
    END_WORK("LoadProgrammesInSeries")
}

void Database::saveSeries(Series *series, Transaction *transaction)
{
    if (series->name.empty())
        PA_LOGTHROW(DBException, ("Series has empty name"));

    Transaction *ts = transaction;
    auto_ptr<Transaction> local_ts;
    if (!ts) {
        local_ts = auto_ptr<Transaction>(getTransaction("SaveSeries"));
        ts = local_ts.get();
    }
    
    START_WORK
    {
        long next_database_id;

        if (!series->isPersistent()) {
            next_database_id = loadNextId("srs_id_seq", ts);

            ts->registerCommitListener(next_database_id, series);
            
            ts->prepared(INSERT_SERIES_STMT)
                (next_database_id)
                (series->name).exec();
        } else {
            next_database_id = series->getDatabaseID();

            ts->prepared(UPDATE_SERIES_STMT)
                (series->name)
                (next_database_id).exec();
        }

        if (!transaction)
            ts->commit();
    }
    END_WORK("SaveSeries")
}

void Database::deleteSeries(Series *series, Transaction *transaction)
{
    Transaction *ts = transaction;
    auto_ptr<Transaction> local_ts;
    if (!ts) {
        local_ts = auto_ptr<Transaction>(getTransaction("DeleteSeries"));
        ts = local_ts.get();
    }
    
    START_WORK
    {
        ts->prepared(DELETE_SERIES_STMT)(series->getDatabaseID()).exec();

        ts->registerCommitListener(0, series);

        // the database will cascade the delete down to the take
        // so we register additional listeners for those objects
        vector<Programme*>::const_iterator iter1;
        for (iter1 = series->getProgrammes().begin(); iter1 != series->getProgrammes().end(); iter1++) {
            Programme *programme = *iter1;
            ts->registerCommitListener(0, programme);

            vector<Item*>::const_iterator iter2;
            for (iter2 = programme->getItems().begin(); iter2 != programme->getItems().end(); iter2++) {
                Item *item = *iter2;
                ts->registerCommitListener(0, item);

                vector<Take*>::const_iterator iter3;
                for (iter3 = item->getTakes().begin(); iter3 != item->getTakes().end(); iter3++) {
                    Take *take = *iter3;
                    ts->registerCommitListener(0, take);
                }
            }
        }

        if (!transaction)
            ts->commit();
    }
    END_WORK("DeleteSeries")
}

void Database::loadItemsInProgramme(Programme *programme, Transaction *transaction)
{
    Transaction *ts = transaction;
    auto_ptr<Transaction> local_ts;
    if (!ts) {
        local_ts = auto_ptr<Transaction>(getTransaction("LoadItemsInProgramme"));
        ts = local_ts.get();
    }
    
    // remove existing in-memory items first
    programme->removeAllItems();

    START_WORK
    {
        result res = ts->prepared(LOAD_ITEMS_IN_PROGRAMME_STMT)(programme->getDatabaseID()).exec();

        result::size_type i;
        for (i = 0; i < res.size(); i++) {
            Item *item = programme->appendNewItem();

            item->wasLoaded(readId(res[i][0]));
            item->description = readString(res[i][1]);
            item->scriptSectionRefs = getScriptReferences(readString(res[i][2]));
            item->setDBOrderIndex(readLong(res[i][3], 0));
        }
    }
    END_WORK("LoadItemsInProgramme")
}

void Database::saveProgramme(Programme *programme, Transaction *transaction)
{
    if (programme->name.empty())
        PA_LOGTHROW(DBException, ("Programme has empty name"));

    Transaction *ts = transaction;
    auto_ptr<Transaction> local_ts;
    if (!ts) {
        local_ts = auto_ptr<Transaction>(getTransaction("SaveProgramme"));
        ts = local_ts.get();
    }
    
    START_WORK
    {
        long next_database_id;

        if (!programme->isPersistent()) {
            next_database_id = loadNextId("prg_id_seq", ts);

            ts->registerCommitListener(next_database_id, programme);
            
            ts->prepared(INSERT_PROGRAMME_STMT)
                (next_database_id)
                (programme->name)
                (programme->getSeriesID()).exec();
        } else {
            next_database_id = programme->getDatabaseID();

            ts->prepared(UPDATE_PROGRAMME_STMT)
                (programme->name)
                (programme->getSeriesID())
                (next_database_id).exec();
        }

        if (!transaction)
            ts->commit();
    }
    END_WORK("SaveProgramme")
}

void Database::deleteProgramme(Programme *programme, Transaction *transaction)
{
    Transaction *ts = transaction;
    auto_ptr<Transaction> local_ts;
    if (!ts) {
        local_ts = auto_ptr<Transaction>(getTransaction("DeleteProgramme"));
        ts = local_ts.get();
    }
    
    START_WORK
    {
        ts->prepared(DELETE_PROGRAMME_STMT)(programme->getDatabaseID()).exec();

        ts->registerCommitListener(0, programme);

        // the database will cascade the delete down to take
        // so we register additional listeners for those objects
        vector<Item*>::const_iterator iter1;
        for (iter1 = programme->getItems().begin(); iter1 != programme->getItems().end(); iter1++) {
            Item *item = *iter1;
            ts->registerCommitListener(0, item);

            vector<Take*>::const_iterator iter2;
            for (iter2 = item->getTakes().begin(); iter2 != item->getTakes().end(); iter2++) {
                Take *take = *iter2;
                ts->registerCommitListener(0, take);
            }
        }

        if (!transaction)
            ts->commit();
    }
    END_WORK("DeleteProgramme")
}

void Database::updateItemsOrder(Programme *programme, Transaction *transaction)
{
    Transaction *ts = transaction;
    auto_ptr<Transaction> local_ts;
    if (!ts) {
        local_ts = auto_ptr<Transaction>(getTransaction("UpdateItemsOrder"));
        ts = local_ts.get();
    }
    
    START_WORK
    {
        vector<Item*>::const_iterator iter;
        for (iter = programme->_items.begin(); iter != programme->_items.end(); iter++) {
            Item *item = *iter;

            if (item->_orderIndex == 0)
                PA_LOGTHROW(DBException, ("Item has invalid order index 0"));

            // only items that are persistent and have a new order index are updated
            if (!item->isPersistent() || item->_orderIndex == item->_prevOrderIndex)
                continue;

            // this will allow the Item to update it's _prevOrderIndex
            ts->registerCommitListener(item->getDatabaseID(), item);

            ts->prepared(UPDATE_ITEM_ORDER_INDEX_STMT)
                (item->_orderIndex)
                (item->getDatabaseID()).exec();
        }

        if (!transaction)
            ts->commit();
    }
    END_WORK("UpdateItemsOrder")
}

void Database::loadTakesInItem(Item *item, Transaction *transaction)
{
    Transaction *ts = transaction;
    auto_ptr<Transaction> local_ts;
    if (!ts) {
        local_ts = auto_ptr<Transaction>(getTransaction("LoadTakesInItem"));
        ts = local_ts.get();
    }

    // remove existing in-memory takes first
    item->removeAllTakes();

    START_WORK
    {
        result res = ts->prepared(LOAD_TAKES_IN_ITEM_STMT)(item->getDatabaseID()).exec();

        result::size_type i;
        for (i = 0; i < res.size(); i++) {
            Take *take = item->createTake();

            take->wasLoaded(readId(res[i][0]));
            take->number = readLong(res[i][1], 0);
            take->comment = readString(res[i][2]);
            take->result = readEnum(res[i][3]);
            take->editRate = readRational(res[i][4], res[i][5]);
            take->length = readInt64(res[i][6], 0);
            take->startPosition = readInt64(res[i][7], 0);
            take->startDate = readDate(res[i][8]);
            take->recordingLocation = readId(res[i][9]);
        }
    }
    END_WORK("LoadTakesInItem")
}

void Database::saveItem(Item *item, Transaction *transaction)
{
    Transaction *ts = transaction;
    auto_ptr<Transaction> local_ts;
    if (!ts) {
        local_ts = auto_ptr<Transaction>(getTransaction("SaveItem"));
        ts = local_ts.get();
    }

    START_WORK
    {
        long next_database_id;

        if (!item->isPersistent()) {
            next_database_id = loadNextId("itm_id_seq", ts);
            
            ts->registerCommitListener(next_database_id, item);
            
            ts->prepared(INSERT_ITEM_STMT)
                (next_database_id)
                (item->description)
                (getScriptReferencesString(item->scriptSectionRefs))
                (item->_orderIndex)
                (item->getProgrammeID()).exec();
        } else {
            next_database_id = item->getDatabaseID();

            ts->prepared(UPDATE_ITEM_STMT)
                (item->description)
                (getScriptReferencesString(item->scriptSectionRefs))
                (item->_orderIndex)
                (item->getProgrammeID())
                (next_database_id).exec();
        }

        if (!transaction)
            ts->commit();
    }
    END_WORK("SaveItem")
}

void Database::deleteItem(Item *item, Transaction *transaction)
{
    Transaction *ts = transaction;
    auto_ptr<Transaction> local_ts;
    if (!ts) {
        local_ts = auto_ptr<Transaction>(getTransaction("DeleteItem"));
        ts = local_ts.get();
    }

    START_WORK
    {
        ts->prepared(DELETE_ITEM_STMT)(item->getDatabaseID()).exec();

        ts->registerCommitListener(0, item);

        // the database will cascade the delete down to the take's material package
        // so we register additional listeners for those objects
        vector<Take*>::const_iterator iter;
        for (iter = item->getTakes().begin(); iter != item->getTakes().end(); iter++)
            ts->registerCommitListener(0, *iter);

        if (!transaction)
            ts->commit();
    }
    END_WORK("DeleteItem")
}

void Database::saveTake(Take *take, Transaction *transaction)
{
    Transaction *ts = transaction;
    auto_ptr<Transaction> local_ts;
    if (!ts) {
        local_ts = auto_ptr<Transaction>(getTransaction("SaveTake"));
        ts = local_ts.get();
    }

    START_WORK
    {
        long next_database_id;

        if (!take->isPersistent()) {
            next_database_id = loadNextId("tke_id_seq", ts);

            ts->registerCommitListener(next_database_id, take);
            
            ts->prepared(INSERT_TAKE_STMT)
                (next_database_id)
                (take->number)
                (take->comment)
                (take->result)
                (take->editRate.numerator)
                (take->editRate.denominator)
                (take->length)
                (take->startPosition)
                (writeDate(take->startDate))
                (take->recordingLocation)
                (take->getItemID()).exec();
        } else {
            next_database_id = take->getDatabaseID();

            ts->prepared(UPDATE_TAKE_STMT)
                (take->number)
                (take->comment)
                (take->result)
                (take->editRate.numerator)
                (take->editRate.denominator)
                (take->length)
                (take->startPosition)
                (writeDate(take->startDate))
                (take->recordingLocation)
                (take->getItemID())
                (next_database_id).exec();
        }

        if (!transaction)
            ts->commit();
    }
    END_WORK("SaveTake")
}

void Database::deleteTake(Take *take, Transaction *transaction)
{
    Transaction *ts = transaction;
    auto_ptr<Transaction> local_ts;
    if (!ts) {
        local_ts = auto_ptr<Transaction>(getTransaction("DeleteTake"));
        ts = local_ts.get();
    }

    START_WORK
    {
        ts->prepared(DELETE_TAKE_STMT)(take->getDatabaseID()).exec();

        ts->registerCommitListener(0, take);

        if (!transaction)
            ts->commit();
    }
    END_WORK("DeleteTake")
}

vector<Transcode*> Database::loadTranscodes(int status, Transaction *transaction)
{
    Transaction *ts = transaction;
    auto_ptr<Transaction> local_ts;
    if (!ts) {
        local_ts = auto_ptr<Transaction>(getTransaction("LoadTranscodes"));
        ts = local_ts.get();
    }

    VectorGuard<Transcode> transcodes;
    
    START_WORK
    {
        result res = ts->prepared(LOAD_TRANSCODES_WITH_STATUS_STMT)(status).exec();
        
        result::size_type i;
        for (i = 0; i < res.size(); i++) {
            Transcode* transcode = new Transcode();
            transcodes.get().push_back(transcode);

            transcode->wasLoaded(readId(res[i][0]));
            transcode->sourceMaterialPackageDbId = readLong(res[i][1], 0);
            transcode->destMaterialPackageDbId = readLong(res[i][2], 0);
            transcode->created = readTimestamp(res[i][3]);
            transcode->targetVideoResolution = readEnum(res[i][4]);
            transcode->status = readEnum(res[i][5]);
        }
    }
    END_WORK("LoadTranscodes")

    return transcodes.release();
}

void Database::saveTranscode(Transcode *transcode, Transaction *transaction)
{
    Transaction *ts = transaction;
    auto_ptr<Transaction> local_ts;
    if (!ts) {
        local_ts = auto_ptr<Transaction>(getTransaction("SaveTranscode"));
        ts = local_ts.get();
    }

    START_WORK
    {
        long next_database_id;

        if (!transcode->isPersistent()) {
            next_database_id = loadNextId("trc_id_seq", ts);
            
            ts->registerCommitListener(next_database_id, transcode);
            
            ts->prepared(INSERT_TRANSCODE_STMT)
                (next_database_id)
                (transcode->sourceMaterialPackageDbId)
                (COND_NUM_PARAM(transcode->destMaterialPackageDbId != 0,
                                transcode->destMaterialPackageDbId))
                (COND_NUM_PARAM(transcode->targetVideoResolution != 0,
                                transcode->targetVideoResolution))
                (transcode->status).exec();
        } else {
            next_database_id = transcode->getDatabaseID();

            ts->prepared(UPDATE_TRANSCODE_STMT)
                (transcode->sourceMaterialPackageDbId)
                (COND_NUM_PARAM(transcode->destMaterialPackageDbId != 0,
                                transcode->destMaterialPackageDbId))
                (COND_NUM_PARAM(transcode->targetVideoResolution != 0,
                                transcode->targetVideoResolution))
                (transcode->status)
                (next_database_id).exec();
        }

        if (!transaction)
            ts->commit();
    }
    END_WORK("SaveTranscode")
}

void Database::deleteTranscode(Transcode *transcode, Transaction *transaction)
{
    Transaction *ts = transaction;
    auto_ptr<Transaction> local_ts;
    if (!ts) {
        local_ts = auto_ptr<Transaction>(getTransaction("DeleteTranscode"));
        ts = local_ts.get();
    }

    START_WORK
    {
        ts->prepared(DELETE_TRANSCODE_STMT)(transcode->getDatabaseID()).exec();

        ts->registerCommitListener(0, transcode);

        if (!transaction)
            ts->commit();
    }
    END_WORK("DeleteTranscode")
}

int Database::resetTranscodeStatus(int from_status, int to_status, Transaction *transaction)
{
    Transaction *ts = transaction;
    auto_ptr<Transaction> local_ts;
    if (!ts) {
        local_ts = auto_ptr<Transaction>(getTransaction("ResetTranscodeStatus"));
        ts = local_ts.get();
    }

    int num_updates = 0;
    
    START_WORK
    {
        result res = ts->prepared(RESET_TRANSCODES_STMT)
            (to_status)
            (from_status).exec();
            
        num_updates += (int)res.affected_rows();

        if (!transaction)
            ts->commit();
    }
    END_WORK("ResetTranscodeStatus")

    return num_updates;
}

int Database::deleteTranscodes(int status, Interval time_before_now, Transaction *transaction)
{
    Transaction *ts = transaction;
    auto_ptr<Transaction> local_ts;
    if (!ts) {
        local_ts = auto_ptr<Transaction>(getTransaction("DeleteTranscodes"));
        ts = local_ts.get();
    }

    int num_deletes = 0;

    START_WORK
    {
        result res = ts->prepared(DELETE_TRANSCODES_STMT)
            (writeInterval(time_before_now))
            (status).exec();

        num_deletes = (int)res.affected_rows();
            
        if (!transaction)
            ts->commit();
    }
    END_WORK("DeleteTranscodes")

    return num_deletes;
}

ProjectName Database::loadOrCreateProjectName(string name, Transaction *transaction)
{
    if (name.empty())
        PA_LOGTHROW(DBException, ("Project name is empty"));

    Transaction *ts = transaction;
    auto_ptr<Transaction> local_ts;
    if (!ts) {
        local_ts = auto_ptr<Transaction>(getTransaction("LoadOrCreateProjectName"));
        ts = local_ts.get();
    }

    ProjectName project_name;
    project_name.name = name;

    // load or create the project name
    START_WORK
    {
        result res = ts->prepared(LOAD_OR_CREATE_PROJECT_NAME_STMT)(name).exec();
        if (res.empty())
            PA_LOGTHROW(DBException, ("Failed to load or create project name"));

        project_name.wasLoaded(readId(res[0][0]));

        if (!transaction)
            ts->commit();
    }
    END_WORK("LoadOrCreateProjectName")

    return project_name;
}

vector<ProjectName> Database::loadProjectNames(Transaction *transaction)
{
    Transaction *ts = transaction;
    auto_ptr<Transaction> local_ts;
    if (!ts) {
        local_ts = auto_ptr<Transaction>(getTransaction("LoadProjectNames"));
        ts = local_ts.get();
    }

    vector<ProjectName> all_project_names;

    START_WORK
    {
        result res = ts->prepared(LOAD_ALL_PROJECT_NAMES_STMT).exec();

        result::size_type i;
        for (i = 0; i < res.size(); i++) {
            ProjectName project_name;

            project_name.wasLoaded(readId(res[i][0]));
            project_name.name = readString(res[i][1]);

            all_project_names.push_back(project_name);
        }
    }
    END_WORK("LoadProjectNames")

    return all_project_names;
}

void Database::deleteProjectName(ProjectName *project_name, Transaction *transaction)
{
    if (!project_name->isPersistent())
        // project name is not persisted in the database
        return;

    Transaction *ts = transaction;
    auto_ptr<Transaction> local_ts;
    if (!ts) {
        local_ts = auto_ptr<Transaction>(getTransaction("DeleteProjectName"));
        ts = local_ts.get();
    }

    START_WORK
    {
        ts->prepared(DELETE_PROJECT_NAME_STMT)(project_name->getDatabaseID()).exec();

        ts->registerCommitListener(0, project_name);

        if (!transaction)
            ts->commit();
    }
    END_WORK("DeleteProjectName")
}

SourcePackage* Database::loadSourcePackage(string name, Transaction* transaction)
{
    Transaction *ts = transaction;
    auto_ptr<Transaction> local_ts;
    if (!ts) {
        local_ts = auto_ptr<Transaction>(getTransaction("LoadSourcePackage"));
        ts = local_ts.get();
    }

    SourcePackage* source_package = 0;

    START_WORK
    {
        result res = ts->prepared(LOAD_SOURCE_PACKAGE_STMT)(name).exec();
        if (res.empty())
            return 0; // no exception if not found

        Package *package;
        loadPackage(ts, res[0], &package);
        source_package = dynamic_cast<SourcePackage*>(package);
        if (!source_package)
            PA_LOGTHROW(DBException, ("Package with name '%s' is not a source package", name.c_str()));
    }
    END_WORK("LoadSourcePackage")

    return source_package;
}

void Database::lockSourcePackages(Transaction *transaction)
{
    START_WORK
    {
        // lock the source package access
        transaction->prepared(LOCK_SOURCE_PACKAGES_STMT).exec();
    }
    END_WORK("LockSourcePackages")
}

Package* Database::loadPackage(long database_id, Transaction *transaction)
{
    Transaction *ts = transaction;
    auto_ptr<Transaction> local_ts;
    if (!ts) {
        local_ts = auto_ptr<Transaction>(getTransaction("LoadPackage"));
        ts = local_ts.get();
    }

    Package *package = 0;

    START_WORK
    {
        result res = ts->prepared(LOAD_PACKAGE_STMT)(database_id).exec();
        if (res.empty())
            PA_LOGTHROW(DBException, ("Package %ld does not exist in database", database_id));

        loadPackage(ts, res[0], &package);
    }
    END_WORK("LoadPackage")

    return package;
}

Package* Database::loadPackage(UMID package_uid, bool assume_exists, Transaction *transaction)
{
    Transaction *ts = transaction;
    auto_ptr<Transaction> local_ts;
    if (!ts) {
        local_ts = auto_ptr<Transaction>(getTransaction("LoadPackage(umid)"));
        ts = local_ts.get();
    }

    Package *package = 0;

    START_WORK
    {
        result res = ts->prepared(LOAD_PACKAGE_WITH_UMID_STMT)(writeUMID(package_uid)).exec();
        if (res.empty()) {
            if (assume_exists)
                PA_LOGTHROW(DBException, ("Package '%s' does not exist in database", getUMIDString(package_uid).c_str()))
            else
                return 0;
        }

        loadPackage(ts, res[0], &package);
    }
    END_WORK("LoadPackage(umid)")

    return package;
}

void Database::loadPackage(Transaction *transaction, const result::tuple &tup, Package **package)
{
    auto_ptr<Package> new_package;

    MaterialPackage *material_package = 0;
    SourcePackage *source_package = 0;
    long essence_desc_database_id = readId(tup[6]);
    if (essence_desc_database_id < 0) {
        // it is a material package
        material_package = new MaterialPackage();
        new_package = auto_ptr<Package>(material_package);
    } else {
        // it is a source package
        source_package = new SourcePackage();
        new_package = auto_ptr<Package>(source_package);
    }

    new_package->wasLoaded(readId(tup[0]));
    new_package->uid = readUMID(tup[1]);
    new_package->name = readString(tup[2]);
    new_package->creationDate = readTimestamp(tup[3]);
    new_package->projectName.name = readString(tup[5]);
    if (!new_package->projectName.name.empty())
        new_package->projectName.wasLoaded(readId(tup[4]));

    if (source_package) {
        source_package->sourceConfigName = readString(tup[7]);

        result res = transaction->prepared(LOAD_ESSENCE_DESCRIPTOR_STMT)(essence_desc_database_id).exec();
        if (res.empty())
            PA_LOGTHROW(DBException, ("Essence descriptor %ld does not exist in database", essence_desc_database_id));

        switch (readEnum(res[0][1])) {
            case FILE_ESSENCE_DESC_TYPE:
            {
                FileEssenceDescriptor *file_ess_descriptor = new FileEssenceDescriptor();
                source_package->descriptor = file_ess_descriptor;
                file_ess_descriptor->fileLocation = readString(res[0][2]);
                file_ess_descriptor->fileFormat = readEnum(res[0][3]);
                file_ess_descriptor->videoResolutionID = readEnum(res[0][4]);
                if (file_ess_descriptor->videoResolutionID == 0)
                    file_ess_descriptor->imageAspectRatio = g_nullRational;
                else
                    file_ess_descriptor->imageAspectRatio = readRational(res[0][5], res[0][6]);
                file_ess_descriptor->audioQuantizationBits = readInt(res[0][7], 0);
                break;
            }
    
            case TAPE_ESSENCE_DESC_TYPE:
            {
                TapeEssenceDescriptor *tape_ess_descriptor = new TapeEssenceDescriptor();
                source_package->descriptor = tape_ess_descriptor;
                tape_ess_descriptor->spoolNumber = readString(res[0][8]);
                break;
            }
    
            case LIVE_ESSENCE_DESC_TYPE:
            {
                LiveEssenceDescriptor *live_ess_descriptor = new LiveEssenceDescriptor();
                source_package->descriptor = live_ess_descriptor;
                live_ess_descriptor->recordingLocation = readId(res[0][9]);
                break;
            }
    
            default:
                PA_LOGTHROW(DBException, ("Unknown essence descriptor type"));
        }

        source_package->descriptor->wasLoaded(readId(res[0][0]));
    }


    // load the tracks

    result res1 = transaction->prepared(LOAD_TRACKS_STMT)(new_package->getDatabaseID()).exec();
    
    result::size_type i1;
    for (i1 = 0; i1 < res1.size(); i1++) {
        Track *track = new Track();
        new_package->tracks.push_back(track);
        
        track->wasLoaded(readId(res1[i1][0]));
        track->id = readInt(res1[i1][1], 0);
        track->number = readInt(res1[i1][2], 0);
        track->name = readString(res1[i1][3]);
        track->dataDef = readEnum(res1[i1][4]);
        track->editRate = readRational(res1[i1][5], res1[i1][6]);

        
        // load source clip

        result res2 = transaction->prepared(LOAD_SOURCE_CLIP_STMT)(track->getDatabaseID()).exec();
        if (res2.empty())
            PA_LOGTHROW(DBException, ("Track (db id %d) is missing a SourceClip", track->getDatabaseID()));

        track->sourceClip = new SourceClip();
        track->sourceClip->wasLoaded(readId(res2[0][0]));
        track->sourceClip->sourcePackageUID = readUMID(res2[0][1]);
        track->sourceClip->sourceTrackID = readInt(res2[0][2], 0);
        track->sourceClip->length = readInt64(res2[0][3], 0);
        track->sourceClip->position = readInt64(res2[0][4], 0);
    }

    
    // load the user comments
    
    res1 = transaction->prepared(LOAD_PACKAGE_USER_COMMENTS_STMT)(new_package->getDatabaseID()).exec();

    for (i1 = 0; i1 < res1.size(); i1++) {
        UserComment user_comment;
        user_comment.wasLoaded(readId(res1[i1][0]));
        user_comment.name = readString(res1[i1][1]);
        user_comment.value = readString(res1[i1][2]);
        user_comment.position = readInt64(res1[i1][3], STATIC_COMMENT_POSITION);
        user_comment.colour = readEnum(res1[i1][4]);
        new_package->_userComments.push_back(user_comment);
    }

    *package = new_package.release();
}

void Database::savePackage(Package *package, Transaction *transaction)
{
    if (!package)
        PA_LOGTHROW(DBException, ("Can't save null Package"));
    if (!package->projectName.isPersistent() && !package->projectName.name.empty())
        PA_LOGTHROW(DBException, ("Project name referenced by package is not persistent"));
    
    Transaction *ts = transaction;
    auto_ptr<Transaction> local_ts;
    if (!ts) {
        local_ts = auto_ptr<Transaction>(getTransaction("SavePackage"));
        ts = local_ts.get();
    }

    START_WORK
    {
        long next_package_database_id;
        long next_descriptor_database_id = 0;


        // save the source package essence descriptor

        if (package->getType() == SOURCE_PACKAGE) {
            SourcePackage *source_package = dynamic_cast<SourcePackage*>(package);

            if (!source_package->descriptor->isPersistent()) {
                next_descriptor_database_id = loadNextId("eds_id_seq", ts);
                
                ts->registerCommitListener(next_descriptor_database_id, package);
                
                switch (source_package->descriptor->getType()) {
                    case FILE_ESSENCE_DESC_TYPE:
                    {
                        FileEssenceDescriptor *file_ess_descriptor =
                            dynamic_cast<FileEssenceDescriptor*>(source_package->descriptor);
                            
                        ts->prepared(INSERT_ESSENCE_DESCRIPTOR_STMT)
                            (next_descriptor_database_id)                    
                            (source_package->descriptor->getType())
                            (file_ess_descriptor->fileLocation)
                            (file_ess_descriptor->fileFormat)
                            (COND_NUM_PARAM(file_ess_descriptor->videoResolutionID != 0,
                                            file_ess_descriptor->videoResolutionID))
                            (COND_NUM_PARAM(file_ess_descriptor->videoResolutionID != 0,
                                            file_ess_descriptor->imageAspectRatio.numerator))
                            (COND_NUM_PARAM(file_ess_descriptor->videoResolutionID != 0,
                                            file_ess_descriptor->imageAspectRatio.denominator))
                            (COND_NUM_PARAM(file_ess_descriptor->audioQuantizationBits != 0,
                                            file_ess_descriptor->audioQuantizationBits))
                            ()
                            ().exec();
                        break;
                    }
    
                    case TAPE_ESSENCE_DESC_TYPE:
                    {
                        TapeEssenceDescriptor *tape_ess_descriptor =
                            dynamic_cast<TapeEssenceDescriptor*>(source_package->descriptor);
                            
                        ts->prepared(INSERT_ESSENCE_DESCRIPTOR_STMT)
                            (next_descriptor_database_id)                    
                            (source_package->descriptor->getType())
                            ()
                            ()
                            ()
                            ()
                            ()
                            ()
                            (tape_ess_descriptor->spoolNumber)
                            ().exec();
                        break;
                    }
    
                    case LIVE_ESSENCE_DESC_TYPE:
                    {
                        LiveEssenceDescriptor *live_ess_descriptor =
                            dynamic_cast<LiveEssenceDescriptor*>(source_package->descriptor);
    
                        ts->prepared(INSERT_ESSENCE_DESCRIPTOR_STMT)
                            (next_descriptor_database_id)                    
                            (source_package->descriptor->getType())
                            ()
                            ()
                            ()
                            ()
                            ()
                            ()
                            ()
                            (live_ess_descriptor->recordingLocation).exec();
                        break;
                    }
    
                    default:
                        PA_LOGTHROW(DBException, ("Unknown essence descriptor type"));
                }
            } else {
                next_descriptor_database_id = source_package->descriptor->getDatabaseID();

                switch (source_package->descriptor->getType()) {
                    case FILE_ESSENCE_DESC_TYPE:
                    {
                        FileEssenceDescriptor *file_ess_descriptor =
                            dynamic_cast<FileEssenceDescriptor*>(source_package->descriptor);
                            
                        ts->prepared(UPDATE_ESSENCE_DESCRIPTOR_STMT)
                            (source_package->descriptor->getType())
                            (file_ess_descriptor->fileLocation)
                            (file_ess_descriptor->fileFormat)
                            (COND_NUM_PARAM(file_ess_descriptor->videoResolutionID != 0,
                                            file_ess_descriptor->videoResolutionID))
                            (COND_NUM_PARAM(file_ess_descriptor->videoResolutionID != 0,
                                            file_ess_descriptor->imageAspectRatio.numerator))
                            (COND_NUM_PARAM(file_ess_descriptor->videoResolutionID != 0,
                                            file_ess_descriptor->imageAspectRatio.denominator))
                            (COND_NUM_PARAM(file_ess_descriptor->audioQuantizationBits != 0,
                                            file_ess_descriptor->audioQuantizationBits))
                            ()
                            ()
                            (next_descriptor_database_id).exec();
                        break;
                    }
    
                    case TAPE_ESSENCE_DESC_TYPE:
                    {
                        TapeEssenceDescriptor *tape_ess_descriptor =
                            dynamic_cast<TapeEssenceDescriptor*>(source_package->descriptor);
                            
                        ts->prepared(UPDATE_ESSENCE_DESCRIPTOR_STMT)
                            (source_package->descriptor->getType())
                            ()
                            ()
                            ()
                            ()
                            ()
                            ()
                            (tape_ess_descriptor->spoolNumber)
                            ()
                            (next_descriptor_database_id).exec();
                        break;
                    }
    
                    case LIVE_ESSENCE_DESC_TYPE:
                    {
                        LiveEssenceDescriptor *live_ess_descriptor =
                            dynamic_cast<LiveEssenceDescriptor*>(source_package->descriptor);
    
                        ts->prepared(UPDATE_ESSENCE_DESCRIPTOR_STMT)
                            (source_package->descriptor->getType())
                            ()
                            ()
                            ()
                            ()
                            ()
                            ()
                            ()
                            (live_ess_descriptor->recordingLocation)
                            (next_descriptor_database_id).exec();
                        break;
                    }
    
                    default:
                        PA_LOGTHROW(DBException, ("Unknown essence descriptor type"));
                }
            }
        }

        // save the package

        if (!package->isPersistent()) {
            next_package_database_id = loadNextId("pkg_id_seq", ts);

            ts->registerCommitListener(next_package_database_id, package);

            ts->prepared(INSERT_PACKAGE_STMT)
                (next_package_database_id)
                (writeUMID(package->uid))
                (package->name)
                (writeTimestamp(package->creationDate))
                (COND_NUM_PARAM(package->projectName.isPersistent(), package->projectName.getDatabaseID()))
                (COND_NUM_PARAM(package->getType() == SOURCE_PACKAGE, next_descriptor_database_id))
                (COND_STR_PARAM(package->getType() == SOURCE_PACKAGE,
                                (dynamic_cast<SourcePackage*>(package))->sourceConfigName)).exec();
        } else {
            next_package_database_id = package->getDatabaseID();
            
            ts->prepared(UPDATE_PACKAGE_STMT)
                (writeUMID(package->uid))
                (package->name)
                (writeTimestamp(package->creationDate))
                (COND_NUM_PARAM(package->projectName.isPersistent(), package->projectName.getDatabaseID()))
                (COND_NUM_PARAM(package->getType() == SOURCE_PACKAGE, next_descriptor_database_id))
                (COND_STR_PARAM(package->getType() == SOURCE_PACKAGE,
                                (dynamic_cast<SourcePackage*>(package))->sourceConfigName))
                (next_package_database_id).exec();
        }

        
        // save the tracks

        vector<Track*>::const_iterator iter;
        for (iter = package->tracks.begin(); iter != package->tracks.end(); iter++) {
            Track *track = *iter;

            long next_track_database_id;

            if (!track->isPersistent()) {
                next_track_database_id = loadNextId("trk_id_seq", ts);

                ts->registerCommitListener(next_track_database_id, track);
                
                ts->prepared(INSERT_TRACK_STMT)
                    (next_track_database_id)
                    (track->id)
                    (track->number)
                    (COND_STR_PARAM(!track->name.empty(), track->name))
                    (track->dataDef)
                    (track->editRate.numerator)
                    (track->editRate.denominator)
                    (next_package_database_id).exec();
            } else {
                next_track_database_id = track->getDatabaseID();

                ts->prepared(UPDATE_TRACK_STMT)
                    (track->id)
                    (track->number)
                    (COND_STR_PARAM(!track->name.empty(), track->name))
                    (track->dataDef)
                    (track->editRate.numerator)
                    (track->editRate.denominator)
                    (next_package_database_id)
                    (next_track_database_id).exec();
            }


            // save the source clip

            long next_source_clip_database_id;

            SourceClip *source_clip = track->sourceClip;
            if (!source_clip)
                PA_LOGTHROW(DBException, ("Cannot save track that is missing a source clip"));

            if (!source_clip->isPersistent()) {
                next_source_clip_database_id = loadNextId("scp_id_seq", ts);

                ts->registerCommitListener(next_source_clip_database_id, source_clip);
                
                ts->prepared(INSERT_SOURCE_CLIP_STMT)
                    (next_source_clip_database_id)
                    (writeUMID(source_clip->sourcePackageUID))
                    (source_clip->sourceTrackID)
                    (source_clip->length)
                    (source_clip->position)
                    (next_track_database_id).exec();
            } else {
                next_source_clip_database_id = source_clip->getDatabaseID();

                ts->prepared(UPDATE_SOURCE_CLIP_STMT)
                    (writeUMID(source_clip->sourcePackageUID))
                    (source_clip->sourceTrackID)
                    (source_clip->length)
                    (source_clip->position)
                    (next_track_database_id)
                    (next_source_clip_database_id).exec();
            }
        }


        // save the user comments

        vector<UserComment>::iterator uct_iter;
        for (uct_iter = package->_userComments.begin(); uct_iter != package->_userComments.end(); uct_iter++) {
            UserComment &user_comment = *uct_iter;  // using ref so that object in vector is updated

            long next_user_comment_database_id;

            if (!user_comment.isPersistent()) {
                next_user_comment_database_id = loadNextId("uct_id_seq", ts);

                ts->registerCommitListener(next_user_comment_database_id, &user_comment);
                
                ts->prepared(INSERT_PACKAGE_USER_COMMENT_STMT)
                    (next_user_comment_database_id)
                    (next_package_database_id)
                    (user_comment.name)
                    (user_comment.value)
                    (COND_NUM_PARAM(user_comment.position >= 0, user_comment.position))
                    (COND_NUM_PARAM(user_comment.colour > 0, user_comment.colour)).exec();
            } else {
                next_user_comment_database_id = user_comment.getDatabaseID();

                ts->prepared(UPDATE_PACKAGE_USER_COMMENT_STMT)
                    (next_package_database_id)
                    (user_comment.name)
                    (user_comment.value)
                    (COND_NUM_PARAM(user_comment.position >= 0, user_comment.position))
                    (COND_NUM_PARAM(user_comment.colour > 0, user_comment.colour))
                    (next_user_comment_database_id).exec();
            }
        }

        if (!transaction)
            ts->commit();
    }
    END_WORK("SavePackage")
}

// delete multiple packages from ids in supplied array
void Database::deletePackageChain(Package *top_package, Transaction *transaction)
{
    Transaction *ts = transaction;
    auto_ptr<Transaction> local_ts;
    if (!ts) {
        local_ts = auto_ptr<Transaction>(getTransaction("DeletePackageChain"));
        ts = local_ts.get();
    }

    START_WORK
    {
        if (!packageRefsExist(top_package, ts)) {
            // safe to delete

            deletePackage(top_package, ts);

            PackageSet packages;
            loadPackageChain(top_package, &packages);

            prodauto::PackageSet::iterator iter;
            for (iter = packages.begin(); iter != packages.end(); iter++)
                deletePackageChain(*iter, ts);
            
            if (!transaction)
                ts->commit();
        }
    }
    END_WORK("DeletePackageChain")
}

// are there any references from source package to this package?
bool Database::packageRefsExist(Package *package, Transaction *transaction)
{
    Transaction *ts = transaction;
    auto_ptr<Transaction> local_ts;
    if (!ts) {
        local_ts = auto_ptr<Transaction>(getTransaction("PackageRefsExist"));
        ts = local_ts.get();
    }

    START_WORK
    {
        result res = ts->prepared(LOAD_SOURCE_UID_STMT)(writeUMID(package->uid)).exec();

        return !res.empty();
    }
    END_WORK("PackageRefsExist")
}

void Database::deletePackage(Package *package, Transaction *transaction)
{
    Transaction *ts = transaction;
    auto_ptr<Transaction> local_ts;
    if (!ts) {
        local_ts = auto_ptr<Transaction>(getTransaction("DeletePackage"));
        ts = local_ts.get();
    }

    START_WORK
    {
        ts->prepared(DELETE_PACKAGE_STMT)(package->getDatabaseID()).exec();
        
        ts->registerCommitListener(0, package);

        // the database will cascade the delete down to the source clips
        // so we register additional listeners for those objects
        vector<Track*>::const_iterator iter1;
        for (iter1 = package->tracks.begin(); iter1 != package->tracks.end(); iter1++) {
            Track *track = *iter1;
            ts->registerCommitListener(0, track);

            if (track->sourceClip)
                ts->registerCommitListener(0, track->sourceClip);
        }

        // the database delete will cascade to the tagged value so we register
        // additional listeners for those objects
        vector<UserComment>::iterator iter2;
        for (iter2 = package->_userComments.begin(); iter2 != package->_userComments.end(); iter2++) {
            UserComment &user_comment = *iter2;  // using ref so that object in vector is updated
            ts->registerCommitListener(0, &user_comment);
        }

        if (!transaction)
            ts->commit();
    }
    END_WORK("DeletePackage")
}

int Database::loadSourceReference(UMID source_package_uid, uint32_t source_track_id,
                                  Package **referenced_package, Track** referenced_track, Transaction *transaction)
{
    Transaction *ts = transaction;
    auto_ptr<Transaction> local_ts;
    if (!ts) {
        local_ts = auto_ptr<Transaction>(getTransaction("LoadSourceReference"));
        ts = local_ts.get();
    }

    START_WORK
    {
        // get the referenced Package database id

        result res = ts->prepared(LOAD_REFERENCED_PACKAGE_STMT)(writeUMID(source_package_uid)).exec();
        if (res.empty())
            return -1;

        // load the package and get the track

        auto_ptr<Package> package(loadPackage(readId(res[0][0])));
        Track *track = package->getTrack(source_track_id);
        if (!track)
            return -2;

        *referenced_package = package.release();
        *referenced_track = track;
        
        return 1;
    }
    END_WORK("LoadSourceReference")
}

void Database::loadPackageChain(Package *top_package, PackageSet *packages, Transaction *transaction)
{
    Transaction *ts = transaction;
    auto_ptr<Transaction> local_ts;
    if (!ts) {
        local_ts = auto_ptr<Transaction>(getTransaction("LoadPackageChain1"));
        ts = local_ts.get();
    }

    vector<Track*>::const_iterator iter;
    for (iter = top_package->tracks.begin(); iter != top_package->tracks.end(); iter++) {
        Track *track = *iter;

        if (track->sourceClip->sourcePackageUID != g_nullUMID) {
            Package *referenced_package;
            Track *referenced_track;

            // check that we don't already have the package before
            // loading it from the database
            bool have_package = false;
            PackageSet::const_iterator iter2;
            for (iter2 = packages->begin(); iter2 != packages->end(); iter2++) {
                Package *package = *iter2;

                if (track->sourceClip->sourcePackageUID == package->uid) {
                    have_package = true;
                    break;
                }
            }
            if (have_package)
                // have package so skip to next track
                continue;

            // load referenced package
            if (loadSourceReference(track->sourceClip->sourcePackageUID, track->sourceClip->sourceTrackID,
                                    &referenced_package, &referenced_track, ts) == 1)
            {
                pair<PackageSet::iterator, bool> result = packages->insert(referenced_package);
                if (!result.second) {
                    delete referenced_package;
                    referenced_package = *result.first;
                }

                // load recursively (depth first)
                loadPackageChain(referenced_package, packages, ts);
            }
        }
    }
}

void Database::loadPackageChain(long database_id, Package** top_package, PackageSet* packages,
                                Transaction *transaction)
{
    Transaction *ts = transaction;
    auto_ptr<Transaction> local_ts;
    if (!ts) {
        local_ts = auto_ptr<Transaction>(getTransaction("LoadPackageChain2"));
        ts = local_ts.get();
    }

    // load the top first package
    *top_package = loadPackage(database_id, ts);
    pair<PackageSet::iterator, bool> result = packages->insert(*top_package);
    if (!result.second) {
        delete *top_package;
        *top_package = *result.first;
    }

    // load referenced packages recursively
    loadPackageChain(*top_package, packages, ts);
}

void Database::loadMaterial(Timestamp &after, Timestamp &before, MaterialPackageSet *top_packages,
                            PackageSet* packages, Transaction *transaction)
{
    Transaction *ts = transaction;
    auto_ptr<Transaction> local_ts;
    if (!ts) {
        local_ts = auto_ptr<Transaction>(getTransaction("LoadMaterial1"));
        ts = local_ts.get();
    }

    START_WORK
    {
        result res = ts->prepared(LOAD_MATERIAL_1_STMT)
            (writeTimestamp(after))
            (writeTimestamp(before)).exec();

        result::size_type i;
        for (i = 0; i < res.size(); i++) {
            Package *top_package;
            
            loadPackageChain(readId(res[i][0]), &top_package, packages);
            if (top_package->getType() != MATERIAL_PACKAGE) {
                // shouldn't ever be here
                delete top_package;
                PA_LOGTHROW(DBException, ("Database package is not a material package"));
            }
            top_packages->insert(dynamic_cast<MaterialPackage*>(top_package));
        }
    }
    END_WORK("LoadMaterial1")
}

void Database::loadMaterial(string uc_name, string uc_value, MaterialPackageSet* top_packages, PackageSet* packages,
                            Transaction *transaction)
{
    Transaction *ts = transaction;
    auto_ptr<Transaction> local_ts;
    if (!ts) {
        local_ts = auto_ptr<Transaction>(getTransaction("LoadMaterial2"));
        ts = local_ts.get();
    }

    START_WORK
    {
        result res = ts->prepared(LOAD_MATERIAL_2_STMT)
            (uc_name)
            (uc_value).exec();

        result::size_type i;
        for (i = 0; i < res.size(); i++) {
            Package *top_package;
            
            loadPackageChain(readId(res[i][0]), &top_package, packages);
            if (top_package->getType() != MATERIAL_PACKAGE) {
                // shouldn't ever be here
                delete top_package;
                PA_LOGTHROW(DBException, ("Database package is not a material package"));
            }
            top_packages->insert(dynamic_cast<MaterialPackage*>(top_package));
        }
    }
    END_WORK("LoadMaterial2")
}

void Database::loadMaterial(const std::vector<long> &packageIDs, MaterialPackageSet *top_packages,
                            PackageSet *packages, Transaction *transaction)
{
    Transaction *ts = transaction;
    auto_ptr<Transaction> local_ts;
    if (!ts) {
        local_ts = auto_ptr<Transaction>(getTransaction("LoadMaterial3"));
        ts = local_ts.get();
    }

    Package *top_package;
    for (std::vector<long>::const_iterator it = packageIDs.begin(); it != packageIDs.end(); ++it) {
        loadPackageChain(*it, &top_package, packages, ts);
        if (top_package->getType() != MATERIAL_PACKAGE) {
            // ID did not refer to a material package
            delete top_package;
            PA_LOGTHROW(DBException, ("Database package is not a material package"));
        }
        top_packages->insert(dynamic_cast<MaterialPackage*>(top_package));
    }
}

void Database::loadResolutionNames(map<int, string> & resolution_names, Transaction *transaction)
{
    Transaction *ts = transaction;
    auto_ptr<Transaction> local_ts;
    if (!ts) {
        local_ts = auto_ptr<Transaction>(getTransaction("LoadResolutionNames"));
        ts = local_ts.get();
    }
    
    resolution_names.clear();

    START_WORK
    {
        result res = ts->prepared(LOAD_ALL_RESOLUTION_NAMES_STMT).exec();

        result::size_type i;
        for (i = 0; i < res.size(); i++)
            resolution_names[readEnum(res[i][0])] = readString(res[i][1]);
    }
    END_WORK("LoadResolutionNames")
}

void Database::loadFileFormatNames(map<int, string> & file_format_names, Transaction *transaction)
{
    Transaction *ts = transaction;
    auto_ptr<Transaction> local_ts;
    if (!ts) {
        local_ts = auto_ptr<Transaction>(getTransaction("LoadFileFormatNames"));
        ts = local_ts.get();
    }
    
    file_format_names.clear();

    START_WORK
    {
        result res = ts->prepared(LOAD_ALL_FILE_FORMAT_NAMES_STMT).exec();

        result::size_type i;
        for (i = 0; i < res.size(); i++)
            file_format_names[readEnum(res[i][0])] = readString(res[i][1]);
    }
    END_WORK("LoadFileFormatNames")
}

void Database::loadTimecodeNames(map<int, string> & timecode_names, Transaction *transaction)
{
    Transaction *ts = transaction;
    auto_ptr<Transaction> local_ts;
    if (!ts) {
        local_ts = auto_ptr<Transaction>(getTransaction("LoadTimecodeNames"));
        ts = local_ts.get();
    }
    
    timecode_names.clear();

    START_WORK
    {
        result res = ts->prepared(LOAD_ALL_TIMECODE_NAMES_STMT).exec();

        result::size_type i;
        for (i = 0; i < res.size(); i++)
        {
            timecode_names[readEnum(res[i][0])] = readString(res[i][1]);
        }
    }
    END_WORK("LoadTimecodeNames")
}

string Database::loadLocationName(long database_id, Transaction *transaction)
{
    Transaction *ts = transaction;
    auto_ptr<Transaction> local_ts;
    if (!ts) {
        local_ts = auto_ptr<Transaction>(getTransaction("LoadLocationName"));
        ts = local_ts.get();
    }
    
    START_WORK
    {
        result res = ts->prepared(LOAD_LOCATION_NAME_STMT)(database_id).exec();
        if (res.empty())
            PA_LOGTHROW(DBException, ("Package %ld does not exist in database", database_id));

        return readString(res[0][0]);
    }
    END_WORK("LoadLocationName")
}

void Database::returnConnection(Transaction *transaction)
{
    vector<Transaction*>::iterator iter;

    LOCK_SECTION(_connectionMutex);

    // find the transaction
    for (iter = _transactionsInUse.begin(); iter != _transactionsInUse.end(); iter++) {
        if (transaction == *iter)
            break;
    }
    assert(iter != _transactionsInUse.end());

    // return connection to pool
    _connectionPool.push_back(transaction->_conn);
    transaction->_conn = 0;

    _transactionsInUse.erase(iter);
}

connection* Database::openConnection(string hostname, string dbname, string username, string password)
{
    connection *conn = 0;
    
    try
    {
        string conn_string;
        if (!hostname.empty())
            conn_string += " host=" + hostname;
        if (!dbname.empty())
            conn_string += " dbname=" + dbname;
        if (!username.empty())
            conn_string += " user=" + username;
        if (!password.empty())
            conn_string += " password=" + password;
        
        conn = new connection(conn_string);
        
        conn->prepare(LOAD_REC_LOCATIONS_STMT, LOAD_REC_LOCATIONS_SQL);

        conn->prepare(INSERT_REC_LOCATIONS_STMT, INSERT_REC_LOCATIONS_SQL)
            ("integer", prepare::treat_direct)
            ("varchar", prepare::treat_string);
        
        conn->prepare(LOAD_RECORDER_STMT, LOAD_RECORDER_SQL)
            ("varchar", prepare::treat_string);
            
        conn->prepare(LOAD_RECORDER_CONFIG_STMT, LOAD_RECORDER_CONFIG_SQL)
            ("integer", prepare::treat_direct);
            
        conn->prepare(LOAD_RECORDER_PARAMS_STMT, LOAD_RECORDER_PARAMS_SQL)
            ("integer", prepare::treat_direct);
            
        conn->prepare(LOAD_RECORDER_INPUT_CONFIG_STMT, LOAD_RECORDER_INPUT_CONFIG_SQL)
            ("integer", prepare::treat_direct);
            
        conn->prepare(LOAD_RECORDER_INPUT_TRACK_CONFIG_STMT, LOAD_RECORDER_INPUT_TRACK_CONFIG_SQL)
            ("integer", prepare::treat_direct);

        conn->prepare(INSERT_RECORDER_STMT, INSERT_RECORDER_SQL)
            ("integer", prepare::treat_direct)
            ("varchar", prepare::treat_string)
            ("integer", prepare::treat_direct);

        conn->prepare(INSERT_RECORDER_CONFIG_STMT, INSERT_RECORDER_CONFIG_SQL)
            ("integer", prepare::treat_direct)
            ("varchar", prepare::treat_string)
            ("integer", prepare::treat_direct);

        conn->prepare(INSERT_RECORDER_PARAM_STMT, INSERT_RECORDER_PARAM_SQL)
            ("integer", prepare::treat_direct)
            ("varchar", prepare::treat_string)
            ("varchar", prepare::treat_string)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct);

        conn->prepare(INSERT_RECORDER_INPUT_CONFIG_STMT, INSERT_RECORDER_INPUT_CONFIG_SQL)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct)
            ("varchar", prepare::treat_string)
            ("integer", prepare::treat_direct);

        conn->prepare(INSERT_RECORDER_INPUT_TRACK_CONFIG_STMT, INSERT_RECORDER_INPUT_TRACK_CONFIG_SQL)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct);

        conn->prepare(UPDATE_RECORDER_STMT, UPDATE_RECORDER_SQL)
            ("varchar", prepare::treat_string)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct);

        conn->prepare(UPDATE_ACTIVE_RECORDER_CONFIG_STMT, UPDATE_ACTIVE_RECORDER_CONFIG_SQL)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct);

        conn->prepare(UPDATE_RECORDER_CONFIG_STMT, UPDATE_RECORDER_CONFIG_SQL)
            ("varchar", prepare::treat_string)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct);

        conn->prepare(UPDATE_RECORDER_PARAM_STMT, UPDATE_RECORDER_PARAM_SQL)
            ("varchar", prepare::treat_string)
            ("varchar", prepare::treat_string)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct);

        conn->prepare(UPDATE_RECORDER_INPUT_CONFIG_STMT, UPDATE_RECORDER_INPUT_CONFIG_SQL)
            ("integer", prepare::treat_direct)
            ("varchar", prepare::treat_string)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct);

        conn->prepare(UPDATE_RECORDER_INPUT_TRACK_CONFIG_STMT, UPDATE_RECORDER_INPUT_TRACK_CONFIG_SQL)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct);

        conn->prepare(DELETE_RECORDER_STMT, DELETE_RECORDER_SQL)
            ("integer", prepare::treat_direct);

        conn->prepare(LOAD_SOURCE_CONFIG_STMT, LOAD_SOURCE_CONFIG_SQL)
            ("integer", prepare::treat_direct);

        conn->prepare(LOAD_NAMED_SOURCE_CONFIG_STMT, LOAD_NAMED_SOURCE_CONFIG_SQL)
            ("integer", prepare::treat_direct);

        conn->prepare(LOAD_SOURCE_TRACK_CONFIG_STMT, LOAD_SOURCE_TRACK_CONFIG_SQL)
            ("integer", prepare::treat_direct);

        conn->prepare(INSERT_SOURCE_CONFIG_STMT, INSERT_SOURCE_CONFIG_SQL)
            ("integer", prepare::treat_direct)
            ("varchar", prepare::treat_string)
            ("integer", prepare::treat_direct)
            ("varchar", prepare::treat_string)
            ("integer", prepare::treat_direct);

        conn->prepare(INSERT_SOURCE_TRACK_CONFIG_STMT, INSERT_SOURCE_TRACK_CONFIG_SQL)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct)
            ("varchar", prepare::treat_string)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct);

        conn->prepare(UPDATE_SOURCE_CONFIG_STMT, UPDATE_SOURCE_CONFIG_SQL)
            ("varchar", prepare::treat_string)
            ("integer", prepare::treat_direct)
            ("varchar", prepare::treat_string)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct);

        conn->prepare(UPDATE_SOURCE_TRACK_CONFIG_STMT, UPDATE_SOURCE_TRACK_CONFIG_SQL)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct)
            ("varchar", prepare::treat_string)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct);

        conn->prepare(DELETE_SOURCE_CONFIG_STMT, DELETE_SOURCE_CONFIG_SQL)
            ("integer", prepare::treat_direct);

        conn->prepare(LOAD_ALL_ROUTER_CONFIGS_STMT, LOAD_ALL_ROUTER_CONFIGS_SQL);

        conn->prepare(LOAD_ROUTER_CONFIG_STMT, LOAD_ROUTER_CONFIG_SQL)
            ("varchar", prepare::treat_string);

        conn->prepare(LOAD_ROUTER_INPUT_CONFIG_STMT, LOAD_ROUTER_INPUT_CONFIG_SQL)
            ("integer", prepare::treat_direct);

        conn->prepare(LOAD_ROUTER_OUTPUT_CONFIG_STMT, LOAD_ROUTER_OUTPUT_CONFIG_SQL)
            ("integer", prepare::treat_direct);

        conn->prepare(LOAD_ALL_MC_CLIP_DEFS_STMT, LOAD_ALL_MC_CLIP_DEFS_SQL);

        conn->prepare(LOAD_MC_CLIP_DEF_STMT, LOAD_MC_CLIP_DEF_SQL)
            ("varchar", prepare::treat_string);

        conn->prepare(LOAD_MC_TRACK_DEF_STMT, LOAD_MC_TRACK_DEF_SQL)
            ("integer", prepare::treat_direct);

        conn->prepare(LOAD_MC_SELECTOR_DEF_STMT, LOAD_MC_SELECTOR_DEF_SQL)
            ("integer", prepare::treat_direct);

        conn->prepare(INSERT_MC_CLIP_DEF_STMT, INSERT_MC_CLIP_DEF_SQL)
            ("integer", prepare::treat_direct)
            ("varchar", prepare::treat_string);

        conn->prepare(INSERT_MC_TRACK_DEF_STMT, INSERT_MC_TRACK_DEF_SQL)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct);

        conn->prepare(INSERT_MC_SELECTOR_DEF_STMT, INSERT_MC_SELECTOR_DEF_SQL)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct);

        conn->prepare(UPDATE_MC_CLIP_DEF_STMT, UPDATE_MC_CLIP_DEF_SQL)
            ("varchar", prepare::treat_string)
            ("integer", prepare::treat_direct);

        conn->prepare(UPDATE_MC_TRACK_DEF_STMT, UPDATE_MC_TRACK_DEF_SQL)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct);

        conn->prepare(UPDATE_MC_SELECTOR_DEF_STMT, UPDATE_MC_SELECTOR_DEF_SQL)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct);

        conn->prepare(DELETE_MC_CLIP_DEF_STMT, DELETE_MC_CLIP_DEF_SQL)
            ("integer", prepare::treat_direct);

        conn->prepare(LOAD_ALL_MC_CUTS_STMT, LOAD_ALL_MC_CUTS_SQL);

        conn->prepare(LOAD_MC_CUTS_STMT, LOAD_MC_CUTS_SQL)
            ("varchar", prepare::treat_string)
            ("varchar", prepare::treat_string)
            ("integer", prepare::treat_direct);

        conn->prepare(INSERT_MC_CUT_STMT, INSERT_MC_CUT_SQL)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct)
            ("varchar", prepare::treat_string)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct);

        conn->prepare(UPDATE_MC_CUT_STMT, UPDATE_MC_CUT_SQL)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct)
            ("varchar", prepare::treat_string)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct);

        conn->prepare(LOAD_ALL_SERIES_STMT, LOAD_ALL_SERIES_SQL);

        conn->prepare(LOAD_PROGRAMMES_IN_SERIES_STMT, LOAD_PROGRAMMES_IN_SERIES_SQL)
            ("integer", prepare::treat_direct);

        conn->prepare(INSERT_SERIES_STMT, INSERT_SERIES_SQL)
            ("integer", prepare::treat_direct)
            ("varchar", prepare::treat_string);

        conn->prepare(UPDATE_SERIES_STMT, UPDATE_SERIES_SQL)
            ("varchar", prepare::treat_string)
            ("integer", prepare::treat_direct);

        conn->prepare(DELETE_SERIES_STMT, DELETE_SERIES_SQL)
            ("integer", prepare::treat_direct);

        conn->prepare(LOAD_ITEMS_IN_PROGRAMME_STMT, LOAD_ITEMS_IN_PROGRAMME_SQL)
            ("integer", prepare::treat_direct);

        conn->prepare(INSERT_PROGRAMME_STMT, INSERT_PROGRAMME_SQL)
            ("integer", prepare::treat_direct)
            ("varchar", prepare::treat_string)
            ("integer", prepare::treat_direct);

        conn->prepare(UPDATE_PROGRAMME_STMT, UPDATE_PROGRAMME_SQL)
            ("varchar", prepare::treat_string)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct);

        conn->prepare(DELETE_PROGRAMME_STMT, DELETE_PROGRAMME_SQL)
            ("integer", prepare::treat_direct);

        conn->prepare(UPDATE_ITEM_ORDER_INDEX_STMT, UPDATE_ITEM_ORDER_INDEX_SQL)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct);

        conn->prepare(LOAD_TAKES_IN_ITEM_STMT, LOAD_TAKES_IN_ITEM_SQL)
            ("integer", prepare::treat_direct);

        conn->prepare(INSERT_ITEM_STMT, INSERT_ITEM_SQL)
            ("integer", prepare::treat_direct)
            ("varchar", prepare::treat_string)
            ("varchar", prepare::treat_string)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct);

        conn->prepare(UPDATE_ITEM_STMT, UPDATE_ITEM_SQL)
            ("varchar", prepare::treat_string)
            ("varchar", prepare::treat_string)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct);

        conn->prepare(DELETE_ITEM_STMT, DELETE_ITEM_SQL)
            ("integer", prepare::treat_direct);

        conn->prepare(INSERT_TAKE_STMT, INSERT_TAKE_SQL)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct)
            ("varchar", prepare::treat_string)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct)
            ("varchar", prepare::treat_string)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct);

        conn->prepare(UPDATE_TAKE_STMT, UPDATE_TAKE_SQL)
            ("integer", prepare::treat_direct)
            ("varchar", prepare::treat_string)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct)
            ("varchar", prepare::treat_string)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct);

        conn->prepare(DELETE_ITEM_STMT, DELETE_ITEM_SQL)
            ("integer", prepare::treat_direct);

        conn->prepare(LOAD_TRANSCODES_WITH_STATUS_STMT, LOAD_TRANSCODES_WITH_STATUS_SQL)
            ("integer", prepare::treat_direct);

        conn->prepare(INSERT_TRANSCODE_STMT, INSERT_TRANSCODE_SQL)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct);

        conn->prepare(UPDATE_TRANSCODE_STMT, UPDATE_TRANSCODE_SQL)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct);

        conn->prepare(DELETE_TRANSCODE_STMT, DELETE_TRANSCODE_SQL)
            ("integer", prepare::treat_direct);

        conn->prepare(RESET_TRANSCODES_STMT, RESET_TRANSCODES_SQL)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct);

        conn->prepare(DELETE_TRANSCODES_STMT, DELETE_TRANSCODES_SQL)
            ("varchar", prepare::treat_string)
            ("integer", prepare::treat_direct);

        conn->prepare(LOAD_OR_CREATE_PROJECT_NAME_STMT, LOAD_OR_CREATE_PROJECT_NAME_SQL)
            ("varchar", prepare::treat_string);

        conn->prepare(LOAD_ALL_PROJECT_NAMES_STMT, LOAD_ALL_PROJECT_NAMES_SQL);

        conn->prepare(DELETE_PROJECT_NAME_STMT, DELETE_PROJECT_NAME_SQL)
            ("integer", prepare::treat_direct);

        conn->prepare(LOAD_SOURCE_PACKAGE_STMT, LOAD_SOURCE_PACKAGE_SQL)
            ("varchar", prepare::treat_string);

        conn->prepare(LOAD_PACKAGE_STMT, LOAD_PACKAGE_SQL)
            ("integer", prepare::treat_direct);

        conn->prepare(LOAD_PACKAGE_WITH_UMID_STMT, LOAD_PACKAGE_WITH_UMID_SQL)
            ("varchar", prepare::treat_string);

        conn->prepare(LOAD_PACKAGE_USER_COMMENTS_STMT, LOAD_PACKAGE_USER_COMMENTS_SQL)
            ("integer", prepare::treat_direct);

        conn->prepare(LOAD_ESSENCE_DESCRIPTOR_STMT, LOAD_ESSENCE_DESCRIPTOR_SQL)
            ("integer", prepare::treat_direct);

        conn->prepare(LOAD_TRACKS_STMT, LOAD_TRACKS_SQL)
            ("integer", prepare::treat_direct);

        conn->prepare(LOAD_SOURCE_CLIP_STMT, LOAD_SOURCE_CLIP_SQL)
            ("integer", prepare::treat_direct);

        conn->prepare(LOCK_SOURCE_PACKAGES_STMT, LOCK_SOURCE_PACKAGES_SQL);

        conn->prepare(INSERT_PACKAGE_STMT, INSERT_PACKAGE_SQL)
            ("integer", prepare::treat_direct)
            ("varchar", prepare::treat_string)
            ("varchar", prepare::treat_string)
            ("varchar", prepare::treat_string)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct)
            ("varchar", prepare::treat_string);

        conn->prepare(UPDATE_PACKAGE_STMT, UPDATE_PACKAGE_SQL)
            ("varchar", prepare::treat_string)
            ("varchar", prepare::treat_string)
            ("varchar", prepare::treat_string)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct)
            ("varchar", prepare::treat_string)
            ("integer", prepare::treat_direct);

        conn->prepare(INSERT_PACKAGE_USER_COMMENT_STMT, INSERT_PACKAGE_USER_COMMENT_SQL)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct)
            ("varchar", prepare::treat_string)
            ("varchar", prepare::treat_string)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct);

        conn->prepare(UPDATE_PACKAGE_USER_COMMENT_STMT, UPDATE_PACKAGE_USER_COMMENT_SQL)
            ("integer", prepare::treat_direct)
            ("varchar", prepare::treat_string)
            ("varchar", prepare::treat_string)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct);

        conn->prepare(INSERT_ESSENCE_DESCRIPTOR_STMT, INSERT_ESSENCE_DESCRIPTOR_SQL)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct)
            ("varchar", prepare::treat_string)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct)
            ("varchar", prepare::treat_string)
            ("integer", prepare::treat_direct);

        conn->prepare(UPDATE_ESSENCE_DESCRIPTOR_STMT, UPDATE_ESSENCE_DESCRIPTOR_SQL)
            ("integer", prepare::treat_direct)
            ("varchar", prepare::treat_string)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct)
            ("varchar", prepare::treat_string)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct);

        conn->prepare(INSERT_TRACK_STMT, INSERT_TRACK_SQL)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct)
            ("varchar", prepare::treat_string)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct);

        conn->prepare(UPDATE_TRACK_STMT, UPDATE_TRACK_SQL)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct)
            ("varchar", prepare::treat_string)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct);

        conn->prepare(INSERT_SOURCE_CLIP_STMT, INSERT_SOURCE_CLIP_SQL)
            ("integer", prepare::treat_direct)
            ("varchar", prepare::treat_string)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct);

        conn->prepare(UPDATE_SOURCE_CLIP_STMT, UPDATE_SOURCE_CLIP_SQL)
            ("varchar", prepare::treat_string)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct);

        conn->prepare(LOAD_SOURCE_UID_STMT, LOAD_SOURCE_UID_SQL)
            ("varchar", prepare::treat_string);

        conn->prepare(DELETE_PACKAGE_STMT, DELETE_PACKAGE_SQL)
            ("integer", prepare::treat_direct);

        conn->prepare(LOAD_REFERENCED_PACKAGE_STMT, LOAD_REFERENCED_PACKAGE_SQL)
            ("varchar", prepare::treat_string);

        conn->prepare(LOAD_MATERIAL_1_STMT, LOAD_MATERIAL_1_SQL)
            ("varchar", prepare::treat_string)
            ("varchar", prepare::treat_string);

        conn->prepare(LOAD_MATERIAL_2_STMT, LOAD_MATERIAL_2_SQL)
            ("varchar", prepare::treat_string)
            ("varchar", prepare::treat_string);

        conn->prepare(LOAD_VERSION_STMT, LOAD_VERSION_SQL);

        conn->prepare(LOAD_ALL_RESOLUTION_NAMES_STMT, LOAD_ALL_RESOLUTION_NAMES_SQL);

        conn->prepare(LOAD_ALL_FILE_FORMAT_NAMES_STMT, LOAD_ALL_FILE_FORMAT_NAMES_SQL);

        conn->prepare(LOAD_ALL_TIMECODE_NAMES_STMT, LOAD_ALL_TIMECODE_NAMES_SQL);

        conn->prepare(LOAD_LOCATION_NAME_STMT, LOAD_LOCATION_NAME_SQL)
            ("integer", prepare::treat_direct);
            
        return conn;
    }
    catch (...)
    {
        delete conn;
        throw;
    }
}

long Database::loadNextId(string seq_name, Transaction *transaction)
{
    Transaction *ts = transaction;
    auto_ptr<Transaction> local_ts;
    if (!ts) {
        local_ts = auto_ptr<Transaction>(getTransaction("LoadNextId"));
        ts = local_ts.get();
    }
    
    START_WORK
    {
        string sql = "SELECT nextval('" + seq_name + "')";

        result res(ts->exec(sql));
        if (res.empty())
            PA_LOGTHROW(DBException, ("Failed to get next id from database sequence '%s'", seq_name.c_str()));

        return readId(res[0][0]);
    }
    END_WORK("LoadNextId")
}

void Database::checkVersion(Transaction *transaction)
{
    Transaction *ts = transaction;
    auto_ptr<Transaction> local_ts;
    if (!ts) {
        local_ts = auto_ptr<Transaction>(getTransaction("CheckVersion"));
        ts = local_ts.get();
    }
    
    START_WORK
    {
        result res = ts->prepared(LOAD_VERSION_STMT).exec();
        if (res.empty())
            PA_LOGTHROW(DBException, ("Database is missing a version in the Version table"));

        int version = readInt(res[0][0], 0);

        if (version != g_compatibilityVersion)
        {
            PA_LOGTHROW(DBException, ("Database version %d not equal to required version %d",
                                      version, g_compatibilityVersion));
        }
    }
    END_WORK("CheckVersion")
}

long Database::readId(const result::field &field)
{
    long result;

    if (field.is_null())
        return -1;

    field.to(result);
    
    return result;
}

int Database::readInt(const result::field &field, int null_value)
{
    int result;

    if (field.is_null())
        return null_value;

    field.to(result);
    
    return result;
}

int Database::readEnum(const result::field &field)
{
    int result;

    if (field.is_null())
        return 0;

    field.to(result);
    
    return result;
}

long Database::readLong(const result::field &field, long null_value)
{
    long result;

    if (field.is_null())
        return null_value;

    field.to(result);
    
    return result;
}

int64_t Database::readInt64(const result::field &field, int64_t null_value)
{
    const char *str_result = "";

    if (field.is_null())
        return null_value;

    field.to(str_result);
    
#ifdef _MSC_VER
    return _strtoi64(str_result, NULL, 10);
#elif __WORDSIZE == 64
    return strtol(str_result, NULL, 10);
#else
    return strtoll(str_result, NULL, 10);
#endif
}

bool Database::readBool(const result::field &field, bool null_value)
{
    bool value;

    if (field.is_null())
        return null_value;

    field.to(value);
    
    return value;
}

string Database::readString(const result::field &field)
{
    const char *result = "";

    if (field.is_null())
        return "";

    field.to(result);
    
    return result;
}

Date Database::readDate(const result::field &field)
{
    Date result;
    const char *str_result = "";
    int year, month, day;

    if (field.is_null())
        return g_nullDate;

    field.to(str_result);
    if (sscanf(str_result, "%d-%d-%d", &year, &month, &day) != 3)
        PA_LOGTHROW(DBException, ("Failed to parse date string '%s' from database\n", str_result));
    
    result.year = (int16_t)year;
    result.month = (uint8_t)month;
    result.day = (uint8_t)day;

    return result;
}

Timestamp Database::readTimestamp(const result::field &field)
{
    Timestamp result;
    const char *str_result = "";
    int year, month, day, hour, min;
    float sec_float;

    if (field.is_null())
        return g_nullTimestamp;

    field.to(str_result);
    if (sscanf(str_result, "%d-%d-%d %d:%d:%f", &year, &month, &day, &hour, &min, &sec_float) != 6)
        PA_LOGTHROW(DBException, ("Failed to parse timestamp string '%s' from database\n", str_result));
    
    result.year = (int16_t)year;
    result.month = (uint8_t)month;
    result.day = (uint8_t)day;
    result.hour = (uint8_t)hour;
    result.min = (uint8_t)min;
    result.sec = (uint8_t)sec_float;
    result.qmsec = (uint8_t)(((long)(sec_float * 1000.0) - (long)(sec_float) * 1000) / 4.0 + 0.5);

    return result;
}

Rational Database::readRational(const result::field &field1, const result::field &field2)
{
    Rational result;
    
    result.numerator = readInt(field1, 0);
    result.denominator = readInt(field2, 0);
    
    return result;
}

UMID Database::readUMID(const result::field &field)
{
    const char *str = "";

    if (field.is_null())
        return g_nullUMID;

    field.to(str);
    
    return getUMID(str);
}

string Database::writeTimestamp(Timestamp value)
{
    char buf[128];

    float sec_float = (float)value.sec + value.qmsec * 4 / 1000.0;
    if (sprintf(buf, "%d-%d-%d %d:%d:%.6f", value.year, value.month, value.day, value.hour, value.min, sec_float) < 0)
        PA_LOGTHROW(DBException, ("Failed to write timestamp to string for database"));

    return buf;
}

string Database::writeDate(Date value)
{
    char buf[128];

    if (sprintf(buf, "%d-%d-%d", value.year, value.month, value.day) < 0)
        PA_LOGTHROW(DBException, ("Failed to write date to string for database"));

    return buf;
}

string Database::writeInterval(Interval value)
{
    return getIntervalString(value);
}

string Database::writeUMID(UMID umid)
{
    return getUMIDString(umid);
}

