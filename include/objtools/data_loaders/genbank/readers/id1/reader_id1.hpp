#ifndef READER_ID1__HPP_INCLUDED
#define READER_ID1__HPP_INCLUDED

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
*  File Description: Data reader from ID1
*
*/

#include <objtools/data_loaders/genbank/reader.hpp>

BEGIN_NCBI_SCOPE

class CConn_ServiceStream;
class CByteSourceReader;
class CObjectIStream;
class CStopWatch;
struct STimeStatistics;
struct STimeSizeStatistics;

BEGIN_SCOPE(objects)

class CID1server_back;
class CID1server_request;
class CID1server_maxcomplex;
class CID1blob_info;
class CID2S_Split_Info;


class NCBI_XREADER_ID1_EXPORT CId1Reader : public CId1ReaderBase
{
public:
    CId1Reader(TConn noConn = 3);
    ~CId1Reader();


    //////////////////////////////////////////////////////////////////
    // Setup methods:

    virtual TConn GetParallelLevel(void) const;
    virtual void SetParallelLevel(TConn noConn);
    virtual void Reconnect(TConn conn);


    //////////////////////////////////////////////////////////////////
    // Id resolution methods:

    virtual void ResolveSeq_id(CLoadLockBlob_ids& ids, const CSeq_id& id,
                               TConn conn);
    virtual int ResolveSeq_id_to_gi(const CSeq_id& seqId,
                                    TConn conn);
    virtual void ResolveGi(CLoadLockBlob_ids& ids, int gi,
                           TConn conn);
    virtual TBlobVersion GetVersion(const CBlob_id& blob_id,
                                    TConn conn);


    //////////////////////////////////////////////////////////////////
    // Blob loading methods:

    virtual void GetTSEBlob(CTSE_Info& tse_info, const CBlob_id& blob_id,
                            TConn conn);
    virtual CRef<CSeq_annot_SNP_Info> GetSNPAnnot(const CBlob_id& blob_id,
                                                  TConn conn);

    void GetSeq_entry(CID1server_back& id1_reply, const CBlob_id& blob_id,
                      TConn conn);
    void SetSeq_entry(CTSE_Info& tse_info, CID1server_back& id1_reply);

protected:
    CConn_ServiceStream* x_GetConnection(TConn conn);
    CConn_ServiceStream* x_NewConnection(void);

    static int CollectStatistics(void); // 0 - no stats, >1 - verbose
    void PrintStatistics(void) const;

    static void PrintStat(const char* type,
                          const char* what,
                          const STimeStatistics& stat);
    static void PrintStat(const char* type,
                          const STimeSizeStatistics& stat);

    static void LogIdStat(const char* type,
                          const char* kind,
                          const string& name,
                          STimeStatistics& stat,
                          CStopWatch& sw);
    static void LogStat(const char* type,
                        const CBlob_id& blob_id,
                        STimeSizeStatistics& stat,
                        CStopWatch& sw,
                        size_t size);
    static void LogStat(const char* type,
                        const string& blob_id,
                        STimeSizeStatistics& stat,
                        CStopWatch& sw,
                        size_t size);

    virtual void x_GetSNPAnnot(CSeq_annot_SNP_Info& snp_info,
                               const CBlob_id&      blob_id,
                               TConn                conn);

    void x_ResolveId(CID1server_back& id1_reply,
                     const CID1server_request& id1_request,
                     TConn conn);

    void x_SendRequest(CConn_ServiceStream* stream,
                       const CID1server_request& request);
    void x_ReceiveReply(CConn_ServiceStream* stream,
                        CID1server_back& reply);

    void x_SendRequest(const CBlob_id& blob_id,
                       CConn_ServiceStream* stream);

    void x_SetParams(CID1server_maxcomplex& params,
                     const CBlob_id& blob_id);
    virtual void x_SetBlobRequest(CID1server_request& request,
                                  const CBlob_id& blob_id);
    virtual void x_ReadBlobReply(CID1server_back& reply,
                                 CObjectIStream& stream,
                                 const CBlob_id& blob_id);
    TBlobVersion x_GetVersion(const CID1server_back& reply);

private:

    CRef<CTSE_Info> x_ReceiveMainBlob(CConn_ServiceStream* stream);

    vector<CConn_ServiceStream *>   m_Pool;
    auto_ptr<CConn_ServiceStream>   m_FirstConnection;
    bool                            m_NoMoreConnections;
};


END_SCOPE(objects)

extern NCBI_XREADER_ID1_EXPORT const string kId1ReaderDriverName;

extern "C" 
{

NCBI_XREADER_ID1_EXPORT
void NCBI_EntryPoint_Id1Reader(
     CPluginManager<objects::CReader>::TDriverInfoList&   info_list,
     CPluginManager<objects::CReader>::EEntryPointRequest method);

NCBI_XREADER_ID1_EXPORT
void NCBI_EntryPoint_xreader_id1(
     CPluginManager<objects::CReader>::TDriverInfoList&   info_list,
     CPluginManager<objects::CReader>::EEntryPointRequest method);

} // extern C

END_NCBI_SCOPE

#endif // READER_ID1__HPP_INCLUDED
