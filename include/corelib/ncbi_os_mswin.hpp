#ifndef CORELIB___NCBI_OS_MSWIN__HPP
#define CORELIB___NCBI_OS_MSWIN__HPP

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
 * Author:  Denis Vakatov, Vladimir Ivanov
 *
 * File Description:
 *   Use this header in the place of <windows.h> to avoid various
 *   MS-Windows "popular" preprocessor macros which screw other people's
 *   code. -- This header redefines such macros with inline functions.
 *
 */

#include <ncbiconf.h>
#if !defined(NCBI_OS_MSWIN)
#  error "ncbi_os_mswin.hpp must be used on MS Windows platforms only"
#endif

// Exclude some old stuff from <windows.h>. 
#if defined(_MSC_VER)  &&  (_MSC_VER > 1200)
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

// GetObject

#ifdef GetObject
#undef GetObject
inline int GetObject(HGDIOBJ hGdiObj, int cbBuffer, LPVOID lpvObject)
{
#  ifdef _UNICODE
    return GetObjectW(hGdiObj, cbBuffer, lpvObject);
#  else
    return GetObjectA(hGdiObj, cbBuffer, lpvObject);
#  endif
}
#endif


#endif  /* CORELIB___NCBI_OS_MSWIN__HPP */
