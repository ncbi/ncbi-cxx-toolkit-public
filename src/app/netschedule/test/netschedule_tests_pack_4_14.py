#
# Authors: Sergey Satskiy
#
# $Id$
#

"""
Netschedule server tests pack for the features appeared in NS-4.14.0
"""

import time, os
import socket
from netschedule_tests_pack import TestBase
from netschedule_tests_pack_4_10 import getClientInfo, NON_EXISTED_JOB, \
                                        getAffinityInfo, getNotificationInfo, \
                                        getGroupInfo, changeAffinity, \
                                        execAny
import grid_v01
import ncbi.grid.ns as grid

# Works for python 2.5. Python 2.7 has it in urlparse module
from cgi import parse_qs


class Scenario500( TestBase ):
    " Scenario 500 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "Main job status changes loop and notifications"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        jobID = self.ns.submitJob( 'TEST', 'bla' )

        # Start listening
        port = self.ns.getPort() + 2 
        sock = socket.socket( socket.AF_INET, # Internet
                              socket.SOCK_DGRAM ) # UDP
        sock.bind( ( "" , port ) )

        ns_client = self.getNetScheduleService( 'TEST', 'scenario500' )
        ns_client.set_client_identification( 'mynode', 'mysession' )
        execAny( ns_client, 'LISTEN ' + jobID + " " + str( port ) + " 20")

        # Main job loop
        jobID, authToken, attrs, jobInput = self.ns.getJob( "TEST", -1, '', '',
                                                           "node1", "session1" )
        self.ns.returnJob( 'TEST', jobID, authToken )
        jobID, authToken, attrs, jobInput = self.ns.getJob( "TEST", -1, '', '',
                                                           "node2", "session2" )
        self.ns.putJob( 'TEST', jobID, authToken, 0, out = "my output" )
        jobID, state, passport = self.ns.getJobsForReading2( 'TEST', -1, "",
                                                           'node3', 'session3' )
        self.ns.rollbackRead2( 'TEST', jobID, passport, 'node4', 'session4' )
        jobID, state, passport = self.ns.getJobsForReading2( 'TEST', -1, "",
                                                           'node5', 'session5' )
        self.ns.confirmRead2( 'TEST', jobID, passport, 'node6', 'session6' )
        self.ns.cancelJob( 'TEST', jobID, 'node7', 'session7' )

        # Receive from the socket
        time.sleep( 5 )
        notifications = ""
        while True:
            try:
                data = sock.recv( 16384, socket.MSG_DONTWAIT )
                if not data:
                    break
                if len( notifications ) != 0:
                    notifications += "\n"
                notifications += data
            except:
                break

        if "job_status=Running&last_event_index=1" not in notifications:
            raise Exception( "Did not receive job_status=Running&last_event_index=1" )
        if "job_status=Pending&last_event_index=2" not in notifications:
            raise Exception( "Did not receive job_status=Pending&last_event_index=2" )
        if "job_status=Running&last_event_index=3" not in notifications:
            raise Exception( "Did not receive job_status=Running&last_event_index=3" )
        if "job_status=Done&last_event_index=4" not in notifications:
            raise Exception( "Did not receive job_status=Done&last_event_index=4" )
        if "job_status=Reading&last_event_index=5" not in notifications:
            raise Exception( "Did not receive job_status=Reading&last_event_index=5" )
        if "job_status=Done&last_event_index=6" not in notifications:
            raise Exception( "Did not receive job_status=Done&last_event_index=6" )
        if "job_status=Reading&last_event_index=7" not in notifications:
            raise Exception( "Did not receive job_status=Reading&last_event_index=7" )
        if "job_status=Confirmed&last_event_index=8" not in notifications:
            raise Exception( "Did not receive job_status=Confirmed&last_event_index=8" )
        if "job_status=Canceled&last_event_index=9" not in notifications:
            raise Exception( "Did not receive job_status=Canceled&last_event_index=9" )

        return True


class Scenario501( TestBase ):
    " Scenario 501 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "Notifications for the timed out job"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        jobID = self.ns.submitJob( 'TEST', 'bla' )

        # Start listening
        port = self.ns.getPort() + 2 
        sock = socket.socket( socket.AF_INET, # Internet
                              socket.SOCK_DGRAM ) # UDP
        sock.bind( ( "" , port ) )

        ns_client = self.getNetScheduleService( 'TEST', 'scenario501' )
        ns_client.set_client_identification( 'mynode', 'mysession' )
        execAny( ns_client, 'LISTEN ' + jobID + " " + str( port ) + " 30")

        # Main job loop
        jobID, authToken, attrs, jobInput = self.ns.getJob( "TEST", -1, '', '',
                                                           "node1", "session1" )
        time.sleep( 15 )

        # Receive from the socket
        time.sleep( 3 )
        notifications = ""
        while True:
            try:
                data = sock.recv( 16384, socket.MSG_DONTWAIT )
                if not data:
                    break
                if len( notifications ) != 0:
                    notifications += "\n"
                notifications += data
            except:
                break

        if "job_status=Running&last_event_index=1" not in notifications:
            raise Exception( "Did not receive job_status=Running&last_event_index=1" )
        if "job_status=Pending&last_event_index=2" not in notifications:
            raise Exception( "Did not receive job_status=Pending&last_event_index=2" )

        return True


class Scenario502( TestBase ):
    " Scenario 502 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "Notifications for the read-timed out job"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        jobID = self.ns.submitJob( 'TEST', 'bla' )

        # Start listening
        port = self.ns.getPort() + 2 
        sock = socket.socket( socket.AF_INET, # Internet
                              socket.SOCK_DGRAM ) # UDP
        sock.bind( ( "" , port ) )

        ns_client = self.getNetScheduleService( 'TEST', 'scenario502' )
        ns_client.set_client_identification( 'mynode', 'mysession' )
        execAny( ns_client, 'LISTEN ' + jobID + " " + str( port ) + " 30")

        # Main job loop
        jobID, authToken, attrs, jobInput = self.ns.getJob( "TEST", -1, '', '',
                                                           "node1", "session1" )
        self.ns.returnJob( 'TEST', jobID, authToken )
        jobID, authToken, attrs, jobInput = self.ns.getJob( "TEST", -1, '', '',
                                                           "node2", "session2" )
        self.ns.putJob( 'TEST', jobID, authToken, 0, out = "my output" )
        jobID, state, passport = self.ns.getJobsForReading2( 'TEST', -1, "",
                                                           'node3', 'session3' )
        time.sleep( 15 )

        # Receive from the socket
        time.sleep( 3 )
        notifications = ""
        while True:
            try:
                data = sock.recv( 16384, socket.MSG_DONTWAIT )
                if not data:
                    break
                if len( notifications ) != 0:
                    notifications += "\n"
                notifications += data
            except:
                break

        if "job_status=Running&last_event_index=1" not in notifications:
            raise Exception( "Did not receive job_status=Running&last_event_index=1" )
        if "job_status=Pending&last_event_index=2" not in notifications:
            raise Exception( "Did not receive job_status=Pending&last_event_index=2" )
        if "job_status=Running&last_event_index=3" not in notifications:
            raise Exception( "Did not receive job_status=Running&last_event_index=3" )
        if "job_status=Done&last_event_index=4" not in notifications:
            raise Exception( "Did not receive job_status=Done&last_event_index=4" )
        if "job_status=Reading&last_event_index=5" not in notifications:
            raise Exception( "Did not receive job_status=Reading&last_event_index=5" )
        if "job_status=Done&last_event_index=6" not in notifications:
            raise Exception( "Did not receive job_status=Done&last_event_index=6" )

        return True


class Scenario503( TestBase ):
    " Scenario 503 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "Notifications for the deleted job"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        jobID = self.ns.submitJob( 'TEST', 'bla' )

        # Start listening
        port = self.ns.getPort() + 2 
        sock = socket.socket( socket.AF_INET, # Internet
                              socket.SOCK_DGRAM ) # UDP
        sock.bind( ( "" , port ) )

        ns_client = self.getNetScheduleService( 'TEST', 'scenario503' )
        ns_client.set_client_identification( 'mynode', 'mysession' )
        execAny( ns_client, 'LISTEN ' + jobID + " " + str( port ) + " 30")

        # Main job loop
        # Wait till the job is deleted by GC
        time.sleep( 35 )

        # Receive from the socket
        time.sleep( 3 )
        notifications = ""
        while True:
            try:
                data = sock.recv( 16384, socket.MSG_DONTWAIT )
                if not data:
                    break
                if len( notifications ) != 0:
                    notifications += "\n"
                notifications += data
            except:
                break

        if "job_status=Deleted&last_event_index=0" not in notifications:
            raise Exception( "Did not receive job_status=Deleted&last_event_index=0" )

        return True


class Scenario504( TestBase ):
    " Scenario 504 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "Notifications for the job which was moved due to session reset"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        jobID = self.ns.submitJob( 'TEST', 'bla' )

        # Start listening
        port = self.ns.getPort() + 2 
        sock = socket.socket( socket.AF_INET, # Internet
                              socket.SOCK_DGRAM ) # UDP
        sock.bind( ( "" , port ) )

        ns_client = self.getNetScheduleService( 'TEST', 'scenario504' )
        ns_client.set_client_identification( 'mynode', 'mysession' )
        execAny( ns_client, 'LISTEN ' + jobID + " " + str( port ) + " 30")

        # Main job loop
        jobID, authToken, attrs, jobInput = self.ns.getJob( "TEST", -1, '', '',
                                                           "node1", "session1" )
        # Imitate a new session of the worker node
        self.ns.getJobStatus( 'TEST', jobID, 'node1', 'new_session' )

        # Receive from the socket
        time.sleep( 3 )
        notifications = ""
        while True:
            try:
                data = sock.recv( 16384, socket.MSG_DONTWAIT )
                if not data:
                    break
                if len( notifications ) != 0:
                    notifications += "\n"
                notifications += data
            except:
                break

        if "job_status=Running&last_event_index=1" not in notifications:
            raise Exception( "Did not receive job_status=Running&last_event_index=1" )
        if "job_status=Pending&last_event_index=2" not in notifications:
            raise Exception( "Did not receive job_status=Pending&last_event_index=2" )

        return True

class Scenario505( TestBase ):
    " Scenario 505 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "RECOnfiguring queues and classes on the fly"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch( "505-1" )

        self.ns.connect( 10 )
        try:
            self.ns.directLogin( '', 'netschedule_admin' )

            # Step 1: Check that there are no queues
            self.ns.directSendCmd( 'QLST' )
            reply = self.ns.directReadSingleReply()
            if reply[ 0 ] != True:
                raise Exception( 'QLST failed: ' + reply[ 1 ] )
            if reply[ 1 ] != '':
                raise Exception( 'Unexpected list of queues. ' \
                                 'Expected no queues, received: ' + reply[ 1 ] )

            # Step 2: touch config and RECO
            if os.system( "touch netscheduled.ini" ) != 0:
                raise Exception( "error touching config file" )
            self.ns.directSendCmd( 'RECO' )
            reply = self.ns.directReadSingleReply()
            if reply[ 0 ] != True:
                raise Exception( 'RECO failed: ' + reply[ 1 ] )
            if 'No changeable parameters' not in reply[ 1 ]:
                raise Exception( 'Unexpected output for RECO: ' + reply[ 1 ] )

            # Step 3: RECO without touching
            self.ns.directSendCmd( 'RECO' )
            reply = self.ns.directReadSingleReply()
            if reply[ 0 ] != True:
                raise Exception( 'RECO failed: ' + reply[ 1 ] )
            if 'file has not been changed' not in reply[ 1 ]:
                raise Exception( 'Unexpected output for RECO: ' + reply[ 1 ] )

            # Step 4: Add 2 queue classes and two queues and RECO
            self.ns.setConfig( "505-2" )
            self.ns.directSendCmd( 'RECO' )
            reply = self.ns.directReadSingleReply()
            if reply[ 0 ] != True:
                raise Exception( 'RECO failed: ' + reply[ 1 ] )
            if reply[ 1 ] != '"server_changes" ' \
                '{"log_notification_thread" [false true], ' \
                '"log_statistics_thread" [true false]}, ' \
                '"added_queue_classes" ["class1", "class2"], ' \
                '"added_queues" {"q1" "", "q2" "class2"}':
                raise Exception( 'Unexpected output for RECO (step 4): ' + reply[ 1 ] )

            # Step 5: Create dynamic queue of a configured class
            self.ns.directSendCmd( 'QCRE dqueue class1' )
            reply = self.ns.directReadSingleReply()
            if reply[ 0 ] != True:
                raise Exception( 'QCRE failed: ' + reply[ 1 ] )
            self.ns.directSendCmd( 'QLST' )
            reply = self.ns.directReadSingleReply()
            if reply[ 0 ] != True:
                raise Exception( 'QLST failed: ' + reply[ 1 ] )
            if reply[ 1 ] != 'dqueue;q1;q2;':
                raise Exception( 'Unexpected list of queues. Received: ' + \
                                 reply[ 1 ] )

            # Step 6: replace config with: del q1, class1, alter q2, class2
            self.ns.setConfig( "505-3" )
            self.ns.directSendCmd( 'RECO' )
            reply = self.ns.directReadSingleReply()
            if reply[ 0 ] != True:
                raise Exception( 'RECO failed: ' + reply[ 1 ] )
            if reply[ 1 ] != '"deleted_queue_classes" ["class1"], ' \
                '"queue_class_changes" {"class2" {"timeout" [1, 2], ' \
                '"description" ["class two", "class two updated"]}}, ' \
                '"deleted_queues" ["q1"], "queue_changes" ' \
                '{"q2" {"max_input_size" [33, 77], "description" ' \
                '["class two", "class two updated"]}}':
                raise Exception( 'Unexpected output for RECO (step 6): ' + reply[ 1 ] )

            time.sleep( 3 )     # give a chance for deleting q1
            self.ns.directSendCmd( 'QLST' )
            reply = self.ns.directReadSingleReply()
            if reply[ 0 ] != True:
                raise Exception( 'QLST failed: ' + reply[ 1 ] )
            if reply[ 1 ] != 'dqueue;q2;':
                raise Exception( 'Unexpected QLST output (step 6): ' + reply[ 1 ] )

            self.ns.directSendCmd( 'STAT QCLASSES' )
            reply = self.ns.directReadMultiReply()
            if reply[ 0 ] != True:
                raise Exception( 'STAT QCLASSES failed: ' + reply[ 1 ] )
            if 'delete_request: true' not in reply[ 1 ]:
                raise Exception( 'Unexpected output of STAT QCLASSES: ' + \
                                 str( reply[ 1 ] ) )

            self.ns.directSendCmd( 'SETQUEUE dqueue' )
            reply = self.ns.directReadSingleReply()
            if reply[ 0 ] != True:
                raise Exception( 'SETQUEUE failed: ' + reply[ 1 ] )
            self.ns.directSendCmd( 'SUBMIT bla' )
            reply = self.ns.directReadSingleReply()
            if reply[ 0 ] != True:
                raise Exception( 'SUBMIT failed: ' + reply[ 1 ] )

            self.ns.directSendCmd( 'QDEL dqueue' )
            reply = self.ns.directReadSingleReply()
            if reply[ 0 ] != True:
                raise Exception( 'QDEL failed: ' + reply[ 1 ] )

            self.ns.directSendCmd( 'QLST' )
            reply = self.ns.directReadSingleReply()
            if reply[ 0 ] != True:
                raise Exception( 'QLST failed: ' + reply[ 1 ] )
            if reply[ 1 ] != 'dqueue;q2;':
                raise Exception( 'Unexpected QLST output (step 6-2): ' + reply[ 1 ] )

            self.ns.directSendCmd( 'CANCELQ' )
            reply = self.ns.directReadSingleReply()
            if reply[ 0 ] != True:
                raise Exception( 'CANCELQ failed: ' + reply[ 1 ] )
            self.ns.directSendCmd( 'SETQUEUE' )
            reply = self.ns.directReadSingleReply()
            if reply[ 0 ] != True:
                raise Exception( 'SETQUEUE failed: ' + reply[ 1 ] )

            time.sleep( 15 )    # wait till the job is actually deleted and the
                                # queue is deleted

            self.ns.directSendCmd( 'QLST' )
            reply = self.ns.directReadSingleReply()
            if reply[ 0 ] != True:
                raise Exception( 'QLST failed: ' + reply[ 1 ] )
            if reply[ 1 ] != 'q2;':
                raise Exception( 'Unexpected QLST output (step 6-3): ' + reply[ 1 ] )

            self.ns.directSendCmd( 'STAT QCLASSES' )
            reply = self.ns.directReadMultiReply()
            if reply[ 0 ] != True:
                raise Exception( 'STAT QCLASSES failed (6-2): ' + reply[ 1 ] )
            if '[qclass class1]' in reply[ 1 ]:
                raise Exception( "Queue class1 had to be deleted but it's not" )

            # Step 7: changes in a queue and in a class
            self.ns.setConfig( "505-4" )
            self.ns.directSendCmd( 'RECO' )
            reply = self.ns.directReadSingleReply()
            if reply[ 0 ] != True:
                raise Exception( 'RECO failed: ' + reply[ 1 ] )
            if reply[ 1 ] != '"queue_class_changes" {"class2" ' \
                '{"failed_retries" [3333, 87]}}, "queue_changes" ' \
                '{"q2" {"failed_retries" [3333, 87], ' \
                '"max_output_size" [333, 444]}}':
                raise Exception( 'Unexpected output for RECO (step 7): ' + reply[ 1 ] )

            # Step 8: restart NS and make sure the queues are still there
            self.ns.disconnect()
            self.ns.kill( "SIGKILL" )
            time.sleep( 5 )
            self.ns.start()
            time.sleep( 5 )

            reply = self.ns.getQueueList()

            if len( reply ) != 1 or reply[ 0 ] != 'q2':
                raise Exception( 'Unexpected QLST output (step 8): ' + str( reply ) )

            # Step 9: remove all the queueus and restart NS
            self.ns.setConfig( "505-5" )

            self.ns.disconnect()
            self.ns.kill( "SIGKILL" )
            time.sleep( 5 )
            self.ns.start()
            time.sleep( 5 )

            reply = self.ns.getQueueList()

            if len( reply ) != 1 or reply[ 0 ] != '':
                raise Exception( 'Unexpected QLST output (step 9): ' + str( reply ) )

        except Exception, exc:
            self.ns.disconnect()
            raise

        self.ns.disconnect()
        return True

