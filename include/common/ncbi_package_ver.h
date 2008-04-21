/*  $Id$
 * ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnology Information
 *
 *  This software/database is a "United States Government Work" under the
 *  terms of the United States Copyright Act.  It was written as part of
 *  the author's official duties as a United States Government employee and
 *  thus cannot be copyrighted.  This software/database is freely available
 *  to the public for use. The National Library of Medicine and the U.S.
 *  Government have not placed any restriction on its use or reproduction.
 *
 *  Although all reasonable efforts have been taken to ensure the accuracy
 *  and reliability of the software and data, the NLM and the U.S.
 *  Government do not and cannot warrant the performance or results that
 *  may be obtained by using this software or data. The NLM and the U.S.
 *  Government disclaim all warranties, express or implied, including
 *  warranties of performance, merchantability or fitness for any particular
 *  purpose.
 *
 *  Please cite the author in any work or product based on this material.
 *
 * ===========================================================================
 */


/*
 * Note:
 * the content of this file will be overwritten by the release scripts.
 * The following define directive will be added:
 * #define NCBI_PACKAGE     1
 *
 * The define directives below will be replaced with the following:
 * #define NCBI_PACKAGE_NAME            "netschedule"
 * #define NCBI_PACKAGE_VERSION_MAJOR   3
 * #define NCBI_PACKAGE_VERSION_MINOR   2
 * #define NCBI_PACKAGE_VERSION_PATCH   10
 * #define NCBI_PACKAGE_CONFIG          "GCC_342-Debug--i686-pc-linux2.6.5-..."
 * where the certain package name and the version are taken from 
 * the release script parameters.
 */


#define NCBI_PACKAGE_NAME           "unknown"
#define NCBI_PACKAGE_VERSION_MAJOR  0
#define NCBI_PACKAGE_VERSION_MINOR  0
#define NCBI_PACKAGE_VERSION_PATCH  0
#define NCBI_PACKAGE_CONFIG         ""

#define __STR(x)                    #x

#define NCBI_PACKAGE_VERSION       __STR(NCBI_PACKAGE_VERSION_MAJOR)  "." \
                                   __STR(NCBI_PACKAGE_VERSION_MINOR)  "." \
                                   __STR(NCBI_PACKAGE_VERSION_PATCH)


