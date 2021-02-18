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
* Author:  Aaron Ucko
*
* File Description:
*   Database service name to server mapping policy.
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <corelib/impl/ncbi_dbsvcmapper.hpp>
#include <corelib/ncbierror.hpp>

BEGIN_NCBI_SCOPE


CEndpointKey::CEndpointKey(const CTempString& name, NStr::TConvErrFlags flags)
{
    CTempString  address  = name;
    SIZE_TYPE    pos      = name.find_first_not_of("0123456789.:");
    if (pos != NPOS) {
        string msg = ("Unsupported endpoint key character "
                      + NStr::CEncode(CTempString(name.substr(pos, 1))));
        if ((flags & NStr::fConvErr_NoThrow) == 0) {
            NCBI_THROW2(CStringException, eFormat, msg, pos);
        } else {
            if ((flags & NStr::fConvErr_NoErrMessage) != 0) {
                CNcbiError::Set(CNcbiError::eInvalidArgument);
            } else {
                CNcbiError::Set(CNcbiError::eInvalidArgument,
                                FORMAT(msg << " at position " << pos));
            }
            m_Value = 0;
            return;
        }
    }
    Uint2 port = 0;
    pos = address.find(':');
    if (pos != NPOS) {
        if ( !NStr::StringToNumeric(address.substr(pos + 1), &port, flags)) {
            m_Value = 0;
            return;
        }
        address = address.substr(0, pos);
    }
    union { // to help produce network byte order
        Uint4 i;
        Uint1 c[4];
    } host = { 0, };
    if (count(address.begin(), address.end(), '.') != 3) {
        string msg = "Wrong number of components in IP address " + address;
        if ((flags & NStr::fConvErr_NoThrow) == 0) {
            NCBI_THROW2(CStringException, eFormat, msg, address.size());
        } else {
            if ((flags & NStr::fConvErr_NoErrMessage) != 0) {
                CNcbiError::Set(CNcbiError::eInvalidArgument);
            } else {
                CNcbiError::Set(CNcbiError::eInvalidArgument, msg);
            }
            m_Value = 0;
            return;
        }
        return;
    }
    for (int i = 0;  i < 4;  ++i) {
        CTempString component;
        switch (i) {
        case 0:
            pos = address.find('.');
            component = address.substr(0, pos);
            break;
        case 1: case 2:
        {
            SIZE_TYPE pos2 = address.find('.', pos + 1);
            component = address.substr(pos + 1, pos2 - pos - 1);
            pos = pos2;
            break;
        }
        case 3:
            component = address.substr(pos + 1);
            break;
        default:
            _TROUBLE;
        }
        if ( !NStr::StringToNumeric(component, &host.c[i], flags)) {
            m_Value = 0;
            return;
        }
        
    }
    m_Value = (host.i << 16) | port;
}


ostream& operator<<(ostream& os, const CEndpointKey& key)
{
    auto host = key.GetHost();
    const unsigned char* b = (const unsigned char*) &host;
    os << Uint4(b[0]) << '.' << Uint4(b[1]) << '.' << Uint4(b[2]) << '.'
       << Uint4(b[3]);
    auto port = key.GetPort();
    if (port != 0) {
        os << ':' << port;
    }
    return os;
}


void IDBServiceMapper::GetServerOptions(const string& name, TOptions* options)
{
    list<string> servers;
    GetServersList(name, &servers);
    options->clear();
    CFastMutexGuard mg(m_Mtx);
    const auto& exclusions = m_ExcludeMap[name];
    for (const string& it : servers) {
        options->emplace_back(new CDBServerOption(it, 0, 0, 1.0));
        auto lb = exclusions.lower_bound(options->back());
        if (lb != exclusions.end()  &&  (*lb)->GetName() == name) {
            options->back()->m_State |= CDBServerOption::fState_Excluded;
        }
    }
}


END_NCBI_SCOPE
