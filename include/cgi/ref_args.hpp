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
#include <vector>
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

class CRefArgs
{
public:
    CRefArgs(void);
    /// Create referrer parser from a set of definitions.
    /// Multiple definitions should be separated by new line.
    /// Host mask should be followed by space(s).
    /// Multiple argument names should be separated with commas.
    /// E.g. ".google. q, query".
    CRefArgs(const string& definitions);
    ~CRefArgs(void);

    /// Add mappings between host mask and CGI argument name for query string.
    /// @sa CRefArgs::CRefArgs
    void AddDefinitions(const string& definitions);
    void AddDefinitions(const string& host_mask, const string& arg_names);

    /// Return query string in the referrer or empty string.
    string GetQueryString(const string& referrer) const;

private:
    typedef vector<string>         TArgNames;
    typedef map<string, TArgNames> THostMap;

    THostMap    m_HostMap;
};


/// Default set of search engine definitions.
const string kDefaultEngineDefinitions =
    ".google. q, query\n"
    ".yahoo. p\n"
    ".msn. q, p\n"
    ".altavista. q\n"
    "aolsearch. query\n"
    ".scirus. q\n"
    ".lycos. query\n"
    "search.netscape query\n";


inline
CRefArgs::CRefArgs(void)
{
    return;
}


inline
CRefArgs::CRefArgs(const string& definitions)
{
    AddDefinitions(definitions);
}


inline
CRefArgs::~CRefArgs(void)
{
    return;
}


END_NCBI_SCOPE

/*
* ===========================================================================
* $Log$
* Revision 1.1  2004/12/07 18:51:58  grichenk
* Initial revision
*
* ==========================================================================
*/

#endif  /* REF_ARGS__HPP */
