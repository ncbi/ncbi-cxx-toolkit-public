#! /usr/bin/env python
"""Script to create the Windows installer for BLAST command line applications"""
# $Id$
#
# Author: Christiam camacho

import os, sys, os.path
from shutil import copy
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
    parser = OptionParser("%prog <installation directory>")
    parser.add_option("-v", "--verbose", action="store_true", default=False,
                      help="Show verbose output", dest="VERBOSE")
    options, args = parser.parse_args()
    if len(args) != 1:
        parser.error("Incorrect number of arguments")
        return 1
    
    installdir = args[0]
    VERBOSE = options.VERBOSE
    
    apps = [ "blastn.exe", 
             "blastp.exe",
             "blastx.exe",
             "tblastx.exe",
             "tblastn.exe",
             "rpsblast.exe",
             "rpstblastn.exe",
             "psiblast.exe",
             "blastdbcmd.exe",
             "makeblastdb.exe",
             "blastdb_aliastool.exe",
             "segmasker.exe",
             "dustmasker.exe",
             "windowmasker.exe",
             "legacy_blast.pl" ]
    
    cwd = os.getcwd()
    for app in apps:
        app = os.path.join(installdir, "bin", app)
        if VERBOSE: 
            print "Copying", app, "to", cwd
        copy(app, cwd)
    
    license_file = os.path.join(SCRIPT_DIR, "..", "..", "LICENSE")
    copy(license_file, cwd)
    update_blast_version(NSIS_CONFIG)
    copy(NSIS_CONFIG, cwd)
    # makensis is in the path of the script courtesy of the release framework
    cmd = "makensis " + os.path.basename(NSIS_CONFIG)
    safe_exec(cmd)

    installer_dir = os.path.join(installdir, "installer")
    if not os.path.exists(installer_dir):
        os.makedirs(installer_dir)

    installer = extract_installer()
    copy(installer, installer_dir)

if __name__ == "__main__":
    sys.exit(main())

