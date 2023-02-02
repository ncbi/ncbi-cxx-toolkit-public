#!/usr/bin/env python3

import os, sys
from optparse import OptionParser


def main():

    parser = OptionParser(
    """
    %prog [input file (html) [output file (hpp)]]
    default input file name: introspection.html
    default output file name: introspection_html.hpp
    """)

    parser.add_option("-v", "--verbose",
                      action="store_true", dest="verbose", default=False,
                      help="be verbose (default: False)")

    options, args = parser.parse_args()
    verbose = options.verbose

    if len(args) not in [0, 1, 2]:
        print('incorrect number of arguments. Expected 0, 1 or 2')
        return 3

    inputFile = 'introspection.html'
    if len(args) > 0:
        inputFile = args[0]

    outputFile = 'introspection_html.hpp'
    if len(args) > 1:
        outputFile = args[1]

    if not os.path.exists(inputFile):
        print('Cannot find file ' + inputFile)
        return 3

    iFile = open(inputFile, 'r')
    try:
        oFile = open(outputFile, 'w')
    except:
        print('Cannot open output file ' + outputFile)
        return 3

    for line in iFile.readlines():
        line = '"' + line.rstrip().replace('"', '\\"') + '\\n"' + '\n'
        oFile.write(line)

    iFile.close()
    oFile.close()
    return 0


if __name__ == '__main__':
    try:
        retVal = main()
    except KeyboardInterrupt:
        retVal = 2
    except Exception as exc:
        print(str(exc))
        retVal = 3

    sys.exit(retVal)

