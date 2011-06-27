#!/usr/bin/env python
#
# Authors: Sergey Satskiy
#
# $Id$
#


"""
grid_cli utility python wrapper
"""

import os, tempfile
from subprocess import Popen, PIPE



class GridClient:
    " Wrapper for grid_cli "

    def __init__( self, host, port, verbose = False ):
        self.__host = host
        self.__port = str( port )
        self.__verbose = verbose

        # Check for grid_cli
        if os.system( "grid_cli help > /dev/null 2>&1" ) != 0:
            raise Exception( "Cannot find grid_cli available via PATH " )
        return

    def __printCmd( self, cmd ):
        " Handy function to print the command if needed "
        if self.__verbose:
            print "Executing command: " + " ".join( cmd )

    def submitJob( self, qname, jobinput, affinity = "",
                   tags = "", progress_msg = "", exclusive = False ):
        " Submit a job and provide the submitted job key "

        cmdLine = [ "grid_cli", "submitjob",
                    "--queue=" + qname,
                    "--input=" + jobinput,
                    "--ns=" + self.__host + ":" + self.__port ]
        if affinity != "":
            cmdLine += [ "--affinity=" + affinity ]
        if tags != "":
            cmdLine += [ "--job-tag=" + tags ]
        if progress_msg != "":
            cmdLine += [ "--progress-message=" + progress_msg ]
        if exclusive:
            cmdLine += [ "--exclusive-job" ]

        self.__printCmd( cmdLine )
        return safeRun( cmdLine ).strip()

    def killJob( self, qname, jobKey, auth = 'netschedule_admin' ):
        " removes a job from the queue "

        cmdLine = [ "grid_cli", "kill", jobKey,
                    "--queue=" + qname,
                    "--ns=" + self.__host + ":" + self.__port ]
        if auth != "":
            cmdLine += [ "--auth=" + auth ]

        self.__printCmd( cmdLine )
        safeRun( cmdLine )
        return

    def killAllJobs( self, qname, auth = 'netschedule_admin' ):
        " removes all the jobs from the queue "

        cmdLine = [ "__grid_cli", "kill", "--all-jobs",
                    "--queue=" + qname,
                    "--ns=" + self.__host + ":" + self.__port ]
        if auth != "":
            cmdLine += [ "--auth=" + auth, "--compat-mode" ]

        self.__printCmd( cmdLine )
        safeRun( cmdLine )
        return





def safeRun( commandArgs ):
    " Provides the process stdout "
    stdOut, stdErr = safeRunWithStderr( commandArgs )
    return stdOut


def safeRunWithStderr( commandArgs ):
    " Runs the given command and provides both stdout and stderr "

    errTmp = tempfile.mkstemp()
    errStream = os.fdopen( errTmp[ 0 ] )
    process = Popen( commandArgs, stdin = PIPE,
                     stdout = PIPE, stderr = errStream )
    process.stdin.close()
    processStdout = process.stdout.read()
    process.stdout.close()
    errStream.seek( 0 )
    err = errStream.read()
    errStream.close()
    process.wait()
    try:
        # On WinXX the file might still be kept and unlink generates exception
        os.unlink( errTmp[ 1 ] )
    except:
        pass

    # 'grep' return codes:
    # 0 - OK, lines found
    # 1 - OK, no lines found
    # 2 - Error occured

    if process.returncode == 0 or \
       ( os.path.basename( commandArgs[ 0 ] ) == "grep" and \
         process.returncode == 1 ):
        # No problems, the ret code is 0 or the grep special case
        return processStdout, err.strip()

    # A problem has been identified
    raise Exception( "Error in '%s' invocation: %s" % \
                     (commandArgs[0], err) )

