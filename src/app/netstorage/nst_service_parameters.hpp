#ifndef NETSTORAGE_SERVICE_PARAMETERS__HPP
#define NETSTORAGE_SERVICE_PARAMETERS__HPP

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
 *   NetStorage services and their properties
 *
 */

#include <corelib/ncbimtx.hpp>
#include <corelib/ncbistr.hpp>
#include <connect/services/json_over_uttp.hpp>

#include "nst_database.hpp"


BEGIN_NCBI_SCOPE


// The latest requirement is to make the service name case insensitive.
// So this constant can use any letters - the test statements are case
// insensitive throughout the code
const string    k_LBSMDNSTTestService = "LBSMDNSTTestService";



// Note: access to the instances of this class is possible only via the service
// registry and the corresponding container access is always done under a lock.
// So it is safe to do any modifications in the members without any locks here.
class CNSTServiceProperties
{
    public:
        CNSTServiceProperties();
        CNSTServiceProperties(const TNSTDBValue<CTimeSpan> &  ttl,
                              const CTimeSpan &  prolong_on_read,
                              const CTimeSpan &  prolong_on_write) :
            m_TTL(ttl), m_ProlongOnRead(prolong_on_read),
            m_ProlongOnWrite(prolong_on_write)
        {}

        TNSTDBValue<CTimeSpan>  GetTTL(void) const
        { return m_TTL; }
        string  GetTTLAsString(void) const;
        CTimeSpan  GetProlongOnRead(void) const
        { return m_ProlongOnRead; }
        CTimeSpan  GetProlongOnWrite(void) const
        { return m_ProlongOnWrite; }

        void SetTTL(const TNSTDBValue<CTimeSpan> & new_val)
        { m_TTL = new_val; }
        void SetProlongOnRead(const CTimeSpan &  new_val)
        { m_ProlongOnRead = new_val; }
        void SetProlongOnWrite(const CTimeSpan &  new_val)
        { m_ProlongOnWrite = new_val; }

        CJsonNode  Serialize(void) const;

        bool operator==(const CNSTServiceProperties &  other) const
        { return (m_TTL == other.m_TTL) &&
                 (m_ProlongOnRead == other.m_ProlongOnRead) &&
                 (m_ProlongOnWrite == other.m_ProlongOnWrite); }
        bool operator!=(const CNSTServiceProperties &  other) const
        { return !this->operator==(other); }

    private:
        TNSTDBValue<CTimeSpan>      m_TTL;
        CTimeSpan                   m_ProlongOnRead;
        CTimeSpan                   m_ProlongOnWrite;
};



class CNSTServiceRegistry
{
    public:
        CNSTServiceRegistry();

        size_t  Size(void) const;
        CJsonNode  ReadConfiguration(const IRegistry &  reg);
        CJsonNode  Serialize(void) const;
        bool  IsKnown(const string &  service) const;
        bool  GetTTL(const string &            service,
                     TNSTDBValue<CTimeSpan> &  ttl) const;
        bool  GetProlongOnRead(const string &  service,
                               CTimeSpan &     prolong_on_read) const;
        bool  GetProlongOnWrite(const string &  service,
                                CTimeSpan &     prolong_on_write) const;
        bool  GetServiceProperties(const string &  service,
                                   CNSTServiceProperties &  props) const;
        CNSTServiceProperties  GetDefaultProperties(void) const;

    private:
        CNSTServiceProperties
            x_ReadServiceProperties(const IRegistry &  reg,
                                    const string &  section,
                                    const CNSTServiceProperties &  defaults);
        CTimeSpan  x_ReadProlongProperty(const IRegistry &  reg,
                                         const string &  section,
                                         const string &  entry);

    private:
        CNSTServiceProperties           m_LBSMDTestServiceProperties;
        CNSTServiceProperties           m_DefaultProperties;

        // The latest requirement is to make the service case insensitive
        typedef map< string,
                     CNSTServiceProperties,
                     PNocase >          TServiceProperties;
        TServiceProperties              m_Services; // All the services
                                                    // netstorage knows about
        mutable CMutex                  m_Lock;     // Lock for the map
};


END_NCBI_SCOPE

#endif /* NETSTORAGE_SERVICE_PARAMETERS__HPP */

