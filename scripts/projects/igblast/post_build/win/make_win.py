#! /usr/bin/env python
"""Script to create the Windows installer for BLAST command line applications"""
# $Id$
#
# Author: Christiam camacho

import os, sys, os.path
import shutil
from optparse import OptionParser
SCRIPT_DIR = os.path.dirname(os.path.abspath(sys.argv[0]))
sys.path.append(os.path.join(SCRIPT_DIR, ".."))
from blast_utils import safe_exec, update_blast_version

VERBOSE = False
    
# NSIS Configuration file
NSIS_CONFIG = os.path.join(SCRIPT_DIR, "ncbi-blast.nsi")

def extract_installer():
    """Extract name of the installer file from NSIS configuration file"""
    from fileinput import FileInput

    retval = "unknown"
    for line in FileInput(NSIS_CONFIG):
        if line.find("OutFile") != -1:
            retval = line.split()[1]
            return retval.strip('"')

def main():
    """ Creates NSIS installer for BLAST command line binaries """
    global VERBOSE #IGNORE:W0603
    parser = OptionParser("%prog <blast_version> <installation directory>")
    parser.add_option("-v", "--verbose", action="store_true", default=False,
                      help="Show verbose output", dest="VERBOSE")
    options, args = parser.parse_args()
    if len(args) != 2:
        parser.error("Incorrect number of arguments")
        return 1
    
    blast_version, installdir = args
    VERBOSE = options.VERBOSE
    
    apps = [ "igblastn.exe", "igblastp.exe" ]
    
    cwd = os.getcwd()
    for app in apps:
        app = os.path.join(installdir, "bin", app)
        if VERBOSE: 
            print "Copying", app, "to", cwd
        shutil.copy(app, cwd)
    
    
    update_blast_version(NSIS_CONFIG, blast_version)
    # Copy necessary files to the current working directory
    shutil.copy(NSIS_CONFIG, cwd)
    license_file = os.path.join(SCRIPT_DIR, "..", "..", "LICENSE")
    shutil.copy(license_file, cwd)

    # Copy the README file from the parent directory
    readme_file = os.path.join(SCRIPT_DIR, "..", "..", "README")
    shutil.copy(readme_file, cwd)

    # Copy the data directories, make sure there are no .svn directories
    for directory in ("internal_data", "optional_file"):
        src = os.path.join(SCRIPT_DIR, "..", "..", "..", ".." \
                           "src", "app", "igblast", directory)
        if VERBOSE:
            print "Copying", src, "to", cwd
        shutil.copytree(src, cwd)
    for root, dirs, files in os.walk(cwd):
        for subdir in dirs:
            path = os.path.join(root, subdir)
            if subdir == ".svn":
                if VERBOSE:
                    print "Removing ", path
                shutil.rmtree(path)

    for aux_file in ("EnvVarUpdate.nsh", "unix2dos.nsh", "ncbilogo.ico"):
        src = os.path.join(SCRIPT_DIR, aux_file)
        if VERBOSE:
            print "Copying", src, "to", cwd
        shutil.copy(src, cwd)
        
    # makensis is in the path of the script courtesy of the release framework
    cmd = "makensis " + os.path.basename(NSIS_CONFIG)
    safe_exec(cmd)

    installer_dir = os.path.join(installdir, "installer")
    if not os.path.exists(installer_dir):
        os.makedirs(installer_dir)

    installer = extract_installer()
    shutil.copy(installer, installer_dir)

if __name__ == "__main__":
    sys.exit(main())

