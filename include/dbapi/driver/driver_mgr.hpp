#ifndef DBAPI_DRIVER___IDRIVERMGR__HPP
#define DBAPI_DRIVER___IDRIVERMGR__HPP

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
 * File Description:  Driver 
 *
 */

#include <corelib/ncbimtx.hpp>
#include <dbapi/driver/public.hpp>

BEGIN_NCBI_SCOPE

class C_DriverMgr : public I_DriverMgr
{
public:
    FDBAPI_CreateContext GetDriver(const string& driver_name);

    virtual void RegisterDriver(const string& driver_name,
				FDBAPI_CreateContext driver_ctx_func);

protected:
    bool LoadDriverDll(const string& driver_name);

private:
    typedef void* (*FDllEntryPoint)(void);
    typedef void (*FDriverRegister)(I_DriverMgr& mgr);
#ifndef NCBI_COMPILER_GCC
    typedef map<string, FDBAPI_CreateContext> TDrivers;
#else
    typedef map<string, void*> TDrivers;
#endif

    CFastMutex m_Mutex;

    TDrivers m_Drivers;
};

END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2002/01/17 23:18:58  soussov
 * makes gcc happy
 *
 * Revision 1.1  2002/01/17 22:05:56  soussov
 * adds driver manager
 *
 *
 * ===========================================================================
 */

#endif  /* DBAPI_DRIVER___IDRIVERMGR__HPP */
