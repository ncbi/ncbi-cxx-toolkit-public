#ifndef READER__HPP_INCLUDED
#define READER__HPP_INCLUDED
/* */

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
*  File Description: Base data reader interface
*
*/

#include <corelib/ncbiobj.hpp>
#include <corelib/plugin_manager.hpp>
#include <objtools/data_loaders/genbank/blob_id.hpp>
#include <objtools/data_loaders/genbank/request_result.hpp>
#include <vector>

//#define GENBANK_USE_SNP_SATELLITE_15 1

BEGIN_NCBI_SCOPE

class CObjectIStream;

BEGIN_SCOPE(objects)

//class CSeqref;
class CBlob_id;
class CSeq_id;
class CSeq_id_Handle;
class CTSE_Info;
class CTSE_Chunk_Info;
class CSeq_annot_SNP_Info;

class CSeq_entry;
class CID2S_Split_Info;
class CID2S_Chunk;

class CReaderRequestResult;
class CLoadLockBlob_ids;

class NCBI_XREADER_EXPORT CReader : public CObject
{
public:
    CReader(void);
    virtual ~CReader(void);

    typedef unsigned TConn;
    typedef CBlob_id TBlob_id;
    typedef int TBlobVersion;
    typedef int TChunk_id;
    typedef int TContentsMask;

    // new interface
    virtual void ResolveString(CReaderRequestResult& result,
                               const string& seq_id) = 0;
    virtual void ResolveSeq_id(CReaderRequestResult& result,
                               const CSeq_id_Handle& seq_id) = 0;
    virtual void ResolveSeq_ids(CReaderRequestResult& result,
                                const CSeq_id_Handle& seq_id) = 0;
    virtual void LoadBlobs(CReaderRequestResult& result,
                           const string& seq_id,
                           TContentsMask mask);
    virtual void LoadBlobs(CReaderRequestResult& result,
                           const CSeq_id_Handle& seq_id,
                           TContentsMask mask);
    virtual void LoadBlobs(CReaderRequestResult& result,
                           CLoadLockBlob_ids blobs,
                           TContentsMask mask);
    virtual void LoadBlob(CReaderRequestResult& result,
                          const CBlob_id& blob_id) = 0;
    virtual TBlobVersion GetBlobVersion(CReaderRequestResult& result,
                                        const TBlob_id& blob_id) = 0;
    virtual void LoadChunk(CReaderRequestResult& result,
                           const TBlob_id& blob_id, TChunk_id chunk_id);

    // return the level of reasonable parallelism
    // 1 - non MTsafe; 0 - no synchronization required,
    // n - at most n connection is advised/supported
    virtual TConn GetParallelLevel(void) const = 0;
    virtual void SetParallelLevel(TConn conn) = 0;
    virtual void Reconnect(TConn conn) = 0;

    // returns the time in secons when already retrived data
    // could become obsolete by fresher version 
    // -1 - never
    virtual int GetConst(const string& const_name) const;

    static bool TryStringPack(void);

    static void SetSeqEntryReadHooks(CObjectIStream& in);
    static void SetSNPReadHooks(CObjectIStream& in);
};


class NCBI_XREADER_EXPORT CId1ReaderBase : public CReader
{
public:
    CId1ReaderBase(void);
    ~CId1ReaderBase(void);

    void ResolveString(CReaderRequestResult& result,
                       const string& seq_id);
    void ResolveSeq_id(CReaderRequestResult& result,
                       const CSeq_id_Handle& seq_id);
    void ResolveSeq_ids(CReaderRequestResult& result,
                        const CSeq_id_Handle& seq_id);
    void LoadBlob(CReaderRequestResult& result,
                  const TBlob_id& blob_id);
    TBlobVersion GetBlobVersion(CReaderRequestResult& result,
                                const TBlob_id& blob_id);
    void LoadChunk(CReaderRequestResult& result,
                   const TBlob_id& blob_id, TChunk_id chunk_id);

    virtual void ResolveSeq_id(CReaderRequestResult& result,
                               CLoadLockBlob_ids& ids,
                               const CSeq_id& id,
                               TConn conn) = 0;
    virtual void ResolveSeq_id(CReaderRequestResult& result,
                               CLoadLockSeq_ids& ids,
                               const CSeq_id& id,
                               TConn conn) = 0;
    
    virtual void GetBlob(CTSE_Info& tse_info,
                         const CBlob_id& blob_id,
                         TConn conn);

    virtual void GetChunk(CTSE_Chunk_Info& chunk_info,
                          const CBlob_id& blob_id,
                          TConn conn);

    virtual void GetTSEBlob(CTSE_Info& tse_info,
                            const CBlob_id& blob_id,
                            TConn conn) = 0;
    virtual void GetSNPBlob(CTSE_Info& tse_info,
                            const CBlob_id& blob_id,
                            TConn conn);

    virtual void GetTSEChunk(CTSE_Chunk_Info& chunk_info,
                             const CBlob_id& blob_id,
                             TConn conn);
    virtual void GetSNPChunk(CTSE_Chunk_Info& chunk_info,
                             const CBlob_id& blob_id,
                             TConn conn);

    virtual TBlobVersion GetVersion(const CBlob_id& blob_id, TConn conn) = 0;

    virtual CRef<CSeq_annot_SNP_Info> GetSNPAnnot(const CBlob_id& blob_id,
                                                  TConn conn) = 0;
    
    enum ESat {
        eSat_SNP        = 15,
        eSat_ANNOT      = 26,
        eSat_TRACE      = 28,
        eSat_TRACE_ASSM = 29,
        eSat_TR_ASSM_CH = 30,
        eSat_TRACE_CHGR = 31
    };

    enum ESubSat {
        eSubSat_main =    0,
        eSubSat_SNP  = 1<<0,
        eSubSat_SNP_graph  = 1<<2,
        eSubSat_CDD  = 1<<3,
        eSubSat_MGC  = 1<<4
    };

    enum {
        kSNP_EntryId = 0,
        kSNP_ChunkId = 0,
        kSkeleton_ChunkId = 0
    };

    static bool IsSNPBlob_id(const CBlob_id& blob_id);
    static void AddSNPBlob_id(CLoadLockBlob_ids& ids, int gi);

    static bool IsAnnotBlob_id(const CBlob_id& blob_id);

    static bool TrySNPSplit(void);
    static bool TrySNPTable(void);
};


END_SCOPE(objects)

NCBI_DECLARE_INTERFACE_VERSION(objects::CReader,  "xreader", 1, 1, 0);

template<>
class CDllResolver_Getter<objects::CReader>
{
public:
    CPluginManager_DllResolver* operator()(void)
    {
        CPluginManager_DllResolver* resolver =
            new CPluginManager_DllResolver
            (CInterfaceVersion<objects::CReader>::GetName(), 
             kEmptyStr,
             CVersionInfo::kAny,
             CDll::eAutoUnload);

        resolver->SetDllNamePrefix("ncbi");
        return resolver;
    }
};


END_NCBI_SCOPE

#endif // READER__HPP_INCLUDED
