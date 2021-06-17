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

#include <ncbi_pch.hpp>
#include <cgi/ref_args.hpp>
#include <cgi/ncbicgi.hpp>
#include <cgi/cgi_util.hpp>
#include <cgi/cgi_exception.hpp>
#include <cgi/error_codes.hpp>

#define NCBI_USE_ERRCODE_X  Cgi_API

BEGIN_NCBI_SCOPE


string CRefArgs::GetDefaultDefinitions(void)
{
    const char* kDefaultEngineDefinitions =
        ".google. q, query\n"
        ".yahoo. p\n"
        ".msn. q, p\n"
        ".altavista. q\n"
        "aolsearch. query\n"
        ".scirus. q\n"
        ".lycos. query\n"
        "search.netscape query\n";
    return kDefaultEngineDefinitions;
}


CRefArgs::CRefArgs(const string& definitions)
{
    AddDefinitions(definitions);
}


CRefArgs::~CRefArgs(void)
{
    return;
}


void CRefArgs::AddDefinitions(const string& definitions)
{
    typedef list<string> TDefList;
    TDefList defs;
    NStr::Split(definitions, "\n", defs,
        NStr::fSplit_MergeDelimiters | NStr::fSplit_Truncate);
    ITERATE(TDefList, def, defs) {
        string host, args;
        if ( NStr::SplitInTwo(*def, " ", host, args) ) {
            AddDefinitions(host, args);
        }
    }
}


void CRefArgs::AddDefinitions(const string& host_mask,
                              const string& arg_names)
{
    typedef list<string> TArgList;
    TArgList arg_list;
    NStr::Split(arg_names, ",", arg_list,
        NStr::fSplit_MergeDelimiters | NStr::fSplit_Truncate);
    ITERATE(TArgList, arg, arg_list) {
        m_HostMap.insert(
            THostMap::value_type(host_mask, NStr::TruncateSpaces(*arg)));
    }
}


string CRefArgs::GetQueryString(const string& referrer) const
{
    try {
        CUrl url(referrer);
        ITERATE(THostMap, it, m_HostMap) {
            if (NStr::FindNoCase(url.GetHost(), it->first) == NPOS) {
                continue;
            }
            if (url.HaveArgs()  &&  url.GetArgs().IsSetValue(it->second)) {
                return url.GetArgs().GetValue(it->second);
            }
        }
    } catch (CUrlException& ex) {
        ERR_POST_X(7, Warning << "Ignoring malformed HTTP referrer " << ex);
    }
    return kEmptyStr;
}


bool CRefArgs::IsListedHost(const string& referrer) const
{
    // Remove http:// prefix
    SIZE_TYPE pos = NStr::Find(referrer, "://");
    string host =  (pos != NPOS) ?
        referrer.substr(pos+3, referrer.size()) : referrer;

    // Find end of host name
    pos = NStr::Find(host, "/");
    if (pos != NPOS) {
        host = host.substr(0, pos);
    }

    ITERATE(THostMap, it, m_HostMap) {
        if (NStr::FindNoCase(host, it->first) != NPOS) {
            return true;
        }
    }
    return false;
}


END_NCBI_SCOPE
