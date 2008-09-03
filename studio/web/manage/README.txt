Installation
------------

Requirements:
* apache2 web server, with static html documents in /srv/www/htdocs and cgi in 
/srv/www/cgi-bin
* perl CGI, CGI::Pretty, Text::Template, DBI, PostgreSQL DBD modules

CGI and CGI::Pretty should already be present as part of perl.
perl-DBI and perl-DBD-Pg can be installed using YaST.

Text::Template can be installed via CPAN as follows:
$ su - root
root> perl -MCPAN -e 'install Text::Template'


1) edit the configuration file "conf/ingex.conf" to match your system setup
2) run "sudo install/install.sh" to install the files.



Usage
-----

Open your web browser to the page http://<host name>/cgi-bin/ingex/index.pl




