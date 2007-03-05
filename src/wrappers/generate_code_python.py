#!/usr/bin/env python

# $Id$
#
# This runs SWIG once or twice as necessary.
# An empty ncbi.py is created if none exists.  Templates and CObject's are
# extracted using the existing ncbi.py, and SWIG is run.  Templates and
# CObject's are extracted again, using the ncbi.py just created by SWIG.
# SWIG is run again if and only if templates or CObject's have changed.
#
# Author: Josh Cherry

import sys, os, glob

python_exe = os.environ['PYTHON_EXE']

# Version info (version.i)
import time
date_str = time.strftime('%a %b %d %H:%M:%S %Z %Y')
open('version.i', 'w').write('%%constant version = "built %s using %s";'
                             % (date_str, os.environ['NCBI_CPP']))

# If ncbi.py does not exist, create it
if len(glob.glob('ncbi.py')) == 0:
    print 'ncbi.py does not exist; creating an empty file'
    open('ncbi.py', 'w')


# Run everything using existing ncbi.py

print 'Extracting templates...'; sys.stdout.flush()
os.system('%s extract_some_templates.py -DWINDOWS ncbi.swig' % python_exe)

print "Creating macro calls for CObject's..."; sys.stdout.flush()
os.system('%s make_cobjects.py ncbi.py' % python_exe)

print "Running SWIG to generate code..."; sys.stdout.flush()
os.system('%s ncbi_swig_python.py -DWINDOWS ncbi.swig' % python_exe)


# Extract stuff again using the new ncbi.py, and check whether it has changed

print 'Extracting info from new ncbi.py:'

try:
    os.remove('old_ncbi_cobjects.i')
except:
    pass
try:
    os.remove('old_ncbi_templates.i')
except:
    pass

os.rename('ncbi_cobjects.i', 'old_ncbi_cobjects.i')
os.rename('ncbi_templates.i', 'old_ncbi_templates.i')


print 'Extracting templates...'; sys.stdout.flush()
os.system('%s extract_some_templates.py -DWINDOWS ncbi.swig' % python_exe)

print "Creating macro calls for CObject's..."; sys.stdout.flush()
os.system('%s make_cobjects.py ncbi.py' % python_exe)

templates_changed = open('ncbi_templates.i').read() != open('old_ncbi_templates.i').read()
cobjects_changed = open('ncbi_cobjects.i').read() != open('old_ncbi_cobjects.i').read()


# If necessary, run SWIG again

if not (templates_changed or cobjects_changed):
    print 'ncbi_templates.i and ncbi_cobjects.i unchanged; no second SWIG run necessary'
    sys.exit(0)
else:
    if templates_changed:
        print 'ncbi_templates.i changed'
    if cobjects_changed:
        print 'ncbi_cobjects.i changed'
    print 'Running SWIG again...'; sys.stdout.flush()
    os.system('%s ncbi_swig_python.py -DWINDOWS ncbi.swig' % python_exe)
