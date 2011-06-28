#
# Authors: Sergey Satskiy
#
# $Id$
#

"""
NetScheduler Submit/Get/Commit/Read/Confirm loader
"""

import sys, time, threading
from ns_traffic_settings import SingleFullOKLoopSettings



class SingleFullOKLoopLoader( threading.Thread ):
    " Submit/Get/Commit/Read/Confirm loader "

    def __init__( self, gridClient, qname ):
        threading.Thread.__init__( self )
        self.__gridClient = gridClient
        self.__qname = qname
        self.__count = 0
        return

    def getName( self ):
        " Loader identification "
        return "Submit/Get/Commit/Read/Confirm"

    def getCount( self ):
        " Provides haw many loops completed "
        return self.__count

    def run( self ):
        " threaded function "
        pSize = SingleFullOKLoopSettings.packageSize
        if pSize <= 0:
            print >> sys.stderr, \
                     "Invalid SingleFullOKLoopSettings.packageSize (" + \
                     str( pSize ) + "). Must be > 0"
            return
        pause = SingleFullOKLoopSettings.pause
        if pause < 0:
            print >> sys.stderr, \
                     "Invalid SingleFullOKLoopSettings.pause (" + \
                     str( pause ) + "). Must be >= 0"
            return

        pCount = SingleFullOKLoopSettings.packagesCount
        if not ( pCount == -1 or pCount > 0 ):
            print >> sys.stderr, \
                     "Invalid SingleFullOKLoopSettings.packagesCount (" + \
                     str( pCount ) + "). Must be > 0 or -1"
            return


        # Settings are OK
        while True:
            pSize = SingleFullOKLoopSettings.packageSize
            while pSize > 0:
                jobKey = self.__safeSubmit()
                if jobKey != "":
                    jobKey = self.__safeGet()
                    if jobKey != "":
                        if self.__safeCommit( jobKey ):
                            groupID, jobs = self.__safeRead()
                            if groupID != -1:
                                self.__safeConfirm( groupID, jobs )

                self.__count += 1
                pSize -= 1

            if pause > 0:
                time.sleep( pause )

            if pCount == -1:    # Infinite loop
                continue
            pCount -= 1
            if pCount <= 0:
                break
        return

    def __safeSubmit( self ):
        " Exception safe submitting a job "
        jobKey = ""
        try:
            jobKey = self.__gridClient.submitJob( self.__qname,
                                                  "bla", "ok_loop_aff" )
        except Exception, excp:
            print >> sys.stderr, \
                     "Submit/Get/Commit/Read/Confirm: Cannot submit a job"
            print >> sys.stderr, str( excp )
        return jobKey

    def __safeGet( self ):
        " Exception safe getting a job "
        jobKey = ""
        try:
            jobKey = self.__gridClient.getJob( self.__qname, "ok_loop_aff" )
        except Exception, excp:
            print >> sys.stderr, \
                     "Submit/Get/Commit/Read/Confirm: Cannot get a job"
            print >> sys.stderr, str( excp )
        return jobKey

    def __safeCommit( self, jobKey ):
        " Exception safe commiting a job "
        try:
            self.__gridClient.commitJob( self.__qname, jobKey, 0 )
        except Exception, excp:
            print >> sys.stderr, \
                     "Submit/Get/Commit/Read/Confirm: Cannot commit a job"
            print >> sys.stderr, str( excp )
            return False
        return True

    def __safeRead( self ):
        " Exception safe reading a job "
        groupID = -1
        jobs = []
        try:
            groupID, jobs = self.__gridClient.readJobs( self.__qname, 1 )
        except Exception, excp:
            print >> sys.stderr, \
                     "Submit/Get/Commit/Read/Confirm: Cannot read a job"
            print >> sys.stderr, str( excp )
        return groupID, jobs

    def __safeConfirm( self, groupID, jobs ):
        " Exception safe confirming reading a job "
        try:
            self.__gridClient.confirmJobRead( self.__qname, groupID, jobs )
        except Exception, excp:
            print >> sys.stderr, \
                     "Submit/Get/Commit/Read/Confirm: " \
                     "Cannot confirm reading a job"
            print >> sys.stderr, str( excp )
        return

