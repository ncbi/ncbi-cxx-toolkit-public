#ifndef FASTME_H
#define FASTME_H

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
*
* Author:  Richard Desper
*
* File Description:  fastme.h
*
*    A part of the Miminum Evolution algorithm
*
*/

#include <corelib/ncbistl.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(fastme)

#define MAX_FILE_NAME_LENGTH 50
#define MAX_EVENT_NAME 24
#ifdef INFINITY
#  undef INFINITY
#endif
#define INFINITY 10000000
#define NEGINFINITY -10000000
#define NONE 0
#define UP 1
#define DOWN 2
#define LEFT 3
#define RIGHT 4
#define SKEW 5

typedef char boolean;

#ifndef true_fastme
#define true_fastme 1
#endif

#ifndef TRUE_FASTME
#define TRUE_FASTME 1
#endif

#ifndef false_fastme
#define false_fastme 0
#endif
#ifndef FALSE_FASTME
#define FALSE_FASTME 0
#endif
#ifndef EXIT_FAILURE
#define EXIT_FAILURE (-1)
#endif

#define ReadOpenParenthesis 0
#define ReadSubTree 1
#define ReadLabel 2
#define ReadWeight 3
#define ReadSize 4
#define ReadEntries 5
#define Done 6

#define MAXSIZE 70000

extern int verbose;

boolean whitespace(char c);

END_SCOPE(fastme)
END_NCBI_SCOPE

#include "fastme_common.h"
#include <corelib/ncbistd.hpp>
#endif   /*  FASTME_H */


/*
 * ===========================================================================
 * $Log$
 * Revision 1.3  2006/12/14 19:59:17  jcherry
 * Eliminated compiler warnings and fixed static/extern in header files
 *
 * Revision 1.2  2004/02/10 20:24:22  ucko
 * Get rid of any previous definition of INFINITY before supplying our
 * own, since some compilers (such as IBM VisualAge) forbid immediate
 * redefinitions.
 *
 * Revision 1.1  2004/02/10 15:16:01  jcherry
 * Initial version
 *
 * ===========================================================================
 */
