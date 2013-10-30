#!/usr/bin/env python
# $Id$

import glob, os, shutil, sys

(bin_dir, lib_dir, install_dir, version) = sys.argv[1:]
common_stems  = ('python_ncbi_dbapi', 'ncbi_xdbapi_?t*')
unix_stems    = ('dbapi', 'dbapi_driver', 'ct_ftds64', 'tds_ftds64',
                 'xconne?t', 'xutil', 'xncbi')
windows_stems = ('ncbi_core', 'ncbi_dbapi', 'ncbi_dbapi_driver')

os.mkdir(install_dir + '/lib')
if os.path.exists(lib_dir + '/python_ncbi_dbapi.so'):
    stems    = common_stems + unix_stems
    template = lib_dir.replace('%', '%%') + '/lib%s.[ds]?*'
else:
    stems    = common_stems + windows_stems
    template = bin_dir.replace('%', '%%') + '/%s.[dp]??'
    
for s in stems:
    for x in glob.glob(template % s):
        shutil.copy(x, install_dir + '/lib/' + os.path.basename(x))

# Build a tarball for system-wide installation only on Linux.
if sys.platform == 'linux2':
    build_tarball = os.path.dirname(sys.argv[0]) + '/build_tarball.sh'
    os.execv(build_tarball, [build_tarball, install_dir, version])
