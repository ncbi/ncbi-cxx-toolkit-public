#!/usr/bin/env python3
""" Various utilities/tools for BLAST """
from __future__ import print_function

__all__ = ["safe_exec", "update_blast_version"]

import os
import subprocess
import platform
import unittest
import tempfile
import re


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
    except OSError as err:
        msg += "Execution failed: " + err
        raise RuntimeError(msg)


class Tester(unittest.TestCase):
    '''Testing class for this script.'''
    def test_one(self):
        ver = "2.3.0"
        line1 = "Hello BLAST"
        with tempfile.NamedTemporaryFile(mode='w+t') as fp:
            print(line1, file=fp)
            print("BLAST_VERSION", file=fp)
            print(line1, file=fp, flush=True)

            update_blast_version(fp.name, ver)

            fp.seek(0)
            line = fp.readline().rstrip()
            self.assertEqual(line1, line)
            line = fp.readline().rstrip()
            self.assertEqual(ver, line)
            line = fp.readline().rstrip()
            self.assertEqual(line1, line)


def update_blast_version(config_file, ver):
    """Updates the BLAST version in the specified file.

    Assumes the specified file contains the string BLAST_VERSION, which will
    be replaced by the contents of the variable passed to this function.
    """
    (fd, fname) = tempfile.mkstemp()
    try:
        with open(config_file, "r") as infile, open(fname, "w") as out:
            for line in infile:
                newline = re.sub("BLAST_VERSION", ver, line.rstrip())
                print(newline, file=out)

        with open(config_file, "w") as out, open(fname, "r") as infile:
            for line in infile:
                print(line.rstrip(), file=out)
    finally:
        os.close(fd)
        os.remove(fname)


def create_new_tarball_name(platform, program, version):
    """ Converts the name of a platform as specified to the prepare_release
    framework to an archive name according to BLAST release naming conventions.

    Note: the platform names come from the prepare_release script conventions,
    more information can be found in http://mini.ncbi.nih.gov/3oo
    """

    retval = "ncbi-" + program + "-" + version
    if program == "blast":
        retval += "+"
    if platform.startswith("Win"):
        retval += "-x64-win64"
    elif platform.startswith("Linux32"):
        retval += "-ia32-linux"
    elif platform.startswith("Linux64"):
        retval += "-x64-linux"
    elif platform.startswith("IntelMAC"):
        retval += "-x64-macosx"
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
    print("PLATFORM = " + p)
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
