#!/usr/bin/env python
# $Id$

import glob, os, sys

# Clean up after broad manifest globbing, which overshoots due to
# needing to match both Unix and Windows library filenames.
for x in glob.glob(sys.argv[1] + '/lib/*xxconnect*'):
    os.unlink(x)
os.unlink(sys.argv[1] + '/lib/python_ncbi_dbapi.so')

# Build a tarball for system-wide installation only on Linux.
if sys.platform == 'linux2':
    build_tarball = os.path.dirname(sys.argv[0]) + '/build_tarball.sh'
    os.execv(build_tarball, [build_tarball] + sys.argv[1:])
