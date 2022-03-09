#!/bin/sh
#
# Create binary RPS package from already compiled tar file
#
# usage: repack_rpm.sh ncbi-blast-2.13.0+-x64-linux.tar.gz
# 
#
RPM_RELEASE_NUMBER=1

TMP_PKG="tmp_pkg"
VERSION="0.0.0"
if [ "X$1" = "X" ] ; then
	echo "This script creates linux x64 RPM package from given BLAST+ binary tar archive"
	echo "please check RPM_RELEASE_NUMBER inside script. normally it should be 1 ( current: ${RPM_RELEASE_NUMBER} )"
	echo "usage:"
	echo "       $0 ncbi-blast-N.N.N+-x64-linux.tar.gz [VERSION]"
	exit 0
fi

TARGZ_ARCH="$1"
if [ "X$2" != "X" ] ; then
     VERSION="$2"
else
	FNAME=`basename ${TARGZ_ARCH}`
	TEST_STR="X"`echo  $FNAME | awk -F- '{ print $2}'  | tr -d "+"`
	if [ ${TEST_STR} != "X" ] ; then
	    VERSION=`echo  $FNAME | cut -d- -f3 | tr -d "+"`
	fi
fi
#.....................................................................................
echo "INFO: INPUT: TARBALL: ${TARGZ_ARCH}"
echo "INFO: INPUT: BLAST VERSION: ${VERSION}"
echo "INFO: INPUT: RPM_RELEASE_NUMBER: ${RPM_RELEASE_NUMBER}"


# ************************************************************************************
# INFO: create temporary directoris for RPM content
# ************************************************************************************
if [ -d ${TMP_PKG} ] ; then
	rm -rf ${TMP_PKG}
fi
mkdir -p "${TMP_PKG}/usr"
if [ ! -d "${TMP_PKG}/usr" ] ; then
	echo "ERROR: Can't create ${TMP_PKG}/usr directory, exiting"
	exit 1
fi
# ************************************************************************************



# ************************************************************************************
# INFO: unpack given BLAST+ archive
# ************************************************************************************
tar -C "${TMP_PKG}/usr" --strip-components=1  -zxvf ${TARGZ_ARCH} ncbi-blast-${VERSION}+/bin/
tar -C "${TMP_PKG}/usr" --strip-components=1  -zxvf ${TARGZ_ARCH} ncbi-blast-${VERSION}+/doc/

# TEMP FIX: add BLAST_PRIVACY to a directory structure
# TODO: get content of the BLAST_PRIVACY form tar.gz once it will be there ( SB-3337)
cat > ${TMP_PKG}/usr/doc/BLAST_PRIVACY<<EOF
As part of our efforts to improve BLAST+, we have implemented usage reporting
to collect limited data. This information shows us whether BLAST+ is being
used by the community, and therefore is worth being maintained and developed
by NCBI. It also allows us to focus our development efforts on the most used
aspects of BLAST+.

See https://www.ncbi.nlm.nih.gov/books/NBK569851/
for what data we collect and how to opt out of usage reporting.
EOF

# ************************************************************************************
# INFO: create spec file for rpmbuild
# ************************************************************************************
cat > repack_bin.spec<<EOF
Name:        ncbi-blast
Version:     ${VERSION}
Release:     ${RPM_RELEASE_NUMBER}
Summary:     NCBI BLAST finds regions of similarity between biological sequences.
Exclusiveos: linux
License:     Public Domain
BuildArch:   i686 x86_64
Distribution: Distribution, NCBI BLAST finds regions of similarity between biological sequences.
Group:        NCBI/BLAST
Packager:     Christiam E. Camacho (camacho@ncbi.nlm.nih.gov)
Prefix:       /usr

AutoReqProv: no
Requires:    /usr/bin/perl
Requires:    ld-linux-x86-64.so.2()(64bit)
Requires:    ld-linux-x86-64.so.2(GLIBC_2.3)(64bit)
Requires:    libbz2.so.1()(64bit)
Requires:    libc.so.6()(64bit)
Requires:    libc.so.6(GLIBC_2.2.5)(64bit)
Requires:    libc.so.6(GLIBC_2.3)(64bit)
Requires:    libc.so.6(GLIBC_2.3.2)(64bit)
Requires:    libc.so.6(GLIBC_2.7)(64bit)
Requires:    libdl.so.2()(64bit)
Requires:    libdl.so.2(GLIBC_2.2.5)(64bit)
Requires:    libgcc_s.so.1()(64bit)
Requires:    libgcc_s.so.1(GCC_3.0)(64bit)
Requires:    libm.so.6()(64bit)
Requires:    libm.so.6(GLIBC_2.2.5)(64bit)
Requires:    libnsl.so.1()(64bit)
Requires:    libpthread.so.0()(64bit)
Requires:    libpthread.so.0(GLIBC_2.2.5)(64bit)
Requires:    libpthread.so.0(GLIBC_2.3.2)(64bit)
Requires:    librt.so.1()(64bit)
Requires:    libstdc++.so.6()(64bit)
Requires:    libstdc++.so.6(CXXABI_1.3)(64bit)
Requires:    libstdc++.so.6(CXXABI_1.3.1)(64bit)
Requires:    libstdc++.so.6(GLIBCXX_3.4)(64bit)
Requires:    libstdc++.so.6(GLIBCXX_3.4.5)(64bit)
Requires:    libz.so.1()(64bit)
Requires:    libz.so.1(ZLIB_1.2.0)(64bit)
Requires:    perl(Archive::Tar)
Requires:    perl(Digest::MD5)
Requires:    perl(File::Temp)
Requires:    perl(File::stat)
Requires:    perl(Getopt::Long)
Requires:    perl(List::MoreUtils)
Requires:    perl(Net::FTP)
Requires:    perl(Pod::Usage)
Requires:    perl(constant)
Requires:    perl(strict)
Requires:    perl(warnings)
Requires:    rpmlib(CompressedFileNames) <= 3.0.4-1
Requires:    rpmlib(FileDigests) <= 4.6.0-1
Requires:    rpmlib(PayloadFilesHavePrefix) <= 4.0-1
Requires:    rpmlib(PayloadIsXz) <= 5.2-1

%description
The NCBI Basic Local Alignment Search Tool (BLAST) finds regions of
local similarity between sequences. The program compares nucleotide or
protein sequences to sequence databases and calculates the statistical
significance of matches. BLAST can be used to infer functional and
evolutionary relationships between sequences as well as help identify
members of gene families.

%files
%defattr(-,root,root)
/usr/bin/*
/usr/doc/README.txt
/usr/doc/BLAST_PRIVACY

%changelog
* Tue Mar 20 2012 <blast-help@ncbi.nlm.nih.gov>
- See release notes in http://www.ncbi.nlm.nih.gov/books/NBK131777
EOF
if [ ! -f "repack_bin.spec" ] ; then
	echo "ERROR: Can't create eyck_bin.spec file exiting"
	exit 2
fi
# ************************************************************************************


# ************************************************************************************
# INFO: create RPM package
# ************************************************************************************
if [ -d "$(pwd)/x86_64" ] ; then
	rm -rf "$(pwd)/x86_64"
fi

rpmbuild -D "_rpmdir $(pwd)" --buildroot `pwd`/tmp_pkg    -bb repack_bin.spec
OUT_RPM="$(pwd)/x86_64/ncbi-blast-${VERSION}-${RPM_RELEASE_NUMBER}.x86_64.rpm"
# 
if [ ! -f "${OUT_RPM}" ] ; then
	echo "ERROR: Can't create ${OUT_RPM} file, exiting"
	exit 2
fi
# ************************************************************************************


# ************************************************************************************
# INFO: print information about RPM package
# ************************************************************************************
rpm -qip ${OUT_RPM}
echo "# ************************************************************************************"
rpm -qlp ${OUT_RPM}
echo "# ************************************************************************************"
echo "# OUTPUT RPM: ./x86_64/ncbi-blast-${VERSION}-${RPM_RELEASE_NUMBER}.x86_64.rpm"
echo "#  FULL PATH: ${OUT_RPM}"
echo "#                                                                                     "
echo "#                                                                                     "
echo "#                                                                                     "
echo "# ************************************************************************************"

