#ifndef CONNECT_SERVICES__NETSCHEDULE_STORAGE_HPP
#define CONNECT_SERVICES__NETSCHEDULE_STORAGE_HPP

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
 * Authors:  Maxim Didneko, Anatoliy Kuznetsov
 *
 * File Description:  
 *      DEPRICATED
 *
 */

#include <corelib/blob_storage.hpp>
#include <connect/services/ns_client_factory.hpp>

BEGIN_NCBI_SCOPE

typedef IBlobStorage INetScheduleStorage;
typedef IBlobStorageFactory INetScheduleStorageFactory;
typedef CBlobStorageException CNetScheduleStorageException;

END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.11  2005/12/20 17:26:22  didenko
 * Reorganized netschedule storage facility.
 * renamed INetScheduleStorage to IBlobStorage and moved it to corelib
 * renamed INetScheduleStorageFactory to IBlobStorageFactory and moved it to corelib
 * renamed CNetScheduleNSStorage_NetCache to CBlobStorage_NetCache and moved it
 * to separate files
 * Moved CNetScheduleClientFactory to separate files
 *
 * Revision 1.10  2005/10/26 16:37:44  didenko
 * Added for non-blocking read for netschedule storage
 *
 * Revision 1.9  2005/05/10 14:11:22  didenko
 * Added blob caching
 *
 * Revision 1.8  2005/05/05 15:18:51  didenko
 * Added debugging facility to worker nodes
 *
 * Revision 1.7  2005/04/20 19:23:47  didenko
 * Added GetBlobAsString, GreateEmptyBlob methods
 * Remave RemoveData to DeleteBlob
 *
 * Revision 1.6  2005/03/29 14:10:16  didenko
 * + removing a date from the storage
 *
 * Revision 1.5  2005/03/28 16:49:00  didenko
 * Added virtual desturctors to all new interfaces to prevent memory leaks
 *
 * Revision 1.4  2005/03/25 16:24:58  didenko
 * Classes restructure
 *
 * Revision 1.3  2005/03/23 13:10:32  kuznets
 * documented and doxygenized
 *
 * Revision 1.2  2005/03/22 21:42:50  didenko
 * Got rid of warnning on Sun WorkShop
 *
 * Revision 1.1  2005/03/22 20:17:55  didenko
 * Initial version
 *
 * ===========================================================================
 */


#endif // CONNECT_SERVICES__NETSCHEDULE_STORAGE_HPP
