#!/usr/bin/env python
import cgi
import os
import re
import subprocess
import sys
import urllib

# ViewVC proper knows nothing of svn:externals; this wrapper resolves
# them before handing the user off to it.
real_vvc = 'https://www.ncbi.nlm.nih.gov/viewvc/v1/'
repos = 'https://svn.ncbi.nlm.nih.gov/repos/'
viewable = repos
base = repos + 'toolkit/trunk'
public = True

try:
    script_name = os.environ['SCRIPT_NAME']
    server_name = os.environ['SERVER_NAME']
    if ('internal' in script_name or
        (server_name[0] == 'i' and not 'public' in script_name)):
        real_vvc = 'https://svn.ncbi.nlm.nih.gov/viewvc/'
        base += '/internal'
        public = False
except:
    pass

if public:
    viewable += 'toolkit/'
base += '/c++'

# Use a hard-coded lookup table for public trees, both because
# trunk/c++ is nominally self-contained rather than referring to the
# gbench repository for the public gui trees and because www.ncbi has
# no svn executable.  Other servers will also fall back on this table
# in the absence of /usr/bin/svn, but shouldn't need to in practice.
path_map = (
    ('include/gui', 'gbench/branches/3.5.0/include/gui'),
    ('src/gui',     'gbench/branches/3.5.0/src/gui')
    )

def resolve_via_map(base, path):
    for x in path_map:
        if path == x[0]:
            return repos + x[1]
        elif path.startswith(x[0] + '/'):
            return repos + x[1] + path[len(x[0]):]
    return base + '/' + path

url_splitter       = re.compile(r'(((https://)[^/]+/)repos/[^/]+)/(.+)')
traditional_syntax = re.compile(r'^(\S+)(?:\s+-r\d+)?\s+(\S+://\S+)')
new_syntax         = re.compile(r'^(?:-r\d+\s+)?(\S+?)(?:\@\d+)?\s+(\S+)')

def parse_externals(url):
    (root, server, scheme, rest) = url_splitter.match(url).groups()
    svn = subprocess.Popen(['svn', 'pg', 'svn:externals', url],
                           stdout = subprocess.PIPE)
    for line in svn.communicate()[0].split('\n'):
        if len(line) == 0:
            continue
        mo = traditional_syntax.match(line)
        if mo is not None:
            # Traditional format, always absolute
            yield mo.groups()
        mo = new_syntax.match(line)
        if mo is None:
            # Ignore as unsupported.  (XXX - warn?)
            yield ('', url)
        # New format (as of 1.5), possibly relative
        # XXX - support path quoting, introduced in 1.6?
        (dest, src) = mo.groups()
        if dest.find('://') < 0:
            # Relative in some fashion
            if dest.startswith('//'):
                dest = scheme + dest[2:]
            elif dest.startswith('/'):
                dest = server + dest[1:]
            else:
                # Relative to repository root or current stem
                d = dest
                if d.startswith('^/'):
                    d    = d[2:]
                    dest = root
                else:
                    dest = url
                while d.startswith('../'):
                    d    = d[3:]
                    dest = dest[:dest.rfind('/')]
                dest = dest + '/' + d
        yield (src, dest)    

def resolve_via_svn(stem, rest):
    while True:
        # print >>sys.stderr, stem + ' / ' + rest
        matched = False
        for (src, dest) in parse_externals(stem):
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

def resolve(base, path):
    if os.path.exists('/usr/bin/svn') and not public:
        return resolve_via_svn(base, path)
    else:
        return resolve_via_map(base, path)

form = cgi.FieldStorage()
path = form.getfirst('p')
resolved = resolve(base, path)
url = real_vvc + urllib.quote(resolved[len(viewable):])
if not resolved.startswith(viewable):
    print >>sys.stderr, path, "resolved to unsupported URL", resolved
    if public: # Try redirecting to internal side
        url = ('https://intranet.ncbi.nlm.nih.gov/ieb/ToolBox/CPP_DOC/internal'
               + '/viewvc.cgi?p=' + urllib.quote(path))
    else:
        print "Status: 500 Internal error"
        sys.exit(1)

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
