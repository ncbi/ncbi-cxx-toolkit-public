#!/usr/bin/env python3

# pylint: disable=C0301     # too long lines
# pylint: disable=C0103     # var names
# pylint: disable=C0305     # trailing newlines
# pylint: disable=C0116     # docstring
# pylint: disable=C0114     # module docstring
# pylint: disable=C0410     # multiple imports in one line
# pylint: disable=W0703     # too general exception
# pylint: disable=W0702     # No exception type(s) specified (bare-except)
# pylint: disable=W0212     # Access to a protected member
# pylint: disable=R1702     # Too many nested blocks
# pylint: disable=R0912     # Too many branches
# pylint: disable=C0206     # Consider iterating with .items()
# pylint: disable=R1732     # Consider using 'with' for resource-allocating operations
# pylint: disable=C0201     # Consider iterating the dictionary directly instead of calling .keys()
# pylint: disable=R0911     # Too many return statements
# pylint: disable=W0603     # Using the global statement
# pylint: disable=W0602     # Using global for
# pylint: disable=R0913     # Too many arguments
# pylint: disable=R0917     # Too many positional arguments
# pylint: disable=R0915     # Too many statements
# pylint: disable=R0914     # Too many local variables


from optparse import OptionParser
from threading import Thread, Event
from urllib import request as urlrequest
import sys
import time
import ssl
import random
import requests

THREADS = []
STOP_EVENT = None
IS_HTTP = None
PSG_TIMEOUT = 2.0

# Starting from python 3.13 the same assignment in a function which creates a
# context stopped working (at least some of the https servers failed to be
# detected as https because of a connection exception). The global version of
# the assignment by some unknown reasons works.
# The unverified context is harmless for http connections so it is left here.
ssl._create_default_https_context = ssl._create_unverified_context

def createHttpRequest(noProtoUrl):
    url = 'http://' + noProtoUrl
    req = urlrequest.Request(url)
    return req


def createHttpsRequest(noProtoUrl):
    # Dirty trick so that self signed certificates work
    ssl._create_default_https_context = ssl._create_unverified_context

    url = 'https://' + noProtoUrl
    req = urlrequest.Request(url)
    return req


def isHttp(host, port):
    global IS_HTTP

    if IS_HTTP is not None:
        return IS_HTTP

    fullAddress = host + ':' + str(port)
    testPath = '/livez'
    try:
        req = createHttpRequest(fullAddress + testPath)
        response = urlrequest.urlopen(req, timeout=PSG_TIMEOUT)
        _ = response.read().decode('utf-8')
        IS_HTTP = True
        print(f"{fullAddress} instance detected as http")
        return True
    except:
        pass

    try:
        req = createHttpsRequest(fullAddress + testPath)
        response = urlrequest.urlopen(req, timeout=PSG_TIMEOUT)
        _ = response.read().decode('utf-8')
        IS_HTTP = False
        print(f"{fullAddress} instance detected as https")
        return False
    except:
        pass

    print(f"{fullAddress} instance is detected as faulty. It neither respond to http nor https probe.")
    return None


def getRequest(host, port, path):
    is_http = isHttp(host, port)
    if is_http is None:
        # Could not detect the server type https/https
        return None

    fullAddr = host + ':' + str(port)
    if is_http:
        return createHttpRequest(fullAddr + path)
    return createHttpsRequest(fullAddr + path)


def parseHostPort(value):
    parts = value.strip().split(':')
    if len(parts) != 2:
        print(f'The input value "{value}" is expected in the host:port format')
        return None, None
    try:
        _ = int(parts[1])
    except:
        print(f'The port number is not integer. The input value "{value}" is expected in the host:port format')
        return None, None
    return parts[0], int(parts[1])


def SendThread(stop_event, host, port, delay):
    """Note: the 'requests' library is used here. The reason is support of the
       sessions i.e. multiple requests over one connection."""

    time.sleep(0.1 * random.randint(0, 1))

    gen_id = str(random.randint(0, 1000))
    peer_id = 'id-' + gen_id
    peer_user_agent = 'user-agent-' + gen_id

    # send hello
    if IS_HTTP:
        hello_url = f"http://{host}:{port}/hello?peer_id={peer_id}&peer_user_agent={peer_user_agent}"
        periodic_url = f"http://{host}:{port}/ID/resolve?seq_id=not_exist&use_cache=yes"
    else:
        hello_url = f"https://{host}:{port}/hello?peer_id={peer_id}&peer_user_agent={peer_user_agent}"
        periodic_url = f"https://{host}:{port}/ID/resolve?seq_id=not_exist&use_cache=yes"

    session = requests.Session()
    r = session.get(hello_url, timeout=PSG_TIMEOUT, headers={'user-agent': ''})
    if r.status_code != 200:
        print(f'/hello request status code {r.status_code} indicates an error')
        return

    while True:
        # send request
        r = session.get(periodic_url, timeout=PSG_TIMEOUT, headers={'user-agent': ''})
        if r.status_code != 200:
            print(f'ID/resolve request status code {r.status_code} indicates an error')
            return

        if stop_event.is_set():
            return

        stop_event.wait(delay)
        if stop_event.is_set():
            return


def main():
    global THREADS
    global STOP_EVENT

    parser = OptionParser(
        """
    Creates the specified # of connections
    Sends hello
    Sends requests with a specified delay between them till killed
    %prog [options] host:port
    """)

    parser.add_option('-c', '--connections',
                      action="store", type="int", dest="conn_num", default=5,
                      help="Number of connections to establish (default: 5)")
    parser.add_option('-d', '--delay',
                      action="store", type="float", dest="delay", default=0.1,
                      help="Delay between requests (default: 0.1 sec)")

    options, args = parser.parse_args()
    if len(args) != 1:
        parser.print_help()
        return 1
    if options.conn_num <= 0:
        print('Number of connections must be > 0')
        return 1
    if options.delay < 0.0:
        print('Delay between requests must be >= 0.0')
        return 1

    host, port = parseHostPort(args[0])
    if host is None or port is None:
        return 1

    if isHttp(host, port) is None:
        return 1

    STOP_EVENT = Event()
    for _ in range(options.conn_num):
        THREADS.append(Thread(target=SendThread,
                              args=[STOP_EVENT, host, port, options.delay]))

    for th in THREADS:
        th.start()

    # Formality
    for th in THREADS:
        th.join()
    return 0


if __name__ == '__main__':
    try:
        retVal = main()
    except KeyboardInterrupt:
        print("Keyboard interrupt")
        retVal = 0

        if STOP_EVENT is not None:
            print("Sending stop to threads")
            STOP_EVENT.set()
        c = 0
        for jth in THREADS:
            print(f"Joining thread #{c}...")
            jth.join()
            c += 1
        print("Finishing up")
    except Exception as exc:
        print(str(exc), file=sys.stderr)
        retVal = 3

    sys.exit(retVal)

