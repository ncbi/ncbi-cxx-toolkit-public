#ifndef READER_PUBSEQ2__HPP_INCLUDED
#define READER_PUBSEQ2__HPP_INCLUDED

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
*  Author:  Anton Butanaev, Eugene Vasilchenko
*
*  File Description: Data reader from Pubseq_OS
*
*/

#include <objtools/data_loaders/genbank/impl/reader_id2_base.hpp>

BEGIN_NCBI_SCOPE

class CDB_Connection;
class CDB_Result;
class CDB_RPCCmd;
class I_DriverContext;
class I_BaseCmd;

BEGIN_SCOPE(objects)

class CId2ReaderBase;

class NCBI_XREADER_PUBSEQOS2_EXPORT CPubseq2Reader : public CId2ReaderBase
{
public:
    CPubseq2Reader(int max_connections = 0,
                   const string& server = kEmptyStr,
                   const string& user = kEmptyStr,
                   const string& pswd = kEmptyStr,
                   const string& dbapi_driver = kEmptyStr);
    CPubseq2Reader(const TPluginManagerParamTree* params,
                   const string& driver_name);

    ~CPubseq2Reader();

    int GetMaximumConnectionsLimit(void) const;

    void x_InitConnection(CDB_Connection& db_conn, TConn conn);

protected:
    virtual void x_AddConnectionSlot(TConn conn);
    virtual void x_RemoveConnectionSlot(TConn conn);
    virtual void x_DisconnectAtSlot(TConn conn, bool failed);
    virtual void x_ConnectAtSlot(TConn conn);
    virtual string x_ConnDescription(TConn conn) const;

    virtual void x_SendPacket(TConn conn, const CID2_Request_Packet& packet);
    virtual void x_ReceiveReply(TConn conn, CID2_Reply& reply);
    virtual void x_EndOfPacket(TConn conn);

    CDB_Connection& x_GetConnection(TConn conn);
    AutoPtr<CObjectIStream> x_SendPacket(CDB_Connection& db_conn,
                                         TConn conn,
                                         const CID2_Request_Packet& packet);

    CObjectIStream& x_GetCurrentResult(TConn conn);
    void x_SetCurrentResult(TConn conn, AutoPtr<CObjectIStream> result);

private:
    string                    m_Server;
    string                    m_User;
    string                    m_Password;
    string                    m_DbapiDriver;

    I_DriverContext*          m_Context;
    int                       m_Timeout;
    int                       m_OpenTimeout;

    struct SConnection
    {
        AutoPtr<CDB_Connection> m_Connection;
        AutoPtr<CObjectIStream> m_Result;
    };

    typedef map<TConn, SConnection> TConnections;
    TConnections                    m_Connections;

    bool                      m_ExclWGSMaster;
};

END_SCOPE(objects)
END_NCBI_SCOPE

#endif // READER_PUBSEQ2__HPP_INCLUDED
