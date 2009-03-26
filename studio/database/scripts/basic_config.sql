BEGIN;



--
-- recordinglocation
--

INSERT INTO recordinglocation VALUES (10, 'BBC');

SELECT setval('rlc_id_seq', max(rlc_identifier)) FROM recordinglocation;


--
-- sourceconfig
--

INSERT INTO sourceconfig VALUES (100, 'VT1', 2, NULL, 10);
INSERT INTO sourceconfig VALUES (101, 'VT2', 2, NULL, 10);
INSERT INTO sourceconfig VALUES (102, 'VT3', 2, NULL, 10);
INSERT INTO sourceconfig VALUES (103, 'VT4', 2, NULL, 10);

SELECT setval('scf_id_seq', max(scf_identifier)) FROM sourceconfig;


--
-- sourcetrackconfig
--

INSERT INTO sourcetrackconfig VALUES (200, 1, 1, 'V',  1, '(25,1)', 10800000, 100);
INSERT INTO sourcetrackconfig VALUES (201, 2, 1, 'A1', 2, '(25,1)', 10800000, 100);
INSERT INTO sourcetrackconfig VALUES (202, 3, 2, 'A2', 2, '(25,1)', 10800000, 100);
INSERT INTO sourcetrackconfig VALUES (203, 4, 3, 'A3', 2, '(25,1)', 10800000, 100);
INSERT INTO sourcetrackconfig VALUES (204, 5, 4, 'A4', 2, '(25,1)', 10800000, 100);
INSERT INTO sourcetrackconfig VALUES (210, 1, 1, 'V',  1, '(25,1)', 10800000, 101);
INSERT INTO sourcetrackconfig VALUES (211, 2, 1, 'A1', 2, '(25,1)', 10800000, 101);
INSERT INTO sourcetrackconfig VALUES (212, 3, 2, 'A2', 2, '(25,1)', 10800000, 101);
INSERT INTO sourcetrackconfig VALUES (213, 4, 3, 'A3', 2, '(25,1)', 10800000, 101);
INSERT INTO sourcetrackconfig VALUES (214, 5, 4, 'A4', 2, '(25,1)', 10800000, 101);
INSERT INTO sourcetrackconfig VALUES (220, 1, 1, 'V',  1, '(25,1)', 10800000, 102);
INSERT INTO sourcetrackconfig VALUES (221, 2, 1, 'A1', 2, '(25,1)', 10800000, 102);
INSERT INTO sourcetrackconfig VALUES (222, 3, 2, 'A2', 2, '(25,1)', 10800000, 102);
INSERT INTO sourcetrackconfig VALUES (223, 4, 3, 'A3', 2, '(25,1)', 10800000, 102);
INSERT INTO sourcetrackconfig VALUES (224, 5, 4, 'A4', 2, '(25,1)', 10800000, 102);
INSERT INTO sourcetrackconfig VALUES (230, 1, 1, 'V',  1, '(25,1)', 10800000, 103);
INSERT INTO sourcetrackconfig VALUES (231, 2, 1, 'A1', 2, '(25,1)', 10800000, 103);
INSERT INTO sourcetrackconfig VALUES (232, 3, 2, 'A2', 2, '(25,1)', 10800000, 103);
INSERT INTO sourcetrackconfig VALUES (233, 4, 3, 'A3', 2, '(25,1)', 10800000, 103);
INSERT INTO sourcetrackconfig VALUES (234, 5, 4, 'A4', 2, '(25,1)', 10800000, 103);

SELECT setval('sct_id_seq', max(sct_identifier)) FROM sourcetrackconfig;


--
-- recorder
--

INSERT INTO recorder VALUES (1, 'Ingex', NULL);
INSERT INTO recorder VALUES (2, 'Ingex-HD', NULL);

SELECT setval('rer_id_seq', max(rer_identifier)) FROM recorder;


--
-- recorderconfig
--

INSERT INTO recorderconfig VALUES (10, 'Ingex-Default', 1);
INSERT INTO recorderconfig VALUES (20, 'Ingex-HD-Default', 2);

SELECT setval('rec_id_seq', max(rec_identifier)) FROM recorderconfig;


--
-- recorder current config
--

UPDATE recorder SET rer_conf_id=10 WHERE rer_identifier = 1;
UPDATE recorder SET rer_conf_id=20 WHERE rer_identifier = 2;


--
-- recorderinputconfig
--

-- Ingex-Default
INSERT INTO recorderinputconfig VALUES (100, 1, 'Input 0', 10);
INSERT INTO recorderinputconfig VALUES (101, 2, 'Input 1', 10);
INSERT INTO recorderinputconfig VALUES (102, 3, 'Input 2', 10);
INSERT INTO recorderinputconfig VALUES (103, 4, 'Input 3', 10);

-- Ingex-HD-Default
INSERT INTO recorderinputconfig VALUES (200, 1, 'Input 0', 20);
INSERT INTO recorderinputconfig VALUES (201, 2, 'Input 1', 20);
INSERT INTO recorderinputconfig VALUES (202, 3, 'Input 2', 20);
INSERT INTO recorderinputconfig VALUES (203, 4, 'Input 3', 20);

SELECT setval('ric_id_seq', max(ric_identifier)) FROM recorderinputconfig;


--
-- recorderinputtrackconfig
--

-- Ingex-Default
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

-- Ingex-HD-Default
INSERT INTO recorderinputtrackconfig VALUES (200, 1, 1, 200, 100, 1);
INSERT INTO recorderinputtrackconfig VALUES (201, 2, 1, 200, 100, 2);
INSERT INTO recorderinputtrackconfig VALUES (202, 3, 2, 200, 100, 3);
INSERT INTO recorderinputtrackconfig VALUES (203, 4, 0, 200, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (204, 5, 0, 200, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (210, 1, 1, 201, 101, 1);
INSERT INTO recorderinputtrackconfig VALUES (211, 2, 1, 201, 101, 2);
INSERT INTO recorderinputtrackconfig VALUES (212, 3, 2, 201, 101, 3);
INSERT INTO recorderinputtrackconfig VALUES (213, 4, 0, 201, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (214, 5, 0, 201, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (220, 1, 1, 202, 102, 1);
INSERT INTO recorderinputtrackconfig VALUES (221, 2, 1, 202, 102, 2);
INSERT INTO recorderinputtrackconfig VALUES (222, 3, 2, 202, 102, 3);
INSERT INTO recorderinputtrackconfig VALUES (223, 4, 0, 202, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (224, 5, 0, 202, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (230, 1, 1, 203, 103, 1);
INSERT INTO recorderinputtrackconfig VALUES (231, 2, 1, 203, 103, 2);
INSERT INTO recorderinputtrackconfig VALUES (232, 3, 2, 203, 103, 3);
INSERT INTO recorderinputtrackconfig VALUES (233, 4, 0, 203, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (234, 5, 0, 203, NULL, NULL);

SELECT setval('rtc_id_seq', max(rtc_identifier)) FROM recorderinputtrackconfig;


--
-- multicameraclipdef
--

INSERT INTO multicameraclipdef VALUES (10, 'multi-camera clip');

SELECT setval('mcd_id_seq', max(mcd_identifier)) FROM multicameraclipdef;


--
-- multicameratrackdef
--

-- multi-camera tracks
INSERT INTO multicameratrackdef VALUES (1, 1, 1, 10);
INSERT INTO multicameratrackdef VALUES (2, 2, 1, 10);
INSERT INTO multicameratrackdef VALUES (3, 3, 2, 10);

SELECT setval('mct_id_seq', max(mct_identifier)) FROM multicameratrackdef;


--
-- multicameraselectordef
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
    VALUES (2, 'TIMECODE_MODE', '1', 1);
INSERT INTO defaultrecorderparameter (drp_identifier, drp_name, drp_value, drp_type)
    VALUES (3, 'COPY_COMMAND', '', 1);
    
INSERT INTO defaultrecorderparameter (drp_identifier, drp_name, drp_value, drp_type)
    VALUES (10, 'ENCODE1_RESOLUTION', '4', 1);
INSERT INTO defaultrecorderparameter (drp_identifier, drp_name, drp_value, drp_type)
    VALUES (11, 'ENCODE1_WRAPPING', '2', 1);
INSERT INTO defaultrecorderparameter (drp_identifier, drp_name, drp_value, drp_type)
    VALUES (12, 'ENCODE1_BITC', 'false', 1);
INSERT INTO defaultrecorderparameter (drp_identifier, drp_name, drp_value, drp_type)
    VALUES (13, 'ENCODE1_DIR', '/video/mxf_online/', 1);
INSERT INTO defaultrecorderparameter (drp_identifier, drp_name, drp_value, drp_type)
    VALUES (14, 'ENCODE1_DEST', '/store/mxf_online/', 1);
    
INSERT INTO defaultrecorderparameter (drp_identifier, drp_name, drp_value, drp_type)
    VALUES (20, 'ENCODE2_RESOLUTION', '8', 1);
INSERT INTO defaultrecorderparameter (drp_identifier, drp_name, drp_value, drp_type)
    VALUES (21, 'ENCODE2_WRAPPING', '2', 1);
INSERT INTO defaultrecorderparameter (drp_identifier, drp_name, drp_value, drp_type)
    VALUES (22, 'ENCODE2_BITC', 'false', 1);
INSERT INTO defaultrecorderparameter (drp_identifier, drp_name, drp_value, drp_type)
    VALUES (23, 'ENCODE2_DIR', '/video/mxf_offline/', 1);
INSERT INTO defaultrecorderparameter (drp_identifier, drp_name, drp_value, drp_type)
    VALUES (24, 'ENCODE2_DEST', '/store/mxf_offline/', 1);
    
INSERT INTO defaultrecorderparameter (drp_identifier, drp_name, drp_value, drp_type)
    VALUES (30, 'QUAD_RESOLUTION', '0', 1);
INSERT INTO defaultrecorderparameter (drp_identifier, drp_name, drp_value, drp_type)
    VALUES (31, 'QUAD_WRAPPING', '1', 1);
INSERT INTO defaultrecorderparameter (drp_identifier, drp_name, drp_value, drp_type)
    VALUES (32, 'QUAD_BITC', 'true', 1);
INSERT INTO defaultrecorderparameter (drp_identifier, drp_name, drp_value, drp_type)
    VALUES (33, 'QUAD_DIR', '/video/browse/', 1);
INSERT INTO defaultrecorderparameter (drp_identifier, drp_name, drp_value, drp_type)
    VALUES (34, 'QUAD_DEST', '', 1);

--
-- Ingex-Default
--

INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('MXF', 'true', 1, 10);

INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('IMAGE_ASPECT', '16/9', 1, 10);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('TIMECODE_MODE', '1', 1, 10);
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
    VALUES ('ENCODE1_DEST', '/store/mxf_online/', 1, 10);
    
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE2_RESOLUTION', '8', 1, 10);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE2_WRAPPING', '2', 1, 10);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE2_BITC', 'false', 1, 10);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE2_DIR', '/video/mxf_offline/', 1, 10);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE2_DEST', '/store/mxf_offline/', 1, 10);
    
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('QUAD_RESOLUTION', '0', 1, 10);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('QUAD_WRAPPING', '1', 1, 10);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('QUAD_BITC', 'true', 1, 10);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('QUAD_DIR', '/video/browse/', 1, 10);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('QUAD_DEST', '', 1, 10);

--
-- Ingex-HD-Default
--

INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('MXF', 'true', 1, 20);

INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('IMAGE_ASPECT', '16/9', 1, 20);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('TIMECODE_MODE', '1', 1, 20);
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
    VALUES ('ENCODE1_DEST', '/store/mxf_online/', 1, 20);
    
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE2_RESOLUTION', '8', 1, 20);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE2_WRAPPING', '2', 1, 20);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE2_BITC', 'false', 1, 20);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE2_DIR', '/video/mxf_offline/', 1, 20);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE2_DEST', '/store/mxf_offline/', 1, 20);
    
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('QUAD_RESOLUTION', '0', 1, 20);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('QUAD_WRAPPING', '1', 1, 20);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('QUAD_BITC', 'true', 1, 20);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('QUAD_DIR', '/video/browse/', 1, 20);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('QUAD_DEST', '', 1, 20);


END;

