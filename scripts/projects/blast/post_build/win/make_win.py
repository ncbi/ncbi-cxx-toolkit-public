#! /usr/bin/env python

import os, sys
from shutil import copyfile
from subprocess import call
from optparse import OptionParser

# NSIS Configuration file
nsis_config = "ncbi-blast.nsi"

def safe_exec(cmd):
    """ Executes a command and checks its return value, throwing an
        exception if it fails.
    """
    import subprocess
    try:
        retcode = subprocess.call(cmd, shell=True)
        if retcode < 0:
            raise RuntimeError("Child was terminated by signal " + -retcode)
    except OSError, e:
        raise RuntimeError("Execution failed: " + e)

def extract_installer():
    global nsis_config
    from fileinput import FileInput
    retval = "unknown"
    f = FileInput(nsis_config)
    for line in f:
        if line.find("OutFile") != -1:
            retval = line.split()[1]
            return retval.strip('"')

def main():
    """ Creates NSIS installer for BLAST command line binaries """
    global nsis_config
    parser = OptionParser(sys.argv[0] + 
                          " <installation directory> <source directory>")
    options, args = parser.parse_args()
    if len(args) != 2:
        parser.error("Incorrect number of arguments")
        return 1
    
    installdir, srcdir = args
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
             "windowmasker.exe" ];
    
    cwd = os.getcwd()
    for app in apps:
        app = os.path.join(installdir, "bin", app)
        copyfile(app, cwd)
    
    license = os.path.join(srcdir, "scripts", "projects", "blast", "LICENSE")
    copyfile(license, cwd)
    
    cmd = "\\snowman\win-coremake\App\ThirdParty\NSIS\makensis " + nsis_config
    safe_exec(cmd)

    installer_dir = os.path.join(installdir, "installer")
    if not os.path.exists(installer_dir):
        os.makedirs(installer_dir)

    installer = extract_installer()
    copyfile(installer, installer_dir)

if __name__ == "__main__":
    sys.exit(main())

