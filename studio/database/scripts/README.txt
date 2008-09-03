In a new installation, create the 'bamzooki' user:
	./create_bamzooki_user.sh


Create database (see create_bamzooki_user.sh for password):
	./create_prodautodb.sh
	

Drop database:
	./drop_prodautodb.sh

	
Add default configuration: (see create_bamzooki_user.sh for password):
	./add_default_config.sh
	
	
Backup a database
    pg_dump -U bamzooki -d prodautodb -Fc -f backup.pgdump

Restore a database:
    First two lines from create_prodautodb.sh
    Then use pg_restore with appropriate options e.g.
    pg_restore -U bamzooki -d prodautodb backup.pgdump

