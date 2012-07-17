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
 * File Description:
 *   Resolve host name to ip address and back using preset ini-file
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistr.hpp>
#include <corelib/ncbireg.hpp>
#include <corelib/ncbi_safe_static.hpp>
#include <util/smalldns.hpp>
#include <util/error_codes.hpp>

#if defined(NCBI_OS_MSWIN)
#  include <winsock2.h>
#elif defined(NCBI_OS_UNIX)
#  include <unistd.h>
#  include <netdb.h>
#else
#  error "Unsupported platform"
#endif
#include <errno.h>


#define NCBI_USE_ERRCODE_X   Util_DNS


BEGIN_NCBI_SCOPE


CSmallDNS::CSmallDNS(const string& local_hosts_file /* = "./hosts.ini" */)
{
    const string section("LOCAL_DNS");
    
    CNcbiIfstream is(local_hosts_file.c_str());
    if ( !is.good() ) {
        ERR_POST_X(1, Error << "CSmallDNS: cannot open file: " << local_hosts_file);
        return;
    }
    CNcbiRegistry reg(is);
    list<string> items;
    
    reg.EnumerateEntries(section, &items);
    ITERATE(list<string>, it, items) {
        string val = reg.Get(section, *it);
        if ( !IsValidIP(val) ) {
            ERR_POST_X(2, Warning << "CSmallDNS: Bad IP address '" << val
                          << "' for " << *it);
        } else {
            m_map[*it] = val;
            m_map[val] = *it;
        }
    }
    is.close();
}
    

CSmallDNS::~CSmallDNS()
{
    return;
}


bool CSmallDNS::IsValidIP(const string& ip)
{
    list<string> dig;
    
    NStr::Split(ip, ".", dig);
    if (dig.size() != 4) {
        return false;
    }
    ITERATE(list<string>, it, dig) {
        try {
            unsigned long i = NStr::StringToULong(*it);
            if ( i > 255 ) {
                return false;
            }
        } catch(...) {
            return false;
        }
    }
    return true;
}


string CSmallDNS::GetLocalIP(void) const
{
    return LocalResolveDNS(GetLocalHost());
}


string CSmallDNS::GetLocalHost(void)
{
    static CSafeStaticPtr<string> s_LocalHostName;

    if ( s_LocalHostName->empty() ) {
#if !defined(MAXHOSTNAMELEN)
#  define MAXHOSTNAMELEN 256
#endif
        char buffer[MAXHOSTNAMELEN];
        buffer[0] = buffer[MAXHOSTNAMELEN-1] = '\0';
        errno = 0;
        if ( gethostname(buffer, (int)sizeof(buffer)) == 0 ) {
            if ( buffer[MAXHOSTNAMELEN - 1] ) {
                ERR_POST_X(3, Warning <<
                    "CSmallDNS: Host name buffer too small");
            } else {
                char* dot_pos = strstr(buffer, ".");
                if ( dot_pos ) {
                    dot_pos[0] = '\0';
                }
                *s_LocalHostName = buffer;
            }
        } else {
            ERR_POST_X(4, Warning <<
                "CSmallDNS: Cannot detect host name, errno:" << errno);
        }
    }
    return s_LocalHostName.Get();
}


string CSmallDNS::LocalResolveDNS(const string& host) const
{
    if ( IsValidIP(host) ) {
        return host;
    }
    map<string, string>::const_iterator it = m_map.find(host);
    if ( it != m_map.end() ) {
        return it->second;
    }
    return kEmptyStr;
}


string CSmallDNS::LocalBackResolveDNS(const string& ip) const
{
    if ( !IsValidIP(ip) ) {
        return kEmptyStr;
    }
    map<string, string>::const_iterator it = m_map.find(ip);
    if ( it != m_map.end() ) {
        return it->second;
    }
    return kEmptyStr;
}


END_NCBI_SCOPE
