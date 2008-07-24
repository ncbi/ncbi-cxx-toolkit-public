#! /usr/bin/env python
"""Script to create the Windows installer for BLAST command line applications"""
# $Id$
#
# Author: Christiam camacho

import os, sys, os.path
from shutil import copy
from optparse import OptionParser

VERBOSE = False
    
# NSIS Configuration file
NSIS_CONFIG = os.path.join(os.path.dirname(os.path.abspath(sys.argv[0])), \
                           "ncbi-blast.nsi")

def safe_exec(cmd):
    """ Executes a command and checks its return value, throwing an
        exception if it fails.
    """
    import subprocess
    if VERBOSE: print cmd
    try:
        retcode = subprocess.call(cmd, shell=True)
        if retcode < 0:
            raise RuntimeError("Child was terminated by signal " + str(-retcode))
        elif retcode != 0:
            raise RuntimeError("Command failed with exit code " + str(retcode))
    except OSError, err:
        raise RuntimeError("Execution failed: " + err)

def extract_installer():
    from fileinput import FileInput

    retval = "unknown"
    for line in FileInput(NSIS_CONFIG):
        if line.find("OutFile") != -1:
            retval = line.split()[1]
            return retval.strip('"')

def main():
    """ Creates NSIS installer for BLAST command line binaries """
    global VERBOSE
    parser = OptionParser("%prog <installation directory> <source directory>")
    parser.add_option("-v", "--verbose", action="store_true", default=False,
                      help="Show verbose output", dest="VERBOSE")
    options, args = parser.parse_args()
    if len(args) != 2:
        parser.error("Incorrect number of arguments")
        return 1
    
    installdir, srcdir = args
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
        copy(app, cwd)
    
    license_file = os.path.join(srcdir, "scripts", "projects", "blast",
                                "LICENSE")
    copy(license_file, cwd)
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

