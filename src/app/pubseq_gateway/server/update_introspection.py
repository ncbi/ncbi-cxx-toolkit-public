#!/usr/bin/env python3

import os, sys, os.path
from optparse import OptionParser
from urllib import request as urlrequest


XSLT_FILE_NAME = 'json2html_test.xsl'

def getContentType(response):
    for item in response.getheaders():
        if item[0].lower() == 'content-type':
            return item[1]
    return None


def getJsonFromServer(host, port):
    try:
        url = 'http://' + host + ':' + str(port) + '/?fmt=json'
        req = urlrequest.Request(url)
        response = urlrequest.urlopen(req, timeout=5)
        content = response.read().decode('utf-8')

        if response.status != 200:
            print('Response from ' + url + ' is not 200: ' + str(response.status))
            return None

        contentType = getContentType(response)
        if contentType is None:
            print('Resonse from ' + url + ' does not have a content type')
            return None

        if contentType.lower() != 'application/json':
            print('Response from '  + url + ' is not json: ' + contentType)
            return None

        return content
    except Exception as exc:
        print('Error getting json from ' + url + ': ' + str(exc))
        return None
    return None

def saveJsonContent(jsonContent):
    fName = 'introspection.json'
    try:
        with open(fName, 'w') as out:
            out.write(jsonContent)
        return fName
    except Exception as exc:
        print('Error writing json into a file ' + fName + ': ' + str(exc))
        return None

def runXslt(jsonFileName):
    htmlFileName = 'introspection.html'

    if 'NCBI' in os.environ:
        transformPath = os.environ['NCBI'] + '/'
    else:
        transformPath = '/netopt/ncbi_tools64/'
    transformPath += 'saxon-11.5/c++/command/transform'
    transformPath = os.path.normpath(transformPath)

    if not os.path.exists(transformPath):
        print('Cannot find transform. Expected here: ' + transformPath)
        return None

    ret = os.system(transformPath + ' -xsl:' + XSLT_FILE_NAME +
                    ' -it:main input=' + jsonFileName + ' > ' + htmlFileName)
    if ret != 0:
        print('Transform execution error code: ' + str(ret))
        return None
    return htmlFileName


def main():

    parser = OptionParser(
    """
    %prog [output .hpp file]
    default input file name: introspection.html
    default output file name: introspection_html.hpp
    """)

    parser.add_option("-v", "--verbose",
                      action="store_true", dest="verbose", default=False,
                      help="be verbose (default: False)")
    parser.add_option("--server",
                      action="store", type="string", dest="server",
                      help="PSG server instance: should host:port. Default: tonka1:2180")

    options, args = parser.parse_args()
    verbose = options.verbose

    if len(args) not in [0, 1]:
        print('incorrect number of arguments. Expected 0 or 1')
        return 3

    srv = "tonka1:2180"
    if options.server:
        srv = options.server

    parts = srv.split(':')
    if len(parts) != 2:
        print('Incorrect server address')
        return 3
    host = parts[0]
    try:
        port = int(parts[1])
    except:
        print('Incorrect server port')
        return 3

    jsonContent = getJsonFromServer(host, port)
    if jsonContent is None:
        return 3

    jsonFileName = saveJsonContent(jsonContent)
    if jsonFileName is None:
        return 3

    # Run the xslt
    htmlFileName = runXslt(jsonFileName)
    if htmlFileName is None:
        return 3

    try:
        os.remove(jsonFileName)
    except:
        pass

    # Final step: convert html into .hpp
    hppFile = 'introspection_html.hpp'
    if len(args) > 0:
        outputFile = args[0]

    iFile = open(htmlFileName, 'r')
    try:
        oFile = open(hppFile, 'w')
    except:
        print('Cannot open output file ' + hppFile)
        return 3

    for line in iFile.readlines():
        line = '"' + line.rstrip().replace('"', '\\"') + '\\n"' + '\n'
        oFile.write(line)

    iFile.close()
    oFile.close()

    try:
        os.remove(htmlFileName)
    except:
        pass

    return 0


if __name__ == '__main__':
    try:
        retVal = main()
    except KeyboardInterrupt:
        retVal = 2
    except Exception as exc:
        print(str(exc))
        retVal = 3

    sys.exit(retVal)

