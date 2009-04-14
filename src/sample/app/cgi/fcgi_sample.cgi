#! /bin/sh
# $Id$

# This script is a simple CGI to FastCGI redirector which forwards
# the CGI environment and request parameters to a FastCGI application
# for more information see http://www.fastcgi.com


/netopt/ncbi_tools/fcgi-current/bin/cgi-fcgi -bind -connect localhost:5000
