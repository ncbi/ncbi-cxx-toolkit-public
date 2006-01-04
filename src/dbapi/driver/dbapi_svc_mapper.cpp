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
* Author:  Sergey Sikorskiy
*
*/

#include <ncbi_pch.hpp>

#include <dbapi/driver/dbapi_svc_mapper.hpp>
#include <corelib/ncbiapp.hpp>
#include <algorithm>

USING_NCBI_SCOPE;


///////////////////////////////////////////////////////////////////////////////
CDBDefaultServiceMapper::CDBDefaultServiceMapper(void)
: IDBServiceMapper("DEFAULT_NAME_MAPPER")
{
}

CDBDefaultServiceMapper::~CDBDefaultServiceMapper(void)
{
}

void    
CDBDefaultServiceMapper::Configure(const IRegistry*)
{
    // Do nothing.
}

TSvrRef 
CDBDefaultServiceMapper::GetServer(const string& service)
{
    if (m_SrvSet.find(service) != m_SrvSet.end()) {
        return TSvrRef();
    }
    
    return TSvrRef(new CDBServer(service));
}

void    
CDBDefaultServiceMapper::Exclude(const string&  service, 
                                 const TSvrRef& server)
{
    CFastMutexGuard mg(m_Mtx); 
    
    m_SrvSet.insert(service);
}

void    
CDBDefaultServiceMapper::SetPreference(const string&, 
                                       const TSvrRef&, 
                                       double)
{
    // Do nothing.
}

///////////////////////////////////////////////////////////////////////////////
CDBServiceMapperCoR::CDBServiceMapperCoR(void)
: IDBServiceMapper("COR_NAME_MAPPER")
{
}

CDBServiceMapperCoR::CDBServiceMapperCoR(const string& name)
: IDBServiceMapper(name)
{
}

CDBServiceMapperCoR::~CDBServiceMapperCoR(void)
{
}

void    
CDBServiceMapperCoR::Configure(const IRegistry* registry)
{
    CFastMutexGuard mg(m_Mtx); 
    
    ConfigureFromRegistry(registry);
}

TSvrRef 
CDBServiceMapperCoR::GetServer(const string& service)
{
    CFastMutexGuard mg(m_Mtx); 
    
    TSvrRef server;
    TDelegates::reverse_iterator dg_it = m_Delegates.rbegin();
    TDelegates::reverse_iterator dg_end = m_Delegates.rend();
    
    for (; server.Empty() && dg_it != dg_end; ++dg_it) {
        server = (*dg_it)->GetServer(service);
    }
    
    return server;
}

void    
CDBServiceMapperCoR::Exclude(const string&  service, 
                             const TSvrRef& server)
{
    CFastMutexGuard mg(m_Mtx); 
    
    NON_CONST_ITERATE(TDelegates, dg_it, m_Delegates) {
        (*dg_it)->Exclude(service, server);
    }
}

void    
CDBServiceMapperCoR::SetPreference(const string&  service, 
                                   const TSvrRef& preferred_server,
                                   double preference)
{
    CFastMutexGuard mg(m_Mtx); 
    
    NON_CONST_ITERATE(TDelegates, dg_it, m_Delegates) {
        (*dg_it)->SetPreference(service, preferred_server, preference);
    }
}


void 
CDBServiceMapperCoR::ConfigureFromRegistry(const IRegistry* registry)
{
    for_each(m_Delegates.begin(), m_Delegates.end(), 
             bind2nd(mem_fun(&IDBServiceMapper::Configure), registry));
}


/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2006/01/04 12:29:54  ssikorsk
 * include <algorithm> explicitly.
 *
 * Revision 1.1  2006/01/03 19:44:08  ssikorsk
 * Initial implementation
 *
 * ===========================================================================
 */
