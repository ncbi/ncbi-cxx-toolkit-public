#!/usr/bin/env python3
"""Driver program for post-build processing"""
# $Id$
#
# Author: Christiam Camacho
#

from __future__ import print_function
import os, sys, os.path
from optparse import OptionParser
import blast_utils
import shutil

VERBOSE = False
SCRIPTS_DIR = os.path.dirname(os.path.abspath(sys.argv[0]))

def main(): #IGNORE:R0911
    """ Creates installers for selected platforms. """
    parser = OptionParser("%prog <blast_version> <platform> \
                          <installation directory> \"<src-tarball>\"")
    parser.add_option("-v", "--verbose", action="store_true", default=False,
                      help="Show verbose output", dest="VERBOSE")
    options, args = parser.parse_args()
    if len(args) != 5:
        parser.error("Incorrect number of arguments")
        return 1

    blast_version, platform, installdir, srctarball, libdir = args

    global VERBOSE #IGNORE:W0603
    VERBOSE = options.VERBOSE
    if VERBOSE:
        print("BLAST version", blast_version)
        print("Platform:", platform)
        print("Installation directory:", installdir)
        print("Source tarball:", srctarball)
        print("Lib directory:", libdir)

    if platform.startswith("Win"):
        # copy ncbi-vdb-md.dll
        shutil.copy(libdir + "\ncbi-vdb-md.dll", installdir)
#        return launch_win_installer_build(installdir, blast_version)
        return do_nothing(platform)

    if platform.startswith("Linux64"):
        return launch_rpm_build(installdir, blast_version, srctarball)
   if platform == "FreeBSD32" or platform.startswith("SunOS") or \
       platform.startswith("Linux32"):
        return do_nothing(platform)
    if platform == "IntelMAC":
#        return mac_post_build(installdir, blast_version)
        return do_nothing(platform)

    
    print("Unknown OS identifier:" + platform, file=sys.stderr)
    print("Exiting post build script.", file=sys.stderr)
    return 2

def launch_win_installer_build(installdir, blast_version):
    '''Windows post-build: create installer'''
    if VERBOSE: 
        print("Packaging for Windows...")
    cmd = "python " + os.path.join(SCRIPTS_DIR, "win", "make_win.py") + " "
    cmd += blast_version + " " + installdir
    if VERBOSE: 
        cmd += " -v"
    blast_utils.safe_exec(cmd)
    return 0

def launch_rpm_build(installdir, blast_version, srctarball):
    '''Linux post-build: create RPM'''
    if VERBOSE: 
        print("Packing linux RPM...")
    cmd = "python3 " + os.path.join(SCRIPTS_DIR, "rpm", "make_rpm.py") + " "
    cmd += blast_version + " " + installdir + " " + srctarball
    if VERBOSE: 
        cmd += " -v"
    if len(srctarball) > 2:     # Skip for local builds
        blast_utils.safe_exec(cmd)
    return 0

def mac_post_build(installdir, blast_version):
    '''MacOSX post-build: create installer'''
    if VERBOSE:
        print("Packaging for MacOSX...")
    script_dir = os.path.join(SCRIPTS_DIR, "macosx")
    cmd = os.path.join(script_dir, "ncbi-blast.sh") + " "
    cmd += installdir + " " + script_dir + " " + blast_version
    blast_utils.safe_exec(cmd)
    return 0

def do_nothing(platform):
    '''No op function'''
    print("No post-build step necessary for", platform)
    return 0

# The script execution entry point
if __name__ == "__main__":
    sys.exit( main() )

