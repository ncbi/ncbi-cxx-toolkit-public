FastCGI sample application readme.
----------------------------------


In order to run a fast CGI sample in the development(debugging) 
mode you need to:

1. Compile the example.
2. Modify the cgi_sample.ini file sectio FastCGI, key StandaloneServer.
Specify a TCP/IP port number for FCGI to run.
4. Modify a CGI-FCGI redirector script (see example xcgi_sample.cgi). 
Make sure redirector connects to the right TCP/IP port.
For a WEB server redirector looks just like a regular CGI script.
5. Run fcgi_sample.cgi as a standalone application.
Sure, you can run this sample under debugger and set a breakpoint in 
CSampleCgiApplication::ProcessRequest().
6. Use a WEB browser to run xcgi_sample.cgi. Request will be redirected to
the fcgi_sample (debugger stops in ProcessRequest).

For more information on FastCGI please see http://www.fastcgi.com


