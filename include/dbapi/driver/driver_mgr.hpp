#ifndef DBAPI_DRIVER___DRIVER_MGR__HPP
#define DBAPI_DRIVER___DRIVER_MGR__HPP

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

#include <corelib/ncbimtx.hpp>
#include <dbapi/driver/public.hpp>

BEGIN_NCBI_SCOPE


class C_DriverMgr : public I_DriverMgr
{
public:
    C_DriverMgr(unsigned int nof_drivers = 16);
    virtual ~C_DriverMgr();

    FDBAPI_CreateContext GetDriver(const string& driver_name, 
                                   string*       err_msg = 0);

    virtual void RegisterDriver(const string&        driver_name,
                                FDBAPI_CreateContext driver_ctx_func);

protected:
    bool LoadDriverDll(const string& driver_name, string* err_msg);

private:
    typedef void            (*FDriverRegister) (I_DriverMgr& mgr);
    typedef FDriverRegister (*FDllEntryPoint)  (void);

    struct SDrivers {
        string               drv_name;
        FDBAPI_CreateContext drv_func;
    };

    unsigned int m_NofDrvs;
    unsigned int m_NofRoom;
    SDrivers*    m_Drivers;
    CFastMutex   m_Mutex1;
    CFastMutex   m_Mutex2;
};


END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
 * Revision 1.7  2002/04/09 22:18:13  vakatov
 * Moved code from the header
 *
 * Revision 1.6  2002/04/09 20:05:57  vakatov
 * Identation
 *
 * Revision 1.5  2002/04/04 23:48:44  soussov
 * return of error message from dlopen added
 *
 * Revision 1.4  2002/01/23 21:14:06  soussov
 * replaces map with array
 *
 * Revision 1.3  2002/01/20 07:26:57  vakatov
 * Fool-proofed to compile func_ptr/void_ptr type casts on all compilers.
 * Added virtual destructor.
 * Temporarily get rid of the MT-safety features -- they need to be
 * implemented here more carefully (in the nearest future).
 *
 * Revision 1.2  2002/01/17 23:18:58  soussov
 * makes gcc happy
 *
 * Revision 1.1  2002/01/17 22:05:56  soussov
 * adds driver manager
 *
 * ===========================================================================
 */

#endif  /* DBAPI_DRIVER___DRIVER_MGR__HPP */
