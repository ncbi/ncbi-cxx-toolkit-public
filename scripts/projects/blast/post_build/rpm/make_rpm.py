#!/usr/bin/env python
# $Id$
# Script to create a source/binary RPM

import sys, os, shutil;
from optparse import OptionParser

verbose = False

# Current version of BLAST
BLAST_VERSION = "2.2.18"
# Name of the temporary rpmbuild directory
RPMBUILD_HOME = "rpmbuild"
PACKAGE_NAME = "ncbi-blast-" + BLAST_VERSION + "+"
# Name of the source TARBALL to create
TARBALL = PACKAGE_NAME + ".tgz"
RPM_SPEC = "ncbi-blast.spec"

def setup_rpmbuild():
    """ Prepare local rpmbuild directory. """
    cleanup_rpm()
    os.mkdir(RPMBUILD_HOME)
    for d in [ 'BUILD', 'SOURCES', 'SPECS', 'SRPMS', 'tmp', 'RPMS' ]:
        os.mkdir(os.path.join(RPMBUILD_HOME, d))
    cwd = os.getcwd()
    os.chdir(os.path.join(RPMBUILD_HOME, 'RPMS'))
    for d in [ 'i386', 'i586', 'i686', 'noarch', 'x86_64' ]:
        os.mkdir(d)
    os.chdir(cwd)

    # Create ~/.rpmmacros
    fname = os.path.join(os.path.expanduser("~"), ".rpmmacros");
    try:
        f = open(fname, "w")
        print >> f, "%_topdir %( echo", RPMBUILD_HOME, ")"
        print >> f, "%_tmppath %{_topdir}/tmp"
        print >> f
        print >> f, "%packager Christiam E. Camacho (camacho@ncbi.nlm.nih.gov)"
    finally:
        f.close()
    if verbose: print "Created ~/.rpmmacros"

def cleanup_rpm():
    """ Delete rpm files """
    if os.path.exists(RPMBUILD_HOME):
        shutil.rmtree(RPMBUILD_HOME)

    rpmmacros = os.path.join(os.path.expanduser("~"), ".rpmmacros")
    if os.path.exists(rpmmacros):
        os.remove(rpmmacros)

def safe_exec(cmd):
    """ Executes a command and checks its return value, throwing an
        exception if it fails.
    """
    import subprocess
    if verbose: print cmd
    try:
        retcode = subprocess.call(cmd, shell=True)
        if retcode < 0:
            raise RuntimeError("Child was terminated by signal " + -retcode)
    except OSError, e:
        raise RuntimeError("Execution failed: " + e)

def svn_checkout():
    # NCBI SVN repository
    SVN_NCBI = "https://svn.ncbi.nlm.nih.gov/repos_htpasswd/toolkit"

    # Check out the sources
    cmd = "svn -q co --username svnread --password allowed " + SVN_NCBI
    cmd += "/release/blast/" + BLAST_VERSION + " " + PACKAGE_NAME
    if os.path.exists(PACKAGE_NAME):
        shutil.rmtree(PACKAGE_NAME)
    safe_exec(cmd)

    # Remove unneeded directories
    shutil.rmtree(os.path.join(PACKAGE_NAME, "builds"))
    shutil.rmtree(os.path.join(PACKAGE_NAME, "scripts"))
    cmd = "find " + PACKAGE_NAME + " -type d -name .svn | xargs rm -fr "
    safe_exec(cmd)

def compress_sources():
    import tarfile
    tar = TarFile(TARBALL, "w:bz2")
    tar.add(PACKAGE_NAME)
    tar.close()

def cleanup():
    """ Remove all files created. """
    if os.path.exists(TARBALL):
        os.remove(TARBALL)
    if os.path.exists(PACKAGE_NAME):
        shutil.rmtree(PACKAGE_NAME)

def run_rpm():
    shutil.rmtree(PACKAGE_NAME)
    shutil.move(TARBALL, os.path.join(RPMBUILD_HOME, "SOURCES"))
    dest_spec = os.path.join(RPMBUILD_HOME, "SPECS", RPM_SPEC)
    shutil.move(RPM_SPEC, dest_spec)
    cmd = "rpmbuild -ba " + dest_spec
    safe_exec(cmd)

def move_rpms_to_installdir(installdir):
    installer_dir = os.path.join(installdir, "installer")
    if not os.path.exists(installer_dir):
        os.makedirs(installer_dir)
        
    args = [ "find", RPMBUILD_HOME, "-name", "*.rpm" ]
    output = Popen(args, stdout=PIPE).communicate()[0];
    for rpm in output.split():
        move(rpm, installer_dir)


def main():
    """ Creates RPMs for linux. """
    parser = OptionParser(sys.argv[0] + " <installation directory>")
    parser.add_option("-v", "--verbose", action="store_true", default=False,
                      help="Show verbose output")
    options, args = parser.parse_args()
    if len(args) != 1:
        parser.error("Incorrect number of arguments")
        return 1
    installdir = args
    global verbose
    verbose = options.verbose
    setup_rpmbuild()
    cleanup()
    svn_checkout()
    compress_sources()
    run_rpm()
    move_rpms_to_installdir(installdir)
    cleanup_rpm()
    cleanup()
    
if __name__ == "__main__":
    sys.exit(main())
