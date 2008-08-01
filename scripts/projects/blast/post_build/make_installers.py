#!/usr/bin/env python
"""Driver program for post-build processing"""
# $Id$
#
# Author: Christiam Camacho
#

import os, sys, os.path
from optparse import OptionParser
from blast_utils import safe_exec, BLAST_VERSION

VERBOSE = False
SCRIPTS_DIR = os.path.dirname(os.path.abspath(sys.argv[0]))

def main(): #IGNORE:R0911
    """ Creates installers for selected platforms. """
    parser = OptionParser("%prog <platform> <installation directory>")
    parser.add_option("-v", "--verbose", action="store_true", default=False,
                      help="Show verbose output", dest="VERBOSE")
    options, args = parser.parse_args()
    if len(args) != 2:
        parser.error("Incorrect number of arguments")
        return 1

    platform, installdir = args

    global VERBOSE #IGNORE:W0603
    VERBOSE = options.VERBOSE
    if VERBOSE:
        print "Platform:", platform
        print "Installation directory:", installdir

    if platform == "Win32":
        return launch_win_installer_build(installdir)                
    if platform == "Win64":
        return launch_win_installer_build(installdir)        
    if platform == "Linux32":
        return launch_rpm_build(installdir)
    if platform == "Linux64":
        return launch_rpm_build(installdir)
    if platform == "FreeBSD32":
        return do_nothing(platform)
    if platform == "PowerMAC":
        return mac_post_build(installdir)
    if platform == "SunOSSparc":
        return do_nothing(platform)
    if platform == "SunOSx86":
        return do_nothing(platform)
    
    print >> sys.stderr, "Unknown OS identifier: " + platform
    print >> sys.stderr, "Exiting post build script."
    return 2

def launch_win_installer_build(installdir):
    '''Windows post-build: create installer'''
    if VERBOSE: 
        print "Packaging for Windows..."
    cmd = "python " + os.path.join(SCRIPTS_DIR, "win", "make_win.py") + " "
    cmd += installdir
    if VERBOSE: 
        cmd += " -v"
    safe_exec(cmd)
    return 0

def launch_rpm_build(installdir):
    '''Linux post-build: create RPM'''
    if VERBOSE: 
        print "Packing linux RPM..."
    cmd = "python " + os.path.join(SCRIPTS_DIR, "rpm", "make_rpm.py") + " "
    cmd += installdir
    if VERBOSE: 
        cmd += " -v"
    safe_exec(cmd)
    return 0

def mac_post_build(installdir):
    '''MacOSX post-build: create installer'''
    if VERBOSE:
        print "Packaging for MacOSX..."
    script_dir = os.path.join(SCRIPTS_DIR, "macosx")
    cmd = os.path.join(script_dir, "ncbi-blast.sh") + " "
    cmd += installdir + " " + script_dir + " " + BLAST_VERSION
    safe_exec(cmd)
    return 0

def do_nothing(platform):
    '''No op function'''
    print "No post-build step necessary for", platform
    return 0

# The script execution entry point
if __name__ == "__main__":
    sys.exit( main() )

