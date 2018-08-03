#ifndef READER_PUBSEQ__HPP_INCLUDED
#define READER_PUBSEQ__HPP_INCLUDED

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

#include <objtools/data_loaders/genbank/impl/reader_id1_base.hpp>

BEGIN_NCBI_SCOPE

class CDB_Connection;
class CDB_Result;
class I_DriverContext;
class I_BaseCmd;

BEGIN_SCOPE(objects)

struct SPubseqReaderReceiveData;

class NCBI_XREADER_PUBSEQOS_EXPORT CPubseqReader : public CId1ReaderBase
{
public:
    CPubseqReader(int max_connections = 0,
                  const string& server = kEmptyStr,
                  const string& user = kEmptyStr,
                  const string& pswd = kEmptyStr,
                  const string& dbapi_driver = kEmptyStr);
    CPubseqReader(const TPluginManagerParamTree* params,
                  const string& driver_name);

    virtual ~CPubseqReader() override;

    virtual int GetMaximumConnectionsLimit(void) const override;

    virtual bool LoadSeq_idGi(CReaderRequestResult& result,
                              const CSeq_id_Handle& seq_id) override;
    virtual bool LoadSeq_idAccVer(CReaderRequestResult& result,
                                  const CSeq_id_Handle& seq_id) override;

    virtual bool LoadSeq_idSeq_ids(CReaderRequestResult& result,
                                   const CSeq_id_Handle& seq_id) override;
    bool LoadGiSeq_ids(CReaderRequestResult& result,
                       const CSeq_id_Handle& seq_id);
    virtual bool LoadSequenceHash(CReaderRequestResult& result,
                                  const CSeq_id_Handle& seq_id) override;
    bool LoadGiHash(CReaderRequestResult& result,
                    const CSeq_id_Handle& seq_id);

    virtual bool LoadSeq_idBlob_ids(CReaderRequestResult& result,
                                    const CSeq_id_Handle& seq_id,
                                    const SAnnotSelector* sel) override;

    bool LoadSeq_idInfo(CReaderRequestResult& result,
                        const CSeq_id_Handle& seq_id,
                        const SAnnotSelector* with_named_accs);

    virtual void GetBlobState(CReaderRequestResult& result,
                              const CBlob_id& blob_id) override;
    virtual void GetBlobVersion(CReaderRequestResult& result,
                                const CBlob_id& blob_id) override;

    virtual void GetBlob(CReaderRequestResult& result,
                         const TBlobId& blob_id,
                         TChunkId chunk_id) override;

    virtual void SetIncludeHUP(bool include_hup = true,
                               const string& web_cookie = NcbiEmptyString) override;

protected:
    virtual void x_AddConnectionSlot(TConn conn) override;
    virtual void x_RemoveConnectionSlot(TConn conn) override;
    virtual void x_DisconnectAtSlot(TConn conn, bool failed) override;
    virtual void x_ConnectAtSlot(TConn conn) override;

    CDB_Connection* x_GetConnection(TConn conn);

    I_BaseCmd* x_SendRequest(const CBlob_id& blob_id,
                             CDB_Connection* db_conn,
                             const char* rpc);

    void x_ReceiveData(CReaderRequestResult& result,
                       SPubseqReaderReceiveData& data,
                       const TBlobId& blob_id,
                       I_BaseCmd& cmd,
                       bool force_blob);
    
private:
    string                    m_Server;
    string                    m_User;
    string                    m_Password;
    string                    m_DbapiDriver;

    unique_ptr<I_DriverContext>   m_Context;

    typedef map< TConn, AutoPtr<CDB_Connection> > TConnections;
    TConnections              m_Connections;

    bool                      m_AllowGzip;
    bool                      m_ExclWGSMaster;
    bool                      m_SetCubbyUser;
};

END_SCOPE(objects)
END_NCBI_SCOPE

#endif // READER_PUBSEQ__HPP_INCLUDED
