#!/bin/sh

# very simple installation
# TODO: site specific settings, etc.

mkdir -p /var/www/cgi-bin/ingex

cp cgi/*.pl /var/www/cgi-bin/ingex
chmod 555 /var/www/cgi-bin/ingex/*.pl

cp modules/*.pm /var/www/cgi-bin/ingex
chmod 444 /var/www/cgi-bin/ingex/*.pm

cp templates/*.tmpl /var/www/cgi-bin/ingex
chmod 444 /var/www/cgi-bin/ingex/*.tmpl

cp conf/*.conf /var/www/cgi-bin/ingex
chmod 444 /var/www/cgi-bin/ingex/*.conf



mkdir -p /var/www/htdocs/ingex

cp styles/*.css /var/www/htdocs/ingex
chmod 444 /var/www/htdocs/ingex/*.css


