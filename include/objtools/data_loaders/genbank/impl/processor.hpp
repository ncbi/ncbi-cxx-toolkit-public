#ifndef PROCESSOR__HPP_INCLUDED
#define PROCESSOR__HPP_INCLUDED
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
*  Author:  Eugene Vasilchenko
*
*  File Description: blob stream processor interface
*
*/

#include <corelib/ncbiobj.hpp>
#include <corelib/ncbi_limits.h>
#include <corelib/ncbi_param.hpp>
#include <objtools/data_loaders/genbank/impl/statistics.hpp>
#include <objtools/data_loaders/genbank/reader_snp.hpp>

BEGIN_NCBI_SCOPE

class CObjectIStream;
class CBeginInfo;
class CStopWatch;

BEGIN_SCOPE(objects)

class CBlob_id;
class CReaderRequestResult;
class CReaderRequestResultRecursion;
class CReadDispatcher;
class CWriter;
class CID2_Reply_Data;
class CLoadLockBlob;
class CLoadLockSetter;
class CTSE_Chunk_Info;
class CTSE_SetObjectInfo;
class CDataLoader;
struct STimeSizeStatistics;

class NCBI_XREADER_EXPORT CProcessor : public CObject 
{
public:
    typedef CBlob_id    TBlobId;
    typedef int         TBlobState;
    typedef int         TBlobVersion;
    typedef int         TChunkId;
    enum EType {
        eType_ID1,                 // ID1server-back
        eType_ID1_SNP,             // ID1server-back with SNPs
        eType_Seq_entry,           // Seq-entry, ASN.1 binary
        eType_Seq_entry_SNP,       // Seq-entry with SNPs, ASN.1 binary
        eType_St_Seq_entry,        // State word + Seq-entry, ASN.1 binary
        eType_St_Seq_entry_SNPT,   // With additional SNP table
        eType_ID2,                 // Any ID2 reply data
        eType_ID2AndSkel,          // Two ID2 reply data objects, Split&Skel
        eType_ID2_SE,              // ID2 reply data with Seq-entry
        eType_ID2_Split,           // ID2 reply data with Split-info (+version)
        eType_ID2_Chunk,           // ID2 reply data with Chunk
        eType_ID2_SNP,             // ID2 reply data with SNP Seq-entry
        eType_ExtAnnot,            // Special kind of external annotations
        eType_AnnotInfo            // Special kind of named annotations
    };
    typedef unsigned TMagic;

    virtual ~CProcessor(void);

    virtual EType GetType(void) const = 0;
    virtual TMagic GetMagic(void) const = 0;

    virtual void ProcessStream(CReaderRequestResult& result,
                               const TBlobId& blob_id,
                               TChunkId chunk_id,
                               CNcbiIstream& stream) const;
    virtual void ProcessObjStream(CReaderRequestResult& result,
                                  const TBlobId& blob_id,
                                  TChunkId chunk_id,
                                  CObjectIStream& obj_stream) const;
    
    void ProcessBlobFromID2Data(CReaderRequestResult& result,
                                const TBlobId& blob_id,
                                TChunkId chunk_id,
                                const CID2_Reply_Data& data) const;

    static void RegisterAllProcessors(CReadDispatcher& dispatcher);

    static bool TryStringPack(void);
    static bool TrySNPSplit(void);
    static bool TrySNPTable(void);

    static void SetSeqEntryReadHooks(CObjectIStream& in);
    static void SetSNPReadHooks(CObjectIStream& in);

    static void SetLoaded(CReaderRequestResult& result,
                          const TBlobId& blob_id,
                          TChunkId chunk_id,
                          CLoadLockBlob& blob);
    static void AddWGSMaster(CLoadLockSetter& blob);
    static void LoadWGSMaster(CDataLoader* loader,
                              CRef<CTSE_Chunk_Info> chunk);

    static TIntId GetGiOffset(void);
    static void OffsetGi(TGi& gi, TIntId gi_offset)
    {
        if ( gi ) {
            gi = gi + gi_offset;
        }
    }
#ifdef NCBI_STRICT_GI
    static void OffsetGi(TIntId& gi, TIntId gi_offset)
    {
        if ( gi ) {
            gi = gi + gi_offset;
        }
    }
#endif
    static bool OffsetId(CSeq_id& id, TIntId gi_offset);
    static bool OffsetId(CSeq_id_Handle& id, TIntId gi_offset);
    static void OffsetAllGis(CBeginInfo obj, TIntId gi_offset);
    static void OffsetAllGis(CTSE_SetObjectInfo& set_info, TIntId gi_offset);

    static TGi ConvertGiFromOM(TGi gi)
    {
        OffsetGi(gi, -GetGiOffset());
        return gi;
    }
    static TGi ConvertGiToOM(TGi gi)
    {
        OffsetGi(gi, GetGiOffset());
        return gi;
    }
    static CSeq_id_Handle ConvertIdFromOM(CSeq_id_Handle id)
    {
        OffsetId(id, -GetGiOffset());
        return id;
    }
    static void OffsetGiToOM(TGi& gi)
    {
        OffsetGi(gi, GetGiOffset());
    }
    static void OffsetIdToOM(CSeq_id& id)
    {
        OffsetId(id, GetGiOffset());
    }
    static void OffsetAllGisFromOM(CBeginInfo obj);
    static void OffsetAllGisToOM(CBeginInfo obj, CTSE_SetObjectInfo* set_info = 0);

protected:
    CProcessor(CReadDispatcher& dispatcher);

    CReadDispatcher* m_Dispatcher;

    CWriter* GetWriter(const CReaderRequestResult& result) const;

    static int CollectStatistics(void); // 0 - no stats, >1 - verbose
    static void LogStat(CReaderRequestResultRecursion& recursion,
                        const CBlob_id& blob_id,
                        CGBRequestStatistics::EStatType stat_type,
                        const char* descr,
                        double size);
    static void LogStat(CReaderRequestResultRecursion& recursion,
                        const CBlob_id& blob_id,
                        int chunk_id,
                        CGBRequestStatistics::EStatType stat_type,
                        const char* descr,
                        double size);
    static void LogStat(CReaderRequestResultRecursion& recursion,
                        const CBlob_id& blob_id,
                        CGBRequestStatistics::EStatType stat_type,
                        const char* descr,
                        CNcbiStreampos size)
    {
        LogStat(recursion, blob_id, stat_type, descr, double(size));
    }
    static void LogStat(CReaderRequestResultRecursion& recursion,
                        const CBlob_id& blob_id,
                        int chunk_id,
                        CGBRequestStatistics::EStatType stat_type,
                        const char* descr,
                        CNcbiStreampos size)
    {
        LogStat(recursion, blob_id, chunk_id, stat_type, descr, double(size));
    }
    static void LogStat(CReaderRequestResultRecursion& recursion,
                        const CBlob_id& blob_id,
                        CGBRequestStatistics::EStatType stat_type,
                        const char* descr,
                        size_t size)
    {
        LogStat(recursion, blob_id, stat_type, descr, double(size));
    }
    static void LogStat(CReaderRequestResultRecursion& recursion,
                        const CBlob_id& blob_id,
                        int chunk_id,
                        CGBRequestStatistics::EStatType stat_type,
                        const char* descr,
                        size_t size)
    {
        LogStat(recursion, blob_id, chunk_id, stat_type, descr, double(size));
    }
};


// Parameters' declarations
NCBI_PARAM_DECL(bool, GENBANK, SNP_PACK_STRINGS);
NCBI_PARAM_DECL(bool, GENBANK, SNP_SPLIT);
NCBI_PARAM_DECL(bool, GENBANK, SNP_TABLE);
NCBI_PARAM_DECL(bool, GENBANK, USE_MEMORY_POOL);
NCBI_PARAM_DECL(int, GENBANK, READER_STATS);
NCBI_PARAM_DECL(bool, GENBANK, CACHE_RECOMPRESS);
NCBI_PARAM_DECL(bool, GENBANK, ADD_WGS_MASTER);
NCBI_PARAM_DECL(Int8, GENBANK, GI_OFFSET);

END_SCOPE(objects)
END_NCBI_SCOPE

#endif//PROCESSOR__HPP_INCLUDED
