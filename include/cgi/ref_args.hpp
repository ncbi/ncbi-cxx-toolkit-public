#ifndef REF_ARGS__HPP
#define REF_ARGS__HPP

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
* Author:  Aleksey Grichenko
*
* File Description:
*   NCBI C++ CGI API:
*      Referrer args - extract query string from HTTP referrers
*/

#include <corelib/ncbistd.hpp>
#include <map>

/** @addtogroup CGIReqRes
 *
 * @{
 */


BEGIN_NCBI_SCOPE

////////////////////////////////////////////////////////
///
///  CRefArgs::
///
///    Extract query string from HTTP referrers
///

class NCBI_XCGI_EXPORT CRefArgs
{
public:
    /// Create referrer parser from a set of definitions.
    /// @param definitions
    ///  Multiple definitions should be separated by new line ('\n').
    ///  Host mask should be followed by space(s).
    ///  Multiple argument names should be separated with commas.
    ///  E.g. ".google. q, query\n.foo. bar".
    CRefArgs(const string& definitions = kEmptyStr);
    ~CRefArgs(void);

    /// Add mappings between host mask and CGI argument name for query string.
    /// @sa CRefArgs::CRefArgs
    void AddDefinitions(const string& definitions);
    void AddDefinitions(const string& host_mask, const string& arg_names);

    /// Find query string in the referrer.
    /// @param referrer
    ///  Full HTTP referrer
    /// @return
    ///  Query string assigned to one of the names associated with the host
    ///  in the referrer or empty string.
    string GetQueryString(const string& referrer) const;

    /// Get default set of search engine definitions.
    static string GetDefaultDefinitions(void);

    /// Check if the host from the referrer string is listed in definitions.
    bool IsListedHost(const string& referrer) const;

private:
    typedef multimap<string, string> THostMap;

    THostMap    m_HostMap;
};


END_NCBI_SCOPE

/*
* ===========================================================================
* $Log$
* Revision 1.4  2005/02/18 17:45:53  grichenk
* IsListedHost() accepts referrer string rather than host name.
*
* Revision 1.3  2005/02/17 20:27:25  grichenk
* Added IsListedHost()
*
* Revision 1.2  2004/12/09 16:35:45  grichenk
* Added GetDefaultDefinitions. Doxygenized comments.
*
* Revision 1.1  2004/12/07 18:51:58  grichenk
* Initial revision
*
* ==========================================================================
*/

#endif  /* REF_ARGS__HPP */
