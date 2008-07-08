#!/bin/sh


echo "** Drop existing 'ingexrecordb' database"
psql -U postgres -d template1 -c "DROP DATABASE ingexrecorderdb"

