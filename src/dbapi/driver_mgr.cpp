/* $Id$
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
* File Name:  $Id$
*
* Author:  Michael Kholodov, Denis Vakatov
*
* File Description:  Driver Manager implementation
*
*/

#include <ncbi_pch.hpp>

#include <corelib/ncbistr.hpp>
#include <corelib/ncbi_safe_static.hpp>

#include <dbapi/driver_mgr.hpp>
#include <dbapi/error_codes.hpp>

#include "ds_impl.hpp"
#include "dbexception.hpp"


#define NCBI_USE_ERRCODE_X   Dbapi_ObjImpls


BEGIN_NCBI_SCOPE


///////////////////////////////////////////////////////////////////////////////

CDriverManager& CDriverManager::GetInstance()
{
    static CSafeStaticPtr<CDriverManager> instance
        (NULL, CSafeStaticLifeSpan::eLifeSpan_Long);

    return instance.Get();
}


void CDriverManager::RemoveInstance()
{
}


CDriverManager::CDriverManager()
{
}


CDriverManager::~CDriverManager()
{
    try {
        CMutexGuard mg(m_Mutex);

        ITERATE(TDsContainer, it, m_ds_list) {
            IDataSource* ds = it->second;

            if (ds) {
                // We won't delete IDataSource unless all connections
                // are closed, because deleting of IDataSource will also
                // delete all connections.
                // This will cause a memory leak but it also will prevent from
                // accessing an already freed memory or even application
                // crash..
                if (ds->GetDriverContext()->NofConnections() == 0) {
                    delete ds;
                }
            }
        }

        m_ds_list.clear();
    }
    NCBI_CATCH_ALL_X( 4, kEmptyStr )
}


IDataSource* CDriverManager::CreateDs(const string&        driver_name,
                                      const map<string, string>* attr)
{
    CMutexGuard mg(m_Mutex);

    TDsContainer::iterator i_ds = m_ds_list.find(driver_name);
    if (i_ds != m_ds_list.end()) {
        return (*i_ds).second;
    }

    I_DriverContext* ctx = GetDriverContextFromMap( driver_name, attr );

    CHECK_NCBI_DBAPI(
        !ctx,
        "CDriverManager::CreateDs() -- Failed to get context for driver: " + driver_name
        );

    return RegisterDs(driver_name, ctx);
}

IDataSource* CDriverManager::CreateDsFrom(const string& drivers,
                                          const IRegistry* reg)
{
    CMutexGuard mg(m_Mutex);

    list<string> names;
    NStr::Split(drivers, ":", names);

    list<string>::iterator i_name = names.begin();
    for( ; i_name != names.end(); ++i_name ) {
        I_DriverContext* ctx = NULL;
        if( reg != NULL ) {
            // Get parameters from registry, if any
            map<string, string> attr;
            list<string> entries;
            reg->EnumerateEntries(*i_name, &entries);
            list<string>::iterator i_param = entries.begin();
            for( ; i_param != entries.end(); ++i_param ) {
                attr[*i_param] = reg->Get(*i_name, *i_param);
            }
            ctx = GetDriverContextFromMap( *i_name, &attr );
        } else {
            ctx = GetDriverContextFromMap( *i_name, NULL );
        }

        if( ctx != 0 ) {
            return RegisterDs( *i_name, ctx );
        }
    }
    return 0;
}

IDataSource* CDriverManager::RegisterDs(const string& driver_name,
                                        I_DriverContext* ctx)
{
    CMutexGuard mg(m_Mutex);

    IDataSource* ds = new CDataSource(ctx);
    m_ds_list.insert(TDsContainer::value_type(driver_name, ds));
    return ds;
}

void CDriverManager::DestroyDs(const string& driver_name)
{
    CMutexGuard mg(m_Mutex);

    TDsContainer::iterator it;
    while ((it = m_ds_list.find(driver_name)) != m_ds_list.end()) {
        delete it->second;
        m_ds_list.erase(it);
    }
}

void CDriverManager::DestroyDs(const IDataSource* ds)
{
    CMutexGuard mg(m_Mutex);

    TDsContainer::iterator iter = m_ds_list.begin();
    TDsContainer::iterator eiter = m_ds_list.end();

    for (; iter != eiter; ++iter) {
        if (iter->second == ds) {
            delete iter->second;
            m_ds_list.erase(iter);
            break;
        }
    }
}


END_NCBI_SCOPE
