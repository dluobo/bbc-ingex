/*
 * $Id: RecorderDatabase.cpp,v 1.1 2008/07/08 16:23:02 philipn Exp $
 *
 * Provides access to the recorder data in a PostgreSQL database
 *
 * Copyright (C) 2008 BBC Research, Philip de Nier, <philipn@users.sourceforge.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
 
/*
    Provides a single connection to the recorder database. Access from multiple 
    threads is serialised using a mutex lock at the top of all the access 
    methods

    The SQL statements listed at the top part of this file are prepared in 
    prepareStatements(). Methods to save, retrieve or delete information from
    the database use these prepared statements.
    
    Note: the PostgresDatabase::checkVersion() does nothing more than read the
    version table information. TODO: check the returned version and throw an
    exception if the version is incompatible.
*/
 
#include "RecorderDatabase.h"
#include "Logging.h"
#include "RecorderException.h"
#include "Utilities.h"


using namespace std;
using namespace rec;
using namespace pqxx;


RecorderDatabase* RecorderDatabase::_instance = 0;

RecorderDatabase* RecorderDatabase::getInstance()
{
    if (!_instance)
    {
        REC_LOGTHROW(("Database not initialised"));
    }
    return _instance;
}

void RecorderDatabase::close()
{
    SAFE_DELETE(_instance);
}






static const char* g_loadRecorderStmt = "loadrecorder";
static const char* g_loadRecorderSQL = 
" \
    SELECT \
        rec_identifier, \
        rec_name \
    FROM Recorder \
    WHERE \
        rec_name = $1 \
";        
        
static const char* g_saveRecorderStmt = "saverecorder";
static const char* g_saveRecorderSQL = 
" \
    INSERT INTO Recorder \
    ( \
        rec_identifier, \
        rec_name \
    ) \
    VALUES \
        ($1, $2) \
";        
        
static const char* g_loadSourceStmt = "loadsource";
static const char* g_loadSourceSQL = 
" \
    SELECT \
        src_identifier, \
        src_type_id, \
        src_barcode, \
        src_rec_instance \
    FROM Source \
    WHERE \
        src_barcode = $1 \
";        
        
static const char* g_getSourceRecInstanceStmt = "getsrcrecinstance";
static const char* g_getSourceRecInstanceSQL = 
" \
    SELECT get_rec_instance($1) \
";        
        
static const char* g_resetSourceRecInstanceStmt = "resetsrcrecinstance";
static const char* g_resetSourceRecInstanceSQL = 
" \
    UPDATE Source \
        SET src_rec_instance = $1 - 1 \
        WHERE src_rec_instance = $2 AND \
            src_identifier = $3; \
";        
        

static const char* g_loadD3SourcesStmt = "loadd3sources";
static const char* g_loadD3SourcesSQL = 
" \
    SELECT \
        d3s_identifier, \
        d3s_format, \
        d3s_prog_title, \
        d3s_episode_title, \
        d3s_tx_date, \
        d3s_mag_prefix, \
        d3s_prog_no, \
        d3s_prod_code, \
        d3s_spool_status, \
        d3s_stock_date, \
        d3s_spool_descr, \
        d3s_memo, \
        d3s_duration, \
        d3s_spool_no, \
        d3s_acc_no, \
        d3s_cat_detail, \
        d3s_item_no \
    FROM D3Source \
    WHERE \
        d3s_source_id = $1 \
    ORDER BY \
        d3s_item_no ASC \
";       

static const char* g_updateD3SourceStmt = "updated3source";
static const char* g_updateD3SourceSQL = 
" \
    UPDATE D3Source \
    SET \
        d3s_format = $1, \
        d3s_prog_title = $2, \
        d3s_episode_title = $3, \
        d3s_tx_date = $4, \
        d3s_mag_prefix = $5, \
        d3s_prog_no = $6, \
        d3s_prod_code = $7, \
        d3s_spool_status = $8, \
        d3s_stock_date = $9, \
        d3s_spool_descr = $10, \
        d3s_memo = $11, \
        d3s_duration = $12, \
        d3s_spool_no = $13, \
        d3s_acc_no = $14, \
        d3s_cat_detail = $15, \
        d3s_item_no = $16 \
    WHERE \
        d3s_identifier = $17 \
";        

static const char* g_saveDestStmt = "savedest";
static const char* g_saveDestSQL = 
" \
    INSERT INTO Destination \
    ( \
        des_identifier, \
        des_type_id, \
        des_barcode, \
        des_source_id, \
        des_d3source_id, \
        des_ingest_item_no \
    ) \
    VALUES \
        ($1, $2, $3, $4, $5, $6) \
";        
        
static const char* g_saveHDDestStmt = "savehddest";
static const char* g_saveHDDestSQL = 
" \
    INSERT INTO HardDiskDestination \
    ( \
        hdd_identifier, \
        hdd_host, \
        hdd_path, \
        hdd_name, \
        hdd_material_package_uid, \
        hdd_file_package_uid, \
        hdd_tape_package_uid, \
        hdd_browse_path, \
        hdd_browse_name, \
        hdd_pse_report_path, \
        hdd_pse_report_name, \
        hdd_pse_result_id, \
        hdd_size, \
        hdd_duration, \
        hdd_browse_size, \
        hdd_cache_id, \
        hdd_dest_id \
    ) \
    VALUES \
        ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12, $13, $14, $15, $16, $17) \
";        
        
static const char* g_saveDigibetaDestStmt = "savedigibetadest";
static const char* g_saveDigibetaDestSQL = 
" \
    INSERT INTO DigibetaDestination \
    ( \
        dbd_identifier, \
        dbd_dest_id \
    ) \
    VALUES \
        ($1, $2) \
";        
        
static const char* g_loadAllMinimalSessionsStmt = "loadallminsessions";
static const char* g_loadAllMinimalSessionsSQL = 
" \
    SELECT \
        rcs_identifier, \
        rcs_recorder_id, \
        rcs_creation, \
        rcs_updated, \
        rcs_status_id, \
        rcs_abort_initiator_id, \
        rcs_comments, \
        rcs_total_d3_errors \
    FROM RecordingSession \
    WHERE \
        rcs_recorder_id = $1 \
";        
        
static const char* g_loadMinimalSessionsStmt = "loadminsessions";
static const char* g_loadMinimalSessionsSQL = 
" \
    SELECT \
        rcs_identifier, \
        rcs_recorder_id, \
        rcs_creation, \
        rcs_updated, \
        rcs_status_id, \
        rcs_abort_initiator_id, \
        rcs_comments, \
        rcs_total_d3_errors \
    FROM RecordingSession \
    WHERE \
        rcs_recorder_id = $1 AND \
        rcs_status_id = $2 \
";        
        
static const char* g_saveSessionStmt = "savesession";
static const char* g_saveSessionSQL = 
" \
    INSERT INTO RecordingSession \
    ( \
        rcs_identifier, \
        rcs_recorder_id, \
        rcs_creation, \
        rcs_status_id, \
        rcs_abort_initiator_id, \
        rcs_comments, \
        rcs_total_d3_errors \
    ) \
    VALUES \
        ($1, $2, $3, $4, $5, $6, $7) \
";        
        
static const char* g_updateSessionStmt = "updatesession";
static const char* g_updateSessionSQL = 
" \
    UPDATE RecordingSession \
    SET \
        rcs_status_id = $1, \
        rcs_abort_initiator_id = $2, \
        rcs_comments = $3, \
        rcs_total_d3_errors = $4 \
    WHERE \
        rcs_identifier = $5 \
";        
        
static const char* g_deleteSessionStmt = "deletesession";
static const char* g_deleteSessionSQL = 
" \
    DELETE FROM RecordingSession \
    WHERE \
        rcs_identifier = $1 \
";        

static const char* g_sessionInProgressStmt = "sessioninprogress";
static const char* g_sessionInProgressSQL = 
" \
    SELECT \
        rcs_identifier \
    FROM \
        RecordingSession \
        INNER JOIN SessionSource ON (rcs_identifier = ssc_session_id) \
        INNER JOIN Source ON (ssc_source_id = src_identifier) \
    WHERE \
        rcs_status_id = 1 AND \
        src_barcode = $1 \
";        
        
static const char* g_saveSessionSourceStmt = "savesessionsource";
static const char* g_saveSessionSourceSQL = 
" \
    INSERT INTO SessionSource \
    ( \
        ssc_identifier, \
        ssc_session_id, \
        ssc_source_id \
    ) \
    VALUES \
        ($1, $2, $3) \
";        
        
static const char* g_saveSessionDestStmt = "savesessiondest";
static const char* g_saveSessionDestSQL = 
" \
    INSERT INTO SessionDestination \
    ( \
        sdt_identifier, \
        sdt_session_id, \
        sdt_dest_id \
    ) \
    VALUES \
        ($1, $2, $3) \
";        
        
static const char* g_loadCacheStmt = "loadcache";
static const char* g_loadCacheSQL = 
" \
    SELECT \
        rch_identifier, \
        rch_recorder_id, \
        rch_path \
    FROM RecorderCache \
    WHERE \
        rch_recorder_id = $1 AND \
        rch_path = $2 \
";        
        
static const char* g_saveCacheStmt = "savecache";
static const char* g_saveCacheSQL = 
" \
    INSERT INTO RecorderCache \
    ( \
        rch_identifier, \
        rch_recorder_id, \
        rch_path \
    ) \
    VALUES \
        ($1, $2, $3) \
";        


static const char* g_loadCacheItemsStmt = "loadcacheitems";
static const char* g_loadCacheItemsSQL = 
" \
    SELECT \
        hdd_identifier, \
        hdd_name, \
        hdd_browse_name, \
        hdd_pse_report_name, \
        hdd_pse_result_id, \
        hdd_size, \
        hdd_duration, \
        rcs_identifier, \
        rcs_creation, \
        rcs_comments, \
        rcs_status_id, \
        d3s_spool_no, \
        d3s_item_no, \
        d3s_prog_no, \
        d3s_mag_prefix, \
        d3s_prod_code \
    FROM \
        HardDiskDestination \
        INNER JOIN Destination ON (hdd_dest_id = des_identifier) \
        INNER JOIN SessionDestination ON (sdt_dest_id = des_identifier) \
        INNER JOIN RecordingSession ON (sdt_session_id = rcs_identifier) \
        INNER JOIN D3Source ON (des_d3source_id = d3s_identifier) \
    WHERE \
        hdd_cache_id = $1 \
";        
        
static const char* g_loadCacheItemStmt = "loadcacheitem";
static const char* g_loadCacheItemSQL = 
" \
    SELECT \
        hdd_identifier, \
        hdd_name, \
        hdd_browse_name, \
        hdd_pse_report_name, \
        hdd_pse_result_id, \
        hdd_size, \
        hdd_duration, \
        rcs_identifier, \
        rcs_creation, \
        rcs_comments, \
        rcs_status_id, \
        d3s_spool_no, \
        d3s_item_no, \
        d3s_prog_no, \
        d3s_mag_prefix, \
        d3s_prod_code \
    FROM \
        HardDiskDestination \
        INNER JOIN Destination ON (hdd_dest_id = des_identifier) \
        INNER JOIN SessionDestination ON (sdt_dest_id = des_identifier) \
        INNER JOIN RecordingSession ON (sdt_session_id = rcs_identifier) \
        INNER JOIN D3Source ON (des_d3source_id = d3s_identifier) \
    WHERE \
        hdd_cache_id = $1 AND \
        hdd_name = $2 \
    ORDER BY \
        hdd_identifier DESC \
";        
        
static const char* g_loadSessionDestsStmt = "loadsessiondests";
static const char* g_loadSessionDestsSQL = 
" \
    SELECT \
        des_identifier, \
        des_type_id, \
        des_barcode, \
        des_source_id, \
        des_d3source_id, \
        des_ingest_item_no \
    FROM Destination \
        INNER JOIN SessionDestination ON (sdt_dest_id = des_identifier) \
        INNER JOIN RecordingSession ON (sdt_session_id = rcs_identifier) \
    WHERE \
        rcs_identifier = $1 \
";        
        
static const char* g_loadDestinationsStmt = "loaddestinations";
static const char* g_loadDestinationsSQL = 
" \
    SELECT \
        des_identifier, \
        des_type_id, \
        des_barcode, \
        des_source_id, \
        des_d3source_id, \
        des_ingest_item_no \
    FROM Destination \
    WHERE \
        des_barcode = $1 \
";        
        
static const char* g_deleteDestStmt = "deletedest";
static const char* g_deleteDestSQL = 
" \
    DELETE FROM Destination \
    WHERE \
        des_identifier = $1 \
";        
        
static const char* g_deleteSessionDestStmt = "deletesessiondest";
static const char* g_deleteSessionDestSQL = 
" \
    DELETE FROM SessionDestination \
    WHERE \
        sdt_identifier = $1 \
";        
        
static const char* g_updateHDDestStmt = "updatehddest";
static const char* g_updateHDDestSQL = 
" \
    UPDATE HardDiskDestination \
    SET \
        hdd_material_package_uid = $1, \
        hdd_file_package_uid = $2, \
        hdd_tape_package_uid = $3, \
        hdd_size = $4, \
        hdd_duration = $5, \
        hdd_browse_size = $6, \
        hdd_pse_result_id = $7, \
        hdd_cache_id = $8 \
    WHERE \
        hdd_identifier = $9 \
";        
        
static const char* g_removeHDDFromCacheStmt = "removehddfromcache";
static const char* g_removeHDDFromCacheSQL = 
" \
    UPDATE HardDiskDestination \
    SET \
        hdd_cache_id = NULL \
    WHERE \
        hdd_identifier = $1 \
";        
        
static const char* g_loadHDDestStmt = "loadhddest";
static const char* g_loadHDDestSQL = 
" \
    SELECT \
        hdd_identifier, \
        hdd_host, \
        hdd_path, \
        hdd_name, \
        hdd_material_package_uid, \
        hdd_file_package_uid, \
        hdd_tape_package_uid, \
        hdd_browse_path, \
        hdd_browse_name, \
        hdd_pse_report_path, \
        hdd_pse_report_name, \
        hdd_pse_result_id, \
        hdd_size, \
        hdd_duration, \
        hdd_browse_size, \
        hdd_cache_id \
    FROM \
        HardDiskDestination \
        INNER JOIN Destination ON (hdd_dest_id = des_identifier) \
    WHERE \
        des_identifier = $1 \
";        

static const char* g_loadHDDest2Stmt = "loadhddest2";
static const char* g_loadHDDest2SQL = 
" \
    SELECT \
        hdd_identifier, \
        hdd_host, \
        hdd_path, \
        hdd_name, \
        hdd_material_package_uid, \
        hdd_file_package_uid, \
        hdd_tape_package_uid, \
        hdd_browse_path, \
        hdd_browse_name, \
        hdd_pse_report_path, \
        hdd_pse_report_name, \
        hdd_pse_result_id, \
        hdd_size, \
        hdd_duration, \
        hdd_browse_size, \
        hdd_cache_id \
    FROM \
        HardDiskDestination \
    WHERE \
        hdd_identifier = $1 \
";        

static const char* g_loadDigibetaDestStmt = "loaddigibetadest";
static const char* g_loadDigibetaDestSQL = 
" \
    SELECT \
        dbd_identifier \
    FROM \
        DigibetaDestination \
        INNER JOIN Destination ON (dbd_dest_id = des_identifier) \
    WHERE \
        des_identifier = $1 \
";        

static const char* g_loadHDDSourceStmt = "loadhddsource";
static const char* g_loadHDDSourceSQL = 
" \
    SELECT \
        src_identifier, \
        src_type_id, \
        src_barcode, \
        src_rec_instance \
    FROM \
        HardDiskDestination \
        INNER JOIN Destination ON (hdd_dest_id = des_identifier) \
        INNER JOIN Source ON (des_source_id = src_identifier) \
    WHERE \
        hdd_identifier = $1 \
";        
        
static const char* g_loadAllLTOTransferSessionsStmt = "loadallltotransfersessions";
static const char* g_loadAllLTOTransferSessionsSQL = 
" \
    SELECT \
        lts_identifier, \
        lts_recorder_id, \
        lts_creation, \
        lts_updated, \
        lts_status_id, \
        lts_abort_initiator_id, \
        lts_comments, \
        lts_tape_device_vendor, \
        lts_tape_device_model, \
        lts_tape_device_revision \
    FROM LTOTransferSession \
    WHERE \
        lts_recorder_id = $1 \
";        
        
static const char* g_loadLTOTransferSessionsStmt = "loadltotransfersessions";
static const char* g_loadLTOTransferSessionsSQL = 
" \
    SELECT \
        lts_identifier, \
        lts_recorder_id, \
        lts_creation, \
        lts_updated, \
        lts_status_id, \
        lts_abort_initiator_id, \
        lts_comments, \
        lts_tape_device_vendor, \
        lts_tape_device_model, \
        lts_tape_device_revision \
    FROM LTOTransferSession \
    WHERE \
        lts_recorder_id = $1 AND \
        lts_status_id = $2 \
";        
        
static const char* g_saveLTOTransferSessionStmt = "saveltotransfersession";
static const char* g_saveLTOTransferSessionSQL = 
" \
    INSERT INTO LTOTransferSession \
    ( \
        lts_identifier, \
        lts_recorder_id, \
        lts_creation, \
        lts_status_id, \
        lts_abort_initiator_id, \
        lts_comments, \
        lts_tape_device_vendor, \
        lts_tape_device_model, \
        lts_tape_device_revision \
    ) \
    VALUES \
        ($1, $2, $3, $4, $5, $6, $7, $8, $9) \
";        
        
static const char* g_updateLTOTransferSessionStmt = "updateltotransfersession";
static const char* g_updateLTOTransferSessionSQL = 
" \
    UPDATE LTOTransferSession \
    SET \
        lts_status_id = $1, \
        lts_abort_initiator_id = $2, \
        lts_comments = $3 \
    WHERE \
        lts_identifier = $4 \
";        
        
static const char* g_deleteLTOTransferSessionStmt = "deleteltotransfersession";
static const char* g_deleteLTOTransferSessionSQL = 
" \
    DELETE FROM LTOTransferSession \
    WHERE \
        lts_identifier = $1 \
";        

static const char* g_updateCompletedLTOFileStmt = "updatecompletedltofile";
static const char* g_updateCompletedLTOFileSQL = 
" \
    UPDATE LTOFile \
    SET \
        ltf_status_id = $1 \
    WHERE \
        ltf_identifier = $2 \
";        
        
static const char* g_checkLTOUsedStmt = "checkltoused";
static const char* g_checkLTOUsedSQL = 
" \
    SELECT \
        lto_identifier \
    FROM \
        LTO \
        INNER JOIN LTOTransferSession ON (lts_identifier = lto_session_id) \
    WHERE \
        lto_spool_no = $1 AND \
        lts_status_id = 2 \
";        
        
static const char* g_loadSessionLTOStmt = "loadsessionlto";
static const char* g_loadSessionLTOSQL = 
" \
    SELECT \
        lto_identifier, \
        lto_spool_no \
    FROM LTO \
    WHERE \
        lto_session_id = $1 \
";        
        
static const char* g_loadLTOFilesStmt = "loadltofiles";
static const char* g_loadLTOFilesSQL = 
" \
    SELECT \
        ltf_identifier, \
        ltf_status_id, \
        ltf_name, \
        ltf_size, \
        ltf_real_duration, \
        ltf_format, \
        ltf_prog_title, \
        ltf_episode_title, \
        ltf_tx_date, \
        ltf_mag_prefix, \
        ltf_prog_no, \
        ltf_prod_code, \
        ltf_spool_status, \
        ltf_stock_date, \
        ltf_spool_descr, \
        ltf_memo, \
        ltf_duration, \
        ltf_spool_no, \
        ltf_acc_no, \
        ltf_cat_detail, \
        ltf_recording_session_id, \
        ltf_hdd_host, \
        ltf_hdd_path, \
        ltf_hdd_name, \
        ltf_hdd_material_package_uid, \
        ltf_hdd_file_package_uid, \
        ltf_hdd_tape_package_uid, \
        ltf_source_spool_no, \
        ltf_source_item_no, \
        ltf_source_prog_no, \
        ltf_source_mag_prefix, \
        ltf_source_prod_code \
    FROM LTOFile \
    WHERE \
        ltf_lto_id = $1 \
";        
        
static const char* g_saveLTOStmt = "savelto";
static const char* g_saveLTOSQL = 
" \
    INSERT INTO LTO \
    ( \
        lto_identifier, \
        lto_session_id, \
        lto_spool_no \
    ) \
    VALUES \
        ($1, $2, $3) \
";        
        
static const char* g_saveLTOFileStmt = "saveltofile";
static const char* g_saveLTOFileSQL = 
" \
    INSERT INTO LTOFIle \
    ( \
        ltf_identifier, \
        ltf_lto_id, \
        ltf_status_id, \
        ltf_name, \
        ltf_size, \
        ltf_real_duration, \
        ltf_format, \
        ltf_prog_title, \
        ltf_episode_title, \
        ltf_tx_date, \
        ltf_mag_prefix, \
        ltf_prog_no, \
        ltf_prod_code, \
        ltf_spool_status, \
        ltf_stock_date, \
        ltf_spool_descr, \
        ltf_memo, \
        ltf_duration, \
        ltf_spool_no, \
        ltf_acc_no, \
        ltf_cat_detail, \
        ltf_recording_session_id, \
        ltf_hdd_host, \
        ltf_hdd_path, \
        ltf_hdd_name, \
        ltf_hdd_material_package_uid, \
        ltf_hdd_file_package_uid, \
        ltf_hdd_tape_package_uid, \
        ltf_source_spool_no, \
        ltf_source_item_no, \
        ltf_source_prog_no, \
        ltf_source_mag_prefix, \
        ltf_source_prod_code \
    ) \
    VALUES \
        ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12, $13, $14, $15, $16, $17, $18, $19, $20, $21, $22, $23, $24, $25, $26, $27, $28, $29, $30, $31, $32, $33) \
";        
        
static const char* g_deleteLTOStmt = "deletelto";
static const char* g_deleteLTOSQL = 
" \
    DELETE FROM LTO \
    WHERE \
        lto_identifier = $1 \
";        

static const char* g_saveInfaxExportStmt = "saveinfaxexport";
static const char* g_saveInfaxExportSQL = 
" \
    INSERT INTO InfaxExport \
    ( \
        ixe_identifier, \
        ixe_transfer_date, \
        ixe_d3_spool_no, \
        ixe_item_no, \
        ixe_prog_title, \
        ixe_episode_title, \
        ixe_mag_prefix, \
        ixe_prog_no, \
        ixe_prod_code, \
        ixe_mxf_name, \
        ixe_digibeta_spool_no, \
        ixe_browse_name, \
        ixe_lto_spool_no \
    ) \
    VALUES \
        ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12, $13) \
";        
        
static const char* g_checkDigibetaUsedStmt = "checkdigibetaused";
static const char* g_checkDigibetaUsedSQL = 
" \
    SELECT \
        des_identifier \
    FROM \
        Destination \
        INNER JOIN SessionDestination ON (des_identifier = sdt_dest_id) \
        INNER JOIN RecordingSession ON (sdt_session_id = rcs_identifier) \
    WHERE \
        des_barcode = $1 AND \
        rcs_status_id = 2 \
";        
        
static const char* g_checkD3UsedStmt = "checkd3used";
static const char* g_checkD3UsedSQL = 
" \
    SELECT \
        src_identifier \
    FROM \
        Source \
        INNER JOIN SessionSource ON (src_identifier = ssc_source_id) \
        INNER JOIN RecordingSession ON (ssc_session_id = rcs_identifier) \
    WHERE \
        src_barcode = $1 AND \
        rcs_status_id = 2 \
";        
        



void PostgresRecorderDatabase::initialise(string host, string name, string user, string password)
{
    if (_instance)
    {
        REC_LOGTHROW(("Database is already initialised"));
    }
    _instance = new PostgresRecorderDatabase(host, name, user, password);
}

PostgresRecorderDatabase::PostgresRecorderDatabase(string host, string name, string user, string password)
: PostgresDatabase(host, name, user, password)
{
    try
    {
        prepareStatements();
    }
    catch (const exception& ex)
    {
        REC_LOGTHROW(("Failed to open a connection to the recorder database: "
            "host='%s', name='%s', user='%s':\n%s",
            host.c_str(), name.c_str(), user.c_str(),
            ex.what()));
    }
    catch (const RecorderException& ex)
    {
        REC_LOGTHROW(("Failed to open a connection to the recorder database: "
            "host='%s', name='%s', user='%s':\n%s",
            host.c_str(), name.c_str(), user.c_str(),
            ex.getMessage().c_str()));
    }
    catch (...)
    {
        REC_LOGTHROW(("Failed to open a connection to the recorder database: "
            "host='%s', name='%s', user='%s':\nunknown database exception",
            host.c_str(), name.c_str(), user.c_str()));
    }
}

PostgresRecorderDatabase::~PostgresRecorderDatabase()
{
}

void PostgresRecorderDatabase::prepareStatements()
{
    START_WORK
    {
        LOCK_SECTION(_accessMutex);

        _connection->prepare(g_loadRecorderStmt, g_loadRecorderSQL)
            ("varchar", prepare::treat_string);
        
        _connection->prepare(g_saveRecorderStmt, g_saveRecorderSQL)
            ("integer", prepare::treat_direct)
            ("varchar", prepare::treat_string);
        
        _connection->prepare(g_loadSourceStmt, g_loadSourceSQL)
            ("varchar", prepare::treat_string);
        
        _connection->prepare(g_loadD3SourcesStmt, g_loadD3SourcesSQL)
            ("integer", prepare::treat_direct);

        _connection->prepare(g_updateD3SourceStmt, g_updateD3SourceSQL)
            ("varchar", prepare::treat_string)
            ("varchar", prepare::treat_string)
            ("varchar", prepare::treat_string)
            ("varchar", prepare::treat_string)
            ("varchar", prepare::treat_string)
            ("varchar", prepare::treat_string)
            ("varchar", prepare::treat_string)
            ("varchar", prepare::treat_string)
            ("varchar", prepare::treat_string)
            ("varchar", prepare::treat_string)
            ("varchar", prepare::treat_string)
            ("integer", prepare::treat_direct)
            ("varchar", prepare::treat_string)
            ("varchar", prepare::treat_string)
            ("varchar", prepare::treat_string)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct);
            
        _connection->prepare(g_saveRecorderStmt, g_saveRecorderSQL)
            ("integer", prepare::treat_direct)
            ("varchar", prepare::treat_string);
        
        _connection->prepare(g_saveDestStmt, g_saveDestSQL)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct)
            ("varchar", prepare::treat_string)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct);
        
        _connection->prepare(g_saveHDDestStmt, g_saveHDDestSQL)
            ("integer", prepare::treat_direct)
            ("varchar", prepare::treat_string)
            ("varchar", prepare::treat_string)
            ("varchar", prepare::treat_string)
            ("varchar", prepare::treat_string)
            ("varchar", prepare::treat_string)
            ("varchar", prepare::treat_string)
            ("varchar", prepare::treat_string)
            ("varchar", prepare::treat_string)
            ("varchar", prepare::treat_string)
            ("varchar", prepare::treat_string)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct);
        
        _connection->prepare(g_saveDigibetaDestStmt, g_saveDigibetaDestSQL)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct);
        
        _connection->prepare(g_saveSessionStmt, g_saveSessionSQL)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct)
            ("varchar", prepare::treat_string)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct)
            ("varchar", prepare::treat_string)
            ("integer", prepare::treat_direct);
        
        _connection->prepare(g_getSourceRecInstanceStmt, g_getSourceRecInstanceSQL)
            ("integer", prepare::treat_direct);
            
        _connection->prepare(g_resetSourceRecInstanceStmt, g_resetSourceRecInstanceSQL)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct);
            
        _connection->prepare(g_updateSessionStmt, g_updateSessionSQL)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct)
            ("varchar", prepare::treat_string)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct);
        
        _connection->prepare(g_loadAllMinimalSessionsStmt, g_loadAllMinimalSessionsSQL)
            ("integer", prepare::treat_direct);
        
        _connection->prepare(g_loadMinimalSessionsStmt, g_loadMinimalSessionsSQL)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct);
        
        _connection->prepare(g_deleteSessionStmt, g_deleteSessionSQL)
            ("integer", prepare::treat_direct);
            
        _connection->prepare(g_sessionInProgressStmt, g_sessionInProgressSQL)
            ("varchar", prepare::treat_string);
            
        _connection->prepare(g_saveSessionSourceStmt, g_saveSessionSourceSQL)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct);
        
        _connection->prepare(g_saveSessionDestStmt, g_saveSessionDestSQL)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct);
        
        _connection->prepare(g_loadCacheStmt, g_loadCacheSQL)
            ("integer", prepare::treat_direct)
            ("varchar", prepare::treat_string);
        
        _connection->prepare(g_saveCacheStmt, g_saveCacheSQL)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct)
            ("varchar", prepare::treat_string);
        
        _connection->prepare(g_loadCacheItemsStmt, g_loadCacheItemsSQL)
            ("integer", prepare::treat_direct);
        
        _connection->prepare(g_loadCacheItemStmt, g_loadCacheItemSQL)
            ("integer", prepare::treat_direct)
            ("varchar", prepare::treat_string);
        
        _connection->prepare(g_loadSessionDestsStmt, g_loadSessionDestsSQL)
            ("integer", prepare::treat_direct);
            
        _connection->prepare(g_deleteDestStmt, g_deleteDestSQL)
            ("integer", prepare::treat_direct);
        
        _connection->prepare(g_deleteSessionDestStmt, g_deleteSessionDestSQL)
            ("integer", prepare::treat_direct);
        
        _connection->prepare(g_loadDestinationsStmt, g_loadDestinationsSQL)
            ("varchar", prepare::treat_string);
            
        _connection->prepare(g_updateHDDestStmt, g_updateHDDestSQL)
            ("varchar", prepare::treat_string)
            ("varchar", prepare::treat_string)
            ("varchar", prepare::treat_string)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct);
            
        _connection->prepare(g_removeHDDFromCacheStmt, g_removeHDDFromCacheSQL)
            ("integer", prepare::treat_direct);
        
        _connection->prepare(g_loadHDDestStmt, g_loadHDDestSQL)
            ("integer", prepare::treat_direct);
            
        _connection->prepare(g_loadHDDest2Stmt, g_loadHDDest2SQL)
            ("integer", prepare::treat_direct);
            
        _connection->prepare(g_loadDigibetaDestStmt, g_loadDigibetaDestSQL)
            ("integer", prepare::treat_direct);
            
        _connection->prepare(g_loadHDDSourceStmt, g_loadHDDSourceSQL)
            ("integer", prepare::treat_direct);
            
        _connection->prepare(g_loadAllLTOTransferSessionsStmt, g_loadAllLTOTransferSessionsSQL)
            ("integer", prepare::treat_direct);
        
        _connection->prepare(g_loadLTOTransferSessionsStmt, g_loadLTOTransferSessionsSQL)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct);
        
        _connection->prepare(g_saveLTOTransferSessionStmt, g_saveLTOTransferSessionSQL)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct)
            ("varchar", prepare::treat_string)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct)
            ("varchar", prepare::treat_string)
            ("varchar", prepare::treat_string)
            ("varchar", prepare::treat_string)
            ("varchar", prepare::treat_string);
        
        _connection->prepare(g_updateLTOTransferSessionStmt, g_updateLTOTransferSessionSQL)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct)
            ("varchar", prepare::treat_string)
            ("integer", prepare::treat_direct);
        
        _connection->prepare(g_deleteLTOTransferSessionStmt, g_deleteLTOTransferSessionSQL)
            ("integer", prepare::treat_direct);
            
        _connection->prepare(g_updateCompletedLTOFileStmt, g_updateCompletedLTOFileSQL)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct);
            
        _connection->prepare(g_checkLTOUsedStmt, g_checkLTOUsedSQL)
            ("varchar", prepare::treat_string);
            
        _connection->prepare(g_checkDigibetaUsedStmt, g_checkDigibetaUsedSQL)
            ("varchar", prepare::treat_string);
            
        _connection->prepare(g_checkD3UsedStmt, g_checkD3UsedSQL)
            ("varchar", prepare::treat_string);
            
        _connection->prepare(g_loadSessionLTOStmt, g_loadSessionLTOSQL)
            ("integer", prepare::treat_direct);
        
        _connection->prepare(g_loadLTOFilesStmt, g_loadLTOFilesSQL)
            ("integer", prepare::treat_direct);
            
        _connection->prepare(g_saveLTOStmt, g_saveLTOSQL)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct)
            ("varchar", prepare::treat_string);
        
        _connection->prepare(g_saveLTOFileStmt, g_saveLTOFileSQL)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct)
            ("varchar", prepare::treat_string)
            ("integer", prepare::treat_direct)
            ("integer", prepare::treat_direct)
            ("varchar", prepare::treat_string)
            ("varchar", prepare::treat_string)
            ("varchar", prepare::treat_string)
            ("varchar", prepare::treat_string)
            ("varchar", prepare::treat_string)
            ("varchar", prepare::treat_string)
            ("varchar", prepare::treat_string)
            ("varchar", prepare::treat_string)
            ("varchar", prepare::treat_string)
            ("varchar", prepare::treat_string)
            ("varchar", prepare::treat_string)
            ("integer", prepare::treat_direct)
            ("varchar", prepare::treat_string)
            ("varchar", prepare::treat_string)
            ("varchar", prepare::treat_string)
            ("integer", prepare::treat_direct)
            ("varchar", prepare::treat_string)
            ("varchar", prepare::treat_string)
            ("varchar", prepare::treat_string)
            ("varchar", prepare::treat_string)
            ("varchar", prepare::treat_string)
            ("varchar", prepare::treat_string)
            ("varchar", prepare::treat_string)
            ("varchar", prepare::treat_string)
            ("varchar", prepare::treat_string)
            ("varchar", prepare::treat_string)
            ("varchar", prepare::treat_string);
            
        _connection->prepare(g_deleteLTOStmt, g_deleteLTOSQL)
            ("integer", prepare::treat_direct);

            
        _connection->prepare(g_saveInfaxExportStmt, g_saveInfaxExportSQL)
            ("integer", prepare::treat_direct)
            ("varchar", prepare::treat_string)
            ("varchar", prepare::treat_string)
            ("integer", prepare::treat_direct)
            ("varchar", prepare::treat_string)
            ("varchar", prepare::treat_string)
            ("varchar", prepare::treat_string)
            ("varchar", prepare::treat_string)
            ("varchar", prepare::treat_string)
            ("varchar", prepare::treat_string)
            ("varchar", prepare::treat_string)
            ("varchar", prepare::treat_string)
            ("varchar", prepare::treat_string);
        
    }
    END_WORK("Prepare database statements")
}

bool PostgresRecorderDatabase::haveConnection()
{
    return PostgresDatabase::haveConnection();
}

RecorderTable* PostgresRecorderDatabase::loadRecorder(string name)
{
    START_WORK
    {
        LOCK_SECTION(_accessMutex);

        work ts(*_connection, "load recorder");
        result res = ts.prepared(g_loadRecorderStmt)(name).exec();
        if (res.size() == 0)
        {
            return 0;
        }
        
        auto_ptr<RecorderTable> recorder(new RecorderTable());
        recorder->databaseId = readLong(res[0][0], -1);
        recorder->name = readString(res[0][1]);
        
        return recorder.release();
    }
    END_WORK("Load recorder")
}

void PostgresRecorderDatabase::saveRecorder(RecorderTable* recorder)
{
    START_WORK
    {
        LOCK_SECTION(_accessMutex);
        
        work ts(*_connection, "save recorder");
        long nextId = getNextId(ts, "rec_id_seq");
        
        ts.prepared(g_saveRecorderStmt)(nextId)(recorder->name).exec();
        ts.commit();
            
        recorder->databaseId = nextId;
    }
    END_WORK("Save recorder")
}

Source* PostgresRecorderDatabase::loadSource(string barcode)
{
    START_WORK
    {
        LOCK_SECTION(_accessMutex);

        // read the Source
        
        work ts(*_connection, "load source");
        result res = ts.prepared(g_loadSourceStmt)(barcode).exec();
        
        if (res.size() == 0)
        {
            Logging::warning("Failed to load source with barcode '%s'\n", barcode.c_str());
            return 0;
        }
        
        if (readLong(res[0][1], -1) != D3_TAPE_SOURCE_TYPE)
        {
            Logging::warning("Unknown source type %d\n", readLong(res[0][1], -1));
            return 0;
        }
        
        auto_ptr<Source> retSource(new Source());
        retSource->databaseId = readLong(res[0][0], -1);
        // res[0][1] "src_type_id" not yet used because the type is assumed to  == 1 (D3) 
        retSource->barcode = readString(res[0][2]);
        retSource->recInstance = readLong(res[0][3], -1);
        
        // read the concrete D3 sources
        retSource->concreteSources = loadD3Sources(ts, retSource->databaseId);
        
        return retSource.release();
    }
    END_WORK("Load source")
}

long PostgresRecorderDatabase::getNewSourceRecordingInstance(long sourceId)
{
    START_WORK
    {
        LOCK_SECTION(_accessMutex);
        
        work ts(*_connection, "get new source recording instance");
        result res = ts.prepared(g_getSourceRecInstanceStmt)(sourceId).exec();
        ts.commit();
        
        if (res.size() == 0)
        {
            REC_LOGTHROW(("Failed to get a new source recording instance number for source %ld", sourceId));
        }
        
        return readLong(res[0][0], -1);
    }
    END_WORK("Get new source recording instance number")
}

void PostgresRecorderDatabase::resetSourceRecordingInstance(long sourceId, long firstTry, long lastTry)
{
    START_WORK
    {
        LOCK_SECTION(_accessMutex);
        
        work ts(*_connection, "reset source recording instance");
        result res = ts.prepared(g_resetSourceRecInstanceStmt)
            (firstTry)
            (lastTry)
            (sourceId).exec();
        ts.commit();
    }
    END_WORK("Reset source recording instance number")
}

void PostgresRecorderDatabase::updateConcreteSource(ConcreteSource* source)
{
    // only D3 sources are supported
    REC_ASSERT(source->getTypeId() == D3_TAPE_SOURCE_TYPE);
    D3Source* d3Source = dynamic_cast<D3Source*>(source);
    
    START_WORK
    {
        LOCK_SECTION(_accessMutex);
        
        work ts(*_connection, "update d3 source");
        
        ts.prepared(g_updateD3SourceStmt)
            (d3Source->format, d3Source->format.size() > 0)
            (d3Source->progTitle, d3Source->progTitle.size() > 0)
            (d3Source->episodeTitle, d3Source->episodeTitle.size() > 0)
            (writeDate(d3Source->txDate), d3Source->txDate != g_nullDate)
            (d3Source->magPrefix, d3Source->magPrefix.size() > 0)
            (d3Source->progNo, d3Source->progNo.size() > 0)
            (d3Source->prodCode, d3Source->prodCode.size() > 0)
            (d3Source->spoolStatus, d3Source->spoolStatus.size() > 0)
            (writeDate(d3Source->stockDate), d3Source->stockDate != g_nullDate)
            (d3Source->spoolDescr, d3Source->spoolDescr.size() > 0)
            (d3Source->memo, d3Source->memo.size() > 0)
            (d3Source->duration, d3Source->duration >= 0)
            (d3Source->spoolNo, d3Source->spoolNo.size() > 0)
            (d3Source->accNo, d3Source->accNo.size() > 0)
            (d3Source->catDetail, d3Source->catDetail.size() > 0)
            (d3Source->itemNo, d3Source->itemNo > 0)
            (d3Source->databaseId).exec();
        
        ts.commit();
    }
    END_WORK("Update D3 source")
}

vector<RecordingSessionTable*> PostgresRecorderDatabase::loadMinimalSessions(long recorderId, int status)
{
    START_WORK
    {
        LOCK_SECTION(_accessMutex);

        work ts(*_connection, "load minimal sessions");
        result res;
        if (status >= 0)
        {
            res = ts.prepared(g_loadMinimalSessionsStmt)(recorderId)(status).exec();
        }
        else
        {
            res = ts.prepared(g_loadAllMinimalSessionsStmt)(recorderId).exec();
        }
        
        AutoPointerVector<RecordingSessionTable> result;
        RecordingSessionTable* session;
        result::const_iterator iter;
        for (iter = res.begin(); iter != res.end(); iter++)
        {
            result.get().push_back(new RecordingSessionTable());
            session = result.get().back();
            session->databaseId = readLong((*iter)[0], -1);
            session->recorder = 0; // (*iter)[1]; minimal session has no recorder
            session->creation = readTimestamp((*iter)[2], g_nullTimestamp);
            session->updated = readTimestamp((*iter)[3], g_nullTimestamp);
            session->status = readInt((*iter)[4], -1);
            session->abortInitiator = readInt((*iter)[5], -1);
            session->comments = readString((*iter)[6]);
            session->totalD3Errors = readLong((*iter)[7], 0);
            // minimal session has no sources and no destinations
        }
        return result.release();
    }
    END_WORK("Load minimal sessions")
}

void PostgresRecorderDatabase::saveSession(RecordingSessionTable* session)
{
    START_WORK
    {
        LOCK_SECTION(_accessMutex);
        
        work ts(*_connection, "save session");
        
        
        // save the session
        long nextSessionId = getNextId(ts, "rcs_id_seq");
        Timestamp now = getTimestampNow(ts);
        
        ts.prepared(g_saveSessionStmt)
            (nextSessionId)
            (session->recorder->databaseId)
            (writeTimestamp(now))
            (session->status)
            (session->abortInitiator, session->abortInitiator >= 0)
            (session->comments)
            (session->totalD3Errors, session->totalD3Errors >= 0).exec();
        session->databaseId = nextSessionId;
        session->creation = now;
        
        
        // save the session source link
        REC_ASSERT(session->source != 0);
        long nextSourceLinkId = getNextId(ts, "ssc_id_seq");
        
        ts.prepared(g_saveSessionSourceStmt)
            (nextSourceLinkId)
            (nextSessionId)
            (session->source->source->databaseId).exec();
        session->source->databaseId = nextSourceLinkId;

        
        ts.commit();
    }
    END_WORK("Save session")
}

void PostgresRecorderDatabase::saveSessionDestinations(RecordingSessionTable* session)
{
    START_WORK
    {
        LOCK_SECTION(_accessMutex);
        
        work ts(*_connection, "save session destinations");
        
        // save the session destination and destination links
        vector<RecordingSessionDestinationTable*>::const_iterator iter;
        for (iter = session->destinations.begin(); iter != session->destinations.end(); iter++)
        {
            if ((*iter)->databaseId >= 0)
            {
                // already present in the database
                continue;
            }

            // save the destination
            saveDestination(ts, (*iter)->destination);
            
            // save the destination link
            long nextDestLinkId = getNextId(ts, "sdt_id_seq");
            
            ts.prepared(g_saveSessionDestStmt)
                (nextDestLinkId)
                (session->databaseId)
                ((*iter)->destination->databaseId).exec();
            (*iter)->databaseId = nextDestLinkId;
        }
        
        ts.commit();
    }
    END_WORK("Save session destinations")
}

void PostgresRecorderDatabase::updateSession(RecordingSessionTable* session)
{
    START_WORK
    {
        LOCK_SECTION(_accessMutex);
        
        work ts(*_connection, "update session");
        
        ts.prepared(g_updateSessionStmt)
            (session->status)
            (session->abortInitiator, session->abortInitiator >= 0)
            (session->comments)
            (session->totalD3Errors)
            (session->databaseId).exec();
        
        ts.commit();
    }
    END_WORK("Update session")
}

void PostgresRecorderDatabase::deleteSession(RecordingSessionTable* session)
{
    START_WORK
    {
        LOCK_SECTION(_accessMutex);
        
        work ts(*_connection, "delete session");
        
        ts.prepared(g_deleteSessionStmt)
            (session->databaseId).exec();
        
        ts.commit();
        
        session->databaseId = -1;
    }
    END_WORK("Delete session")
}

bool PostgresRecorderDatabase::sessionInProgress(string sourceBarcode)
{
    START_WORK
    {
        LOCK_SECTION(_accessMutex);
        
        work ts(*_connection, "source used in session in progress");
        result res = ts.prepared(g_sessionInProgressStmt)
            (sourceBarcode).exec();
        
        return res.size() > 0;
    }
    END_WORK("Source used in session in progress")
}

CacheTable* PostgresRecorderDatabase::loadCache(long recorderId, string path)
{
    START_WORK
    {
        LOCK_SECTION(_accessMutex);

        work ts(*_connection, "load cache");
        result res = ts.prepared(g_loadCacheStmt)(recorderId)(path).exec();
        if (res.size() == 0)
        {
            return 0;
        }
        
        auto_ptr<CacheTable> cache(new CacheTable());
        cache->databaseId = readLong(res[0][0], -1);
        cache->recorderId = readLong(res[0][1], -1);
        cache->path = readString(res[0][2]);
        
        return cache.release();
    }
    END_WORK("Load cache")
}

void PostgresRecorderDatabase::saveCache(CacheTable* cache)
{
    START_WORK
    {
        LOCK_SECTION(_accessMutex);
        
        work ts(*_connection, "save cache");
        long nextId = getNextId(ts, "rch_id_seq");
        
        ts.prepared(g_saveCacheStmt)(nextId)(cache->recorderId)(cache->path).exec();
        ts.commit();
            
        cache->databaseId = nextId;
    }
    END_WORK("Save cache")
}

vector<CacheItem*> PostgresRecorderDatabase::loadCacheItems(long cacheId)
{
    START_WORK
    {
        LOCK_SECTION(_accessMutex);

        work ts(*_connection, "load cache items");
        result res = ts.prepared(g_loadCacheItemsStmt)(cacheId).exec();
        
        AutoPointerVector<CacheItem> result;
        CacheItem* item;
        result::const_iterator iter;
        for (iter = res.begin(); iter != res.end(); iter++)
        {
            result.get().push_back(new CacheItem());
            item = result.get().back();
            item->hdDestinationId = readLong((*iter)[0], -1);
            item->name = readString((*iter)[1]);
            item->browseName = readString((*iter)[2]);
            item->pseName = readString((*iter)[3]);
            item->pseResult = readInt((*iter)[4], 0);
            item->size = readInt64((*iter)[5], -1);
            item->duration = readInt64((*iter)[6], -1);
            item->sessionId = readLong((*iter)[7], -1);
            item->sessionCreation = readTimestamp((*iter)[8], g_nullTimestamp);
            item->sessionComments = readString((*iter)[9]);
            item->sessionStatus = readInt((*iter)[10], -1);
            item->sourceSpoolNo = readString((*iter)[11]);
            item->sourceItemNo = readInt((*iter)[12], 0);
            item->sourceProgNo = readString((*iter)[13]);
            item->sourceMagPrefix = readString((*iter)[14]);
            item->sourceProdCode = readString((*iter)[15]);
        }
        
        return result.release();
    }
    END_WORK("Load cache items")
}

CacheItem* PostgresRecorderDatabase::loadCacheItem(long cacheId, string name)
{
    START_WORK
    {
        LOCK_SECTION(_accessMutex);

        work ts(*_connection, "load cache item");
        result res = ts.prepared(g_loadCacheItemStmt)(cacheId)(name).exec();
        if (res.size() == 0)
        {
            return 0;
        }
        
        // note: there could in theory be more than 1 item in the result, but the
        // results are orderered by identifier (which increments sequentially) and 
        // therefore we take the first item in the result which is the latest item 
        
        auto_ptr<CacheItem> item(new CacheItem());
        item->hdDestinationId = readLong(res[0][0], -1);
        item->name = readString(res[0][1]);
        item->browseName = readString(res[0][2]);
        item->pseName = readString(res[0][3]);
        item->pseResult = readInt(res[0][4], 0);
        item->size = readInt64(res[0][5], -1);
        item->duration = readInt64(res[0][6], -1);
        item->sessionId = readLong(res[0][7], -1);
        item->sessionCreation = readTimestamp(res[0][8], g_nullTimestamp);
        item->sessionComments = readString(res[0][9]);
        item->sessionStatus = readInt(res[0][10], -1);
        item->sourceSpoolNo = readString(res[0][11]);
        item->sourceItemNo = readInt(res[0][12], 0);
        item->sourceProgNo = readString(res[0][13]);
        item->sourceMagPrefix = readString(res[0][14]);
        item->sourceProdCode = readString(res[0][15]);
        
        return item.release();
    }
    END_WORK("Load cache item")
}

vector<Destination*> PostgresRecorderDatabase::loadSessionDestinations(long sessionId)
{
    START_WORK
    {
        LOCK_SECTION(_accessMutex);

        work ts(*_connection, "load session destinations");
        result res;
        res = ts.prepared(g_loadSessionDestsStmt)(sessionId).exec();
        
        AutoPointerVector<Destination> sessionDests;
        Destination* dest = 0;
        vector<int> typeIds;
        result::const_iterator resIter;
        for (resIter = res.begin(); resIter != res.end(); resIter++)
        {
            sessionDests.get().push_back(new Destination());
            dest = sessionDests.get().back();
            dest->databaseId = readLong((*resIter)[0], -1);
            typeIds.push_back(readInt((*resIter)[1], -1));
            dest->barcode = readString((*resIter)[2]);
            dest->sourceId = readLong((*resIter)[3], -1);
            dest->d3SourceId = readLong((*resIter)[4], -1);
            dest->ingestItemNo = readInt((*resIter)[5], -1);
        }
        
        vector<Destination*>::const_iterator destIter;
        vector<int>::const_iterator typeIdIter;
        for (destIter = sessionDests.get().begin(), typeIdIter = typeIds.begin(); 
            destIter != sessionDests.get().end(), typeIdIter != typeIds.end(); 
            destIter++, typeIdIter++)
        {
            // load concrete destination
            if ((*typeIdIter) == HARD_DISK_DEST_TYPE)
            {
                (*destIter)->concreteDestination = loadHardDiskDestination(ts, (*destIter)->databaseId);
            }
            else // (*typeIdIter) == DIGIBETA_DEST_TYPE)
            {
                REC_ASSERT((*typeIdIter) == DIGIBETA_DEST_TYPE);
                (*destIter)->concreteDestination = loadDigibetaDestination(ts, (*destIter)->databaseId);
            }
        }
        
        return sessionDests.release();
    }
    END_WORK("Load session destinations")
}

vector<Destination*> PostgresRecorderDatabase::loadDestinations(string barcode)
{
    START_WORK
    {
        LOCK_SECTION(_accessMutex);

        // read the Destination
        
        
        work ts(*_connection, "load destinations");
        result res = ts.prepared(g_loadDestinationsStmt)(barcode).exec();
        
        AutoPointerVector<Destination> dests;
        Destination* dest = 0;
        vector<int> typeIds;
        result::const_iterator resIter;
        for (resIter = res.begin(); resIter != res.end(); resIter++)
        {
            dests.get().push_back(new Destination());
            dest = dests.get().back();
            dest->databaseId = readLong((*resIter)[0], -1);
            typeIds.push_back(readInt((*resIter)[1], -1));
            dest->barcode = readString((*resIter)[2]);
            dest->sourceId = readLong((*resIter)[3], -1);
            dest->d3SourceId = readLong((*resIter)[4], -1);
            dest->ingestItemNo = readInt((*resIter)[5], -1);
        }
        
        vector<Destination*>::const_iterator destIter;
        vector<int>::const_iterator typeIdIter;
        for (destIter = dests.get().begin(), typeIdIter = typeIds.begin(); 
            destIter != dests.get().end(), typeIdIter != typeIds.end(); 
            destIter++, typeIdIter++)
        {
            // load concrete destination
            if ((*typeIdIter) == HARD_DISK_DEST_TYPE)
            {
                (*destIter)->concreteDestination = loadHardDiskDestination(ts, (*destIter)->databaseId);
            }
            else // (*typeIdIter) == DIGIBETA_DEST_TYPE)
            {
                REC_ASSERT((*typeIdIter) == DIGIBETA_DEST_TYPE);
                (*destIter)->concreteDestination = loadDigibetaDestination(ts, (*destIter)->databaseId);
            }
        }
        
        return dests.release();
    }
    END_WORK("Load destinations")
}

void PostgresRecorderDatabase::deleteDestination(Destination* dest)
{
    START_WORK
    {
        LOCK_SECTION(_accessMutex);
        
        work ts(*_connection, "delete destination");
        ts.prepared(g_deleteDestStmt)
            (dest->databaseId).exec();
        ts.commit();
        
        dest->databaseId = -1;
    }
    END_WORK("Delete destination")
}

void PostgresRecorderDatabase::deleteSessionDestination(RecordingSessionDestinationTable* sessionDest)
{
    START_WORK
    {
        LOCK_SECTION(_accessMutex);
        
        work ts(*_connection, "delete session destination");
        ts.prepared(g_deleteSessionDestStmt)
            (sessionDest->databaseId).exec();
        ts.commit();
        
        sessionDest->databaseId = -1;
        if (sessionDest->destination != 0)
        {
            sessionDest->destination->databaseId = -1;
            if (sessionDest->destination->concreteDestination != 0)
            {
                sessionDest->destination->concreteDestination->databaseId = -1;
            }
        }
    }
    END_WORK("Delete session destination")
}

void PostgresRecorderDatabase::updateHardDiskDestination(HardDiskDestination* hdDest)
{
    START_WORK
    {
        LOCK_SECTION(_accessMutex);
        
        work ts(*_connection, "update hard disk destination");
        ts.prepared(g_updateHDDestStmt)
            (hdDest->materialPackageUID.toString(), hdDest->materialPackageUID != g_nullUMID)
            (hdDest->filePackageUID.toString(), hdDest->filePackageUID != g_nullUMID)
            (hdDest->tapePackageUID.toString(), hdDest->tapePackageUID != g_nullUMID)
            (hdDest->size, hdDest->size >= 0)
            (hdDest->duration, hdDest->duration >= 0)
            (hdDest->browseSize, hdDest->browseSize >= 0)
            (hdDest->pseResult, hdDest->pseResult > 0)
            (hdDest->cacheId, hdDest->cacheId >= 0)
            (hdDest->databaseId).exec();
        ts.commit();
    }
    END_WORK("Update hard disk destination")
}

void PostgresRecorderDatabase::removeHardDiskDestinationFromCache(long hdDestId)
{
    START_WORK
    {
        LOCK_SECTION(_accessMutex);
        
        work ts(*_connection, "remove hard disk destination from cache");
        ts.prepared(g_removeHDDFromCacheStmt)
            (hdDestId).exec();
        ts.commit();
    }
    END_WORK("Remove hard disk destination from cache")
}

HardDiskDestination* PostgresRecorderDatabase::loadHDDest(long hdDestId)
{
    START_WORK
    {
        LOCK_SECTION(_accessMutex);

        work ts(*_connection, "load HDD 2");
        result res = ts.prepared(g_loadHDDest2Stmt)(hdDestId).exec();
        
        if (res.size() == 0)
        {
            REC_LOGTHROW(("Failed to load hard disk destination with identifier %ld", hdDestId)); 
        }
        
        auto_ptr<HardDiskDestination> hdd(new HardDiskDestination());
        hdd->databaseId = readLong(res[0][0], -1);
        hdd->hostName = readString(res[0][1]);
        hdd->path = readString(res[0][2]);
        hdd->name = readString(res[0][3]);
        hdd->materialPackageUID.fromString(readString(res[0][4]));
        hdd->filePackageUID.fromString(readString(res[0][5]));
        hdd->tapePackageUID.fromString(readString(res[0][6]));
        hdd->browsePath = readString(res[0][7]);
        hdd->browseName = readString(res[0][8]);
        hdd->psePath = readString(res[0][9]);
        hdd->pseName = readString(res[0][10]);
        hdd->pseResult = readInt(res[0][11], 0);
        hdd->size = readInt64(res[0][12], -1);
        hdd->duration = readInt64(res[0][13], -1);
        hdd->browseSize = readInt64(res[0][14], -1);
        hdd->cacheId = readLong(res[0][15], -1);
        
        return hdd.release();
    }
    END_WORK("Load hard disk destination 2")
}

Source* PostgresRecorderDatabase::loadHDDSource(long hdDestId)
{
    START_WORK
    {
        LOCK_SECTION(_accessMutex);

        work ts(*_connection, "load HDD source");
        result res = ts.prepared(g_loadHDDSourceStmt)(hdDestId).exec();
        
        if (res.size() == 0)
        {
            REC_LOGTHROW(("Failed to load source for hard disk destination %ld", hdDestId));
        }
        
        if (readLong(res[0][1], -1) != D3_TAPE_SOURCE_TYPE)
        {
            REC_LOGTHROW(("Unknown source type %d\n", readLong(res[0][1], -1)));
        }

        
        auto_ptr<Source> retSource(new Source());
        retSource->databaseId = readLong(res[0][0], -1);
        // res[0][1] "src_type_id" not yet used because the type is assumed to  == 1 (D3) 
        retSource->barcode = readString(res[0][2]);
        retSource->recInstance = readLong(res[0][3], -1);
        
        
        // read the concrete D3 sources
        retSource->concreteSources = loadD3Sources(ts, retSource->databaseId);
        
        return retSource.release();
    }
    END_WORK("Load HDD source")
}

vector<LTOTransferSessionTable*> PostgresRecorderDatabase::loadLTOTransferSessions(long recorderId, int status)
{
    START_WORK
    {
        LOCK_SECTION(_accessMutex);

        work ts(*_connection, "load lto transfer sessions");
        result res;
        if (status >= 0)
        {
            res = ts.prepared(g_loadLTOTransferSessionsStmt)(recorderId)(status).exec();
        }
        else
        {
            res = ts.prepared(g_loadAllLTOTransferSessionsStmt)(recorderId).exec();
        }
        
        AutoPointerVector<LTOTransferSessionTable> sessions;
        LTOTransferSessionTable* session;
        result::const_iterator iter1;
        for (iter1 = res.begin(); iter1 != res.end(); iter1++)
        {
            sessions.get().push_back(new LTOTransferSessionTable());
            session = sessions.get().back();
            session->databaseId = readLong((*iter1)[0], -1);
            session->recorderId = readLong((*iter1)[1], -1);
            session->creation = readTimestamp((*iter1)[2], g_nullTimestamp);
            session->updated = readTimestamp((*iter1)[3], g_nullTimestamp);
            session->status = readInt((*iter1)[4], -1);
            session->abortInitiator = readInt((*iter1)[5], -1);
            session->comments = readString((*iter1)[6]);
            session->tapeDeviceVendor = readString((*iter1)[7]);
            session->tapeDeviceModel = readString((*iter1)[8]);
            session->tapeDeviceRevision = readString((*iter1)[9]);
        }
        
        vector<LTOTransferSessionTable*>::iterator iter2;
        for (iter2 = sessions.get().begin(); iter2 != sessions.get().end(); iter2++)
        {
            (*iter2)->lto = loadSessionLTO(ts, (*iter2)->databaseId);
        }
        
        return sessions.release();
    }
    END_WORK("Load LTO transfer sessions")
}

void PostgresRecorderDatabase::saveLTOTransferSession(LTOTransferSessionTable* session)
{
    START_WORK
    {
        LOCK_SECTION(_accessMutex);
        
        work ts(*_connection, "save lto session");
        
        
        // save the session
        long nextSessionId = getNextId(ts, "lts_id_seq");
        Timestamp now = getTimestampNow(ts);
        
        ts.prepared(g_saveLTOTransferSessionStmt)
            (nextSessionId)
            (session->recorderId)
            (writeTimestamp(now))
            (session->status)
            (session->abortInitiator, session->abortInitiator >= 0)
            (session->comments)
            (session->tapeDeviceVendor)
            (session->tapeDeviceModel)
            (session->tapeDeviceRevision).exec();
        session->databaseId = nextSessionId;
        session->creation = now;
        
        if (session->lto != 0)
        {
            saveLTO(ts, nextSessionId, session->lto);
        }
        else
        {
            Logging::warning("Saving a LTO transfer session %ld with no LTO\n", nextSessionId);
        }
        
        ts.commit();
    }
    END_WORK("Save LTO transfer session")
}

void PostgresRecorderDatabase::updateLTOTransferSession(LTOTransferSessionTable* session)
{
    START_WORK
    {
        LOCK_SECTION(_accessMutex);
        
        work ts(*_connection, "update lto transfer session");
        
        ts.prepared(g_updateLTOTransferSessionStmt)
            (session->status)
            (session->abortInitiator, session->abortInitiator >= 0)
            (session->comments)
            (session->databaseId).exec();
        
        ts.commit();
    }
    END_WORK("Update lto transfer session")
}

void PostgresRecorderDatabase::deleteLTOTransferSession(LTOTransferSessionTable* session)
{
    START_WORK
    {
        LOCK_SECTION(_accessMutex);
        
        work ts(*_connection, "delete lto transfer session");
        
        ts.prepared(g_deleteLTOTransferSessionStmt)
            (session->databaseId).exec();
        
        if (session->lto != 0)
        {
            deleteLTO(ts, session->lto);
        }
            
        ts.commit();
        
        session->databaseId = -1;
    }
    END_WORK("Delete lto transfer session")
}

void PostgresRecorderDatabase::updateLTOFile(long id, int status)
{
    START_WORK
    {
        LOCK_SECTION(_accessMutex);
        
        work ts(*_connection, "update completed lto file");
        ts.prepared(g_updateCompletedLTOFileStmt)
            (status)
            (id).exec();
        ts.commit();
    }
    END_WORK("Update completed lto file")
}

bool PostgresRecorderDatabase::ltoUsedInCompletedSession(string spoolNo)
{
    START_WORK
    {
        LOCK_SECTION(_accessMutex);
        
        work ts(*_connection, "check lto used");
        result res = ts.prepared(g_checkLTOUsedStmt)
            (spoolNo).exec();
        
        return res.size() > 0;
    }
    END_WORK("Check lto used in completed session")
}

bool PostgresRecorderDatabase::digibetaUsedInCompletedSession(string spoolNo)
{
    START_WORK
    {
        LOCK_SECTION(_accessMutex);
        
        work ts(*_connection, "check digibeta used");
        result res = ts.prepared(g_checkDigibetaUsedStmt)
            (spoolNo).exec();
        
        return res.size() > 0;
    }
    END_WORK("Check digibeta used in completed session")
}

bool PostgresRecorderDatabase::d3UsedInCompletedSession(string spoolNo)
{
    START_WORK
    {
        LOCK_SECTION(_accessMutex);
        
        work ts(*_connection, "check d3 used");
        result res = ts.prepared(g_checkD3UsedStmt)
            (spoolNo).exec();
        
        return res.size() > 0;
    }
    END_WORK("Check D3 used in completed session")
}

void PostgresRecorderDatabase::saveInfaxExport(InfaxExportTable* infaxExport)
{
    START_WORK
    {
        LOCK_SECTION(_accessMutex);
        
        work ts(*_connection, "save infax export");
        
        long nextExportId = getNextId(ts, "ixe_id_seq");
        
        ts.prepared(g_saveInfaxExportStmt)
            (nextExportId)
            (writeTimestamp(infaxExport->transferDate))
            (infaxExport->d3SpoolNo)
            (infaxExport->d3ItemNo)
            (infaxExport->progTitle)
            (infaxExport->episodeTitle)
            (infaxExport->magPrefix)
            (infaxExport->progNo)
            (infaxExport->prodCode)
            (infaxExport->mxfName)
            (infaxExport->digibetaSpoolNo)
            (infaxExport->browseName)
            (infaxExport->ltoSpoolNo).exec();
        infaxExport->databaseId = nextExportId;
        
        ts.commit();
    }
    END_WORK("Save Infax export")
}


void PostgresRecorderDatabase::saveDestination(work& ts, Destination* destination)
{
    START_WORK
    {
        long nextDestId = getNextId(ts, "des_id_seq");
        
        // save destination
        ts.prepared(g_saveDestStmt)
            (nextDestId)
            (destination->concreteDestination->getTypeId())
            (destination->barcode, destination->barcode.size() > 0)
            (destination->sourceId)
            (destination->d3SourceId, destination->d3SourceId > 0)
            (destination->ingestItemNo, destination->ingestItemNo > 0).exec();
        destination->databaseId = nextDestId;

        if (destination->concreteDestination->getTypeId() == HARD_DISK_DEST_TYPE)
        {
            saveHardDiskDestination(ts, nextDestId, dynamic_cast<HardDiskDestination*>(destination->concreteDestination));
        }
        else // (destination->concreteDestination->getTypeId() == DIGIBETA_DEST_TYPE)
        {
            REC_ASSERT(destination->concreteDestination->getTypeId() == DIGIBETA_DEST_TYPE);
            saveDigibetaDestination(ts, nextDestId, dynamic_cast<DigibetaDestination*>(destination->concreteDestination));
        }
    }
    END_WORK("Save destination")
}

void PostgresRecorderDatabase::saveHardDiskDestination(work& ts, long destId, HardDiskDestination* hdd)
{
    START_WORK
    {
        long nextHDDId = getNextId(ts, "hdd_id_seq");
        
        ts.prepared(g_saveHDDestStmt)
            (nextHDDId)
            (hdd->hostName)
            (hdd->path)
            (hdd->name)
            (hdd->materialPackageUID.toString(), hdd->materialPackageUID != g_nullUMID)
            (hdd->filePackageUID.toString(), hdd->filePackageUID != g_nullUMID)
            (hdd->tapePackageUID.toString(), hdd->tapePackageUID != g_nullUMID)
            (hdd->browsePath)
            (hdd->browseName)
            (hdd->psePath)
            (hdd->pseName)
            (hdd->pseResult, hdd->pseResult > 0)
            (hdd->size, hdd->size >= 0)
            (hdd->duration, hdd->duration >= 0)
            (hdd->browseSize, hdd->browseSize >= 0)
            (hdd->cacheId)
            (destId).exec();
        hdd->databaseId = nextHDDId;
    }
    END_WORK("Save hard disk destination")
}

void PostgresRecorderDatabase::saveDigibetaDestination(work& ts, long destId, DigibetaDestination* dbd)
{
    START_WORK
    {
        long nextDBDId = getNextId(ts, "dbd_id_seq");
        
        ts.prepared(g_saveDigibetaDestStmt)
            (nextDBDId)
            (destId).exec();
        dbd->databaseId = nextDBDId;
    }
    END_WORK("Save digibeta destination")
}

HardDiskDestination* PostgresRecorderDatabase::loadHardDiskDestination(work& ts, long destDatabaseId)
{
    START_WORK
    {
        result res;
        res = ts.prepared(g_loadHDDestStmt)(destDatabaseId).exec();
        
        if (res.size() == 0)
        {
            REC_LOGTHROW(("Failed to load hard disk destination for destination %ld", destDatabaseId)); 
        }
        
        auto_ptr<HardDiskDestination> hdd(new HardDiskDestination());
        hdd->databaseId = readLong(res[0][0], -1);
        hdd->hostName = readString(res[0][1]);
        hdd->path = readString(res[0][2]);
        hdd->name = readString(res[0][3]);
        hdd->materialPackageUID.fromString(readString(res[0][4]));
        hdd->filePackageUID.fromString(readString(res[0][5]));
        hdd->tapePackageUID.fromString(readString(res[0][6]));
        hdd->browsePath = readString(res[0][7]);
        hdd->browseName = readString(res[0][8]);
        hdd->psePath = readString(res[0][9]);
        hdd->pseName = readString(res[0][10]);
        hdd->pseResult = readInt(res[0][11], 0);
        hdd->size = readInt64(res[0][12], -1);
        hdd->duration = readInt64(res[0][13], -1);
        hdd->browseSize = readInt64(res[0][14], -1);
        hdd->cacheId = readLong(res[0][15], -1);
        
        return hdd.release();
    }
    END_WORK("Load hard disk destination")
}

DigibetaDestination* PostgresRecorderDatabase::loadDigibetaDestination(work& ts, long destDatabaseId)
{
    START_WORK
    {
        result res;
        res = ts.prepared(g_loadDigibetaDestStmt)(destDatabaseId).exec();
        
        if (res.size() == 0)
        {
            REC_LOGTHROW(("Failed to load digibeta destination for destination %ld", destDatabaseId)); 
        }
        
        auto_ptr<DigibetaDestination> dbd(new DigibetaDestination());
        dbd->databaseId = readLong(res[0][0], -1);
        
        return dbd.release();
    }
    END_WORK("Load digibeta destination")
}

vector<ConcreteSource*> PostgresRecorderDatabase::loadD3Sources(pqxx::work& ts, long databaseId)
{
    START_WORK
    {
        result res = ts.prepared(g_loadD3SourcesStmt)(databaseId).exec();
        
        if (res.size() == 0)
        {
            REC_LOGTHROW(("Failed to load the concrete D3 source associated with source %ld\n", databaseId)); 
        }

        AutoPointerVector<ConcreteSource> d3Sources;
        D3Source* d3Source;
        
        result::const_iterator resIter;
        for (resIter = res.begin(); resIter != res.end(); resIter++)
        {
            d3Sources.get().push_back(new D3Source());
            d3Source = dynamic_cast<D3Source*>(d3Sources.get().back());

            d3Source->databaseId = readLong((*resIter)[0], -1);
            d3Source->format = readString((*resIter)[1]);
            d3Source->progTitle = readString((*resIter)[2]);
            d3Source->episodeTitle = readString((*resIter)[3]);
            d3Source->txDate = readDate((*resIter)[4], g_nullDate);
            d3Source->magPrefix = readString((*resIter)[5]);
            d3Source->progNo = readString((*resIter)[6]);
            d3Source->prodCode = readString((*resIter)[7]);
            d3Source->spoolStatus = readString((*resIter)[8]);
            d3Source->stockDate = readDate((*resIter)[9], g_nullDate);
            d3Source->spoolDescr = readString((*resIter)[10]);
            d3Source->memo = readString((*resIter)[11]);
            d3Source->duration = readInt64((*resIter)[12], -1);
            d3Source->spoolNo = readString((*resIter)[13]);
            d3Source->accNo = readString((*resIter)[14]);
            d3Source->catDetail = readString((*resIter)[15]);
            d3Source->itemNo = readInt((*resIter)[16], 0);
        }
        
        return d3Sources.release();
    }
    END_WORK("Load D3 Sources")
}    

LTOTable* PostgresRecorderDatabase::loadSessionLTO(pqxx::work& ts, long sessionId)
{
    START_WORK
    {
        result res = ts.prepared(g_loadSessionLTOStmt)(sessionId).exec();
        
        if (res.size() == 0)
        {
            Logging::warning("LTO transfer session %ld has no LTO\n", sessionId); 
            return 0;
        }
        
        auto_ptr<LTOTable> newLTOTable(new LTOTable());
        newLTOTable->databaseId = readLong(res[0][0], -1);
        newLTOTable->spoolNo = readString(res[0][1]);
        
        newLTOTable->ltoFiles = loadLTOFiles(ts, newLTOTable->databaseId);
        if (newLTOTable->ltoFiles.size() == 0)
        {
            Logging::warning("LTO %s (%ld) has no files\n", newLTOTable->spoolNo.c_str(), newLTOTable->databaseId); 
        }
        
        return newLTOTable.release();
    }
    END_WORK("Load LTO")
}

vector<LTOFileTable*> PostgresRecorderDatabase::loadLTOFiles(pqxx::work& ts, long ltoId)
{
    START_WORK
    {
        result res = ts.prepared(g_loadLTOFilesStmt)(ltoId).exec();
        
        AutoPointerVector<LTOFileTable> files;
        
        LTOFileTable* ltoFile;
        result::const_iterator resIter;
        for (resIter = res.begin(); resIter != res.end(); resIter++)
        {
            files.get().push_back(new LTOFileTable());
            ltoFile = files.get().back();

            ltoFile->databaseId = readLong((*resIter)[0], -1);
            ltoFile->status = readInt((*resIter)[1], -1);
            ltoFile->name = readString((*resIter)[2]);
            ltoFile->size = readInt64((*resIter)[3], -1);
            ltoFile->realDuration = readInt64((*resIter)[4], -1);
            ltoFile->format = readString((*resIter)[5]);
            ltoFile->progTitle = readString((*resIter)[6]);
            ltoFile->episodeTitle = readString((*resIter)[7]);
            ltoFile->txDate = readDate((*resIter)[8], g_nullDate);
            ltoFile->magPrefix = readString((*resIter)[9]);
            ltoFile->progNo = readString((*resIter)[10]);
            ltoFile->prodCode = readString((*resIter)[11]);
            ltoFile->spoolStatus = readString((*resIter)[12]);
            ltoFile->stockDate = readDate((*resIter)[13], g_nullDate);
            ltoFile->spoolDescr = readString((*resIter)[14]);
            ltoFile->memo = readString((*resIter)[15]);
            ltoFile->duration = readInt64((*resIter)[16], -1);
            ltoFile->spoolNo = readString((*resIter)[17]);
            ltoFile->accNo = readString((*resIter)[18]);
            ltoFile->catDetail = readString((*resIter)[19]);
            ltoFile->recordingSessionId = readLong((*resIter)[20], -1);
            ltoFile->hddHostName = readString((*resIter)[21]);
            ltoFile->hddPath = readString((*resIter)[22]);
            ltoFile->hddName = readString((*resIter)[23]);
            ltoFile->materialPackageUID.fromString(readString((*resIter)[24]));
            ltoFile->filePackageUID.fromString(readString((*resIter)[25]));
            ltoFile->tapePackageUID.fromString(readString((*resIter)[26]));
            ltoFile->sourceSpoolNo = readString((*resIter)[27]);
            ltoFile->sourceItemNo = readInt((*resIter)[28], 0);
            ltoFile->sourceProgNo = readString((*resIter)[29]);
            ltoFile->sourceMagPrefix = readString((*resIter)[30]);
            ltoFile->sourceProdCode = readString((*resIter)[31]);
        }
        
        return files.release();
    }
    END_WORK("Load LTO files")
}

void PostgresRecorderDatabase::saveLTO(pqxx::work& ts, long sessionId, LTOTable* lto)
{
    START_WORK
    {
        long nextId = getNextId(ts, "lto_id_seq");
        
        ts.prepared(g_saveLTOStmt)
            (nextId)
            (sessionId)
            (lto->spoolNo).exec();
            
        vector<LTOFileTable*>::iterator iter;
        for (iter = lto->ltoFiles.begin(); iter != lto->ltoFiles.end(); iter++)
        {
            saveLTOFile(ts, nextId, *iter);
        }
            
        lto->databaseId = nextId;
    }
    END_WORK("Save lto")
}

void PostgresRecorderDatabase::saveLTOFile(pqxx::work& ts, long ltoId, LTOFileTable* ltoFile)
{
    START_WORK
    {
        long nextId = getNextId(ts, "ltf_id_seq");
        
        ts.prepared(g_saveLTOFileStmt)
            (nextId)
            (ltoId)
            (ltoFile->status)
            (ltoFile->name)
            (ltoFile->size, ltoFile->size >= 0)
            (ltoFile->realDuration, ltoFile->realDuration >= 0)
            (ltoFile->format, ltoFile->format.size() > 0)
            (ltoFile->progTitle, ltoFile->progTitle.size() > 0)
            (ltoFile->episodeTitle, ltoFile->episodeTitle.size() > 0)
            (writeDate(ltoFile->txDate), ltoFile->txDate != g_nullDate)
            (ltoFile->magPrefix, ltoFile->magPrefix.size() > 0)
            (ltoFile->progNo, ltoFile->progNo.size() > 0)
            (ltoFile->prodCode, ltoFile->prodCode.size() > 0)
            (ltoFile->spoolStatus, ltoFile->spoolStatus.size() > 0)
            (writeDate(ltoFile->stockDate), ltoFile->stockDate != g_nullDate)
            (ltoFile->spoolDescr, ltoFile->spoolDescr.size() > 0)
            (ltoFile->memo, ltoFile->memo.size() > 0)
            (ltoFile->duration, ltoFile->duration >= 0)
            (ltoFile->spoolNo, ltoFile->spoolNo.size() > 0)
            (ltoFile->accNo, ltoFile->accNo.size() > 0)
            (ltoFile->catDetail, ltoFile->catDetail.size() > 0)
            (ltoFile->recordingSessionId)
            (ltoFile->hddHostName)
            (ltoFile->hddPath)
            (ltoFile->hddName)
            (ltoFile->materialPackageUID.toString(), ltoFile->materialPackageUID != g_nullUMID)
            (ltoFile->filePackageUID.toString(), ltoFile->filePackageUID != g_nullUMID)
            (ltoFile->tapePackageUID.toString(), ltoFile->tapePackageUID != g_nullUMID)
            (ltoFile->sourceSpoolNo)
            (ltoFile->sourceItemNo)
            (ltoFile->sourceProgNo)
            (ltoFile->sourceMagPrefix)
            (ltoFile->sourceProdCode).exec();
            
        ltoFile->databaseId = nextId;
    }
    END_WORK("Save lto file")
}

void PostgresRecorderDatabase::deleteLTO(pqxx::work& ts, LTOTable* lto)
{
    START_WORK
    {
        ts.prepared(g_deleteLTOStmt)
            (lto->databaseId).exec();
        
        lto->databaseId = -1;
    }
    END_WORK("Delete LTO")
}


