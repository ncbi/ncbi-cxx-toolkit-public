#!/usr/bin/env python
# $Id$
#
# Author: Christiam Camacho
#

import os, sys, os.path
from optparse import OptionParser

verbose = False
scripts_dir = ""

def main():
    """ Creates installers for selected platforms. """
    parser = OptionParser("%prog <platform> " +
                          "<installation directory> <source directory>")
    parser.add_option("-v", "--verbose", action="store_true", default=False,
                      help="Show verbose output")
    options, args = parser.parse_args()
    if len(args) != 3:
        parser.error("Incorrect number of arguments")
        return 1

    platform, installdir, srcdir = args;
    global scripts_dir
    scripts_dir = os.path.join(srcdir, "scripts", "projects", "blast", 
                               "post_build")

    global verbose
    verbose = options.verbose
    if verbose:
        print "Platform:", platform
        print "Installation directory:", installdir
        print "Source directory:", srcdir
        print "Scripts directory:", scripts_dir

    if platform == "Win32":
        return Win32PostBuild(installdir, srcdir)
    if platform == "Win64":
        return do_nothing(platform)
    if platform == "Linux32":
        return Linux32PostBuild(installdir, scripts_dir)
    if platform == "Linux64":
        return Linux64PostBuild(installdir, scripts_dir)
    if platform == "FreeBSD32":
        return do_nothing(platform)
    if platform == "PowerMAC":
        return PowerMACPostBuild(installdir, srcdir)
    if platform == "SunOSSparc":
        return do_nothing(platform)
    if platform == "SunOSx86":
        return do_nothing(platform)
    
    print >> sys.stderr, "Unknown OS identifier: " + platform
    print >> sys.stderr, "Exiting post build script."
    return 2

def Win32PostBuild(installdir, srcdir):
    if verbose: print "Packaging for Win32..."
    cmd = "python.exe " + os.path.join(scripts_dir, "win", "make_win.py") + " "
    cmd += installdir + " " + srcdir
    if verbose: cmd += " -v"
    safe_exec(cmd)
    return 0

def Linux32PostBuild(installdir, scripts_dir):
    if verbose: print "Packing linux 32 bit RPM..."
    cmd = "python " + os.path.join(scripts_dir, "rpm", "make_rpm.py") + " "
    cmd += installdir + " " + scripts_dir
    if verbose: cmd += " -v"
    safe_exec(cmd)
    return 0

def Linux64PostBuild(installdir, scripts_dir):
    if verbose: print "Packing linux 64 bit RPM..."
    cmd = "python " + os.path.join(scripts_dir, "rpm", "make_rpm.py") + " "
    cmd += installdir + " " + scripts_dir
    if verbose: cmd += " -v"
    safe_exec(cmd)
    return 0

def PowerMACPostBuild(installdir, srcdir):
    if verbose: print "Packaging for MacOSX..."
    cmd = os.path.join(scripts_dir, "macosx", "ncbi-blast.sh") + " "
    cmd += installdir + " " + srcdir
    safe_exec(cmd)
    return 0

def do_nothing(platform):
    if verbose: print "No post-build step necessary for platform."
    return 0

def safe_exec(cmd):
    """ Executes a command and checks its return value, throwing an
        exception if it fails.
    """
    import subprocess
    if verbose: print cmd
    try:
        retcode = subprocess.call(cmd, shell=True)
        if retcode < 0:
            raise RuntimeError("Child was terminated by signal " + -retcode)
        elif retcode != 0:
            raise RuntimeError("Command failed with exit code " + retcode)
    except OSError, e:
        raise RuntimeError("Execution failed: " + e)


# The script execution entry point
if __name__ == "__main__":
    sys.exit( main() )

