#!/usr/bin/env python
# $Id$
#
# Author: Christiam Camacho
#

import os, sys, os.path
from optparse import OptionParser

verbose = False

def main():
    """ Creates installers for selected platforms. """
    parser = OptionParser(sys.argv[0] + " <platform> " +
                          "<installation directory> <source directory>")
    parser.add_option("-v", "--verbose", action="store_true", default=False,
                      help="Show verbose output")
    options, args = parser.parse_args()
    if len(args) != 3:
        parser.error("Incorrect number of arguments")
        return 1

    platform, installdir, srcdir = args;

    global verbose
    verbose = options.verbose
    if verbose:
        print "Platform:", platform
        print "Installation directory:", installdir
        print "Source directory:", srcdir

    if platform == "Win32":
        return Win32PostBuild(installdir, srcdir)
    if platform == "Win64":
        return Win64PostBuild(installdir, srcdir)
    if platform == "Linux32":
        return Linux32PostBuild(installdir, srcdir)
    if platform == "Linux64":
        return Linux64PostBuild(installdir, srcdir)
    if platform == "FreeBSD32":
        return FreeBSD32PostBuild(installdir, srcdir)
    if platform == "PowerMAC":
        return PowerMACPostBuild(installdir, srcdir)
    if platform == "SunOSSparc":
        return SunOSSparcPostBuild(installdir, srcdir)
    if platform == "SunOSx86":
        return SunOSx86PostBuild(installdir, srcdir)
    
    print >> sys.stderr, "Unknown OS identifier: " + platform
    print >> sys.stderr, "Exiting post build script."
    return 2

def Win32PostBuild(installdir, srcdir):
    if verbose: print "Packaging for Win32..."
    cmd = os.path.join("win", "make_win.py") + " " + installdir + " " + srcdir
    safe_exec(cmd)
    return 0

def Win64PostBuild(installdir, srcdir):
    if verbose: print "No packaging necessary for Win64."
    return 0

def Linux32PostBuild(installdir, srcdir):
    if verbose: print "Packing linux 32 bit RPM..."
    cmd = os.path.join("rpm", "make_rpm.py") + " " + installdir + " " + srcdir
    safe_exec(cmd)
    return 0

def Linux64PostBuild(installdir, srcdir):
    if verbose: print "Packing linux 64 bit RPM..."
    cmd = os.path.join("rpm", "make_rpm.py") + " " + installdir + " " + srcdir
    safe_exec(cmd)
    return 0

def FreeBSD32PostBuild(installdir, srcdir):
    print "Post build packaging is not required on Free BSD 32"
    return 0

def PowerMACPostBuild(installdir, srcdir):
    if verbose: print "Packaging for MacOSX..."
    cmd = os.path.join("macosx", "ncbi-blast.sh") + " " + installdir + " "
    cmd += srcdir
    safe_exec(cmd)
    return 0

def SunOSSparcPostBuild(installdir, srcdir):
    print "Post build packaging is not required on SUN OS Sparc"
    return 0

def SunOSx86PostBuild(installdir, srcdir):
    print "Post build packaging is not required on SUN OS x86"
    return 0

def safe_exec(cmd):
    """ Executes a command and checks its return value, throwing an
        exception if it fails.
    """
    from subprocess import *
    try:
        retcode = call(cmd, shell=True)
        if retcode < 0:
            raise RuntimeError("Child was terminated by signal " + -retcode)
    except OSError, e:
        raise RuntimeError("Execution failed: " + e)


# The script execution entry point
if __name__ == "__main__":
    sys.exit( main() )

