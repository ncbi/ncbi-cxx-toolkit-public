#!/usr/bin/env python
"""Driver program for post-build processing"""
# $Id$
#
# Author: Christiam Camacho
#

import os, sys, os.path
from optparse import OptionParser

VERBOSE = False
SCRIPTS_DIR = ""

def main():
    """ Creates installers for selected platforms. """
    parser = OptionParser("%prog <platform> " +
                          "<installation directory> <source directory>")
    parser.add_option("-v", "--verbose", action="store_true", default=False,
                      help="Show verbose output", dest="verbose")
    options, args = parser.parse_args()
    if len(args) != 3:
        parser.error("Incorrect number of arguments")
        return 1

    platform, installdir, srcdir = args
    global SCRIPTS_DIR
    SCRIPTS_DIR = os.path.join(srcdir, "scripts", "projects", "blast", 
                               "post_build")

    global VERBOSE
    VERBOSE = options.VERBOSE
    if VERBOSE:
        print "Platform:", platform
        print "Installation directory:", installdir
        print "Source directory:", srcdir
        print "Scripts directory:", SCRIPTS_DIR

# TODO: Try building installer on win64
    if platform == "Win32":
        #return win32_post_build(installdir, srcdir)
        return do_nothing(platform)
    if platform == "Win64":
        return win32_post_build(installdir, srcdir)
        #return do_nothing(platform)
    if platform == "Linux32":
        return linux32_post_build(installdir)
    if platform == "Linux64":
        return linux64_post_build(installdir)
    if platform == "FreeBSD32":
        return do_nothing(platform)
    if platform == "PowerMAC":
        return mac_post_build(installdir, srcdir)
    if platform == "SunOSSparc":
        return do_nothing(platform)
    if platform == "SunOSx86":
        return do_nothing(platform)
    
    print >> sys.stderr, "Unknown OS identifier: " + platform
    print >> sys.stderr, "Exiting post build script."
    return 2

def win32_post_build(installdir, srcdir):
    '''Win32 post-build: create installer'''
    if VERBOSE: print "Packaging for Win32..."
    cmd = "python.exe " + os.path.join(SCRIPTS_DIR, "win", "make_win.py") + " "
    cmd += installdir + " " + srcdir
    if VERBOSE: cmd += " -v"
    safe_exec(cmd)
    return 0

def linux32_post_build(installdir):
    '''Linux32 post-build: create RPM'''
    if VERBOSE: print "Packing linux 32 bit RPM..."
    cmd = "python " + os.path.join(SCRIPTS_DIR, "rpm", "make_rpm.py") + " "
    cmd += installdir + " " + SCRIPTS_DIR
    if VERBOSE: cmd += " -v"
    safe_exec(cmd)
    return 0

def linux64_post_build(installdir):
    '''Linux64 post-build: create RPM'''
    if VERBOSE: print "Packing linux 64 bit RPM..."
    cmd = "python " + os.path.join(SCRIPTS_DIR, "rpm", "make_rpm.py") + " "
    cmd += installdir + " " + SCRIPTS_DIR
    if VERBOSE: cmd += " -v"
    safe_exec(cmd)
    return 0

def mac_post_build(installdir, srcdir):
    '''MacOSX post-build: create installer'''
    if VERBOSE: print "Packaging for MacOSX..."
    cmd = os.path.join(SCRIPTS_DIR, "macosx", "ncbi-blast.sh") + " "
    cmd += installdir + " " + srcdir
    safe_exec(cmd)
    return 0

def do_nothing(platform):
    '''No op function'''
    if VERBOSE: print "No post-build step necessary for", platform
    return 0

def safe_exec(cmd):
    """ Executes a command and checks its return value, throwing an
        exception if it fails.
    """
    import subprocess
    if VERBOSE: print cmd
    try:
        retcode = subprocess.call(cmd, shell=True)
        if retcode < 0:
            raise RuntimeError("Child was terminated by signal " + -retcode)
        elif retcode != 0:
            raise RuntimeError("Command failed with exit code " + retcode)
    except OSError, err:
        raise RuntimeError("Execution failed: " + err)


# The script execution entry point
if __name__ == "__main__":
    sys.exit( main() )

