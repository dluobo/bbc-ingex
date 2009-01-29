BEGIN;



--
-- recordinglocation
--

INSERT INTO recordinglocation VALUES (10, 'Lot');
INSERT INTO recordinglocation VALUES (11, 'Stage');
INSERT INTO recordinglocation VALUES (12, 'Studio A');

SELECT setval('rlc_id_seq', max(rlc_identifier)) FROM recordinglocation;


--
-- sourceconfig
--

INSERT INTO sourceconfig VALUES (100, 'Lot - VT1', 2, NULL, 10);
INSERT INTO sourceconfig VALUES (101, 'Lot - VT2', 2, NULL, 10);
INSERT INTO sourceconfig VALUES (102, 'Lot - VT3', 2, NULL, 10);
INSERT INTO sourceconfig VALUES (103, 'Lot - VT4', 2, NULL, 10);
INSERT INTO sourceconfig VALUES (104, 'Lot - Mixer Out', 2, NULL, 10);
INSERT INTO sourceconfig VALUES (110, 'Stage - VT1', 2, NULL, 11);
INSERT INTO sourceconfig VALUES (111, 'Stage - VT2', 2, NULL, 11);
INSERT INTO sourceconfig VALUES (112, 'Stage - VT3', 2, NULL, 11);
INSERT INTO sourceconfig VALUES (113, 'Stage - VT4', 2, NULL, 11);
INSERT INTO sourceconfig VALUES (114, 'Stage - Mixer Out', 2, NULL, 11);
INSERT INTO sourceconfig VALUES (120, 'StudioA - VT1', 2, NULL, 12);
INSERT INTO sourceconfig VALUES (121, 'StudioA - VT2', 2, NULL, 12);
INSERT INTO sourceconfig VALUES (122, 'StudioA - VT3', 2, NULL, 12);
INSERT INTO sourceconfig VALUES (123, 'StudioA - VT4', 2, NULL, 12);
INSERT INTO sourceconfig VALUES (124, 'StudioA - Mixer Out', 2, NULL, 12);

SELECT setval('scf_id_seq', max(scf_identifier)) FROM sourceconfig;


--
-- sourcetrackconfig
--

-- Lot
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
-- Mixer Out
INSERT INTO sourcetrackconfig VALUES (240, 1, 1, 'V',  1, '(25,1)', 10800000, 104);

-- Stage
INSERT INTO sourcetrackconfig VALUES (300, 1, 1, 'V',  1, '(25,1)', 10800000, 110);
INSERT INTO sourcetrackconfig VALUES (301, 2, 1, 'A1', 2, '(25,1)', 10800000, 110);
INSERT INTO sourcetrackconfig VALUES (302, 3, 2, 'A2', 2, '(25,1)', 10800000, 110);
INSERT INTO sourcetrackconfig VALUES (303, 4, 3, 'A3', 2, '(25,1)', 10800000, 110);
INSERT INTO sourcetrackconfig VALUES (304, 5, 4, 'A4', 2, '(25,1)', 10800000, 110);
INSERT INTO sourcetrackconfig VALUES (310, 1, 1, 'V',  1, '(25,1)', 10800000, 111);
INSERT INTO sourcetrackconfig VALUES (311, 2, 1, 'A1', 2, '(25,1)', 10800000, 111);
INSERT INTO sourcetrackconfig VALUES (312, 3, 2, 'A2', 2, '(25,1)', 10800000, 111);
INSERT INTO sourcetrackconfig VALUES (313, 4, 3, 'A3', 2, '(25,1)', 10800000, 111);
INSERT INTO sourcetrackconfig VALUES (314, 5, 4, 'A4', 2, '(25,1)', 10800000, 111);
INSERT INTO sourcetrackconfig VALUES (320, 1, 1, 'V',  1, '(25,1)', 10800000, 112);
INSERT INTO sourcetrackconfig VALUES (321, 2, 1, 'A1', 2, '(25,1)', 10800000, 112);
INSERT INTO sourcetrackconfig VALUES (322, 3, 2, 'A2', 2, '(25,1)', 10800000, 112);
INSERT INTO sourcetrackconfig VALUES (323, 4, 3, 'A3', 2, '(25,1)', 10800000, 112);
INSERT INTO sourcetrackconfig VALUES (324, 5, 4, 'A4', 2, '(25,1)', 10800000, 112);
INSERT INTO sourcetrackconfig VALUES (330, 1, 1, 'V',  1, '(25,1)', 10800000, 113);
INSERT INTO sourcetrackconfig VALUES (331, 2, 1, 'A1', 2, '(25,1)', 10800000, 113);
INSERT INTO sourcetrackconfig VALUES (332, 3, 2, 'A2', 2, '(25,1)', 10800000, 113);
INSERT INTO sourcetrackconfig VALUES (333, 4, 3, 'A3', 2, '(25,1)', 10800000, 113);
INSERT INTO sourcetrackconfig VALUES (334, 5, 4, 'A4', 2, '(25,1)', 10800000, 113);
-- Mixer Out
INSERT INTO sourcetrackconfig VALUES (340, 1, 1, 'V',  1, '(25,1)', 10800000, 114);

-- Studio A
INSERT INTO sourcetrackconfig VALUES (400, 1, 1, 'V',  1, '(25,1)', 10800000, 120);
INSERT INTO sourcetrackconfig VALUES (401, 2, 1, 'A1', 2, '(25,1)', 10800000, 120);
INSERT INTO sourcetrackconfig VALUES (402, 3, 2, 'A2', 2, '(25,1)', 10800000, 120);
INSERT INTO sourcetrackconfig VALUES (403, 4, 3, 'A3', 2, '(25,1)', 10800000, 120);
INSERT INTO sourcetrackconfig VALUES (404, 5, 4, 'A4', 2, '(25,1)', 10800000, 120);
INSERT INTO sourcetrackconfig VALUES (410, 1, 1, 'V',  1, '(25,1)', 10800000, 121);
INSERT INTO sourcetrackconfig VALUES (411, 2, 1, 'A1', 2, '(25,1)', 10800000, 121);
INSERT INTO sourcetrackconfig VALUES (412, 3, 2, 'A2', 2, '(25,1)', 10800000, 121);
INSERT INTO sourcetrackconfig VALUES (413, 4, 3, 'A3', 2, '(25,1)', 10800000, 121);
INSERT INTO sourcetrackconfig VALUES (414, 5, 4, 'A4', 2, '(25,1)', 10800000, 121);
INSERT INTO sourcetrackconfig VALUES (420, 1, 1, 'V',  1, '(25,1)', 10800000, 122);
INSERT INTO sourcetrackconfig VALUES (421, 2, 1, 'A1', 2, '(25,1)', 10800000, 122);
INSERT INTO sourcetrackconfig VALUES (422, 3, 2, 'A2', 2, '(25,1)', 10800000, 122);
INSERT INTO sourcetrackconfig VALUES (423, 4, 3, 'A3', 2, '(25,1)', 10800000, 122);
INSERT INTO sourcetrackconfig VALUES (424, 5, 4, 'A4', 2, '(25,1)', 10800000, 122);
INSERT INTO sourcetrackconfig VALUES (430, 1, 1, 'V',  1, '(25,1)', 10800000, 123);
INSERT INTO sourcetrackconfig VALUES (431, 2, 1, 'A1', 2, '(25,1)', 10800000, 123);
INSERT INTO sourcetrackconfig VALUES (432, 3, 2, 'A2', 2, '(25,1)', 10800000, 123);
INSERT INTO sourcetrackconfig VALUES (433, 4, 3, 'A3', 2, '(25,1)', 10800000, 123);
INSERT INTO sourcetrackconfig VALUES (434, 5, 4, 'A4', 2, '(25,1)', 10800000, 123);
-- Mixer Out
INSERT INTO sourcetrackconfig VALUES (440, 1, 1, 'V',  1, '(25,1)', 10800000, 124);

SELECT setval('sct_id_seq', max(sct_identifier)) FROM sourcetrackconfig;


--
-- recorder
--
-- rer_identifier | rer_name | rer_conf_id

INSERT INTO recorder VALUES (1, 'Ingex-Lot', NULL);
INSERT INTO recorder VALUES (2, 'Ingex-Stage', NULL);
INSERT INTO recorder VALUES (3, 'Ingex-StudioA', NULL);
INSERT INTO recorder VALUES (4, 'Router-Lot', NULL);
INSERT INTO recorder VALUES (5, 'Router-Stage', NULL);
INSERT INTO recorder VALUES (6, 'Router-StudioA', NULL);

SELECT setval('rer_id_seq', max(rer_identifier)) FROM recorder;


--
-- recorderconfig
--
-- rec_identifier | rec_name | rec_recorder_id

INSERT INTO recorderconfig VALUES (11, 'Lot', 1);
INSERT INTO recorderconfig VALUES (12, 'Stage', 2);
INSERT INTO recorderconfig VALUES (13, 'Studio A', 3);
INSERT INTO recorderconfig VALUES (14, 'Lot Router', 4);
INSERT INTO recorderconfig VALUES (15, 'Stage Router', 5);
INSERT INTO recorderconfig VALUES (16, 'Studio A Router', 6);

SELECT setval('rec_id_seq', max(rec_identifier)) FROM recorderconfig;

-- set current recorder config for each recorder
UPDATE recorder SET rer_conf_id=11 WHERE rer_identifier = 1;
UPDATE recorder SET rer_conf_id=12 WHERE rer_identifier = 2;
UPDATE recorder SET rer_conf_id=13 WHERE rer_identifier = 3;
UPDATE recorder SET rer_conf_id=14 WHERE rer_identifier = 4;
UPDATE recorder SET rer_conf_id=15 WHERE rer_identifier = 5;
UPDATE recorder SET rer_conf_id=16 WHERE rer_identifier = 6;


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
-- recorderparameters
--

-- Lot
-- NB. Lot is online only
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('IMAGE_ASPECT', '16/9', 1, 11);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('TIMECODE_MODE', '1', 1, 11);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('COPY_COMMAND', '/home/ingex/ap-workspace/ingex/studio/processing/media_transfer/xferclient.pl', 1, 11);
    
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE1_RESOLUTION', '4', 1, 11);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE1_WRAPPING', '2', 1, 11);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE1_BITC', 'false', 1, 11);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE1_DIR', '/video/mxf_online/', 1, 11);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE1_DEST', '/store/mxf_online_lot/', 1, 11);
    
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE2_RESOLUTION', '0', 1, 11);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE2_WRAPPING', '2', 1, 11);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE2_BITC', 'false', 1, 11);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE2_DIR', '/video/mxf_online/', 1, 11);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE2_DEST', '/store/mxf_online/', 1, 11);
    
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('QUAD_RESOLUTION', '0', 1, 11);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('QUAD_WRAPPING', '1', 1, 11);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('QUAD_BITC', 'true', 1, 11);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('QUAD_DIR', '/video/browse/', 1, 11);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('QUAD_DEST', '', 1, 11);

-- Stage
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('IMAGE_ASPECT', '16/9', 1, 12);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('TIMECODE_MODE', '1', 1, 12);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('COPY_COMMAND', '/home/ingex/ap-workspace/ingex/studio/processing/media_transfer/xferclient.pl', 1, 12);
    
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE1_RESOLUTION', '4', 1, 12);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE1_WRAPPING', '2', 1, 12);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE1_BITC', 'false', 1, 12);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE1_DIR', '/video/mxf_online/', 1, 12);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE1_DEST', '/store/mxf_online/', 1, 12);
    
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE2_RESOLUTION', '8', 1, 12);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE2_WRAPPING', '2', 1, 12);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE2_BITC', 'false', 1, 12);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE2_DIR', '/video/mxf_offline/', 1, 12);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE2_DEST', '/store/mxf_offline/', 1, 12);
    
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('QUAD_RESOLUTION', '0', 1, 12);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('QUAD_WRAPPING', '1', 1, 12);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('QUAD_BITC', 'true', 1, 12);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('QUAD_DIR', '/video/browse/', 1, 12);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('QUAD_DEST', '', 1, 12);


-- Studio A
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('IMAGE_ASPECT', '16/9', 1, 13);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('TIMECODE_MODE', '1', 1, 13);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('COPY_COMMAND', '/home/ingex/ap-workspace/ingex/studio/processing/media_transfer/xferclient.pl', 1, 13);
    
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE1_RESOLUTION', '4', 1, 13);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE1_WRAPPING', '2', 1, 13);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE1_BITC', 'false', 1, 13);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE1_DIR', '/video/mxf_online/', 1, 13);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE1_DEST', '/store/mxf_online/', 1, 13);
    
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE2_RESOLUTION', '8', 1, 13);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE2_WRAPPING', '2', 1, 13);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE2_BITC', 'false', 1, 13);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE2_DIR', '/video/mxf_offline/', 1, 13);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE2_DEST', '/store/mxf_offline/', 1, 13);
    
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('QUAD_RESOLUTION', '0', 1, 13);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('QUAD_WRAPPING', '1', 1, 13);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('QUAD_BITC', 'true', 1, 13);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('QUAD_DIR', '/video/browse/', 1, 13);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('QUAD_DEST', '', 1, 13);

-- Router recorders
-- Lot Router
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('IMAGE_ASPECT', '16/9', 1, 14);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('TIMECODE_MODE', '1', 1, 14);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('COPY_COMMAND', '', 1, 14);
    
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE1_RESOLUTION', '0', 1, 14);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE1_WRAPPING', '2', 1, 14);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE1_BITC', 'false', 1, 14);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE1_DIR', '/video/mxf_online/', 1, 14);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE1_DEST', '/store/mxf_online_lot/', 1, 14);
    
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE2_RESOLUTION', '0', 1, 14);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE2_WRAPPING', '2', 1, 14);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE2_BITC', 'false', 1, 14);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE2_DIR', '/video/mxf_online/', 1, 14);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE2_DEST', '/store/mxf_online/', 1, 14);
    
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('QUAD_RESOLUTION', '0', 1, 14);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('QUAD_WRAPPING', '1', 1, 14);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('QUAD_BITC', 'true', 1, 14);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('QUAD_DIR', '/video/browse/', 1, 14);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('QUAD_DEST', '', 1, 14);

-- Stage Router
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('IMAGE_ASPECT', '16/9', 1, 15);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('TIMECODE_MODE', '1', 1, 15);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('COPY_COMMAND', '', 1, 15);
    
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE1_RESOLUTION', '0', 1, 15);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE1_WRAPPING', '2', 1, 15);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE1_BITC', 'false', 1, 15);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE1_DIR', '/video/mxf_online/', 1, 15);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE1_DEST', '/store/mxf_online_lot/', 1, 15);
    
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE2_RESOLUTION', '0', 1, 15);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE2_WRAPPING', '2', 1, 15);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE2_BITC', 'false', 1, 15);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE2_DIR', '/video/mxf_online/', 1, 15);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE2_DEST', '/store/mxf_online/', 1, 15);
    
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('QUAD_RESOLUTION', '0', 1, 15);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('QUAD_WRAPPING', '1', 1, 15);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('QUAD_BITC', 'true', 1, 15);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('QUAD_DIR', '/video/browse/', 1, 15);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('QUAD_DEST', '', 1, 15);

-- Stage Router
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('IMAGE_ASPECT', '16/9', 1, 16);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('TIMECODE_MODE', '1', 1, 16);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('COPY_COMMAND', '', 1, 16);
    
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE1_RESOLUTION', '0', 1, 16);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE1_WRAPPING', '2', 1, 16);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE1_BITC', 'false', 1, 16);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE1_DIR', '/video/mxf_online/', 1, 16);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE1_DEST', '/store/mxf_online_lot/', 1, 16);
    
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE2_RESOLUTION', '0', 1, 16);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE2_WRAPPING', '2', 1, 16);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE2_BITC', 'false', 1, 16);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE2_DIR', '/video/mxf_online/', 1, 16);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE2_DEST', '/store/mxf_online/', 1, 16);
    
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('QUAD_RESOLUTION', '0', 1, 16);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('QUAD_WRAPPING', '1', 1, 16);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('QUAD_BITC', 'true', 1, 16);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('QUAD_DIR', '/video/browse/', 1, 16);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('QUAD_DEST', '', 1, 16);



--
-- recorderinputconfig
--
-- ric_identifier | ric_index | ric_name | ric_recorder_conf_id

-- Ingex-Lot
INSERT INTO recorderinputconfig VALUES (100, 1, 'Channel 0', 11);
INSERT INTO recorderinputconfig VALUES (101, 2, 'Channel 1', 11);
INSERT INTO recorderinputconfig VALUES (102, 3, 'Channel 2', 11);
INSERT INTO recorderinputconfig VALUES (103, 4, 'Channel 3', 11);

-- Ingex-Stage
INSERT INTO recorderinputconfig VALUES (110, 1, 'Channel 0', 12);
INSERT INTO recorderinputconfig VALUES (111, 2, 'Channel 1', 12);
INSERT INTO recorderinputconfig VALUES (112, 3, 'Channel 2', 12);
INSERT INTO recorderinputconfig VALUES (113, 4, 'Channel 3', 12);

-- Ingex-StudioA
INSERT INTO recorderinputconfig VALUES (120, 1, 'Channel 0', 13);
INSERT INTO recorderinputconfig VALUES (121, 2, 'Channel 1', 13);
INSERT INTO recorderinputconfig VALUES (122, 3, 'Channel 2', 13);
INSERT INTO recorderinputconfig VALUES (123, 4, 'Channel 3', 13);

-- Router-Lot
INSERT INTO recorderinputconfig VALUES (130, 1, 'Channel 0', 14);

-- Router-Stage
INSERT INTO recorderinputconfig VALUES (140, 1, 'Channel 0', 15);

-- Router-StudioA
INSERT INTO recorderinputconfig VALUES (150, 1, 'Channel 0', 16);


SELECT setval('ric_id_seq', max(ric_identifier)) FROM recorderinputconfig;


--
-- recorderinputtrackconfig
--
-- rtc_identifier | rtc_index | rtc_track_number | rtc_recorder_input_id | rtc_source_id | rtc_source_track_id

-- Lot
INSERT INTO recorderinputtrackconfig VALUES (400, 1, 1, 100, 100, 1);
INSERT INTO recorderinputtrackconfig VALUES (401, 2, 1, 100, 100, 2);
INSERT INTO recorderinputtrackconfig VALUES (402, 3, 2, 100, 100, 3);
INSERT INTO recorderinputtrackconfig VALUES (403, 4, 0, 100, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (404, 5, 0, 100, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (410, 1, 1, 101, 101, 1);
INSERT INTO recorderinputtrackconfig VALUES (411, 2, 1, 101, 101, 2);
INSERT INTO recorderinputtrackconfig VALUES (412, 3, 2, 101, 101, 3);
INSERT INTO recorderinputtrackconfig VALUES (413, 4, 0, 101, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (414, 5, 0, 101, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (420, 1, 1, 102, 102, 1);
INSERT INTO recorderinputtrackconfig VALUES (421, 2, 1, 102, 102, 2);
INSERT INTO recorderinputtrackconfig VALUES (422, 3, 2, 102, 102, 3);
INSERT INTO recorderinputtrackconfig VALUES (423, 4, 0, 102, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (424, 5, 0, 102, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (430, 1, 1, 103, 103, 1);
INSERT INTO recorderinputtrackconfig VALUES (431, 2, 1, 103, 103, 2);
INSERT INTO recorderinputtrackconfig VALUES (432, 3, 2, 103, 103, 3);
INSERT INTO recorderinputtrackconfig VALUES (433, 4, 0, 103, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (434, 5, 0, 103, NULL, NULL);


-- Stage
INSERT INTO recorderinputtrackconfig VALUES (500, 1, 1, 110, 110, 1);
INSERT INTO recorderinputtrackconfig VALUES (501, 2, 1, 110, 110, 2);
INSERT INTO recorderinputtrackconfig VALUES (502, 3, 2, 110, 110, 3);
INSERT INTO recorderinputtrackconfig VALUES (503, 4, 0, 110, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (504, 5, 0, 110, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (510, 1, 1, 111, 111, 1);
INSERT INTO recorderinputtrackconfig VALUES (511, 2, 1, 111, 111, 2);
INSERT INTO recorderinputtrackconfig VALUES (512, 3, 2, 111, 111, 3);
INSERT INTO recorderinputtrackconfig VALUES (513, 4, 0, 111, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (514, 5, 0, 111, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (520, 1, 1, 112, 112, 1);
INSERT INTO recorderinputtrackconfig VALUES (521, 2, 1, 112, 112, 2);
INSERT INTO recorderinputtrackconfig VALUES (522, 3, 2, 112, 112, 3);
INSERT INTO recorderinputtrackconfig VALUES (523, 4, 0, 112, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (524, 5, 0, 112, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (530, 1, 1, 113, 113, 1);
INSERT INTO recorderinputtrackconfig VALUES (531, 2, 1, 113, 113, 2);
INSERT INTO recorderinputtrackconfig VALUES (532, 3, 2, 113, 113, 3);
INSERT INTO recorderinputtrackconfig VALUES (533, 4, 0, 113, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (534, 5, 0, 113, NULL, NULL);

-- Studio A
INSERT INTO recorderinputtrackconfig VALUES (600, 1, 1, 120, 120, 1);
INSERT INTO recorderinputtrackconfig VALUES (601, 2, 1, 120, 120, 2);
INSERT INTO recorderinputtrackconfig VALUES (602, 3, 2, 120, 120, 3);
INSERT INTO recorderinputtrackconfig VALUES (603, 4, 0, 120, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (604, 5, 0, 120, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (610, 1, 1, 121, 121, 1);
INSERT INTO recorderinputtrackconfig VALUES (611, 2, 1, 121, 121, 2);
INSERT INTO recorderinputtrackconfig VALUES (612, 3, 2, 121, 121, 3);
INSERT INTO recorderinputtrackconfig VALUES (613, 4, 0, 121, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (614, 5, 0, 121, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (620, 1, 1, 122, 122, 1);
INSERT INTO recorderinputtrackconfig VALUES (621, 2, 1, 122, 122, 2);
INSERT INTO recorderinputtrackconfig VALUES (622, 3, 2, 122, 122, 3);
INSERT INTO recorderinputtrackconfig VALUES (623, 4, 0, 122, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (624, 5, 0, 122, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (630, 1, 1, 123, 123, 1);
INSERT INTO recorderinputtrackconfig VALUES (631, 2, 1, 123, 123, 2);
INSERT INTO recorderinputtrackconfig VALUES (632, 3, 2, 123, 123, 3);
INSERT INTO recorderinputtrackconfig VALUES (633, 4, 0, 123, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (634, 5, 0, 123, NULL, NULL);

-- Lot Router
INSERT INTO recorderinputtrackconfig VALUES (700, 1, 1, 130, 104, 1);

-- Stage Router
INSERT INTO recorderinputtrackconfig VALUES (800, 1, 1, 140, 114, 1);

-- Lot Router
INSERT INTO recorderinputtrackconfig VALUES (900, 1, 1, 150, 124, 1);


SELECT setval('rtc_id_seq', max(rtc_identifier)) FROM recorderinputtrackconfig;


--
-- multicameraclipdef
--
-- mcd_identifier | mcd_name

INSERT INTO multicameraclipdef VALUES (1, 'Lot multi-camera clip');
INSERT INTO multicameraclipdef VALUES (2, 'Stage multi-camera clip');
INSERT INTO multicameraclipdef VALUES (3, 'StudioA multi-camera clip');

SELECT setval('mcd_id_seq', max(mcd_identifier)) FROM multicameraclipdef;


--
-- multicameratrackdef
--
-- mct_identifier | mct_index | mct_track_number | mct_multi_camera_def_id

-- Lot multi-camera clip
INSERT INTO multicameratrackdef VALUES (10, 1, 1, 1);
INSERT INTO multicameratrackdef VALUES (11, 2, 1, 1);
INSERT INTO multicameratrackdef VALUES (12, 3, 2, 1);

-- Stage multi-camera clip
INSERT INTO multicameratrackdef VALUES (20, 1, 1, 2);
INSERT INTO multicameratrackdef VALUES (21, 2, 1, 2);
INSERT INTO multicameratrackdef VALUES (22, 3, 2, 2);

-- StudioA multi-camera clip
INSERT INTO multicameratrackdef VALUES (30, 1, 1, 3);
INSERT INTO multicameratrackdef VALUES (31, 2, 1, 3);
INSERT INTO multicameratrackdef VALUES (32, 3, 2, 3);

SELECT setval('mct_id_seq', max(mct_identifier)) FROM multicameratrackdef;


--
-- multicameraselectordef
--
-- mcs_identifier | mcs_index | mcs_multi_camera_track_def_id | mcs_source_id | mcs_source_track_id

-- Lot
INSERT INTO multicameraselectordef VALUES (10, 1, 10, 100, 1);
INSERT INTO multicameraselectordef VALUES (11, 2, 10, 101, 1);
INSERT INTO multicameraselectordef VALUES (12, 3, 10, 102, 1);
INSERT INTO multicameraselectordef VALUES (13, 4, 10, 103, 1);
INSERT INTO multicameraselectordef VALUES (14, 1, 11, 100, 2);
INSERT INTO multicameraselectordef VALUES (15, 1, 12, 100, 3);

-- Stage
INSERT INTO multicameraselectordef VALUES (20, 1, 20, 110, 1);
INSERT INTO multicameraselectordef VALUES (21, 2, 20, 111, 1);
INSERT INTO multicameraselectordef VALUES (22, 3, 20, 112, 1);
INSERT INTO multicameraselectordef VALUES (23, 4, 20, 113, 1);
INSERT INTO multicameraselectordef VALUES (24, 1, 21, 110, 2);
INSERT INTO multicameraselectordef VALUES (25, 1, 22, 110, 3);

-- Studio A
INSERT INTO multicameraselectordef VALUES (30, 1, 30, 120, 1);
INSERT INTO multicameraselectordef VALUES (31, 2, 30, 121, 1);
INSERT INTO multicameraselectordef VALUES (32, 3, 30, 122, 1);
INSERT INTO multicameraselectordef VALUES (33, 4, 30, 123, 1);
INSERT INTO multicameraselectordef VALUES (34, 1, 31, 120, 2);
INSERT INTO multicameraselectordef VALUES (35, 1, 32, 120, 3);

SELECT setval('mcs_id_seq', max(mcs_identifier)) FROM multicameraselectordef;




--
-- Proxy definitions
---




END;

