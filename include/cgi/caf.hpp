#ifndef CGI___CAF__HPP
#define CGI___CAF__HPP

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
*/

#include <corelib/ncbistl.hpp>

#include <stddef.h>


/** @addtogroup CookieAffinity
 *
 * @{
 */


BEGIN_NCBI_SCOPE


///////////////////////////////////////////////////////
//
// CCookieAffinity::
//
// Cookie affinity service interface definition.
//
//

class NCBI_XCGI_EXPORT CCookieAffinity
{
public:
    virtual ~CCookieAffinity() {}

    // Return result of encryption of "string" with "key"; 0 if failed
    // return string to be free()'d by the caller
    virtual char* Encode(const char* str, const char* key) = 0;

    // Return IP address of the current host, 0 if failed
    virtual char* GetHostIP(char* buf, size_t bufsize) = 0;
};


END_NCBI_SCOPE


/* @} */

#endif  /* CGI___CAF__HPP */
