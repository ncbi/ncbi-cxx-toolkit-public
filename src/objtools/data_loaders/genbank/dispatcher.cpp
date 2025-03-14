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
#include <objtools/data_loaders/genbank/impl/dispatcher.hpp>
#include <objtools/data_loaders/genbank/reader.hpp>
#include <objtools/data_loaders/genbank/writer.hpp>
#include <objtools/data_loaders/genbank/impl/processor.hpp>
#include <objtools/data_loaders/genbank/impl/request_result.hpp>
#include <objtools/data_loaders/genbank/impl/statistics.hpp>
#include <objtools/error_codes.hpp>
#include <objmgr/objmgr_exception.hpp>
#include <objmgr/impl/tse_split_info.hpp>
#include <objmgr/impl/tse_chunk_info.hpp>
#include <objects/general/Dbtag.hpp>


#define NCBI_USE_ERRCODE_X   Objtools_Rd_Disp

BEGIN_NCBI_SCOPE

NCBI_DEFINE_ERR_SUBCODE_X(10);

BEGIN_SCOPE(objects)

NCBI_PARAM_DECL(bool, GENBANK, ALLOW_INCOMPLETE_COMMANDS);
NCBI_PARAM_DEF_EX(bool, GENBANK, ALLOW_INCOMPLETE_COMMANDS, false,
                  eParam_NoThread, GENBANK_ALLOW_INCOMPLETE_COMMANDS);

static 
bool s_AllowIncompleteCommands(void)
{
    static CSafeStatic<NCBI_PARAM_TYPE(GENBANK, ALLOW_INCOMPLETE_COMMANDS)> s_Value;
    return s_Value->Get();
}


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
    CGBRequestStatistics("resolved", "blob state"),
    CGBRequestStatistics("resolved", "blob versions"),
    CGBRequestStatistics("loaded", "blob data"),
    CGBRequestStatistics("loaded", "SNP data"),
    CGBRequestStatistics("loaded", "split data"),
    CGBRequestStatistics("loaded", "chunk data"),
    CGBRequestStatistics("parsed", "blob data"),
    CGBRequestStatistics("parsed", "SNP data"),
    CGBRequestStatistics("parsed", "split data"),
    CGBRequestStatistics("parsed", "chunk data"),
    CGBRequestStatistics("attached", "blob data"),
    CGBRequestStatistics("attached", "SNP data"),
    CGBRequestStatistics("attached", "split data"),
    CGBRequestStatistics("attached", "chunk data"),
    CGBRequestStatistics("loaded", "sequence hash"),
    CGBRequestStatistics("loaded", "sequence length"),
    CGBRequestStatistics("loaded", "sequence type")
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
    static CSafeStatic<NCBI_PARAM_TYPE(GENBANK, READER_STATS)> s_Value;
    return s_Value->Get();
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


bool CReadDispatcher::HasReaderWithHUPIncluded() const
{
    for ( auto& rd : m_Readers ) {
        if ( rd.second->HasHUPIncluded() ) {
            return true;
        }
    }
    return false;
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


bool CReadDispatcher::CannotProcess(const CSeq_id_Handle& sih)
{
    return !sih || sih.Which() == CSeq_id::e_Local ||
        (sih.Which() == CSeq_id::e_General &&
         NStr::EqualNocase(sih.GetSeqId()->GetGeneral().GetDb(), "SRA"));
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


size_t CReadDispatcherCommand::GetStatisticsCount(void) const
{
    return 1;
}


namespace {
    class CCommandLoadSeq_idSeq_ids : public CReadDispatcherCommand
    {
    public:
        typedef CSeq_id_Handle TKey;
        typedef CLoadLockSeqIds TLock;
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
        typedef CLoadLockGi TLock;
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
        typedef CLoadLockAcc TLock;
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
        typedef CLoadLockLabel TLock;
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
        typedef CLoadLockTaxId TLock;
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

    class CCommandLoadSequenceHash : public CReadDispatcherCommand
    {
    public:
        typedef CSeq_id_Handle TKey;
        typedef CLoadLockHash TLock;
        CCommandLoadSequenceHash(CReaderRequestResult& result,
                                 const TKey& key)
            : CReadDispatcherCommand(result),
              m_Key(key), m_Lock(result, key)
            {
            }

        bool IsDone(void)
            {
                return m_Lock.IsLoadedHash();
            }
        bool Execute(CReader& reader)
            {
                return reader.LoadSequenceHash(GetResult(), m_Key);
            }
        bool MayBeSkipped(void) const
            {
                return true;
            }
        string GetErrMsg(void) const
            {
                return "LoadSequenceHash("+m_Key.AsString()+"): "
                    "data not found";
            }
        CGBRequestStatistics::EStatType GetStatistics(void) const
            {
                return CGBRequestStatistics::eStat_Hash;
            }
        string GetStatisticsDescription(void) const
            {
                return "hash("+m_Key.AsString()+")";
            }
        
    private:
        TKey m_Key;
        TLock m_Lock;
    };

    class CCommandLoadSequenceLength : public CReadDispatcherCommand
    {
    public:
        typedef CSeq_id_Handle TKey;
        typedef CLoadLockLength TLock;
        CCommandLoadSequenceLength(CReaderRequestResult& result,
                                 const TKey& key)
            : CReadDispatcherCommand(result),
              m_Key(key), m_Lock(result, key)
            {
            }

        bool IsDone(void)
            {
                return m_Lock.IsLoadedLength();
            }
        bool Execute(CReader& reader)
            {
                return reader.LoadSequenceLength(GetResult(), m_Key);
            }
        bool MayBeSkipped(void) const
            {
                return true;
            }
        string GetErrMsg(void) const
            {
                return "LoadSequenceLength("+m_Key.AsString()+"): "
                    "data not found";
            }
        CGBRequestStatistics::EStatType GetStatistics(void) const
            {
                return CGBRequestStatistics::eStat_Length;
            }
        string GetStatisticsDescription(void) const
            {
                return "length("+m_Key.AsString()+")";
            }
        
    private:
        TKey m_Key;
        TLock m_Lock;
    };

    class CCommandLoadSequenceType : public CReadDispatcherCommand
    {
    public:
        typedef CSeq_id_Handle TKey;
        typedef CLoadLockType TLock;
        CCommandLoadSequenceType(CReaderRequestResult& result,
                                 const TKey& key)
            : CReadDispatcherCommand(result),
              m_Key(key), m_Lock(result, key)
            {
            }

        bool IsDone(void)
            {
                return m_Lock.IsLoadedType();
            }
        bool Execute(CReader& reader)
            {
                return reader.LoadSequenceType(GetResult(), m_Key);
            }
        bool MayBeSkipped(void) const
            {
                return true;
            }
        string GetErrMsg(void) const
            {
                return "LoadSequenceType("+m_Key.AsString()+"): "
                    "data not found";
            }
        CGBRequestStatistics::EStatType GetStatistics(void) const
            {
                return CGBRequestStatistics::eStat_Type;
            }
        string GetStatisticsDescription(void) const
            {
                return "type("+m_Key.AsString()+")";
            }
        
    private:
        TKey m_Key;
        TLock m_Lock;
    };

    bool s_Blob_idsLoaded(CLoadLockBlobIds& ids,
                          CReaderRequestResult& result,
                          const CSeq_id_Handle& seq_id)
    {
        if ( ids.IsLoaded() ) {
            return true;
        }
        // check if seq-id is known as absent
        CLoadLockSeqIds seq_ids(result, seq_id, eAlreadyLoaded);
        if ( seq_ids && !seq_ids.GetData().IsFound() ) {
            // mark blob-ids as absent too
            ids.SetNoBlob_ids(seq_ids.GetState());
            return true;
        }
        return false;
    }

    class CCommandLoadSeq_idBlob_ids : public CReadDispatcherCommand
    {
    public:
        typedef CSeq_id_Handle TKey;
        typedef CLoadLockBlobIds TLock;
        CCommandLoadSeq_idBlob_ids(CReaderRequestResult& result,
                                   const TKey& key,
                                   const SAnnotSelector* sel)
            : CReadDispatcherCommand(result),
              m_Key(key), m_Selector(sel),
              m_Lock(result, key, sel)
            {
            }

        bool IsDone(void)
            {
                return s_Blob_idsLoaded(m_Lock, GetResult(), m_Key);
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
        TLock m_Lock;
    };

    template<class CLoadLock>
    bool sx_IsLoaded(size_t i,
                     CReaderRequestResult& result,
                     const vector<CSeq_id_Handle>& ids,
                     const vector<bool>& loaded)
    {
        if ( loaded[i] || CReadDispatcher::CannotProcess(ids[i]) ) {
            return true;
        }
        CLoadLock lock(result, ids[i]);
        if ( lock.IsLoaded() && !lock.IsFound() ) {
            return true;
        }
        return false;
    }

    template<class CLoadLock>
    bool sx_BulkIsDone(CReaderRequestResult& result,
                       const vector<CSeq_id_Handle>& ids,
                       const vector<bool>& loaded)
    {
        for ( size_t i = 0; i < ids.size(); ++i ) {
            if ( sx_IsLoaded<CLoadLock>(i, result, ids, loaded) ) {
                continue;
            }
            return false;
        }
        return true;
    }

    template<class CLoadLock>
    string sx_DescribeError(CReaderRequestResult& result,
                            const vector<CSeq_id_Handle>& ids,
                            const vector<bool>& loaded)
    {
        string ret;
        for ( size_t i = 0; i < ids.size(); ++i ) {
            if ( sx_IsLoaded<CLoadLock>(i, result, ids, loaded) ) {
                continue;
            }
            if ( !ret.empty() ) {
                ret += ", ";
            }
            ret += ids[i].AsString();
        }
        ret += " ["+NStr::SizetToString(ids.size())+"]";
        return ret;
    }

    class CCommandLoadBulkIds : public CReadDispatcherCommand
    {
    public:
        typedef vector<CSeq_id_Handle> TKey;
        typedef vector<bool> TLoaded;
        typedef vector<CSeq_id_Handle> TIds;
        typedef vector<TIds> TRet;
        typedef CLoadLockSeqIds CLoadLock;
        CCommandLoadBulkIds(CReaderRequestResult& result,
                            const TKey& key, TLoaded& loaded, TRet& ret)
            : CReadDispatcherCommand(result),
              m_Key(key), m_Loaded(loaded), m_Ret(ret)
            {
            }

        bool Execute(CReader& reader)
            {
                return reader.LoadBulkIds(GetResult(), m_Key, m_Loaded, m_Ret);
            }
        bool IsDone(void)
            {
                return sx_BulkIsDone<CLoadLock>(GetResult(), m_Key, m_Loaded);
            }
        string GetErrMsg(void) const
            {
                return "LoadBulkIds("+
                    sx_DescribeError<CLoadLock>(GetResult(), m_Key, m_Loaded)+
                    "): data not found";
            }
        string GetStatisticsDescription(void) const
            {
                return "bulkids("+
                    sx_DescribeError<CLoadLock>(GetResult(), m_Key, m_Loaded)+
                    ")";
            }
        CGBRequestStatistics::EStatType GetStatistics(void) const
            {
                return CGBRequestStatistics::eStat_Seq_idSeq_ids;
            }
        size_t GetStatisticsCount(void) const
            {
                return m_Key.size();
            }
        
    private:
        const TKey& m_Key;
        TLoaded& m_Loaded;
        TRet& m_Ret;
    };

    class CCommandLoadAccVers : public CReadDispatcherCommand
    {
    public:
        typedef vector<CSeq_id_Handle> TKey;
        typedef vector<bool> TLoaded;
        typedef vector<CSeq_id_Handle> TRet;
        typedef CLoadLockAcc CLoadLock;
        CCommandLoadAccVers(CReaderRequestResult& result,
                            const TKey& key, TLoaded& loaded, TRet& ret)
            : CReadDispatcherCommand(result),
              m_Key(key), m_Loaded(loaded), m_Ret(ret)
            {
            }

        bool Execute(CReader& reader)
            {
                return reader.LoadAccVers(GetResult(), m_Key, m_Loaded, m_Ret);
            }
        bool IsDone(void)
            {
                return sx_BulkIsDone<CLoadLock>(GetResult(), m_Key, m_Loaded);
            }
        string GetErrMsg(void) const
            {
                return "LoadAccVers("+
                    sx_DescribeError<CLoadLock>(GetResult(), m_Key, m_Loaded)+
                    "): data not found";
            }
        string GetStatisticsDescription(void) const
            {
                return "accs("+
                    sx_DescribeError<CLoadLock>(GetResult(), m_Key, m_Loaded)+
                    ")";
            }
        CGBRequestStatistics::EStatType GetStatistics(void) const
            {
                return CGBRequestStatistics::eStat_Seq_idAcc;
            }
        size_t GetStatisticsCount(void) const
            {
                return m_Key.size();
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
        typedef CLoadLockGi CLoadLock;
        CCommandLoadGis(CReaderRequestResult& result,
                        const TKey& key, TLoaded& loaded, TRet& ret)
            : CReadDispatcherCommand(result),
              m_Key(key), m_Loaded(loaded), m_Ret(ret)
            {
            }

        bool Execute(CReader& reader)
            {
                return reader.LoadGis(GetResult(), m_Key, m_Loaded, m_Ret);
            }
        bool IsDone(void)
            {
                return sx_BulkIsDone<CLoadLock>(GetResult(), m_Key, m_Loaded);
            }
        string GetErrMsg(void) const
            {
                return "LoadGis("+
                    sx_DescribeError<CLoadLock>(GetResult(), m_Key, m_Loaded)+
                    "): data not found";
            }
        string GetStatisticsDescription(void) const
            {
                return "gis("+
                    sx_DescribeError<CLoadLock>(GetResult(), m_Key, m_Loaded)+
                    ")";
            }
        CGBRequestStatistics::EStatType GetStatistics(void) const
            {
                return CGBRequestStatistics::eStat_Seq_idGi;
            }
        size_t GetStatisticsCount(void) const
            {
                return m_Key.size();
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
        typedef CLoadLockLabel CLoadLock;
        CCommandLoadLabels(CReaderRequestResult& result,
                           const TKey& key, TLoaded& loaded, TRet& ret)
            : CReadDispatcherCommand(result),
              m_Key(key), m_Loaded(loaded), m_Ret(ret)
            {
            }

        bool Execute(CReader& reader)
            {
                return reader.LoadLabels(GetResult(), m_Key, m_Loaded, m_Ret);
            }
        bool IsDone(void)
            {
                return sx_BulkIsDone<CLoadLock>(GetResult(), m_Key, m_Loaded);
            }
        string GetErrMsg(void) const
            {
                return "LoadLabels("+
                    sx_DescribeError<CLoadLock>(GetResult(), m_Key, m_Loaded)+
                    "): data not found";
            }
        string GetStatisticsDescription(void) const
            {
                return "labels("+
                    sx_DescribeError<CLoadLock>(GetResult(), m_Key, m_Loaded)+
                    ")";
            }
        CGBRequestStatistics::EStatType GetStatistics(void) const
            {
                return CGBRequestStatistics::eStat_Seq_idLabel;
            }
        size_t GetStatisticsCount(void) const
            {
                return m_Key.size();
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
        typedef vector<TTaxId> TRet;
        typedef CLoadLockTaxId CLoadLock;
        CCommandLoadTaxIds(CReaderRequestResult& result,
                           const TKey& key, TLoaded& loaded, TRet& ret)
            : CReadDispatcherCommand(result),
              m_Key(key), m_Loaded(loaded), m_Ret(ret)
            {
            }

        bool Execute(CReader& reader)
            {
                return reader.LoadTaxIds(GetResult(), m_Key, m_Loaded, m_Ret);
            }
        bool MayBeSkipped(void) const
            {
                return true;
            }
        bool IsDone(void)
            {
                return sx_BulkIsDone<CLoadLock>(GetResult(), m_Key, m_Loaded);
            }
        string GetErrMsg(void) const
            {
                return "LoadTaxIds("+
                    sx_DescribeError<CLoadLock>(GetResult(), m_Key, m_Loaded)+
                    "): data not found";
            }
        string GetStatisticsDescription(void) const
            {
                return "taxids("+
                    sx_DescribeError<CLoadLock>(GetResult(), m_Key, m_Loaded)+
                    ")";
            }
        CGBRequestStatistics::EStatType GetStatistics(void) const
            {
                return CGBRequestStatistics::eStat_Seq_idTaxId;
            }
        size_t GetStatisticsCount(void) const
            {
                return m_Key.size();
            }
        
    private:
        const TKey& m_Key;
        TLoaded& m_Loaded;
        TRet& m_Ret;
    };

    class CCommandLoadHashes : public CReadDispatcherCommand
    {
    public:
        typedef vector<CSeq_id_Handle> TKey;
        typedef vector<bool> TLoaded;
        typedef vector<bool> TKnown;
        typedef vector<int> TRet;
        typedef CLoadLockHash CLoadLock;
        CCommandLoadHashes(CReaderRequestResult& result,
                           const TKey& key, TLoaded& loaded,
                           TRet& ret, TKnown& known)
            : CReadDispatcherCommand(result),
              m_Key(key), m_Loaded(loaded), m_Ret(ret), m_Known(known)
            {
            }

        bool Execute(CReader& reader)
            {
                return reader.LoadHashes(GetResult(), m_Key, m_Loaded,
                                         m_Ret, m_Known);
            }
        bool MayBeSkipped(void) const
            {
                return true;
            }
        bool IsDone(void)
            {
                return sx_BulkIsDone<CLoadLock>(GetResult(), m_Key, m_Loaded);
            }
        string GetErrMsg(void) const
            {
                return "LoadHashes("+
                    sx_DescribeError<CLoadLock>(GetResult(), m_Key, m_Loaded)+
                    "): data not found";
            }
        string GetStatisticsDescription(void) const
            {
                return "hashes("+
                    sx_DescribeError<CLoadLock>(GetResult(), m_Key, m_Loaded)+
                    ")";
            }
        CGBRequestStatistics::EStatType GetStatistics(void) const
            {
                return CGBRequestStatistics::eStat_Hash;
            }
        size_t GetStatisticsCount(void) const
            {
                return m_Key.size();
            }
        
    private:
        const TKey& m_Key;
        TLoaded& m_Loaded;
        TRet& m_Ret;
        TKnown& m_Known;
    };

    class CCommandLoadLengths : public CReadDispatcherCommand
    {
    public:
        typedef vector<CSeq_id_Handle> TKey;
        typedef vector<bool> TLoaded;
        typedef vector<TSeqPos> TRet;
        typedef CLoadLockLength CLoadLock;
        CCommandLoadLengths(CReaderRequestResult& result,
                           const TKey& key, TLoaded& loaded, TRet& ret)
            : CReadDispatcherCommand(result),
              m_Key(key), m_Loaded(loaded), m_Ret(ret)
            {
            }

        bool Execute(CReader& reader)
            {
                return reader.LoadLengths(GetResult(), m_Key, m_Loaded, m_Ret);
            }
        bool MayBeSkipped(void) const
            {
                return true;
            }
        bool IsDone(void)
            {
                return sx_BulkIsDone<CLoadLock>(GetResult(), m_Key, m_Loaded);
            }
        string GetErrMsg(void) const
            {
                return "LoadLengths("+
                    sx_DescribeError<CLoadLock>(GetResult(), m_Key, m_Loaded)+
                    "): data not found";
            }
        string GetStatisticsDescription(void) const
            {
                return "lengths("+
                    sx_DescribeError<CLoadLock>(GetResult(), m_Key, m_Loaded)+
                    ")";
            }
        CGBRequestStatistics::EStatType GetStatistics(void) const
            {
                return CGBRequestStatistics::eStat_Length;
            }
        size_t GetStatisticsCount(void) const
            {
                return m_Key.size();
            }
        
    private:
        const TKey& m_Key;
        TLoaded& m_Loaded;
        TRet& m_Ret;
    };

    class CCommandLoadTypes : public CReadDispatcherCommand
    {
    public:
        typedef vector<CSeq_id_Handle> TKey;
        typedef vector<bool> TLoaded;
        typedef vector<CSeq_inst::EMol> TRet;
        typedef CLoadLockType CLoadLock;
        CCommandLoadTypes(CReaderRequestResult& result,
                           const TKey& key, TLoaded& loaded, TRet& ret)
            : CReadDispatcherCommand(result),
              m_Key(key), m_Loaded(loaded), m_Ret(ret)
            {
            }

        bool Execute(CReader& reader)
            {
                return reader.LoadTypes(GetResult(), m_Key, m_Loaded, m_Ret);
            }
        bool MayBeSkipped(void) const
            {
                return true;
            }
        bool IsDone(void)
            {
                return sx_BulkIsDone<CLoadLock>(GetResult(), m_Key, m_Loaded);
            }
        string GetErrMsg(void) const
            {
                return "LoadTypes("+
                    sx_DescribeError<CLoadLock>(GetResult(), m_Key, m_Loaded)+
                    "): data not found";
            }
        string GetStatisticsDescription(void) const
            {
                return "types("+
                    sx_DescribeError<CLoadLock>(GetResult(), m_Key, m_Loaded)+
                    ")";
            }
        CGBRequestStatistics::EStatType GetStatistics(void) const
            {
                return CGBRequestStatistics::eStat_Type;
            }
        size_t GetStatisticsCount(void) const
            {
                return m_Key.size();
            }
        
    private:
        const TKey& m_Key;
        TLoaded& m_Loaded;
        TRet& m_Ret;
    };

    class CCommandLoadStates : public CReadDispatcherCommand
    {
    public:
        typedef vector<CSeq_id_Handle> TKey;
        typedef vector<bool> TLoaded;
        typedef vector<int> TRet;
        typedef CLoadLockBlobIds CLoadLock;
        CCommandLoadStates(CReaderRequestResult& result,
                           const TKey& key, TLoaded& loaded, TRet& ret)
            : CReadDispatcherCommand(result),
              m_Key(key), m_Loaded(loaded), m_Ret(ret)
            {
            }

        bool Execute(CReader& reader)
            {
                return reader.LoadStates(GetResult(), m_Key, m_Loaded, m_Ret);
            }
        bool IsDone(void)
            {
                return sx_BulkIsDone<CLoadLock>(GetResult(), m_Key, m_Loaded);
            }
        string GetErrMsg(void) const
            {
                return "LoadStates("+
                    sx_DescribeError<CLoadLock>(GetResult(), m_Key, m_Loaded)+
                    "): data not found";
            }
        string GetStatisticsDescription(void) const
            {
                return "states("+
                    sx_DescribeError<CLoadLock>(GetResult(), m_Key, m_Loaded)+
                    ")";
            }
        CGBRequestStatistics::EStatType GetStatistics(void) const
            {
                return CGBRequestStatistics::eStat_BlobState;
            }
        size_t GetStatisticsCount(void) const
            {
                return m_Key.size();
            }
        
    private:
        const TKey& m_Key;
        TLoaded& m_Loaded;
        TRet& m_Ret;
    };

    static const size_t kMaxErrorSeqIds = 100;

    string sx_ErrorSeqIds(CReaderRequestResult& result, const vector<CBlob_id>& keys)
    {
        string ret = "; seq-ids: { ";
        if (result.GetRequestedId()) {
            ret += result.GetRequestedId().AsString();
        }
        else {
            size_t total = 0;
            for (auto key : keys) {
                CTSE_LoadLock lock = result.GetTSE_LoadLock(key);
                CTSE_Info::TSeqIds ids;
                lock->GetBioseqsIds(ids);
                if (!ids.empty()) {
                    int cnt = 0;
                    for (auto& id : ids) {
                        if (++total > kMaxErrorSeqIds) continue;
                        if (cnt++ > 0) ret += ", ";
                        ret += id.AsString();
                    }
                }
            }
            if (total == 0) return "";
            if (total > kMaxErrorSeqIds) {
                ret += ", ... (+" + NStr::NumericToString(total - kMaxErrorSeqIds) + " more)";
            }
        }
        ret += " }";
        return ret;
    }

    string sx_ErrorSeqIds(CReaderRequestResult& result,
                          const vector<pair<CBlob_id, vector<int>>>& keys)
    {
        string ret = "; seq-ids: { ";
        if (result.GetRequestedId()) {
            ret += result.GetRequestedId().AsString();
        }
        else {
            size_t total = 0;
            for ( auto& key : keys ) {
                CTSE_LoadLock lock = result.GetTSE_LoadLock(key.first);
                CTSE_Info::TSeqIds ids;
                lock->GetBioseqsIds(ids);
                if (!ids.empty()) {
                    int cnt = 0;
                    for (auto& id : ids) {
                        if (++total > kMaxErrorSeqIds) continue;
                        if (cnt++ > 0) ret += ", ";
                        ret += id.AsString();
                    }
                }
            }
            if (total == 0) return "";
            if (total > kMaxErrorSeqIds) {
                ret += ", ... (+" + NStr::NumericToString(total - kMaxErrorSeqIds) + " more)";
            }
        }
        ret += " }";
        return ret;
    }

    class CCommandLoadBlobState : public CReadDispatcherCommand
    {
    public:
        typedef CBlob_id TKey;
        typedef CLoadLockBlobState TLock;
        CCommandLoadBlobState(CReaderRequestResult& result,
                              const TKey& key)
            : CReadDispatcherCommand(result),
              m_Key(key), m_Lock(result, key)
            {
            }

        bool IsDone(void)
            {
                return m_Lock.IsLoadedBlobState();
            }
        bool Execute(CReader& reader)
            {
                return reader.LoadBlobState(GetResult(), m_Key);
            }
        string GetErrMsg(void) const
            {
                return "LoadBlobVersion("+m_Key.ToString()+")" +
                    sx_ErrorSeqIds(GetResult(), {m_Key}) + ": data not found";
            }
        CGBRequestStatistics::EStatType GetStatistics(void) const
            {
                return CGBRequestStatistics::eStat_BlobState;
            }
        string GetStatisticsDescription(void) const
            {
                return "blob-version("+m_Key.ToString()+")";
            }
        
    private:
        TKey m_Key;
        TLock m_Lock;
    };

    class CCommandLoadBlobVersion : public CReadDispatcherCommand
    {
    public:
        typedef CBlob_id TKey;
        typedef CLoadLockBlobVersion TLock;
        CCommandLoadBlobVersion(CReaderRequestResult& result,
                                const TKey& key)
            : CReadDispatcherCommand(result),
              m_Key(key), m_Lock(result, key)
            {
            }

        bool IsDone(void)
            {
                return m_Lock.IsLoadedBlobVersion();
            }
        bool Execute(CReader& reader)
            {
                return reader.LoadBlobVersion(GetResult(), m_Key);
            }
        string GetErrMsg(void) const
            {
                return "LoadBlobVersion("+m_Key.ToString()+")" +
                    sx_ErrorSeqIds(GetResult(), {m_Key}) + ": "
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
                             const CReader::TBlobIds& infos,
                             CReadDispatcher::TContentsMask mask = fBlobHasAll,
                             const SAnnotSelector* sel = 0)
    {
        for ( auto& info : infos ) {
            if ( !info.Matches(mask, sel) ) {
                continue;
            }
            CLoadLockBlob blob(result, *info.GetBlob_id());
            if ( !blob.IsLoadedBlob() ) {
                return false;
            }
        }
        return true;
    }
    
    bool s_AllBlobsAreLoaded(CReaderRequestResult& result,
                             const CLoadLockBlobIds& blobs,
                             CReadDispatcher::TContentsMask mask,
                             const SAnnotSelector* sel)
    {
        _ASSERT(blobs.IsLoaded());
        return s_AllBlobsAreLoaded(result, blobs.GetBlob_ids().Get(), mask, sel);
    }

    class CCommandLoadBlobs : public CReadDispatcherCommand
    {
    public:
        typedef CLoadLockBlobIds TIds;
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
        typedef CLoadLockBlobIds TIds;
        typedef CReadDispatcher::TContentsMask TMask;
        CCommandLoadSeq_idBlobs(CReaderRequestResult& result,
                                const TKey& key, TMask mask,
                                const SAnnotSelector* sel)
            : CReadDispatcherCommand(result),
              m_Key(key), m_Ids(result, key, sel),
              m_Mask(mask), m_Selector(sel)
            {
            }

        bool IsDone(void)
            {
                return s_Blob_idsLoaded(m_Ids, GetResult(), m_Key) &&
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
        TIds m_Ids;
        TMask m_Mask;
        const SAnnotSelector* m_Selector;
    };

    class CCommandLoadBlobList : public CReadDispatcherCommand
    {
    public:
        typedef CReader::TBlobIds TIds;
        CCommandLoadBlobList(CReaderRequestResult& result,
                             const TIds& ids)
            : CReadDispatcherCommand(result),
              m_Ids(ids)
            {
            }

        bool IsDone(void)
            {
                return s_AllBlobsAreLoaded(GetResult(), m_Ids);
            }
        bool Execute(CReader& reader)
            {
                return reader.LoadBlobs(GetResult(), m_Ids);
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
        const TIds& m_Ids;
    };
    
    class CCommandLoadBlob : public CReadDispatcherCommand
    {
    public:
        typedef CBlob_id TKey;
        typedef CLoadLockBlob TLock;
        CCommandLoadBlob(CReaderRequestResult& result,
                         const TKey& key,
                         const CBlob_Info* blob_info = 0)
            : CReadDispatcherCommand(result),
              m_Key(key),
              m_Lock(result, key),
              m_BlobInfo(blob_info)
            {
            }

        bool IsDone(void)
            {
                return m_Lock.IsLoadedBlob();
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
                return "LoadBlob("+m_Key.ToString()+")" +
                    sx_ErrorSeqIds(GetResult(), {m_Key}) + ": "
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
        TLock m_Lock;
        const CBlob_Info* m_BlobInfo;
    };

    class CCommandLoadChunk : public CReadDispatcherCommand
    {
    public:
        typedef CBlob_id TKey;
        typedef CLoadLockBlob TLock;
        typedef int TChunkId;
        CCommandLoadChunk(CReaderRequestResult& result,
                          const TKey& key,
                          TChunkId chunk_id)
            : CReadDispatcherCommand(result),
              m_Key(key), m_Lock(result, key, chunk_id),
              m_ChunkId(chunk_id)
            {
            }

        bool IsDone(void)
            {
                return m_Lock.IsLoadedChunk();
            }
        bool Execute(CReader& reader)
            {
                return reader.LoadChunk(GetResult(), m_Key, m_ChunkId);
            }
        string GetErrMsg(void) const
            {
                return "LoadChunk("+m_Key.ToString()+", "+
                    NStr::IntToString(m_ChunkId)+")" +
                    sx_ErrorSeqIds(GetResult(), {m_Key}) + ": "
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
    };

    class CCommandLoadChunks : public CReadDispatcherCommand
    {
    public:
        typedef CBlob_id TKey;
        typedef CLoadLockBlob TLock;
        typedef int TChunkId;
        typedef vector<TChunkId> TChunkIds;
        CCommandLoadChunks(CReaderRequestResult& result,
                           const TKey& key,
                           const TChunkIds chunk_ids)
            : CReadDispatcherCommand(result),
              m_Key(key), m_Lock(result, key),
              m_ChunkIds(chunk_ids)
            {
            }

        bool IsDone(void)
            {
                ITERATE ( TChunkIds, it, m_ChunkIds ) {
                    if ( !m_Lock.IsLoadedChunk(*it) ) {
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
                str << "LoadChunks(" << m_Key.ToString() << "; chunks: {";
                int cnt = 0;
                ITERATE ( TChunkIds, it, m_ChunkIds ) {
                    if ( !m_Lock.IsLoadedChunk(*it) ) {
                        if ( cnt++ ) str << ',';
                        str << ' ' << *it;
                    }
                }
                str << " })" + sx_ErrorSeqIds(GetResult(), {m_Key}) + ": data not found";
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
                ITERATE ( TChunkIds, it, m_ChunkIds ) {
                    int id = *it;
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
    };

    class CCommandLoadBlobChunks : public CReadDispatcherCommand
    {
    public:
        typedef int TChunkId;
        typedef vector<TChunkId> TChunkIds;
        typedef vector<pair<CBlob_id, TChunkIds>> TKey;
        typedef vector<CLoadLockBlob> TLock;
        CCommandLoadBlobChunks(CReaderRequestResult& result,
                               const TKey& key)
            : CReadDispatcherCommand(result),
              m_Key(key)
            {
                for ( auto& blob : key ) {
                    m_Lock.push_back(CLoadLockBlob(result, blob.first));
                }
            }

        bool IsDone(void)
            {
                for ( size_t i = 0; i < m_Key.size(); ++i ) {
                    for ( auto chunk : m_Key[i].second ) {
                        if ( !m_Lock[i].IsLoadedChunk(chunk) ) {
                            return false;
                        }
                    }
                }
                return true;
            }
        bool Execute(CReader& reader)
            {
                return reader.LoadChunks(GetResult(), m_Key);
            }
        string GetErrMsg(void) const
            {
                CNcbiOstrstream str;
                str << "LoadChunks(";
                int blob_cnt = 0;
                for ( size_t i = 0; i < m_Key.size(); ++i ) {
                    int chunk_cnt = 0;
                    for ( auto& chunk : m_Key[i].second ) {
                        if ( !m_Lock[i].IsLoadedChunk(chunk) ) {
                            if ( chunk_cnt++ ) {
                                str << ',';
                            }
                            else {
                                if ( blob_cnt++ ) {
                                    str << ", ";
                                }
                                str << "("<<m_Key[i].first.ToString() << "; chunks: {";
                            }
                            str << ' ' << chunk;
                        }
                    }
                    if ( chunk_cnt ) {
                        str << "})";
                    }
                }
                str << sx_ErrorSeqIds(GetResult(), m_Key) + ": data not found";
                return CNcbiOstrstreamToString(str);
            }
        CGBRequestStatistics::EStatType GetStatistics(void) const
            {
                return CGBRequestStatistics::eStat_LoadChunk;
            }
        string GetStatisticsDescription(void) const
            {
                CNcbiOstrstream str;
                int blob_cnt = 0;
                for ( auto& blob : m_Key ) {
                    if ( blob_cnt++ ) {
                        str << ',';
                    }
                    int cnt = 0;
                    for ( auto id : blob.second ) {
                        if ( id >= 0 && id < kMax_Int ) {
                            if ( !cnt ) {
                                str << "chunk(" << blob.first.ToString() << '.';
                                cnt = 1;
                            }
                            else {
                                str << ',';
                            }
                            str << id;
                        }
                    }
                    if ( !cnt ) {
                        str << "blob(" << blob.first.ToString();
                    }
                    str << ')';
                }
                return CNcbiOstrstreamToString(str);
            }
        
    private:
        TKey m_Key;
        TLock m_Lock;
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
                    CLoadLockBlobIds blob_ids(result, *id, 0);
                    if ( !blob_ids ) {
                        return false;
                    }
                    if ( !s_Blob_idsLoaded(blob_ids, result, *id) ) {
                        return false;
                    }
                    CFixedBlob_ids ids = blob_ids.GetBlob_ids();
                    ITERATE ( CFixedBlob_ids, it, ids ) {
                        if ( !it->Matches(fBlobHasCore, 0) ) {
                            continue;
                        }
                        CLoadLockBlob blob(result, *it->GetBlob_id());
                        if ( !blob.IsLoadedBlob() ) {
                            return false;
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
                CReaderRequestResultRecursion r(command.GetResult());
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
             !reader.MayBeSkippedOnErrors() &&
             !s_AllowIncompleteCommands() ) {
            NCBI_THROW(CLoaderException, eLoaderFailed, command.GetErrMsg());
        }
    }

    if ( !command.MayBeSkipped() &&
         !s_AllowIncompleteCommands() ) {
        NCBI_THROW(CLoaderException, eLoaderFailed, command.GetErrMsg());
    }
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


void CReadDispatcher::LoadSequenceHash(CReaderRequestResult& result,
                                       const CSeq_id_Handle& seq_id)
{
    CCommandLoadSequenceHash command(result, seq_id);
    Process(command);
}


void CReadDispatcher::LoadSequenceLength(CReaderRequestResult& result,
                                         const CSeq_id_Handle& seq_id)
{
    CCommandLoadSequenceLength command(result, seq_id);
    Process(command);
}


void CReadDispatcher::LoadSequenceType(CReaderRequestResult& result,
                                       const CSeq_id_Handle& seq_id)
{
    CCommandLoadSequenceType command(result, seq_id);
    Process(command);
}


void CReadDispatcher::LoadBulkIds(CReaderRequestResult& result,
                                  const TIds& ids, TLoaded& loaded, TBulkIds& ret)
{
    CCommandLoadBulkIds command(result, ids, loaded, ret);
    Process(command);
}


void CReadDispatcher::LoadAccVers(CReaderRequestResult& result,
                                  const TIds& ids, TLoaded& loaded, TIds& ret)
{
    CCommandLoadAccVers command(result, ids, loaded, ret);
    Process(command);
}


void CReadDispatcher::LoadGis(CReaderRequestResult& result,
                              const TIds& ids, TLoaded& loaded, TGis& ret)
{
    CCommandLoadGis command(result, ids, loaded, ret);
    Process(command);
}


void CReadDispatcher::LoadLabels(CReaderRequestResult& result,
                                 const TIds& ids, TLoaded& loaded, TLabels& ret)
{
    CCommandLoadLabels command(result, ids, loaded, ret);
    Process(command);
}


void CReadDispatcher::LoadTaxIds(CReaderRequestResult& result,
                                 const TIds& ids, TLoaded& loaded, TTaxIds& ret)
{
    CCommandLoadTaxIds command(result, ids, loaded, ret);
    Process(command);
}


void CReadDispatcher::LoadHashes(CReaderRequestResult& result,
                                 const TIds& ids, TLoaded& loaded,
                                 THashes& ret, TKnown& known)
{
    CCommandLoadHashes command(result, ids, loaded, ret, known);
    Process(command);
}


void CReadDispatcher::LoadLengths(CReaderRequestResult& result,
                                  const TIds& ids, TLoaded& loaded, TLengths& ret)
{
    CCommandLoadLengths command(result, ids, loaded, ret);
    Process(command);
}


void CReadDispatcher::LoadTypes(CReaderRequestResult& result,
                                const TIds& ids, TLoaded& loaded, TTypes& ret)
{
    CCommandLoadTypes command(result, ids, loaded, ret);
    Process(command);
}


void CReadDispatcher::LoadStates(CReaderRequestResult& result,
                                 const TIds& ids, TLoaded& loaded, TStates& ret)
{
    CCommandLoadStates command(result, ids, loaded, ret);
    Process(command);
}


void CReadDispatcher::LoadSeq_idBlob_ids(CReaderRequestResult& result,
                                         const CSeq_id_Handle& seq_id,
                                         const SAnnotSelector* sel)
{
    CCommandLoadSeq_idBlob_ids command(result, seq_id, sel);
    Process(command);
}


void CReadDispatcher::LoadBlobState(CReaderRequestResult& result,
                                    const TBlobId& blob_id)
{
    CCommandLoadBlobState command(result, blob_id);
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
                                const CLoadLockBlobIds& blobs,
                                TContentsMask mask,
                                const SAnnotSelector* sel)
{
    CCommandLoadBlobs command(result, blobs, mask, sel);
    Process(command);
}


void CReadDispatcher::LoadBlobs(CReaderRequestResult& result,
                                const TBlobInfos& blob_infos)
{
    CCommandLoadBlobList command(result, blob_infos);
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


void CReadDispatcher::LoadChunks(CReaderRequestResult& result,
                                 const TBlobChunkIds& chunk_ids)
{
    CCommandLoadBlobChunks command(result, chunk_ids);
    Process(command);
}


void CReadDispatcher::LoadBlobSet(CReaderRequestResult& result,
                                  const TIds& seq_ids)
{
    CCommandLoadBlobSet command(result, seq_ids);
    Process(command);
}


bool CReadDispatcher::SetBlobState(size_t i,
                                   CReaderRequestResult& result,
                                   const TIds& ids, TLoaded& loaded,
                                   TStates& ret)
{
    if ( loaded[i] || CannotProcess(ids[i]) ) {
        return true;
    }
    CLoadLockBlobIds lock(result, ids[i]);
    if ( lock.IsLoaded() ) {
        CFixedBlob_ids blob_ids = lock.GetBlob_ids();
        if ( !blob_ids.IsFound() ) {
            ret[i] = lock.GetState();
            return true;
        }
        ITERATE ( CFixedBlob_ids, it, blob_ids ) {
            if ( it->Matches(fBlobHasCore, 0) ) {
                CFixedBlob_ids::TState state = lock.GetState();
                if ( state == CFixedBlob_ids::kUnknownState ) {
                    CLoadLockBlobState state_lock(result, *it->GetBlob_id());
                    if ( state_lock.IsLoadedBlobState() ) {
                        state = state_lock.GetBlobState();
                    }
                }
                if ( state != CFixedBlob_ids::kUnknownState ) {
                    ret[i] = state;
                    loaded[i] = true;
                    return true;
                }
                return false;
            }
        }
    }
    else {
        CLoadLockSeqIds ids_lock(result, ids[i], eAlreadyLoaded);
        if ( ids_lock && !ids_lock.GetData().IsFound() ) {
            ret[i] = ids_lock.GetState();
            return true;
        }
    }
    return false;
}


void CReadDispatcher::LogStat(CReadDispatcherCommand& command,
                              CReaderRequestResultRecursion& recursion)
{
    CReaderRequestResult& result = command.GetResult();
    double time = recursion.GetCurrentRequestTime();
    size_t count = command.GetStatisticsCount();
    CGBRequestStatistics& stat = sx_Statistics[command.GetStatistics()];
    stat.AddTime(time, count);
    if ( CollectStatistics() >= 2 ) {
        string descr = command.GetStatisticsDescription();
        const CSeq_id_Handle& idh = result.GetRequestedId();
        if ( idh ) {
            descr = descr + " for " + idh.AsString();
        }
        LOG_POST_X(8, setw(recursion.GetRecursionLevel()) << "" <<
                   "Dispatcher: read " <<
                   descr << " in " <<
                   setiosflags(ios::fixed) <<
                   setprecision(3) << (time*1000) << " ms");
    }
}


void CReadDispatcher::LogStat(CReadDispatcherCommand& command,
                              CReaderRequestResultRecursion& recursion,
                              double size)
{
    CReaderRequestResult& result = command.GetResult();
    double time = recursion.GetCurrentRequestTime();
    CGBRequestStatistics& stat = sx_Statistics[command.GetStatistics()];
    stat.AddTimeSize(time, size);
    if ( CollectStatistics() >= 2 ) {
        string descr = command.GetStatisticsDescription();
        const CSeq_id_Handle& idh = result.GetRequestedId();
        if ( idh ) {
            descr = descr + " for " + idh.AsString();
        }
        LOG_POST_X(9, setw(recursion.GetRecursionLevel()) << "" <<
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
