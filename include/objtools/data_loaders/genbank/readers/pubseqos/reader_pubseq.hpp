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

#include <objtools/data_loaders/genbank/reader.hpp>
#include <memory>

BEGIN_NCBI_SCOPE

class CDB_Connection;
class CDB_RPCCmd;
class CDB_Result;
class I_DriverContext;

BEGIN_SCOPE(objects)


class CPubseqReader;
class CPubseqBlob;


class NCBI_XREADER_PUBSEQOS_EXPORT CPubseqReader : public CId1ReaderBase
{
public:
    CPubseqReader(TConn parallelLevel = 2,
                  const string& server = "PUBSEQ_OS",
                  const string& user="anyone",
                  const string& pswd = "allowed");

    ~CPubseqReader();

    virtual void ResolveSeq_id(CReaderRequestResult& result,
                               CLoadLockBlob_ids& ids,
                               const CSeq_id& id, TConn conn);
    virtual void ResolveSeq_id(CReaderRequestResult& result,
                               CLoadLockSeq_ids& ids, const CSeq_id& id,
                               TConn conn);
    virtual TBlobVersion GetVersion(const CBlob_id& blob_id, TConn conn);

    virtual void GetTSEBlob(CTSE_Info& tse_info,
                            const CBlob_id& blob_id,
                            TConn conn);
    virtual CRef<CSeq_annot_SNP_Info> GetSNPAnnot(const CBlob_id& blob_id,
                                                  TConn conn);

    virtual TConn GetParallelLevel(void) const;
    virtual void SetParallelLevel(TConn);
    CDB_Connection* GetConnection(TConn);
    virtual void Reconnect(TConn);

private:
    CDB_Connection* x_GetConnection(TConn conn);
    CDB_Connection* x_NewConnection(void);


    CDB_RPCCmd* x_SendRequest(const CBlob_id& blob_id,
                              CDB_Connection* db_conn);
    CDB_Result* x_ReceiveData(CTSE_Info* tse_info, CDB_RPCCmd& cmd);

    void x_ReceiveMainBlob(CTSE_Info& tse_info,
                           const CBlob_id& blob_id,
                           CDB_Result& result);
    CRef<CSeq_annot_SNP_Info> x_ReceiveSNPAnnot(CDB_Result& result);
    
    string                    m_Server;
    string                    m_User;
    string                    m_Password;
    I_DriverContext*          m_Context;
    vector<CDB_Connection *>  m_Pool;
    bool                      m_NoMoreConnections;
};


END_SCOPE(objects)

extern NCBI_XREADER_PUBSEQOS_EXPORT const string kPubseqReaderDriverName;

extern "C" 
{

NCBI_XREADER_PUBSEQOS_EXPORT
void NCBI_EntryPoint_ReaderPubseqos(
     CPluginManager<objects::CReader>::TDriverInfoList&   info_list,
     CPluginManager<objects::CReader>::EEntryPointRequest method);

NCBI_XREADER_PUBSEQOS_EXPORT
void NCBI_EntryPoint_xreader_pubseqos(
     CPluginManager<objects::CReader>::TDriverInfoList&   info_list,
     CPluginManager<objects::CReader>::EEntryPointRequest method);

} // extern C

END_NCBI_SCOPE

#endif // READER_PUBSEQ__HPP_INCLUDED
