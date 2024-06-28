#ifndef CGI___CAF_PLAIN__HPP
#define CGI___CAF_PLAIN__HPP
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
* Author:  Anatoliy Kuznetsov
*
* File Description:
*   Non-encoded implementation of cookie affinity interface
*
*/

#include <caf.hpp>
#include <connect/ext/ncbi_ifconf.h>
#include <stdlib.h>
#include <string.h>


/** @addtogroup CookieAffinity
 *
 * @{
 */


BEGIN_NCBI_SCOPE


///////////////////////////////////////////////////////
//
//  CCookieAffinity_Plain::
//
// Cookie affinity service interface implementation with no encoding
//

class CCookieAffinity_Plain : public CCookieAffinity
{
public:
    // Return copy of the "str", or 0 if failed.
    // The returned string is to be free()'d by the caller.
    virtual char* Encode(const char* str, const char* key);

    // Return IP address of the current host, or 0 if failed.
    virtual char* GetHostIP(char* buf, size_t bufsize);
};


/* @} */


/////////////////////////////////////////////////////////////////////////////
//  IMPLEMENTATION of INLINE functions
/////////////////////////////////////////////////////////////////////////////


inline char* CCookieAffinity_Plain::Encode(const char* str,
                                           const char* /*key*/)
{
    size_t len = ::strlen(str);
    if ( !len )
        return 0;
    char* buf = (char*) ::malloc(len + 1);
    ::strcpy(buf, str);
    return buf;
}


inline char* CCookieAffinity_Plain::GetHostIP(char* buf, size_t bufsize)
{
    return NcbiGetHostIP(buf, bufsize);
}


END_NCBI_SCOPE

#endif  /* CGI___CAF_PLAIN__HPP */
