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
* Author: Anton Golikov
*
* File Description: Resolve host name to ip address using preset ini-file
*
* ===========================================================================
*/

#include <util/smalldns.hpp>
#include <corelib/ncbistr.hpp>
#include <corelib/ncbireg.hpp>

BEGIN_NCBI_SCOPE

static const string s_FileDNS("./hosts.ini");

bool IsValidIP(const string& ipname)
{
    list<string> dig;
    
    NStr::Split(ipname, ".", dig);
    if( dig.size() != 4 )
        return false;

    for(list<string>::const_iterator it = dig.begin(); it != dig.end(); ++it) {
        try {
            unsigned long i = NStr::StringToULong(*it);
            if( i > 255 )
                return false;
        } catch(...) {
            return false;
        }
    }
    return true;
}

string LocalResolveDNS(const string& hostname,
                       const string& local_hosts_file /* = kEmptyStr */)
{
    if( IsValidIP(hostname) )
        return hostname;

    CNcbiIfstream is( local_hosts_file.empty() ?
                      s_FileDNS.c_str() :
                      local_hosts_file.c_str());
    if( !is.good() ) {
        _TRACE("LocalResolveDNS: cannot open hosts file: "
               << (local_hosts_file.empty() ? s_FileDNS : local_hosts_file));
        return kEmptyStr;
    }
    
    CNcbiRegistry reg(is);
    return reg.Get("LOCAL_DNS", hostname);
}

END_NCBI_SCOPE
