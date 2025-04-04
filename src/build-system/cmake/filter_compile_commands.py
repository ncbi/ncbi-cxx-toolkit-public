#!/usr/bin/env python3

# The script removes C-generated files from the compile_commands.json file.
# The list of exclusions is formed manually


from optparse import OptionParser
import sys
import datetime
import os, os.path
import json

VERBOSE = False

EXCLUDE_FILES = ['asndbld/objDbld.c',
                 'asn/objBlComm.c',
                 'asn/objblst3.c',
                 'asn/objBlobj.c',
                 'asn/objBlast4.c',
                 'asn/objnetres.c']


def timestamp():
    now = datetime.datetime.now()
    return now.strftime('%Y-%m-%d %H:%M:%S')

def print_verbose(msg):
    if VERBOSE:
        print(timestamp() + ' ' + msg)

def print_error(msg):
    print(timestamp() + ' ' + msg, file=sys.stderr)

def getCompileCommandsJsonPath(try_paths):
    self_path = os.path.abspath(__file__)
    self_dir = os.path.dirname(self_path)

    json_path = os.path.dirname(self_dir)
    json_path = os.path.dirname(json_path)
    json_path = os.path.dirname(json_path)
    json_path += os.path.sep + os.path.sep.join(['BUILD', 'build', 'compile_commands.json'])
    try_paths.append(json_path)

    if os.path.exists(json_path):
        return json_path

    json_path = './' + os.path.sep.join(['BUILD', 'build', 'compile_commands.json'])
    try_paths.append(json_path)
    if os.path.exists(json_path):
        return json_path
    return None

def fileMatch(candidate):
    for item in EXCLUDE_FILES:
        if candidate.endswith(item):
            return True
    return False


def main():
    global VERBOSE

    parser = OptionParser(
        """
    Filters C generated files from the compile_commands.json file. The json file
    is used by the Clang static analyzer.

    %prog [options]
    """)

    parser.add_option("-v", "--verbose",
                      action="store_true", dest="verbose", default=False,
                      help="be verbose (default: False)")

    options, args = parser.parse_args()
    VERBOSE = options.verbose
    if len(args) != 0:
        print_error('No arguments are supported. See help for details (--help option).')
        return 1

    try_paths = []
    compile_commands_path = getCompileCommandsJsonPath(try_paths)
    if compile_commands_path is None:
        print_error('Could not find compile_commands.json file. Tried paths: '
                    + ', '.join(try_paths))
        return 1

    print_verbose('Found compile_commands.json at ' + compile_commands_path)

    try:
        with open(compile_commands_path, "r") as f:
            content = json.load(f)
    except Exception as exc:
        print_error('Error loading compile_commands.json content: ' + str(exc))
        return 1

    print_verbose('Loaded compile_commands.json from ' + compile_commands_path)
    print_verbose('Found ' + str(len(content)) + ' items')

    remove_indexes = []
    for index in range(len(content)):
        if fileMatch(content[index]['file']):
            remove_indexes.append(index)

    remove_count = len(remove_indexes)
    print_verbose('Number of files to remove: ' + str(remove_count))

    while len(remove_indexes) > 0:
        index = remove_indexes.pop()
        print_verbose('Removing file ' + content[index]['file'])
        content.pop(index)

    if remove_count > 0:
        print_verbose('Writing the modified json file...')
        try:
            with open(compile_commands_path, "w") as f:
                json.dump(content, f, indent=1)
        except Exception as exc:
            print_error('Error updating compile_commands.json content: ' + str(exc))
            return 1

    return 0


if __name__ == '__main__':
    try:
        retVal = main()
    except KeyboardInterrupt:
        retVal = 2
        print('Interrupted from the keyboard', file=sys.stderr)
    except Exception as exc:
        print(str(exc), file=sys.stderr)
        retVal = 3

    sys.exit(retVal)

