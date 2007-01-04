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

#include "ds_impl.hpp"
#include "dbexception.hpp"


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
        typedef map<string, IDataSource*> TContainer;

        CMutexGuard mg(m_Mutex);

        ITERATE(TContainer, it, m_ds_list) {
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
    NCBI_CATCH_ALL( kEmptyStr )
}


IDataSource* CDriverManager::CreateDs(const string&        driver_name,
                                      const map<string, string>* attr)
{
    CMutexGuard mg(m_Mutex);

    map<string, IDataSource*>::iterator i_ds = m_ds_list.find(driver_name);
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

    IDataSource *ds = new CDataSource(ctx);
    m_ds_list[driver_name] = ds;
    return ds;
}

void CDriverManager::DestroyDs(const string& driver_name)
{
    CMutexGuard mg(m_Mutex);

    map<string, IDataSource*>::iterator i_ds = m_ds_list.find(driver_name);
    if (i_ds != m_ds_list.end()) {
        delete i_ds->second;
        m_ds_list.erase(i_ds);
    }
}


END_NCBI_SCOPE

/*
*
* $Log$
* Revision 1.24  2007/01/04 22:24:04  ssikorsk
* Revamp code to use m_Mutex.
*
* Revision 1.23  2006/05/15 16:53:17  ssikorsk
* Increased lifetime of the CDriverManager singleton.
*
* Revision 1.22  2006/04/06 19:07:04  ssikorsk
* Do not delete IDataSource from CDriverManager::~CDriverManager
* if this datasource still has open connections
*
* Revision 1.21  2005/12/28 13:10:29  ssikorsk
* Restore CSafeStaticPtr-based singleton
*
* Revision 1.20  2005/11/02 15:02:25  ssikorsk
* Catch all exceptions in destructors.
*
* Revision 1.19  2005/04/08 16:51:28  ssikorsk
* Restored a previous CDriverManager::GetInstance logic because of ctlib issues
*
* Revision 1.18  2005/04/07 14:43:43  ssikorsk
* CDriverManager::GetInstance uses CSafeStaticPtr now
*
* Revision 1.17  2005/04/04 13:03:56  ssikorsk
* Revamp of DBAPI exception class CDB_Exception
*
* Revision 1.16  2005/03/08 17:12:58  ssikorsk
* Allow to add a driver search path for the driver manager
*
* Revision 1.15  2005/03/01 15:22:55  ssikorsk
* Database driver manager revamp to use "core" CPluginManager
*
* Revision 1.14  2004/12/20 16:45:34  ucko
* Accept any IRegistry rather than specifically requiring a CNcbiRegistry.
* Move CVS log to end.
*
* Revision 1.13  2004/05/17 21:10:28  gorelenk
* Added include of PCH ncbi_pch.hpp
*
* Revision 1.12  2004/04/22 11:29:53  ivanov
* Use pointer to driver manager instance instead auto_ptr
*
* Revision 1.11  2003/12/17 14:56:33  ivanov
* Changed type of the Driver Manager instance to auto_ptr.
* Added RemoveInstance() method.
*
* Revision 1.10  2003/02/10 17:30:05  kholodov
* Fixed: forgot to add actual deletion code to DestroyDs()
*
* Revision 1.9  2003/02/10 17:19:27  kholodov
* Added: DestroyDs() method
*
* Revision 1.8  2002/09/23 18:35:24  kholodov
* Added: GetErrorInfo() and GetErrorAsEx() methods.
*
* Revision 1.7  2002/09/04 22:18:57  vakatov
* CDriverManager::CreateDs() -- Get rid of comp.warning, improve diagnostics
*
* Revision 1.6  2002/07/09 17:12:29  kholodov
* Fixed duplicate context creation
*
* Revision 1.5  2002/07/03 15:03:05  kholodov
* Added: throw exception when driver cannot be loaded
*
* Revision 1.4  2002/04/29 19:13:13  kholodov
* Modified: using C_DriverMgr as parent class of CDriverManager
*
* Revision 1.3  2002/04/02 18:16:03  ucko
* More fixes to CDriverManager::LoadDriverDll.
*
* Revision 1.2  2002/04/01 22:31:40  kholodov
* Fixed DLL entry point names
*
* Revision 1.1  2002/01/30 14:51:21  kholodov
* User DBAPI implementation, first commit
*
*/
