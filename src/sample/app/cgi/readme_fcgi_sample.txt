FastCGI sample application readme.
----------------------------------


In order to run a fast CGI sample in the development (debugging) 
mode you need to:

1. Compile the example.

2. The CGI-FCGI redirector script "fcgi_sample.cgi" and the "fcgi_sample.ini"
   configuration file (section [FastCGI], entry StandaloneServer) have a
   TCP/IP port number (currently 5000; note the prepending colon!) which
   "fcgi_sample.cgi" and "fcgi_sample.fcgi" use to communicate.

3. For a WEB server, the "fcgi_sample.cgi" redirector looks just like a
   regular CGI script, and it 'll tunnel the HTTP request and response to
   and from your "real", FastCGI application, "fcgi_sample.fcgi".

4. Run "fcgi_sample.fcgi" as a standalone application.
 *) Sure, you can run this sample under debugger and set a breakpoint in 
    CSampleCgiApplication::ProcessRequest().

5. Use WEB browser to run "fcgi_sample.cgi". Request will be redirected to
   the "fcgi_sample.fcgi".
 *) Debugger will stop in ProcessRequest() if you did the [4.*] step above.


For more information on FastCGI please see http://www.fastcgi.com
