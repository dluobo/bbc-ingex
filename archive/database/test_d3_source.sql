-- Test D3 sources
--
-- Note: src_identifier must < 128 so that it does not conflict with the sql output from
--       process_infax_dump_v2.cpp


INSERT INTO Source (
   src_identifier, src_type_id, src_barcode)
VALUES (
   1, 1, 'DA 999999');
INSERT INTO D3Source (
   d3s_format, d3s_prog_title,  d3s_episode_title,
   d3s_tx_date, d3s_mag_prefix, d3s_prog_no,
   d3s_prod_code, d3s_spool_status, d3s_stock_date,
   d3s_spool_descr, d3s_memo, d3s_duration,
   d3s_spool_no, d3s_acc_no, d3s_cat_detail,
   d3s_item_no, d3s_source_id)
VALUES (
   'D3', 'TEST 0', 'TEST 0',
   '2007-12-01', 'A', 'XYZ1234A',
   '0', 'M', '2007-11-14',
   'TEST SPOOL 0', 'ONLY USE FOR TESTING', 45000,
   'DA 999999', 'DA999999', NULL,
   1, 1);

INSERT INTO Source (
   src_identifier, src_type_id, src_barcode)
VALUES (
   2, 1, 'DA 999998');
INSERT INTO D3Source (
   d3s_format, d3s_prog_title,  d3s_episode_title,
   d3s_tx_date, d3s_mag_prefix, d3s_prog_no,
   d3s_prod_code, d3s_spool_status, d3s_stock_date,
   d3s_spool_descr, d3s_memo, d3s_duration,
   d3s_spool_no, d3s_acc_no, d3s_cat_detail,
   d3s_item_no, d3s_source_id)
VALUES (
   'D3', 'TEST 1', 'TEST 1',
   '2007-12-01', 'A', 'XYZ1234B',
   '0', 'M', '2007-11-14',
   'TEST SPOOL 1', 'ONLY USE FOR TESTING', 45000,
   'DA 999998', 'DA999998', NULL,
   1, 2);

INSERT INTO Source (
   src_identifier, src_type_id, src_barcode)
VALUES (
   3, 1, 'DA 999997');
INSERT INTO D3Source (
   d3s_format, d3s_prog_title,  d3s_episode_title,
   d3s_tx_date, d3s_mag_prefix, d3s_prog_no,
   d3s_prod_code, d3s_spool_status, d3s_stock_date,
   d3s_spool_descr, d3s_memo, d3s_duration,
   d3s_spool_no, d3s_acc_no, d3s_cat_detail,
   d3s_item_no, d3s_source_id)
VALUES (
   'D3', 'TEST 2', 'TEST 2',
   '2007-12-01', 'A', 'XYZ1234C',
   '0', 'M', '2007-11-14',
   'TEST SPOOL 2', 'ONLY USE FOR TESTING', 45000,
   'DA 999997', 'DA999997', NULL,
   1, 3);

INSERT INTO Source (
   src_identifier, src_type_id, src_barcode)
VALUES (
   4, 1, 'DA 999996');
INSERT INTO D3Source (
   d3s_format, d3s_prog_title,  d3s_episode_title,
   d3s_tx_date, d3s_mag_prefix, d3s_prog_no,
   d3s_prod_code, d3s_spool_status, d3s_stock_date,
   d3s_spool_descr, d3s_memo, d3s_duration,
   d3s_spool_no, d3s_acc_no, d3s_cat_detail,
   d3s_item_no, d3s_source_id)
VALUES (
   'D3', 'TEST 3', 'TEST 3',
   '2007-12-01', 'A', 'XYZ1234D',
   '0', 'M', '2007-11-14',
   'TEST SPOOL 3', 'ONLY USE FOR TESTING', 45000,
   'DA 999996', 'DA999996', NULL,
   1, 4);

INSERT INTO Source (
   src_identifier, src_type_id, src_barcode)
VALUES (
   5, 1, 'DA 999995');
INSERT INTO D3Source (
   d3s_format, d3s_prog_title,  d3s_episode_title,
   d3s_tx_date, d3s_mag_prefix, d3s_prog_no,
   d3s_prod_code, d3s_spool_status, d3s_stock_date,
   d3s_spool_descr, d3s_memo, d3s_duration,
   d3s_spool_no, d3s_acc_no, d3s_cat_detail,
   d3s_item_no, d3s_source_id)
VALUES (
   'D3', 'TEST 4', 'TEST 4 - item 1',
   '2007-12-01', 'A', 'XYZ1234E',
   '0', 'M', '2007-11-14',
   'TEST SPOOL 4', 'ONLY USE FOR TESTING', 45000,
   'DA 999995', 'DA999995', NULL,
   1, 5);
INSERT INTO D3Source (
   d3s_format, d3s_prog_title,  d3s_episode_title,
   d3s_tx_date, d3s_mag_prefix, d3s_prog_no,
   d3s_prod_code, d3s_spool_status, d3s_stock_date,
   d3s_spool_descr, d3s_memo, d3s_duration,
   d3s_spool_no, d3s_acc_no, d3s_cat_detail,
   d3s_item_no, d3s_source_id)
VALUES (
   'D3', 'TEST 4', 'TEST 4 - item 2',
   '2007-12-01', 'A', 'XYZ1234E',
   '0', 'M', '2007-11-14',
   'TEST SPOOL 4', 'ONLY USE FOR TESTING', 45000,
   'DA 999995', 'DA999995', NULL,
   2, 5);
INSERT INTO D3Source (
   d3s_format, d3s_prog_title,  d3s_episode_title,
   d3s_tx_date, d3s_mag_prefix, d3s_prog_no,
   d3s_prod_code, d3s_spool_status, d3s_stock_date,
   d3s_spool_descr, d3s_memo, d3s_duration,
   d3s_spool_no, d3s_acc_no, d3s_cat_detail,
   d3s_item_no, d3s_source_id)
VALUES (
   'D3', 'TEST 4', 'TEST 4 - item 3',
   '2007-12-01', 'A', 'XYZ1234E',
   '0', 'M', '2007-11-14',
   'TEST SPOOL 4', 'ONLY USE FOR TESTING', 45000,
   'DA 999995', 'DA999995', NULL,
   3, 5);

SELECT setval('src_id_seq', 100);
SELECT setval('d3s_id_seq', 100);

