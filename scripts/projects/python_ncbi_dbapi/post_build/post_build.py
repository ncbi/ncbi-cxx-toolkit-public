#!/usr/bin/env python
# $Id$

import glob, os, shutil, subprocess, sys

(bin_dir, lib_dir, install_dir, src_dir) = sys.argv[1:]
common_stems  = ('python_ncbi_dbapi', 'ncbi_xdbapi_*')
unix_stems    = ('dbapi', 'dbapi_driver', 'ct_ftds64', 'tds_ftds64',
                 'xconne?t', 'xutil', 'xncbi')
windows_stems = ('ncbi_core', 'ncbi_dbapi', 'ncbi_dbapi_driver')
installations = ('2.7env',)

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

# Build wheels only on Linux (currently the only supported platform anyway).
if sys.platform == 'linux2':
    proj_src_dir = src_dir + '/src/dbapi/lang_bind/python'
    tmp_dir = install_dir + '/tmp'
    cpp_build = os.path.dirname(os.path.abspath(lib_dir))
    #build_tarball = os.path.dirname(sys.argv[0]) + '/build_tarball.sh'
    #os.execv(build_tarball, [build_tarball, install_dir, version])
    for x in installations:
        py_bin_dir = '/opt/python-' + x + '/bin'
        if os.path.isdir(tmp_dir):
            shutil.rmtree(tmp_dir)
        os.mkdir(tmp_dir)
        if 0 != subprocess.call((py_bin_dir + '/pip', 'install', '-t', tmp_dir,
                                 'packit')):
            raise RuntimeError('Error installing packit')
        for x in ('python_ncbi_dbapi.cpp', 'python_ncbi_dbapi.hpp',
                  'setup.py'):
            shutil.copy(proj_src_dir + '/' + x, tmp_dir + '/' + x)
        shutil.copytree(proj_src_dir + '/pythonpp', tmp_dir + '/pythonpp')
        orig_cwd = os.getcwd()
        os.chdir(tmp_dir)
        if 0 != subprocess.call((py_bin_dir + '/python', 'setup.py',
                                 '--with-c++-build=' + cpp_build,
                                 '--command-packages', 'packit',
                                 'bdist_wheel')):
            raise RuntimeError('Error running setup.py')
        os.chdir(orig_cwd)
        wheels = glob.glob(tmp_dir + '/dist/*.whl')
        if len(wheels) != 1:
            raise RuntimeError('Expected to get exactly 1 wheel, but got '
                               + len(wheels))
        shutil.move(wheels[0], install_dir)
        shutil.rmtree(tmp_dir)
