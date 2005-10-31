#ifndef DBAPI_DRIVER_MSDBLIB___INTERFACES__HPP
#define DBAPI_DRIVER_MSDBLIB___INTERFACES__HPP

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
 * File Description:  Driver for Microsoft DBLib server
 *
 */

#define MS_DBLIB_IN_USE 

#include <dbapi/driver/dblib/interfaces.hpp>

#endif  /* DBAPI_DRIVER_DBLIB___INTERFACES__HPP */



/*
 * ===========================================================================
 * $Log$
 * Revision 1.14  2005/10/31 12:14:38  ssikorsk
 * Merged msdblib/interfaces.hpp into dblib/interfaces.hpp
 *
 * Revision 1.13  2005/10/20 12:57:33  ssikorsk
 * - CMSDBLibContext::m_AppName
 * - CMSDBLibContext::m_HostName
 *
 * Revision 1.12  2005/09/14 14:32:39  ssikorsk
 * Add ConnectedToMSSQLServer and GetTDSVersion methods to
 * the CMSDBLibContext class
 *
 * Revision 1.11  2005/09/07 11:00:07  ssikorsk
 * Added GetColumnNum method
 *
 * Revision 1.10  2005/07/07 15:43:20  ssikorsk
 * Integrated interfaces with FreeTDS v0.63
 *
 * Revision 1.9  2005/03/01 15:21:52  ssikorsk
 * Database driver manager revamp to use "core" CPluginManager
 *
 * Revision 1.8  2005/02/23 21:33:00  soussov
 * Adds Abort() method to connection
 *
 * Revision 1.7  2004/07/06 14:26:48  gorelenk
 * Changed typedef for LPCBYTE to be consistent with Platform SDK headers
 *
 * Revision 1.6  2004/05/18 19:22:08  gorelenk
 * Conditionaly added typedef for LPCBYTE missed in MSVC7 headers .
 *
 * Revision 1.5  2003/07/17 20:42:47  soussov
 * connections pool improvements
 *
 * Revision 1.4  2003/06/06 18:43:16  soussov
 * Removes SetPacketSize()
 *
 * Revision 1.3  2003/06/05 15:56:19  soussov
 * adds DumpResults method for LangCmd and RPC, SetResultProcessor method for Connection interface
 *
 * Revision 1.2  2003/02/13 15:43:18  ivanov
 * Added export specifier NCBI_DBAPIDRIVER_MSDBLIB_EXPORT for class definitions
 *
 * Revision 1.1  2002/07/02 16:02:25  soussov
 * initial commit
 *
 * ===========================================================================
 */
