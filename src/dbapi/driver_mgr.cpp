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
*
* $Log$
* Revision 1.3  2002/04/02 18:16:03  ucko
* More fixes to CDriverManager::LoadDriverDll.
*
* Revision 1.2  2002/04/01 22:31:40  kholodov
* Fixed DLL entry point names
*
* Revision 1.1  2002/01/30 14:51:21  kholodov
* User DBAPI implementation, first commit
*
*
*
*
*/

#include <dbapi/driver_mgr.hpp>
#include "ds_impl.hpp"

#include <corelib/ncbistr.hpp>
#include <corelib/ncbidll.hpp>

BEGIN_NCBI_SCOPE

static CSafeStaticPtr<CDriverManager> singleDM;

CDriverManager& CDriverManager::GetInstance()
{
  return singleDM.Get();
}

CDriverManager::CDriverManager()
{
}


CDriverManager::~CDriverManager()
{
  map<string, IDataSource*>::iterator i_ds_list = m_ds_list.begin();

  for( ; i_ds_list != m_ds_list.end(); ++i_ds_list ) {
    delete (*i_ds_list).second;
  }
  m_ds_list.clear();
}

void CDriverManager::RegisterDriver(const string& driver_name,
				    FDBAPI_CreateContext driver_ctx_func)
{
  if( s_drivers.find(driver_name) == s_drivers.end() ) {
#ifdef NCBI_COMPILER_GCC
    s_drivers[driver_name] = (void*) driver_ctx_func;
#else
    s_drivers[driver_name] = driver_ctx_func;
#endif
  }
}


FDBAPI_CreateContext
CDriverManager::GetDriver(const string& driver_name)
{
  TDrivers::iterator i = s_drivers.find(driver_name);

  if (i == s_drivers.end()) {

    //#if defined(CAN_LOAD_DLLS)
    if ( !LoadDriverDll(driver_name) ) {
      return 0;
    }
    //#else
    //    return 0;
    //#endif
  }


#ifdef NCBI_COMPILER_GCC
  return (FDBAPI_CreateContext) s_drivers[driver_name];
#else
  return s_drivers[driver_name];
#endif
}


bool CDriverManager::LoadDriverDll(const string& driver_name)
{
  typedef void (*FRegistrationFunction)(I_DriverMgr& mgr);
  typedef FRegistrationFunction (*FEntryPoint)(void);

  CDll dll("dbapi_driver_" + driver_name);

  FEntryPoint func = 0;
  if( driver_name == "ctlib" ) {
    dll.GetEntryPoint("DBAPI_E_ctlib", &func);
  }
  else if( driver_name == "dblib" ) {
    dll.GetEntryPoint("DBAPI_E_dblib", &func);
  }
  else if( driver_name == "ftds" ) {
    dll.GetEntryPoint("DBAPI_E_ftds", &func);
  }

  if( func != 0 ) {
    func()(*this);
    return true;
  }

  return false;
}


IDataSource* CDriverManager::CreateDs(const string& driver_name,
				      map<string, string> *attr)
{
  FDBAPI_CreateContext ctx_func = GetDriver(driver_name);
  if( ctx_func != 0 ) {
    I_DriverContext *ctx = ctx_func(attr);
    if( ctx != 0 ) {
      return RegisterDs(driver_name, ctx);
    }
  }
  return 0;
}

IDataSource* CDriverManager::CreateDsFrom(const string& drivers,
					  CNcbiRegistry* reg)
{
  list<string> names;
  NStr::Split(drivers, ":", names);

  list<string>::iterator i_name = names.begin();
  for( ; i_name != names.end(); ++i_name ) {
    FDBAPI_CreateContext ctx_func = GetDriver(*i_name);
    if( ctx_func != 0 ) {
      I_DriverContext *ctx = 0;
      if( reg != 0 ) {
	// Get parameters from registry, if any
	map<string, string> attr;
	list<string> entries;
	reg->EnumerateEntries(*i_name, &entries);
	list<string>::iterator i_param = entries.begin();
	for( ; i_param != entries.end(); ++i_param ) {
	  attr[*i_param] = reg->Get(*i_name, *i_param);
	}
	ctx = ctx_func(&attr);
      }
      else {
	ctx = ctx_func(0);
      }

      if( ctx != 0 ) { 
	return RegisterDs(*i_name, ctx);
      }
    }
  }
  return 0;
}

IDataSource* CDriverManager::RegisterDs(const string& driver_name,
					I_DriverContext* ctx)
{
  map<string, IDataSource*>::iterator i_ds = m_ds_list.find(driver_name);
  if( i_ds == m_ds_list.end() ) {
    IDataSource *ds = new CDataSource(ctx, 0);
    m_ds_list[driver_name] = ds;
    return ds;
  }
  else
    return (*i_ds).second;
}

END_NCBI_SCOPE
