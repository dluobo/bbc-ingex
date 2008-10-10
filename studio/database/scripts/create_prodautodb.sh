psql -v ON_ERROR_STOP=yes -U postgres -d template1 -c "CREATE DATABASE prodautodb"
psql -v ON_ERROR_STOP=yes -U postgres -d template1 -c "GRANT CREATE ON DATABASE prodautodb TO bamzooki"
psql -v ON_ERROR_STOP=yes -U postgres -d prodautodb -c "CREATE LANGUAGE plpgsql"
psql -v ON_ERROR_STOP=yes -U bamzooki -d prodautodb -f ProdAutoDatabase.sql


