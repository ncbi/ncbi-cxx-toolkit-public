#! /usr/bin/env python
"""Prepare and run tests for netstorage.

This requires :
* nestoraged binary
* netstoraged.ini.template
* test_netstorage
* test_netstorage.py
* test_netstorage_server.bash

"""
import optparse
import logging

import os
import shutil
import sys
import tempfile
import tarfile

# Plus SQL
NECESSARY_FILES = [
    ('netstoraged', 'x'),
    ('test_netstorage', 'x'),
    ('netstoraged.ini.template', 'r'),
    ('test_netstorage.py', 'x'),
    ('test_netstorage_server.bash', 'x')
]
PERMISSION_NAMES={'r':os.R_OK, 'x':os.X_OK}

def _to_permission(name):
    """Convert compound permission to access flag"""
    ret=0
    for ch in name:
        if not ch in PERMISSION_NAMES:
            extra=''
            if len(name) > 1:
                extra = ' (found in ' + str(name) + ')'
            raise ValueError('I do not known permission ' + str(ch) + extra)
        ret = ret + PERMISSION_NAMES[ch]
    return ret


def _validate_files(path, message_prefix):
    """Validate files presence and permissions"""
    for necessary_file in NECESSARY_FILES:
        full_name = os.path.join(path, necessary_file[0])
        if not os.path.exists(full_name):
            logging.error(message_prefix + str(necessary_file[0]) + ' is missing')
            return False
        if not os.access(full_name, _to_permission(necessary_file[1])):
            logging.error(message_prefix + str(necessary_file[0]) + ' access bits are incorrect. Expecting ' + str(necessary_file[1]))
            return False
    return True

def _validate_config(command, parser, options):
    if command == 'prepare':
        if options.source_path is None:
            parser.print_help()
            logging.error('netstorage sources path is mandatory for "%s" command' % command)
            return False
        if options.archive_path is None:
            parser.print_help()
            logging.error('archive name is mandatory for "%s" command' % command)
            return False
    elif command == 'run':
        if options.archive_path is None:
            parser.print_help()
            logging.error('archive name is mandatory for "%s" command' % command)
            return False
        if options.ddl_user is None:
            parser.print_help()
            logging.error('DDL user is mandatory for "%s" command' % command)
            return False
        if options.ddl_pass is None:
            parser.print_help()
            logging.error('DDL password is mandatory for "%s" command' % command)
            return False
        if options.ft_api_token is None:
            parser.print_help()
            logging.error('FileTrack API token is mandatory for "%s" command' % command)
            return False
    else:
        parser.print_help()
        logging.error('Unknown command ' + command)
        return False
    return True

def setup(verbose):
    """Setup logging"""
    LOG_FORMAT="%(asctime)s %(message)s"
    logging.basicConfig(format=LOG_FORMAT, level=logging.DEBUG if verbose else logging.INFO)

def prepare(src, archive):
    """Prepare archive with everything necessary for running test"""
    logging.info('Preparing the test package')
    if os.path.exists(archive):
        logging.error("%s already exists! Not overriding it. Stopping." % archive)
        return 1
    dest = tempfile.mkdtemp(prefix='nst_test_')
    try:
        shutil.copy(os.path.join(src, 'bin', 'netstoraged'), dest)
        shutil.copy(os.path.join(src, 'bin', 'test_netstorage'), dest)
        shutil.copy(os.path.join(src, '..', 'src', 'app', 'netstorage', 'test', 'netstoraged.ini.template'), dest)
        shutil.copy(os.path.join(src, '..', 'src', 'app', 'netstorage', 'test', 'test_netstorage.py'), dest)
        shutil.copy(os.path.join(src, '..', 'src', 'misc', 'netstorage', 'test', 'test_netstorage_server.bash'), dest)
    
        for name in os.listdir(os.path.join(src, '..', 'src', 'app', 'netstorage')):
            if not name.endswith('.sql'):
                continue
            shutil.copy(os.path.join(src, '..', 'src', 'app', 'netstorage', name), dest)
        if not _validate_files(dest, ''):
            return False
        tar = tarfile.open(archive, "w")
        old_cwd = os.getcwd()
        try:
            os.chdir(dest)
            for name in os.listdir("."):
                tar.add(name)
        finally:
            os.chdir(old_cwd)
            tar.close()
    finally:
        shutil.rmtree(dest, ignore_errors=True)
    return 0

def run(ddl_username, ddl_password, ft_api_token, archive):
    """Run tests from archive"""
    if not os.path.exists(archive):
        logging.error('test archive ' + str(archive) + " does not exist. Can't continue")
        return False

    os.environ['TEST_NETSTORAGE_DB_USERNAME']=ddl_username
    os.environ['TEST_NETSTORAGE_DB_PASSWORD']=ddl_password
    os.environ['TEST_NETSTORAGE_FILETRACK_API_KEY']=ft_api_token

    old_cwd = os.getcwd()
    dest = tempfile.mkdtemp(prefix='nst_test_run_')
    try:
        tar = tarfile.open(archive, 'r')
        try:
            os.chdir(dest)
            tar.extractall()
        finally:
            tar.close()
        if not _validate_files('.', 'Bad archive ' + str(archive) + ': '):   
            return False
        return os.system('python3 ' + './test_netstorage.py') == 0
    finally:
        os.chdir(old_cwd)

def main():
    """Command line interface"""
    parser = optparse.OptionParser("""%prog [prepare|run]

Prepare and run netstorage test package. The script assumes that we have
the necessary parts of the C++ Toolkit source tree present, and that all
required executables have been built in this tree.""")
    parser.add_option("-v", "--verbose", action="store_true", dest="verbose",
                      default=False, help="be verbose (default: False)")
    parser.add_option("-s", "--source", action="store", dest="source_path",
                      default=".", help="Path to the built netstorage sources (default: .)")
    parser.add_option("-a", "--archive", action="store", dest="archive_path",
                      default=None, help="Destination archive name (mandatory)")
    parser.add_option("-u", "--ddl-user", action="store", dest="ddl_user",
                      default=None, help="DB user name for DDL operations")
    parser.add_option("-p", "--ddl-pass", action="store", dest="ddl_pass",
                      default=None, help="DB user password for DDL oprations")
    parser.add_option("-t", "--ft-api-token", action="store", dest="ft_api_token",
                      default=None, help="FileTrack API token")
    options, args = parser.parse_args()

    setup(options.verbose)

    if len(args) == 0:
        parser.print_help()
        logging.error("Command is needed")
        return False
    command = str(args[0])
    if not _validate_config(command, parser, options):
        return False
    if command == 'prepare':
        return prepare(options.source_path, options.archive_path)
    elif command == 'run':
        return run(options.ddl_user, options.ddl_pass, options.ft_api_token, options.archive_path)
    parser.print_help()
    logging.error('Unknown command ' + command)
    return False

if __name__ == "__main__":
    if not main():
        sys.exit(1)
    sys.exit(0)

