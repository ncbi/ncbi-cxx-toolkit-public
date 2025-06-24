/*  $Id$
 * ===========================================================================
 *
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
 *
 * ===========================================================================
 *
 * Author: Eugene Vasilchenko, Aleksey Grichenko
 *
 * File Description: Event loop for PSG data loader
 *
 * ===========================================================================
 */

#include <ncbi_pch.hpp>
#include <objtools/data_loaders/genbank/impl/psg_processors.hpp>
#include <objtools/data_loaders/genbank/impl/psg_cache.hpp>
#include <objtools/data_loaders/genbank/impl/wgsmaster.hpp>
#include <objmgr/impl/data_source.hpp>
#include <objmgr/impl/tse_split_info.hpp>
#include <objmgr/impl/tse_chunk_info.hpp>
#include <objmgr/impl/tse_loadlock.hpp>
#include <objmgr/impl/split_parser.hpp>
#include <objmgr/annot_selector.hpp>
#include <serial/objistr.hpp>
#include <serial/serial.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqsplit/ID2S_Split_Info.hpp>
#include <objects/seqsplit/ID2S_Chunk.hpp>
#include <objects/seqsplit/ID2S_Feat_type_Info.hpp>

#if defined(HAVE_PSG_LOADER)

BEGIN_NCBI_NAMESPACE;
BEGIN_NAMESPACE(objects);
BEGIN_NAMESPACE(psgl);


static
bool s_SameId(const CPSG_BlobId* id1, const CPSG_BlobId& id2)
{
    return id1 && id1->GetId() == id2.GetId();
}


static
bool s_HasFailedStatus(const CPSG_NamedAnnotStatus& na_status)
{
    for ( auto& s : na_status.GetId2AnnotStatusList() ) {
        if ( s.second == EPSG_Status::eError ) {
            return true;
        }
    }
    return false;
}


template<class Class>
static
string x_FormatPtr(const shared_ptr<Class>& ptr)
{
    return (ptr? " ...": " null");
}


/////////////////////////////////////////////////////////////////////////////
// CPSGL_BioseqInfo_Processor
/////////////////////////////////////////////////////////////////////////////


CPSGL_BioseqInfo_Processor::CPSGL_BioseqInfo_Processor(const CSeq_id_Handle& seq_id,
                                                       CPSGCaches* caches)
    : m_Seq_id(seq_id),
      m_BioseqInfoStatus(EPSG_Status::eNotFound),
      m_Caches(caches)
{
}


CPSGL_BioseqInfo_Processor::~CPSGL_BioseqInfo_Processor()
{
}


const char* CPSGL_BioseqInfo_Processor::GetProcessorName() const
{
    return "CPSGL_BioseqInfo_Processor";
}


ostream& CPSGL_BioseqInfo_Processor::PrintProcessorArgs(ostream& out) const
{
    return out << '(' << m_Seq_id << ')';;
}


CPSGL_Processor::EProcessResult
CPSGL_BioseqInfo_Processor::ProcessItemFast(EPSG_Status status,
                                            const shared_ptr<CPSG_ReplyItem>& item)
{
    _TRACE(Descr()<<": ProcessItemFast("<<int(status)<<", "<<item.get()<<")");
    if ( item->GetType() == CPSG_ReplyItem::eBioseqInfo ) {
        auto bioseq_info = dynamic_pointer_cast<CPSG_BioseqInfo>(item);
        shared_ptr<SPsgBioseqInfo> result;
        if ( m_Caches && status == EPSG_Status::eSuccess && bioseq_info ) {
            result = m_Caches->m_BioseqInfoCache.Add(m_Seq_id, *bioseq_info);
        }
        CFastMutexGuard guard(m_BioseqInfoMutex);
        if ( !m_BioseqInfo ) {
            m_BioseqInfoStatus = status;
            m_BioseqInfo = bioseq_info;
            m_BioseqInfoResult = result;
        }
        _TRACE(Descr()<<": ProcessItemFast("<<int(status)<<", "<<item.get()<<") info: "<<m_BioseqInfo.get());
    }
    return eProcessed;
}


CPSGL_Processor::EProcessResult
CPSGL_BioseqInfo_Processor::ProcessReplyFast(EPSG_Status status,
                                             const shared_ptr<CPSG_Reply>& reply)
{
    _TRACE(Descr()<<": ProcessReplyFast("<<int(status)<<")");
    if ( status == EPSG_Status::eSuccess &&
         m_BioseqInfoStatus == EPSG_Status::eSuccess &&
         m_BioseqInfo ) {
        // all good
        return eProcessed;
    }
    if ( (status == EPSG_Status::eNotFound ||
          status == EPSG_Status::eSuccess) &&
         (m_BioseqInfoStatus == EPSG_Status::eNotFound) ) {
        // PSG correctly reported no bioseq info
        if ( m_Caches ) {
            m_Caches->m_NoBioseqInfoCache.Add(GetSeq_id(), true);
        }
        return eProcessed;
    }
    if ( (status == EPSG_Status::eForbidden ||
          status == EPSG_Status::eSuccess) &&
         (m_BioseqInfoStatus == EPSG_Status::eForbidden) ) {
        // PSG correctly reported no forbidden bioseq info
        return eProcessed;
    }
    // inconsistent reply - invalid bioseq info or status
    return x_Failed("inconsistent reply:"+
                    x_Format(status, reply)+
                    " bioseq info"+x_FormatAndSetError(m_BioseqInfoStatus)+x_FormatPtr(m_BioseqInfo));
}


/////////////////////////////////////////////////////////////////////////////
// CPSGL_BlobInfo_Processor
/////////////////////////////////////////////////////////////////////////////


CPSGL_BlobInfo_Processor::CPSGL_BlobInfo_Processor(const CSeq_id_Handle& seq_id,
                                                   CPSGCaches* caches)
    : m_Seq_id(seq_id),
      m_BlobInfoStatus(EPSG_Status::eNotFound),
      m_Caches(caches)
{
}


CPSGL_BlobInfo_Processor::CPSGL_BlobInfo_Processor(const string& blob_id,
                                                   CPSGCaches* caches)
    : m_Blob_id(blob_id),
      m_BlobInfoStatus(EPSG_Status::eNotFound),
      m_Caches(caches)
{
}


CPSGL_BlobInfo_Processor::CPSGL_BlobInfo_Processor(const CSeq_id_Handle& seq_id,
                                                   const string& blob_id,
                                                   CPSGCaches* caches)
    : m_Seq_id(seq_id),
      m_Blob_id(blob_id),
      m_BlobInfoStatus(EPSG_Status::eNotFound),
      m_Caches(caches)
{
}


CPSGL_BlobInfo_Processor::~CPSGL_BlobInfo_Processor()
{
}


const char* CPSGL_BlobInfo_Processor::GetProcessorName() const
{
    return "CPSGL_BlobInfo_Processor";
}


ostream& CPSGL_BlobInfo_Processor::PrintProcessorArgs(ostream& out) const
{
    if ( !m_Blob_id.empty() ) {
        return out << '(' << m_Blob_id << ')';
    }
    else {
        return out << '(' << m_Seq_id << ')';
    }
}


CPSGL_Processor::EProcessResult
CPSGL_BlobInfo_Processor::ProcessItemFast(EPSG_Status status,
                                          const shared_ptr<CPSG_ReplyItem>& item)
{
    if ( item->GetType() == CPSG_ReplyItem::eBlobInfo ) {
        if ( auto blob_info = dynamic_pointer_cast<CPSG_BlobInfo>(item) ) {
            if ( blob_info->GetId<CPSG_BlobId>() ) {
                shared_ptr<SPsgBlobInfo> result = make_shared<SPsgBlobInfo>(*blob_info);
                if ( m_Caches && status == EPSG_Status::eSuccess ) {
                    m_Caches->m_BlobInfoCache.Add(result->blob_id_main, result);
                }
                CFastMutexGuard guard(m_BlobInfoMutex);
                if ( !m_BlobInfo ) {
                    m_BlobInfoStatus = status;
                    m_BlobInfo = blob_info;
                    m_BlobInfoResult = result;
                }
            }
        }
    }
    return eProcessed;
}


CPSGL_Processor::EProcessResult
CPSGL_BlobInfo_Processor::ProcessReplyFast(EPSG_Status status,
                                           const shared_ptr<CPSG_Reply>& reply)
{
    if ( status == EPSG_Status::eSuccess &&
         m_BlobInfoStatus == EPSG_Status::eSuccess &&
         m_BlobInfo ) {
        // all good
        return eProcessed;
    }
    if ( (status == EPSG_Status::eNotFound ||
          status == EPSG_Status::eSuccess) &&
         (m_BlobInfoStatus == EPSG_Status::eNotFound) ) {
        // PSG correctly reported no blob info
        return eProcessed;
    }
    if ( (status == EPSG_Status::eForbidden ||
          status == EPSG_Status::eSuccess) &&
         (m_BlobInfoStatus == EPSG_Status::eForbidden ||
          m_BlobInfoStatus == EPSG_Status::eNotFound) ) {
        // PSG correctly reported forbidden blob info
        m_BlobInfoStatus = EPSG_Status::eForbidden;
        return eProcessed;
    }
    // inconsistent reply - invalid bioseq info or status
    return x_Failed("inconsistent reply:"+
                    x_Format(status, reply)+
                    " blob info"+x_FormatAndSetError(m_BlobInfoStatus)+x_FormatPtr(m_BlobInfo));
}


/////////////////////////////////////////////////////////////////////////////
// CPSGL_Info_Processor
/////////////////////////////////////////////////////////////////////////////


CPSGL_Info_Processor::CPSGL_Info_Processor(const CSeq_id_Handle& seq_id,
                                           CPSGCaches* caches)
    : m_Seq_id(seq_id),
      m_BioseqInfoStatus(EPSG_Status::eNotFound),
      m_BlobInfoStatus(EPSG_Status::eNotFound),
      m_Caches(caches)
{
}


CPSGL_Info_Processor::CPSGL_Info_Processor(const CSeq_id_Handle& seq_id,
                                           const string& blob_id,
                                           CPSGCaches* caches)
    : m_Seq_id(seq_id),
      m_Blob_id(blob_id),
      m_BioseqInfoStatus(EPSG_Status::eNotFound),
      m_BlobInfoStatus(EPSG_Status::eNotFound),
      m_Caches(caches)
{
}


CPSGL_Info_Processor::~CPSGL_Info_Processor()
{
}


const char* CPSGL_Info_Processor::GetProcessorName() const
{
    return "CPSGL_Info_Processor";
}


ostream& CPSGL_Info_Processor::PrintProcessorArgs(ostream& out) const
{
    if ( m_Blob_id.empty() ) {
        return out << '(' << m_Seq_id << ')';
    }
    else {
        return out << '(' << m_Seq_id << ", " << m_Blob_id << ')';
    }
}


CPSGL_Processor::EProcessResult
CPSGL_Info_Processor::ProcessItemFast(EPSG_Status status,
                                      const shared_ptr<CPSG_ReplyItem>& item)
{
    _TRACE(Descr()<<": ProcessItemFast("<<int(status)<<", "<<item.get()<<")");
    if ( item->GetType() == CPSG_ReplyItem::eBioseqInfo ) {
        auto bioseq_info = dynamic_pointer_cast<CPSG_BioseqInfo>(item);
        shared_ptr<SPsgBioseqInfo> result;
        if ( m_Caches && status == EPSG_Status::eSuccess && bioseq_info ) {
            result = m_Caches->m_BioseqInfoCache.Add(m_Seq_id, *bioseq_info);
        }
        CFastMutexGuard guard(m_InfoMutex);
        if ( !m_BioseqInfo ) {
            m_BioseqInfoStatus = status;
            m_BioseqInfo = bioseq_info;
            m_BioseqInfoResult = result;
        }
        _TRACE(Descr()<<": ProcessItemFast("<<int(status)<<", "<<item.get()<<") info: "<<m_BioseqInfo.get());
    }
    if ( item->GetType() == CPSG_ReplyItem::eBlobInfo ) {
        if ( auto blob_info = dynamic_pointer_cast<CPSG_BlobInfo>(item) ) {
            if ( blob_info->GetId<CPSG_BlobId>() ) {
                shared_ptr<SPsgBlobInfo> result = make_shared<SPsgBlobInfo>(*blob_info);
                if ( m_Caches && status == EPSG_Status::eSuccess ) {
                    m_Caches->m_BlobInfoCache.Add(result->blob_id_main, result);
                }
                CFastMutexGuard guard(m_InfoMutex);
                if ( !m_BlobInfo ) {
                    m_BlobInfoStatus = status;
                    m_BlobInfo = blob_info;
                    m_BlobInfoResult = result;
                }
            }
        }
    }
    return eProcessed;
}


CPSGL_Processor::EProcessResult
CPSGL_Info_Processor::ProcessReplyFast(EPSG_Status status,
                                       const shared_ptr<CPSG_Reply>& reply)
{
    _TRACE(Descr()<<": ProcessReplyFast("<<int(status)<<")");
    // first check bioseq info replies
    if ( (status == EPSG_Status::eSuccess ||
          status == EPSG_Status::eForbidden) &&
         (m_BioseqInfoStatus == EPSG_Status::eSuccess ||
          m_BioseqInfoStatus == EPSG_Status::eForbidden) &&
         m_BioseqInfo ) {
        // correct bioseq info reply
    }
    else if ( (status == EPSG_Status::eNotFound ||
               status == EPSG_Status::eSuccess) &&
              (m_BioseqInfoStatus == EPSG_Status::eNotFound) ) {
        // correct 'not found' reply
        if ( m_Caches ) {
            m_Caches->m_NoBioseqInfoCache.Add(GetSeq_id(), true);
        }
    }
    else {
        // inconsistent bioseq info reply
        return x_Failed("inconsistent reply:"+
                        x_Format(status, reply)+
                        " bioseq info"+x_FormatAndSetError(m_BioseqInfoStatus)+x_FormatPtr(m_BioseqInfo));
    }
    // now check blob info replies
    if ( (status == EPSG_Status::eSuccess) &&
         (m_BlobInfoStatus == EPSG_Status::eSuccess) &&
         m_BlobInfo ) {
        // correct blob info reply
    }
    else if ( (status == EPSG_Status::eSuccess ||
               status == EPSG_Status::eNotFound) &&
              (m_BlobInfoStatus == EPSG_Status::eForbidden ||
               m_BlobInfoStatus == EPSG_Status::eNotFound) ) {
        // correct 'not found' reply
        if ( m_Caches && m_BioseqInfoResult && m_BioseqInfoResult->HasBlobId() ) {
            m_Caches->m_NoBlobInfoCache.Add(m_BioseqInfoResult->GetPSGBlobId(), m_BlobInfoStatus);
        }
    }
    else if ( (status == EPSG_Status::eForbidden) &&
              (m_BlobInfoStatus == EPSG_Status::eForbidden ||
               m_BlobInfoStatus == EPSG_Status::eNotFound) ) {
        // correct 'forbidden' reply
        m_BlobInfoStatus = EPSG_Status::eForbidden;
        if ( m_Caches && m_BioseqInfoResult && m_BioseqInfoResult->HasBlobId() ) {
            m_Caches->m_NoBlobInfoCache.Add(m_BioseqInfoResult->GetPSGBlobId(), m_BlobInfoStatus);
        }
    }
    else {
        // inconsistent reply - invalid blob info or status
        return x_Failed("inconsistent reply:"+
                        x_Format(status, reply)+
                        " blob info"+x_Format(m_BlobInfoStatus)+x_FormatPtr(m_BlobInfo));
    }
    // all good
    return eProcessed;
}


/////////////////////////////////////////////////////////////////////////////
// CPSGL_IpgTaxId_Processor
/////////////////////////////////////////////////////////////////////////////


CPSGL_IpgTaxId_Processor::CPSGL_IpgTaxId_Processor(const CSeq_id_Handle& seq_id,
                                                   bool is_WP_acc,
                                                   CPSGCaches* caches)
    : m_Seq_id(seq_id),
      m_IsWPAcc(is_WP_acc),
      m_IpgTaxIdStatus(EPSG_Status::eNotFound),
      m_Caches(caches)
{
}


CPSGL_IpgTaxId_Processor::~CPSGL_IpgTaxId_Processor()
{
}


const char* CPSGL_IpgTaxId_Processor::GetProcessorName() const
{
    return "CPSGL_IpgTaxId_Processor";
}


ostream& CPSGL_IpgTaxId_Processor::PrintProcessorArgs(ostream& out) const
{
    return out << '(' << m_Seq_id << ')';
}


CPSGL_Processor::EProcessResult
CPSGL_IpgTaxId_Processor::ProcessItemFast(EPSG_Status status,
                                          const shared_ptr<CPSG_ReplyItem>& item)
{
    _TRACE(Descr()<<": ProcessItemFast("<<int(status)<<", "<<item.get()<<")");
    if ( item->GetType() == CPSG_ReplyItem::eIpgInfo ) {
        if ( auto ipg_info = dynamic_pointer_cast<CPSG_IpgInfo>(item) ) {
            TTaxId tax_id = INVALID_TAX_ID;
            CFastMutexGuard guard(m_IpgTaxIdMutex);
            if ( m_TaxId == INVALID_TAX_ID ) {
                if ( !m_IsWPAcc ) {
                    m_TaxId = tax_id = ipg_info->GetTaxId();
                }
                else if (ipg_info->GetNucleotide().empty()) {
                    m_TaxId = tax_id = ipg_info->GetTaxId();
                }
                _TRACE(Descr()<<": ProcessItemFast("<<int(status)<<", "<<item.get()<<") tax id: "<<m_TaxId);
            }
            if ( m_Caches && tax_id != INVALID_TAX_ID ) {
                m_Caches->m_IpgTaxIdCache.Add(m_Seq_id, tax_id);
            }
        }
        else {
            _TRACE(Descr()<<": ProcessItemFast("<<int(status)<<", "<<item.get()<<") no tax id");
            return x_Failed("no IPG tax id");
        }
    }
    return eProcessed;
}


CPSGL_Processor::EProcessResult
CPSGL_IpgTaxId_Processor::ProcessReplyFast(EPSG_Status status,
                                           const shared_ptr<CPSG_Reply>& reply)
{
    _TRACE(Descr()<<": ProcessReplyFast("<<int(status)<<")");
    if ( status == EPSG_Status::eNotFound ) {
        return eProcessed;
    }
    if ( status != EPSG_Status::eSuccess ) {
        return x_Failed(x_Format(status, reply));
    }
    return eProcessed;
}


/////////////////////////////////////////////////////////////////////////////
// CPSGL_CDDAnnot_Processor
/////////////////////////////////////////////////////////////////////////////


CPSGL_CDDAnnot_Processor::CPSGL_CDDAnnot_Processor(const SCDDIds& cdd_ids,
                                                   const TSeqIdSet& seq_id_set,
                                                   CDataSource* data_source,
                                                   CPSGCaches* caches)
    : m_CDDIds(cdd_ids),
      m_SeqIdSet(seq_id_set),
      m_DataSource(data_source),
      m_Caches(caches)
{
}


CPSGL_CDDAnnot_Processor::~CPSGL_CDDAnnot_Processor()
{
}


const char* CPSGL_CDDAnnot_Processor::GetProcessorName() const
{
    return "CPSGL_CDDAnnot_Processor";
}


ostream& CPSGL_CDDAnnot_Processor::PrintProcessorArgs(ostream& out) const
{
    return out << '(' << m_CDDIds.gi << ')';;
}


CPSGL_Processor::EProcessResult
CPSGL_CDDAnnot_Processor::ProcessItemFast(EPSG_Status status,
                                          const shared_ptr<CPSG_ReplyItem>& item)
{
    switch ( item->GetType() ) {
    case CPSG_ReplyItem::eNamedAnnotInfo:
        m_AnnotInfo = dynamic_pointer_cast<CPSG_NamedAnnotInfo>(item);
        break;
    case CPSG_ReplyItem::eNamedAnnotStatus:
        m_AnnotStatus = dynamic_pointer_cast<CPSG_NamedAnnotStatus>(item);
        if ( s_HasFailedStatus(*m_AnnotStatus) ) {
            return x_Failed("annot status: failed");
        }
        break;
    case CPSG_ReplyItem::eBlobInfo:
        m_BlobInfo = dynamic_pointer_cast<CPSG_BlobInfo>(item);
        break;
    case CPSG_ReplyItem::eBlobData:
        m_BlobData = dynamic_pointer_cast<CPSG_BlobData>(item);
        break;
    default:
        break;
    }
    return eProcessed;
}


CPSGL_Processor::EProcessResult
CPSGL_CDDAnnot_Processor::ProcessReplyFast(EPSG_Status status,
                                           const shared_ptr<CPSG_Reply>& reply)
{
    if ( !m_AnnotInfo || !m_BlobInfo || !m_BlobData ) {
        if ( !m_AnnotInfo && m_Caches ) {
            m_Caches->m_NoCDDCache.Add(x_MakeLocalCDDEntryId(m_CDDIds), true);
        }
        return eProcessed;
    }

    CPSG_BlobId blob_id = m_AnnotInfo->GetBlobId();
    if ( !s_SameId(m_BlobInfo->GetId<CPSG_BlobId>(), blob_id) ||
         !s_SameId(m_BlobData->GetId<CPSG_BlobId>(), blob_id) ) {
        // inconsistent blob ids
        return x_Failed("inconsistent CDD blob ids for "+m_SeqIdSet.front().AsString());
    }
    return eToNextStage;
}


CPSGL_Processor::EProcessResult
CPSGL_CDDAnnot_Processor::ProcessReplySlow(EPSG_Status status,
                                           const shared_ptr<CPSG_Reply>& reply)
{
    CPSG_BlobId blob_id = m_AnnotInfo->GetBlobId();
    CRef<CPsgBlobId> dl_blob_id;
    if ( kCreateLocalCDDEntries ) {
        dl_blob_id = new CPsgBlobId(x_MakeLocalCDDEntryId(m_CDDIds));
    }
    else {
        SPsgAnnotInfo::TInfos infos{m_AnnotInfo};
        if ( m_Caches ) {
            m_Caches->m_AnnotInfoCache.Add(make_pair(kCDDAnnotName, m_SeqIdSet), infos);
        }
        dl_blob_id = new CPsgBlobId(blob_id.GetId());
    }
    dl_blob_id->SetTSEName(kCDDAnnotName);
    
    CTSE_LoadLock load_lock = m_DataSource->GetTSE_LoadLock(CBlobIdKey(dl_blob_id));
    _ASSERT(load_lock);
    if (load_lock.IsLoaded()) {
        if ( load_lock->x_NeedsDelayedMainChunk() ) {
            // not loaded yet, only split info generated from annot info
        }
        else {
            // fully loaded, just return result
            if ( !x_IsEmptyCDD(*load_lock) ) {
                m_TSE_Lock = load_lock;
            }
            return eProcessed;
        }
    }
    else {
        if ( kCreateLocalCDDEntries ) {
            x_CreateLocalCDDEntry(m_DataSource, m_CDDIds);
        }
        else {
            SPsgAnnotInfo::TInfos infos{m_AnnotInfo};
            if ( m_Caches ) {
                m_Caches->m_AnnotInfoCache.Add(make_pair(kCDDAnnotName, m_SeqIdSet), infos);
            }
            if ( m_Caches ) {
                m_Caches->m_BlobInfoCache.Add(blob_id.GetId(), make_shared<SPsgBlobInfo>(*m_BlobInfo));
            }
            UpdateOMBlobId(load_lock, dl_blob_id);
        }
    }
    
    unique_ptr<CObjectIStream> in(GetBlobDataStream(*m_BlobInfo, *m_BlobData));
    if (!in.get()) {
        // failed
        return x_Failed("cannot open CDD data stream for "+m_SeqIdSet.front().AsString());
    }
    CRef<CSeq_entry> entry(new CSeq_entry);
    *in >> *entry;
    if ( load_lock.IsLoaded() ) {
        // may need to create main chunk
        if ( load_lock->x_NeedsDelayedMainChunk() ) {
            CTSE_Chunk_Info& chunk = load_lock->GetSplitInfo().GetChunk(kDelayedMain_ChunkId);
            AutoPtr<CInitGuard> chunk_load_lock = chunk.GetLoadInitGuard();
            if ( chunk_load_lock.get() && *chunk_load_lock.get() ) {
                load_lock->SetSeq_entry(*entry);
                chunk.SetLoaded();
            }
        }
    }
    else {
        load_lock->SetSeq_entry(*entry);
        load_lock.SetLoaded();
    }
    if ( !x_IsEmptyCDD(*load_lock) ) {
        m_TSE_Lock = load_lock;
    }
    return eProcessed;
}


/////////////////////////////////////////////////////////////////////////////
// CPSGL_Get_Processor
/////////////////////////////////////////////////////////////////////////////


CPSGL_Get_Processor::CPSGL_Get_Processor(const CSeq_id_Handle& seq_id,
                                         CDataSource* data_source,
                                         CPSGCaches* caches,
                                         bool add_wgs_master)
    : CPSGL_Blob_Processor(data_source,
                           caches,
                           add_wgs_master),
      m_Seq_id(seq_id),
      m_BioseqInfoStatus(EPSG_Status::eNotFound)
{
    _ASSERT(m_Seq_id);
}


CPSGL_Get_Processor::~CPSGL_Get_Processor()
{
}


const char* CPSGL_Get_Processor::GetProcessorName() const
{
    return "CPSGL_Get_Processor";
}


ostream& CPSGL_Get_Processor::PrintProcessorArgs(ostream& out) const
{
    return out << '('<<m_Seq_id<<')';
}


bool CPSGL_Get_Processor::HasBlob_id() const
{
    return m_BioseqInfoResult && m_BioseqInfoResult->HasBlobId();
}


const string& CPSGL_Get_Processor::GetPSGBlobId() const
{
    return m_BioseqInfoResult->psg_blob_id;
}


CConstRef<CPsgBlobId> CPSGL_Get_Processor::GetDLBlobId() const
{
    return m_BioseqInfoResult->GetDLBlobId();
}


CPSGL_Processor::EProcessResult
CPSGL_Get_Processor::ProcessItemFast(EPSG_Status status,
                                     const shared_ptr<CPSG_ReplyItem>& item)
{
    if ( item->GetType() == CPSG_ReplyItem::eBioseqInfo ) {
        auto bioseq_info = dynamic_pointer_cast<CPSG_BioseqInfo>(item);
        shared_ptr<SPsgBioseqInfo> result;
        if ( m_Caches && status == EPSG_Status::eSuccess && bioseq_info ) {
            result = m_Caches->m_BioseqInfoCache.Add(m_Seq_id, *bioseq_info);
        }
        CConstRef<CPsgBlobId> dl_blob_id;
        {{
            CFastMutexGuard guard(m_GetMutex);
            if ( !m_BioseqInfo ) {
                m_BioseqInfoStatus = status;
                m_BioseqInfo = bioseq_info;
                if ( result && result->HasBlobId() ) {
                    m_BioseqInfoResult = result;
                    dl_blob_id = result->GetDLBlobId();
                }
            }
        }}
        if ( dl_blob_id ) {
            SetDLBlobId(dl_blob_id->ToPsgId(), dl_blob_id);
        }
        _TRACE(Descr()<<": ProcessItemFast("<<int(status)<<", "<<item.get()<<") info: "<<m_BioseqInfo.get());
        return eProcessed;
    }
    else {
        return CPSGL_Blob_Processor::ProcessItemFast(status, item);
    }
}


CPSGL_Processor::EProcessResult
CPSGL_Get_Processor::x_PreProcessReply(EPSG_Status status,
                                       const shared_ptr<CPSG_Reply>& reply)
{
    // determine sequence blob id
    if ( status == EPSG_Status::eError || m_BioseqInfoStatus == EPSG_Status::eError ) {
        return x_Failed(x_Format(status, reply)+
                        " bioseq info"+x_FormatAndSetError(m_BioseqInfoStatus));
    }
    if ( (status == EPSG_Status::eNotFound ||
          status == EPSG_Status::eSuccess) &&
         (m_BioseqInfoStatus == EPSG_Status::eNotFound) ) {
        // PSG correctly reported no bioseq info, no blobs to return
        _TRACE(Descr()<<": ProcessReplySlow(): processed w/o TSE");
        m_BioseqInfoStatus = EPSG_Status::eNotFound;
        if ( m_Caches ) {
            m_Caches->m_NoBioseqInfoCache.Add(m_Seq_id, true);
        }
        return eProcessed;
    }

    if ( !m_BioseqInfoResult ||
         !(status == EPSG_Status::eSuccess ||
           status == EPSG_Status::eForbidden) ||
         m_BioseqInfoStatus != EPSG_Status::eSuccess ) {
        // inconsistent reply - invalid bioseq info or status
        // unexpected status
        return x_Failed(x_Format(status, reply)+
                        " bioseq info"+x_FormatAndSetError(m_BioseqInfoStatus)+x_FormatPtr(m_BioseqInfoResult));
    }
    
    auto& psg_blob_id = GetPSGBlobId();
    if ( (status == EPSG_Status::eForbidden) ||
         (m_BioseqInfoStatus == EPSG_Status::eForbidden) ||
         GotForbidden() || GotUnauthorized() ) {
        // PSG reported 'forbidden' status - no blobs to return
        // but we need to prepare blob state
        CBioseq_Handle::TBioseqStateFlags state = CBioseq_Handle::fState_no_data;
        if ( GotUnauthorized() ) {
            state |= CBioseq_Handle::fState_confidential;
        }
        else {
            state |= CBioseq_Handle::fState_withdrawn;
        }
        // copy 'dead' state from bioseq info
        if ( m_BioseqInfoResult->IsDead() ) {
            state |= CBioseq_Handle::fState_dead;
        }
        // copy other state flags from blob info
        if ( !psg_blob_id.empty() ) {
            state |= (GetBlobInfoState(psg_blob_id) & ~CBioseq_Handle::fState_dead);
        }
        m_GotForbidden = true;
        m_ForbiddenBlobState = state;
        _TRACE(Descr()<<": ProcessReplySlow(): forbidden state: "<<state);
        m_BioseqInfoStatus = EPSG_Status::eForbidden;
        return eProcessed;
    }
    if ( psg_blob_id.empty() ) {
        // inconsistent reply - no blob id
        return x_Failed(x_Format(status, reply)+
                        "psg_blob_id is empty"+x_FormatAndSetError(m_BioseqInfoStatus));
    }

    _ASSERT(m_BioseqInfoStatus == EPSG_Status::eSuccess);
    _ASSERT(m_BioseqInfo);
    _ASSERT(m_BioseqInfoResult);
    // do not attempt to get bioseq TSE lock
    return eToNextStage;
}


CPSGL_Processor::EProcessResult
CPSGL_Get_Processor::ProcessReplyFast(EPSG_Status status,
                                      const shared_ptr<CPSG_Reply>& reply)
{
    _TRACE(Descr()<<": ProcessReplyFast("<<(int)status<<")");
    // process reply without getting TSE lock
    return x_PreProcessReply(status, reply);
}


CPSGL_Processor::EProcessResult
CPSGL_Get_Processor::ProcessReplySlow(EPSG_Status status,
                                      const shared_ptr<CPSG_Reply>& reply)
{
    _TRACE(Descr()<<": ProcessReplySlow("<<(int)status<<")");
    // attempt to get bioseq TSE lock
    _ASSERT(m_BioseqInfoStatus == EPSG_Status::eSuccess);
    _ASSERT(m_BioseqInfo);
    _ASSERT(m_BioseqInfoResult);
    return ProcessTSE_Lock(GetPSGBlobId(), m_TSE_Lock);
}


CPSGL_Processor::EProcessResult
CPSGL_Get_Processor::ProcessReplyFinal()
{
    _TRACE(Descr()<<": ProcessReplyFinal()");
    // even if lock cannot be obtained we return eProcessed now to allow recursive getblob request
    _ASSERT(m_BioseqInfoStatus == EPSG_Status::eSuccess);
    _ASSERT(m_BioseqInfo);
    _ASSERT(m_BioseqInfoResult);
    ProcessTSE_Lock(GetPSGBlobId(), m_TSE_Lock, eWaitForLock);
    return eProcessed;
}


CConstRef<CPsgBlobId> CPSGL_Get_Processor::CreateDLBlobId(STSESlot* /*tse_slot*/)
{
    return null;
}


/////////////////////////////////////////////////////////////////////////////
// CPSGL_GetBlob_Processor
/////////////////////////////////////////////////////////////////////////////

CPSGL_GetBlob_Processor::CPSGL_GetBlob_Processor(const CPsgBlobId& dl_blob_id,
                                                 CDataSource* data_source,
                                                 CPSGCaches* caches,
                                                 bool add_wgs_master)
    : CPSGL_Blob_Processor(data_source,
                           caches,
                           add_wgs_master),
      m_Blob_id(dl_blob_id.ToPsgId())
{
    SetDLBlobId(m_Blob_id, Ref(&dl_blob_id));
}


CPSGL_GetBlob_Processor::~CPSGL_GetBlob_Processor()
{
}


const char* CPSGL_GetBlob_Processor::GetProcessorName() const
{
    return "CPSGL_GetBlob_Processor";
}


ostream& CPSGL_GetBlob_Processor::PrintProcessorArgs(ostream& out) const
{
    return out << '('<<m_Blob_id<<')';
}


CPSGL_Processor::EProcessResult
CPSGL_GetBlob_Processor::ProcessReplyFast(EPSG_Status status,
                                          const shared_ptr<CPSG_Reply>& reply)
{
    _TRACE(Descr()<<": ProcessReplyFast()");
    if ( status == EPSG_Status::eError ) {
        return x_Failed(x_Format(status, reply));
    }
    if ( status == EPSG_Status::eForbidden ) {
        _TRACE(Descr()<<": ProcessReplyFast(): forbidden: "<<m_Blob_id);
        m_GotForbidden = true;
        return eProcessed;
    }

    return eToNextStage;
}


CPSGL_Processor::EProcessResult
CPSGL_GetBlob_Processor::ProcessReplySlow(EPSG_Status status,
                                          const shared_ptr<CPSG_Reply>& reply)
{
    _TRACE(Descr()<<": ProcessReplySlow()");
    if ( status == EPSG_Status::eError ) {
        return x_Failed(x_Format(status, reply));
    }
    if ( status == EPSG_Status::eForbidden ) {
        _TRACE(Descr()<<": ProcessReplySlow(): forbidden: "<<m_Blob_id);
        m_GotForbidden = true;
        return eProcessed;
    }

    // prepare TSE lock
    return ProcessTSE_Lock(m_Blob_id, m_TSE_Lock);
}


CPSGL_Processor::EProcessResult
CPSGL_GetBlob_Processor::ProcessReplyFinal()
{
    _TRACE(Descr()<<": FinalizeResult()");
    // prepare TSE lock
    return ProcessTSE_Lock(m_Blob_id, m_TSE_Lock, eWaitForLock);
}


/////////////////////////////////////////////////////////////////////////////
// CPSGL_GetChunk_Processor
/////////////////////////////////////////////////////////////////////////////

CPSGL_GetChunk_Processor::CPSGL_GetChunk_Processor(CTSE_Chunk_Info& chunk,
                                                   CDataSource* data_source,
                                                   CPSGCaches* caches,
                                                   bool add_wgs_master)
    : CPSGL_Blob_Processor(data_source,
                           caches,
                           add_wgs_master)
{
    AddChunk(chunk);
}


CPSGL_GetChunk_Processor::~CPSGL_GetChunk_Processor()
{
}


const char* CPSGL_GetChunk_Processor::GetProcessorName() const
{
    return "CPSGL_GetChunk_Processor";
}


ostream& CPSGL_GetChunk_Processor::PrintProcessorArgs(ostream& out) const
{
    out << '(';
    PrintChunk(out, *m_Chunks.front());
    return out << ')';
}


ostream& CPSGL_GetChunk_Processor::PrintChunk(ostream& out, CTSE_Chunk_Info& chunk) const
{
    return out << chunk.GetBlobId().ToString()<<'/'<<chunk.GetChunkId();
}


void CPSGL_GetChunk_Processor::AddChunk(CTSE_Chunk_Info& chunk)
{
    m_Chunks.push_back(&chunk);
    SetLockedChunkInfo(chunk);
}


CPSGL_Processor::EProcessResult
CPSGL_GetChunk_Processor::ProcessReplySlow(EPSG_Status status,
                                           const shared_ptr<CPSG_Reply>& reply)
{
    if ( status == EPSG_Status::eError ) {
        return x_Failed(x_Format(status, reply));
    }

    // TODO: check if the chunk is loaded
    return eProcessed;
}


/////////////////////////////////////////////////////////////////////////////
// CPSGL_NA_Processor
/////////////////////////////////////////////////////////////////////////////


CPSGL_NA_Processor::CPSGL_NA_Processor(const TSeq_ids& ids,
                                       CDataSource* data_source,
                                       CPSGCaches* caches,
                                       bool add_wgs_master)
    : CPSGL_Blob_Processor(data_source,
                           caches,
                           add_wgs_master),
      m_Seq_ids(ids)
{
}


CPSGL_NA_Processor::~CPSGL_NA_Processor()
{
}


const char* CPSGL_NA_Processor::GetProcessorName() const
{
    return "CPSGL_NA_Processor";
}


ostream& CPSGL_NA_Processor::PrintProcessorArgs(ostream& out) const
{
    return out << '('<<m_Seq_ids.front()<<')';
}


static
pair<CRef<CTSE_Chunk_Info>, string>
s_CreateNAChunk(const CPSG_NamedAnnotInfo& psg_annot_info)
{
    CRef<CTSE_Chunk_Info> chunk(new CTSE_Chunk_Info(kDelayedMain_ChunkId));
    string name;
    
    unsigned main_count = 0;
    unsigned zoom_count = 0;
    // detailed annot info
    set<string> names;
    for ( auto& annot_info_ref : psg_annot_info.GetId2AnnotInfoList() ) {
        if ( s_GetDebugLevel() >= 8 ) {
            LOG_POST(Info<<"PSG loader: "<<psg_annot_info.GetBlobId().GetId()<<" NA info "
                     <<MSerial_AsnText<<*annot_info_ref);
        }
        const CID2S_Seq_annot_Info& annot_info = *annot_info_ref;
        // create special external annotations blob
        CAnnotName name(annot_info.GetName());
        if ( name.IsNamed() && !ExtractZoomLevel(name.GetName(), 0, 0) ) {
            //setter.GetTSE_LoadLock()->SetName(name);
            names.insert(name.GetName());
            ++main_count;
        }
        else {
            ++zoom_count;
        }
        
        vector<SAnnotTypeSelector> types;
        if ( annot_info.IsSetAlign() ) {
            types.push_back(SAnnotTypeSelector(CSeq_annot::C_Data::e_Align));
        }
        if ( annot_info.IsSetGraph() ) {
            types.push_back(SAnnotTypeSelector(CSeq_annot::C_Data::e_Graph));
        }
        if ( annot_info.IsSetFeat() ) {
            for ( auto feat_type_info_iter : annot_info.GetFeat() ) {
                const CID2S_Feat_type_Info& finfo = *feat_type_info_iter;
                int feat_type = finfo.GetType();
                if ( feat_type == 0 ) {
                    types.push_back(SAnnotTypeSelector
                                    (CSeq_annot::C_Data::e_Seq_table));
                }
                else if ( !finfo.IsSetSubtypes() ) {
                    types.push_back(SAnnotTypeSelector
                                    (CSeqFeatData::E_Choice(feat_type)));
                }
                else {
                    for ( auto feat_subtype : finfo.GetSubtypes() ) {
                        types.push_back(SAnnotTypeSelector
                                        (CSeqFeatData::ESubtype(feat_subtype)));
                    }
                }
            }
        }
        
        CTSE_Chunk_Info::TLocationSet loc;
        CSplitParser::x_ParseLocation(loc, annot_info.GetSeq_loc());
        
        ITERATE ( vector<SAnnotTypeSelector>, it, types ) {
            chunk->x_AddAnnotType(name, *it, loc);
        }
    }
    if ( names.size() == 1 ) {
        name = *names.begin();
    }
    else {
        chunk = null;
    }
    
    if ( s_GetDebugLevel() >= 5 ) {
        LOG_POST(Info<<"PSG loader: TSE "<<psg_annot_info.GetBlobId().GetId()<<
                 " annots: "<<name<<" "<<main_count<<"+"<<zoom_count);
    }
    return make_pair(chunk, name);
}


CPSGL_Processor::EProcessResult
CPSGL_NA_Processor::ProcessItemFast(EPSG_Status status,
                                    const shared_ptr<CPSG_ReplyItem>& item)
{
    switch (item->GetType()) {
    case CPSG_ReplyItem::eNamedAnnotStatus:
        if ( auto annot_status = dynamic_pointer_cast<CPSG_NamedAnnotStatus>(item) ) {
            if ( s_HasFailedStatus(*annot_status) ) {
                return x_Failed("annot status: failed");
            }
            for ( auto& s : annot_status->GetId2AnnotStatusList() ) {
                if ( s.second == EPSG_Status::eNotFound ) {
                    m_Caches->m_NoAnnotInfoCache.Add(make_pair(s.first, m_Seq_ids), true);
                }
            }
        }
        else {
            return x_Failed("annot status: absent");
        }
        return eProcessed;
    case CPSG_ReplyItem::eNamedAnnotInfo:
        return eToNextStage;
    default:
        return CPSGL_Blob_Processor::ProcessItemFast(status, item);
    }
}


CPSGL_Processor::EProcessResult
CPSGL_NA_Processor::ProcessItemSlow(EPSG_Status status,
                                    const shared_ptr<CPSG_ReplyItem>& item)
{
    switch (item->GetType()) {
    case CPSG_ReplyItem::eNamedAnnotInfo:
        if ( auto annot_info = dynamic_pointer_cast<CPSG_NamedAnnotInfo>(item) ) {
            SResult r;
            r.m_NA = annot_info->GetName();
            r.m_Blob_id = annot_info->GetBlobId().GetId();
            auto [ chunk, name ] = s_CreateNAChunk(*annot_info);
            if ( chunk ) {
                // we have annot info to create NA TSE split info
                CRef<CPsgBlobId> dl_blob_id(new CPsgBlobId(r.m_Blob_id));
                if ( !name.empty() ) {
                    dl_blob_id->SetTSEName(name);
                }
                CTSE_LoadLock load_lock = m_DataSource->GetTSE_LoadLock(CBlobIdKey(dl_blob_id));
                if ( !load_lock.IsLoaded() ) {
                    UpdateOMBlobId(load_lock, dl_blob_id);
                    load_lock->GetSplitInfo().AddChunk(*chunk);
                    _ASSERT(load_lock->x_NeedsDelayedMainChunk());
                    load_lock.SetLoaded();
                }
                r.m_TSE_Lock = load_lock;
            }
            else {
                // no annot info, use plain blob id and TSE
                ProcessTSE_Lock(r.m_Blob_id, r.m_TSE_Lock);
            }
            {{
                CFastMutexGuard guard(m_NAProcessorMutex);
                m_AnnotInfos.push_back(make_pair(status, annot_info));
                m_Results.push_back(r);
            }}
        }
        return eProcessed;
    default:
        return CPSGL_Blob_Processor::ProcessItemSlow(status, item);
    }
}


CPSGL_Processor::EProcessResult
CPSGL_NA_Processor::ProcessReplySlow(EPSG_Status status,
                                     const shared_ptr<CPSG_Reply>& reply)
{
    if ( status == EPSG_Status::eError ) {
        return x_Failed(x_Format(status, reply));
    }

    // prepare NA TSE locks
    EProcessResult result = eProcessed;
    for ( auto& r : m_Results ) {
        auto lock_result = ProcessTSE_Lock(r.m_Blob_id, r.m_TSE_Lock);
        if ( lock_result != eProcessed ) {
            result = lock_result;
        }
    }
    return result;
}


CPSGL_Processor::EProcessResult
CPSGL_NA_Processor::ProcessReplyFinal()
{
    _TRACE(Descr()<<": FinalizeResult()");
    for ( auto& r : m_Results ) {
        ProcessTSE_Lock(r.m_Blob_id, r.m_TSE_Lock, eWaitForLock);
    }
    // even if lock cannot be obtained we return eProcessed now to allow recursive getblob request
    return eProcessed;
}


/////////////////////////////////////////////////////////////////////////////
// CPSGL_LocalCDDBlob_Processor
/////////////////////////////////////////////////////////////////////////////


CPSGL_LocalCDDBlob_Processor::CPSGL_LocalCDDBlob_Processor(CTSE_Chunk_Info& cdd_chunk_info,
                                                           const SCDDIds& cdd_ids,
                                                           CDataSource* data_source,
                                                           CPSGCaches* caches,
                                                           bool add_wgs_master)
    : CPSGL_Blob_Processor(data_source,
                           caches,
                           add_wgs_master),
      m_CDDChunkInfo(cdd_chunk_info),
      m_CDDIds(cdd_ids)
{
    _ASSERT(cdd_chunk_info.GetChunkId() == kDelayedMain_ChunkId);
}


CPSGL_LocalCDDBlob_Processor::~CPSGL_LocalCDDBlob_Processor()
{
}


const char* CPSGL_LocalCDDBlob_Processor::GetProcessorName() const
{
    return "CPSGL_LocalCDDBlob_Processor";
}


ostream& CPSGL_LocalCDDBlob_Processor::PrintProcessorArgs(ostream& out) const
{
    return out << '('<<m_CDDIds.gi<<')';
}


CConstRef<CPsgBlobId> CPSGL_LocalCDDBlob_Processor::CreateDLBlobId(STSESlot* /*tse_slot*/)
{
    // we can map PSG blob_id to OM blob id only at the end of reply
    return null;
}


CPSGL_Processor::EProcessResult
CPSGL_LocalCDDBlob_Processor::ProcessItemFast(EPSG_Status status,
                                              const shared_ptr<CPSG_ReplyItem>& item)
{
    switch (item->GetType()) {
    case CPSG_ReplyItem::eNamedAnnotStatus:
        if ( auto annot_status = dynamic_pointer_cast<CPSG_NamedAnnotStatus>(item) ) {
            if ( s_HasFailedStatus(*annot_status) ) {
                return x_Failed("annot status: failed");
            }
        }
        else {
            return x_Failed("annot status: absent");
        }
        break;
    case CPSG_ReplyItem::eNamedAnnotInfo:
        if ( auto annot_info = dynamic_pointer_cast<CPSG_NamedAnnotInfo>(item) ) {
            string na = annot_info->GetName();
            if ( na == kCDDAnnotName ) {
                {{
                    CFastMutexGuard guard(m_CDDProcessorMutex);
                    if ( m_PSG_Blob_id.empty() ) {
                        m_PSG_Blob_id = annot_info->GetBlobId().GetId();
                        // redirect NA TSE (if any) into the CDD chunk
                        SetLockedDelayedChunkInfo(m_PSG_Blob_id, m_CDDChunkInfo);
                        CConstRef<CPsgBlobId> dl_blob_id(&dynamic_cast<const CPsgBlobId&>(*m_CDDChunkInfo.GetBlobId()));
                        SetDLBlobId(m_PSG_Blob_id, dl_blob_id);
                    }
                    else {
                        // TODO - error?
                    }
                }}
                CTSE_Lock tse_lock;
                ProcessTSE_Lock(m_PSG_Blob_id, tse_lock);
            }
        }
        break;
    default:
        return CPSGL_Blob_Processor::ProcessItemFast(status, item);
    }
    return eProcessed;
}


CPSGL_Processor::EProcessResult
CPSGL_LocalCDDBlob_Processor::ProcessReplySlow(EPSG_Status status,
                                               const shared_ptr<CPSG_Reply>& reply)
{
    _TRACE(Descr()<<": ProcessReplySlow("<<int(status)<<")");
    if ( status == EPSG_Status::eError ) {
        return x_Failed(x_Format(status, reply));
    }
    if ( status == EPSG_Status::eNotFound ) {
        x_CreateEmptyLocalCDDEntry(m_DataSource, &m_CDDChunkInfo);
        return eProcessed;
    }
    if ( !m_PSG_Blob_id.empty() ) {
        CTSE_Lock tse_lock;
        ProcessTSE_Lock(m_PSG_Blob_id, tse_lock);
    }
    return eProcessed;
}


END_NAMESPACE(psgl);
END_NAMESPACE(objects);
END_NCBI_NAMESPACE;

#endif // HAVE_PSG_LOADER
