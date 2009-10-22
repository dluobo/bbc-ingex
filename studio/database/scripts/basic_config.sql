BEGIN;



--
-- recordinglocation
--

INSERT INTO recordinglocation VALUES (10, 'Studio A');

SELECT setval('rlc_id_seq', max(rlc_identifier)) FROM recordinglocation;


--
-- sourceconfig
--

INSERT INTO sourceconfig VALUES (100, 'VT1', 2, NULL, 10);
INSERT INTO sourceconfig VALUES (101, 'VT2', 2, NULL, 10);
INSERT INTO sourceconfig VALUES (102, 'VT3', 2, NULL, 10);
INSERT INTO sourceconfig VALUES (103, 'VT4', 2, NULL, 10);
INSERT INTO sourceconfig VALUES (104, 'Mixer Out', 2, NULL, 10);

SELECT setval('scf_id_seq', max(scf_identifier)) FROM sourceconfig;


--
-- sourcetrackconfig
--

-- VT1
INSERT INTO sourcetrackconfig VALUES (200, 1, 1, 'V',  1, '(25,1)', 10800000, 100);
INSERT INTO sourcetrackconfig VALUES (201, 2, 1, 'A1', 2, '(25,1)', 10800000, 100);
INSERT INTO sourcetrackconfig VALUES (202, 3, 2, 'A2', 2, '(25,1)', 10800000, 100);
INSERT INTO sourcetrackconfig VALUES (203, 4, 3, 'A3', 2, '(25,1)', 10800000, 100);
INSERT INTO sourcetrackconfig VALUES (204, 5, 4, 'A4', 2, '(25,1)', 10800000, 100);
-- VT2
INSERT INTO sourcetrackconfig VALUES (210, 1, 1, 'V',  1, '(25,1)', 10800000, 101);
INSERT INTO sourcetrackconfig VALUES (211, 2, 1, 'A1', 2, '(25,1)', 10800000, 101);
INSERT INTO sourcetrackconfig VALUES (212, 3, 2, 'A2', 2, '(25,1)', 10800000, 101);
INSERT INTO sourcetrackconfig VALUES (213, 4, 3, 'A3', 2, '(25,1)', 10800000, 101);
INSERT INTO sourcetrackconfig VALUES (214, 5, 4, 'A4', 2, '(25,1)', 10800000, 101);
-- VT3
INSERT INTO sourcetrackconfig VALUES (220, 1, 1, 'V',  1, '(25,1)', 10800000, 102);
INSERT INTO sourcetrackconfig VALUES (221, 2, 1, 'A1', 2, '(25,1)', 10800000, 102);
INSERT INTO sourcetrackconfig VALUES (222, 3, 2, 'A2', 2, '(25,1)', 10800000, 102);
INSERT INTO sourcetrackconfig VALUES (223, 4, 3, 'A3', 2, '(25,1)', 10800000, 102);
INSERT INTO sourcetrackconfig VALUES (224, 5, 4, 'A4', 2, '(25,1)', 10800000, 102);
-- VT4
INSERT INTO sourcetrackconfig VALUES (230, 1, 1, 'V',  1, '(25,1)', 10800000, 103);
INSERT INTO sourcetrackconfig VALUES (231, 2, 1, 'A1', 2, '(25,1)', 10800000, 103);
INSERT INTO sourcetrackconfig VALUES (232, 3, 2, 'A2', 2, '(25,1)', 10800000, 103);
INSERT INTO sourcetrackconfig VALUES (233, 4, 3, 'A3', 2, '(25,1)', 10800000, 103);
INSERT INTO sourcetrackconfig VALUES (234, 5, 4, 'A4', 2, '(25,1)', 10800000, 103);
-- Mixer Out
INSERT INTO sourcetrackconfig VALUES (240, 1, 1, 'V',  1, '(25,1)', 10800000, 104);


SELECT setval('sct_id_seq', max(sct_identifier)) FROM sourcetrackconfig;


--
-- recorder
--

INSERT INTO recorder VALUES (1, 'Ingex', NULL);
INSERT INTO recorder VALUES (2, 'Ingex-HD', NULL);
INSERT INTO recorder VALUES (3, 'Ingex-Router', NULL);

SELECT setval('rer_id_seq', max(rer_identifier)) FROM recorder;


--
-- recorderconfig
--

INSERT INTO recorderconfig VALUES (10, 'Ingex-MXF', 1);
INSERT INTO recorderconfig VALUES (11, 'Ingex-Quicktime', 1);
INSERT INTO recorderconfig VALUES (20, 'Ingex-HD-MXF', 2);
INSERT INTO recorderconfig VALUES (21, 'Ingex-HD-Quicktime', 2);
INSERT INTO recorderconfig VALUES (30, 'Ingex-Router-StudioA', 3);

SELECT setval('rec_id_seq', max(rec_identifier)) FROM recorderconfig;


--
-- recorder current config
--

UPDATE recorder SET rer_conf_id=10 WHERE rer_identifier = 1;
UPDATE recorder SET rer_conf_id=20 WHERE rer_identifier = 2;
UPDATE recorder SET rer_conf_id=30 WHERE rer_identifier = 3;

--
-- recorderinputconfig
--

-- Ingex-MXF
INSERT INTO recorderinputconfig VALUES (100, 1, 'Input 0', 10);
INSERT INTO recorderinputconfig VALUES (101, 2, 'Input 1', 10);
INSERT INTO recorderinputconfig VALUES (102, 3, 'Input 2', 10);
INSERT INTO recorderinputconfig VALUES (103, 4, 'Input 3', 10);

-- Ingex-Quicktime
INSERT INTO recorderinputconfig VALUES (110, 1, 'Input 0', 11);
INSERT INTO recorderinputconfig VALUES (111, 2, 'Input 1', 11);
INSERT INTO recorderinputconfig VALUES (112, 3, 'Input 2', 11);
INSERT INTO recorderinputconfig VALUES (113, 4, 'Input 3', 11);

-- Ingex-HD-MXF
INSERT INTO recorderinputconfig VALUES (200, 1, 'Input 0', 20);
INSERT INTO recorderinputconfig VALUES (201, 2, 'Input 1', 20);
INSERT INTO recorderinputconfig VALUES (202, 3, 'Input 2', 20);
INSERT INTO recorderinputconfig VALUES (203, 4, 'Input 3', 20);

-- Ingex-HD-Quicktime
INSERT INTO recorderinputconfig VALUES (210, 1, 'Input 0', 21);
INSERT INTO recorderinputconfig VALUES (211, 2, 'Input 1', 21);
INSERT INTO recorderinputconfig VALUES (212, 3, 'Input 2', 21);
INSERT INTO recorderinputconfig VALUES (213, 4, 'Input 3', 21);

-- Ingex-Router-StudioA
INSERT INTO recorderinputconfig VALUES (300, 1, 'Input 0', 30);


SELECT setval('ric_id_seq', max(ric_identifier)) FROM recorderinputconfig;


--
-- recorderinputtrackconfig
-- rtc_identifier | rtc_index | rtc_track_number | rtc_recorder_input_id | rtc_source_id | rtc_source_track_id
--

-- Ingex-MXF
INSERT INTO recorderinputtrackconfig VALUES (100, 1, 1, 100, 100, 1);
INSERT INTO recorderinputtrackconfig VALUES (101, 2, 1, 100, 100, 2);
INSERT INTO recorderinputtrackconfig VALUES (102, 3, 2, 100, 100, 3);
INSERT INTO recorderinputtrackconfig VALUES (103, 4, 0, 100, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (104, 5, 0, 100, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (110, 1, 1, 101, 101, 1);
INSERT INTO recorderinputtrackconfig VALUES (111, 2, 1, 101, 101, 2);
INSERT INTO recorderinputtrackconfig VALUES (112, 3, 2, 101, 101, 3);
INSERT INTO recorderinputtrackconfig VALUES (113, 4, 0, 101, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (114, 5, 0, 101, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (120, 1, 1, 102, 102, 1);
INSERT INTO recorderinputtrackconfig VALUES (121, 2, 1, 102, 102, 2);
INSERT INTO recorderinputtrackconfig VALUES (122, 3, 2, 102, 102, 3);
INSERT INTO recorderinputtrackconfig VALUES (123, 4, 0, 102, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (124, 5, 0, 102, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (130, 1, 1, 103, 103, 1);
INSERT INTO recorderinputtrackconfig VALUES (131, 2, 1, 103, 103, 2);
INSERT INTO recorderinputtrackconfig VALUES (132, 3, 2, 103, 103, 3);
INSERT INTO recorderinputtrackconfig VALUES (133, 4, 0, 103, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (134, 5, 0, 103, NULL, NULL);

-- Ingex-Quicktime
INSERT INTO recorderinputtrackconfig VALUES (200, 1, 1, 110, 100, 1);
INSERT INTO recorderinputtrackconfig VALUES (201, 2, 1, 110, 100, 2);
INSERT INTO recorderinputtrackconfig VALUES (202, 3, 2, 110, 100, 3);
INSERT INTO recorderinputtrackconfig VALUES (203, 4, 0, 110, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (204, 5, 0, 110, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (210, 1, 1, 111, 101, 1);
INSERT INTO recorderinputtrackconfig VALUES (211, 2, 1, 111, 101, 2);
INSERT INTO recorderinputtrackconfig VALUES (212, 3, 2, 111, 101, 3);
INSERT INTO recorderinputtrackconfig VALUES (213, 4, 0, 111, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (214, 5, 0, 111, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (220, 1, 1, 112, 102, 1);
INSERT INTO recorderinputtrackconfig VALUES (221, 2, 1, 112, 102, 2);
INSERT INTO recorderinputtrackconfig VALUES (222, 3, 2, 112, 102, 3);
INSERT INTO recorderinputtrackconfig VALUES (223, 4, 0, 112, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (224, 5, 0, 112, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (230, 1, 1, 113, 103, 1);
INSERT INTO recorderinputtrackconfig VALUES (231, 2, 1, 113, 103, 2);
INSERT INTO recorderinputtrackconfig VALUES (232, 3, 2, 113, 103, 3);
INSERT INTO recorderinputtrackconfig VALUES (233, 4, 0, 113, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (234, 5, 0, 113, NULL, NULL);

-- Ingex-HD-MXF
INSERT INTO recorderinputtrackconfig VALUES (400, 1, 1, 200, 100, 1);
INSERT INTO recorderinputtrackconfig VALUES (401, 2, 1, 200, 100, 2);
INSERT INTO recorderinputtrackconfig VALUES (402, 3, 2, 200, 100, 3);
INSERT INTO recorderinputtrackconfig VALUES (403, 4, 0, 200, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (404, 5, 0, 200, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (410, 1, 1, 201, 101, 1);
INSERT INTO recorderinputtrackconfig VALUES (411, 2, 1, 201, 101, 2);
INSERT INTO recorderinputtrackconfig VALUES (412, 3, 2, 201, 101, 3);
INSERT INTO recorderinputtrackconfig VALUES (413, 4, 0, 201, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (414, 5, 0, 201, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (420, 1, 1, 202, 102, 1);
INSERT INTO recorderinputtrackconfig VALUES (421, 2, 1, 202, 102, 2);
INSERT INTO recorderinputtrackconfig VALUES (422, 3, 2, 202, 102, 3);
INSERT INTO recorderinputtrackconfig VALUES (423, 4, 0, 202, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (424, 5, 0, 202, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (430, 1, 1, 203, 103, 1);
INSERT INTO recorderinputtrackconfig VALUES (431, 2, 1, 203, 103, 2);
INSERT INTO recorderinputtrackconfig VALUES (432, 3, 2, 203, 103, 3);
INSERT INTO recorderinputtrackconfig VALUES (433, 4, 0, 203, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (434, 5, 0, 203, NULL, NULL);

-- Ingex-HD-Quicktime
INSERT INTO recorderinputtrackconfig VALUES (500, 1, 1, 210, 100, 1);
INSERT INTO recorderinputtrackconfig VALUES (501, 2, 1, 210, 100, 2);
INSERT INTO recorderinputtrackconfig VALUES (502, 3, 2, 210, 100, 3);
INSERT INTO recorderinputtrackconfig VALUES (503, 4, 0, 210, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (504, 5, 0, 210, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (510, 1, 1, 211, 101, 1);
INSERT INTO recorderinputtrackconfig VALUES (511, 2, 1, 211, 101, 2);
INSERT INTO recorderinputtrackconfig VALUES (512, 3, 2, 211, 101, 3);
INSERT INTO recorderinputtrackconfig VALUES (513, 4, 0, 211, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (514, 5, 0, 211, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (520, 1, 1, 212, 102, 1);
INSERT INTO recorderinputtrackconfig VALUES (521, 2, 1, 212, 102, 2);
INSERT INTO recorderinputtrackconfig VALUES (522, 3, 2, 212, 102, 3);
INSERT INTO recorderinputtrackconfig VALUES (523, 4, 0, 212, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (524, 5, 0, 212, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (530, 1, 1, 213, 103, 1);
INSERT INTO recorderinputtrackconfig VALUES (531, 2, 1, 213, 103, 2);
INSERT INTO recorderinputtrackconfig VALUES (532, 3, 2, 213, 103, 3);
INSERT INTO recorderinputtrackconfig VALUES (533, 4, 0, 213, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (534, 5, 0, 213, NULL, NULL);

-- Ingex-Router-StudioA
INSERT INTO recorderinputtrackconfig VALUES (600, 1, 1, 300, 104, 1);


SELECT setval('rtc_id_seq', max(rtc_identifier)) FROM recorderinputtrackconfig;


--
-- multicameraclipdef
--

INSERT INTO multicameraclipdef VALUES (10, 'StudioA multicam');

SELECT setval('mcd_id_seq', max(mcd_identifier)) FROM multicameraclipdef;


--
-- multicameratrackdef
--

-- multi-camera tracks
-- mct_identifier | mct_index | mct_track_number | mct_multi_camera_def_id
INSERT INTO multicameratrackdef VALUES (1, 1, 1, 10);
INSERT INTO multicameratrackdef VALUES (2, 2, 1, 10);
INSERT INTO multicameratrackdef VALUES (3, 3, 2, 10);

SELECT setval('mct_id_seq', max(mct_identifier)) FROM multicameratrackdef;


--
-- multicameraselectordef
-- mcs_identifier | mcs_index | mcs_multi_camera_track_def_id | mcs_source_id | mcs_source_track_id
--

INSERT INTO multicameraselectordef VALUES (100, 1, 1, 100, 1);
INSERT INTO multicameraselectordef VALUES (101, 2, 1, 101, 1);
INSERT INTO multicameraselectordef VALUES (102, 3, 1, 102, 1);
INSERT INTO multicameraselectordef VALUES (103, 4, 1, 103, 1);
INSERT INTO multicameraselectordef VALUES (104, 1, 2, 100, 2);
INSERT INTO multicameraselectordef VALUES (105, 1, 3, 100, 3);

SELECT setval('mcs_id_seq', max(mcs_identifier)) FROM multicameraselectordef;

--
-- defaultrecorderparameters
--

INSERT INTO defaultrecorderparameter (drp_identifier, drp_name, drp_value, drp_type)
    VALUES (1, 'IMAGE_ASPECT', '16/9', 1);
INSERT INTO defaultrecorderparameter (drp_identifier, drp_name, drp_value, drp_type)
    VALUES (2, 'COPY_COMMAND', '', 1);
    
INSERT INTO defaultrecorderparameter (drp_identifier, drp_name, drp_value, drp_type)
    VALUES (10, 'ENCODE1_RESOLUTION', '4', 1);
INSERT INTO defaultrecorderparameter (drp_identifier, drp_name, drp_value, drp_type)
    VALUES (11, 'ENCODE1_WRAPPING', '2', 1);
INSERT INTO defaultrecorderparameter (drp_identifier, drp_name, drp_value, drp_type)
    VALUES (12, 'ENCODE1_BITC', 'false', 1);
INSERT INTO defaultrecorderparameter (drp_identifier, drp_name, drp_value, drp_type)
    VALUES (13, 'ENCODE1_DIR', '/video/mxf_online/', 1);
INSERT INTO defaultrecorderparameter (drp_identifier, drp_name, drp_value, drp_type)
    VALUES (14, 'ENCODE1_COPY_DEST', '/store/mxf_online/', 1);
INSERT INTO defaultrecorderparameter (drp_identifier, drp_name, drp_value, drp_type)
    VALUES (15, 'ENCODE1_COPY_PRIORITY', '3', 1);
    
INSERT INTO defaultrecorderparameter (drp_identifier, drp_name, drp_value, drp_type)
    VALUES (20, 'ENCODE2_RESOLUTION', '8', 1);
INSERT INTO defaultrecorderparameter (drp_identifier, drp_name, drp_value, drp_type)
    VALUES (21, 'ENCODE2_WRAPPING', '2', 1);
INSERT INTO defaultrecorderparameter (drp_identifier, drp_name, drp_value, drp_type)
    VALUES (22, 'ENCODE2_BITC', 'false', 1);
INSERT INTO defaultrecorderparameter (drp_identifier, drp_name, drp_value, drp_type)
    VALUES (23, 'ENCODE2_DIR', '/video/mxf_offline/', 1);
INSERT INTO defaultrecorderparameter (drp_identifier, drp_name, drp_value, drp_type)
    VALUES (24, 'ENCODE2_COPY_DEST', '/store/mxf_offline/', 1);
INSERT INTO defaultrecorderparameter (drp_identifier, drp_name, drp_value, drp_type)
    VALUES (25, 'ENCODE2_COPY_PRIORITY', '2', 1);
    
INSERT INTO defaultrecorderparameter (drp_identifier, drp_name, drp_value, drp_type)
    VALUES (30, 'ENCODE3_RESOLUTION', '21', 1);
INSERT INTO defaultrecorderparameter (drp_identifier, drp_name, drp_value, drp_type)
    VALUES (31, 'ENCODE3_WRAPPING', '3', 1);
INSERT INTO defaultrecorderparameter (drp_identifier, drp_name, drp_value, drp_type)
    VALUES (32, 'ENCODE3_BITC', 'false', 1);
INSERT INTO defaultrecorderparameter (drp_identifier, drp_name, drp_value, drp_type)
    VALUES (33, 'ENCODE3_DIR', '/video/browse/', 1);
INSERT INTO defaultrecorderparameter (drp_identifier, drp_name, drp_value, drp_type)
    VALUES (34, 'ENCODE3_COPY_DEST', '/store/browse/', 1);
INSERT INTO defaultrecorderparameter (drp_identifier, drp_name, drp_value, drp_type)
    VALUES (35, 'ENCODE3_COPY_PRIORITY', '1', 1);
    
INSERT INTO defaultrecorderparameter (drp_identifier, drp_name, drp_value, drp_type)
    VALUES (40, 'QUAD_RESOLUTION', '0', 1);
INSERT INTO defaultrecorderparameter (drp_identifier, drp_name, drp_value, drp_type)
    VALUES (41, 'QUAD_WRAPPING', '1', 1);
INSERT INTO defaultrecorderparameter (drp_identifier, drp_name, drp_value, drp_type)
    VALUES (42, 'QUAD_BITC', 'true', 1);
INSERT INTO defaultrecorderparameter (drp_identifier, drp_name, drp_value, drp_type)
    VALUES (43, 'QUAD_DIR', '/video/browse/', 1);
INSERT INTO defaultrecorderparameter (drp_identifier, drp_name, drp_value, drp_type)
    VALUES (44, 'QUAD_COPY_DEST', '', 1);
INSERT INTO defaultrecorderparameter (drp_identifier, drp_name, drp_value, drp_type)
    VALUES (45, 'QUAD_COPY_PRIORITY', '3', 1);

--
-- Ingex-MXF
--

INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('IMAGE_ASPECT', '16/9', 1, 10);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('COPY_COMMAND', '', 1, 10);
    
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE1_RESOLUTION', '4', 1, 10);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE1_WRAPPING', '2', 1, 10);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE1_BITC', 'false', 1, 10);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE1_DIR', '/video/mxf_online/', 1, 10);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE1_COPY_DEST', '/store/mxf_online/', 1, 10);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE1_COPY_PRIORITY', '3', 1, 10);
    
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE2_RESOLUTION', '8', 1, 10);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE2_WRAPPING', '2', 1, 10);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE2_BITC', 'false', 1, 10);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE2_DIR', '/video/mxf_offline/', 1, 10);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE2_COPY_DEST', '/store/mxf_offline/', 1, 10);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE2_COPY_PRIORITY', '2', 1, 10);
    
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE3_RESOLUTION', '21', 1, 10);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE3_WRAPPING', '3', 1, 10);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE3_BITC', 'false', 1, 10);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE3_DIR', '/video/browse/', 1, 10);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE3_COPY_DEST', '/store/browse/', 1, 10);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE3_COPY_PRIORITY', '1', 1, 10);
    
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('QUAD_RESOLUTION', '0', 1, 10);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('QUAD_WRAPPING', '1', 1, 10);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('QUAD_BITC', 'true', 1, 10);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('QUAD_DIR', '/video/browse/', 1, 10);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('QUAD_COPY_DEST', '', 1, 10);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('QUAD_COPY_PRIORITY', '3', 1, 10);

--
-- Ingex-Quicktime
--

INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('IMAGE_ASPECT', '16/9', 1, 11);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('COPY_COMMAND', '', 1, 11);
    
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE1_RESOLUTION', '3', 1, 11);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE1_WRAPPING', '3', 1, 11);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE1_BITC', 'false', 1, 11);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE1_DIR', '/video/dv/', 1, 11);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE1_COPY_DEST', '/store/dv/', 1, 11);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE1_COPY_PRIORITY', '1', 1, 11);
    
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE2_RESOLUTION', '0', 1, 11);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE2_WRAPPING', '3', 1, 11);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE2_BITC', 'false', 1, 11);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE2_DIR', '/video/dv/', 1, 11);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE2_COPY_DEST', '/store/dv/', 1, 11);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE2_COPY_PRIORITY', '2', 1, 11);
    
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('QUAD_RESOLUTION', '0', 1, 11);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('QUAD_WRAPPING', '1', 1, 11);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('QUAD_BITC', 'true', 1, 11);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('QUAD_DIR', '/video/browse/', 1, 11);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('QUAD_COPY_DEST', '', 1, 11);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('QUAD_COPY_PRIORITY', '3', 1, 11);

--
-- Ingex-HD-MXF
--

INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('IMAGE_ASPECT', '16/9', 1, 20);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('COPY_COMMAND', '', 1, 20);
    
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE1_RESOLUTION', '17', 1, 20);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE1_WRAPPING', '2', 1, 20);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE1_BITC', 'false', 1, 20);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE1_DIR', '/video/mxf_online/', 1, 20);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE1_COPY_DEST', '/store/mxf_online/', 1, 20);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE1_COPY_PRIORITY', '3', 1, 20);
    
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE2_RESOLUTION', '8', 1, 20);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE2_WRAPPING', '2', 1, 20);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE2_BITC', 'false', 1, 20);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE2_DIR', '/video/mxf_offline/', 1, 20);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE2_COPY_DEST', '/store/mxf_offline/', 1, 20);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE2_COPY_PRIORITY', '2', 1, 20);
    
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE3_RESOLUTION', '21', 1, 20);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE3_WRAPPING', '3', 1, 20);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE3_BITC', 'false', 1, 20);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE3_DIR', '/video/browse/', 1, 20);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE3_COPY_DEST', '/store/browse/', 1, 20);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE3_COPY_PRIORITY', '1', 1, 20);
    
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('QUAD_RESOLUTION', '0', 1, 20);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('QUAD_WRAPPING', '1', 1, 20);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('QUAD_BITC', 'true', 1, 20);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('QUAD_DIR', '/video/browse/', 1, 20);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('QUAD_COPY_DEST', '', 1, 20);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('QUAD_COPY_PRIORITY', '3', 1, 20);

--
-- Ingex-HD-Quicktime
--

INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('IMAGE_ASPECT', '16/9', 1, 21);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('COPY_COMMAND', '', 1, 21);
    
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE1_RESOLUTION', '19', 1, 21);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE1_WRAPPING', '3', 1, 21);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE1_BITC', 'false', 1, 21);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE1_DIR', '/video/dv/', 1, 21);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE1_COPY_DEST', '/store/dv/', 1, 21);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE1_COPY_PRIORITY', '1', 1, 21);
    
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE2_RESOLUTION', '0', 1, 21);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE2_WRAPPING', '3', 1, 21);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE2_BITC', 'false', 1, 21);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE2_DIR', '/video/dv/', 1, 21);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE2_COPY_DEST', '/store/dv/', 1, 21);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE2_COPY_PRIORITY', '2', 1, 21);
    
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('QUAD_RESOLUTION', '0', 1, 21);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('QUAD_WRAPPING', '1', 1, 21);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('QUAD_BITC', 'true', 1, 21);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('QUAD_DIR', '/video/browse/', 1, 21);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('QUAD_COPY_DEST', '', 1, 21);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('QUAD_COPY_PRIORITY', '3', 1, 21);



END;

