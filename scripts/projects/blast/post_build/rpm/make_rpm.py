#!/usr/bin/env python
"""Script to create a source/binary RPM.
"""
# $Id$

import sys, os, shutil
from optparse import OptionParser
import subprocess
SCRIPT_DIR = os.path.dirname(os.path.abspath(sys.argv[0]))
sys.path.append(os.path.join(SCRIPT_DIR, ".."))
from blast_utils import *   #IGNORE:W0401

VERBOSE = False

# Name of the temporary rpmbuild directory
RPMBUILD_HOME = "rpmbuild"
PACKAGE_NAME = ""
# Name of the source TARBALL to create
TARBALL = ""
# Local RPM configuration file
RPMMACROS = os.path.join(os.path.expanduser("~"), ".rpmmacros")

def setup_rpmbuild():
    """ Prepare local rpmbuild directory. """
    cleanup_rpm()
    os.mkdir(RPMBUILD_HOME)
    for directory in [ 'BUILD', 'SOURCES', 'SPECS', 'SRPMS', 'tmp', 'RPMS' ]:
        os.mkdir(os.path.join(RPMBUILD_HOME, directory))
    cwd = os.getcwd()
    os.chdir(os.path.join(RPMBUILD_HOME, 'RPMS'))
    for subdir in [ 'i386', 'i586', 'i686', 'noarch', 'x86_64' ]:
        os.mkdir(subdir)
    os.chdir(cwd)

    # Create ~/.rpmmacros
    try:
        out = open(RPMMACROS, "w")
        print >> out, "%_topdir %( echo", os.path.join(cwd, RPMBUILD_HOME), ")"
        print >> out, "%_tmppath %( echo", 
        print >> out, os.path.join(cwd, RPMBUILD_HOME, "tmp"), ")"
        print >> out
        print >> out, "%packager Christiam E. Camacho (camacho@ncbi.nlm.nih.gov)"
    finally:
        out.close()
    if VERBOSE: 
        print "Created", RPMMACROS

def cleanup_rpm():
    """ Delete rpm files """
    if os.path.exists(RPMBUILD_HOME):
        shutil.rmtree(RPMBUILD_HOME)

    if os.path.exists(RPMMACROS):
        os.remove(RPMMACROS)

def cleanup_svn_co():
    """ Remove unnecessary directories/files from svn checkout """
    import fnmatch
           
    cmd = "find " + PACKAGE_NAME + " -type d -name .svn | xargs rm -fr "
    safe_exec(cmd) 
        
    for path in ["builds", "scripts"]:
        path = os.path.join(PACKAGE_NAME, path)
        if os.path.exists(path):
            shutil.rmtree(path)
            if VERBOSE: 
                print "Deleting", path
               
    projects_path = os.path.join(PACKAGE_NAME, "c++", "scripts", "projects")
    for root, dirs, files in os.walk(projects_path): 
        for name in files:
            name = os.path.join(root, name)
            if fnmatch.fnmatch(name, "*blast/*"): 
                continue
            if VERBOSE:
                print "Deleting file", name
            os.remove(name)
            
        for name in dirs:
            name = os.path.join(root, name)
            if fnmatch.fnmatch(name, "*blast*"):
                continue
            if VERBOSE: 
                print "Deleting directory", name
            shutil.rmtree(name)
            

def svn_checkout(blast_version):
    """Checkout BLAST sources for this release from SVN"""
    # NCBI SVN repository
    svn_ncbi = "https://svn.ncbi.nlm.nih.gov/repos_htpasswd/toolkit"

    # Check out the sources
    cmd = "svn -q co --username svnread --password allowed " + svn_ncbi
    cmd += "/release/blast/" + blast_version + " " + PACKAGE_NAME
    if os.path.exists(PACKAGE_NAME):
        shutil.rmtree(PACKAGE_NAME)
    safe_exec(cmd)

    cleanup_svn_co()

def compress_sources():
    """Compress sources to be included in source RPM"""
    import tarfile
    tar = tarfile.open(TARBALL, "w:bz2")
    tar.add(PACKAGE_NAME)
    tar.close()

def cleanup():
    """ Remove all files created. """
    if os.path.exists(TARBALL):
        os.remove(TARBALL)
    if os.path.exists(PACKAGE_NAME):
        shutil.rmtree(PACKAGE_NAME)

def run_rpm(blast_version):
    """Run the rpmbuild command"""
    shutil.rmtree(PACKAGE_NAME)
    shutil.move(TARBALL, os.path.join(RPMBUILD_HOME, "SOURCES"))
    rpm_spec = "ncbi-blast.spec"
    src = os.path.join(SCRIPT_DIR, rpm_spec)
    dest = os.path.join(RPMBUILD_HOME, "SPECS", rpm_spec)
    shutil.copyfile(src, dest)
    update_blast_version(dest, blast_version)
    cmd = "/usr/bin/rpmbuild -ba " + dest
    safe_exec(cmd)

def move_rpms_to_installdir(installdir):
    """Copy the resulting RPM files into the installation directory"""
    installer_dir = os.path.join(installdir, "installer")
    if not os.path.exists(installer_dir):
        os.makedirs(installer_dir)
        
    args = [ "find", RPMBUILD_HOME, "-name", "*.rpm" ]
    output = subprocess.Popen(args, stdout=subprocess.PIPE).communicate()[0]
    for rpm in output.split():
        if VERBOSE: 
            print "mv", rpm, installer_dir
        shutil.move(rpm, installer_dir)


def main():
    """ Creates RPMs for linux. """
    parser = OptionParser("%prog <blast_version> <installation directory>")
    parser.add_option("-v", "--verbose", action="store_true", default=False,
                      help="Show verbose output", dest="VERBOSE")
    options, args = parser.parse_args()
    if len(args) != 2:
        parser.error("Incorrect number of arguments")
        return 1
    
    blast_version, installdir = args
    global VERBOSE, PACKAGE_NAME, TARBALL #IGNORE:W0603
    VERBOSE = options.VERBOSE
    if VERBOSE: 
        print "Installing RPM to", installdir
        
    PACKAGE_NAME = "ncbi-blast-" + blast_version + "+"
    TARBALL = PACKAGE_NAME + ".tgz"
    
    setup_rpmbuild()
    cleanup()
    svn_checkout(blast_version)
    compress_sources()
    run_rpm(blast_version)
    move_rpms_to_installdir(installdir)
    cleanup_rpm()
    cleanup()
    
if __name__ == "__main__":
    sys.exit(main())
