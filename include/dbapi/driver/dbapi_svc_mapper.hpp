#ifndef DBAPI_SVC_MAPPER_HPP
#define DBAPI_SVC_MAPPER_HPP

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
* Author: Sergey Sikorskiy
*
* File Description:
*
*============================================================================
*/

#include <corelib/impl/ncbi_dbsvcmapper.hpp>
#include <corelib/ncbimtx.hpp>
#include <set>

BEGIN_NCBI_SCOPE


///////////////////////////////////////////////////////////////////////////////
/// CDBDefaultServiceMapper
///

class NCBI_DBAPIDRIVER_EXPORT CDBDefaultServiceMapper : public IDBServiceMapper
{
public:
    CDBDefaultServiceMapper(void);
    virtual ~CDBDefaultServiceMapper(void);
    
    virtual void    Configure    (const IRegistry* registry = NULL);
    virtual TSvrRef GetServer    (const string&    service);
    virtual void    Exclude      (const string&    service,
                                  const TSvrRef&   server);
    virtual void    SetPreference(const string&    service,
                                  const TSvrRef&   preferred_server,
                                  double           preference = 100);
    
private:
    typedef set<string> TSrvSet;
    
    CFastMutex  m_Mtx;
    TSrvSet     m_SrvSet;
};

///////////////////////////////////////////////////////////////////////////////
/// CDBServiceMapperCoR
///
/// IDBServiceMapper adaptor which implements the chain of responsibility
/// pattern
///

class NCBI_DBAPIDRIVER_EXPORT CDBServiceMapperCoR : public IDBServiceMapper
{
public:
    CDBServiceMapperCoR(void);
    virtual ~CDBServiceMapperCoR(void);
    
    virtual void    Configure    (const IRegistry* registry = NULL);
    virtual TSvrRef GetServer    (const string&    service);
    virtual void    Exclude      (const string&    service,
                                  const TSvrRef&   server);
    virtual void    SetPreference(const string&    service,
                                  const TSvrRef&   preferred_server,
                                  double           preference = 100);
    
    void push(IDBServiceMapper* mapper)
    {
        if (mapper) {
            CFastMutexGuard mg(m_Mtx); 

            m_Delegates.push_back(CRef<IDBServiceMapper>(mapper));
        }
    }
    void pop(void)
    {
        CFastMutexGuard mg(m_Mtx); 

        m_Delegates.pop_back();
    }
    CRef<IDBServiceMapper> top(void) const
    {
        CFastMutexGuard mg(m_Mtx); 

        return m_Delegates.back();
    }
    bool empty(void) const
    {
        CFastMutexGuard mg(m_Mtx); 

        return m_Delegates.empty();
    }
    
protected:
    CDBServiceMapperCoR(const string& name);
    
    void ConfigureFromRegistry(const IRegistry* registry = NULL);
    
    typedef vector<CRef<IDBServiceMapper> > TDelegates;
    
    mutable CFastMutex m_Mtx;
    TDelegates         m_Delegates;
};


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2006/01/03 19:36:14  ssikorsk
 * Added CDBDefaultServiceMapper and CDBServiceMapperCoR which
 * implement the IDBServiceMapper interface.
 *
 * ===========================================================================
 */

#endif // DBAPI_SVC_MAPPER_HPP
