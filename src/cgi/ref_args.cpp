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
    NStr::Split(definitions, "\n", defs);
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
    NStr::Split(arg_names, ",", arg_list);
    ITERATE(TArgList, arg, arg_list) {
        m_HostMap.insert(
            THostMap::value_type(host_mask, NStr::TruncateSpaces(*arg)));
    }
}


string CRefArgs::GetQueryString(const string& referrer) const
{
    // Remove http:// prefix
    SIZE_TYPE pos = NStr::Find(referrer, "://");
    string host =  (pos != NPOS) ?
        referrer.substr(pos+3, referrer.size()) : referrer;

    // Find end of host name
    pos = NStr::Find(host, "/");
    if (pos == NPOS) {
        return kEmptyStr;
    }

    string args = host.substr(pos + 1, host.size());
    host = host.substr(0, pos);
    // Find arguments if any
    pos = NStr::Find(args, "?");
    if (pos == NPOS) {
        // No arguments - nothing to process
        return kEmptyStr;
    }
    args = args.substr(pos + 1, args.size());
    ITERATE(THostMap, it, m_HostMap) {
        if (NStr::FindNoCase(host, it->first) == NPOS) {
            continue;
        }
        TCgiEntries entries;
        CCgiRequest::ParseEntries(args, entries);
        TCgiEntriesCI query = entries.find(it->second);
        if (query != entries.end()) {
            return string(query->second);
        }
    }
    return kEmptyStr;
}


bool CRefArgs::IsListedHost(const string& host) const
{
    ITERATE(THostMap, it, m_HostMap) {
        if (NStr::FindNoCase(host, it->first) != NPOS) {
            return true;
        }
    }
    return false;
}


END_NCBI_SCOPE



/*
* ===========================================================================
* $Log$
* Revision 1.4  2005/02/17 20:27:25  grichenk
* Added IsListedHost()
*
* Revision 1.3  2004/12/16 20:11:48  yasmax
* Corrected length of host
*
* Revision 1.2  2004/12/09 16:35:45  grichenk
* Added GetDefaultDefinitions. Doxygenized comments.
*
* Revision 1.1  2004/12/07 18:51:58  grichenk
* Initial revision
*
* ==========================================================================
*/
