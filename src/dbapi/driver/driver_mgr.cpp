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
 * Author:  Vladimir Soussov
 *
 * File Description:  Driver manager
 *
 */

#include <dbapi/driver/drivermgr.hpp>
#include <corelib/ncbidll.hpp>

BEGIN_NCBI_SCOPE

void C_DriverMgr::RegisterDriver(const string& driver_name,
				 FDBAPI_CreateContext driver_ctx_func)
{
    m_Drivers[driver_name]= driver_ctx_func;
}

FDBAPI_CreateContext C_DriverMgr::GetDriver(const string& driver_name)
{
    m_Mutex.Lock();
    TDrivers::iterator drv = m_Drivers.find(driver_name);

    if (drv == m_Drivers.end()) {
        if (!LoadDriverDll(driver_name)) {
	    m_Mutex.Unlock();
            return 0;
        }
    }

    m_Mutex.Unlock();
    return m_Drivers[driver_name];
}

static void DriverDllName(string& dll_name, const string& driver_name)
{
    dll_name= "dbapi_driver_";
    dll_name+= driver_name;
}

static void DriverEntryPointName(string& entry_point_name, const string& driver_name)
{
    entry_point_name= "DBAPI_E_";
    entry_point_name+= driver_name;
}


bool C_DriverMgr::LoadDriverDll(const string& driver_name)
{
    string dll_name;
    DriverDllName(dll_name, driver_name);

    try {
	CDll drvDll(dll_name);
	string entry_point_name;
	DriverEntryPointName(entry_point_name, driver_name);

	FDllEntryPoint entry_point;
	if(!drvDll.GetEntryPoint(entry_point_name, &entry_point)) {
	    drvDll.Unload();
	    return false;
	}
	FDriverRegister reg= (FDriverRegister)((*entry_point)());
	(*reg)(*this);
	return true;
    }
    catch (exception&) {
	return false;
    }
}

END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2002/01/17 22:17:25  soussov
 * adds driver manager
 *
 * ===========================================================================
 */
