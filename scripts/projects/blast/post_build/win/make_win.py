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
             "makembindex.exe",
             "blastdb_aliastool.exe",
             "segmasker.exe",
             "dustmasker.exe",
             "windowmasker.exe",
             "convert2blastmask.exe",
             "blastdbcheck.exe",
             "blast_formatter.exe",
             "legacy_blast.pl",
             "update_blastdb.pl" ]
    
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

    # User manual PDF is replaced by README.txt
    f = open("README.txt", "w")
    f.write("Documentation available in http://www.ncbi.nlm.nih.gov/books/NBK1762\n")
    f.close()

    for aux_file in ("EnvVarUpdate.nsh", "ncbilogo.ico"):
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

