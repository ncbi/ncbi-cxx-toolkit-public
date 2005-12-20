#ifndef CONNECT_SERVICES__GRID_DEFAULT_FACTORY_HPP
#define CONNECT_SERVICES__GRID_DEFAULT_FACTORY_HPP

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
 * Authors:  Maxim Didenko
 *         DEPRICATED
 */

#include <connect/services/blob_storage_netcache.hpp>
#include <connect/services/ns_client_factory.hpp>

BEGIN_NCBI_SCOPE

typedef CBlobStorageFactory_NetCache CNetScheduleStorageFactory_NetCache;

END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.10  2005/12/20 17:26:22  didenko
 * Reorganized netschedule storage facility.
 * renamed INetScheduleStorage to IBlobStorage and moved it to corelib
 * renamed INetScheduleStorageFactory to IBlobStorageFactory and moved it to corelib
 * renamed CNetScheduleNSStorage_NetCache to CBlobStorage_NetCache and moved it
 * to separate files
 * Moved CNetScheduleClientFactory to separate files
 *
 * Revision 1.9  2005/12/12 15:13:16  didenko
 * Now CNetScheduleStorageFactory_NetCache class reads all init
 * parameters from the registry
 *
 * Revision 1.8  2005/08/15 19:08:43  didenko
 * Changed NetScheduler Storage parameters
 *
 * Revision 1.7  2005/05/12 14:01:04  didenko
 * Made input/output caching turned off by default
 *
 * Revision 1.6  2005/05/11 13:29:14  didenko
 * Fixed a linker error on Windows in DLL configuration
 *
 * Revision 1.5  2005/05/10 15:15:14  didenko
 * Added clean up procedure
 *
 * Revision 1.4  2005/05/10 14:11:22  didenko
 * Added blob caching
 *
 * Revision 1.3  2005/03/30 21:26:01  didenko
 * Added missing header files
 *
 * Revision 1.2  2005/03/28 14:38:04  didenko
 * Cosmetics
 *
 * Revision 1.1  2005/03/25 16:23:43  didenko
 * Initail version
 *
 * ===========================================================================
 */


#endif // CONNECT_SERVICES__GRID_DEFAULT_FACTORY_HPP
