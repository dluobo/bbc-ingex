BEGIN;



--
-- recordinglocation
--

INSERT INTO recordinglocation VALUES (10, 'Den');
INSERT INTO recordinglocation VALUES (11, 'Evan');

SELECT setval('rlc_id_seq', max(rlc_identifier)) FROM recordinglocation;


--
-- sourceconfig
--

INSERT INTO sourceconfig VALUES (100, 'Cam1-Den', 2, NULL, 10);
INSERT INTO sourceconfig VALUES (101, 'Cam2-Den', 2, NULL, 10);
INSERT INTO sourceconfig VALUES (102, 'Cam3-Den', 2, NULL, 10);
INSERT INTO sourceconfig VALUES (103, 'Cam4-Den', 2, NULL, 10);
INSERT INTO sourceconfig VALUES (104, 'Cam5-Evan', 2, NULL, 11);
INSERT INTO sourceconfig VALUES (105, 'Cam6-Evan', 2, NULL, 11);
INSERT INTO sourceconfig VALUES (106, 'Cam7-Den', 2, NULL, 10);
INSERT INTO sourceconfig VALUES (107, 'Cam8-Evan', 2, NULL, 11);
--INSERT INTO sourceconfig VALUES (110, 'Audio Multitrack', 2, NULL, 10);

SELECT setval('scf_id_seq', max(scf_identifier)) FROM sourceconfig;


--
-- sourcetrackconfig
--

-- Cam1234-Den
INSERT INTO sourcetrackconfig VALUES (200, 1, 1, 'V',  1, '(25,1)', 10800000, 100);
INSERT INTO sourcetrackconfig VALUES (201, 2, 1, 'A1', 2, '(25,1)', 10800000, 100);
INSERT INTO sourcetrackconfig VALUES (202, 3, 2, 'A2', 2, '(25,1)', 10800000, 100);
--INSERT INTO sourcetrackconfig VALUES (203, 4, 3, 'A3', 2, '(25,1)', 10800000, 100);
--INSERT INTO sourcetrackconfig VALUES (204, 5, 4, 'A4', 2, '(25,1)', 10800000, 100);
INSERT INTO sourcetrackconfig VALUES (210, 1, 1, 'V',  1, '(25,1)', 10800000, 101);
--INSERT INTO sourcetrackconfig VALUES (211, 2, 1, 'A1', 2, '(25,1)', 10800000, 101);
--INSERT INTO sourcetrackconfig VALUES (212, 3, 2, 'A2', 2, '(25,1)', 10800000, 101);
--INSERT INTO sourcetrackconfig VALUES (213, 4, 3, 'A3', 2, '(25,1)', 10800000, 101);
--INSERT INTO sourcetrackconfig VALUES (214, 5, 4, 'A4', 2, '(25,1)', 10800000, 101);
INSERT INTO sourcetrackconfig VALUES (220, 1, 1, 'V',  1, '(25,1)', 10800000, 102);
--INSERT INTO sourcetrackconfig VALUES (221, 2, 1, 'A1', 2, '(25,1)', 10800000, 102);
--INSERT INTO sourcetrackconfig VALUES (222, 3, 2, 'A2', 2, '(25,1)', 10800000, 102);
--INSERT INTO sourcetrackconfig VALUES (223, 4, 3, 'A3', 2, '(25,1)', 10800000, 102);
--INSERT INTO sourcetrackconfig VALUES (224, 5, 4, 'A4', 2, '(25,1)', 10800000, 102);
INSERT INTO sourcetrackconfig VALUES (230, 1, 1, 'V',  1, '(25,1)', 10800000, 103);
--INSERT INTO sourcetrackconfig VALUES (231, 2, 1, 'A1', 2, '(25,1)', 10800000, 103);
--INSERT INTO sourcetrackconfig VALUES (232, 3, 2, 'A2', 2, '(25,1)', 10800000, 103);
--INSERT INTO sourcetrackconfig VALUES (233, 4, 3, 'A3', 2, '(25,1)', 10800000, 103);
--INSERT INTO sourcetrackconfig VALUES (234, 5, 4, 'A4', 2, '(25,1)', 10800000, 103);

-- Cam56-Evan
INSERT INTO sourcetrackconfig VALUES (240, 1, 1, 'V',  1, '(25,1)', 10800000, 104);
INSERT INTO sourcetrackconfig VALUES (241, 2, 1, 'A1', 2, '(25,1)', 10800000, 104);
INSERT INTO sourcetrackconfig VALUES (242, 3, 2, 'A2', 2, '(25,1)', 10800000, 104);
--INSERT INTO sourcetrackconfig VALUES (243, 4, 3, 'A3', 2, '(25,1)', 10800000, 104);
--INSERT INTO sourcetrackconfig VALUES (244, 5, 4, 'A4', 2, '(25,1)', 10800000, 104);
INSERT INTO sourcetrackconfig VALUES (250, 1, 1, 'V',  1, '(25,1)', 10800000, 105);
--INSERT INTO sourcetrackconfig VALUES (251, 2, 1, 'A1', 2, '(25,1)', 10800000, 105);
--INSERT INTO sourcetrackconfig VALUES (252, 3, 2, 'A2', 2, '(25,1)', 10800000, 105);
--INSERT INTO sourcetrackconfig VALUES (253, 4, 3, 'A3', 2, '(25,1)', 10800000, 105);
--INSERT INTO sourcetrackconfig VALUES (254, 5, 4, 'A4', 2, '(25,1)', 10800000, 105);

-- Cam7-Den
INSERT INTO sourcetrackconfig VALUES (260, 1, 1, 'V',  1, '(25,1)', 10800000, 106);
--INSERT INTO sourcetrackconfig VALUES (261, 2, 1, 'A1', 2, '(25,1)', 10800000, 106);
--INSERT INTO sourcetrackconfig VALUES (262, 3, 2, 'A2', 2, '(25,1)', 10800000, 106);
--INSERT INTO sourcetrackconfig VALUES (263, 4, 3, 'A3', 2, '(25,1)', 10800000, 106);
--INSERT INTO sourcetrackconfig VALUES (264, 5, 4, 'A4', 2, '(25,1)', 10800000, 106);

-- Cam8-Evan
INSERT INTO sourcetrackconfig VALUES (270, 1, 1, 'V',  1, '(25,1)', 10800000, 107);

-- Audio Multitrack
--INSERT INTO sourcetrackconfig VALUES (281, 1, 1, 'A1', 2, '(25,1)', 10800000, 110);
--INSERT INTO sourcetrackconfig VALUES (282, 2, 2, 'A2', 2, '(25,1)', 10800000, 110);
--INSERT INTO sourcetrackconfig VALUES (283, 3, 3, 'A3', 2, '(25,1)', 10800000, 110);
--INSERT INTO sourcetrackconfig VALUES (284, 4, 4, 'A4', 2, '(25,1)', 10800000, 110);
--INSERT INTO sourcetrackconfig VALUES (285, 5, 5, 'A5', 2, '(25,1)', 10800000, 110);
--INSERT INTO sourcetrackconfig VALUES (286, 6, 6, 'A6', 2, '(25,1)', 10800000, 110);
--INSERT INTO sourcetrackconfig VALUES (287, 7, 7, 'A7', 2, '(25,1)', 10800000, 110);
--INSERT INTO sourcetrackconfig VALUES (288, 8, 8, 'A8', 2, '(25,1)', 10800000, 110);


SELECT setval('sct_id_seq', max(sct_identifier)) FROM sourcetrackconfig;


--
-- recorder
--

INSERT INTO recorder VALUES (1, 'Ingex1', NULL);
INSERT INTO recorder VALUES (2, 'Ingex2', NULL);
INSERT INTO recorder VALUES (3, 'Ingex3', NULL);

SELECT setval('rer_id_seq', max(rer_identifier)) FROM recorder;


--
-- recorderconfig
--

INSERT INTO recorderconfig VALUES (10, 'Den1', 1);
INSERT INTO recorderconfig VALUES (11, 'Den2', 2);
INSERT INTO recorderconfig VALUES (12, 'Evan', 3);

SELECT setval('rec_id_seq', max(rec_identifier)) FROM recorderconfig;



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
    VALUES (10, 'ENCODE1_RESOLUTION', '9', 1);
INSERT INTO defaultrecorderparameter (drp_identifier, drp_name, drp_value, drp_type)
    VALUES (11, 'ENCODE1_WRAPPING', '2', 1);
INSERT INTO defaultrecorderparameter (drp_identifier, drp_name, drp_value, drp_type)
    VALUES (12, 'ENCODE1_BITC', 'true', 1);
INSERT INTO defaultrecorderparameter (drp_identifier, drp_name, drp_value, drp_type)
    VALUES (13, 'ENCODE1_DIR', '/video/mxf_offline/', 1);
INSERT INTO defaultrecorderparameter (drp_identifier, drp_name, drp_value, drp_type)
    VALUES (14, 'ENCODE1_DEST', '/store/mxf_offline/', 1);
    
INSERT INTO defaultrecorderparameter (drp_identifier, drp_name, drp_value, drp_type)
    VALUES (20, 'ENCODE2_RESOLUTION', '4', 1);
INSERT INTO defaultrecorderparameter (drp_identifier, drp_name, drp_value, drp_type)
    VALUES (21, 'ENCODE2_WRAPPING', '2', 1);
INSERT INTO defaultrecorderparameter (drp_identifier, drp_name, drp_value, drp_type)
    VALUES (22, 'ENCODE2_BITC', 'false', 1);
INSERT INTO defaultrecorderparameter (drp_identifier, drp_name, drp_value, drp_type)
    VALUES (23, 'ENCODE2_DIR', '/video/mxf_online/', 1);
INSERT INTO defaultrecorderparameter (drp_identifier, drp_name, drp_value, drp_type)
    VALUES (24, 'ENCODE2_DEST', '/store/mxf_online/', 1);
    
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

-- Ingex1 - Den
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('MXF', 'true', 1, 10);

INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('IMAGE_ASPECT', '16/9', 1, 10);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('TIMECODE_MODE', '1', 1, 10);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('COPY_COMMAND', '/home/ingex/ap-workspace/ingex/studio/scripts/xfer.pl', 1, 10);
    
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE1_RESOLUTION', '9', 1, 10);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE1_WRAPPING', '2', 1, 10);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE1_BITC', 'true', 1, 10);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE1_DIR', '/video/mxf_offline/', 1, 10);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE1_DEST', '/store/mxf_offline/', 1, 10);
    
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE2_RESOLUTION', '4', 1, 10);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE2_WRAPPING', '2', 1, 10);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE2_BITC', 'false', 1, 10);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE2_DIR', '/video/mxf_online/', 1, 10);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE2_DEST', '/store/mxf_online/', 1, 10);
    
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

-- Ingex2 - Den2
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('MXF', 'true', 1, 11);

INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('IMAGE_ASPECT', '16/9', 1, 11);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('TIMECODE_MODE', '1', 1, 11);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('COPY_COMMAND', '/home/ingex/ap-workspace/ingex/studio/scripts/xfer.pl', 1, 11);
    
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE1_RESOLUTION', '9', 1, 11);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE1_WRAPPING', '2', 1, 11);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE1_BITC', 'true', 1, 11);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE1_DIR', '/video/mxf_offline/', 1, 11);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE1_DEST', '/store/mxf_offline/', 1, 11);
    
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE2_RESOLUTION', '4', 1, 11);
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

-- Ingex3 - Evan
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('MXF', 'true', 1, 12);

INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('IMAGE_ASPECT', '16/9', 1, 12);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('TIMECODE_MODE', '1', 1, 12);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('COPY_COMMAND', '/home/ingex/ap-workspace/ingex/studio/scripts/xfer.pl', 1, 12);
    
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE1_RESOLUTION', '9', 1, 12);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE1_WRAPPING', '2', 1, 12);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE1_BITC', 'true', 1, 12);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE1_DIR', '/video/mxf_offline/', 1, 12);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE1_DEST', '/store/mxf_offline/', 1, 12);
    
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE2_RESOLUTION', '4', 1, 12);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE2_WRAPPING', '2', 1, 12);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE2_BITC', 'false', 1, 12);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE2_DIR', '/video/mxf_online/', 1, 12);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE2_DEST', '/store/mxf_online/', 1, 12);
    
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('QUAD_RESOLUTION', '0', 1, 12);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('QUAD_WRAPPING', '1', 1, 12);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('QUAD_BITC', 'true', 1, 12);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('QUAD_DIR', '/video/browse/', 1, 12);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('QUAD_DEST', '/store/browse/', 1, 12);



--
-- recorder update (i.e. selected config)
--

UPDATE recorder SET rer_conf_id=10 WHERE rer_identifier = 1;
UPDATE recorder SET rer_conf_id=11 WHERE rer_identifier = 2;
UPDATE recorder SET rer_conf_id=12 WHERE rer_identifier = 3;


--
-- recorderinputconfig
--

-- Ingex1
-- Den
INSERT INTO recorderinputconfig VALUES (100, 1, 'Channel 0', 10);
INSERT INTO recorderinputconfig VALUES (101, 2, 'Channel 1', 10);
INSERT INTO recorderinputconfig VALUES (102, 3, 'Channel 2', 10);
INSERT INTO recorderinputconfig VALUES (103, 4, 'Channel 3', 10);

-- Ingex2
-- Den2
INSERT INTO recorderinputconfig VALUES (110, 1, 'Channel 0', 11);
INSERT INTO recorderinputconfig VALUES (111, 2, 'Channel 1', 11);
INSERT INTO recorderinputconfig VALUES (112, 3, 'Channel 2', 11);
INSERT INTO recorderinputconfig VALUES (113, 4, 'Channel 3', 11);

-- Ingex3
-- Evan
INSERT INTO recorderinputconfig VALUES (120, 1, 'Channel 0', 12);
INSERT INTO recorderinputconfig VALUES (121, 2, 'Channel 1', 12);
INSERT INTO recorderinputconfig VALUES (122, 3, 'Channel 2', 12);
INSERT INTO recorderinputconfig VALUES (123, 4, 'Channel 3', 12);


SELECT setval('ric_id_seq', max(ric_identifier)) FROM recorderinputconfig;


--
-- recorderinputtrackconfig
--

-- Ingex1 Den
INSERT INTO recorderinputtrackconfig VALUES (400, 1, 1, 100, 100, 1);
INSERT INTO recorderinputtrackconfig VALUES (401, 2, 1, 100, 100, 2);
INSERT INTO recorderinputtrackconfig VALUES (402, 3, 2, 100, 100, 3);
INSERT INTO recorderinputtrackconfig VALUES (403, 4, 0, 100, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (404, 5, 0, 100, NULL, NULL);
--INSERT INTO recorderinputtrackconfig VALUES (405, 6, 0, 100, NULL, NULL);
--INSERT INTO recorderinputtrackconfig VALUES (406, 7, 0, 100, NULL, NULL);
--INSERT INTO recorderinputtrackconfig VALUES (407, 8, 0, 100, NULL, NULL);
--INSERT INTO recorderinputtrackconfig VALUES (408, 9, 0, 100, NULL, NULL);

INSERT INTO recorderinputtrackconfig VALUES (410, 1, 1, 101, 101, 1);
INSERT INTO recorderinputtrackconfig VALUES (411, 2, 0, 101, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (412, 3, 0, 101, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (413, 4, 0, 101, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (414, 5, 0, 101, NULL, NULL);

INSERT INTO recorderinputtrackconfig VALUES (420, 1, 1, 102, 102, 1);
INSERT INTO recorderinputtrackconfig VALUES (421, 2, 0, 102, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (422, 3, 0, 102, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (423, 4, 0, 102, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (424, 5, 0, 102, NULL, NULL);
--INSERT INTO recorderinputtrackconfig VALUES (425, 6, 5, 102, 110, 5);
--INSERT INTO recorderinputtrackconfig VALUES (426, 7, 6, 102, 110, 6);
--INSERT INTO recorderinputtrackconfig VALUES (427, 8, 7, 102, 110, 7);
--INSERT INTO recorderinputtrackconfig VALUES (428, 9, 8, 102, 110, 8);

INSERT INTO recorderinputtrackconfig VALUES (430, 1, 1, 103, 103, 1);
INSERT INTO recorderinputtrackconfig VALUES (431, 2, 0, 103, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (432, 3, 0, 103, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (433, 4, 0, 103, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (434, 5, 0, 103, NULL, NULL);


-- Ingex2 Den2
INSERT INTO recorderinputtrackconfig VALUES (500, 1, 1, 110, 106, 1);
INSERT INTO recorderinputtrackconfig VALUES (501, 2, 0, 110, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (502, 3, 0, 110, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (503, 4, 0, 110, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (504, 5, 0, 110, NULL, NULL);

INSERT INTO recorderinputtrackconfig VALUES (510, 1, 0, 111, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (511, 2, 0, 111, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (512, 3, 0, 111, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (513, 4, 0, 111, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (514, 5, 0, 111, NULL, NULL);

INSERT INTO recorderinputtrackconfig VALUES (520, 1, 0, 112, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (521, 2, 0, 112, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (522, 3, 0, 112, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (523, 4, 0, 112, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (524, 5, 0, 112, NULL, NULL);

INSERT INTO recorderinputtrackconfig VALUES (530, 1, 0, 113, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (531, 2, 0, 113, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (532, 3, 0, 113, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (533, 4, 0, 113, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (534, 5, 0, 113, NULL, NULL);

-- Ingex3 Evan
INSERT INTO recorderinputtrackconfig VALUES (600, 1, 1, 120, 107, 1);
INSERT INTO recorderinputtrackconfig VALUES (601, 2, 0, 120, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (602, 3, 0, 120, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (603, 4, 0, 120, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (604, 5, 0, 120, NULL, NULL);

INSERT INTO recorderinputtrackconfig VALUES (610, 1, 0, 121, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (611, 2, 0, 121, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (612, 3, 0, 121, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (613, 4, 0, 121, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (614, 5, 0, 121, NULL, NULL);

INSERT INTO recorderinputtrackconfig VALUES (620, 1, 1, 122, 104, 1);
INSERT INTO recorderinputtrackconfig VALUES (621, 2, 1, 122, 104, 2);
INSERT INTO recorderinputtrackconfig VALUES (622, 3, 2, 122, 104, 3);
INSERT INTO recorderinputtrackconfig VALUES (623, 4, 0, 122, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (624, 5, 0, 122, NULL, NULL);

INSERT INTO recorderinputtrackconfig VALUES (630, 1, 1, 123, 105, 1);
INSERT INTO recorderinputtrackconfig VALUES (631, 2, 0, 123, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (632, 3, 0, 123, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (633, 4, 0, 123, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (634, 5, 0, 123, NULL, NULL);


SELECT setval('rtc_id_seq', max(rtc_identifier)) FROM recorderinputtrackconfig;


--
-- multicameraclipdef
--

INSERT INTO multicameraclipdef VALUES (1, 'Den 5-way multi-camera clip');
INSERT INTO multicameraclipdef VALUES (2, 'Evan 3-way multi-camera clip');

SELECT setval('mcd_id_seq', max(mcd_identifier)) FROM multicameraclipdef;


--
-- multicameratrackdef
--

-- Den 5-way multi-camera clip
-- one video, two audio
INSERT INTO multicameratrackdef VALUES (1, 1, 1, 1);
INSERT INTO multicameratrackdef VALUES (2, 2, 1, 1);
INSERT INTO multicameratrackdef VALUES (3, 3, 2, 1);

-- Evan 3-way multi-camera clip
-- one video, two audio
INSERT INTO multicameratrackdef VALUES (4, 1, 1, 2);
INSERT INTO multicameratrackdef VALUES (5, 2, 1, 2);
INSERT INTO multicameratrackdef VALUES (6, 3, 2, 2);

SELECT setval('mct_id_seq', max(mct_identifier)) FROM multicameratrackdef;


--
-- multicameraselectordef
--

-- Den
-- 5 selectors for the video track, 1 selector for each audio track
INSERT INTO multicameraselectordef VALUES (100, 1, 1, 100, 1);
INSERT INTO multicameraselectordef VALUES (101, 2, 1, 101, 1);
INSERT INTO multicameraselectordef VALUES (102, 3, 1, 102, 1);
INSERT INTO multicameraselectordef VALUES (103, 4, 1, 103, 1);
INSERT INTO multicameraselectordef VALUES (104, 5, 1, 106, 1);
INSERT INTO multicameraselectordef VALUES (105, 1, 2, 100, 2);
INSERT INTO multicameraselectordef VALUES (106, 1, 3, 100, 3);

-- Evan
-- 3 selectors for the video track, 1 selector for each audio track
INSERT INTO multicameraselectordef VALUES (110, 1, 4, 104, 1);
INSERT INTO multicameraselectordef VALUES (111, 2, 4, 105, 1);
INSERT INTO multicameraselectordef VALUES (112, 3, 4, 107, 1);
INSERT INTO multicameraselectordef VALUES (113, 1, 5, 104, 2);
INSERT INTO multicameraselectordef VALUES (114, 1, 6, 104, 3);

SELECT setval('mcs_id_seq', max(mcs_identifier)) FROM multicameraselectordef;




--
-- Proxy definitions
---




END;

