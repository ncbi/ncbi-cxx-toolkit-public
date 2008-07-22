#!/usr/bin/env python
# $Id$
# Script to create a source RPM (in progress)

import sys, os, shutil;

# Current version of BLAST
blast_version = "2.2.18"
# Name of the temporary rpmbuild directory
rpmbuild_home = "rpmbuild"
# NCBI SVN repository
svn_ncbi = "https://svn.ncbi.nlm.nih.gov/repos/toolkit"
package_name = "ncbi-blast-" + blast_version + "+"
# Name of the source tarball to create
tarball = package_name + ".tgz"

def setup_rpmbuild():
    """ Prepare local rpmbuild directory. """
    global rpmbuild_home
    shutil.rmtree(rpmbuild_home)
    for d in [ 'BUILD', 'SOURCES', 'SPECS', 'SRPMS', 'tmp', 'RPMS' ]:
        os.mkdir(d)
    cwd = os.getcwd()
    os.chdir('RPMS')
    for d in [ 'i386', 'i586', 'i686', 'noarch', 'x86_64' ]:
        os.mkdir(d)
    os.chdir(cwd)

    # Create ~/.rpmmacros
    fname = os.path.join(os.path.expanduser("~"), ".rpmmacros");
    try:
        f = open(fname, "w")
        print >> f, "%_topdir %( echo", rpmbuild_home, ")"
        print >> f, "%_tmppath %{_topdir}/tmp"
        print >> f
        print >> f, "%packager Christiam E. Camacho (camacho@ncbi.nlm.nih.gov)"
    finally:
        f.close()

def cleanup():
    """ Remove all files created. """
    global tarball
    os.remove(tarball)
    shutil.rmtree(package_name)

def main():
    setup_rpmbuild()
    
if __name__ == "__main__":
    sys.exit(main())
