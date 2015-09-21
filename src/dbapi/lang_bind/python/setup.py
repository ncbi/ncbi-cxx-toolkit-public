#!/usr/bin/env python

#                           PUBLIC DOMAIN NOTICE
#              National Center for Biotechnology Information
#
# This software/database is a "United States Government Work" under the
# terms of the United States Copyright Act.  It was written as part of
# the author's official duties as a United States Government employee and
# thus cannot be copyrighted.  This software/database is freely available
# to the public for use. The National Library of Medicine and the U.S.
# Government have not placed any restriction on its use or reproduction.
#
# Although all reasonable efforts have been taken to ensure the accuracy
# and reliability of the software and data, the NLM and the U.S.
# Government do not and cannot warrant the performance or results that
# may be obtained by using this software or data. The NLM and the U.S.
# Government disclaim all warranties, express or implied, including
# warranties of performance, merchantability or fitness for any particular
# purpose.
#
# Please cite the author in any work or product based on this material.

import glob
import os
import re
import shutil
import sys
try:
    from setuptools import setup, Extension
except ImportError:
    from distutils.core import setup, Extension
from distutils.sysconfig import parse_makefile, expand_makefile_vars

try:
    ncbi = os.environ['NCBI']
except KeyError:
    ncbi = '/netopt/ncbi_tools64'
def_buildroot = ncbi + '/c++/ReleaseMT'
buildroot = def_buildroot
support_dir = 'python_ncbi_dbapi_support'

# Work around setuptools' lack of support for custom global arguments
# besides boolean --with(out)-* flags, which are insufficiently
# expressive and have a limited effect.

w_cxx='--with-c++-build='

filtered_args = []
for a in sys.argv:
    if a.startswith(w_cxx):
        buildroot=a[len(w_cxx):]
    elif a == '--help':
        filtered_args.append(a)
        print("""In addition to the standard options described below,
this script supports one custom option:

  --with-c++-build=DIR Build against the C++ Toolkit installation in DIR
                       (default: %s)
""" % (def_buildroot,))
    else:
        filtered_args.append(a)
sys.argv = filtered_args

vv = parse_makefile(buildroot + '/build/Makefile.mk')
def mfv(v):
    return expand_makefile_vars('$(' + v + ')', vv)

unparsed_rx = re.compile(r'\$\(.*?\)')
for v in ('CXX', 'CXXFLAGS'):
    os.environ.setdefault(v, unparsed_rx.sub('', mfv(v)))
os.environ.setdefault('CPPFLAGS',
                      unparsed_rx.sub('', mfv('CPPFLAGS'))
                      + ' -DPYDBAPI_SUPPORT_DIR=\\"' + support_dir + '\\"'
                      + ' -I' + mfv('top_srcdir')
                      + '/src/dbapi/lang_bind/python')
os.environ.setdefault('LDFLAGS', mfv('RUNPATH_ORIGIN') + '/' + support_dir)

libs = ['dbapi', 'dbapi_driver' + mfv('DLL'), mfv('XCONNEXT'), 'xconnect',
        'xutil', 'xncbi']

ext = Extension('python_ncbi_dbapi', ['python_ncbi_dbapi.cpp'],
                library_dirs = [mfv('libdir')],
                libraries    = libs)

# Arrange to incorporate needed shared libraries and drivers.
lib_files = []
for x in libs + ['ncbi_xdbapi_*' + mfv('DLL'), '*_ftds??']:
    for y in glob.glob(mfv('libdir') + '/lib' + x + '.??*'):
        lib_files.append(os.path.basename(y))

setup(
    name         = 'python_ncbi_dbapi',
    version      = '1.15.2',
    description  = 'NCBI DBAPI wrapper',
    author       = 'Aaron Ucko',
    author_email = 'ucko@ncbi.nlm.nih.gov',
    ext_modules  = [ext],
    packages     = [support_dir],
    package_data = {support_dir: lib_files},
    package_dir  = {support_dir: mfv('libdir')}
)
