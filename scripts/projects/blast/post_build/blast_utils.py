#!/usr/bin/env python
""" Various utilities/tools for BLAST """

__all__ = [ "safe_exec", "BLAST_VERSION", "update_blast_version" ]

from subprocess import call

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

# TODO: Must be posted to windows
def update_blast_version(config_file):
    """Updates the BLAST version in the specified file"""
    cmd = "/usr/bin/perl -pi -e 's/BLAST_VERSION/" + BLAST_VERSION + "/' "
    cmd += config_file
    safe_exec(cmd)