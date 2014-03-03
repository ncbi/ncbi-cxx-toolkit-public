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

#include <objtools/data_loaders/genbank/reader_id1_base.hpp>

BEGIN_NCBI_SCOPE

class CDB_Connection;
class CDB_Result;
class I_DriverContext;
class I_BaseCmd;

BEGIN_SCOPE(objects)

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

    ~CPubseqReader();

    int GetMaximumConnectionsLimit(void) const;

    bool LoadSeq_idGi(CReaderRequestResult& result,
                      const CSeq_id_Handle& seq_id);
    bool LoadSeq_idAccVer(CReaderRequestResult& result,
                          const CSeq_id_Handle& seq_id);

    bool LoadSeq_idSeq_ids(CReaderRequestResult& result,
                           const CSeq_id_Handle& seq_id);
    bool LoadGiSeq_ids(CReaderRequestResult& result,
                       const CSeq_id_Handle& seq_id);

    bool LoadSeq_idBlob_ids(CReaderRequestResult& result,
                            const CSeq_id_Handle& seq_id,
                            const SAnnotSelector* sel);

    bool LoadSeq_idInfo(CReaderRequestResult& result,
                        const CSeq_id_Handle& seq_id,
                        const SAnnotSelector* with_named_accs);

    void GetBlobState(CReaderRequestResult& result,
                      const CBlob_id& blob_id);
    void GetBlobVersion(CReaderRequestResult& result,
                        const CBlob_id& blob_id);

    void GetBlob(CReaderRequestResult& result,
                 const TBlobId& blob_id,
                 TChunkId chunk_id);

protected:
    void x_AddConnectionSlot(TConn conn);
    void x_RemoveConnectionSlot(TConn conn);
    void x_DisconnectAtSlot(TConn conn, bool failed);
    void x_ConnectAtSlot(TConn conn);

    CDB_Connection* x_GetConnection(TConn conn);

    I_BaseCmd* x_SendRequest(const CBlob_id& blob_id,
                             CDB_Connection* db_conn,
                             const char* rpc);

    struct SReceiveData {
        SReceiveData() : zip_type(0), blob_state(0), blob_version(0) {}

        AutoPtr<CDB_Result> dbr;
        int zip_type;
        TBlobState blob_state;
        TBlobVersion blob_version;
    };
    SReceiveData x_ReceiveData(CReaderRequestResult& result,
                               const TBlobId& blob_id,
                               I_BaseCmd& cmd,
                               bool force_blob);
    
private:
    string                    m_Server;
    string                    m_User;
    string                    m_Password;
    string                    m_DbapiDriver;

    I_DriverContext*          m_Context;

    typedef map< TConn, AutoPtr<CDB_Connection> > TConnections;
    TConnections              m_Connections;

    bool                      m_AllowGzip;
    bool                      m_ExclWGSMaster;
};

END_SCOPE(objects)
END_NCBI_SCOPE

#endif // READER_PUBSEQ__HPP_INCLUDED
