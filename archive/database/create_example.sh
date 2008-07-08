#!/bin/sh


echo "** Create 'ingex' user"
psql -v ON_ERROR_STOP=yes -U postgres -d template1 -c "CREATE USER ingex WITH PASSWORD 'ingex'" || echo "** Ignoring previous error"

echo "** Create 'ingexrecorderdb' database"
psql -v ON_ERROR_STOP=yes -U postgres -d template1 -c "CREATE DATABASE ingexrecorderdb"
psql -v ON_ERROR_STOP=yes -U postgres -d template1 -c "GRANT CREATE ON DATABASE ingexrecorderdb TO ingex"
psql -v ON_ERROR_STOP=yes -U postgres -d ingexrecorderdb -c "CREATE LANGUAGE plpgsql"
psql -v ON_ERROR_STOP=yes -U ingex -d ingexrecorderdb -f IngexRecorderDb.sql

echo "** Load test data"
psql -v ON_ERROR_STOP=yes -U ingex -d ingexrecorderdb < test_d3_source.sql > /dev/null

echo "** load the real data here..."

echo "** Count rows in D3Source table"
psql -U ingex -d ingexrecorderdb -c "select count(*) from d3source;"

echo "** Count rows in D3Source table, but not including test items which have d3s_identifier <= 100"
psql -U ingex -d ingexrecorderdb -c "select count(*) from d3source where d3s_identifier > 100;"

echo "** Vacuum"
psql -U ingex -d ingexrecorderdb -c "VACUUM ANALYSE;"

