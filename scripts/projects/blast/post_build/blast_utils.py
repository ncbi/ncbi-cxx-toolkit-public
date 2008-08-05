#!/usr/bin/env python
""" Various utilities/tools for BLAST """

__all__ = [ "safe_exec", "BLAST_VERSION", "update_blast_version" ]

import os
from subprocess import call
from shutil import move

BLAST_VERSION = "2.2.18" 

def safe_exec(cmd):
    """ Executes a command and checks its return value, throwing an
        exception if it fails.
    """   
    try:
        msg = "Command: '" + cmd + "' "
        retcode = call(cmd, shell=True)
        if retcode < 0:
            msg += "Termined by signal " + str(-retcode)
            raise RuntimeError(msg)
        elif retcode != 0:
            msg += "Failed with exit code " + str(retcode)
            raise RuntimeError(msg)
    except OSError, err:
        msg += "Execution failed: " + err
        raise RuntimeError(msg)

def update_blast_version(config_file):
    """Updates the BLAST version in the specified file.
    
    Assumes the specified file contains the string BLAST_VERSION, which will
    be replaced by the contents of the variable of the same name in this module.
    """
    import re
    temp_fname = os.tmpnam()
    move(config_file, temp_fname)
    try:
        out = open(config_file, "w")
        infile = open(temp_fname, "r")
        for line in infile:
            print >> out, re.sub("BLAST_VERSION", BLAST_VERSION, line),
    finally:
        out.close()
        infile.close()
        os.unlink(temp_fname)
