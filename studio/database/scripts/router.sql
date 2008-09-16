BEGIN;


--
-- Router configurations
--

-- router config
INSERT INTO RouterConfig 
    (ror_identifier, ror_name) 
VALUES 
    (1, 'KW-Router');
SELECT setval('ror_id_seq', max(ror_identifier)) FROM RouterConfig;


-- router input config
INSERT INTO RouterInputConfig 
    (rti_identifier, rti_index, rti_name, rti_router_conf_id, rti_source_id, rti_source_track_id) 
VALUES 
    (1, 1, 'VT1', 1, 1, 1);

INSERT INTO RouterInputConfig 
    (rti_identifier, rti_index, rti_name, rti_router_conf_id, rti_source_id, rti_source_track_id) 
VALUES 
    (2, 2, 'VT2', 1, 2, 1);

INSERT INTO RouterInputConfig 
    (rti_identifier, rti_index, rti_name, rti_router_conf_id, rti_source_id, rti_source_track_id) 
VALUES 
    (3, 3, 'VT3', 1, 3, 1);

INSERT INTO RouterInputConfig 
    (rti_identifier, rti_index, rti_name, rti_router_conf_id, rti_source_id, rti_source_track_id) 
VALUES 
    (4, 4, 'VT4', 1, 4, 1);

SELECT setval('rti_id_seq', max(rti_identifier)) FROM RouterInputConfig;


-- router output config
INSERT INTO RouterOutputConfig 
    (rto_identifier, rto_index, rto_name, rto_router_conf_id) 
VALUES 
    (1, 1, 'Out1', 1);

INSERT INTO RouterOutputConfig 
    (rto_identifier, rto_index, rto_name, rto_router_conf_id) 
VALUES 
    (2, 2, 'Out2', 1);

SELECT setval('rto_id_seq', max(rto_identifier)) FROM RouterOutputConfig;

END;
