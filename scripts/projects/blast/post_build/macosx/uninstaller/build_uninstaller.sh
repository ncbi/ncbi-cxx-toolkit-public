#!/bin/sh
#
#  $Id$
# * ===========================================================================
# *
# *                            PUBLIC DOMAIN NOTICE
# *               National Center for Biotechnology Information
# *
# *  This software/database is a "United States Government Work" under the
# *  terms of the United States Copyright Act.  It was written as part of
# *  the author's official duties as a United States Government employee and
# *  thus cannot be copyrighted.  This software/database is freely available
# *  to the public for use. The National Library of Medicine and the U.S.
# *  Government have not placed any restriction on its use or reproduction.
# *
# *  Although all reasonable efforts have been taken to ensure the accuracy
# *  and reliability of the software and data, the NLM and the U.S.
# *  Government do not and cannot warrant the performance or results that
# *  may be obtained by using this software or data. The NLM and the U.S.
# *  Government disclaim all warranties, express or implied, including
# *  warranties of performance, merchantability or fitness for any particular
# *  purpose.
# *
# *  Please cite the author in any work or product based on this material.
# *
# * ===========================================================================
# *
# * Authors:  Yury Merezhuk
# *
# 

ASCRIPT=uninstall_ncbi_blast.applescript
APP_NAME=uninstall_ncbi_blast.app
ZIP_NAME=uninstall_ncbi_blast.zip
OSACOMPILE="/usr/bin/osacompile"

#
# Check re requisites
#
if [ ! -f ${ASCRIPT} ] ; then
	echo "Error: missing ${ASCRIPT}"
	exit 1
fi

if [ ! -x ${OSACOMPILE}  ] ; then
	echo "Error: missing AppleScript compiler ${OSACOMPILE}"
	exit 2
fi
if [ X`which zip` = X  ]  ; then
	echo "Error: missing zip utility."
	exit 22
fi
if [ X`which unzip` = X   ]  ; then
	echo "Error: missing unzip utility."
	exit 23
fi
if [ X`which awk` = X   ]  ; then
	echo "Error: missing awk utility."
	exit 23
fi


# Some clean ups
if [ -f ${ZIP_NAME} ] ; then
	tm_mark=`date +%s`
	mv ${ZIP_NAME} ${ZIP_NAME}.${tm_mark}
	if [ ! -f ${ZIP_NAME}.${tm_mark} ] ; then
		echo "Error: Can't move old ${ZIP_NAME} to ${ZIP_NAME}.${tm_mark}"
		exit 3
	else
		echo "Info: old ${ZIP_NAME} renamed to ${ZIP_NAME}.${tm_mark}"
	fi
fi

if [ -d ${APP_NAME} ] ; then
	rm -rf ${APP_NAME}
fi

# ..... Compile new version
${OSACOMPILE} -o ${APP_NAME} ${ASCRIPT}
if [ ! -d ${APP_NAME} ] ; then
	echo "Error: Can't compile ${ASCRIPT} to Application"
	exit 4
fi
echo "Info:"
echo "Info: Application ${APP_NAME} created"
find uninstall_ncbi_blast.app | awk '{ print "Info: "$0}'
#  .... create zip archive
zip -q -r ${ZIP_NAME}  ${APP_NAME} 
if [ ! -f ${ZIP_NAME} ] ; then
	echo "Error: Can't create zip archive from ${APP_NAME}"
	exit 5
fi
echo "Info:"
echo "Info: Zip archive ${ZIP_NAME} created"
unzip -l uninstall_ncbi_blast.zip |  awk '{ print "Info: "$0}'
echo "Info: Done."



	
	

