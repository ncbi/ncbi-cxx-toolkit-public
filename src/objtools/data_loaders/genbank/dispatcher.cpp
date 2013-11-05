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

#include <ncbi_pch.hpp>
#include <objtools/data_loaders/genbank/dispatcher.hpp>
#include <objtools/data_loaders/genbank/reader.hpp>
#include <objtools/data_loaders/genbank/writer.hpp>
#include <objtools/data_loaders/genbank/processor.hpp>
#include <objtools/data_loaders/genbank/request_result.hpp>
#include <objtools/data_loaders/genbank/statistics.hpp>
#include <objtools/error_codes.hpp>
#include <objmgr/objmgr_exception.hpp>
#include <objmgr/impl/tse_split_info.hpp>
#include <objmgr/impl/tse_chunk_info.hpp>


#define NCBI_USE_ERRCODE_X   Objtools_Rd_Disp

BEGIN_NCBI_SCOPE

NCBI_DEFINE_ERR_SUBCODE_X(10);

BEGIN_SCOPE(objects)

/////////////////////////////////////////////////////////////////////////////
// CReadDispatcher
/////////////////////////////////////////////////////////////////////////////


static CGBRequestStatistics sx_Statistics[CGBRequestStatistics::eStats_Count] =
{
    CGBRequestStatistics("resolved", "string ids"),
    CGBRequestStatistics("resolved", "seq-ids"),
    CGBRequestStatistics("resolved", "gis"),
    CGBRequestStatistics("resolved", "accs"),
    CGBRequestStatistics("resolved", "labels"),
    CGBRequestStatistics("resolved", "taxids"),
    CGBRequestStatistics("resolved", "blob ids"),
    CGBRequestStatistics("resolved", "blob versions"),
    CGBRequestStatistics("loaded", "blob data"),
    CGBRequestStatistics("loaded", "SNP data"),
    CGBRequestStatistics("loaded", "split data"),
    CGBRequestStatistics("loaded", "chunk data"),
    CGBRequestStatistics("parsed", "blob data"),
    CGBRequestStatistics("parsed", "SNP data"),
    CGBRequestStatistics("parsed", "split data"),
    CGBRequestStatistics("parsed", "chunk data")
};

CGBRequestStatistics::CGBRequestStatistics(const char* action,
                                           const char* entity)
    : m_Action(action), m_Entity(entity),
      m_Count(0), m_Time(0), m_Size(0)
{
}

const CGBRequestStatistics& CGBRequestStatistics::GetStatistics(EStatType type)
{
    if ( type < eStat_First || type > eStat_Last ) {
        NCBI_THROW_FMT(CLoaderException, eOtherError,
                       "CGBRequestStatistics::GetStatistics: "
                       "invalid statistics type: "<<type);
    }
    return sx_Statistics[type];
}

void CGBRequestStatistics::PrintStat(void) const
{
    size_t count = GetCount();
    if ( count > 0 ) {
        double time = GetTime();
        double size = GetSize();
        if ( size <= 0 ) {
            LOG_POST_X(5, "GBLoader: " << GetAction() << ' ' <<
                       count << ' ' << GetEntity() << " in " <<
                       setiosflags(ios::fixed) <<
                       setprecision(3) <<
                       (time) << " s (" <<
                       (time*1000/count) << " ms/one)");
        }
        else {
            LOG_POST_X(6, "GBLoader: " << GetAction() << ' ' <<
                       count << ' ' << GetEntity() << " in " <<
                       setiosflags(ios::fixed) <<
                       setprecision(3) <<
                       (time) << " s (" <<
                       (time*1000/count) << " ms/one)" <<
                       setprecision(2) << " (" <<
                       (size/1024.0) << " kB " <<
                       (size/time/1024) << " kB/s)");
        }
    }
}


void CGBRequestStatistics::PrintStatistics(void)
{
    for ( int type = eStat_First; type <= eStat_Last; ++type ) {
        sx_Statistics[type].PrintStat();
    }
}

inline
int CReadDispatcher::CollectStatistics(void)
{
    static const int s_Value =
        NCBI_PARAM_TYPE(GENBANK, READER_STATS)::GetDefault();
    return s_Value;
}


CReadDispatcher::CReadDispatcher(void)
{
    CollectStatistics();
    CProcessor::RegisterAllProcessors(*this);
}


CReadDispatcher::~CReadDispatcher(void)
{
    if ( CollectStatistics() > 0 ) {
        CGBRequestStatistics::PrintStatistics();
    }
}


void CReadDispatcher::InsertReader(TLevel level, CRef<CReader> reader)
{
    if ( !reader ) {
        return;
    }

    m_Readers[level] = reader;
    reader->m_Dispatcher = this;
}


void CReadDispatcher::InsertWriter(TLevel level, CRef<CWriter> writer)
{
    if ( !writer ) {
        return;
    }

    m_Writers[level] = writer;
}


void CReadDispatcher::InsertProcessor(CRef<CProcessor> processor)
{
    if ( !processor ) {
        return;
    }

    m_Processors[processor->GetType()] = processor;
}


CWriter* CReadDispatcher::GetWriter(const CReaderRequestResult& result, 
                                    CWriter::EType type) const
{
    ITERATE ( TWriters, i, m_Writers ) {
        if ( i->first >= result.GetLevel() ) {
            break;
        }
        if ( i->second->CanWrite(type) ) {
            return const_cast<CWriter*>(i->second.GetPointer());
        }
    }
    return 0;
}


const CProcessor& CReadDispatcher::GetProcessor(CProcessor::EType type) const
{
    TProcessors::const_iterator iter = m_Processors.find(type);
    if ( iter == m_Processors.end() ) {
        NCBI_THROW_FMT(CLoaderException, eLoaderFailed,
                       "CReadDispatcher::GetProcessor: "
                       "processor unknown: "<<type);
    }
    return *iter->second;
}


void CReadDispatcher::CheckReaders(void) const
{
    if ( m_Readers.empty() ) {
        NCBI_THROW(CLoaderException, eLoaderFailed, "no reader loaded");
    }
}


void CReadDispatcher::ResetCaches(void)
{
    NON_CONST_ITERATE(TReaders, rd, m_Readers) {
        rd->second->ResetCache();
    }
    NON_CONST_ITERATE(TWriters, wr, m_Writers) {
        wr->second->ResetCache();
    }
}


CReadDispatcherCommand::CReadDispatcherCommand(CReaderRequestResult& result)
    : m_Result(result)
{
}


CReadDispatcherCommand::~CReadDispatcherCommand(void)
{
}


bool CReadDispatcherCommand::MayBeSkipped(void) const
{
    return false;
}


namespace {
    class CCommandLoadStringSeq_ids : public CReadDispatcherCommand
    {
    public:
        typedef string TKey;
        typedef CLoadLockSeq_ids TLock;
        CCommandLoadStringSeq_ids(CReaderRequestResult& result,
                                  const TKey& key)
            : CReadDispatcherCommand(result),
              m_Key(key), m_Lock(result, key)
            {
            }

        bool IsDone(void)
            {
                return m_Lock.IsLoaded();
            }
        bool Execute(CReader& reader)
            {
                return reader.LoadStringSeq_ids(GetResult(), m_Key);
            }
        string GetErrMsg(void) const
            {
                return "LoadStringSeq_ids("+m_Key+"): "
                    "data not found";
            }
        CGBRequestStatistics::EStatType GetStatistics(void) const
            {
                return CGBRequestStatistics::eStat_StringSeq_ids;
            }
        string GetStatisticsDescription(void) const
            {
                return "Seq-ids(string "+m_Key+")";
            }
        
    private:
        TKey m_Key;
        TLock m_Lock;
    };

    class CCommandLoadSeq_idSeq_ids : public CReadDispatcherCommand
    {
    public:
        typedef CSeq_id_Handle TKey;
        typedef CLoadLockSeq_ids TLock;
        CCommandLoadSeq_idSeq_ids(CReaderRequestResult& result,
                                  const TKey& key)
            : CReadDispatcherCommand(result),
              m_Key(key), m_Lock(result, key)
            {
            }

        bool IsDone(void)
            {
                return m_Lock.IsLoaded();
            }
        bool Execute(CReader& reader)
            {
                return reader.LoadSeq_idSeq_ids(GetResult(), m_Key);
            }
        string GetErrMsg(void) const
            {
                return "LoadSeq_idSeq_ids("+m_Key.AsString()+"): "
                    "data not found";
            }
        CGBRequestStatistics::EStatType GetStatistics(void) const
            {
                return CGBRequestStatistics::eStat_Seq_idSeq_ids;
            }
        string GetStatisticsDescription(void) const
            {
                return "Seq-ids("+m_Key.AsString()+")";
            }
        
    private:
        TKey m_Key;
        TLock m_Lock;
    };

    class CCommandLoadSeq_idGi : public CReadDispatcherCommand
    {
    public:
        typedef CSeq_id_Handle TKey;
        typedef CLoadLockSeq_ids TLock;
        CCommandLoadSeq_idGi(CReaderRequestResult& result,
                             const TKey& key)
            : CReadDispatcherCommand(result),
              m_Key(key), m_Lock(result, key)
            {
            }

        bool IsDone(void)
            {
                return m_Lock.IsLoadedGi();
            }
        bool Execute(CReader& reader)
            {
                return reader.LoadSeq_idGi(GetResult(), m_Key);
            }
        string GetErrMsg(void) const
            {
                return "LoadSeq_idGi("+m_Key.AsString()+"): "
                    "data not found";
            }
        CGBRequestStatistics::EStatType GetStatistics(void) const
            {
                return CGBRequestStatistics::eStat_Seq_idGi;
            }
        string GetStatisticsDescription(void) const
            {
                return "gi("+m_Key.AsString()+")";
            }
        
    private:
        TKey m_Key;
        TLock m_Lock;
    };

    class CCommandLoadSeq_idAccVer : public CReadDispatcherCommand
    {
    public:
        typedef CSeq_id_Handle TKey;
        typedef CLoadLockSeq_ids TLock;
        CCommandLoadSeq_idAccVer(CReaderRequestResult& result,
                                 const TKey& key)
            : CReadDispatcherCommand(result),
              m_Key(key), m_Lock(result, key)
            {
            }

        bool IsDone(void)
            {
                return m_Lock.IsLoadedAccVer();
            }
        bool Execute(CReader& reader)
            {
                return reader.LoadSeq_idAccVer(GetResult(), m_Key);
            }
        string GetErrMsg(void) const
            {
                return "LoadSeq_idAccVer("+m_Key.AsString()+"): "
                    "data not found";
            }
        CGBRequestStatistics::EStatType GetStatistics(void) const
            {
                return CGBRequestStatistics::eStat_Seq_idAcc;
            }
        string GetStatisticsDescription(void) const
            {
                return "acc("+m_Key.AsString()+")";
            }
        
    private:
        TKey m_Key;
        TLock m_Lock;
    };

    class CCommandLoadSeq_idLabel : public CReadDispatcherCommand
    {
    public:
        typedef CSeq_id_Handle TKey;
        typedef CLoadLockSeq_ids TLock;
        CCommandLoadSeq_idLabel(CReaderRequestResult& result,
                                const TKey& key)
            : CReadDispatcherCommand(result),
              m_Key(key), m_Lock(result, key)
            {
            }

        bool IsDone(void)
            {
                return m_Lock.IsLoadedLabel();
            }
        bool Execute(CReader& reader)
            {
                return reader.LoadSeq_idLabel(GetResult(), m_Key);
            }
        string GetErrMsg(void) const
            {
                return "LoadSeq_idLabel("+m_Key.AsString()+"): "
                    "data not found";
            }
        CGBRequestStatistics::EStatType GetStatistics(void) const
            {
                return CGBRequestStatistics::eStat_Seq_idLabel;
            }
        string GetStatisticsDescription(void) const
            {
                return "label("+m_Key.AsString()+")";
            }
        
    private:
        TKey m_Key;
        TLock m_Lock;
    };

    class CCommandLoadSeq_idTaxId : public CReadDispatcherCommand
    {
    public:
        typedef CSeq_id_Handle TKey;
        typedef CLoadLockSeq_ids TLock;
        CCommandLoadSeq_idTaxId(CReaderRequestResult& result,
                                const TKey& key)
            : CReadDispatcherCommand(result),
              m_Key(key), m_Lock(result, key)
            {
            }

        bool IsDone(void)
            {
                return m_Lock.IsLoadedTaxId();
            }
        bool Execute(CReader& reader)
            {
                return reader.LoadSeq_idTaxId(GetResult(), m_Key);
            }
        bool MayBeSkipped(void) const
            {
                return true;
            }
        string GetErrMsg(void) const
            {
                return "LoadSeq_idTaxId("+m_Key.AsString()+"): "
                    "data not found";
            }
        CGBRequestStatistics::EStatType GetStatistics(void) const
            {
                return CGBRequestStatistics::eStat_Seq_idTaxId;
            }
        string GetStatisticsDescription(void) const
            {
                return "taxid("+m_Key.AsString()+")";
            }
        
    private:
        TKey m_Key;
        TLock m_Lock;
    };

    bool s_Blob_idsLoaded(CLoadLockBlob_ids& ids, CLoadLockSeq_ids& seq_ids)
    {
        if ( ids.IsLoaded() ) {
            return true;
        }
        // check if seq-id is known as absent
        if ( seq_ids.IsLoaded() &&
             (seq_ids->GetState() & CBioseq_Handle::fState_no_data) ) {
            // mark blob-ids as absent too
            ids.SetNoBlob_ids(seq_ids->GetState());
            return true;
        }
        return false;
    }

    class CCommandLoadSeq_idBlob_ids : public CReadDispatcherCommand
    {
    public:
        typedef CSeq_id_Handle TKey;
        typedef CLoadLockSeq_ids TPreLock;
        typedef CLoadLockBlob_ids TLock;
        CCommandLoadSeq_idBlob_ids(CReaderRequestResult& result,
                                   const TKey& key,
                                   const SAnnotSelector* sel)
            : CReadDispatcherCommand(result),
              m_Key(key), m_Selector(sel),
              m_PreLock(result, key), m_Lock(result, key, sel)
            {
            }

        bool IsDone(void)
            {
                return s_Blob_idsLoaded(m_Lock, m_PreLock);
            }
        bool Execute(CReader& reader)
            {
                return reader.LoadSeq_idBlob_ids(GetResult(),
                                                 m_Key, m_Selector);
            }
        string GetErrMsg(void) const
            {
                return "LoadSeq_idBlob_ids("+m_Key.AsString()+"): "
                    "data not found";
            }
        CGBRequestStatistics::EStatType GetStatistics(void) const
            {
                return CGBRequestStatistics::eStat_Seq_idBlob_ids;
            }
        string GetStatisticsDescription(void) const
            {
                return "blob-ids("+m_Key.AsString()+")";
            }
        
    private:
        TKey m_Key;
        const SAnnotSelector* m_Selector;
        TPreLock m_PreLock;
        TLock m_Lock;
    };

    class CCommandLoadAccVers : public CReadDispatcherCommand
    {
    public:
        typedef vector<CSeq_id_Handle> TKey;
        typedef vector<bool> TLoaded;
        typedef vector<CSeq_id_Handle> TRet;
        CCommandLoadAccVers(CReaderRequestResult& result,
                            const TKey& key, TLoaded& loaded, TRet& ret)
            : CReadDispatcherCommand(result),
              m_Key(key), m_Loaded(loaded), m_Ret(ret)
            {
            }

        bool IsDone(void)
            {
                return std::find(m_Loaded.begin(), m_Loaded.end(), false) ==
                    m_Loaded.end();
            }
        bool Execute(CReader& reader)
            {
                return reader.LoadAccVers(GetResult(), m_Key, m_Loaded, m_Ret);
            }
        string GetErrMsg(void) const
            {
                return "LoadAccVers("+NStr::SizetToString(m_Key.size())+": "+
                    m_Key[0].AsString()+", ...): "
                    "data not found";
            }
        CGBRequestStatistics::EStatType GetStatistics(void) const
            {
                return CGBRequestStatistics::eStat_Seq_idAcc;
            }
        string GetStatisticsDescription(void) const
            {
                return "accs("+NStr::SizetToString(m_Key.size())+": "+
                    m_Key[0].AsString()+", ...)";
            }
        
    private:
        const TKey& m_Key;
        TLoaded& m_Loaded;
        TRet& m_Ret;
    };

    class CCommandLoadGis : public CReadDispatcherCommand
    {
    public:
        typedef vector<CSeq_id_Handle> TKey;
        typedef vector<bool> TLoaded;
        typedef vector<TGi> TRet;
        CCommandLoadGis(CReaderRequestResult& result,
                        const TKey& key, TLoaded& loaded, TRet& ret)
            : CReadDispatcherCommand(result),
              m_Key(key), m_Loaded(loaded), m_Ret(ret)
            {
            }

        bool IsDone(void)
            {
                return std::find(m_Loaded.begin(), m_Loaded.end(), false) ==
                    m_Loaded.end();
            }
        bool Execute(CReader& reader)
            {
                return reader.LoadGis(GetResult(), m_Key, m_Loaded, m_Ret);
            }
        string GetErrMsg(void) const
            {
                return "LoadGis("+NStr::SizetToString(m_Key.size())+": "+
                    m_Key[0].AsString()+", ...): "
                    "data not found";
            }
        CGBRequestStatistics::EStatType GetStatistics(void) const
            {
                return CGBRequestStatistics::eStat_Seq_idGi;
            }
        string GetStatisticsDescription(void) const
            {
                return "gis("+NStr::SizetToString(m_Key.size())+": "+
                    m_Key[0].AsString()+", ...)";
            }
        
    private:
        const TKey& m_Key;
        TLoaded& m_Loaded;
        TRet& m_Ret;
    };

    class CCommandLoadLabels : public CReadDispatcherCommand
    {
    public:
        typedef vector<CSeq_id_Handle> TKey;
        typedef vector<bool> TLoaded;
        typedef vector<string> TRet;
        CCommandLoadLabels(CReaderRequestResult& result,
                           const TKey& key, TLoaded& loaded, TRet& ret)
            : CReadDispatcherCommand(result),
              m_Key(key), m_Loaded(loaded), m_Ret(ret)
            {
            }

        bool IsDone(void)
            {
                return std::find(m_Loaded.begin(), m_Loaded.end(), false) ==
                    m_Loaded.end();
            }
        bool Execute(CReader& reader)
            {
                return reader.LoadLabels(GetResult(), m_Key, m_Loaded, m_Ret);
            }
        string GetErrMsg(void) const
            {
                return "LoadLabels("+NStr::SizetToString(m_Key.size())+": "+
                    m_Key[0].AsString()+", ...): "
                    "data not found";
            }
        CGBRequestStatistics::EStatType GetStatistics(void) const
            {
                return CGBRequestStatistics::eStat_Seq_idLabel;
            }
        string GetStatisticsDescription(void) const
            {
                return "labels("+NStr::SizetToString(m_Key.size())+": "+
                    m_Key[0].AsString()+", ...)";
            }
        
    private:
        const TKey& m_Key;
        TLoaded& m_Loaded;
        TRet& m_Ret;
    };

    class CCommandLoadTaxIds : public CReadDispatcherCommand
    {
    public:
        typedef vector<CSeq_id_Handle> TKey;
        typedef vector<bool> TLoaded;
        typedef vector<int> TRet;
        CCommandLoadTaxIds(CReaderRequestResult& result,
                           const TKey& key, TLoaded& loaded, TRet& ret)
            : CReadDispatcherCommand(result),
              m_Key(key), m_Loaded(loaded), m_Ret(ret)
            {
            }

        bool IsDone(void)
            {
                return std::find(m_Loaded.begin(), m_Loaded.end(), false) ==
                    m_Loaded.end();
            }
        bool Execute(CReader& reader)
            {
                return reader.LoadTaxIds(GetResult(), m_Key, m_Loaded, m_Ret);
            }
        string GetErrMsg(void) const
            {
                return "LoadTaxIds("+NStr::SizetToString(m_Key.size())+": "+
                    m_Key[0].AsString()+", ...): "
                    "data not found";
            }
        CGBRequestStatistics::EStatType GetStatistics(void) const
            {
                return CGBRequestStatistics::eStat_Seq_idTaxId;
            }
        string GetStatisticsDescription(void) const
            {
                return "taxids("+NStr::SizetToString(m_Key.size())+": "+
                    m_Key[0].AsString()+", ...)";
            }
        
    private:
        const TKey& m_Key;
        TLoaded& m_Loaded;
        TRet& m_Ret;
    };

    class CCommandLoadBlobVersion : public CReadDispatcherCommand
    {
    public:
        typedef CBlob_id TKey;
        typedef CLoadLockBlob TLock;
        CCommandLoadBlobVersion(CReaderRequestResult& result,
                                const TKey& key)
            : CReadDispatcherCommand(result),
              m_Key(key), m_Lock(result, key)
            {
            }

        bool IsDone(void)
            {
                return m_Lock.IsSetBlobVersion();
            }
        bool Execute(CReader& reader)
            {
                return reader.LoadBlobVersion(GetResult(), m_Key);
            }
        string GetErrMsg(void) const
            {
                return "LoadBlobVersion("+m_Key.ToString()+"): "
                    "data not found";
            }
        CGBRequestStatistics::EStatType GetStatistics(void) const
            {
                return CGBRequestStatistics::eStat_BlobVersion;
            }
        string GetStatisticsDescription(void) const
            {
                return "blob-version("+m_Key.ToString()+")";
            }
        
    private:
        TKey m_Key;
        TLock m_Lock;
    };

    bool s_AllBlobsAreLoaded(CReaderRequestResult& result,
                             const CLoadLockBlob_ids& blobs,
                             CReadDispatcher::TContentsMask mask,
                             const SAnnotSelector* sel)
    {
        _ASSERT(blobs.IsLoaded());

        CFixedBlob_ids blob_ids = blobs->GetBlob_ids();
        ITERATE ( CFixedBlob_ids, it, blob_ids ) {
            const CBlob_Info& info = *it;
            if ( info.Matches(mask, sel) ) {
                if ( !result.IsBlobLoaded(*info.GetBlob_id()) ) {
                    return false;
                }
            }
        }
        return true;
    }

    class CCommandLoadBlobs : public CReadDispatcherCommand
    {
    public:
        typedef CLoadLockBlob_ids TIds;
        typedef CReadDispatcher::TContentsMask TMask;
        CCommandLoadBlobs(CReaderRequestResult& result,
                          TIds ids, TMask mask, const SAnnotSelector* sel)
            : CReadDispatcherCommand(result),
              m_Ids(ids), m_Mask(mask), m_Selector(sel)
            {
            }

        bool IsDone(void)
            {
                return s_AllBlobsAreLoaded(GetResult(),
                                           m_Ids, m_Mask, m_Selector);
            }
        bool Execute(CReader& reader)
            {
                return reader.LoadBlobs(GetResult(),
                                        m_Ids, m_Mask, m_Selector);
            }
        string GetErrMsg(void) const
            {
                return "LoadBlobs(CLoadInfoBlob_ids): "
                    "data not found";
            }
        CGBRequestStatistics::EStatType GetStatistics(void) const
            {
                return CGBRequestStatistics::eStat_LoadBlob;
            }
        string GetStatisticsDescription(void) const
            {
                return "blobs(...)";
            }
        
    private:
        TIds m_Ids;
        TMask m_Mask;
        const SAnnotSelector* m_Selector;
    };
    class CCommandLoadSeq_idBlobs : public CReadDispatcherCommand
    {
    public:
        typedef CSeq_id_Handle TKey;
        typedef CLoadLockSeq_ids TSeqIds;
        typedef CLoadLockBlob_ids TIds;
        typedef CReadDispatcher::TContentsMask TMask;
        CCommandLoadSeq_idBlobs(CReaderRequestResult& result,
                                const TKey& key, TMask mask,
                                const SAnnotSelector* sel)
            : CReadDispatcherCommand(result),
              m_Key(key), m_SeqIds(result, key), m_Ids(result, key, sel),
              m_Mask(mask), m_Selector(sel)
            {
            }

        bool IsDone(void)
            {
                return s_Blob_idsLoaded(m_Ids, m_SeqIds) &&
                    s_AllBlobsAreLoaded(GetResult(),
                                        m_Ids, m_Mask, m_Selector);
            }
        bool Execute(CReader& reader)
            {
                return reader.LoadBlobs(GetResult(),
                                        m_Key, m_Mask, m_Selector);
            }
        string GetErrMsg(void) const
            {
                return "LoadBlobs("+m_Key.AsString()+"): "
                    "data not found";
            }
        CGBRequestStatistics::EStatType GetStatistics(void) const
            {
                return CGBRequestStatistics::eStat_LoadBlob;
            }
        string GetStatisticsDescription(void) const
            {
                return "blobs("+m_Key.AsString()+")";
            }
        
    private:
        TKey m_Key;
        TSeqIds m_SeqIds;
        TIds m_Ids;
        TMask m_Mask;
        const SAnnotSelector* m_Selector;
    };

    class CCommandLoadBlob : public CReadDispatcherCommand
    {
    public:
        typedef CBlob_id TKey;
        CCommandLoadBlob(CReaderRequestResult& result,
                         const TKey& key,
                         const CBlob_Info* blob_info = 0)
            : CReadDispatcherCommand(result),
              m_Key(key),
              m_BlobInfo(blob_info)
            {
            }

        bool IsDone(void)
            {
                return GetResult().IsBlobLoaded(m_Key);
            }
        bool Execute(CReader& reader)
            {
                if ( m_BlobInfo ) {
                    return reader.LoadBlob(GetResult(), *m_BlobInfo);
                }
                else {
                    return reader.LoadBlob(GetResult(), m_Key);
                }
            }
        string GetErrMsg(void) const
            {
                return "LoadBlob("+m_Key.ToString()+"): "
                    "data not found";
            }
        CGBRequestStatistics::EStatType GetStatistics(void) const
            {
                return CGBRequestStatistics::eStat_LoadBlob;
            }
        string GetStatisticsDescription(void) const
            {
                return "blob("+m_Key.ToString()+")";
            }
        
    private:
        TKey m_Key;
        const CBlob_Info* m_BlobInfo;
    };

    class CCommandLoadChunk : public CReadDispatcherCommand
    {
    public:
        typedef CBlob_id TKey;
        typedef CLoadLockBlob TLock;
        typedef int TChunkId;
        typedef CTSE_Chunk_Info TChunkInfo;
        CCommandLoadChunk(CReaderRequestResult& result,
                          const TKey& key,
                          TChunkId chunk_id)
            : CReadDispatcherCommand(result),
              m_Key(key), m_Lock(result, key),
              m_ChunkId(chunk_id),
              m_ChunkInfo(m_Lock->GetSplitInfo().GetChunk(chunk_id))
            {
            }

        bool IsDone(void)
            {
                return m_ChunkInfo.IsLoaded();
            }
        bool Execute(CReader& reader)
            {
                return reader.LoadChunk(GetResult(), m_Key, m_ChunkId);
            }
        string GetErrMsg(void) const
            {
                return "LoadChunk("+m_Key.ToString()+", "+
                    NStr::IntToString(m_ChunkId)+"): "
                    "data not found";
            }
        CGBRequestStatistics::EStatType GetStatistics(void) const
            {
                return CGBRequestStatistics::eStat_LoadChunk;
            }
        string GetStatisticsDescription(void) const
            {
                return "chunk("+m_Key.ToString()+"."+
                    NStr::IntToString(m_ChunkId)+")";
            }
        
    private:
        TKey m_Key;
        TLock m_Lock;
        TChunkId m_ChunkId;
        TChunkInfo& m_ChunkInfo;
    };

    class CCommandLoadChunks : public CReadDispatcherCommand
    {
    public:
        typedef CBlob_id TKey;
        typedef CLoadLockBlob TLock;
        typedef int TChunkId;
        typedef vector<TChunkId> TChunkIds;
        typedef CTSE_Chunk_Info TChunkInfo;
        typedef vector<CTSE_Chunk_Info*> TChunkInfos;
        CCommandLoadChunks(CReaderRequestResult& result,
                           const TKey& key,
                           const TChunkIds chunk_ids)
            : CReadDispatcherCommand(result),
              m_Key(key), m_Lock(result, key),
              m_ChunkIds(chunk_ids)
            {
                ITERATE(TChunkIds, it, m_ChunkIds) {
                    m_ChunkInfos.push_back(
                        &m_Lock->GetSplitInfo().GetChunk(*it));
                }
            }

        bool IsDone(void)
            {
                ITERATE(TChunkInfos, it, m_ChunkInfos) {
                    if ( !(*it)->IsLoaded() ) {
                        return false;
                    }
                }
                return true;
            }
        bool Execute(CReader& reader)
            {
                return reader.LoadChunks(GetResult(), m_Key, m_ChunkIds);
            }
        string GetErrMsg(void) const
            {
                CNcbiOstrstream str;
                str << "LoadChunks(" << m_Key.ToString() << ", {";
                int cnt = 0;
                ITERATE(TChunkInfos, it, m_ChunkInfos) {
                    if ( !(*it)->IsLoaded() ) {
                        if ( cnt++ ) str << ',';
                        str << ' ' << (*it)->GetChunkId();
                    }
                }
                str << " }): data not found";
                return CNcbiOstrstreamToString(str);
            }
        CGBRequestStatistics::EStatType GetStatistics(void) const
            {
                return CGBRequestStatistics::eStat_LoadChunk;
            }
        string GetStatisticsDescription(void) const
            {
                CNcbiOstrstream str;
                int cnt = 0;
                ITERATE(TChunkInfos, it, m_ChunkInfos) {
                    int id = (*it)->GetChunkId();
                    if ( id >= 0 && id < kMax_Int ) {
                        if ( !cnt ) {
                            str << "chunk(" << m_Key.ToString() << '.';
                            cnt = 1;
                        }
                        else {
                            str << ',';
                        }
                        str << id;
                    }
                }
                if ( !cnt ) {
                    str << "blob(" << m_Key.ToString();
                }
                str << ')';
                return CNcbiOstrstreamToString(str);
            }
        
    private:
        TKey m_Key;
        TLock m_Lock;
        TChunkIds m_ChunkIds;
        TChunkInfos m_ChunkInfos;
    };

    class CCommandLoadBlobSet : public CReadDispatcherCommand
    {
    public:
        typedef CReadDispatcher::TIds TIds;
        CCommandLoadBlobSet(CReaderRequestResult& result,
                            const TIds& seq_ids)
            : CReadDispatcherCommand(result),
              m_Ids(seq_ids)
            {
            }

        bool IsDone(void)
            {
                CReaderRequestResult& result = GetResult();
                ITERATE(TIds, id, m_Ids) {
                    CLoadLockSeq_ids seq_ids(result, *id);
                    CLoadLockBlob_ids blob_ids(result, *id, 0);
                    if ( !s_Blob_idsLoaded(blob_ids, seq_ids) ) {
                        return false;
                    }
                    CFixedBlob_ids ids = blob_ids->GetBlob_ids();
                    ITERATE ( CFixedBlob_ids, it, ids ) {
                        if ( it->Matches(fBlobHasCore, 0) ) {
                            if ( !result.IsBlobLoaded(*it->GetBlob_id()) ) {
                                return false;
                            }
                        }
                    }
                }
                return true;
            }
        bool Execute(CReader& reader)
            {
                return reader.LoadBlobSet(GetResult(), m_Ids);
            }
        string GetErrMsg(void) const
            {
                return "LoadBlobSet(" +
                    NStr::SizetToString(m_Ids.size()) + " ids): "
                    "data not found";
            }
        CGBRequestStatistics::EStatType GetStatistics(void) const
            {
                return CGBRequestStatistics::eStat_LoadBlob;
            }
        string GetStatisticsDescription(void) const
            {
                return "blobs(" +
                    NStr::SizetToString(m_Ids.size()) + " ids)";
            }
        
    private:
        TIds    m_Ids;
    };
}

BEGIN_LOCAL_NAMESPACE;

struct SSaveResultLevel
{
    SSaveResultLevel(CReadDispatcherCommand& command)
        : m_Command(command),
          m_SavedLevel(command.GetResult().GetLevel())
        {
        }
    
    ~SSaveResultLevel(void)
        {
            m_Command.GetResult().SetLevel(m_SavedLevel);
        }
    
    CReadDispatcherCommand& m_Command;
    CReadDispatcher::TLevel m_SavedLevel;
};

END_LOCAL_NAMESPACE;


void CReadDispatcher::Process(CReadDispatcherCommand& command,
                              const CReader* asking_reader)
{
    CheckReaders();

    if ( command.IsDone() ) {
        return;
    }

    SSaveResultLevel save_level(command);
    NON_CONST_ITERATE ( TReaders, rdr, m_Readers ) {
        if ( asking_reader ) {
            // skip all readers before the asking one
            if ( rdr->second == asking_reader ) {
                // found the asking reader, start processing next readers
                asking_reader = 0;
            }
            continue;
        }
        CReader& reader = *rdr->second;
        command.GetResult().SetLevel(rdr->first);
        int retry_count = 0;
        int max_retry_count = reader.GetRetryCount();
        do {
            ++retry_count;
            try {
                CReaderRequestResult::CRecurse r(command.GetResult());
                if ( !command.Execute(reader) ) {
                    retry_count = kMax_Int;
                }
                LogStat(command, r);
            }
            catch ( CLoaderException& exc ) {
                if ( exc.GetErrCode() == exc.eRepeatAgain ) {
                    // no actual error, just restart
                    --retry_count;
                    LOG_POST_X(10, Info<<
                               "CReadDispatcher: connection reopened "
                               "due to inactivity timeout");
                }
                else if ( exc.GetErrCode() == exc.eNoConnection ) {
                    LOG_POST_X(1, Warning<<
                               "CReadDispatcher: Exception: "<<exc);
                    retry_count = kMax_Int;
                }
                else {
                    if ( retry_count >= max_retry_count &&
                         !command.MayBeSkipped() &&
                         !reader.MayBeSkippedOnErrors() ) {
                        throw;
                    }
                    LOG_POST_X(2, Warning<<
                               "CReadDispatcher: Exception: "<<exc);
                }
            }
            catch ( CException& exc ) {
                // error in the command
                if ( retry_count >= max_retry_count &&
                     !command.MayBeSkipped() &&
                     !reader.MayBeSkippedOnErrors() ) {
                    throw;
                }
                LOG_POST_X(3, Warning <<
                           "CReadDispatcher: Exception: "<<exc);
            }
            catch ( exception& exc ) {
                // error in the command
                if ( retry_count >= max_retry_count &&
                     !command.MayBeSkipped() &&
                     !reader.MayBeSkippedOnErrors() ) {
                    throw;
                }
                LOG_POST_X(4, Warning <<
                           "CReadDispatcher: Exception: "<<exc.what());
            }
            if ( command.IsDone() ) {
                return;
            }
        } while ( retry_count < max_retry_count );
        if ( !command.MayBeSkipped() &&
             !reader.MayBeSkippedOnErrors() ) {
            NCBI_THROW(CLoaderException, eLoaderFailed, command.GetErrMsg());
        }
    }

    if ( command.MayBeSkipped() ) {
        return;
    }

    NCBI_THROW(CLoaderException, eLoaderFailed, command.GetErrMsg());
}


void CReadDispatcher::LoadStringSeq_ids(CReaderRequestResult& result,
                                        const string& seq_id)
{
    CCommandLoadStringSeq_ids command(result, seq_id);
    Process(command);
}


void CReadDispatcher::LoadSeq_idSeq_ids(CReaderRequestResult& result,
                                        const CSeq_id_Handle& seq_id)
{
    CCommandLoadSeq_idSeq_ids command(result, seq_id);
    Process(command);
}


void CReadDispatcher::LoadSeq_idGi(CReaderRequestResult& result,
                                   const CSeq_id_Handle& seq_id)
{
    CCommandLoadSeq_idGi command(result, seq_id);
    Process(command);
}


void CReadDispatcher::LoadSeq_idAccVer(CReaderRequestResult& result,
                                    const CSeq_id_Handle& seq_id)
{
    CCommandLoadSeq_idAccVer command(result, seq_id);
    Process(command);
}


void CReadDispatcher::LoadSeq_idLabel(CReaderRequestResult& result,
                                      const CSeq_id_Handle& seq_id)
{
    CCommandLoadSeq_idLabel command(result, seq_id);
    Process(command);
}


void CReadDispatcher::LoadSeq_idTaxId(CReaderRequestResult& result,
                                      const CSeq_id_Handle& seq_id)
{
    CCommandLoadSeq_idTaxId command(result, seq_id);
    Process(command);
}


void CReadDispatcher::LoadAccVers(CReaderRequestResult& result,
                                  const TIds ids, TLoaded& loaded, TIds& ret)
{
    CCommandLoadAccVers command(result, ids, loaded, ret);
    Process(command);
}


void CReadDispatcher::LoadGis(CReaderRequestResult& result,
                              const TIds ids, TLoaded& loaded, TGis& ret)
{
    CCommandLoadGis command(result, ids, loaded, ret);
    Process(command);
}


void CReadDispatcher::LoadLabels(CReaderRequestResult& result,
                                 const TIds ids, TLoaded& loaded, TLabels& ret)
{
    CCommandLoadLabels command(result, ids, loaded, ret);
    Process(command);
}


void CReadDispatcher::LoadTaxIds(CReaderRequestResult& result,
                                 const TIds ids, TLoaded& loaded, TTaxIds& ret)
{
    CCommandLoadTaxIds command(result, ids, loaded, ret);
    Process(command);
}


void CReadDispatcher::LoadSeq_idBlob_ids(CReaderRequestResult& result,
                                         const CSeq_id_Handle& seq_id,
                                         const SAnnotSelector* sel)
{
    CCommandLoadSeq_idBlob_ids command(result, seq_id, sel);
    Process(command);
}


void CReadDispatcher::LoadBlobVersion(CReaderRequestResult& result,
                                      const TBlobId& blob_id,
                                      const CReader* asking_reader)
{
    CCommandLoadBlobVersion command(result, blob_id);
    Process(command, asking_reader);
}

void CReadDispatcher::LoadBlobs(CReaderRequestResult& result,
                                const CSeq_id_Handle& seq_id,
                                TContentsMask mask,
                                const SAnnotSelector* sel)
{
    CCommandLoadSeq_idBlobs command(result, seq_id, mask, sel);
    Process(command);
}


void CReadDispatcher::LoadBlobs(CReaderRequestResult& result,
                                CLoadLockBlob_ids blobs,
                                TContentsMask mask,
                                const SAnnotSelector* sel)
{
    CCommandLoadBlobs command(result, blobs, mask, sel);
    Process(command);
}


void CReadDispatcher::LoadBlob(CReaderRequestResult& result,
                               const CBlob_id& blob_id)
{
    CCommandLoadBlob command(result, blob_id);
    Process(command);
}


void CReadDispatcher::LoadBlob(CReaderRequestResult& result,
                               const CBlob_Info& blob_info)
{
    CCommandLoadBlob command(result, *blob_info.GetBlob_id(), &blob_info);
    Process(command);
}


void CReadDispatcher::LoadChunk(CReaderRequestResult& result,
                                const TBlobId& blob_id, TChunkId chunk_id)
{
    CCommandLoadChunk command(result, blob_id, chunk_id);
    Process(command);
}


void CReadDispatcher::LoadChunks(CReaderRequestResult& result,
                                 const TBlobId& blob_id,
                                 const TChunkIds& chunk_ids)
{
    CCommandLoadChunks command(result, blob_id, chunk_ids);
    Process(command);
}


void CReadDispatcher::LoadBlobSet(CReaderRequestResult& result,
                                  const TIds& seq_ids)
{
    CCommandLoadBlobSet command(result, seq_ids);
    Process(command);
}


void CReadDispatcher::SetAndSaveBlobState(CReaderRequestResult& result,
                                          const TBlobId& blob_id,
                                          TBlobState state) const
{
    CLoadLockBlob blob(result, blob_id);
    SetAndSaveBlobState(result, blob_id, blob, state);
}


void CReadDispatcher::SetAndSaveBlobState(CReaderRequestResult& /*result*/,
                                          const TBlobId& /*blob_id*/,
                                          CLoadLockBlob& blob,
                                          TBlobState state) const
{
    if ( (blob.GetBlobState() & state) == state ) {
        return;
    }
    blob.SetBlobState(state);
}


void CReadDispatcher::SetAndSaveBlobVersion(CReaderRequestResult& result,
                                            const TBlobId& blob_id,
                                            TBlobVersion version) const
{
    CLoadLockBlob blob(result, blob_id);
    SetAndSaveBlobVersion(result, blob_id, blob, version);
}


void CReadDispatcher::SetAndSaveBlobVersion(CReaderRequestResult& result,
                                            const TBlobId& blob_id,
                                            CLoadLockBlob& blob,
                                            TBlobVersion version) const
{
    if ( blob.IsSetBlobVersion() && blob.GetBlobVersion() == version ) {
        return;
    }
    blob.SetBlobVersion(version);
    CWriter *writer = GetWriter(result, CWriter::eIdWriter);
    if( writer ) {
        writer->SaveBlobVersion(result, blob_id, version);
    }
}


void CReadDispatcher::LogStat(CReadDispatcherCommand& command,
                              CStopWatch& sw)
{
    CReaderRequestResult& result = command.GetResult();
    double time = result.GetCurrentRequestTime(sw.Elapsed());
    CGBRequestStatistics& stat = sx_Statistics[command.GetStatistics()];
    stat.AddTime(time);
    if ( CollectStatistics() >= 2 ) {
        string descr = command.GetStatisticsDescription();
        const CSeq_id_Handle& idh = result.GetRequestedId();
        if ( idh ) {
            descr = descr + " for " + idh.AsString();
        }
        LOG_POST_X(8, setw(result.GetRecursionLevel()) << "" <<
                   "Dispatcher: read " <<
                   descr << " in " <<
                   setiosflags(ios::fixed) <<
                   setprecision(3) << (time*1000) << " ms");
    }
}


void CReadDispatcher::LogStat(CReadDispatcherCommand& command,
                              CStopWatch& sw,
                              double size)
{
    CReaderRequestResult& result = command.GetResult();
    double time = result.GetCurrentRequestTime(sw.Elapsed());
    CGBRequestStatistics& stat = sx_Statistics[command.GetStatistics()];
    stat.AddTimeSize(time, size);
    if ( CollectStatistics() >= 2 ) {
        string descr = command.GetStatisticsDescription();
        const CSeq_id_Handle& idh = result.GetRequestedId();
        if ( idh ) {
            descr = descr + " for " + idh.AsString();
        }
        LOG_POST_X(9, setw(result.GetRecursionLevel()) << "" <<
                   descr << " in " <<
                   setiosflags(ios::fixed) <<
                   setprecision(3) <<
                   (time*1000) << " ms (" <<
                   setprecision(2) <<
                   (size/1024.0) << " kB " <<
                   setprecision(2) <<
                   (size/time/1024) << " kB/s)");
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE
