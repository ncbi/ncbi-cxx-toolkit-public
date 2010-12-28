#!/opt/python-2.5.1/bin/python
import cgi
import os
import subprocess
import sys
import urllib

# ViewVC proper knows nothing of svn:externals; this wrapper resolves
# them before handing the user off to it.
real_vvc = 'http://svn.ncbi.nlm.nih.gov/viewvc/'
repos = 'https://svn.ncbi.nlm.nih.gov/repos/'
base = repos + 'toolkit/trunk'
if 'internal' in os.environ['SCRIPT_NAME']:
    base += '/internal'
base += '/c++'

def resolve(stem, rest):
    while True:
        # print >>sys.stderr, stem + ' / ' + rest
        svn = subprocess.Popen(['svn', 'pg', 'svn:externals', stem],
                               stdout = subprocess.PIPE)
        matched = False
        for line in svn.communicate()[0].split('\n'):
            if len(line):
                (src, dest) = line.split() # ignore revision specifications
                if rest == src:
                    return dest
                elif rest.startswith(src + '/'):
                    matched = True
                    stem = dest
                    rest = rest[len(src)+1:]
                    break
        if not matched:
            try:
                next_slash = rest.index('/')
                stem += '/' + rest[:next_slash]
                rest = rest[next_slash+1:]
            except ValueError:
                break
    return stem + '/' + rest

form = cgi.FieldStorage()
path = form.getfirst('p')
resolved = resolve(base, path)
if not resolved.startswith(repos):
    print >>sys.stderr, path, "resolved to unsupported URL", resolved

url = real_vvc + urllib.quote(resolved[len(repos):])
qpath = cgi.escape(path, True)
qurl = cgi.escape(url, True)
print "Location:", url
print "Content-type: text/html"
print
# Cover all bases in case the Location header somehow didn't suffice.
print """<html>
  <head>
    <title>NCBI C++ Toolkit revision history: %s</title>
    <meta http-equiv="refresh" content="0; %s">
  <head>
  <body>
    <a href="%s">View %s's revision history</a>
  </body>
</html>""" % (qpath, qurl, qurl, qpath)
