#!/usr/bin/env python
""" Various utilities/tools for BLAST """

__all__ = [ "safe_exec", "update_blast_version" ]

import os
import subprocess
import shutil
import platform
        
def safe_exec(cmd):
    """ Executes a command and checks its return value, throwing an
        exception if it fails.
    """   
    try:
        msg = "Command: '" + cmd + "' "
        retcode = subprocess.call(cmd, shell=True)
        if retcode < 0:
            msg += "Termined by signal " + str(-retcode)
            raise RuntimeError(msg)
        elif retcode != 0:
            msg += "Failed with exit code " + str(retcode)
            raise RuntimeError(msg)
    except OSError, err:
        msg += "Execution failed: " + err
        raise RuntimeError(msg)

def update_blast_version(config_file, blast_version):
    """Updates the BLAST version in the specified file.
    
    Assumes the specified file contains the string BLAST_VERSION, which will
    be replaced by the contents of the variable passed to this function.
    """
    import re
    temp_fname = os.tmpnam()
    shutil.move(config_file, temp_fname)
    try:
        out = open(config_file, "w")
        infile = open(temp_fname, "r")
        for line in infile:
            print >> out, re.sub("BLAST_VERSION", blast_version, line),
    finally:
        out.close()
        infile.close()
        os.unlink(temp_fname)

def create_new_tarball_name(platform, version):
    """ Converts the name of a platform as specified to the prepare_release 
    framework to an archive name according to BLAST release naming conventions.
    
    Note: the platform names come from the prepare_release script conventions,
    more information can be found in http://mini.ncbi.nih.gov/3oo
    """
    
    retval = "ncbi-blast-" + version + "+"
    if platform.startswith("Win32"):
        retval += "-ia32-win32"
    elif platform.startswith("Win64"):
        retval += "-x64-win64"
    elif platform.startswith("Linux32"):
        retval += "-ia32-linux"
    elif platform.startswith("Linux64"):
        retval += "-x64-linux"
    elif platform == "IntelMAC":
        retval += "-universal-macosx"
    elif platform == "SunOSSparc":
        retval += "-sparc64-solaris"
    elif platform == "SunOSx86":
        retval += "-x64-solaris"
    else:
        raise RuntimeError("Unknown platform: " + platform)
    return retval

def determine_platform():
    """ Determines the platform (as defined in prepare_release) for the current
    hostname 
    """

    p = platform.platform().lower()
    print "PLATFORM = " + p
    if p.find("linux") != -1:
        if p.find("x86_64") != -1:
            return "Linux64"
        else:
            return "Linux32"
    elif p.find("sunos") != -1:
        if p.find("sparc") != -1:
            return "SunOSSparc"
        else:
            return "SunOSx86"
    elif p.find("windows") != -1 or p.find("cygwin") != -1:
        if platform.architecture()[0].find("64") != -1:
            return "Win64"
        else:
            return "Win32"
    elif p.find("darwin") != -1:
        return "IntelMAC"
    else:
        raise RuntimeError("Unknown platform: " + p)
