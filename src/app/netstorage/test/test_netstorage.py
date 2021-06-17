#!/usr/bin/env python3
#
# Authors: Sergey Satskiy
#
# $Id$
#

"""
NetStorage Integration Test
"""

import sys
import os
import os.path
import datetime
import subprocess
import time
from optparse import OptionParser


VERBOSE = False
DEFAULT_SERVER_PORT = 9123
DEFAULT_DB_NAME = 'NETSTORAGE_INTEGRATION_TEST'
SERVICE_NAME = 'NETSTORAGE_INTEGRATION_TEST_SERVICE'

SQSH = '/opt/sybase/utils/bin/sqsh-ms'


def main():
    """The real entry point"""

    parser = OptionParser("""%prog

Sets up a NetStorage server config, checks the DB, run the server
and then runs the tests. The following environment variables must be set:
- TEST_NETSTORAGE_DB_USERNAME
- TEST_NETSTORAGE_DB_PASSWORD
- TEST_NETSTORAGE_FILETRACK_API_KEY
The following environment variables could be set:
- TEST_NETSTORAGE_DB_NAME (default: NETSTORAGE_INTEGRATION_TEST)
- TEST_NETSTORAGE_PORT (default: 9123)""")
    parser.add_option("-v", "--verbose",
                      action="store_true", dest="verbose", default=False,
                      help="be verbose (default: False)")

    # parse the command line options
    options, args = parser.parse_args()
    global VERBOSE
    VERBOSE = options.verbose

    printVerbose('---------- Start ----------')

    printVerbose('Checking prerequisites...')
    checkPrerequisites()
    printVerbose('Prerequisites: OK')

    printVerbose('Getting the current DB version...')
    dbVer, spVer = getDBVersion()
    printVerbose('Current DB version: ' + str(dbVer) + '.' + str(spVer))

    printVerbose('Collecting DB update scripts...')
    dbUpdates = collectDBUpdateScripts()
    printVerbose('DB update scripts collected')

    printVerbose('Updating DB if needed...')
    updateDB(dbVer, spVer, dbUpdates)
    printVerbose('Updating DB finished')

    printVerbose('Preparing netstoraged.ini file...')
    prepareConfig()
    printVerbose('netstoraged.ini prepared: OK')

    printVerbose('Running NetStoraged in non daemon mode...')
    proc = runServer()
    printVerbose('Server run: OK')

    printVerbose('Running tests against the server...')
    runTests()
    printVerbose('Tests finished')

    printVerbose('Stopping NetStorage...')
    stopServer(proc)
    printVerbose('NetStorage stopped')
    return 0


def checkPrerequisites():
    """Checks that all the required files are here and the environment"""
    errors = []

    # Check the environment
    if 'TEST_NETSTORAGE_DB_USERNAME' not in os.environ:
        errors.append('  The environment variable TEST_NETSTORAGE_DB_USERNAME '
                      'is not found')
    if 'TEST_NETSTORAGE_DB_PASSWORD' not in os.environ:
        errors.append('  The environment variable TEST_NETSTORAGE_DB_PASSWORD '
                      'is not found')
    if 'TEST_NETSTORAGE_FILETRACK_API_KEY' not in os.environ:
        errors.append('  The environment variable '
                      'TEST_NETSTORAGE_FILETRACK_API_KEY is not found')
    if 'TEST_NETSTORAGE_PORT' in os.environ:
        val = os.environ['TEST_NETSTORAGE_PORT']
        try:
            val = int(val)
        except:
            errors.append(  'The environment variable TEST_NETSTORAGE_PORT '
                          'must be an integer port number')

    # Check the files
    must = ['netstoraged', 'netstoraged.ini.template',
            'test_netstorage_server.bash', 'test_netstorage']
    selfDir = getSelfDir()
    for name in must:
        if not os.path.exists(selfDir + name):
            errors.append('  ' + name + ' file is not found. Expected here: ' +
                          selfDir + name)

    # At least one .sql file must be in the directory
    count = 0
    for root, dirs, files in os.walk(selfDir, topdown=False):
        for name in files:
            if name == 'nst_db.sql':
                # Skip it -- it is for an initial DB creation but we expect the
                # db exists
                continue
            if name.startswith('nst_db_') and name.endswith('.sql'):
                count += 1
    if count == 0:
        errors.append('  Could not find any .sql files. '
                      'At least one is expected at ' + selfDir)

    if errors:
        raise Exception('The following errors have been detected:\n' +
                        '\n'.join(errors))


def prepareConfig():
    """Substitutes fields in the templace config"""
    selfDir = getSelfDir()
    srcPath = selfDir + 'netstoraged.ini.template'
    dstPath = selfDir + 'netstoraged.ini'
    with open(srcPath, 'r', encoding='utf-8') as diskfile:
        content = diskfile.read()

    dbName, dbUser, dbPassword = getDBParams()
    ftAPIKey = getFiletrackAPIKey()
    port = getServerPort()
    content = content.replace('{DB_NAME}', dbName) \
                     .replace('{DB_USERNAME}', dbUser) \
                     .replace('{DB_PASSWORD}', dbPassword) \
                     .replace('{FILETRACK_API_KEY}', ftAPIKey) \
                     .replace('{PORT}', str(port)) \
                     .replace('{NST_SERVICE}', SERVICE_NAME)

    with open(dstPath, 'w', encoding='utf-8') as diskfile:
        diskfile.write(content)

    printVerbose('DB name: ' + dbName)
    printVerbose('DB user: ' + dbUser)
    printVerbose('DB password: ' + dbPassword)
    printVerbose('FT API key: ' + ftAPIKey)
    printVerbose('Server port: ' + str(port))
    printVerbose('Service name: ' + SERVICE_NAME)


def runServer():
    """Runs the server in non-daemon mode and returns a proc info"""
    binPath = getSelfDir() + 'netstoraged'
    configPath = getSelfDir() + 'netstoraged.ini'
    logPath = getSelfDir() + 'netstoraged.log'

    if os.path.exists(logPath):
        printVerbose('Removing the old log file ' + logPath)
        os.unlink(logPath)
        printVerbose('Removed')
    cmdLine = [binPath, '-conffile',
               configPath, '-logfile',
               logPath, '-nodaemon']
    printVerbose('Server command line: ' + ' '.join(cmdLine))
    proc = subprocess.Popen(cmdLine)
    time.sleep(5)   # Give it some time to start
    printVerbose('Server is running...')
    return proc


def getDBVersion():
    """Provides the current DB version"""
    dbName, dbUser, dbPassword = getDBParams()
    sql = "SELECT version FROM Versions WHERE name='db_structure';" \
          "SELECT version FROM Versions WHERE name='sp_code'"
    cmdLine = SQSH + ' -S ' + dbName + ' -D ' + dbName + ' -U ' + dbUser + \
              " -P '" + dbPassword + "' -C \"" + sql + "\" -h"
    printVerbose('SQL Query to get the current DB version: ' + cmdLine)
    output = subprocess.getoutput(cmdLine)
    printVerbose('SQL Query finished')
    parts = output.strip().replace('\n', '').split(' ')
    if len(parts) < 2:
        raise Exception('Unexpected MS SQL reply: ' + output + '\nExpected '
                        'two integer versions')
    try:
        dbVer = int(parts[0])
        spVer = int(parts[-1])
    except Exception as exc:
        raise Exception('Could not get the DB structure. Expected two integer '
                        'versions. Received: ' + output)

    return dbVer, spVer


def collectDBUpdateScripts():
    """Collects all the .sql scripts in the current dir"""
    def convertVersion(s):
        if '.' in s:
            parts = s.split('.')
            if len(parts) != 2:
                raise Exception('DB update file has unexpected version '
                                'format: ' + s)
            try:
                dbVer = int(parts[0])
                spVer = int(parts[1])
            except:
                raise Exception('DB update file has unexpected version '
                                'format: ' + s)
            return dbVer, spVer
        try:
            dbVer = int(s)
        except:
            raise Exception('DB update file has unexpected version '
                            'format: ' + s)
        return dbVer, 0

    result = []
    selfDir = getSelfDir()
    for name in os.listdir(selfDir):
        if os.path.isdir(name):
            continue
        if name == 'nst_db.sql':
            # Skip it -- it is for an initial DB creation but we expect the
            # db exists
            continue
        if name.startswith('nst_db_') and name.endswith('.sql'):
            parts = name[7:-4].split('_to_')
            if len(parts) != 2:
                raise Exception('DB update file ' + name +
                                ' has unexpected name format')

            dbVerFrom, spVerFrom = convertVersion(parts[0])
            dbVerTo, spVerTo = convertVersion(parts[1])
            result.append((dbVerFrom, spVerFrom, dbVerTo, spVerTo,
                           selfDir + name))
    result = sorted(result, key=lambda val: val[0] * 10000 + val[1])
    return result


def updateDB(dbVer, spVer, dbUpdates):
    """Updates the DB structure and SPs if needed"""
    if not dbUpdates:
        raise Exception('No DB update files found')

    def isLatest(dbVer, spVer, dbUpdates):
        latestDBVer = dbUpdates[-1][2]
        latestSPVer = dbUpdates[-1][3]
        return latestDBVer == dbVer and latestSPVer == spVer

    if isLatest(dbVer, spVer, dbUpdates):
        printVerbose('No DB update required. The ' + str(dbVer) + '.' +
                     str(spVer) + ' is the latest DB version.')
        return

    # It is not the latest. Find the current index.
    for index, item in enumerate(dbUpdates):
        if item[0] == dbVer and item[1] == spVer:
            break
    else:
        raise Exception('Cannot find the current DB version '
                        'in the DB update scripts')

    # Generate the sqshrc file
    rcFileName = getSelfDir() + 'sqsh.rc'
    with open(rcFileName, 'w', encoding='utf-8') as diskfile:
        diskfile.write("\set semicolon_hack=on")

    while index <= len(dbUpdates) - 1:
        executeSQL(dbUpdates[index][4], rcFileName)
        index += 1

    # Check that the DB has been updated
    printVerbose('Checking the DB update postcondition...')
    dbVer, spVer = getDBVersion()
    if not isLatest(dbVer, spVer, dbUpdates):
        raise Exception('Unsuccessfull DB update. Expected version: ' +
                        str(dbUpdates[-1][2]) + '.' + str(dbUpdates[-1][3]) +
                        ' Real version: ' +
                        str(dbVer) + '.' + str(spVer))


def executeSQL(scriptFile, rcFileName):
    """Executes a single .sql file"""
    dbName, dbUser, dbPassword = getDBParams()
    printVerbose('Executing DB update script: ' + scriptFile)
    with open(scriptFile, 'r', encoding='utf-8') as diskfile:
        content = diskfile.read()

    contentUpdatedDB = content.replace('USE [NETSTORAGE_DEV]',
                                       'USE [' + dbName + ']')
    if contentUpdatedDB == content:
        raise Exception('Unexpected content of the ' + scriptFile +
                        '. Expected to have "USE [NETSTORAGE_DEV]", '
                        'found nothing')

    contentUpdatedCondition = contentUpdatedDB.replace('IF 1 = 1',
                                                       'IF 1 = 0')
    if contentUpdatedCondition == contentUpdatedDB:
        raise Exception('Unexpected content of the ' + scriptFile +
                        '. Expected to have "IF 1 = 1", found nothing')

    parts = list(os.path.split(scriptFile))
    parts[-1] = '_' + parts[-1]
    tempName =  os.path.sep.join(parts)
    with open(tempName, 'w', encoding='utf-8') as diskfile:
        diskfile.write(contentUpdatedCondition)

    cmdLine = SQSH + ' -c GO -S ' + dbName + ' -D ' + dbName + ' -U ' + dbUser + \
              " -P '" + dbPassword + "' -i " + tempName + ' -r ' + rcFileName
    printVerbose('DB update command line: ' + cmdLine)
    status, _ = subprocess.getstatusoutput(cmdLine)
    os.unlink(tempName)
    if status != 0:
        raise Exception('Error executing DB update script ' + scriptFile +
                        '. Status code: ' + str(status))


def runTests():
    """Runs the tests against the running server"""
    binPath = getSelfDir() + 'test_netstorage_server.bash'
    cmdLine = [binPath, SERVICE_NAME, 'localhost:' + str(getServerPort())]

    printVerbose('Running tests...')
    proc = subprocess.Popen(cmdLine)
    proc.wait()
    printVerbose('Tests finished')

    ret = proc.returncode
    printVerbose('Tests return code: ' + str(ret))


def stopServer(proc):
    """Stops the netstoraged process"""
    printVerbose('Sending TERM to the server...')
    proc.terminate()
    printVerbose('TERM is sent')

    printVerbose('Wait till server finishes...')
    proc.wait()
    printVerbose('Server finished')

    ret = proc.returncode
    printVerbose('Server return code: ' + str(ret))


def printVerbose(msg):
    """Prints stdout message conditionally"""
    if VERBOSE:
        timestamp = datetime.datetime.now().strftime('%m-%d-%y %H:%M:%S')
        print(' '.join((timestamp, msg)))


def printStderr(msg):
    """Prints onto stderr with a prefix"""
    timestamp = datetime.datetime.now().strftime( '%m-%d-%y %H:%M:%S' )
    print(' '.join((timestamp, 'NetStorage integration test script.', msg)),
          file=sys.stderr)


def getSelfDir():
    selfDir = os.path.dirname(os.path.realpath(sys.argv[0]))
    if not selfDir.endswith(os.path.sep):
        selfDir += os.path.sep
    return selfDir


def getDBParams():
    """Provides DB name, user name and password"""
    dbName = DEFAULT_DB_NAME
    if 'TEST_NETSTORAGE_DB_NAME' in os.environ:
        dbName = os.environ['TEST_NETSTORAGE_DB_NAME']
    dbUser = os.environ['TEST_NETSTORAGE_DB_USERNAME']
    dbPassword = os.environ['TEST_NETSTORAGE_DB_PASSWORD']
    return dbName, dbUser, dbPassword


def getFiletrackAPIKey():
    """Provides the filetrack API key"""
    return os.environ['TEST_NETSTORAGE_FILETRACK_API_KEY']


def getServerPort():
    """Provides the server port"""
    port = DEFAULT_SERVER_PORT
    if 'TEST_NETSTORAGE_PORT' in os.environ:
        port = int(os.environ['TEST_NETSTORAGE_PORT'])
    return port


# The script execution entry point
if __name__ == "__main__":
    try:
        retVal = main()
    except SystemExit:
        # That was an OptionParser help exit
        retVal = 0
    except KeyboardInterrupt:
        # Ctrl+C
        printStderr('Ctrl + C received')
        retVal = 3
    except Exception as exc:
        printStderr(str(exc))
        retVal = 4
    except:
        printStderr('Generic unknown script error')
        retVal = 5

    printVerbose('Return code: ' + str(retVal))
    printVerbose('---------- Finish ---------')
    sys.exit(retVal)
