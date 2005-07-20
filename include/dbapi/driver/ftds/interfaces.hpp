#ifndef DBAPI_DRIVER_FTDS___INTERFACES__HPP
#define DBAPI_DRIVER_FTDS___INTERFACES__HPP

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
 * File Description:  Driver for TDS server
 *
 */

#define FTDS_IN_USE

#include <dbapi/driver/dblib/interfaces.hpp>

#endif  /* DBAPI_DRIVER_FTDS___INTERFACES__HPP */



/*
 * ===========================================================================
 * $Log$
 * Revision 1.21  2005/07/20 12:33:04  ssikorsk
 * Merged ftds/interfaces.hpp into dblib/interfaces.hpp
 *
 * Revision 1.20  2005/03/01 15:21:52  ssikorsk
 * Database driver manager revamp to use "core" CPluginManager
 *
 * Revision 1.19  2005/02/23 21:33:11  soussov
 * Adds Abort() method to connection
 *
 * Revision 1.18  2004/12/20 16:20:48  ssikorsk
 * Refactoring of dbapi/driver/samples
 *
 * Revision 1.17  2004/12/10 15:26:41  ssikorsk
 * FreeTDS is ported on windows
 *
 * Revision 1.16  2004/10/16 20:48:42  vakatov
 * +  #include <dbapi/driver/ftds/ncbi_ftds_rename_sybdb.h>
 * to allow the renaming (and the use of the renamed) DBLIB symbols in
 * the built-in FreeTDS.
 *
 * Revision 1.15  2003/12/18 19:00:55  soussov
 * makes FTDS_CreateContext return an existing context if reuse_context option is set
 *
 * Revision 1.14  2003/07/17 20:43:33  soussov
 * connections pool improvements
 *
 * Revision 1.13  2003/06/05 15:55:47  soussov
 * adds DumpResults method for LangCmd and RPC, SetResultProcessor method for Connection interface
 *
 * Revision 1.12  2003/04/01 20:28:20  vakatov
 * Temporarily rollback to R1.10 -- until more backward-incompatible
 * changes (in CException) are ready to commit (to avoid breaking the
 * compatibility twice).
 *
 * Revision 1.10  2003/01/06 20:09:01  vakatov
 * cosmetics (identation)
 *
 * Revision 1.9  2002/12/13 21:56:41  vakatov
 * Use the newly defined #NCBI_FTDS value "7"
 *
 * Revision 1.8  2002/12/05 22:39:39  soussov
 * Adapted for TDS8
 *
 * Revision 1.7  2002/05/29 22:03:50  soussov
 * Makes BlobResult read ahead
 *
 * Revision 1.6  2002/03/28 00:45:18  vakatov
 * CTDS_CursorCmd::  use CTDS_CursorResult rather than I_Result (to fix access)
 *
 * Revision 1.5  2002/03/26 15:29:30  soussov
 * new image/text operations added
 *
 * Revision 1.4  2002/01/28 19:57:13  soussov
 * removing the long query timout
 *
 * Revision 1.3  2002/01/14 20:38:01  soussov
 * timeout support for tds added
 *
 * Revision 1.2  2001/11/06 17:58:04  lavr
 * Formatted uniformly as the rest of the library
 *
 * Revision 1.1  2001/10/25 00:39:20  vakatov
 * Initial revision
 *
 * ===========================================================================
 */
