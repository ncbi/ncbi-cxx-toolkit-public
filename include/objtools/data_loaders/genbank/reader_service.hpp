#ifndef READER_SERVICE__HPP_INCLUDED
#define READER_SERVICE__HPP_INCLUDED

/*  $Id$
* ===========================================================================
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
* ===========================================================================
*
*  Author:  Eugene Vasilchenko
*
*  File Description: Common class to control ID1/ID2 service connections
*
*/

#include <corelib/ncbistd.hpp>
#include <connect/ncbi_server_info.h>
#include <connect/ncbi_conn_stream.hpp>
#include <vector>

BEGIN_NCBI_SCOPE

class CConn_IOStream;

BEGIN_SCOPE(objects)

class CReader;

class NCBI_XREADER_EXPORT CReaderServiceConnector : private CConnIniter
{
public:
    CReaderServiceConnector(void);
    CReaderServiceConnector(const string& service_name,
                            int open_timeout,
                            int timeout);
    ~CReaderServiceConnector(void);

    struct SConnInfo {
        SConnInfo(void) : m_ServerInfo(0) {
        }

        void MarkAsGood(void) {
            m_ServerInfo = 0;
        }

        AutoPtr<CConn_IOStream> m_Stream;
        const SSERV_Info* m_ServerInfo;
    };

    const string& GetServiceName(void) const {
        return m_ServiceName;
    }
    int GetTimeout(void) const {
        return m_Timeout;
    }
    int GetOpenTimeout(void) const {
        return m_OpenTimeout;
    }
    void SetServiceName(const string& service_name);
    void SetTimeout(int timeout);
    void SetOpenTimeout(int open_timeout);

    SConnInfo Connect(void);

    string GetConnDescription(CConn_IOStream& stream) const;

    void RememberIfBad(SConnInfo& conn_info);

protected:
    typedef vector< AutoPtr<SSERV_Info, CDeleter<SSERV_Info> > > TSkipServers;

    string m_ServiceName;
    int    m_OpenTimeout, m_Timeout;

    TSkipServers   m_SkipServers;

private:
    // forbid copying
    CReaderServiceConnector(const CReaderServiceConnector&);
    void operator=(const CReaderServiceConnector&);
};

END_SCOPE(objects)
END_NCBI_SCOPE

#endif // READER_SERVICE__HPP_INCLUDED
