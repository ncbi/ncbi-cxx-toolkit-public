#ifndef DBAPI___DRIVER_MGR__HPP
#define DBAPI___DRIVER_MGR__HPP

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
 * Author:  Michael Kholodov, Denis Vakatov
 *   
 * File Description: Driver Manager definition
 *
 */

#include <corelib/ncbistd.hpp>
#include <corelib/ncbireg.hpp>
#include <corelib/ncbi_safe_static.hpp>
#include <dbapi/driver/interfaces.hpp>
#include <map>

BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//
//  CDriverManager::
//
//  Static class for registering drivers and getting the datasource
//

class CDriverManager : public I_DriverMgr
{
    friend class CSafeStaticPtr<CDriverManager>;

public:

    // Get a single instance of CDriverManager
    static CDriverManager& GetInstance();

    // Create datasource object
    class IDataSource* CreateDs(const string& driver_name,
				map<string, string> *attr = 0);

    class IDataSource* CreateDsFrom(const string& drivers,
				    CNcbiRegistry* reg = 0);

    // Put the driver entry point into the drivers table
    virtual void RegisterDriver(const string& driver_name,
				FDBAPI_CreateContext driver_ctx_func);

protected:

    // Prohibit explicit construction and destruction
    CDriverManager();
    virtual ~CDriverManager();

    // Get the driver entry point from the drivers table
    // If no static driver found, the attempt to load DLL
    // driver is made
    FDBAPI_CreateContext GetDriver(const string& driver_name);

    // Load driver dll on request 
    // NOTE: Currently it will attempt to load libraries
    // with default name convention and path.
    bool LoadDriverDll(const string& driver_name);

    // Put the new data source into the internal list with
    // corresponding driver name, return previous, if already exists
    class IDataSource* RegisterDs(const string& driver_name,
				  class I_DriverContext* ctx);

private:
    // Define the type of the drivers table
#ifdef NCBI_COMPILER_GCC
    typedef map<string, void*> TDrivers;
#else
    typedef map<string, FDBAPI_CreateContext> TDrivers;
#endif

    TDrivers s_drivers;
    map<string, class IDataSource*> m_ds_list;
};

END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2002/01/30 14:51:24  kholodov
 * User DBAPI implementation, first commit
 *
 * ===========================================================================
 */

#endif  /* DBAPI___DBAPI__HPP */
