#ifndef DBAPI_DRIVER___DRIVERS__HPP
#define DBAPI_DRIVER___DRIVERS__HPP

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
 * File Description:  Drivers' registration
 *
 */


/** @addtogroup DbDriverReg
 *
 * @{
 */


BEGIN_NCBI_SCOPE

class I_DriverMgr;

extern void DBAPI_RegisterDriver_CTLIB   (I_DriverMgr& mgr);
extern void DBAPI_RegisterDriver_DBLIB   (I_DriverMgr& mgr);
extern void DBAPI_RegisterDriver_FTDS    (I_DriverMgr& mgr);
extern void DBAPI_RegisterDriver_ODBC    (I_DriverMgr& mgr);
extern void DBAPI_RegisterDriver_MSDBLIB (I_DriverMgr& mgr);
extern void DBAPI_RegisterDriver_MYSQL   (I_DriverMgr& mgr);

extern void DBAPI_RegisterDriver_CTLIB   (void);
extern void DBAPI_RegisterDriver_DBLIB   (void);
extern void DBAPI_RegisterDriver_FTDS    (void);
extern void DBAPI_RegisterDriver_ODBC    (void);
extern void DBAPI_RegisterDriver_MSDBLIB (void);
extern void DBAPI_RegisterDriver_MYSQL   (void);

END_NCBI_SCOPE


/* @} */


/*
 * ===========================================================================
 * $Log$
 * Revision 1.7  2005/03/01 15:21:52  ssikorsk
 * Database driver manager revamp to use "core" CPluginManager
 *
 * Revision 1.6  2004/03/17 19:24:20  gorelenk
 * Added "export" for DBAPI_RegisterDriver_* function declaration.
 *
 * Revision 1.5  2003/04/11 17:46:03  siyan
 * Added doxygen support
 *
 * Revision 1.4  2003/02/19 03:38:42  vakatov
 * + DBAPI_RegisterDriver_MYSQL
 *
 * Revision 1.3  2002/07/09 17:00:21  soussov
 * separates the msdblib
 *
 * Revision 1.2  2002/07/02 20:52:31  soussov
 * adds RegisterDriver for ODBC
 *
 * Revision 1.1  2002/01/17 22:05:56  soussov
 * adds driver manager
 *
 * ===========================================================================
 */

#endif  /* DBAPI_DRIVER___DRIVERS__HPP */
