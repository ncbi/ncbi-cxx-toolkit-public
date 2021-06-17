#!/usr/bin/python

#
# Run this program to convert new style counts files containing version, parameters, and
# version data inforamtion into the old format.
#

import sys

HELP_STR = """
------------------------------------------------------------------------------
WINDOWMASKER COUNTS FILE CONVERTER
------------------------------------------------------------------------------

New windowmasker counts files formats were change to incorporate additional
information, such as version of file formats and algorithms used to compute
the n-mer counts statistics, windowmasker parameters used for file creation, 
and additional user-defined metadata.

Old versions of windowmasker can not parse new counts files. This program
converts the new counts files back to the old format.

USAGE: windowmasker_counts_adapter.py <input> <output>

input  --- the path to a counts file in new format;
output --- the path to the result of the conversion to the old format.
------------------------------------------------------------------------------
"""

def check_type( fname ):
    f = open( fname, 'rb' )
    head = f.read( 4 )
    if len( head ) < 4:
        return 'other'
    if head[0:2] == '##':
        return 'ascii'
    v = [ord( a ) for a in head]
    if v == [0,0,0,3]:
        return 'binary_be'
    if v == [3,0,0,0]:
        return 'binary_le'
    return 'other'

def convert_ascii( in_fname, out_fname ):
    outf = open( out_fname, 'w' )
    for line in file( in_fname ):
        if len( line ) >= 2 and line[0:2] == '##':
            continue
        outf.write( line )
    outf.flush()

def convert_binary( in_fname, out_fname, t ):
    outf = open( out_fname, 'wb' )
    inf = open( in_fname, 'rb' )
    head = inf.read( 4 )
    head = inf.read( 4 )
    if len( head ) < 4:
        sys.exit( 'metadata length error' )
    if t == 'binary_le':
        sz = (ord( head[0] )) + \
             (ord( head[1] )<<8) + \
             (ord( head[2] )<<16) + \
             (ord( head[3] )<<24)
    else:
        sz = (ord( head[3] )) + \
             (ord( head[2] )<<8) + \
             (ord( head[1] )<<16) + \
             (ord( head[0] )<<24)
    head = inf.read( sz )
    if len( head ) < sz:
        sys.exit( 'metadata error' )
    outf.write( inf.read() )
    outf.flush()

def main():
    global HELP_STR
    if len( sys.argv ) < 3:
        print HELP_STR
        sys.exit( 1 )
    (in_fname, out_fname) = (sys.argv[1:3])
    t = check_type( sys.argv[1] )
    if t == "ascii":
        convert_ascii( in_fname, out_fname )
    elif t[0:6] == "binary":
        convert_binary( in_fname, out_fname, t )
    else:
        sys.exit( "input is already in old format" )

if __name__ == '__main__':
    main()

