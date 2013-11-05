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
 *
 * ===========================================================================
 *
 *  Author:  Eugene Vasilchenko
 *
 *  File Description: Data reader from ID2
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbi_param.hpp>
#include <corelib/ncbi_system.hpp> // for SleepSec
#include <corelib/request_ctx.hpp>

#include <objtools/data_loaders/genbank/reader_id2_base.hpp>
#include <objtools/data_loaders/genbank/dispatcher.hpp>
#include <objtools/data_loaders/genbank/processors.hpp>

#include <objmgr/objmgr_exception.hpp>
#include <objmgr/annot_selector.hpp>
#include <objmgr/impl/tse_info.hpp>
#include <objmgr/impl/tse_chunk_info.hpp>
#include <objmgr/impl/tse_split_info.hpp>

#include <objtools/data_loaders/genbank/request_result.hpp>
#include <objtools/error_codes.hpp>

#include <corelib/ncbimtx.hpp>

#include <corelib/plugin_manager_impl.hpp>

#include <objects/general/Dbtag.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/id2/id2__.hpp>
#include <objects/seqsplit/seqsplit__.hpp>

#include <serial/iterator.hpp>
#include <serial/serial.hpp>
#include <serial/objistr.hpp>

#include <corelib/plugin_manager_store.hpp>
#include <corelib/ncbi_safe_static.hpp>

#include <iomanip>


#define NCBI_USE_ERRCODE_X   Objtools_Rd_Id2Base

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

NCBI_PARAM_DECL(int, GENBANK, ID2_DEBUG);
NCBI_PARAM_DECL(int, GENBANK, ID2_MAX_CHUNKS_REQUEST_SIZE);
NCBI_PARAM_DECL(int, GENBANK, ID2_MAX_IDS_REQUEST_SIZE);

#ifdef _DEBUG
# define DEFAULT_DEBUG_LEVEL CId2ReaderBase::eTraceError
#else
# define DEFAULT_DEBUG_LEVEL 0
#endif

NCBI_PARAM_DEF_EX(int, GENBANK, ID2_DEBUG, DEFAULT_DEBUG_LEVEL,
                  eParam_NoThread, GENBANK_ID2_DEBUG);
NCBI_PARAM_DEF_EX(int, GENBANK, ID2_MAX_CHUNKS_REQUEST_SIZE, 100,
                  eParam_NoThread, GENBANK_ID2_MAX_CHUNKS_REQUEST_SIZE);
NCBI_PARAM_DEF_EX(int, GENBANK, ID2_MAX_IDS_REQUEST_SIZE, 100,
                  eParam_NoThread, GENBANK_ID2_MAX_IDS_REQUEST_SIZE);

int CId2ReaderBase::GetDebugLevel(void)
{
    static const int s_Value =
        NCBI_PARAM_TYPE(GENBANK, ID2_DEBUG)::GetDefault();
    return s_Value;
}


CId2ReaderBase::CDebugPrinter::CDebugPrinter(CReader::TConn conn,
                                             const char* name)
{
    *this << name << '(' << conn << ')';
    PrintHeader();
}


CId2ReaderBase::CDebugPrinter::CDebugPrinter(const char* name)
{
    *this << name;
    PrintHeader();
}


void CId2ReaderBase::CDebugPrinter::PrintHeader(void)
{
    *this << ": ";
#ifdef NCBI_THREADS
    *this << "T" << CThread::GetSelf() << ' ';
#endif
    *this << CTime(CTime::eCurrent) << ": ";
}


CId2ReaderBase::CDebugPrinter::~CDebugPrinter()
{
    LOG_POST_X(1, rdbuf());
}


// Number of chunks allowed in a single request
// 0 = unlimited request size
// 1 = do not use packets or get-chunks requests
static size_t GetMaxChunksRequestSize(void)
{
    static const size_t s_Value =
        (size_t)
        NCBI_PARAM_TYPE(GENBANK, ID2_MAX_CHUNKS_REQUEST_SIZE)::GetDefault();
    return s_Value;
}


static size_t GetMaxIdsRequestSize(void)
{
    static size_t s_Value =
        (size_t)
        NCBI_PARAM_TYPE(GENBANK, ID2_MAX_IDS_REQUEST_SIZE)::GetDefault();
    return s_Value;
}


static inline
bool
SeparateChunksRequests(size_t max_request_size = GetMaxChunksRequestSize())
{
    return max_request_size == 1;
}


static inline
bool
LimitChunksRequests(size_t max_request_size = GetMaxChunksRequestSize())
{
    return max_request_size > 0;
}


struct SId2BlobInfo
{
    CId2ReaderBase::TContentsMask m_ContentMask;
    typedef list< CRef<CID2S_Seq_annot_Info> > TAnnotInfo;
    TAnnotInfo m_AnnotInfo;
};


struct SId2LoadedSet
{
    typedef pair<int, CReader::TSeqIds> TSeq_idsInfo; // state & ids
    typedef map<string, TSeq_idsInfo> TStringSeq_idsSet;
    typedef map<CSeq_id_Handle, TSeq_idsInfo> TSeq_idSeq_idsSet;
    typedef map<CBlob_id, SId2BlobInfo> TBlob_ids;
    typedef pair<int, TBlob_ids> TBlob_idsInfo; // state & blob-info
    typedef map<CSeq_id_Handle, TBlob_idsInfo> TBlob_idSet;
    typedef map<CBlob_id, CConstRef<CID2_Reply_Data> > TSkeletons;
    typedef map<CBlob_id, int> TBlobStates;

    TStringSeq_idsSet   m_Seq_idsByString;
    TSeq_idSeq_idsSet   m_Seq_ids;
    TBlob_idSet         m_Blob_ids;
    TSkeletons          m_Skeletons;
    TBlobStates         m_BlobStates;
};


CId2ReaderBase::CId2ReaderBase(void)
    : m_RequestSerialNumber(1),
      m_AvoidRequest(0)
{
}


CId2ReaderBase::~CId2ReaderBase(void)
{
}


#define MConnFormat MSerial_AsnBinary


void CId2ReaderBase::x_SetResolve(CID2_Request_Get_Blob_Id& get_blob_id,
                                  const string& seq_id)
{
    get_blob_id.SetSeq_id().SetSeq_id().SetString(seq_id);
    get_blob_id.SetExternal();
}


void CId2ReaderBase::x_SetResolve(CID2_Request_Get_Blob_Id& get_blob_id,
                                  const CSeq_id& seq_id)
{
    //get_blob_id.SetSeq_id().SetSeq_id().SetSeq_id(const_cast<CSeq_id&>(seq_id));
    get_blob_id.SetSeq_id().SetSeq_id().SetSeq_id().Assign(seq_id);
    get_blob_id.SetExternal();
    _ASSERT(get_blob_id.GetSeq_id().GetSeq_id().GetSeq_id().Which() != CSeq_id::e_not_set);
}


void CId2ReaderBase::x_SetDetails(CID2_Get_Blob_Details& /*details*/,
                                  TContentsMask /*mask*/)
{
}


void CId2ReaderBase::x_SetExclude_blobs(CID2_Request_Get_Blob_Info& get_blob_info,
                                        const CSeq_id_Handle& idh,
                                        CReaderRequestResult& result)
{
    if ( SeparateChunksRequests() ) {
        // Minimize size of request rather than response
        return;
    }
    CReaderRequestResult::TLoadedBlob_ids loaded_blob_ids;
    result.GetLoadedBlob_ids(idh, loaded_blob_ids);
    if ( loaded_blob_ids.empty() ) {
        return;
    }
    CID2_Request_Get_Blob_Info::C_Blob_id::C_Resolve::TExclude_blobs&
        exclude_blobs =
        get_blob_info.SetBlob_id().SetResolve().SetExclude_blobs();
    ITERATE(CReaderRequestResult::TLoadedBlob_ids, id, loaded_blob_ids) {
        CRef<CID2_Blob_Id> blob_id(new CID2_Blob_Id);
        x_SetResolve(*blob_id, *id);
        exclude_blobs.push_back(blob_id);
    }
}


CId2ReaderBase::TBlobId CId2ReaderBase::GetBlobId(const CID2_Blob_Id& blob_id)
{
    CBlob_id ret;
    ret.SetSat(blob_id.GetSat());
    ret.SetSubSat(blob_id.GetSub_sat());
    ret.SetSatKey(blob_id.GetSat_key());
    //ret.SetVersion(blob_id.GetVersion());
    return ret;
}


void CId2ReaderBase::x_SetResolve(CID2_Blob_Id& blob_id, const CBlob_id& src)
{
    blob_id.SetSat(src.GetSat());
    blob_id.SetSub_sat(src.GetSubSat());
    blob_id.SetSat_key(src.GetSatKey());
    //blob_id.SetVersion(src.GetVersion());
}


bool CId2ReaderBase::LoadStringSeq_ids(CReaderRequestResult& result,
                                       const string& seq_id)
{
    CLoadLockSeq_ids ids(result, seq_id);
    if ( ids.IsLoaded() ) {
        return true;
    }

    CID2_Request req;
    x_SetResolve(req.SetRequest().SetGet_blob_id(), seq_id);
    x_ProcessRequest(result, req, 0);
    return true;
}


bool CId2ReaderBase::LoadSeq_idSeq_ids(CReaderRequestResult& result,
                                       const CSeq_id_Handle& seq_id)
{
    CLoadLockSeq_ids ids(result, seq_id);
    if ( ids.IsLoaded() ) {
        return true;
    }

    CID2_Request req;
    CID2_Request::C_Request::TGet_seq_id& get_id =
        req.SetRequest().SetGet_seq_id();
    get_id.SetSeq_id().SetSeq_id().Assign(*seq_id.GetSeqId());
    get_id.SetSeq_id_type(CID2_Request_Get_Seq_id::eSeq_id_type_all);
    x_ProcessRequest(result, req, 0);
    return true;
}


bool CId2ReaderBase::LoadSeq_idGi(CReaderRequestResult& result,
                                  const CSeq_id_Handle& seq_id)
{
    CLoadLockSeq_ids ids(result, seq_id);
    if ( ids.IsLoadedGi() ) {
        return true;
    }
    CID2_Request req;
    CID2_Request::C_Request::TGet_seq_id& get_id =
        req.SetRequest().SetGet_seq_id();
    get_id.SetSeq_id().SetSeq_id().Assign(*seq_id.GetSeqId());
    get_id.SetSeq_id_type(CID2_Request_Get_Seq_id::eSeq_id_type_gi);
    x_ProcessRequest(result, req, 0);

    if ( !ids.IsLoadedGi() ) {
        return LoadSeq_idSeq_ids(result, seq_id);
    }

    return true;
}


bool CId2ReaderBase::LoadSeq_idAccVer(CReaderRequestResult& result,
                                      const CSeq_id_Handle& seq_id)
{
    CLoadLockSeq_ids ids(result, seq_id);
    if ( ids.IsLoadedAccVer() ) {
        return true;
    }
    CID2_Request req;
    CID2_Request::C_Request::TGet_seq_id& get_id =
        req.SetRequest().SetGet_seq_id();
    get_id.SetSeq_id().SetSeq_id().Assign(*seq_id.GetSeqId());
    get_id.SetSeq_id_type(CID2_Request_Get_Seq_id::eSeq_id_type_text);
    x_ProcessRequest(result, req, 0);

    if ( !ids.IsLoadedAccVer() ) {
        return LoadSeq_idSeq_ids(result, seq_id);
    }

    return true;
}


bool CId2ReaderBase::LoadSeq_idLabel(CReaderRequestResult& result,
                                     const CSeq_id_Handle& seq_id)
{
    if ( m_AvoidRequest & fAvoidRequest_for_Seq_id_label ) {
        return LoadSeq_idSeq_ids(result, seq_id);
    }

    CLoadLockSeq_ids ids(result, seq_id);
    if ( ids.IsLoadedLabel() ) {
        return true;
    }
    CID2_Request req;
    CID2_Request::C_Request::TGet_seq_id& get_id =
        req.SetRequest().SetGet_seq_id();
    get_id.SetSeq_id().SetSeq_id().Assign(*seq_id.GetSeqId());
    get_id.SetSeq_id_type(CID2_Request_Get_Seq_id::eSeq_id_type_label);
    x_ProcessRequest(result, req, 0);

    if ( !ids.IsLoadedLabel() ) {
        m_AvoidRequest |= fAvoidRequest_for_Seq_id_label;
        return LoadSeq_idSeq_ids(result, seq_id);
    }

    return true;
}


bool CId2ReaderBase::LoadSeq_idTaxId(CReaderRequestResult& result,
                                     const CSeq_id_Handle& seq_id)
{
    if ( m_AvoidRequest & fAvoidRequest_for_Seq_id_taxid ) {
        return CReader::LoadSeq_idTaxId(result, seq_id);
    }

    CLoadLockSeq_ids ids(result, seq_id);
    if ( ids.IsLoadedTaxId() ) {
        return true;
    }
    CID2_Request req;
    CID2_Request::C_Request::TGet_seq_id& get_id =
        req.SetRequest().SetGet_seq_id();
    get_id.SetSeq_id().SetSeq_id().Assign(*seq_id.GetSeqId());
    get_id.SetSeq_id_type(CID2_Request_Get_Seq_id::eSeq_id_type_taxid);
    x_ProcessRequest(result, req, 0);

    if ( !ids.IsLoadedTaxId() ) {
        m_AvoidRequest |= fAvoidRequest_for_Seq_id_taxid;
        return true; // repeat
    }

    return true;
}


bool CId2ReaderBase::LoadAccVers(CReaderRequestResult& result,
                                 const TIds& ids, TLoaded& loaded, TIds& ret)
{
    size_t max_request_size = GetMaxIdsRequestSize();
    if ( max_request_size <= 1 ) {
        return CReader::LoadAccVers(result, ids, loaded, ret);
    }

    size_t count = ids.size();
    vector<AutoPtr<CLoadLockSeq_ids> > locks(count);
    CID2_Request_Packet packet;
    size_t packet_start = 0;
    
    for ( size_t i = 0; i < count; ++i ) {
        if ( loaded[i] ) {
            continue;
        }
        locks[i].reset(new CLoadLockSeq_ids(result, ids[i]));
        if ( (*locks[i]).IsLoadedAccVer() ) {
            ret[i] = (*locks[i])->GetAccVer();
            loaded[i] = true;
            locks[i].reset();
            continue;
        }
        
        CRef<CID2_Request> req(new CID2_Request);
        CID2_Request::C_Request::TGet_seq_id& get_id =
            req->SetRequest().SetGet_seq_id();
        get_id.SetSeq_id().SetSeq_id().Assign(*ids[i].GetSeqId());
        get_id.SetSeq_id_type(CID2_Request_Get_Seq_id::eSeq_id_type_text);
        if ( packet.Set().empty() ) {
            packet_start = i;
        }
        packet.Set().push_back(req);
        if ( packet.Set().size() == max_request_size ) {
            x_ProcessPacket(result, packet, 0);
            size_t count = i+1;
            for ( size_t i = packet_start; i < count; ++i ) {
                if ( loaded[i] ) {
                    continue;
                }
                _ASSERT(locks[i].get());
                if ( (*locks[i]).IsLoadedAccVer() ) {
                    ret[i] = (*locks[i])->GetAccVer();
                    loaded[i] = true;
                    locks[i].reset();
                    continue;
                }
            }
            packet.Set().clear();
        }
    }

    if ( !packet.Set().empty() ) {
        x_ProcessPacket(result, packet, 0);

        for ( size_t i = packet_start; i < count; ++i ) {
            if ( loaded[i] ) {
                continue;
            }
            _ASSERT(locks[i].get());
            if ( (*locks[i]).IsLoadedAccVer() ) {
                ret[i] = (*locks[i])->GetAccVer();
                loaded[i] = true;
                locks[i].reset();
                continue;
            }
        }
    }

    return true;
}


bool CId2ReaderBase::LoadGis(CReaderRequestResult& result,
                             const TIds& ids, TLoaded& loaded, TGis& ret)
{
    size_t max_request_size = GetMaxIdsRequestSize();
    if ( max_request_size <= 1 ) {
        return CReader::LoadGis(result, ids, loaded, ret);
    }

    size_t count = ids.size();
    vector<AutoPtr<CLoadLockSeq_ids> > locks(count);
    CID2_Request_Packet packet;
    size_t packet_start = 0;
    
    for ( size_t i = 0; i < count; ++i ) {
        if ( loaded[i] ) {
            continue;
        }
        locks[i].reset(new CLoadLockSeq_ids(result, ids[i]));
        if ( (*locks[i]).IsLoadedGi() ) {
            ret[i] = (*locks[i])->GetGi();
            loaded[i] = true;
            locks[i].reset();
            continue;
        }
        
        CRef<CID2_Request> req(new CID2_Request);
        CID2_Request::C_Request::TGet_seq_id& get_id =
            req->SetRequest().SetGet_seq_id();
        get_id.SetSeq_id().SetSeq_id().Assign(*ids[i].GetSeqId());
        get_id.SetSeq_id_type(CID2_Request_Get_Seq_id::eSeq_id_type_gi);
        if ( packet.Set().empty() ) {
            packet_start = i;
        }
        packet.Set().push_back(req);
        if ( packet.Set().size() == max_request_size ) {
            x_ProcessPacket(result, packet, 0);
            size_t count = i+1;
            for ( size_t i = packet_start; i < count; ++i ) {
                if ( loaded[i] ) {
                    continue;
                }
                _ASSERT(locks[i].get());
                if ( (*locks[i]).IsLoadedGi() ) {
                    ret[i] = (*locks[i])->GetGi();
                    loaded[i] = true;
                    locks[i].reset();
                    continue;
                }
            }
            packet.Set().clear();
        }
    }

    if ( !packet.Set().empty() ) {
        x_ProcessPacket(result, packet, 0);

        for ( size_t i = packet_start; i < count; ++i ) {
            if ( loaded[i] ) {
                continue;
            }
            _ASSERT(locks[i].get());
            if ( (*locks[i]).IsLoadedGi() ) {
                ret[i] = (*locks[i])->GetGi();
                loaded[i] = true;
                locks[i].reset();
                continue;
            }
        }
    }

    return true;
}


bool CId2ReaderBase::LoadLabels(CReaderRequestResult& result,
                                const TIds& ids, TLoaded& loaded, TLabels& ret)
{
    size_t max_request_size = GetMaxIdsRequestSize();
    if ( max_request_size <= 1 ) {
        return CReader::LoadLabels(result, ids, loaded, ret);
    }

    size_t count = ids.size();
    vector<AutoPtr<CLoadLockSeq_ids> > locks(count);
    CID2_Request_Packet packet;
    size_t packet_start = 0;
    
    for ( size_t i = 0; i < count; ++i ) {
        if ( loaded[i] ) {
            continue;
        }
        locks[i].reset(new CLoadLockSeq_ids(result, ids[i]));
        if ( (*locks[i]).IsLoadedLabel() ) {
            ret[i] = (*locks[i])->GetLabel();
            loaded[i] = true;
            locks[i].reset();
            continue;
        }
        
        CRef<CID2_Request> req(new CID2_Request);
        CID2_Request::C_Request::TGet_seq_id& get_id =
            req->SetRequest().SetGet_seq_id();
        get_id.SetSeq_id().SetSeq_id().Assign(*ids[i].GetSeqId());
        if ( m_AvoidRequest & fAvoidRequest_for_Seq_id_label ) {
            get_id.SetSeq_id_type(CID2_Request_Get_Seq_id::eSeq_id_type_all);
        }
        else {
            get_id.SetSeq_id_type(CID2_Request_Get_Seq_id::eSeq_id_type_label);
        }
        if ( packet.Set().empty() ) {
            packet_start = i;
        }
        packet.Set().push_back(req);
        if ( packet.Set().size() == max_request_size ) {
            x_ProcessPacket(result, packet, 0);
            size_t count = i+1;
            for ( size_t i = packet_start; i < count; ++i ) {
                if ( loaded[i] ) {
                    continue;
                }
                _ASSERT(locks[i].get());
                if ( (*locks[i]).IsLoadedLabel() ) {
                    ret[i] = (*locks[i])->GetLabel();
                    loaded[i] = true;
                    locks[i].reset();
                    continue;
                }
                else {
                    m_AvoidRequest |= fAvoidRequest_for_Seq_id_label;
                    locks[i].reset();
                }
            }
            packet.Set().clear();
        }
    }

    if ( !packet.Set().empty() ) {
        x_ProcessPacket(result, packet, 0);

        for ( size_t i = packet_start; i < count; ++i ) {
            if ( loaded[i] ) {
                continue;
            }
            _ASSERT(locks[i].get());
            if ( (*locks[i]).IsLoadedLabel() ) {
                ret[i] = (*locks[i])->GetLabel();
                loaded[i] = true;
                locks[i].reset();
                continue;
            }
            else {
                m_AvoidRequest |= fAvoidRequest_for_Seq_id_label;
                locks[i].reset();
            }
        }
    }

    return true;
}


bool CId2ReaderBase::LoadTaxIds(CReaderRequestResult& result,
                                const TIds& ids, TLoaded& loaded, TTaxIds& ret)
{
    size_t max_request_size = GetMaxIdsRequestSize();
    if ( max_request_size <= 1 ||
         (m_AvoidRequest & fAvoidRequest_for_Seq_id_taxid) ) {
        return CReader::LoadTaxIds(result, ids, loaded, ret);
    }

    size_t count = ids.size();
    vector<AutoPtr<CLoadLockSeq_ids> > locks(count);
    CID2_Request_Packet packet;
    size_t packet_start = 0;
    
    for ( size_t i = 0; i < count; ++i ) {
        if ( loaded[i] ) {
            continue;
        }
        if ( m_AvoidRequest & fAvoidRequest_for_Seq_id_taxid ) {
            locks.clear();
            return CReader::LoadTaxIds(result, ids, loaded, ret);
        }
        locks[i].reset(new CLoadLockSeq_ids(result, ids[i]));
        if ( (*locks[i]).IsLoadedTaxId() ) {
            ret[i] = (*locks[i])->GetTaxId();
            loaded[i] = true;
            locks[i].reset();
            continue;
        }
        
        CRef<CID2_Request> req(new CID2_Request);
        CID2_Request::C_Request::TGet_seq_id& get_id =
            req->SetRequest().SetGet_seq_id();
        get_id.SetSeq_id().SetSeq_id().Assign(*ids[i].GetSeqId());
        get_id.SetSeq_id_type(CID2_Request_Get_Seq_id::eSeq_id_type_taxid);
        if ( packet.Set().empty() ) {
            packet_start = i;
        }
        packet.Set().push_back(req);
        if ( packet.Set().size() == max_request_size ) {
            x_ProcessPacket(result, packet, 0);
            size_t count = i+1;
            for ( size_t i = packet_start; i < count; ++i ) {
                if ( loaded[i] ) {
                    continue;
                }
                _ASSERT(locks[i].get());
                if ( (*locks[i]).IsLoadedTaxId() ) {
                    ret[i] = (*locks[i])->GetTaxId();
                    loaded[i] = true;
                    locks[i].reset();
                    continue;
                }
                else {
                    m_AvoidRequest |= fAvoidRequest_for_Seq_id_taxid;
                    locks[i].reset();
                }
            }
            packet.Set().clear();
        }
    }

    if ( !packet.Set().empty() ) {
        x_ProcessPacket(result, packet, 0);

        for ( size_t i = packet_start; i < count; ++i ) {
            if ( loaded[i] ) {
                continue;
            }
            _ASSERT(locks[i].get());
            if ( (*locks[i]).IsLoadedTaxId() ) {
                ret[i] = (*locks[i])->GetTaxId();
                loaded[i] = true;
                locks[i].reset();
                continue;
            }
            else {
                m_AvoidRequest |= fAvoidRequest_for_Seq_id_taxid;
                locks[i].reset();
            }
        }
    }

    return true;
}


bool CId2ReaderBase::LoadSeq_idBlob_ids(CReaderRequestResult& result,
                                        const CSeq_id_Handle& seq_id,
                                        const SAnnotSelector* sel)
{
    CLoadLockBlob_ids ids(result, seq_id, sel);
    if ( ids.IsLoaded() ) {
        return true;
    }

    CID2_Request req;
    CID2_Request_Get_Blob_Id& get_blob_id = req.SetRequest().SetGet_blob_id();
    x_SetResolve(get_blob_id, *seq_id.GetSeqId());
    if ( sel && sel->IsIncludedAnyNamedAnnotAccession() ) {
        CID2_Request_Get_Blob_Id::TSources& srcs = get_blob_id.SetSources();
        ITERATE ( SAnnotSelector::TNamedAnnotAccessions, it,
                  sel->GetNamedAnnotAccessions() ) {
            srcs.push_back(it->first);
        }
    }
    x_ProcessRequest(result, req, sel);
    return true;
}


bool CId2ReaderBase::LoadBlobVersion(CReaderRequestResult& result,
                                     const CBlob_id& blob_id)
{
    CID2_Request req;
    CID2_Request_Get_Blob_Info& req2 = req.SetRequest().SetGet_blob_info();
    x_SetResolve(req2.SetBlob_id().SetBlob_id(), blob_id);
    x_ProcessRequest(result, req, 0);
    return true;
}


bool CId2ReaderBase::LoadBlobs(CReaderRequestResult& result,
                               const string& seq_id,
                               TContentsMask /*mask*/,
                               const SAnnotSelector* /*sel*/)
{
    if ( m_AvoidRequest & fAvoidRequest_nested_get_blob_info ) {
        return LoadStringSeq_ids(result, seq_id);
    }
    CLoadLockSeq_ids ids(result, seq_id);
    if ( ids.IsLoaded() ) {
        return true;
    }

    return LoadStringSeq_ids(result, seq_id);
    /*
    CID2_Request req;
    CID2_Request_Get_Blob_Info& req2 = req.SetRequest().SetGet_blob_info();
    x_SetResolve(req2.SetBlob_id().SetResolve().SetRequest(), seq_id);
    x_SetDetails(req2.SetGet_data(), mask, sel);
    x_ProcessRequest(result, req);
    */
}


bool CId2ReaderBase::LoadBlobs(CReaderRequestResult& result,
                               const CSeq_id_Handle& seq_id,
                               TContentsMask mask,
                               const SAnnotSelector* sel)
{
    CLoadLockBlob_ids ids(result, seq_id, sel);
    if ( !ids.IsLoaded() ) {
        if ( (m_AvoidRequest & fAvoidRequest_nested_get_blob_info) ||
             !(mask & fBlobHasAllLocal) ) {
            if ( !LoadSeq_idBlob_ids(result, seq_id, sel) ) {
                return false;
            }
        }
    }
    if ( ids.IsLoaded() ) {
        // shortcut - we know Seq-id -> Blob-id resolution
        return LoadBlobs(result, ids, mask, sel);
    }
    else {
        CID2_Request req;
        CID2_Request_Get_Blob_Info& req2 = req.SetRequest().SetGet_blob_info();
        x_SetResolve(req2.SetBlob_id().SetResolve().SetRequest(),
                     *seq_id.GetSeqId());
        x_SetDetails(req2.SetGet_data(), mask);
        x_SetExclude_blobs(req2, seq_id, result);
        x_ProcessRequest(result, req, sel);
        return true;
    }
}


bool CId2ReaderBase::LoadBlobs(CReaderRequestResult& result,
                               CLoadLockBlob_ids blobs,
                               TContentsMask mask,
                               const SAnnotSelector* sel)
{
    size_t max_request_size = GetMaxChunksRequestSize();
    CConn conn(result, this);
    CID2_Request_Packet packet;
    CFixedBlob_ids blob_ids = blobs->GetBlob_ids();
    ITERATE ( CFixedBlob_ids, it, blob_ids ) {
        const CBlob_Info& info = *it;
        const CBlob_id& blob_id = *info.GetBlob_id();
        if ( !info.Matches(mask, sel) ) {
            continue; // skip this blob
        }
        if ( result.IsBlobLoaded(blob_id) ) {
            continue;
        }

        if ( info.IsSetAnnotInfo() ) {
            CLoadLockBlob blob(result, blob_id);
            if ( !blob.IsLoaded() ) {
                CProcessor_AnnotInfo::LoadBlob(result, info);
            }
            _ASSERT(blob.IsLoaded());
            continue;
        }
        
        if ( CProcessor_ExtAnnot::IsExtAnnot(blob_id) ) {
            const int chunk_id = CProcessor::kMain_ChunkId;
            CLoadLockBlob blob(result, blob_id);
            if ( !CProcessor::IsLoaded(result, blob_id, chunk_id, blob) ) {
                dynamic_cast<const CProcessor_ExtAnnot&>
                    (m_Dispatcher->GetProcessor(CProcessor::eType_ExtAnnot))
                    .Process(result, blob_id, CProcessor::kMain_ChunkId);
            }
            _ASSERT(CProcessor::IsLoaded(result, blob_id, chunk_id, blob));
            continue;
        }

        CRef<CID2_Request> req(new CID2_Request);
        packet.Set().push_back(req);
        CID2_Request_Get_Blob_Info& req2 =
            req->SetRequest().SetGet_blob_info();
        x_SetResolve(req2.SetBlob_id().SetBlob_id(), blob_id);
        x_SetDetails(req2.SetGet_data(), mask);
        if ( LimitChunksRequests(max_request_size) &&
             packet.Get().size() >= max_request_size ) {
            x_ProcessPacket(result, packet, sel);
            packet.Set().clear();
        }
    }
    if ( !packet.Get().empty() ) {
        x_ProcessPacket(result, packet, sel);
    }
    conn.Release();
    return true;
}


bool CId2ReaderBase::LoadBlob(CReaderRequestResult& result,
                              const TBlobId& blob_id)
{
    CConn conn(result, this);
    CLoadLockBlob blob(result, blob_id);
    if ( blob.IsLoaded() ) {
        conn.Release();
        return true;
    }

    if ( CProcessor_ExtAnnot::IsExtAnnot(blob_id) ) {
        conn.Release();
        const int chunk_id = CProcessor::kMain_ChunkId;
        if ( !CProcessor::IsLoaded(result, blob_id, chunk_id, blob) ) {
            dynamic_cast<const CProcessor_ExtAnnot&>
                (m_Dispatcher->GetProcessor(CProcessor::eType_ExtAnnot))
                .Process(result, blob_id, chunk_id);
        }
        _ASSERT(CProcessor::IsLoaded(result, blob_id, chunk_id, blob));
        return true;
    }

    CID2_Request req;
    CID2_Request_Get_Blob_Info& req2 = req.SetRequest().SetGet_blob_info();
    x_SetResolve(req2.SetBlob_id().SetBlob_id(), blob_id);
    req2.SetGet_data();
    x_ProcessRequest(result, req, 0);
    return true;
}


bool CId2ReaderBase::LoadChunk(CReaderRequestResult& result,
                               const CBlob_id& blob_id,
                               TChunkId chunk_id)
{
    CLoadLockBlob blob(result, blob_id);
    _ASSERT(blob);
    CTSE_Chunk_Info& chunk_info = blob->GetSplitInfo().GetChunk(chunk_id);
    if ( chunk_info.IsLoaded() ) {
        return true;
    }
    CInitGuard init(chunk_info, result);
    if ( !init ) {
        _ASSERT(chunk_info.IsLoaded());
        return true;
    }

    CID2_Request req;
    if ( chunk_id == CProcessor::kDelayedMain_ChunkId ) {
        CID2_Request_Get_Blob_Info& req2 = req.SetRequest().SetGet_blob_info();
        x_SetResolve(req2.SetBlob_id().SetBlob_id(), blob_id);
        req2.SetGet_data();
        x_ProcessRequest(result, req, 0);
        if ( !chunk_info.IsLoaded() ) {
            ERR_POST_X(2, "ExtAnnot chunk is not loaded: "<<blob_id);
            chunk_info.SetLoaded();
        }
    }
    else {
        CID2S_Request_Get_Chunks& req2 = req.SetRequest().SetGet_chunks();
        x_SetResolve(req2.SetBlob_id(), blob_id);

        if ( blob->GetBlobVersion() > 0 ) {
            req2.SetBlob_id().SetVersion(blob->GetBlobVersion());
        }
        req2.SetSplit_version(blob->GetSplitInfo().GetSplitVersion());
        req2.SetChunks().push_back(CID2S_Chunk_Id(chunk_id));
        x_ProcessRequest(result, req, 0);
    }
    //_ASSERT(chunk_info.IsLoaded());
    return true;
}


void LoadedChunksPacket(CID2_Request_Packet& packet,
                        vector<CTSE_Chunk_Info*>& chunks,
                        const CBlob_id& blob_id,
                        vector< AutoPtr<CInitGuard> >& guards)
{
    NON_CONST_ITERATE(vector<CTSE_Chunk_Info*>, it, chunks) {
        if ( !(*it)->IsLoaded() ) {
            ERR_POST_X(3, "ExtAnnot chunk is not loaded: " << blob_id);
            (*it)->SetLoaded();
        }
    }
    packet.Set().clear();
    chunks.clear();
    guards.clear();
}


bool CId2ReaderBase::LoadChunks(CReaderRequestResult& result,
                                const CBlob_id& blob_id,
                                const TChunkIds& chunk_ids)
{
    if ( chunk_ids.size() == 1 ) {
        return LoadChunk(result, blob_id, chunk_ids[0]);
    }
    size_t max_request_size = GetMaxChunksRequestSize();
    if ( SeparateChunksRequests(max_request_size) ) {
        return CReader::LoadChunks(result, blob_id, chunk_ids);
    }
    CLoadLockBlob blob(result, blob_id);
    _ASSERT(blob);

    CID2_Request_Packet packet;

    CRef<CID2_Request> chunks_req(new CID2_Request);
    CID2S_Request_Get_Chunks& get_chunks =
        chunks_req->SetRequest().SetGet_chunks();

    x_SetResolve(get_chunks.SetBlob_id(), blob_id);
    if ( blob->GetBlobVersion() > 0 ) {
        get_chunks.SetBlob_id().SetVersion(blob->GetBlobVersion());
    }
    get_chunks.SetSplit_version(blob->GetSplitInfo().GetSplitVersion());
    CID2S_Request_Get_Chunks::TChunks& chunks = get_chunks.SetChunks();

    vector< AutoPtr<CInitGuard> > guards;
    vector< AutoPtr<CInitGuard> > ext_guards;
    vector<CTSE_Chunk_Info*> ext_chunks;
    ITERATE(TChunkIds, id, chunk_ids) {
        CTSE_Chunk_Info& chunk_info = blob->GetSplitInfo().GetChunk(*id);
        if ( chunk_info.IsLoaded() ) {
            continue;
        }
        if ( *id == CProcessor::kDelayedMain_ChunkId ) {
            AutoPtr<CInitGuard> init(new CInitGuard(chunk_info, result));
            if ( !init ) {
                _ASSERT(chunk_info.IsLoaded());
                continue;
            }
            ext_guards.push_back(init);
            CRef<CID2_Request> ext_req(new CID2_Request);
            CID2_Request_Get_Blob_Info& ext_req_data =
                ext_req->SetRequest().SetGet_blob_info();
            x_SetResolve(ext_req_data.SetBlob_id().SetBlob_id(), blob_id);
            ext_req_data.SetGet_data();
            packet.Set().push_back(ext_req);
            ext_chunks.push_back(&chunk_info);
            if ( LimitChunksRequests(max_request_size) &&
                 packet.Get().size() >= max_request_size ) {
                // Request collected chunks
                x_ProcessPacket(result, packet, 0);
                LoadedChunksPacket(packet, ext_chunks, blob_id, ext_guards);
            }
        }
        else {
            AutoPtr<CInitGuard> init(new CInitGuard(chunk_info, result));
            if ( !init ) {
                _ASSERT(chunk_info.IsLoaded());
                continue;
            }
            guards.push_back(init);
            chunks.push_back(CID2S_Chunk_Id(*id));
            if ( LimitChunksRequests(max_request_size) &&
                 chunks.size() >= max_request_size ) {
                // Process collected chunks
                x_ProcessRequest(result, *chunks_req, 0);
                guards.clear();
                chunks.clear();
            }
        }
    }
    if ( !chunks.empty() ) {
        if ( LimitChunksRequests(max_request_size) &&
             packet.Get().size() + chunks.size() > max_request_size ) {
            // process chunks separately from packet
            x_ProcessRequest(result, *chunks_req, 0);
        }
        else {
            // Use the same packet for chunks
            packet.Set().push_back(chunks_req);
        }
    }
    if ( !packet.Get().empty() ) {
        x_ProcessPacket(result, packet, 0);
        LoadedChunksPacket(packet, ext_chunks, blob_id, ext_guards);
    }
    return true;
}


bool CId2ReaderBase::x_LoadSeq_idBlob_idsSet(CReaderRequestResult& result,
                                             const TSeqIds& seq_ids)
{
    size_t max_request_size = GetMaxChunksRequestSize();
    if ( SeparateChunksRequests(max_request_size) ) {
        ITERATE(TSeqIds, id, seq_ids) {
            LoadSeq_idBlob_ids(result, *id, 0);
        }
        return true;
    }
    CID2_Request_Packet packet;
    ITERATE(TSeqIds, id, seq_ids) {
        CLoadLockBlob_ids ids(result, *id, 0);
        if ( ids.IsLoaded() ) {
            continue;
        }

        CRef<CID2_Request> req(new CID2_Request);
        x_SetResolve(req->SetRequest().SetGet_blob_id(), *id->GetSeqId());
        packet.Set().push_back(req);
        if ( LimitChunksRequests(max_request_size) &&
             packet.Get().size() >= max_request_size ) {
            // Request collected chunks
            x_ProcessPacket(result, packet, 0);
            packet.Set().clear();
        }
    }
    if ( !packet.Get().empty() ) {
        x_ProcessPacket(result, packet, 0);
    }
    return true;
}


bool CId2ReaderBase::LoadBlobSet(CReaderRequestResult& result,
                                 const TSeqIds& seq_ids)
{
    size_t max_request_size = GetMaxChunksRequestSize();
    if ( SeparateChunksRequests(max_request_size) ) {
        return CReader::LoadBlobSet(result, seq_ids);
    }

    bool loaded_blob_ids = false;
    if (m_AvoidRequest & fAvoidRequest_nested_get_blob_info) {
        if ( !x_LoadSeq_idBlob_idsSet(result, seq_ids) ) {
            return false;
        }
        loaded_blob_ids = true;
    }

    set<CBlob_id> load_blob_ids;
    CID2_Request_Packet packet;
    ITERATE(TSeqIds, id, seq_ids) {
        if ( !loaded_blob_ids &&
             m_AvoidRequest & fAvoidRequest_nested_get_blob_info ) {
            if ( !x_LoadSeq_idBlob_idsSet(result, seq_ids) ) {
                return false;
            }
            loaded_blob_ids = true;
        }
        CLoadLockBlob_ids ids(result, *id, 0);
        if ( ids.IsLoaded() ) {
            // shortcut - we know Seq-id -> Blob-id resolution
            CFixedBlob_ids blob_ids = ids->GetBlob_ids();
            ITERATE ( CFixedBlob_ids, it, blob_ids ) {
                const CBlob_Info& info = *it;
                const CBlob_id& blob_id = *info.GetBlob_id();
                if ( (info.GetContentsMask() & fBlobHasCore) == 0 ) {
                    continue; // skip this blob
                }
                if ( result.IsBlobLoaded(blob_id) ) {
                    continue;
                }
                if ( !load_blob_ids.insert(blob_id).second ) {
                    continue;
                }
                CRef<CID2_Request> req(new CID2_Request);
                CID2_Request_Get_Blob_Info& req2 =
                    req->SetRequest().SetGet_blob_info();
                x_SetResolve(req2.SetBlob_id().SetBlob_id(), blob_id);
                x_SetDetails(req2.SetGet_data(), fBlobHasCore);
                packet.Set().push_back(req);
                if ( LimitChunksRequests(max_request_size) &&
                     packet.Get().size() >= max_request_size ) {
                    x_ProcessPacket(result, packet, 0);
                    packet.Set().clear();
                }
            }
        }
        else {
            CRef<CID2_Request> req(new CID2_Request);
            CID2_Request_Get_Blob_Info& req2 =
                req->SetRequest().SetGet_blob_info();
            x_SetResolve(req2.SetBlob_id().SetResolve().SetRequest(),
                         *id->GetSeqId());
            x_SetDetails(req2.SetGet_data(), fBlobHasCore);
            x_SetExclude_blobs(req2, *id, result);
            packet.Set().push_back(req);
            if ( LimitChunksRequests(max_request_size) &&
                 packet.Get().size() >= max_request_size ) {
                x_ProcessPacket(result, packet, 0);
                packet.Set().clear();
            }
        }
    }
    if ( packet.Get().empty() ) {
        return loaded_blob_ids;
    }
    x_ProcessPacket(result, packet, 0);
    return true;
}


void CId2ReaderBase::x_ProcessRequest(CReaderRequestResult& result,
                                      CID2_Request& req,
                                      const SAnnotSelector* sel)
{
    CID2_Request_Packet packet;
    packet.Set().push_back(Ref(&req));
    x_ProcessPacket(result, packet, sel);
}


void CId2ReaderBase::x_SetContextData(CID2_Request& request)
{
    if ( request.GetRequest().IsInit() ) {
        CRef<CID2_Param> param(new CID2_Param);
        param->SetName("log:client_name");
        param->SetValue().push_back(GetDiagContext().GetAppName());
        request.SetParams().Set().push_back(param);
    }
    CRequestContext& rctx = CDiagContext::GetRequestContext();
    if ( rctx.IsSetSessionID() ) {
        CRef<CID2_Param> param(new CID2_Param);
        param->SetName("session_id");
        param->SetValue().push_back(rctx.GetSessionID());
        request.SetParams().Set().push_back(param);
    }
    if ( rctx.IsSetHitID() ) {
        CRef<CID2_Param> param(new CID2_Param);
        param->SetName("log:ncbi_phid");
        param->SetValue().push_back(rctx.GetHitID());
        request.SetParams().Set().push_back(param);
    }
}


void CId2ReaderBase::x_ProcessPacket(CReaderRequestResult& result,
                                     CID2_Request_Packet& packet,
                                     const SAnnotSelector* sel)
{
    // Fill request context information
    if ( !packet.Get().empty() ) {
        x_SetContextData(*packet.Set().front());
    }

    // prepare serial nums and result state
    size_t request_count = packet.Get().size();
    int start_serial_num =
        m_RequestSerialNumber.Add(request_count) - request_count;
    {{
        int cur_serial_num = start_serial_num;
        NON_CONST_ITERATE ( CID2_Request_Packet::Tdata, it, packet.Set() ) {
            (*it)->SetSerial_number(cur_serial_num++);
        }
    }}
    vector<char> done(request_count);
    vector<SId2LoadedSet> loaded_sets(request_count);

    CConn conn(result, this);
    CRef<CID2_Reply> reply;
    try {
        // send request
        {{
            if ( GetDebugLevel() >= eTraceConn ) {
                CDebugPrinter s(conn, "CId2Reader");
                s << "Sending";
                if ( GetDebugLevel() >= eTraceASN ) {
                    s << ": " << MSerial_AsnText << packet;
                }
                else {
                    s << " ID2-Request-Packet";
                }
                s << "...";
            }
            try {
                x_SendPacket(conn, packet);
            }
            catch ( CException& exc ) {
                NCBI_RETHROW(exc, CLoaderException, eConnectionFailed,
                             "failed to send request: "+
                             x_ConnDescription(conn));
            }
            if ( GetDebugLevel() >= eTraceConn ) {
                CDebugPrinter s(conn, "CId2Reader");
                s << "Sent ID2-Request-Packet.";
            }
        }}

        // process replies
        size_t remaining_count = request_count;
        while ( remaining_count > 0 ) {
            reply.Reset(new CID2_Reply);
            if ( GetDebugLevel() >= eTraceConn ) {
                CDebugPrinter s(conn, "CId2Reader");
                s << "Receiving ID2-Reply...";
            }
            try {
                x_ReceiveReply(conn, *reply);
            }
            catch ( CException& exc ) {
                NCBI_RETHROW(exc, CLoaderException, eConnectionFailed,
                             "reply deserialization failed: "+
                             x_ConnDescription(conn));
            }
            if ( GetDebugLevel() >= eTraceConn   ) {
                CDebugPrinter s(conn, "CId2Reader");
                s << "Received";
                if ( GetDebugLevel() >= eTraceASN ) {
                    if ( GetDebugLevel() >= eTraceBlobData ) {
                        s << ": " << MSerial_AsnText << *reply;
                    }
                    else {
                        CTypeIterator<CID2_Reply_Data> iter = Begin(*reply);
                        if ( iter && iter->IsSetData() ) {
                            CID2_Reply_Data::TData save;
                            save.swap(iter->SetData());
                            size_t size = 0, count = 0, max_chunk = 0;
                            ITERATE ( CID2_Reply_Data::TData, i, save ) {
                                ++count;
                                size_t chunk = (*i)->size();
                                size += chunk;
                                max_chunk = max(max_chunk, chunk);
                            }
                            s << ": " << MSerial_AsnText << *reply <<
                                "Data: " << size << " bytes in " <<
                                count << " chunks with " <<
                                max_chunk << " bytes in chunk max";
                            save.swap(iter->SetData());
                        }
                        else {
                            s << ": " << MSerial_AsnText << *reply;
                        }
                    }
                }
                else {
                    s << " ID2-Reply.";
                }
            }
            if ( GetDebugLevel() >= eTraceBlob ) {
                for ( CTypeConstIterator<CID2_Reply_Data> it(Begin(*reply));
                      it; ++it ) {
                    if ( it->IsSetData() ) {
                        try {
                            CProcessor_ID2::DumpDataAsText(*it, NcbiCout);
                        }
                        catch ( CException& exc ) {
                            ERR_POST_X(1, "Exception while dumping data: "
                                       <<exc);
                        }
                    }
                }
            }
            size_t num = reply->GetSerial_number() - start_serial_num;
            if ( reply->IsSetDiscard() ) {
                // discard whole reply for now
                continue;
            }
            if ( num >= request_count || done[num] ) {
                // unknown serial num - bad reply
                if ( TErrorFlags error = x_GetError(result, *reply) ) {
                    if ( error & fError_inactivity_timeout ) {
                        conn.Restart();
                        NCBI_THROW_FMT(CLoaderException, eRepeatAgain,
                                       "CId2ReaderBase: connection timed out"<<
                                       x_ConnDescription(conn));
                    }
                    if ( error & fError_bad_connection ) {
                        NCBI_THROW_FMT(CLoaderException, eConnectionFailed,
                                       "CId2ReaderBase: connection failed"<<
                                       x_ConnDescription(conn));
                    }
                }
                else if ( reply->GetReply().IsEmpty() ) {
                    ERR_POST_X(8, "CId2ReaderBase: bad reply serial number: "<<
                               x_ConnDescription(conn));
                    continue;
                }
                NCBI_THROW_FMT(CLoaderException, eOtherError,
                               "CId2ReaderBase: bad reply serial number: "<<
                               x_ConnDescription(conn));
            }
            try {
                x_ProcessReply(result, loaded_sets[num], *reply);
            }
            catch ( CException& exc ) {
                NCBI_RETHROW(exc, CLoaderException, eOtherError,
                             "CId2ReaderBase: failed to process reply: "+
                             x_ConnDescription(conn));
            }
            if ( reply->IsSetEnd_of_reply() ) {
                done[num] = true;
                x_UpdateLoadedSet(result, loaded_sets[num], sel);
                --remaining_count;
            }
        }
        reply.Reset();
        if ( conn.IsAllocated() ) {
            x_EndOfPacket(conn);
        }
    }
    catch ( exception& /*rethrown*/ ) {
        if ( GetDebugLevel() >= eTraceError ) {
            CDebugPrinter s(conn, "CId2Reader");
            s << "Error processing request: " << MSerial_AsnText << packet;
            if ( reply &&
                 (reply->IsSetSerial_number() ||
                  reply->IsSetParams() ||
                  reply->IsSetError() ||
                  reply->IsSetEnd_of_reply() ||
                  reply->IsSetReply()) ) {
                try {
                    s << "Last reply: " << MSerial_AsnText << *reply;
                }
                catch ( exception& /*ignored*/ ) {
                }
            }
        }
        throw;
    }
    conn.Release();
}


void CId2ReaderBase::x_ReceiveReply(CObjectIStream& stream,
                                    TConn /*conn*/,
                                    CID2_Reply& reply)
{
    stream >> reply;
}


void CId2ReaderBase::x_EndOfPacket(TConn /*conn*/)
{
    // do nothing by default
}


void CId2ReaderBase::x_UpdateLoadedSet(CReaderRequestResult& result,
                                       SId2LoadedSet& data,
                                       const SAnnotSelector* sel)
{
    NON_CONST_ITERATE ( SId2LoadedSet::TStringSeq_idsSet, it,
                        data.m_Seq_idsByString ) {
        SetAndSaveStringSeq_ids(result, it->first, it->second.first,
                                CFixedSeq_ids(eTakeOwnership, it->second.second));
    }
    data.m_Seq_idsByString.clear();
    NON_CONST_ITERATE ( SId2LoadedSet::TSeq_idSeq_idsSet, it,
                        data.m_Seq_ids ) {
        SetAndSaveSeq_idSeq_ids(result, it->first, it->second.first,
                                CFixedSeq_ids(eTakeOwnership, it->second.second));
    }
    data.m_Seq_ids.clear();
    ITERATE ( SId2LoadedSet::TBlob_idSet, it, data.m_Blob_ids ) {
        CLoadLockBlob_ids ids(result, it->first, sel);
        if ( ids.IsLoaded() ) {
            continue;
        }
        int state = it->second.first;
        TBlobIds blob_ids;
        ITERATE ( SId2LoadedSet::TBlob_ids, it2, it->second.second ) {
            CConstRef<CBlob_id> blob_id(new CBlob_id(it2->first));
            CBlob_Info blob_info(blob_id, it2->second.m_ContentMask);
            const SId2BlobInfo::TAnnotInfo& ainfos = it2->second.m_AnnotInfo;
            CRef<CBlob_Annot_Info> blob_annot_info;
            ITERATE ( SId2BlobInfo::TAnnotInfo, it3, ainfos ) {
                const CID2S_Seq_annot_Info& annot_info = **it3;
                if ( !blob_annot_info ) {
                    blob_annot_info = new CBlob_Annot_Info;
                }
                if ( (it2->second.m_ContentMask & fBlobHasNamedAnnot) &&
                     annot_info.IsSetName() ) {
                    blob_annot_info->AddNamedAnnotName(annot_info.GetName());
                }
                if ( annot_info.IsSetName() &&
                     annot_info.IsSetSeq_loc() &&
                     (annot_info.IsSetAlign() ||
                      annot_info.IsSetGraph() ||
                      annot_info.IsSetFeat()) ) {
                    // complete annot info
                    blob_annot_info->AddAnnotInfo(annot_info);
                }
                // Heuristics to determine incorrect annot info records.
                if ( annot_info.IsSetAlign() && annot_info.IsSetGraph() ) {
                    blob_annot_info.Reset();
                    break;
                }
            }
            if ( blob_annot_info &&
                 !(blob_annot_info->GetAnnotInfo().empty() &&
                   blob_annot_info->GetNamedAnnotNames().empty()) ) {
                blob_info.SetAnnotInfo(blob_annot_info);
            }
            blob_ids.push_back(blob_info);
        }
        SetAndSaveSeq_idBlob_ids(result, it->first, sel, ids, state,
                                 CFixedBlob_ids(eTakeOwnership, blob_ids));
    }
}


CId2ReaderBase::TErrorFlags
CId2ReaderBase::x_GetError(CReaderRequestResult& result,
                           const CID2_Error& error)
{
    TErrorFlags error_flags = 0;
    switch ( error.GetSeverity() ) {
    case CID2_Error::eSeverity_warning:
        error_flags |= fError_warning;
        break;
    case CID2_Error::eSeverity_failed_command:
        error_flags |= fError_bad_command;
        break;
    case CID2_Error::eSeverity_failed_connection:
        error_flags |= fError_bad_connection;
        if ( error.IsSetMessage() &&
             NStr::FindNoCase(error.GetMessage(), "timed") &&
             NStr::FindNoCase(error.GetMessage(), "out") ) {
            error_flags |= fError_inactivity_timeout;
        }
        break;
    case CID2_Error::eSeverity_failed_server:
        error_flags |= fError_bad_connection;
        break;
    case CID2_Error::eSeverity_no_data:
        error_flags |= fError_no_data;
        break;
    case CID2_Error::eSeverity_restricted_data:
        error_flags |= fError_no_data;
        break;
    case CID2_Error::eSeverity_unsupported_command:
        m_AvoidRequest |= fAvoidRequest_nested_get_blob_info;
        error_flags |= fError_bad_command;
        break;
    case CID2_Error::eSeverity_invalid_arguments:
        error_flags |= fError_bad_command;
        break;
    }
    if ( error.IsSetRetry_delay() ) {
        result.AddRetryDelay(error.GetRetry_delay());
    }
    return error_flags;
}


CId2ReaderBase::TErrorFlags
CId2ReaderBase::x_GetMessageError(const CID2_Error& error)
{
    TErrorFlags error_flags = 0;
    switch ( error.GetSeverity() ) {
    case CID2_Error::eSeverity_warning:
        error_flags |= fError_warning;
        if ( error.IsSetMessage() ) {
            if ( NStr::FindNoCase(error.GetMessage(), "obsolete") != NPOS ) {
                error_flags |= fError_warning_dead;
            }
            if ( NStr::FindNoCase(error.GetMessage(), "removed") != NPOS ) {
                error_flags |= fError_warning_suppressed;
            }
            if ( NStr::FindNoCase(error.GetMessage(), "suppressed") != NPOS ) {
                error_flags |= fError_warning_suppressed;
            }
        }
        break;
    case CID2_Error::eSeverity_failed_command:
        error_flags |= fError_bad_command;
        break;
    case CID2_Error::eSeverity_failed_connection:
        error_flags |= fError_bad_connection;
        break;
    case CID2_Error::eSeverity_failed_server:
        error_flags |= fError_bad_connection;
        break;
    case CID2_Error::eSeverity_no_data:
        error_flags |= fError_no_data;
        break;
    case CID2_Error::eSeverity_restricted_data:
        error_flags |= fError_no_data;
        if ( error.IsSetMessage() &&
             (NStr::FindNoCase(error.GetMessage(), "withdrawn") != NPOS ||
              NStr::FindNoCase(error.GetMessage(), "removed") != NPOS) ) {
            error_flags |= fError_withdrawn;
        }
        else {
            error_flags |= fError_restricted;
        }
        break;
    case CID2_Error::eSeverity_unsupported_command:
        m_AvoidRequest |= fAvoidRequest_nested_get_blob_info;
        error_flags |= fError_bad_command;
        break;
    case CID2_Error::eSeverity_invalid_arguments:
        error_flags |= fError_bad_command;
        break;
    }
    return error_flags;
}


CId2ReaderBase::TErrorFlags
CId2ReaderBase::x_GetError(CReaderRequestResult& result,
                           const CID2_Reply& reply)
{
    TErrorFlags errors = 0;
    if ( reply.IsSetError() ) {
        ITERATE ( CID2_Reply::TError, it, reply.GetError() ) {
            errors |= x_GetError(result, **it);
        }
    }
    return errors;
}


CId2ReaderBase::TErrorFlags
CId2ReaderBase::x_GetMessageError(const CID2_Reply& reply)
{
    TErrorFlags errors = 0;
    if ( reply.IsSetError() ) {
        ITERATE ( CID2_Reply::TError, it, reply.GetError() ) {
            errors |= x_GetMessageError(**it);
        }
    }
    return errors;
}


CReader::TBlobState
CId2ReaderBase::x_GetBlobState(const CID2_Reply& reply,
                               TErrorFlags* errors_ptr)
{
    TBlobState blob_state = 0;
    TErrorFlags errors = x_GetMessageError(reply);
    if ( errors_ptr ) {
        *errors_ptr = errors;
    }
    if ( errors & fError_no_data ) {
        blob_state |= CBioseq_Handle::fState_no_data;
        if ( errors & fError_restricted ) {
            blob_state |= CBioseq_Handle::fState_confidential;
        }
        if ( errors & fError_withdrawn ) {
            blob_state |= CBioseq_Handle::fState_withdrawn;
        }
    }
    if ( errors & fError_warning_dead ) {
        blob_state |= CBioseq_Handle::fState_dead;
    }
    if ( errors & fError_warning_suppressed ) {
        blob_state |= CBioseq_Handle::fState_suppress_perm;
    }
    return blob_state;
}


void CId2ReaderBase::x_ProcessReply(CReaderRequestResult& result,
                                    SId2LoadedSet& loaded_set,
                                    const CID2_Reply& reply)
{
    if ( x_GetError(result, reply) &
         (fError_bad_command | fError_bad_connection) ) {
        return;
    }
    switch ( reply.GetReply().Which() ) {
    case CID2_Reply::TReply::e_Get_seq_id:
        x_ProcessGetSeqId(result, loaded_set, reply,
                          reply.GetReply().GetGet_seq_id());
        break;
    case CID2_Reply::TReply::e_Get_blob_id:
        x_ProcessGetBlobId(result, loaded_set, reply,
                           reply.GetReply().GetGet_blob_id());
        break;
    case CID2_Reply::TReply::e_Get_blob_seq_ids:
        x_ProcessGetBlobSeqIds(result, loaded_set, reply,
                               reply.GetReply().GetGet_blob_seq_ids());
        break;
    case CID2_Reply::TReply::e_Get_blob:
        x_ProcessGetBlob(result, loaded_set, reply,
                         reply.GetReply().GetGet_blob());
        break;
    case CID2_Reply::TReply::e_Get_split_info:
        x_ProcessGetSplitInfo(result, loaded_set, reply,
                              reply.GetReply().GetGet_split_info());
        break;
    case CID2_Reply::TReply::e_Get_chunk:
        x_ProcessGetChunk(result, loaded_set, reply,
                          reply.GetReply().GetGet_chunk());
        break;
    default:
        break;
    }
}


void CId2ReaderBase::x_ProcessGetSeqId(CReaderRequestResult& result,
                                       SId2LoadedSet& loaded_set,
                                       const CID2_Reply& main_reply,
                                       const CID2_Reply_Get_Seq_id& reply)
{
    // we can save this data in cache
    const CID2_Request_Get_Seq_id& request = reply.GetRequest();
    const CID2_Seq_id& req_id = request.GetSeq_id();
    switch ( req_id.Which() ) {
    case CID2_Seq_id::e_String:
        x_ProcessGetStringSeqId(result, loaded_set, main_reply,
                                req_id.GetString(),
                                reply);
        break;

    case CID2_Seq_id::e_Seq_id:
        x_ProcessGetSeqIdSeqId(result, loaded_set, main_reply,
                               CSeq_id_Handle::GetHandle(req_id.GetSeq_id()),
                               reply);
        break;

    default:
        break;
    }
}


void CId2ReaderBase::x_ProcessGetStringSeqId(
    CReaderRequestResult& result,
    SId2LoadedSet& loaded_set,
    const CID2_Reply& main_reply,
    const string& seq_id,
    const CID2_Reply_Get_Seq_id& reply)
{
    CLoadLockSeq_ids ids(result, seq_id);
    if ( ids.IsLoaded() ) {
        return;
    }

    int state = 0;
    TErrorFlags errors = x_GetMessageError(main_reply);
    if ( errors & fError_no_data ) {
        // no Seq-ids
        state |= CBioseq_Handle::fState_no_data;
        if ( errors & fError_restricted ) {
            state |= CBioseq_Handle::fState_confidential;
        }
        if ( errors & fError_withdrawn ) {
            state |= CBioseq_Handle::fState_withdrawn;
        }
        SetAndSaveNoStringSeq_ids(result, seq_id, ids, state);
        return;
    }

    switch ( reply.GetRequest().GetSeq_id_type() ) {
    case CID2_Request_Get_Seq_id::eSeq_id_type_all:
    {{
        CReader::TSeqIds seq_ids;
        ITERATE ( CID2_Reply_Get_Seq_id::TSeq_id, it, reply.GetSeq_id() ) {
            seq_ids.push_back(CSeq_id_Handle::GetHandle(**it));
        }
        if ( reply.IsSetEnd_of_reply() ) {
            SetAndSaveStringSeq_ids(result, seq_id, ids, state,
                                    CFixedSeq_ids(eTakeOwnership, seq_ids));
        }
        else {
            loaded_set.m_Seq_idsByString[seq_id].first = state;
            loaded_set.m_Seq_idsByString[seq_id].second.swap(seq_ids);
        }
        break;
    }}
    case CID2_Request_Get_Seq_id::eSeq_id_type_gi:
    case CID2_Request_Get_Seq_id::eSeq_id_type_any:
    {{
        ITERATE ( CID2_Reply_Get_Seq_id::TSeq_id, it, reply.GetSeq_id() ) {
            if ( (**it).IsGi() ) {
                SetAndSaveStringGi(result, seq_id, ids, (**it).GetGi());
                break;
            }
        }
        break;
    }}
    default:
        // ???
        break;
    }
}


void CId2ReaderBase::x_ProcessGetSeqIdSeqId(
    CReaderRequestResult& result,
    SId2LoadedSet& loaded_set,
    const CID2_Reply& main_reply,
    const CSeq_id_Handle& seq_id,
    const CID2_Reply_Get_Seq_id& reply)
{
    CLoadLockSeq_ids ids(result, seq_id);
    if ( ids.IsLoaded() ) {
        return;
    }

    int state = 0;
    TErrorFlags errors = x_GetMessageError(main_reply);
    if ( errors & fError_no_data ) {
        state |= CBioseq_Handle::fState_no_data;
        // no Seq-ids
        if ( errors & fError_restricted ) {
            state |= CBioseq_Handle::fState_confidential;
        }
        if ( errors & fError_withdrawn ) {
            state |= CBioseq_Handle::fState_withdrawn;
        }
        SetAndSaveNoSeq_idSeq_ids(result, seq_id, ids, state);
        return;
    }
    switch ( reply.GetRequest().GetSeq_id_type() ) {
    case CID2_Request_Get_Seq_id::eSeq_id_type_all:
    {{
        CReader::TSeqIds seq_ids;
        ITERATE ( CID2_Reply_Get_Seq_id::TSeq_id, it, reply.GetSeq_id() ) {
            seq_ids.push_back(CSeq_id_Handle::GetHandle(**it));
        }
        if ( reply.IsSetEnd_of_reply() ) {
            SetAndSaveSeq_idSeq_ids(result, seq_id, ids, state,
                                    CFixedSeq_ids(eTakeOwnership, seq_ids));
        }
        else {
            loaded_set.m_Seq_ids[seq_id].first = state;
            loaded_set.m_Seq_ids[seq_id].second.swap(seq_ids);
        }
        break;
    }}
    case CID2_Request_Get_Seq_id::eSeq_id_type_gi:
    case CID2_Request_Get_Seq_id::eSeq_id_type_any:
    {{
        ITERATE ( CID2_Reply_Get_Seq_id::TSeq_id, it, reply.GetSeq_id() ) {
            if ( (**it).IsGi() ) {
                SetAndSaveSeq_idGi(result, seq_id, ids, (**it).GetGi());
                break;
            }
        }
        break;
    }}
    case CID2_Request_Get_Seq_id::eSeq_id_type_text:
    {{
        ITERATE ( CID2_Reply_Get_Seq_id::TSeq_id, it, reply.GetSeq_id() ) {
            if ( (**it).GetTextseq_Id() ) {
                SetAndSaveSeq_idAccVer(result, seq_id, ids, (**it));
                return;
            }
        }
        CRef<CSeq_id> no_acc(new CSeq_id);
        no_acc->SetGi(ZERO_GI);
        SetAndSaveSeq_idAccVer(result, seq_id, ids, *no_acc);
        break;
    }}
    case CID2_Request_Get_Seq_id::eSeq_id_type_label:
    {{
        ITERATE ( CID2_Reply_Get_Seq_id::TSeq_id, it, reply.GetSeq_id() ) {
            const CSeq_id& id = **it;
            if ( id.IsGeneral() ) {
                const CDbtag& dbtag = id.GetGeneral();
                const CObject_id& obj_id = dbtag.GetTag();
                if ( obj_id.IsStr() && dbtag.GetDb() == "LABEL" ) {
                    SetAndSaveSeq_idLabel(result, seq_id, ids,
                                          obj_id.GetStr());
                    break;
                }
            }
        }
        break;
    }}
    case CID2_Request_Get_Seq_id::eSeq_id_type_taxid:
    {{
        ITERATE ( CID2_Reply_Get_Seq_id::TSeq_id, it, reply.GetSeq_id() ) {
            const CSeq_id& id = **it;
            if ( id.IsGeneral() ) {
                const CDbtag& dbtag = id.GetGeneral();
                const CObject_id& obj_id = dbtag.GetTag();
                if ( obj_id.IsId() && dbtag.GetDb() == "TAXID" ) {
                    SetAndSaveSeq_idTaxId(result, seq_id, ids,
                                          obj_id.GetId());
                    break;
                }
            }
        }
        if ( !ids.IsLoadedTaxId() ) {
            ids.SetLoadedTaxId(-1);
        }
        break;
    }}
    default:
        // ???
        break;
    }
}


void CId2ReaderBase::x_ProcessGetBlobId(
    CReaderRequestResult& result,
    SId2LoadedSet& loaded_set,
    const CID2_Reply& main_reply,
    const CID2_Reply_Get_Blob_Id& reply)
{
    const CSeq_id& seq_id = reply.GetSeq_id();
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(seq_id);
    TErrorFlags errors;
    TBlobState blob_state = x_GetBlobState(main_reply, &errors);
    if ( blob_state & CBioseq_Handle::fState_no_data ) {
        SetAndSaveNoSeq_idBlob_ids(result, idh, 0, blob_state);
        return;
    }
    
    SId2LoadedSet::TBlob_idsInfo& ids = loaded_set.m_Blob_ids[idh];
    if ( errors & fError_warning ) {
        ids.first |= CBioseq_Handle::fState_other_error;
    }
    const CID2_Blob_Id& src_blob_id = reply.GetBlob_id();
    CBlob_id blob_id = GetBlobId(src_blob_id);
    if ( blob_state ) {
        loaded_set.m_BlobStates[blob_id] |= blob_state;
    }
    TContentsMask mask = 0;
    {{ // TODO: temporary logic, this info should be returned by server
        if ( blob_id.GetSubSat() == CID2_Blob_Id::eSub_sat_main ) {
            mask |= fBlobHasAllLocal;
        }
        else {
            if ( seq_id.IsGeneral() ) {
                const CObject_id& obj_id = seq_id.GetGeneral().GetTag();
                if ( obj_id.IsId() &&
                     obj_id.GetId() == blob_id.GetSatKey() ) {
                    mask |= fBlobHasAllLocal;
                }
                else {
                    mask |= fBlobHasExtAnnot;
                }
            }
            else {
                mask |= fBlobHasExtAnnot;
            }
        }
    }}
    SId2BlobInfo& blob_info = ids.second[blob_id];
    if ( reply.IsSetAnnot_info() && mask == fBlobHasExtAnnot ) {
        blob_info.m_AnnotInfo = reply.GetAnnot_info();
        ITERATE ( SId2BlobInfo::TAnnotInfo, it, blob_info.m_AnnotInfo ) {
            const CID2S_Seq_annot_Info& info = **it;
            if ( info.IsSetName() && NStr::StartsWith(info.GetName(), "NA") ) {
                mask &= fBlobHasNamedAnnot;
                if ( info.IsSetFeat() ) {
                    mask |= fBlobHasNamedFeat;
                }
                if ( info.IsSetGraph() ) {
                    mask |= fBlobHasNamedGraph;
                }
                if ( info.IsSetAlign() ) {
                    mask |= fBlobHasNamedAlign;
                }
            }
        }
    }
    blob_info.m_ContentMask = mask;
    if ( src_blob_id.IsSetVersion() && src_blob_id.GetVersion() > 0 ) {
        SetAndSaveBlobVersion(result, blob_id, src_blob_id.GetVersion());
    }
}


void CId2ReaderBase::x_ProcessGetBlobSeqIds(
    CReaderRequestResult& /* result */,
    SId2LoadedSet& /*loaded_set*/,
    const CID2_Reply& /*main_reply*/,
    const CID2_Reply_Get_Blob_Seq_ids&/*reply*/)
{
/*
    if ( reply.IsSetIds() ) {
        CID2_Blob_Seq_ids ids;
        x_ReadData(reply.GetIds(), Begin(ids));
        ITERATE ( CID2_Blob_Seq_ids::Tdata, it, ids.Get() ) {
            if ( !(*it)->IsSetReplaced() ) {
                result.AddBlob_id((*it)->GetSeq_id(),
                                  GetBlobId(reply.GetBlob_id()), "");
            }
        }
    }
*/
}


void CId2ReaderBase::x_ProcessGetBlob(
    CReaderRequestResult& result,
    SId2LoadedSet& loaded_set,
    const CID2_Reply& main_reply,
    const CID2_Reply_Get_Blob& reply)
{
    TChunkId chunk_id = CProcessor::kMain_ChunkId;
    const CID2_Blob_Id& src_blob_id = reply.GetBlob_id();
    TBlobId blob_id = GetBlobId(src_blob_id);

    if ( src_blob_id.IsSetVersion() && src_blob_id.GetVersion() > 0 ) {
        SetAndSaveBlobVersion(result, blob_id, src_blob_id.GetVersion());
    }

    TBlobState blob_state = x_GetBlobState(main_reply);
    if ( blob_state & CBioseq_Handle::fState_no_data ) {
        CLoadLockBlob blob(result, blob_id);
        blob.SetBlobState(blob_state);
        SetAndSaveNoBlob(result, blob_id, chunk_id, blob);
        _ASSERT(CProcessor::IsLoaded(result, blob_id, chunk_id, blob));
        return;
    }

    if ( !reply.IsSetData() ) {
        // assume only blob info reply
        if ( blob_state ) {
            loaded_set.m_BlobStates[blob_id] |= blob_state;
        }
        return;
    }

    const CID2_Reply_Data& data = reply.GetData();
    if ( data.GetData().empty() ) {
        if ( reply.GetSplit_version() != 0 &&
             data.GetData_type() == data.eData_type_seq_entry ) {
            // Skeleton Seq-entry could be attached to the split-info
            ERR_POST_X(6, Warning << "CId2ReaderBase: ID2-Reply-Get-Blob: "
                       "no data in reply: "<<blob_id);
            return;
        }
        ERR_POST_X(6, "CId2ReaderBase: ID2-Reply-Get-Blob: "
                   "no data in reply: "<<blob_id);
        CLoadLockBlob blob(result, blob_id);
        SetAndSaveNoBlob(result, blob_id, chunk_id, blob);
        _ASSERT(CProcessor::IsLoaded(result, blob_id, chunk_id, blob));
        return;
    }

    if ( reply.GetSplit_version() != 0 ) {
        // split info will follow
        // postpone parsing this blob
        loaded_set.m_Skeletons[blob_id] = &data;
        return;
    }

    CLoadLockBlob blob(result, blob_id);
    if ( blob.IsLoaded() ) {
        if ( blob->x_NeedsDelayedMainChunk() ) {
            chunk_id = CProcessor::kDelayedMain_ChunkId;
        }
        else {
            m_AvoidRequest |= fAvoidRequest_nested_get_blob_info;
            ERR_POST_X(4, Info << "CId2ReaderBase: ID2-Reply-Get-Blob: "
                          "blob already loaded: "<<blob_id);
            return;
        }
    }

    if ( blob->HasSeq_entry() ) {
        ERR_POST_X(5, "CId2ReaderBase: ID2-Reply-Get-Blob: "
                      "Seq-entry already loaded: "<<blob_id);
        return;
    }

    if ( blob_state ) {
        m_Dispatcher->SetAndSaveBlobState(result, blob_id, blob, blob_state);
    }

    if ( reply.GetBlob_id().GetSub_sat() == CID2_Blob_Id::eSub_sat_snp ) {
        m_Dispatcher->GetProcessor(CProcessor::eType_Seq_entry_SNP)
            .ProcessBlobFromID2Data(result, blob_id, chunk_id, data);
    }
    else {
        dynamic_cast<const CProcessor_ID2&>
            (m_Dispatcher->GetProcessor(CProcessor::eType_ID2))
            .ProcessData(result, blob_id, blob_state, chunk_id, data);
    }
    _ASSERT(CProcessor::IsLoaded(result, blob_id, chunk_id, blob));
}


void CId2ReaderBase::x_ProcessGetSplitInfo(
    CReaderRequestResult& result,
    SId2LoadedSet& loaded_set,
    const CID2_Reply& main_reply,
    const CID2S_Reply_Get_Split_Info& reply)
{
    TChunkId chunk_id = CProcessor::kMain_ChunkId;
    const CID2_Blob_Id& src_blob_id = reply.GetBlob_id();
    TBlobId blob_id = GetBlobId(src_blob_id);
    if ( src_blob_id.IsSetVersion() && src_blob_id.GetVersion() > 0 ) {
        SetAndSaveBlobVersion(result, blob_id, src_blob_id.GetVersion());
    }
    if ( !reply.IsSetData() ) {
        ERR_POST_X(11, "CId2ReaderBase: ID2S-Reply-Get-Split-Info: "
                       "no data in reply: "<<blob_id);
        return;
    }

    CLoadLockBlob blob(result, blob_id);
    if ( !blob ) {
        ERR_POST_X(9, "CId2ReaderBase: ID2S-Reply-Get-Split-Info: "
                      "no blob: " << blob_id);
        return;
    }
    if ( blob.IsLoaded() ) {
        if ( blob->x_NeedsDelayedMainChunk() ) {
            chunk_id = CProcessor::kDelayedMain_ChunkId;
        }
        else {
            m_AvoidRequest |= fAvoidRequest_nested_get_blob_info;
            ERR_POST_X(10, Info<<"CId2ReaderBase: ID2S-Reply-Get-Split-Info: "
                       "blob already loaded: " << blob_id);
            return;
        }
    }

    TBlobState blob_state = x_GetBlobState(main_reply);
    {{
        SId2LoadedSet::TBlobStates::iterator iter =
            loaded_set.m_BlobStates.find(blob_id);
        if ( iter != loaded_set.m_BlobStates.end() ) {
            blob_state |= iter->second;
        }
    }}
    if ( blob_state & CBioseq_Handle::fState_no_data ) {
        blob.SetBlobState(blob_state);
        SetAndSaveNoBlob(result, blob_id, chunk_id, blob);
        _ASSERT(CProcessor::IsLoaded(result, blob_id, chunk_id, blob));
        return;
    }

    CConstRef<CID2_Reply_Data> skel;
    {{
        SId2LoadedSet::TSkeletons::iterator iter =
            loaded_set.m_Skeletons.find(blob_id);
        if ( iter != loaded_set.m_Skeletons.end() ) {
            skel = iter->second;
        }
    }}

    if ( blob_state ) {
        m_Dispatcher->SetAndSaveBlobState(result, blob_id, blob, blob_state);
    }

    dynamic_cast<const CProcessor_ID2&>
        (m_Dispatcher->GetProcessor(CProcessor::eType_ID2))
        .ProcessData(result, blob_id, blob->GetBlobState(), chunk_id,
                     reply.GetData(), reply.GetSplit_version(), skel);

    _ASSERT(CProcessor::IsLoaded(result, blob_id, chunk_id, blob));
    loaded_set.m_Skeletons.erase(blob_id);
}


void CId2ReaderBase::x_ProcessGetChunk(
    CReaderRequestResult& result,
    SId2LoadedSet& /*loaded_set*/,
    const CID2_Reply& /*main_reply*/,
    const CID2S_Reply_Get_Chunk& reply)
{
    TBlobId blob_id = GetBlobId(reply.GetBlob_id());
    CLoadLockBlob blob(result, blob_id);
    if ( !blob ) {
        ERR_POST_X(12, "CId2ReaderBase: ID2S-Reply-Get-Chunk: "
                       "no blob: " << blob_id);
        return;
    }
    if ( !blob.IsLoaded() ) {
        ERR_POST_X(13, "CId2ReaderBase: ID2S-Reply-Get-Chunk: "
                       "blob is not loaded yet: " << blob_id);
        return;
    }
    if ( !reply.IsSetData() ) {
        ERR_POST_X(14, "CId2ReaderBase: ID2S-Reply-Get-Chunk: "
                       "no data in reply: "<<blob_id);
        return;
    }
    
    dynamic_cast<const CProcessor_ID2&>
        (m_Dispatcher->GetProcessor(CProcessor::eType_ID2))
        .ProcessData(result, blob_id, 0, reply.GetChunk_id(), reply.GetData());
}


/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////


END_SCOPE(objects)
END_NCBI_SCOPE
