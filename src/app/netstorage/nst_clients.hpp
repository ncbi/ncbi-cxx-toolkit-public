#ifndef NETSTORAGE_CLIENTS__HPP
#define NETSTORAGE_CLIENTS__HPP

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
 * Authors:  Sergey Satskiy
 *
 * File Description:
 *   NetStorage clients registry supporting facilities
 *
 */

#include <string>

#include <corelib/ncbimtx.hpp>
#include <connect/services/json_over_uttp.hpp>

#include "nst_precise_time.hpp"
#include "nst_metadata_options.hpp"



BEGIN_NCBI_SCOPE

// Shared with the database wrapper code
const Int8      k_UndefinedClientID = -1;



// Note: access to the instances of this class is possible only via the client
// registry and the corresponding container access is always done under a lock.
// So it is safe to do any modifications in the members without any locks here.
class CNSTClient
{
    public:
        // Used for a bit mask to identify what kind of
        // operations the client tried to perform
        enum ENSTClientType {
            eReader        = 1,
            eWriter        = 2,
            eAdministrator = 4
        };

    public:
        CNSTClient();

        CNSTPreciseTime GetRegistrationTime(void) const
        { return m_RegistrationTime; }
        CNSTPreciseTime GetLastAccess(void) const
        { return m_LastAccess; }
        void RegisterSocketWriteError(void)
        { ++m_NumberOfSockErrors; }
        unsigned int GetType(void) const
        { return m_Type; }
        void AppendType(unsigned int  type_to_append)
        { m_Type |= type_to_append; }
        unsigned int  GetPeerAddress(void) const
        { return m_Addr; }
        void SetApplication(const string &  application)
        { m_Application = application; }
        void SetTicket(const string &  ticket)
        { m_Ticket = ticket; }
        void SetService(const string &  service)
        { m_Service = service; }
        void SetProtocolVersion(const string &  protocol_version)
        { m_ProtocolVersion = protocol_version; }
        void SetProtocolVersionProvided(bool val )
        { m_ProtocolVersionProvided = val; }
        void SetMetadataOption(EMetadataOption  val)
        { m_MetadataOption = val; }
        void SetPeerAddress(unsigned int  peer_address)
        { m_Addr = peer_address; }
        void  AddBytesWritten(size_t  count)
        { m_NumberOfBytesWritten += count; }
        void  AddBytesRead(size_t  count)
        { m_NumberOfBytesRead += count; }
        void  AddBytesRelocated(size_t  count)
        { m_NumberOfBytesRelocated += count; }
        void  AddObjectsWritten(size_t  count)
        { m_NumberOfObjectsWritten += count; }
        void  AddObjectsRead(size_t  count)
        { m_NumberOfObjectsRead += count; }
        void  AddObjectsRelocated(size_t  count)
        { m_NumberOfObjectsRelocated += count; }
        void  AddObjectsDeleted(size_t  count)
        { m_NumberOfObjectsDeleted += count; }

        void  Touch(void);
        CJsonNode  Serialize(void) const;

        Int8  GetDBClientID(void) const
        { return m_DBClientID; }
        void  SetDBClientID(Int8  id)
        { m_DBClientID = id; }

    private:
        string          m_Application;    // Absolute exec path
        string          m_Ticket;         // Optional auth ticket
        string          m_Service;        // Optional service
        EMetadataOption m_MetadataOption; // Metadata option after HELLO
        string          m_ProtocolVersion;
        bool            m_ProtocolVersionProvided;

        unsigned int    m_Type;           // bit mask of ENSTClientType
        unsigned int    m_Addr;           // Client peer address

        CNSTPreciseTime m_RegistrationTime;
        CNSTPreciseTime m_LastAccess;     // The last time the client accessed
                                          // netstorage

        size_t          m_NumberOfBytesWritten;
        size_t          m_NumberOfBytesRead;
        size_t          m_NumberOfBytesRelocated;
        size_t          m_NumberOfObjectsWritten;
        size_t          m_NumberOfObjectsRead;
        size_t          m_NumberOfObjectsRelocated;
        size_t          m_NumberOfObjectsDeleted;

        size_t          m_NumberOfSockErrors;

        Int8            m_DBClientID;     // Cached database value

        string  x_TypeAsString(void) const;
};



class CNSTClientRegistry
{
    public:
        CNSTClientRegistry();

        size_t  Size(void) const;
        void  Touch(const string &  client,
                    const string &  applications,
                    const string &  ticket,
                    const string &  service,
                    const string &  protocol_version,
                    EMetadataOption metadataOption,
                    unsigned int    peer_address);
        void  Touch(const string &  client);
        void  RegisterSocketWriteError(const string &  client);
        void  AppendType(const string &  client,
                         unsigned int    type_to_append);
        void  AddBytesWritten(const string &  client, size_t  count);
        void  AddBytesRead(const string &  client, size_t  count);
        void  AddBytesRelocated(const string &  client, size_t  count);
        void  AddObjectsWritten(const string &  client, size_t  count);
        void  AddObjectsRead(const string &  client, size_t  count);
        void  AddObjectsRelocated(const string &  client, size_t  count);
        void  AddObjectsDeleted(const string &  client, size_t  count);

        CJsonNode Serialize(void) const;

        Int8  GetDBClientID(const string &  client) const;
        void  SetDBClientID(const string &  client, Int8  id);

    private:
        map< string, CNSTClient >   m_Clients;  // All the server clients
                                                // netstorage knows about
        mutable CMutex              m_Lock;     // Lock for the map
};



END_NCBI_SCOPE

#endif /* NETSTORAGE_CLIENTS__HPP */

