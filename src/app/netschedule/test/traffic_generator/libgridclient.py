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

    def submitBatch( self, qname, jobs ):
        " Performs a batch submit "
        if len( jobs ) <= 0:
            raise Exception( "Jobs list expected" )

        # Save the jobs in a temporary file
        f, filename = tempfile.mkstemp()
        for job in jobs:
            os.write( f, "input=" + job + "\n" )
        os.close( f )

        try:
            # Execute the command
            cmdLine = [ "grid_cli", "submitjob",
                        "--batch=" + str( len(jobs) ),
                        "--input-file=" + filename,
                        "--queue=" + qname,
                        "--ns=" + self.__host + ":" + self.__port ]
            self.__printCmd( cmdLine )
            jobids = safeRun( cmdLine )

        except:
            # Remove the temporary file
            os.unlink( filename )
            raise

        # Remove the temporary file
        os.unlink( filename )

        ids = []
        for jobid in jobids.split( '\n' ):
            jobid = jobid.strip()
            if jobid != "":
                ids.append( jobid )
        return ids

    def getJob( self, qname, aff = '' ):
        " Get a job for execution "

        cmdLine = [ "grid_cli", "requestjob",
                    "--queue=" + qname,
                    "--ns=" + self.__host + ":" + self.__port ]
        if aff != '':
            cmdLine += [ "--affinity=" + aff ]

        self.__printCmd( cmdLine )
        output = safeRun( cmdLine ).split( '\n' )

        if len( output ) == 0:
            return ""
        return output[ 0 ].strip()      # This is JOB ID

    def commitJob( self, qname, jobID, retCode, out = "" ):
        " Commits the job "

        cmdLine = [ "grid_cli", "commitjob",
                    "--queue=" + qname, jobID,
                    "--return-code=" + str( retCode ),
                    "--ns=" + self.__host + ":" + self.__port ]
        if out != "":
            cmdLine += [ "--job-output=" + out ]

        self.__printCmd( cmdLine )
        return safeRun( cmdLine )

    def readJobs( self, qname, count ):
        " Get jobs for reading "

        cmdLine = [ "grid_cli", "readjobs",
                    "--limit=" + str( count ),
                    "--queue=" + qname,
                    "--ns=" + self.__host + ":" + self.__port ]

        self.__printCmd( cmdLine )
        groupID = -1
        jobs = []
        for line in safeRun( cmdLine ).split( '\n' ):
            line = line.strip()
            if line == "":
                continue
            if groupID == -1:
                groupID = int( line )
                continue
            jobs.append( line )
        return groupID, jobs

    def confirmJobRead( self, qname, groupID, jobs ):
        " Confirms the fact that jobs have been read "
        if len( jobs ) <= 0:
            raise Exception( "Jobs list expected" )

        cmdLine = [ "grid_cli", "readjobs",
                    "--confirm-read=" + str( groupID ),
                    "--queue=" + qname,
                    "--ns=" + self.__host + ":" + self.__port ]
        for jobid in jobs:
            cmdLine.append( "--job-id=" + jobid )

        self.__printCmd( cmdLine )
        return safeRun( cmdLine )




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

