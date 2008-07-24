#!/usr/bin/env python
""" Various utilities/tools for BLAST """

from subprocess import call 

def safe_exec(cmd):
    """ Executes a command and checks its return value, throwing an
        exception if it fails.
    """   
    try:
        retcode = call(cmd, shell=True)
        if retcode < 0:
            raise RuntimeError("Child was terminated by signal " + 
                               str(-retcode))
        elif retcode != 0:
            raise RuntimeError("Command failed with exit code " + str(retcode))
    except OSError, err:
        raise RuntimeError("Execution failed: " + err)