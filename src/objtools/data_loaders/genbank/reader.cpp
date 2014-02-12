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
 *  File Description: Base data reader interface
 *
 */

#include <ncbi_pch.hpp>
#include <objtools/data_loaders/genbank/reader.hpp>
#include <objtools/data_loaders/genbank/reader_params.h>
#include <objtools/data_loaders/genbank/dispatcher.hpp>
#include <objtools/data_loaders/genbank/request_result.hpp>
#include <objtools/data_loaders/genbank/processors.hpp>
#include <objtools/data_loaders/genbank/cache_manager.hpp>
#include <objtools/error_codes.hpp>

#include <objmgr/objmgr_exception.hpp>
#include <objmgr/annot_selector.hpp>
#include <algorithm>
#include <math.h>
#include <corelib/ncbi_system.hpp>


#define NCBI_USE_ERRCODE_X   Objtools_Reader

BEGIN_NCBI_SCOPE

NCBI_DEFINE_ERR_SUBCODE_X(7);

BEGIN_SCOPE(objects)

#define DEFAULT_RETRY_COUNT 5
#define DEFAULT_WAIT_TIME_ERRORS 2
#define DEFAULT_WAIT_TIME 1
#define DEFAULT_WAIT_TIME_MULTIPLIER 1.5
#define DEFAULT_WAIT_TIME_INCREMENT 1
#define DEFAULT_WAIT_TIME_MAX 30

static CIncreasingTime::SAllParams s_WaitTimeParams = {
    {
        NCBI_GBLOADER_READER_PARAM_WAIT_TIME,
        0,
        DEFAULT_WAIT_TIME
    },
    {
        NCBI_GBLOADER_READER_PARAM_WAIT_TIME_MAX,
        0,
        DEFAULT_WAIT_TIME_MAX
    },
    {
        NCBI_GBLOADER_READER_PARAM_WAIT_TIME_MULTIPLIER,
        0,
        DEFAULT_WAIT_TIME_MULTIPLIER
    },
    {
        NCBI_GBLOADER_READER_PARAM_WAIT_TIME_INCREMENT,
        0,
        DEFAULT_WAIT_TIME_INCREMENT
    }
};

CReader::CReader(void)
    : m_Dispatcher(0),
      m_MaxConnections(0),
      m_PreopenConnection(true),
      m_NextNewConnection(0),
      m_NumFreeConnections(0, 1000),
      m_MaximumRetryCount(3),
      m_ConnectFailCount(0),
      m_WaitTimeErrors(DEFAULT_WAIT_TIME_ERRORS),
      m_WaitTime(s_WaitTimeParams)
      
{
}

CReader::~CReader(void)
{
}


void CReader::InitParams(CConfig& conf,
                         const string& driver_name,
                         int default_max_conn)
{
    int retry_count =
        conf.GetInt(driver_name,
                    NCBI_GBLOADER_READER_PARAM_RETRY_COUNT,
                    CConfig::eErr_NoThrow,
                    DEFAULT_RETRY_COUNT);
    SetMaximumRetryCount(retry_count);
    bool open_initial_connection =
        conf.GetBool(driver_name,
                     NCBI_GBLOADER_READER_PARAM_PREOPEN,
                     CConfig::eErr_NoThrow,
                     true);
    SetPreopenConnection(open_initial_connection);
    m_WaitTimeErrors =
        conf.GetInt(driver_name,
                    NCBI_GBLOADER_READER_PARAM_WAIT_TIME_ERRORS,
                    CConfig::eErr_NoThrow,
                    DEFAULT_WAIT_TIME_ERRORS);
    m_WaitTime.Init(conf, driver_name, s_WaitTimeParams);

    int max_connections = conf.GetInt(driver_name,
                                      NCBI_GBLOADER_READER_PARAM_NUM_CONN,
                                      CConfig::eErr_NoThrow,
                                      -1);
    if ( max_connections == -1 ) {
        max_connections = conf.GetInt(driver_name,
                                      NCBI_GBLOADER_READER_PARAM2_NUM_CONN,
                                      CConfig::eErr_NoThrow,
                                      default_max_conn);
    }
    SetMaximumConnections(max_connections, default_max_conn);
}


void CReader::OpenInitialConnection(bool force)
{
    if ( GetMaximumConnections() > 0 && (force || GetPreopenConnection()) ) {
        for ( int attempt = 1; ; ++attempt ) {
            TConn conn = x_AllocConnection();
            try {
                OpenConnection(conn);
                x_ReleaseConnection(conn);
                return;
            }
            catch ( CLoaderException& exc ) {
                x_ReleaseClosedConnection(conn);
                if ( exc.GetErrCode() == exc.eNoConnection ) {
                    // no connection can be opened
                    throw;
                }
                LOG_POST_X(1, Warning<<"CReader: "
                           "cannot open initial connection: "<<exc.what());
                if ( attempt >= GetRetryCount() ) {
                    // this is the last attempt to establish connection
                    NCBI_RETHROW(exc, CLoaderException, eNoConnection,
                                 "cannot open initial connection");
                }
            }
            catch ( CException& exc ) {
                x_ReleaseClosedConnection(conn);
                LOG_POST_X(2, Warning<<"CReader: "
                           "cannot open initial connection: "<<exc.what());
                if ( attempt >= GetRetryCount() ) {
                    // this is the last attempt to establish connection
                    NCBI_RETHROW(exc, CLoaderException, eNoConnection,
                                 "cannot open initial connection");
                }
            }
        }
    }
}


void CReader::SetPreopenConnection(bool preopen)
{
    m_PreopenConnection = preopen;
}


bool CReader::GetPreopenConnection(void) const
{
    return m_PreopenConnection;
}


void CReader::SetMaximumConnections(int max, int default_max_conn)
{
    if ( max == 0 ) {
        max = default_max_conn;
    }
    SetMaximumConnections(max);
}


int CReader::SetMaximumConnections(int max)
{
    int limit = GetMaximumConnectionsLimit();
    if ( max > limit ) {
        max = limit;
    }
    if ( max < 0 ) {
        max = 0;
    }
    int error_count = 0;
    while( GetMaximumConnections() < max ) {
        try {
            x_AddConnection();
            error_count = 0;
        }
        catch ( exception& exc ) {
            LOG_POST_X(3, Warning <<
                       "CReader: cannot add connection: "<<exc.what());
            if ( ++error_count >= GetRetryCount() ) {
                if ( max > 0 && GetMaximumConnections() == 0 ) {
                    throw;
                }
            }
        }
    }
    while( GetMaximumConnections() > max ) {
        x_RemoveConnection();
    }
    return GetMaximumConnections();
}


int CReader::GetMaximumConnections(void) const
{
    return m_MaxConnections;
}


int CReader::GetMaximumConnectionsLimit(void) const
{
    return 1;
}


void CReader::WaitBeforeNewConnection(TConn /*conn*/)
{
    CMutexGuard guard(m_ConnectionsMutex);
    if ( !m_NextConnectTime.IsEmpty() ) {
        double wait_seconds =
            m_NextConnectTime.DiffNanoSecond(CTime(CTime::eCurrent))*1e-9;
        if ( wait_seconds > 0 ) {
            LOG_POST_X(6, Warning << "CReader: waiting "<<
                       wait_seconds<<"s before new connection");
            _TRACE("CReader: waiting "<<wait_seconds<<
                   "s before new connection");
            SleepMicroSec((unsigned long)(wait_seconds*1e6));
            return;
        }
        else {
            m_NextConnectTime.Clear();
            return;
        }
    }
    else if ( m_ConnectFailCount >= 2 ) {
        double wait_seconds = m_WaitTime.GetTime(m_ConnectFailCount-2);
        if ( wait_seconds > 0 ) {
            LOG_POST_X(7, Warning << "CReader: waiting "<<
                       wait_seconds<<"s before new connection");
            _TRACE("CReader: waiting "<<wait_seconds<<
                   "s before new connection");
            SleepMicroSec((unsigned long)(wait_seconds*1e6));
        }
    }
}


void CReader::ConnectSucceeds(TConn /*conn*/)
{
    m_ConnectFailCount = 0;
}


void CReader::ConnectFailed(TConn /*conn*/)
{
    CMutexGuard guard(m_ConnectionsMutex);
    ++m_ConnectFailCount;
    m_LastTimeFailed = CTime(CTime::eCurrent);
}


void CReader::SetNewConnectionDelayMicroSec(unsigned long micro_sec)
{
    CMutexGuard guard(m_ConnectionsMutex);
    CTime curr(CTime::eCurrent);
    m_NextConnectTime = curr.AddTimeSpan(CTimeSpan(micro_sec*1e-6));
}


void CReader::OpenConnection(TConn conn)
{
    WaitBeforeNewConnection(conn);
    try {
        x_ConnectAtSlot(conn);
    }
    catch ( exception& /*rethrown*/ ) {
        ConnectFailed(conn);
        throw;
    }
    ConnectSucceeds(conn);
}


void CReader::x_AddConnection(void)
{
    CMutexGuard guard(m_ConnectionsMutex);
    TConn conn = m_NextNewConnection++;
    x_AddConnectionSlot(conn);
    x_ReleaseClosedConnection(conn);
    ++m_MaxConnections;
    _ASSERT(m_MaxConnections > 0);
}


void CReader::x_RemoveConnection(void)
{
    TConn conn = x_AllocConnection(true);
    CMutexGuard guard(m_ConnectionsMutex);
    _ASSERT(m_MaxConnections > 0);
    --m_MaxConnections;
    x_RemoveConnectionSlot(conn);
}


CReader::TConn CReader::x_AllocConnection(bool oldest)
{
    if ( GetMaximumConnections() <= 0 ) {
        NCBI_THROW(CLoaderException, eNoConnection, "connections limit is 0");
    }
    m_NumFreeConnections.Wait();
    CMutexGuard guard(m_ConnectionsMutex);
    SConnSlot slot;
    if ( oldest ) {
        slot =  m_FreeConnections.back();
        m_FreeConnections.pop_back();
    }
    else {
        slot =  m_FreeConnections.front();
        m_FreeConnections.pop_front();
    }
    if ( !slot.m_LastUseTime.IsEmpty() ) {
        double age = CTime(CTime::eCurrent).DiffNanoSecond(slot.m_LastUseTime)*1e-9;
        if ( age > 60 ) {
            // too old, disconnect
            x_DisconnectAtSlot(slot.m_Conn, false);
        }
        else if ( age < slot.m_RetryDelay ) {
            double wait_seconds = slot.m_RetryDelay - age;
            LOG_POST_X(6, Warning << "CReader: waiting "<<
                       wait_seconds<<"s before next command");
            _TRACE("CReader: waiting "<<wait_seconds<<
                   "s before next connection");
            SleepMicroSec((unsigned long)(wait_seconds*1e6));
        }
    }
    return slot.m_Conn;
}


void CReader::x_ReleaseClosedConnection(TConn conn)
{
    CMutexGuard guard(m_ConnectionsMutex);
    SConnSlot slot;
    slot.m_Conn = conn;
    slot.m_RetryDelay = 0;
    m_FreeConnections.push_back(slot);
    m_NumFreeConnections.Post(1);
}


void CReader::x_ReleaseConnection(TConn conn, double retry_delay)
{
    CMutexGuard guard(m_ConnectionsMutex);
    SConnSlot slot;
    slot.m_Conn = conn;
    slot.m_LastUseTime = CTime(CTime::eCurrent);
    slot.m_RetryDelay = retry_delay;
    m_FreeConnections.push_front(slot);
    m_NumFreeConnections.Post(1);
}


void CReader::x_AbortConnection(TConn conn, bool failed)
{
    CMutexGuard guard(m_ConnectionsMutex);
    try {
        x_DisconnectAtSlot(conn, failed);
        x_ReleaseClosedConnection(conn);
        return;
    }
    catch ( exception& exc ) {
        ERR_POST_X(4, "CReader("<<conn<<"): "
                   "cannot reuse connection: "<<exc.what());
    }
    // cannot reuse connection number, allocate new one
    try {
        x_RemoveConnectionSlot(conn);
    }
    catch ( exception& ) {
        // ignore
    }
    _ASSERT(m_MaxConnections > 0);
    --m_MaxConnections;
    x_AddConnection();
}


void CReader::x_DisconnectAtSlot(TConn conn, bool failed)
{
    if ( failed ) {
        LOG_POST_X(5, Warning << "CReader("<<conn<<"): "
                   " GenBank connection failed: reconnecting...");
    }
    else {
        LOG_POST_X(5, Info << "CReader("<<conn<<"): "
                   " GenBank connection too old: reconnecting...");
    }
    x_RemoveConnectionSlot(conn);
    x_AddConnectionSlot(conn);
}


int CReader::GetConst(const string& ) const
{
    return 0;
}


void CReader::SetMaximumRetryCount(int retry_count)
{
    m_MaximumRetryCount = retry_count;
}


int CReader::GetRetryCount(void) const
{
    return m_MaximumRetryCount;
}


bool CReader::MayBeSkippedOnErrors(void) const
{
    return false;
}


bool CReader::LoadSeq_idGi(CReaderRequestResult& result,
                           const CSeq_id_Handle& seq_id)
{
    CLoadLockSeq_ids ids(result, seq_id);
    if ( ids.IsLoadedGi() ) {
        return true;
    }
    m_Dispatcher->LoadSeq_idSeq_ids(result, seq_id);
    return ids.IsLoadedGi();
}


bool CReader::LoadSeq_idAccVer(CReaderRequestResult& result,
                               const CSeq_id_Handle& seq_id)
{
    CLoadLockSeq_ids ids(result, seq_id);
    if ( ids.IsLoadedAccVer() ) {
        return true;
    }
    m_Dispatcher->LoadSeq_idSeq_ids(result, seq_id);
    return ids.IsLoadedAccVer();
}


bool CReader::LoadSeq_idLabel(CReaderRequestResult& result,
                              const CSeq_id_Handle& seq_id)
{
    CLoadLockSeq_ids ids(result, seq_id);
    if ( ids.IsLoadedLabel() ) {
        return true;
    }
    m_Dispatcher->LoadSeq_idSeq_ids(result, seq_id);
    return ids.IsLoadedLabel();
}


bool CReader::LoadSeq_idTaxId(CReaderRequestResult& result,
                              const CSeq_id_Handle& seq_id)
{
    CLoadLockSeq_ids ids(result, seq_id);
    if ( ids.IsLoadedTaxId() ) {
        return true;
    }
    
    m_Dispatcher->LoadBlobs(result, seq_id, fBlobHasCore|fBlobHasDescr, 0);
    CLoadLockBlob_ids blobs(result, seq_id, static_cast<SAnnotSelector*>(0));
    _ASSERT(blobs.IsLoaded());
    CFixedBlob_ids blob_ids = blobs->GetBlob_ids();
    ITERATE ( CFixedBlob_ids, it, blob_ids ) {
        const CBlob_Info& info = *it;
        const CBlob_id& blob_id = *info.GetBlob_id();
        if ( !info.Matches(fBlobHasCore, 0) ) {
            continue;
        }
        CLoadLockBlob blob(result, blob_id);
        _ASSERT(blob.IsLoaded());
        if ((blob.GetBlobState() & CBioseq_Handle::fState_no_data) != 0) {
            continue;
        }
            
        CConstRef<CBioseq_Info> bs_info = blob->FindMatchingBioseq(seq_id);
        if ( bs_info ) {
            ids.SetLoadedTaxId(bs_info->GetTaxId());
            break;
        }
    }
    
    if ( !ids.IsLoadedTaxId() ) {
        ids.SetLoadedTaxId(0);
    }

    return true;
}


bool CReader::LoadAccVers(CReaderRequestResult& result,
                          const TIds& ids, TLoaded& loaded, TIds& ret)
{
    size_t count = ids.size();
    for ( size_t i = 0; i < count; ++i ) {
        if ( loaded[i] || CReadDispatcher::CannotProcess(ids[i]) ) {
            continue;
        }
        CLoadLockSeq_ids seq_ids(result, ids[i]);
        if ( !seq_ids.IsLoadedAccVer() ) {
            m_Dispatcher->LoadSeq_idAccVer(result, ids[i]);
        }
        if ( seq_ids.IsLoadedAccVer() ) {
            ret[i] = seq_ids->GetAccVer();
            loaded[i] = true;
        }
    }
    return true;
}


bool CReader::LoadGis(CReaderRequestResult& result,
                      const TIds& ids, TLoaded& loaded, TGis& ret)
{
    size_t count = ids.size();
    for ( size_t i = 0; i < count; ++i ) {
        if ( loaded[i] || CReadDispatcher::CannotProcess(ids[i]) ) {
            continue;
        }
        CLoadLockSeq_ids seq_ids(result, ids[i]);
        if ( !seq_ids.IsLoadedGi() ) {
            m_Dispatcher->LoadSeq_idGi(result, ids[i]);
        }
        if ( seq_ids.IsLoadedGi() ) {
            ret[i] = seq_ids->GetGi();
            loaded[i] = true;
        }
    }
    return true;
}


bool CReader::LoadLabels(CReaderRequestResult& result,
                         const TIds& ids, TLoaded& loaded, TLabels& ret)
{
    size_t count = ids.size();
    for ( size_t i = 0; i < count; ++i ) {
        if ( loaded[i] || CReadDispatcher::CannotProcess(ids[i]) ) {
            continue;
        }
        CLoadLockSeq_ids seq_ids(result, ids[i]);
        if ( !seq_ids.IsLoadedLabel() ) {
            m_Dispatcher->LoadSeq_idLabel(result, ids[i]);
        }
        if ( seq_ids.IsLoadedLabel() ) {
            ret[i] = seq_ids->GetLabel();
            loaded[i] = true;
        }
    }
    return true;
}


bool CReader::LoadTaxIds(CReaderRequestResult& result,
                         const TIds& ids, TLoaded& loaded, TTaxIds& ret)
{
    size_t count = ids.size();
    for ( size_t i = 0; i < count; ++i ) {
        if ( loaded[i] || CReadDispatcher::CannotProcess(ids[i]) ) {
            continue;
        }
        CLoadLockSeq_ids seq_ids(result, ids[i]);
        if ( !seq_ids.IsLoadedTaxId() ) {
            m_Dispatcher->LoadSeq_idTaxId(result, ids[i]);
        }
        if ( seq_ids.IsLoadedTaxId() ) {
            ret[i] = seq_ids->GetTaxId();
            loaded[i] = true;
        }
    }
    return true;
}


bool CReader::LoadSeq_idBlob_ids(CReaderRequestResult& result,
                                 const CSeq_id_Handle& seq_id,
                                 const SAnnotSelector* sel)
{
    if ( !sel || !sel->IsIncludedAnyNamedAnnotAccession() ) {
        return false;
    }
    // recurse to non-named version
    CLoadLockBlob_ids src_ids(result, seq_id, 0);
    m_Dispatcher->LoadSeq_idBlob_ids(result, seq_id, 0);
    if ( !src_ids.IsLoaded() ) {
        return false;
    }
    CLoadLockBlob_ids dst_ids(result, seq_id, sel);
    dst_ids.SetLoadedBlob_ids(src_ids);
    return true;
}


bool CReader::LoadBlobs(CReaderRequestResult& result,
                        const string& seq_id,
                        TContentsMask mask,
                        const SAnnotSelector* sel)
{
    CLoadLockSeq_ids ids(result, seq_id);
    if ( !ids.IsLoaded() ) {
        if ( !LoadStringSeq_ids(result, seq_id) && !ids.IsLoaded() ) {
            return false;
        }
        if ( !ids.IsLoaded() ) {
            return true;
        }
    }
    CFixedSeq_ids seq_ids = ids->GetSeq_ids();
    if ( !seq_ids.empty() ) {
        m_Dispatcher->LoadBlobs(result, seq_ids.front(), mask, sel);
    }
    return true;
}


bool CReader::LoadBlobs(CReaderRequestResult& result,
                        const CSeq_id_Handle& seq_id,
                        TContentsMask mask,
                        const SAnnotSelector* sel)
{
    CLoadLockBlob_ids ids(result, seq_id, sel);
    if ( !ids.IsLoaded() ) {
        if ( !LoadSeq_idBlob_ids(result, seq_id, sel) && !ids.IsLoaded() ) {
            return false;
        }
        if ( !ids.IsLoaded() ) {
            return true;
        }
    }
    m_Dispatcher->LoadBlobs(result, ids, mask, sel);
    return true;
}


bool CReader::LoadBlobs(CReaderRequestResult& result,
                        CLoadLockBlob_ids blobs,
                        TContentsMask mask,
                        const SAnnotSelector* sel)
{
    int loaded_count = 0;
    CFixedBlob_ids blob_ids = blobs->GetBlob_ids();
    ITERATE ( CFixedBlob_ids, it, blob_ids ) {
        const CBlob_Info& info = *it;
        const CBlob_id& blob_id = *info.GetBlob_id();
        if ( info.Matches(mask, sel) ) {
            if ( result.IsBlobLoaded(blob_id) ) {
                continue;
            }
            
            if ( info.IsSetAnnotInfo() ) {
                CLoadLockBlob blob(result, blob_id);
                if ( !blob.IsLoaded() ) {
                    CProcessor_AnnotInfo::LoadBlob(result, info);
                }
                _ASSERT(blob.IsLoaded());
                ++loaded_count;
                continue;
            }

            m_Dispatcher->LoadBlob(result, blob_id);
            if ( result.IsBlobLoaded(blob_id) ) {
                ++loaded_count;
            }
        }
    }
    return loaded_count > 0;
}


bool CReader::LoadBlob(CReaderRequestResult& result,
                       const CBlob_Info& blob_info)
{
    const CBlob_id& blob_id = *blob_info.GetBlob_id();
    if ( result.IsBlobLoaded(blob_id) ) {
        return true;
    }
    
    if ( blob_info.IsSetAnnotInfo() ) {
        CLoadLockBlob blob(result, blob_id);
        if ( !blob.IsLoaded() ) {
            CProcessor_AnnotInfo::LoadBlob(result, blob_info);
        }
        _ASSERT(blob.IsLoaded());
        return true;
    }

    return LoadBlob(result, blob_id);
}


bool CReader::LoadChunk(CReaderRequestResult& /*result*/,
                        const TBlobId& /*blob_id*/,
                        TChunkId /*chunk_id*/)
{
    return false;
}


bool CReader::LoadChunks(CReaderRequestResult& result,
                         const TBlobId& blob_id,
                         const TChunkIds& chunk_ids)
{
    bool ret = false;
    ITERATE(TChunkIds, id, chunk_ids) {
        ret |= LoadChunk(result, blob_id, *id);
    }
    return ret;
}


bool CReader::LoadBlobSet(CReaderRequestResult& result,
                          const TSeqIds& seq_ids)
{
    bool ret = false;
    ITERATE(TSeqIds, id, seq_ids) {
        ret |= LoadBlobs(result, *id, fBlobHasCore, 0);
    }
    return ret;
}


void CReader::SetAndSaveNoBlob(CReaderRequestResult& result,
                               const TBlobId& blob_id,
                               TChunkId chunk_id,
                               TBlobState blob_state)
{
    blob_state |= CBioseq_Handle::fState_no_data;
    if ( result.SetNoBlob(blob_id, blob_state) ) {
        CWriter* writer =
            m_Dispatcher->GetWriter(result, CWriter::eBlobWriter);
        if ( writer ) {
            const CProcessor_St_SE* prc =
                dynamic_cast<const CProcessor_St_SE*>
                (&m_Dispatcher->GetProcessor(CProcessor::eType_St_Seq_entry));
            if ( prc ) {
                prc->SaveNoBlob(result, blob_id, chunk_id, blob_state, writer);
            }
        }
    }
}


void CReader::SetAndSaveNoBlob(CReaderRequestResult& result,
                               const TBlobId& blob_id,
                               TChunkId chunk_id,
                               const CLoadLockBlob& lock)
{
    SetAndSaveNoBlob(result, blob_id, chunk_id, lock.GetBlobState());
}


void CReader::SetAndSaveNoStringSeq_ids(CReaderRequestResult& result,
                                        const string& seq_id,
                                        TBlobState state) const
{
    CLoadLockSeq_ids lock(result, seq_id);
    SetAndSaveNoStringSeq_ids(result, seq_id, lock, state);
}


void CReader::SetAndSaveNoSeq_idSeq_ids(CReaderRequestResult& result,
                                        const CSeq_id_Handle& seq_id,
                                        TBlobState state) const
{
    CLoadLockSeq_ids lock(result, seq_id);
    SetAndSaveNoSeq_idSeq_ids(result, seq_id, lock, state);
}


void CReader::SetAndSaveStringSeq_ids(CReaderRequestResult& result,
                                      const string& seq_id,
                                      TBlobState state,
                                      const CFixedSeq_ids& seq_ids) const
{
    CLoadLockSeq_ids lock(result, seq_id);
    SetAndSaveStringSeq_ids(result, seq_id, lock, state, seq_ids);
}


void CReader::SetAndSaveStringGi(CReaderRequestResult& result,
                                 const string& seq_id,
                                 TGi gi) const
{
    CLoadLockSeq_ids lock(result, seq_id);
    SetAndSaveStringGi(result, seq_id, lock, gi);
}


void CReader::SetAndSaveSeq_idSeq_ids(CReaderRequestResult& result,
                                      const CSeq_id_Handle& seq_id,
                                      TBlobState state,
                                      const CFixedSeq_ids& seq_ids) const
{
    CLoadLockSeq_ids lock(result, seq_id);
    SetAndSaveSeq_idSeq_ids(result, seq_id, lock, state, seq_ids);
}


void CReader::SetAndSaveSeq_idGi(CReaderRequestResult& result,
                                 const CSeq_id_Handle& seq_id,
                                 TGi gi) const
{
    CLoadLockSeq_ids lock(result, seq_id);
    SetAndSaveSeq_idGi(result, seq_id, lock, gi);
}


void CReader::SetAndSaveSeq_idAccVer(CReaderRequestResult& result,
                                     const CSeq_id_Handle& seq_id,
                                     const CSeq_id& acc_id) const
{
    CLoadLockSeq_ids lock(result, seq_id);
    SetAndSaveSeq_idAccVer(result, seq_id, lock, acc_id);
}


void CReader::SetAndSaveSeq_idLabel(CReaderRequestResult& result,
                                    const CSeq_id_Handle& seq_id,
                                    const string& label) const
{
    CLoadLockSeq_ids lock(result, seq_id);
    SetAndSaveSeq_idLabel(result, seq_id, lock, label);
}


void CReader::SetAndSaveSeq_idTaxId(CReaderRequestResult& result,
                                    const CSeq_id_Handle& seq_id,
                                    int taxid) const
{
    CLoadLockSeq_ids lock(result, seq_id);
    SetAndSaveSeq_idTaxId(result, seq_id, lock, taxid);
}


void CReader::SetAndSaveSeq_idBlob_ids(CReaderRequestResult& result,
                                       const CSeq_id_Handle& seq_id,
                                       const SAnnotSelector* sel,
                                       TBlobState state,
                                       const CFixedBlob_ids& blob_ids) const
{
    CLoadLockBlob_ids lock(result, seq_id, sel);
    SetAndSaveSeq_idBlob_ids(result, seq_id, sel, lock, state, blob_ids);
}


void CReader::SetAndSaveNoSeq_idBlob_ids(CReaderRequestResult& result,
                                         const CSeq_id_Handle& seq_id,
                                         const SAnnotSelector* sel,
                                         TBlobState state) const
{
    CLoadLockBlob_ids lock(result, seq_id, sel);
    SetAndSaveNoSeq_idBlob_ids(result, seq_id, sel, lock, state);
}


void CReader::SetAndSaveBlobVersion(CReaderRequestResult& result,
                                    const TBlobId& blob_id,
                                    TBlobVersion version) const
{
    if ( result.SetBlobVersion(blob_id, version) ) {
        CWriter *writer = m_Dispatcher->GetWriter(result, CWriter::eIdWriter);
        if( writer ) {
            writer->SaveBlobVersion(result, blob_id, version);
        }
    }
}


void CReader::SetAndSaveNoStringSeq_ids(CReaderRequestResult& result,
                                        const string& seq_id,
                                        CLoadLockSeq_ids& lock,
                                        TBlobState state) const
{
    state |= CBioseq_Handle::fState_no_data;
    if ( !lock.SetNoSeq_ids(state) ) {
        return;
    }
    CWriter *writer = m_Dispatcher->GetWriter(result, CWriter::eIdWriter);
    if( writer ) {
        writer->SaveStringSeq_ids(result, seq_id);
    }
}


void CReader::SetAndSaveNoSeq_idSeq_ids(CReaderRequestResult& result,
                                        const CSeq_id_Handle& seq_id,
                                        CLoadLockSeq_ids& lock,
                                        TBlobState state) const
{
    state |= CBioseq_Handle::fState_no_data;
    if ( !lock.SetNoSeq_ids(state) ) {
        return;
    }
    CWriter *writer = m_Dispatcher->GetWriter(result, CWriter::eIdWriter);
    if( writer ) {
        writer->SaveSeq_idSeq_ids(result, seq_id);
    }
    SetAndSaveNoSeq_idBlob_ids(result, seq_id, 0, state);
}


void CReader::SetAndSaveStringSeq_ids(CReaderRequestResult& result,
                                      const string& seq_id,
                                      CLoadLockSeq_ids& lock,
                                      TBlobState state,
                                      const CFixedSeq_ids& seq_ids) const
{
    if ( seq_ids.empty() ) {
        SetAndSaveNoStringSeq_ids(result, seq_id, lock, state);
        return;
    }
    if ( !lock.SetLoadedSeq_ids(state, seq_ids) ) {
        return;
    }
    CWriter *writer = m_Dispatcher->GetWriter(result, CWriter::eIdWriter);
    if( writer ) {
        writer->SaveStringSeq_ids(result, seq_id);
    }
}


void CReader::SetAndSaveStringGi(CReaderRequestResult& result,
                                 const string& seq_id,
                                 CLoadLockSeq_ids& lock,
                                 TGi gi) const
{
    if ( !lock.SetLoadedGi(gi) ) {
        return;
    }
    CWriter *writer = m_Dispatcher->GetWriter(result, CWriter::eIdWriter);
    if( writer ) {
        writer->SaveStringGi(result, seq_id);
    }
}


void CReader::SetAndSaveSeq_idSeq_ids(CReaderRequestResult& result,
                                      const CSeq_id_Handle& seq_id,
                                      CLoadLockSeq_ids& lock,
                                      TBlobState state,
                                      const CFixedSeq_ids& seq_ids) const
{
    if ( seq_ids.empty() ) {
        SetAndSaveNoSeq_idSeq_ids(result, seq_id, lock, state);
        return;
    }
    else {
        if ( !lock.SetLoadedSeq_ids(state, seq_ids) ) {
            return;
        }
    }
    CWriter *writer = m_Dispatcher->GetWriter(result, CWriter::eIdWriter);
    if( writer ) {
        writer->SaveSeq_idSeq_ids(result, seq_id);
    }
    if ( state & CBioseq_Handle::fState_no_data ) {
        SetAndSaveNoSeq_idBlob_ids(result, seq_id, 0, state);
    }
}


void CReader::SetAndSaveSeq_idGi(CReaderRequestResult& result,
                                 const CSeq_id_Handle& seq_id,
                                 CLoadLockSeq_ids& lock,
                                 TGi gi) const
{
    if ( !lock.SetLoadedGi(gi) ) {
        return;
    }
    CWriter *writer = m_Dispatcher->GetWriter(result, CWriter::eIdWriter);
    if( writer ) {
        writer->SaveSeq_idGi(result, seq_id);
    }
}


void CReader::SetAndSaveSeq_idAccVer(CReaderRequestResult& result,
                                     const CSeq_id_Handle& seq_id,
                                     CLoadLockSeq_ids& lock,
                                     const CSeq_id& acc_id) const
{
    CSeq_id_Handle acc = CSeq_id_Handle::GetHandle(acc_id);
    if ( !lock.SetLoadedAccVer(acc) ) {
        return;
    }
    CWriter *writer = m_Dispatcher->GetWriter(result, CWriter::eIdWriter);
    if( writer ) {
        writer->SaveSeq_idAccVer(result, seq_id);
    }
}


void CReader::SetAndSaveSeq_idLabel(CReaderRequestResult& result,
                                    const CSeq_id_Handle& seq_id,
                                    CLoadLockSeq_ids& lock,
                                    const string& label) const
{
    if ( !lock.SetLoadedLabel(label) ) {
        return;
    }
    CWriter *writer = m_Dispatcher->GetWriter(result, CWriter::eIdWriter);
    if( writer ) {
        writer->SaveSeq_idLabel(result, seq_id);
    }
}


void CReader::SetAndSaveSeq_idTaxId(CReaderRequestResult& result,
                                    const CSeq_id_Handle& seq_id,
                                    CLoadLockSeq_ids& lock,
                                    int taxid) const
{
    if ( !lock.SetLoadedTaxId(taxid) ) {
        return;
    }
    CWriter *writer = m_Dispatcher->GetWriter(result, CWriter::eIdWriter);
    if( writer ) {
        writer->SaveSeq_idTaxId(result, seq_id);
    }
}


void CReader::SetAndSaveSeq_idBlob_ids(CReaderRequestResult& result,
                                       const CSeq_id_Handle& seq_id,
                                       const SAnnotSelector* sel,
                                       CLoadLockBlob_ids& lock,
                                       TBlobState state,
                                       const CFixedBlob_ids& blob_ids) const
{
    if ( blob_ids.empty() ) {
        SetAndSaveNoSeq_idBlob_ids(result, seq_id, sel, lock, state);
        return;
    }
    if ( !lock.SetLoadedBlob_ids(state, blob_ids) ) {
        return;
    }
    CWriter *writer = m_Dispatcher->GetWriter(result, CWriter::eIdWriter);
    if( writer ) {
        writer->SaveSeq_idBlob_ids(result, seq_id, sel);
    }
}


void CReader::SetAndSaveNoSeq_idBlob_ids(CReaderRequestResult& result,
                                         const CSeq_id_Handle& seq_id,
                                         const SAnnotSelector* sel,
                                         CLoadLockBlob_ids& lock,
                                         TBlobState state) const
{
    state |= CBioseq_Handle::fState_no_data;
    if ( !lock.SetNoBlob_ids(state) ) {
        return;
    }
    CWriter *writer = m_Dispatcher->GetWriter(result, CWriter::eIdWriter);
    if( writer ) {
        writer->SaveSeq_idBlob_ids(result, seq_id, sel);
    }
}


int CReader::ReadInt(CNcbiIstream& stream)
{
    int value;
    stream.read(reinterpret_cast<char*>(&value), sizeof(value));
    if ( stream.gcount() != sizeof(value) ) {
        NCBI_THROW(CLoaderException, eLoaderFailed,
                   "cannot read value");
    }
    return value;
}


void CReader::InitializeCache(CReaderCacheManager& /*cache_manager*/,
                              const TPluginManagerParamTree* /*params*/)
{
}


void CReader::ResetCache(void)
{
}


CReaderCacheManager::CReaderCacheManager(void)
{
}


CReaderCacheManager::~CReaderCacheManager(void)
{
}


CReaderCacheManager::SReaderCacheInfo::SReaderCacheInfo(ICache& cache, ECacheType cache_type)
    : m_Cache(&cache),
      m_Type(cache_type)
{
}

CReaderCacheManager::SReaderCacheInfo::~SReaderCacheInfo(void)
{
}

/////////////////////////////////////////////////////////////////////////////
// CReaderAllocatedConnection
/////////////////////////////////////////////////////////////////////////////

CReaderAllocatedConnection::CReaderAllocatedConnection(CReaderRequestResult& result, CReader* reader)
    : m_Result(0), m_Reader(0), m_Conn(0), m_Restart(false)
{
    if ( !reader ) {
        return;
    }
    CReaderAllocatedConnection* pconn = result.m_AllocatedConnection;
    if ( !pconn ) {
        result.ReleaseNotLoadedBlobs();
        m_Conn = reader->x_AllocConnection();
        m_Reader = reader;
        m_Result = &result;
        result.ClearRetryDelay();
        result.m_AllocatedConnection = this;
    }
    else if ( pconn->m_Reader == reader ) {
        // reuse allocated connection
        m_Conn = pconn->m_Conn;
        pconn->m_Conn = 0;
        pconn->m_Reader = 0;
        pconn->m_Result = 0;
        m_Reader = reader;
        m_Result = &result;
        result.m_AllocatedConnection = this;
    }
    else {
        NCBI_THROW(CLoaderException, eLoaderFailed,
                   "Only one reader can allocate connection for a result");
    }
}
 

CReaderAllocatedConnection::~CReaderAllocatedConnection(void)
{
    if ( m_Result ) {
        _ASSERT(m_Result->m_AllocatedConnection == this);
        _ASSERT(m_Reader);
        m_Result->ReleaseNotLoadedBlobs();
        m_Result->m_AllocatedConnection = 0;
        m_Reader->x_AbortConnection(m_Conn, !m_Restart);
    }
}


void CReaderAllocatedConnection::Restart(void)
{
    m_Restart = true;
}


void CReaderAllocatedConnection::Release(void)
{
    if ( m_Result ) {
        _ASSERT(m_Result->m_AllocatedConnection == this);
        _ASSERT(m_Reader);
        double retry_delay = m_Result->GetRetryDelay();
        m_Result->m_AllocatedConnection = 0;
        m_Result = 0;
        m_Reader->x_ReleaseConnection(m_Conn, min(retry_delay, 60.0));
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE
