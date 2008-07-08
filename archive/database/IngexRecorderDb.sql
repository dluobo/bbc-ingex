
------------------------------------
-- TYPES
------------------------------------

------------------------------------
-- TABLES
------------------------------------

-- VERSION

CREATE TABLE Version
(
    ver_identifier INTEGER DEFAULT 1 NOT NULL,
    ver_version INTEGER NOT NULL,
    CHECK (ver_identifier = 1),    
    CONSTRAINT ver_pkey PRIMARY KEY (ver_identifier)
);


-- SOURCE

CREATE TABLE SourceType
(
    srt_identifier INTEGER NOT NULL,
    srt_name VARCHAR(256) NOT NULL,
    CONSTRAINT srt_key UNIQUE (srt_name),
    CONSTRAINT srt_pkey PRIMARY KEY (srt_identifier)
) WITHOUT OIDS;

CREATE SEQUENCE src_id_seq;
CREATE TABLE Source
(
    src_identifier INTEGER DEFAULT nextval('src_id_seq') NOT NULL,
    src_type_id INTEGER NOT NULL,
    src_barcode VARCHAR(32),
    src_rec_instance INTEGER DEFAULT 0 NOT NULL,
    CONSTRAINT src_barcode_key UNIQUE (src_barcode), -- creates an index for us
    CONSTRAINT src_type_fkey FOREIGN KEY (src_type_id) REFERENCES SourceType (srt_identifier),
    CONSTRAINT src_pkey PRIMARY KEY (src_identifier)
) WITHOUT OIDS;



-- D3 SOURCE

CREATE SEQUENCE d3s_id_seq;
CREATE TABLE D3Source
(
    d3s_identifier INTEGER DEFAULT nextval('d3s_id_seq') NOT NULL,
    d3s_format VARCHAR(6),
    d3s_prog_title VARCHAR(72),
    d3s_episode_title VARCHAR(144),
    d3s_tx_date DATE,
    d3s_mag_prefix VARCHAR(1),
    d3s_prog_no VARCHAR(8),
    d3s_prod_code VARCHAR(2),
    d3s_spool_status VARCHAR(1),
    d3s_stock_date DATE,
    d3s_spool_descr VARCHAR(29),
    d3s_memo VARCHAR(120),
    d3s_duration BIGINT,
    d3s_spool_no VARCHAR(14),
    d3s_acc_no VARCHAR(14),
    d3s_cat_detail VARCHAR(10),
    d3s_item_no INTEGER,
    d3s_source_id INTEGER NOT NULL,
    CONSTRAINT d3s_source_fkey FOREIGN KEY (d3s_source_id) REFERENCES Source (src_identifier)
            ON DELETE CASCADE,
    CONSTRAINT d3s_pkey PRIMARY KEY (d3s_identifier)
) WITHOUT OIDS;

CREATE INDEX d3s_source_index ON D3Source (d3s_source_id);



-- DESTINATION 

CREATE TABLE DestinationType
(
    dst_identifier INTEGER NOT NULL,
    dst_name VARCHAR(256) NOT NULL,
    CONSTRAINT dst_key UNIQUE (dst_name),
    CONSTRAINT dst_pkey PRIMARY KEY (dst_identifier)
) WITHOUT OIDS;

CREATE SEQUENCE des_id_seq;
CREATE TABLE Destination
(
    des_identifier INTEGER DEFAULT nextval('des_id_seq') NOT NULL,
    des_type_id INTEGER NOT NULL,
    des_barcode VARCHAR(32), -- not all destinations will have a barcode
    des_source_id INTEGER NOT NULL,
    des_d3source_id INTEGER,
    des_ingest_item_no INTEGER,
    CONSTRAINT des_type_fkey FOREIGN KEY (des_type_id) REFERENCES DestinationType (dst_identifier),
    CONSTRAINT des_source_fkey FOREIGN KEY (des_source_id) REFERENCES Source (src_identifier),
    CONSTRAINT des_d3source_fkey FOREIGN KEY (des_d3source_id) REFERENCES D3Source (d3s_identifier),
    CONSTRAINT des_pkey PRIMARY KEY (des_identifier)
) WITHOUT OIDS;

CREATE INDEX des_barcode_index ON Destination (des_barcode);
CREATE INDEX des_source_index ON Destination (des_source_id);
CREATE INDEX des_d3source_index ON Destination (des_d3source_id);



-- RECORDER

CREATE SEQUENCE rec_id_seq;
CREATE TABLE Recorder
(
    rec_identifier INTEGER DEFAULT nextval('rec_id_seq') NOT NULL,
    rec_name VARCHAR(256) NOT NULL,
    CONSTRAINT rec_name_key UNIQUE (rec_name),
    CONSTRAINT rec_pkey PRIMARY KEY (rec_identifier)
) WITHOUT OIDS;



-- RECORDER CACHE

CREATE SEQUENCE rch_id_seq;
CREATE TABLE RecorderCache
(
    rch_identifier INTEGER DEFAULT nextval('rch_id_seq') NOT NULL,
    rch_recorder_id INTEGER NOT NULL,
    rch_path VARCHAR(512) NOT NULL,
    CONSTRAINT rch_recorder_fkey FOREIGN KEY (rch_recorder_id) REFERENCES Recorder (rec_identifier)
                ON DELETE CASCADE,
    CONSTRAINT rch_pkey PRIMARY KEY (rch_identifier)
) WITHOUT OIDS;



-- RECORDING SESSION

CREATE TABLE RecordingSessionStatus
(
    rss_identifier INTEGER NOT NULL,
    rss_name VARCHAR(256) NOT NULL,
    CONSTRAINT rss_key UNIQUE (rss_name),
    CONSTRAINT rss_pkey PRIMARY KEY (rss_identifier)
) WITHOUT OIDS;

CREATE TABLE AbortInitiator
(
    abi_identifier INTEGER NOT NULL,
    abi_name VARCHAR(256) NOT NULL,
    CONSTRAINT abi_key UNIQUE (abi_name),
    CONSTRAINT abi_pkey PRIMARY KEY (abi_identifier)
) WITHOUT OIDS;


CREATE SEQUENCE rcs_id_seq;
CREATE TABLE RecordingSession
(
    rcs_identifier INTEGER DEFAULT nextval('rcs_id_seq') NOT NULL,
    rcs_recorder_id INTEGER NOT NULL,
    rcs_creation TIMESTAMP WITHOUT TIME ZONE DEFAULT now() NOT NULL,
    rcs_updated TIMESTAMP WITHOUT TIME ZONE DEFAULT now() NOT NULL,
    rcs_status_id INTEGER NOT NULL,
    rcs_abort_initiator_id INTEGER,
    rcs_comments VARCHAR(256),
    rcs_total_d3_errors INTEGER,
    CONSTRAINT rcs_status_fkey FOREIGN KEY (rcs_status_id) REFERENCES RecordingSessionStatus (rss_identifier),
    CONSTRAINT rcs_abort_initiator_fkey FOREIGN KEY (rcs_abort_initiator_id) REFERENCES AbortInitiator (abi_identifier),
    CONSTRAINT rcs_recorder_fkey FOREIGN KEY (rcs_recorder_id) REFERENCES Recorder (rec_identifier)
        ON DELETE CASCADE,
    CONSTRAINT rcs_pkey PRIMARY KEY (rcs_identifier)
) WITHOUT OIDS;



-- RECORDING SESSION SOURCE


CREATE SEQUENCE ssc_id_seq;
CREATE TABLE SessionSource
(
    ssc_identifier INTEGER DEFAULT nextval('ssc_id_seq') NOT NULL,
    ssc_session_id INTEGER NOT NULL,
    ssc_source_id INTEGER NOT NULL,
    CONSTRAINT ssc_source_fkey FOREIGN KEY (ssc_source_id) REFERENCES Source (src_identifier),
    CONSTRAINT ssc_session_fkey FOREIGN KEY (ssc_session_id) REFERENCES RecordingSession (rcs_identifier)
        ON DELETE CASCADE,
    CONSTRAINT ssc_pkey PRIMARY KEY (ssc_identifier)
) WITHOUT OIDS;

CREATE INDEX ssc_source_index ON SessionSource (ssc_source_id);
CREATE INDEX ssc_session_index ON SessionSource (ssc_session_id);


-- RECORDING SESSION DESTINATION


CREATE SEQUENCE sdt_id_seq;
CREATE TABLE SessionDestination
(
    sdt_identifier INTEGER DEFAULT nextval('sdt_id_seq') NOT NULL,
    sdt_session_id INTEGER NOT NULL,
    sdt_dest_id INTEGER NOT NULL,
    CONSTRAINT sdt_dest_fkey FOREIGN KEY (sdt_dest_id) REFERENCES Destination (des_identifier),
    CONSTRAINT sdt_session_fkey FOREIGN KEY (sdt_session_id) REFERENCES RecordingSession (rcs_identifier)
        ON DELETE CASCADE,
    CONSTRAINT sdt_pkey PRIMARY KEY (sdt_identifier)
) WITHOUT OIDS;

CREATE INDEX sdt_dest_index ON SessionDestination (sdt_dest_id);
CREATE INDEX sdt_session_index ON SessionDestination (sdt_session_id);




-- HARD DISK DESTINATION


CREATE TABLE PSEResult
(
    pse_identifier INTEGER NOT NULL,
    pse_name VARCHAR(256) NOT NULL,
    CONSTRAINT pse_key UNIQUE (pse_name),
    CONSTRAINT pse_pkey PRIMARY KEY (pse_identifier)
) WITHOUT OIDS;


CREATE SEQUENCE hdd_id_seq;
CREATE TABLE HardDiskDestination
(
    hdd_identifier INTEGER DEFAULT nextval('hdd_id_seq') NOT NULL,
    hdd_host VARCHAR(512) NOT NULL,
    hdd_path VARCHAR(512) NOT NULL,
    hdd_name VARCHAR(256) NOT NULL,
    hdd_material_package_uid CHAR(64),
    hdd_file_package_uid CHAR(64),
    hdd_tape_package_uid CHAR(64),
    hdd_browse_path VARCHAR(512),
    hdd_browse_name VARCHAR(256),
    hdd_pse_report_path VARCHAR(512),
    hdd_pse_report_name VARCHAR(256),
    hdd_pse_result_id INTEGER,
    hdd_size BIGINT,
    hdd_duration BIGINT,
    hdd_browse_size BIGINT,
    hdd_dest_id INTEGER NOT NULL,
    hdd_cache_id INTEGER,
    CONSTRAINT hdd_pse_result_fkey FOREIGN KEY (hdd_pse_result_id) REFERENCES PSEResult (pse_identifier),
    CONSTRAINT hdd_dest_fkey FOREIGN KEY (hdd_dest_id) REFERENCES Destination (des_identifier)
            ON DELETE CASCADE,
    CONSTRAINT hdd_cache_fkey FOREIGN KEY (hdd_cache_id) REFERENCES RecorderCache (rch_identifier)
            ON DELETE SET NULL,
    CONSTRAINT hdd_pkey PRIMARY KEY (hdd_identifier)
) WITHOUT OIDS;

CREATE INDEX hdd_dest_index ON HardDiskDestination (hdd_dest_id);
CREATE INDEX hdd_name_index ON HardDiskDestination (hdd_name);


-- DIGIBETA DESTINATION


CREATE SEQUENCE dbd_id_seq;
CREATE TABLE DigibetaDestination
(
    dbd_identifier INTEGER DEFAULT nextval('dbd_id_seq') NOT NULL,
    dbd_dest_id INTEGER NOT NULL,
    CONSTRAINT dbd_dest_fkey FOREIGN KEY (dbd_dest_id) REFERENCES Destination (des_identifier)
            ON DELETE CASCADE,
    CONSTRAINT dbd_pkey PRIMARY KEY (dbd_identifier)
) WITHOUT OIDS;

CREATE INDEX dbd_dest_index ON DigibetaDestination (dbd_dest_id);



-- LTO TRANSFER SESSION

CREATE TABLE LTOTransferSessionStatus
(
    lss_identifier INTEGER NOT NULL,
    lss_name VARCHAR(256) NOT NULL,
    CONSTRAINT lss_key UNIQUE (lss_name),
    CONSTRAINT lss_pkey PRIMARY KEY (lss_identifier)
) WITHOUT OIDS;


CREATE SEQUENCE lts_id_seq;
CREATE TABLE LTOTransferSession
(
    lts_identifier INTEGER DEFAULT nextval('lts_id_seq') NOT NULL,
    lts_recorder_id INTEGER NOT NULL,
    lts_creation TIMESTAMP WITHOUT TIME ZONE DEFAULT now() NOT NULL,
    lts_updated TIMESTAMP WITHOUT TIME ZONE DEFAULT now() NOT NULL,
    lts_status_id INTEGER NOT NULL,
    lts_abort_initiator_id INTEGER,
    lts_comments VARCHAR(256),
    lts_tape_device_vendor VARCHAR(256),
    lts_tape_device_model VARCHAR(256),
    lts_tape_device_revision VARCHAR(256),
    CONSTRAINT lts_status_fkey FOREIGN KEY (lts_status_id) REFERENCES LTOTransferSessionStatus (lss_identifier),
    CONSTRAINT lts_abort_initiator_fkey FOREIGN KEY (lts_abort_initiator_id) REFERENCES AbortInitiator (abi_identifier),
    CONSTRAINT lts_recorder_fkey FOREIGN KEY (lts_recorder_id) REFERENCES Recorder (rec_identifier)
            ON DELETE CASCADE,
    CONSTRAINT lts_pkey PRIMARY KEY (lts_identifier)
) WITHOUT OIDS;




-- LTO


CREATE SEQUENCE lto_id_seq;
CREATE TABLE LTO
(
    lto_identifier INTEGER DEFAULT nextval('lto_id_seq') NOT NULL,
    lto_spool_no VARCHAR(32) NOT NULL,
    lto_session_id INTEGER,
    CONSTRAINT lto_session_fkey FOREIGN KEY (lto_session_id) REFERENCES LTOTransferSession (lts_identifier)
            ON DELETE SET NULL,
    CONSTRAINT lto_pkey PRIMARY KEY (lto_identifier)
) WITHOUT OIDS;

CREATE INDEX lto_spool_no_index ON LTO (lto_spool_no);
CREATE INDEX lto_session_index ON LTO (lto_session_id);



-- LTO FILE


CREATE TABLE LTOFileTransferStatus
(
    lfs_identifier INTEGER NOT NULL,
    lfs_name VARCHAR(256) NOT NULL,
    CONSTRAINT lfs_key UNIQUE (lfs_name),
    CONSTRAINT lfs_pkey PRIMARY KEY (lfs_identifier)
) WITHOUT OIDS;


CREATE SEQUENCE ltf_id_seq;
CREATE TABLE LTOFile
(
    ltf_identifier INTEGER DEFAULT nextval('ltf_id_seq') NOT NULL,
    ltf_lto_id INTEGER NOT NULL,
    ltf_status_id INTEGER NOT NULL,
    ltf_name VARCHAR(256) NOT NULL,
    ltf_size BIGINT,
    ltf_real_duration BIGINT,
    ltf_format VARCHAR(6),
    ltf_prog_title VARCHAR(72),
    ltf_episode_title VARCHAR(144),
    ltf_tx_date DATE,
    ltf_mag_prefix VARCHAR(1),
    ltf_prog_no VARCHAR(8),
    ltf_prod_code VARCHAR(2),
    ltf_spool_status VARCHAR(1),
    ltf_stock_date DATE,
    ltf_spool_descr VARCHAR(29),
    ltf_memo VARCHAR(120),
    ltf_duration BIGINT,
    ltf_spool_no VARCHAR(14),
    ltf_acc_no VARCHAR(14),
    ltf_cat_detail VARCHAR(10),
    ltf_item_no INTEGER,
    ltf_recording_session_id INTEGER,
    ltf_hdd_host VARCHAR(512),
    ltf_hdd_path VARCHAR(512),
    ltf_hdd_name VARCHAR(256),
    ltf_hdd_material_package_uid CHAR(64),
    ltf_hdd_file_package_uid CHAR(64),
    ltf_hdd_tape_package_uid CHAR(64),
    ltf_source_spool_no VARCHAR(14),
    ltf_source_item_no INTEGER,
    ltf_source_mag_prefix VARCHAR(1),
    ltf_source_prog_no VARCHAR(8),
    ltf_source_prod_code VARCHAR(2),
    CONSTRAINT ltf_status_fkey FOREIGN KEY (ltf_status_id) REFERENCES LTOFileTransferStatus (lfs_identifier),
    CONSTRAINT ltf_lto_fkey FOREIGN KEY (ltf_lto_id) REFERENCES LTO (lto_identifier)
        ON DELETE CASCADE,
    CONSTRAINT ltf_recording_session_fkey FOREIGN KEY (ltf_recording_session_id) REFERENCES RecordingSession (rcs_identifier)
        ON DELETE SET NULL,
    CONSTRAINT ltf_pkey PRIMARY KEY (ltf_identifier)
) WITHOUT OIDS;

CREATE INDEX ltf_lto_index ON LTOFile (ltf_lto_id);
CREATE INDEX ltf_recording_session_index ON LTOFile (ltf_recording_session_id);
CREATE INDEX ltf_name_index ON LTOFile (ltf_name);
CREATE INDEX ltf_source_spool_no_index ON LTOFile (ltf_source_spool_no);




-- INFAX EXPORT

CREATE SEQUENCE ixe_export_number_seq;
CREATE SEQUENCE ixe_id_seq;
CREATE TABLE InfaxExport
(
    ixe_identifier INTEGER DEFAULT nextval('ixe_id_seq') NOT NULL,
    ixe_export_number INTEGER DEFAULT NULL,
    ixe_export_date TIMESTAMP WITHOUT TIME ZONE DEFAULT NULL,
    ixe_creation TIMESTAMP WITHOUT TIME ZONE DEFAULT now() NOT NULL,
    ixe_transfer_date TIMESTAMP WITHOUT TIME ZONE,
    ixe_d3_spool_no VARCHAR(14),
    ixe_item_no INTEGER,
    ixe_prog_title VARCHAR(72),
    ixe_episode_title VARCHAR(144),
    ixe_mag_prefix VARCHAR(1),
    ixe_prog_no VARCHAR(8),
    ixe_prod_code VARCHAR(2),
    ixe_mxf_name VARCHAR(256),
    ixe_digibeta_spool_no VARCHAR(14),
    ixe_browse_name VARCHAR(256),
    ixe_lto_spool_no VARCHAR(14),
    CONSTRAINT ixe_pkey PRIMARY KEY (ixe_identifier)
) WITHOUT OIDS;





------------------------------------
-- VIEWS
------------------------------------


        
------------------------------------
-- RULES
------------------------------------


------------------------------------
-- FUNCTIONS
------------------------------------

-- note: instance numbers returned by this function will start counting from 1

CREATE FUNCTION get_rec_instance(INTEGER) RETURNS INTEGER AS $$
    UPDATE Source 
        SET src_rec_instance = src_rec_instance + 1
        WHERE src_identifier = $1;
    SELECT src_rec_instance FROM Source WHERE src_identifier = $1;
$$ LANGUAGE SQL;



------------------------------------
-- DATA
------------------------------------

-- database version

INSERT INTO Version (ver_version) VALUES (1);


-- source types

INSERT INTO SourceType (srt_identifier, srt_name) VALUES (1, 'D3 Tape');


-- destination types

INSERT INTO DestinationType (dst_identifier, dst_name) VALUES (1, 'Hard disk');
INSERT INTO DestinationType (dst_identifier, dst_name) VALUES (2, 'Digibeta');


-- recording session status

INSERT INTO RecordingSessionStatus (rss_identifier, rss_name) VALUES (1, 'Started');
INSERT INTO RecordingSessionStatus (rss_identifier, rss_name) VALUES (2, 'Completed');
INSERT INTO RecordingSessionStatus (rss_identifier, rss_name) VALUES (3, 'Aborted');


-- pse result

INSERT INTO PSEResult (pse_identifier, pse_name) VALUES (1, 'Passed');
INSERT INTO PSEResult (pse_identifier, pse_name) VALUES (2, 'Failed');


-- abort initiator

INSERT INTO AbortInitiator (abi_identifier, abi_name) VALUES (1, 'User');
INSERT INTO AbortInitiator (abi_identifier, abi_name) VALUES (2, 'System');


-- lto transfer session status

INSERT INTO LTOTransferSessionStatus (lss_identifier, lss_name) VALUES (1, 'Started');
INSERT INTO LTOTransferSessionStatus (lss_identifier, lss_name) VALUES (2, 'Completed');
INSERT INTO LTOTransferSessionStatus (lss_identifier, lss_name) VALUES (3, 'Aborted');


-- lto file transfer status

INSERT INTO LTOFileTransferStatus (lfs_identifier, lfs_name) VALUES (1, 'Not started');
INSERT INTO LTOFileTransferStatus (lfs_identifier, lfs_name) VALUES (2, 'Started');
INSERT INTO LTOFileTransferStatus (lfs_identifier, lfs_name) VALUES (3, 'Completed');
INSERT INTO LTOFileTransferStatus (lfs_identifier, lfs_name) VALUES (4, 'Failed');



