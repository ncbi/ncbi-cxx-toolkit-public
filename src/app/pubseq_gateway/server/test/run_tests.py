#!/usr/bin/env python3

import sys, os

dirname = os.path.abspath(os.path.dirname(sys.argv[0]))
baselineDir = dirname + os.path.sep + 'baseline'
if not os.path.exists(baselineDir):
    # Need to unpack
    baselineArchive = dirname + os.path.sep + 'baseline.tar.gz'
    if not os.path.exists(baselineArchive):
        print('Cannot find baseline archive ' + baselineArchive)
        sys.exit(1)
    if os.system('tar -xzf ' + baselineArchive + ' -C ' + dirname) != 0:
        print('Error unpacking baseline archive ' + baselineArchive)
        sys.exit(1)
os.system('udc ' + dirname)
