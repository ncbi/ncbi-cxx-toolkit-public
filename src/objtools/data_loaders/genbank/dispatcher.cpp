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
#include <objmgr/objmgr_exception.hpp>
#include <objmgr/impl/tse_split_info.hpp>
#include <objmgr/impl/tse_chunk_info.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

/////////////////////////////////////////////////////////////////////////////
// CReadDispatcher
/////////////////////////////////////////////////////////////////////////////


CReadDispatcher::CReadDispatcher(void)
{
    CProcessor::RegisterAllProcessors(*this);
}


CReadDispatcher::~CReadDispatcher(void)
{
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
        NCBI_THROW(CLoaderException, eLoaderFailed,
                   "processor unknown: "+NStr::IntToString(type));
    }
    return *iter->second;
}


void CReadDispatcher::CheckReaders(void) const
{
    if ( m_Readers.empty() ) {
        NCBI_THROW(CLoaderException, eLoaderFailed, "no reader loaded");
    }
}


class CReadDispatcherCommand
{
public:
    CReadDispatcherCommand(CReaderRequestResult& result)
        : m_Result(result)
        {
        }
    
    virtual ~CReadDispatcherCommand(void)
        {
        }
    
    virtual bool IsDone(void) = 0;

    // return false if it doesn't make sense to retry
    virtual bool Execute(CReader& reader) = 0;

    virtual string GetErrMsg(void) const = 0;

    CReaderRequestResult& GetResult(void) const
        {
            return m_Result;
        }

private:
    CReaderRequestResult& m_Result;
};


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
                return m_Lock->IsLoadedGi();
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
        
    private:
        TKey m_Key;
        TLock m_Lock;
    };

    class CCommandLoadSeq_idBlob_ids : public CReadDispatcherCommand
    {
    public:
        typedef CSeq_id_Handle TKey;
        typedef CLoadLockBlob_ids TLock;
        CCommandLoadSeq_idBlob_ids(CReaderRequestResult& result,
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
                return reader.LoadSeq_idBlob_ids(GetResult(), m_Key);
            }
        string GetErrMsg(void) const
            {
                return "LoadSeq_idBlob_ids("+m_Key.AsString()+"): "
                    "data not found";
            }
        
    private:
        TKey m_Key;
        TLock m_Lock;
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
        
    private:
        TKey m_Key;
        TLock m_Lock;
    };

    bool s_AllBlobsAreLoaded(CReaderRequestResult& result,
                             const CLoadLockBlob_ids& blobs,
                             CReadDispatcher::TContentsMask mask)
    {
        _ASSERT(blobs.IsLoaded());

        ITERATE ( CLoadInfoBlob_ids, it, *blobs ) {
            const CBlob_Info& info = it->second;
            if ( (info.GetContentsMask() & mask) != 0 ) {
                CLoadLockBlob blob(result, it->first);
                if ( !blob.IsLoaded() ) {
                    return false;
                }
            }
        }
        return true;
    }
    int s_CountNotLoaded(CReaderRequestResult& result,
                         const CLoadLockBlob_ids& blobs,
                         CReadDispatcher::TContentsMask mask)
    {
        if ( !blobs.IsLoaded() ) {
            return kMax_Int;
        }

        int not_loaded = 0;
        ITERATE ( CLoadInfoBlob_ids, it, *blobs ) {
            const CBlob_Info& info = it->second;
            if ( (info.GetContentsMask() & mask) != 0 ) {
                CLoadLockBlob blob(result, it->first);
                if ( !blob.IsLoaded() ) {
                    ++not_loaded;
                }
            }
        }
        return not_loaded;
    }

    class CCommandLoadBlobs : public CReadDispatcherCommand
    {
    public:
        typedef CLoadLockBlob_ids TIds;
        typedef CReadDispatcher::TContentsMask TMask;
        CCommandLoadBlobs(CReaderRequestResult& result,
                          TIds ids, TMask mask)
            : CReadDispatcherCommand(result),
              m_Ids(ids), m_Mask(mask)
            {
            }

        bool IsDone(void)
            {
                return s_AllBlobsAreLoaded(GetResult(), m_Ids, m_Mask);
            }
        bool Execute(CReader& reader)
            {
                return reader.LoadBlobs(GetResult(), m_Ids, m_Mask);
            }
        string GetErrMsg(void) const
            {
                return "LoadBlobs(CLoadInfoBlob_ids): "
                    "data not found";
            }
        
    private:
        TIds m_Ids;
        TMask m_Mask;
    };
    /*
    class CCommandLoadStringBlobs : public CReadDispatcherCommand
    {
    public:
        typedef string TKey;
        typedef CLoadLockBlob_ids TIds;
        typedef CReadDispatcher::TContentsMask TMask;
        CCommandLoadStringBlobs(CReaderRequestResult& result,
                                const TKey& key, TMask mask)
            : CReadDispatcherCommand(result),
              m_Key(key), m_Ids(result, key), m_Mask(mask)
            {
            }

        bool IsDone(void)
            {
                return m_Ids.IsLoaded() &&
                    s_AllBlobsAreLoaded(GetResult(), m_Ids, m_Mask);
            }
        bool Execute(CReader& reader)
            {
                return reader.LoadBlobs(GetResult(), m_Key, m_Mask);
            }
        string GetErrMsg(void) const
            {
                return "LoadBlobs("+m_Key+"): "
                    "data not found";
            }
        
    private:
        TKey m_Key;
        TIds m_Ids;
        TMask m_Mask;
    };
    */
    class CCommandLoadSeq_idBlobs : public CReadDispatcherCommand
    {
    public:
        typedef CSeq_id_Handle TKey;
        typedef CLoadLockBlob_ids TIds;
        typedef CReadDispatcher::TContentsMask TMask;
        CCommandLoadSeq_idBlobs(CReaderRequestResult& result,
                                const TKey& key, TMask mask)
            : CReadDispatcherCommand(result),
              m_Key(key), m_Ids(result, key), m_Mask(mask)
            {
            }

        bool IsDone(void)
            {
                return m_Ids.IsLoaded() &&
                    s_AllBlobsAreLoaded(GetResult(), m_Ids, m_Mask);
            }
        bool Execute(CReader& reader)
            {
                return reader.LoadBlobs(GetResult(), m_Key, m_Mask);
            }
        string GetErrMsg(void) const
            {
                return "LoadBlobs("+m_Key.AsString()+"): "
                    "data not found";
            }
        
    private:
        TKey m_Key;
        TIds m_Ids;
        TMask m_Mask;
    };

    class CCommandLoadBlob : public CReadDispatcherCommand
    {
    public:
        typedef CBlob_id TKey;
        typedef CLoadLockBlob TLock;
        CCommandLoadBlob(CReaderRequestResult& result,
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
                return reader.LoadBlob(GetResult(), m_Key);
            }
        string GetErrMsg(void) const
            {
                return "LoadBlob("+m_Key.ToString()+"): "
                    "data not found";
            }
        
    private:
        TKey m_Key;
        TLock m_Lock;
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
        
    private:
        TKey m_Key;
        TLock m_Lock;
        TChunkId m_ChunkId;
        TChunkInfo& m_ChunkInfo;
    };
}


void CReadDispatcher::Process(CReadDispatcherCommand& command)
{
    CheckReaders();

    if ( command.IsDone() ) {
        return;
    }

    NON_CONST_ITERATE ( TReaders, rdr, m_Readers ) {
        CReader& reader = *rdr->second;
        command.GetResult().SetLevel(rdr->first);
        int retry_count = 0;
        do {
            ++retry_count;
            try {
                if ( !command.Execute(reader) ) {
                    retry_count = kMax_Int;
                }
            }
            catch ( CLoaderException& exc ) {
                if ( exc.GetErrCode() == exc.eNoConnection ) {
                    ERR_POST("CReadDispatcher: Exception: "<<exc.what());
                    retry_count = kMax_Int;
                }
                else {
                    if ( retry_count >= reader.GetRetryCount() &&
                         !reader.MayBeSkippedOnErrors() ) {
                        throw;
                    }
                    ERR_POST("CReadDispatcher: Exception: "<<exc.what());
                }
            }
            catch ( exception& exc ) {
                // error in the command
                if ( retry_count >= reader.GetRetryCount() &&
                     !reader.MayBeSkippedOnErrors() ) {
                    throw;
                }
                ERR_POST("CReadDispatcher: Exception: "<<exc.what());
            }
            if ( command.IsDone() ) {
                return;
            }
        } while ( retry_count < reader.GetRetryCount() );
        if ( !reader.MayBeSkippedOnErrors() ) {
            NCBI_THROW(CLoaderException, eLoaderFailed, command.GetErrMsg());
        }
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


void CReadDispatcher::LoadSeq_idBlob_ids(CReaderRequestResult& result,
                                         const CSeq_id_Handle& seq_id)
{
    CCommandLoadSeq_idBlob_ids command(result, seq_id);
    Process(command);
}


void CReadDispatcher::LoadBlobVersion(CReaderRequestResult& result,
                                     const TBlobId& blob_id)
{
    CCommandLoadBlobVersion command(result, blob_id);
    Process(command);
}

/*
void CReadDispatcher::LoadBlobs(CReaderRequestResult& result,
                                const string& seq_id,
                                TContentsMask mask)
{
    CCommandLoadStringBlobs command(result, seq_id, mask);
    Process(command);
}
*/

void CReadDispatcher::LoadBlobs(CReaderRequestResult& result,
                                const CSeq_id_Handle& seq_id,
                                TContentsMask mask)
{
    CCommandLoadSeq_idBlobs command(result, seq_id, mask);
    Process(command);
}


void CReadDispatcher::LoadBlobs(CReaderRequestResult& result,
                                CLoadLockBlob_ids blobs,
                                TContentsMask mask)
{
    CCommandLoadBlobs command(result, blobs, mask);
    Process(command);
}


void CReadDispatcher::LoadBlob(CReaderRequestResult& result,
                               const CBlob_id& blob_id)
{
    CCommandLoadBlob command(result, blob_id);
    Process(command);
}


void CReadDispatcher::LoadChunk(CReaderRequestResult& result,
                                const TBlobId& blob_id, TChunkId chunk_id)
{
    CCommandLoadChunk command(result, blob_id, chunk_id);
    Process(command);
}


void CReadDispatcher::SetAndSaveBlobState(CReaderRequestResult& result,
                                          const TBlobId& blob_id,
                                          TBlobState state) const
{
    CLoadLockBlob blob(result, blob_id);
    SetAndSaveBlobState(result, blob_id, blob, state);
}


void CReadDispatcher::SetAndSaveBlobState(CReaderRequestResult& result,
                                          const TBlobId& blob_id,
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


END_SCOPE(objects)
END_NCBI_SCOPE
