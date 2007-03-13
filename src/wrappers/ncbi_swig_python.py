#!/usr/bin/env python

# $Id$
#
# Author: Josh Cherry
#
# Read a specially formatted input file for SWIG.  Create a
# SWIG .i file, and record certain special header files
# that are to be %include'd, even if they're %import'ed first.
# Run the SWIG preprocessor on the .i, with the -importall
# option.  Change any %import's of the special headers
# to %include's.  Feed the result to SWIG, using the
# -nopreprocess switch.


import sys, os, re
import sppp

# Process command-line arguments.
# There can be any number of arguments that start with "-D".
# These specify symbols to define for the pre-preprocessor.
# There must be exactly one other argument, which specifies
# the input file.

defined_symbols = [s[2:] for s in sys.argv[1:] if s.startswith('-D')]
other_args = [s for s in sys.argv[1:] if not s.startswith('-D')]
if len(other_args) != 1:
    sys.stderr.write('usage: %s [-DSYMBOL ...] input_file\n' % sys.argv[0])
    sys.exit(1)
ifname = other_args[0]                                # our input
swig_input_fname = os.path.splitext(ifname)[0] + '.i' # our output (SWIG input)

# read input file and make swig interface file

outf = open(swig_input_fname, 'w')
headers = sppp.ProcessFile(ifname, outf, defined_symbols)
outf.close()

# Run swig preprocessor
cline = '''
%s -E -python -c++ -modern -importall -ignoremissing \
-w201,312,350,351,361,362,383,394,401,503,504,508,510 \
-I./dummy_headers -I./python \
-I%s -I%s \
%s > %s \
''' % (os.environ['SWIG'], os.environ['NCBI_INC'], os.environ['NCBI_INCLUDE'],
       swig_input_fname, swig_input_fname + '.pp')
status = os.system(cline)
if status:
    sys.exit(status)


# Modify preprocessor output
fid = open(swig_input_fname + '.pp')
s = fid.read()
fid.close()
lines = s.split('\n')
mod_pp = open(swig_input_fname + '.mpp', 'w')
for line in lines:
    if line.startswith('%importfile'):
        m = re.search('%importfile "(.*)"', line)
        fname = m.groups()[0]
        # Convert Windows path names to UNIX-style
        fname = fname.replace('\\', '/').replace('//', '/')  
        for header in headers:
            if fname.endswith(header):
                line = line.replace('%importfile', '%includefile', 1)
                break
    mod_pp.write(line + '\n')
mod_pp.close()

import ncbi_modify
s = open(swig_input_fname + '.mpp').read()
s_mod = ncbi_modify.Modify(s)
open(swig_input_fname + '.mpp', 'w').write(s_mod)

# Run swig on modified preprocessor output
ofname = os.path.splitext(ifname)[0] + '_wrap.cpp'
cline = '''
%s -o %s -nopreprocess \
-python -v -c++ -modern -importall -ignoremissing -noextern \
-w201,312,350,351,361,362,383,394,401,503,504,508,510 \
-I./dummy_headers -I./python \
-I%s -I%s \
%s \
''' % (os.environ['SWIG'], ofname, os.environ['NCBI_INC'],
       os.environ['NCBI_INCLUDE'],
       swig_input_fname + '.mpp')
status = os.system(cline)
#if status:
#    sys.exit(status)
sys.exit(0)
