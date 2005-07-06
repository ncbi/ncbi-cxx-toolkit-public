# $Id$
#
# Pre-preprocessing for SWIG.
#
# This is a layer of processing before even the SWIG
# preprocessor sees anything.
# The main purpose of all this is to assure that specified headers
# are really %include'd, even if they are %import'ed first, e.g., due to
# the -importall SWIG option.
#
# It takes a three-section file, with the sections separated by
# "%%" lines (*.swig, by convention).  The product is a SWIG
# interface file (optional; *.i by convention) and a special list
# of headers.  The first and last sections are simply passed along
# to the *.i.  The middle section is special.  Its lines can be
# 1. A header file.  This is added to the list of headers.  Both
#    an %include and an inlined #include are put into the *.i.
# 2. "%include foo.swig".  foo.swig gets this treatment recursively.
# 3. A blank line, or one starting with "//".  This is ignored.
# 4. A line beginning with "%ifdef SYMNAME" or "%ifndef SYMNAME".
#    Depending upon whether SYMNAME is in the list of defined
#    symbols, either the rest of the line is processed as above
#    or nothing happens.
#
# Author: Josh Cherry

import re

# Recursive function.
# Returns (part before %%, header_list, part after %%)
def RecursiveProcess(ifname, defined_symbols):
    fid = open(ifname)
    s = fid.read()
    fid.close()

    l = re.split('^%%|\n%%', s)

    lines = l[1].split('\n')
    headers = []
    before = l[0]
    after = l[2]
    for line in lines:
        # Conditional inclusion
        if line.startswith('%ifdef ') or line.startswith('%ifndef '):
            (cond, symbol, rest) = line.split(None, 2)
            if (cond == '%ifdef' and symbol in defined_symbols) \
               or (cond == '%ifndef' and not symbol in defined_symbols):
                line = rest
            else:
                line = ''

        if line.startswith('%include '):
            included_fname = line[len('%include '):]
            included_res = RecursiveProcess(included_fname, defined_symbols)
            before  += included_res[0]
            headers += included_res[1]
            after   += included_res[2]
        elif line != '' and line[0:2] != '//':
            headers.append(line)
    return (before, headers, after)


# outf is a file object for writing, or None
# for callers only interested in a list of headers
def ProcessFile(ifname, outf=None, defined_symbols=[]):
    (before, headers, after) = RecursiveProcess(ifname, defined_symbols)

    if outf != None:
        outf.write('%{\n')
        for header in headers:
            outf.write('#include <' + header + '>\n')
        outf.write('%}\n\n')

        outf.write(before)
        for header in headers:
            outf.write('%include ' + header + '\n')
        outf.write(after)

    return headers


