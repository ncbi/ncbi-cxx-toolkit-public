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

#include <objtools/data_loaders/genbank/impl/reader_id2_base.hpp>
#include <objtools/data_loaders/genbank/impl/dispatcher.hpp>
#include <objtools/data_loaders/genbank/impl/processors.hpp>

#include <objmgr/objmgr_exception.hpp>
#include <objmgr/annot_selector.hpp>
#include <objmgr/impl/tse_info.hpp>
#include <objmgr/impl/tse_chunk_info.hpp>
#include <objmgr/impl/tse_split_info.hpp>

#include <objtools/data_loaders/genbank/impl/request_result.hpp>
#include <objtools/error_codes.hpp>

#include <corelib/ncbimtx.hpp>

#include <corelib/plugin_manager_impl.hpp>

#include <objects/general/Dbtag.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/id2/id2__.hpp>
#include <objects/id2/id2processor.hpp>
#include <objects/id2/id2processor_interface.hpp>
#include <objects/seqsplit/seqsplit__.hpp>

#include <serial/iterator.hpp>
#include <serial/serial.hpp>
#include <serial/objistr.hpp>

#include <corelib/plugin_manager_store.hpp>
#include <corelib/ncbi_safe_static.hpp>

#include <iomanip>


#define NCBI_USE_ERRCODE_X   Objtools_Rd_Id2Base

BEGIN_NCBI_SCOPE

NCBI_DEFINE_ERR_SUBCODE_X(15);

BEGIN_SCOPE(objects)

NCBI_PARAM_DECL(int, GENBANK, ID2_DEBUG);
NCBI_PARAM_DECL(int, GENBANK, ID2_MAX_CHUNKS_REQUEST_SIZE);
NCBI_PARAM_DECL(int, GENBANK, ID2_MAX_IDS_REQUEST_SIZE);
NCBI_PARAM_DECL(string, GENBANK, ID2_PROCESSOR);

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
NCBI_PARAM_DEF_EX(string, GENBANK, ID2_PROCESSOR, "",
                  eParam_NoThread, GENBANK_ID2_PROCESSOR);

int CId2ReaderBase::GetDebugLevel(void)
{
    static CSafeStatic<NCBI_PARAM_TYPE(GENBANK, ID2_DEBUG)> s_Value;
    return s_Value->Get();
}


static const char kSpecialId_label[] = "LABEL";
static const char kSpecialId_taxid[] = "TAXID";
static const char kSpecialId_hash[] = "HASH";
static const char kSpecialId_length[] = "Seq-inst.length";
static const char kSpecialId_type[] = "Seq-inst.mol";


// Number of chunks allowed in a single request
// 0 = unlimited request size
// 1 = do not use packets or get-chunks requests
static size_t GetMaxChunksRequestSize(void)
{
    static CSafeStatic<NCBI_PARAM_TYPE(GENBANK, ID2_MAX_CHUNKS_REQUEST_SIZE)> s_Value;
    return (size_t)s_Value->Get();
}


static size_t GetMaxIdsRequestSize(void)
{
    static CSafeStatic<NCBI_PARAM_TYPE(GENBANK, ID2_MAX_IDS_REQUEST_SIZE)> s_Value;
    return (size_t)s_Value->Get();
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
    vector<string> proc_list;
    string proc_param = NCBI_PARAM_TYPE(GENBANK, ID2_PROCESSOR)::GetDefault();
    NStr::Tokenize(proc_param, ";", proc_list, NStr::eNoMergeDelims);
    ITERATE ( vector<string>, it, proc_list ) {
        const string& proc_name = *it;
        CRef<CID2Processor> proc;
        try {
            proc = CPluginManagerGetter<CID2Processor>::Get()->
                CreateInstance(proc_name);
        }
        catch ( CException& exc ) {
            ERR_POST_X(15, "CId2ReaderBase: "
                       "cannot load ID2 processor "<<proc_name<<": "<<exc);
        }
        if ( proc ) {
            // send init request
            CID2_Request req;
            req.SetRequest().SetInit();
            x_SetContextData(req);
            CID2_Request_Packet packet;
            packet.Set().push_back(Ref(&req));
            proc->ProcessSomeRequests(packet);
            m_Processors.push_back(proc);
        }
    }
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
    CLoadLockSeqIds ids(result, seq_id);
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
    CLoadLockSeqIds ids(result, seq_id);
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
    CLoadLockGi lock(result, seq_id);
    if ( lock.IsLoadedGi() ) {
        return true;
    }
    CID2_Request req;
    CID2_Request::C_Request::TGet_seq_id& get_id =
        req.SetRequest().SetGet_seq_id();
    get_id.SetSeq_id().SetSeq_id().Assign(*seq_id.GetSeqId());
    get_id.SetSeq_id_type(CID2_Request_Get_Seq_id::eSeq_id_type_gi);
    x_ProcessRequest(result, req, 0);

    if ( !lock.IsLoadedGi() ) {
        return CReader::LoadSeq_idGi(result, seq_id);
    }

    return true;
}


bool CId2ReaderBase::LoadSeq_idAccVer(CReaderRequestResult& result,
                                      const CSeq_id_Handle& seq_id)
{
    CLoadLockAcc lock(result, seq_id);
    if ( lock.IsLoadedAccVer() ) {
        return true;
    }
    CID2_Request req;
    CID2_Request::C_Request::TGet_seq_id& get_id =
        req.SetRequest().SetGet_seq_id();
    get_id.SetSeq_id().SetSeq_id().Assign(*seq_id.GetSeqId());
    get_id.SetSeq_id_type(CID2_Request_Get_Seq_id::eSeq_id_type_text);
    x_ProcessRequest(result, req, 0);

    if ( lock.IsLoadedAccVer() ) {
        return true;
    }
    // load via full Seq-ids list
    return CReader::LoadSeq_idAccVer(result, seq_id);
}


bool CId2ReaderBase::LoadSeq_idLabel(CReaderRequestResult& result,
                                     const CSeq_id_Handle& seq_id)
{
    if ( m_AvoidRequest & fAvoidRequest_for_Seq_id_label ) {
        return CReader::LoadSeq_idLabel(result, seq_id);
    }

    CLoadLockLabel ids(result, seq_id);
    if ( ids.IsLoadedLabel() ) {
        return true;
    }
    CID2_Request req;
    CID2_Request::C_Request::TGet_seq_id& get_id =
        req.SetRequest().SetGet_seq_id();
    get_id.SetSeq_id().SetSeq_id().Assign(*seq_id.GetSeqId());
    get_id.SetSeq_id_type(CID2_Request_Get_Seq_id::eSeq_id_type_label);
    x_ProcessRequest(result, req, 0);

    if ( ids.IsLoadedLabel() ) {
        return true;
    }
    m_AvoidRequest |= fAvoidRequest_for_Seq_id_label;
    // load via full Seq-ids list
    return CReader::LoadSeq_idLabel(result, seq_id);
}


bool CId2ReaderBase::LoadSeq_idTaxId(CReaderRequestResult& result,
                                     const CSeq_id_Handle& seq_id)
{
    if ( m_AvoidRequest & fAvoidRequest_for_Seq_id_taxid ) {
        return CReader::LoadSeq_idTaxId(result, seq_id);
    }

    CLoadLockTaxId lock(result, seq_id);
    if ( lock.IsLoadedTaxId() ) {
        return true;
    }
    CID2_Request req;
    CID2_Request::C_Request::TGet_seq_id& get_id =
        req.SetRequest().SetGet_seq_id();
    get_id.SetSeq_id().SetSeq_id().Assign(*seq_id.GetSeqId());
    get_id.SetSeq_id_type(CID2_Request_Get_Seq_id::eSeq_id_type_taxid);
    x_ProcessRequest(result, req, 0);

    if ( !lock.IsLoadedTaxId() ) {
        m_AvoidRequest |= fAvoidRequest_for_Seq_id_taxid;
        return true; // repeat
    }

    return true;
}


bool CId2ReaderBase::LoadSequenceHash(CReaderRequestResult& result,
                                      const CSeq_id_Handle& seq_id)
{
    if ( m_AvoidRequest & fAvoidRequest_for_Seq_id_hash ) {
        return CReader::LoadSequenceHash(result, seq_id);
    }

    CLoadLockHash lock(result, seq_id);
    if ( lock.IsLoadedHash() ) {
        return true;
    }
    CID2_Request req;
    CID2_Request::C_Request::TGet_seq_id& get_id =
        req.SetRequest().SetGet_seq_id();
    get_id.SetSeq_id().SetSeq_id().Assign(*seq_id.GetSeqId());
    get_id.SetSeq_id_type(CID2_Request_Get_Seq_id::eSeq_id_type_hash);
    x_ProcessRequest(result, req, 0);

    if ( !lock.IsLoadedHash() ) {
        m_AvoidRequest |= fAvoidRequest_for_Seq_id_hash;
        return true; // repeat
    }

    return true;
}


bool CId2ReaderBase::LoadSequenceLength(CReaderRequestResult& result,
                                        const CSeq_id_Handle& seq_id)
{
    if ( m_AvoidRequest & fAvoidRequest_for_Seq_id_length ) {
        return CReader::LoadSequenceLength(result, seq_id);
    }

    CLoadLockLength lock(result, seq_id);
    if ( lock.IsLoadedLength() ) {
        return true;
    }
    CID2_Request req;
    CID2_Request::C_Request::TGet_seq_id& get_id =
        req.SetRequest().SetGet_seq_id();
    get_id.SetSeq_id().SetSeq_id().Assign(*seq_id.GetSeqId());
    get_id.SetSeq_id_type(CID2_Request_Get_Seq_id::eSeq_id_type_all |
                          CID2_Request_Get_Seq_id::eSeq_id_type_seq_length);
    x_ProcessRequest(result, req, 0);

    if ( !lock.IsLoadedLength() ) {
        m_AvoidRequest |= fAvoidRequest_for_Seq_id_length;
        return true; // repeat
    }

    return true;
}


bool CId2ReaderBase::LoadSequenceType(CReaderRequestResult& result,
                                      const CSeq_id_Handle& seq_id)
{
    if ( m_AvoidRequest & fAvoidRequest_for_Seq_id_type ) {
        return CReader::LoadSequenceType(result, seq_id);
    }

    CLoadLockType lock(result, seq_id);
    if ( lock.IsLoadedType() ) {
        return true;
    }
    CID2_Request req;
    CID2_Request::C_Request::TGet_seq_id& get_id =
        req.SetRequest().SetGet_seq_id();
    get_id.SetSeq_id().SetSeq_id().Assign(*seq_id.GetSeqId());
    get_id.SetSeq_id_type(CID2_Request_Get_Seq_id::eSeq_id_type_all |
                          CID2_Request_Get_Seq_id::eSeq_id_type_seq_mol);
    x_ProcessRequest(result, req, 0);

    if ( !lock.IsLoadedType() ) {
        m_AvoidRequest |= fAvoidRequest_for_Seq_id_type;
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
    CID2_Request_Packet packet;
    size_t packet_start = 0;
    
    for ( size_t i = 0; i < count; ++i ) {
        if ( loaded[i] || CReadDispatcher::CannotProcess(ids[i]) ) {
            continue;
        }
        CLoadLockAcc lock(result, ids[i]);
        if ( lock.IsLoadedAccVer() ) {
            ret[i] = lock.GetAccVer();
            loaded[i] = true;
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
                if ( loaded[i] || CReadDispatcher::CannotProcess(ids[i]) ) {
                    continue;
                }
                CLoadLockAcc lock(result, ids[i]);
                if ( lock.IsLoadedAccVer() ) {
                    ret[i] = lock.GetAccVer();
                    loaded[i] = true;
                    continue;
                }
            }
            packet.Set().clear();
        }
    }

    if ( !packet.Set().empty() ) {
        x_ProcessPacket(result, packet, 0);

        for ( size_t i = packet_start; i < count; ++i ) {
            if ( loaded[i] || CReadDispatcher::CannotProcess(ids[i]) ) {
                continue;
            }
            CLoadLockAcc lock(result, ids[i]);
            if ( lock.IsLoadedAccVer() ) {
                ret[i] = lock.GetAccVer();
                loaded[i] = true;
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
    CID2_Request_Packet packet;
    size_t packet_start = 0;
    
    for ( size_t i = 0; i < count; ++i ) {
        if ( loaded[i] || CReadDispatcher::CannotProcess(ids[i]) ) {
            continue;
        }
        CLoadLockGi lock(result, ids[i]);
        if ( lock.IsLoadedGi() ) {
            ret[i] = lock.GetGi();
            loaded[i] = true;
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
                if ( loaded[i] || CReadDispatcher::CannotProcess(ids[i]) ) {
                    continue;
                }
                CLoadLockGi lock(result, ids[i]);
                if ( lock.IsLoadedGi() ) {
                    ret[i] = lock.GetGi();
                    loaded[i] = true;
                    continue;
                }
            }
            packet.Set().clear();
        }
    }

    if ( !packet.Set().empty() ) {
        x_ProcessPacket(result, packet, 0);

        for ( size_t i = packet_start; i < count; ++i ) {
            if ( loaded[i] || CReadDispatcher::CannotProcess(ids[i]) ) {
                continue;
            }
            CLoadLockGi lock(result, ids[i]);
            if ( lock.IsLoadedGi() ) {
                ret[i] = lock.GetGi();
                loaded[i] = true;
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
    CID2_Request_Packet packet;
    size_t packet_start = 0;
    
    for ( size_t i = 0; i < count; ++i ) {
        if ( loaded[i] || CReadDispatcher::CannotProcess(ids[i]) ) {
            continue;
        }
        CLoadLockLabel lock(result, ids[i]);
        if ( lock.IsLoadedLabel() ) {
            ret[i] = lock.GetLabel();
            loaded[i] = true;
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
                if ( loaded[i] || CReadDispatcher::CannotProcess(ids[i]) ) {
                    continue;
                }
                CLoadLockLabel lock(result, ids[i]);
                if ( lock.IsLoadedLabel() ) {
                    ret[i] = lock.GetLabel();
                    loaded[i] = true;
                    continue;
                }
                else {
                    m_AvoidRequest |= fAvoidRequest_for_Seq_id_label;
                    CLoadLockSeqIds ids_lock(result, ids[i]);
                    if ( ids_lock.IsLoaded() ) {
                        string label = ids_lock.GetSeq_ids().FindLabel();
                        lock.SetLoadedLabel(label,
                                            ids_lock.GetExpirationTime());
                        ret[i] = label;
                        loaded[i] = true;
                    }
                }
            }
            packet.Set().clear();
        }
    }

    if ( !packet.Set().empty() ) {
        x_ProcessPacket(result, packet, 0);

        for ( size_t i = packet_start; i < count; ++i ) {
            if ( loaded[i] || CReadDispatcher::CannotProcess(ids[i]) ) {
                continue;
            }
            CLoadLockLabel lock(result, ids[i]);
            if ( lock.IsLoadedLabel() ) {
                ret[i] = lock.GetLabel();
                loaded[i] = true;
                continue;
            }
            else {
                m_AvoidRequest |= fAvoidRequest_for_Seq_id_label;
                CLoadLockSeqIds ids_lock(result, ids[i]);
                if ( ids_lock.IsLoaded() ) {
                    string label = ids_lock.GetSeq_ids().FindLabel();
                    lock.SetLoadedLabel(label,
                                        ids_lock.GetExpirationTime());
                    ret[i] = label;
                    loaded[i] = true;
                }
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
    CID2_Request_Packet packet;
    size_t packet_start = 0;
    
    for ( size_t i = 0; i < count; ++i ) {
        if ( loaded[i] || CReadDispatcher::CannotProcess(ids[i]) ) {
            continue;
        }
        if ( m_AvoidRequest & fAvoidRequest_for_Seq_id_taxid ) {
            return CReader::LoadTaxIds(result, ids, loaded, ret);
        }
        CLoadLockTaxId lock(result, ids[i]);
        if ( lock.IsLoadedTaxId() ) {
            ret[i] = lock.GetTaxId();
            loaded[i] = true;
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
                if ( loaded[i] || CReadDispatcher::CannotProcess(ids[i]) ) {
                    continue;
                }
                CLoadLockTaxId lock(result, ids[i]);
                if ( lock.IsLoadedTaxId() ) {
                    ret[i] = lock.GetTaxId();
                    loaded[i] = true;
                    continue;
                }
                else {
                    m_AvoidRequest |= fAvoidRequest_for_Seq_id_taxid;
                }
            }
            packet.Set().clear();
        }
    }

    if ( !packet.Set().empty() ) {
        x_ProcessPacket(result, packet, 0);

        for ( size_t i = packet_start; i < count; ++i ) {
            if ( loaded[i] || CReadDispatcher::CannotProcess(ids[i]) ) {
                continue;
            }
            CLoadLockTaxId lock(result, ids[i]);
            if ( lock.IsLoadedTaxId() ) {
                ret[i] = lock.GetTaxId();
                loaded[i] = true;
                continue;
            }
            else {
                m_AvoidRequest |= fAvoidRequest_for_Seq_id_taxid;
            }
        }
    }

    return true;
}


bool CId2ReaderBase::LoadHashes(CReaderRequestResult& result,
                                const TIds& ids, TLoaded& loaded, THashes& ret)
{
    size_t max_request_size = GetMaxIdsRequestSize();
    if ( max_request_size <= 1 ||
         (m_AvoidRequest & fAvoidRequest_for_Seq_id_hash) ) {
        return CReader::LoadHashes(result, ids, loaded, ret);
    }

    size_t count = ids.size();
    CID2_Request_Packet packet;
    size_t packet_start = 0;
    
    for ( size_t i = 0; i < count; ++i ) {
        if ( loaded[i] || CReadDispatcher::CannotProcess(ids[i]) ) {
            continue;
        }
        if ( m_AvoidRequest & fAvoidRequest_for_Seq_id_hash ) {
            return CReader::LoadHashes(result, ids, loaded, ret);
        }
        CLoadLockHash lock(result, ids[i]);
        if ( lock.IsLoadedHash() ) {
            ret[i] = lock.GetHash();
            loaded[i] = true;
            continue;
        }
        
        CRef<CID2_Request> req(new CID2_Request);
        CID2_Request::C_Request::TGet_seq_id& get_id =
            req->SetRequest().SetGet_seq_id();
        get_id.SetSeq_id().SetSeq_id().Assign(*ids[i].GetSeqId());
        get_id.SetSeq_id_type(CID2_Request_Get_Seq_id::eSeq_id_type_hash);
        if ( packet.Set().empty() ) {
            packet_start = i;
        }
        packet.Set().push_back(req);
        if ( packet.Set().size() == max_request_size ) {
            x_ProcessPacket(result, packet, 0);
            size_t count = i+1;
            for ( size_t i = packet_start; i < count; ++i ) {
                if ( loaded[i] || CReadDispatcher::CannotProcess(ids[i]) ) {
                    continue;
                }
                CLoadLockHash lock(result, ids[i]);
                if ( lock.IsLoadedHash() ) {
                    ret[i] = lock.GetHash();
                    loaded[i] = true;
                    continue;
                }
                else {
                    m_AvoidRequest |= fAvoidRequest_for_Seq_id_hash;
                }
            }
            packet.Set().clear();
        }
    }

    if ( !packet.Set().empty() ) {
        x_ProcessPacket(result, packet, 0);

        for ( size_t i = packet_start; i < count; ++i ) {
            if ( loaded[i] || CReadDispatcher::CannotProcess(ids[i]) ) {
                continue;
            }
            CLoadLockHash lock(result, ids[i]);
            if ( lock.IsLoadedHash() ) {
                ret[i] = lock.GetHash();
                loaded[i] = true;
                continue;
            }
            else {
                m_AvoidRequest |= fAvoidRequest_for_Seq_id_hash;
            }
        }
    }

    return true;
}


bool CId2ReaderBase::LoadLengths(CReaderRequestResult& result,
                                 const TIds& ids, TLoaded& loaded, TLengths& ret)
{
    size_t max_request_size = GetMaxIdsRequestSize();
    if ( max_request_size <= 1 ||
         (m_AvoidRequest & fAvoidRequest_for_Seq_id_length) ) {
        return CReader::LoadLengths(result, ids, loaded, ret);
    }

    size_t count = ids.size();
    CID2_Request_Packet packet;
    size_t packet_start = 0;
    
    for ( size_t i = 0; i < count; ++i ) {
        if ( loaded[i] || CReadDispatcher::CannotProcess(ids[i]) ) {
            continue;
        }
        if ( m_AvoidRequest & fAvoidRequest_for_Seq_id_length ) {
            return CReader::LoadLengths(result, ids, loaded, ret);
        }
        CLoadLockLength lock(result, ids[i]);
        if ( lock.IsLoadedLength() ) {
            ret[i] = lock.GetLength();
            loaded[i] = true;
            continue;
        }
        
        CRef<CID2_Request> req(new CID2_Request);
        CID2_Request::C_Request::TGet_seq_id& get_id =
            req->SetRequest().SetGet_seq_id();
        get_id.SetSeq_id().SetSeq_id().Assign(*ids[i].GetSeqId());
        get_id.SetSeq_id_type(CID2_Request_Get_Seq_id::eSeq_id_type_all |
                              CID2_Request_Get_Seq_id::eSeq_id_type_seq_length);
        if ( packet.Set().empty() ) {
            packet_start = i;
        }
        packet.Set().push_back(req);
        if ( packet.Set().size() == max_request_size ) {
            x_ProcessPacket(result, packet, 0);
            size_t count = i+1;
            for ( size_t i = packet_start; i < count; ++i ) {
                if ( loaded[i] || CReadDispatcher::CannotProcess(ids[i]) ) {
                    continue;
                }
                CLoadLockLength lock(result, ids[i]);
                if ( lock.IsLoadedLength() ) {
                    ret[i] = lock.GetLength();
                    loaded[i] = true;
                    continue;
                }
                else {
                    m_AvoidRequest |= fAvoidRequest_for_Seq_id_length;
                }
            }
            packet.Set().clear();
        }
    }

    if ( !packet.Set().empty() ) {
        x_ProcessPacket(result, packet, 0);

        for ( size_t i = packet_start; i < count; ++i ) {
            if ( loaded[i] || CReadDispatcher::CannotProcess(ids[i]) ) {
                continue;
            }
            CLoadLockLength lock(result, ids[i]);
            if ( lock.IsLoadedLength() ) {
                ret[i] = lock.GetLength();
                loaded[i] = true;
                continue;
            }
            else {
                m_AvoidRequest |= fAvoidRequest_for_Seq_id_length;
            }
        }
    }

    return true;
}


bool CId2ReaderBase::LoadTypes(CReaderRequestResult& result,
                               const TIds& ids, TLoaded& loaded, TTypes& ret)
{
    size_t max_request_size = GetMaxIdsRequestSize();
    if ( max_request_size <= 1 ||
         (m_AvoidRequest & fAvoidRequest_for_Seq_id_type) ) {
        return CReader::LoadTypes(result, ids, loaded, ret);
    }

    size_t count = ids.size();
    CID2_Request_Packet packet;
    size_t packet_start = 0;
    
    for ( size_t i = 0; i < count; ++i ) {
        if ( loaded[i] || CReadDispatcher::CannotProcess(ids[i]) ) {
            continue;
        }
        if ( m_AvoidRequest & fAvoidRequest_for_Seq_id_type ) {
            return CReader::LoadTypes(result, ids, loaded, ret);
        }
        CLoadLockType lock(result, ids[i]);
        if ( lock.IsLoadedType() ) {
            ret[i] = lock.GetType();
            loaded[i] = true;
            continue;
        }
        
        CRef<CID2_Request> req(new CID2_Request);
        CID2_Request::C_Request::TGet_seq_id& get_id =
            req->SetRequest().SetGet_seq_id();
        get_id.SetSeq_id().SetSeq_id().Assign(*ids[i].GetSeqId());
        get_id.SetSeq_id_type(CID2_Request_Get_Seq_id::eSeq_id_type_all |
                              CID2_Request_Get_Seq_id::eSeq_id_type_seq_mol);
        if ( packet.Set().empty() ) {
            packet_start = i;
        }
        packet.Set().push_back(req);
        if ( packet.Set().size() == max_request_size ) {
            x_ProcessPacket(result, packet, 0);
            size_t count = i+1;
            for ( size_t i = packet_start; i < count; ++i ) {
                if ( loaded[i] || CReadDispatcher::CannotProcess(ids[i]) ) {
                    continue;
                }
                CLoadLockType lock(result, ids[i]);
                if ( lock.IsLoadedType() ) {
                    ret[i] = lock.GetType();
                    loaded[i] = true;
                    continue;
                }
                else {
                    m_AvoidRequest |= fAvoidRequest_for_Seq_id_type;
                }
            }
            packet.Set().clear();
        }
    }

    if ( !packet.Set().empty() ) {
        x_ProcessPacket(result, packet, 0);

        for ( size_t i = packet_start; i < count; ++i ) {
            if ( loaded[i] || CReadDispatcher::CannotProcess(ids[i]) ) {
                continue;
            }
            CLoadLockType lock(result, ids[i]);
            if ( lock.IsLoadedType() ) {
                ret[i] = lock.GetType();
                loaded[i] = true;
                continue;
            }
            else {
                m_AvoidRequest |= fAvoidRequest_for_Seq_id_type;
            }
        }
    }

    return true;
}


bool CId2ReaderBase::LoadStates(CReaderRequestResult& result,
                                const TIds& ids, TLoaded& loaded, TStates& ret)
{
    size_t max_request_size = GetMaxIdsRequestSize();
    if ( max_request_size <= 1 ) {
        return CReader::LoadStates(result, ids, loaded, ret);
    }

    size_t count = ids.size();
    CID2_Request_Packet packet;
    size_t packet_start = 0;
    
    for ( size_t i = 0; i < count; ++i ) {
        if ( CReadDispatcher::SetBlobState(i, result, ids, loaded, ret) ) {
            continue;
        }
        
        CRef<CID2_Request> req(new CID2_Request);
        x_SetResolve(req->SetRequest().SetGet_blob_id(), *ids[i].GetSeqId());
        if ( packet.Set().empty() ) {
            packet_start = i;
        }
        packet.Set().push_back(req);
        if ( packet.Set().size() == max_request_size ) {
            x_ProcessPacket(result, packet, 0);
            size_t count = i+1;
            for ( size_t i = packet_start; i < count; ++i ) {
                CReadDispatcher::SetBlobState(i, result, ids, loaded, ret);
            }
            packet.Set().clear();
        }
    }

    if ( !packet.Set().empty() ) {
        x_ProcessPacket(result, packet, 0);

        for ( size_t i = packet_start; i < count; ++i ) {
            CReadDispatcher::SetBlobState(i, result, ids, loaded, ret);
        }
    }

    return true;
}


bool CId2ReaderBase::LoadSeq_idBlob_ids(CReaderRequestResult& result,
                                        const CSeq_id_Handle& seq_id,
                                        const SAnnotSelector* sel)
{
    CLoadLockBlobIds ids(result, seq_id, sel);
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


bool CId2ReaderBase::LoadBlobState(CReaderRequestResult& result,
                                   const CBlob_id& blob_id)
{
    CLoadLockBlobState lock(result, blob_id);
    if ( lock.IsLoadedBlobState() ) {
        return true;
    }
    CID2_Request req;
    CID2_Request_Get_Blob_Info& req2 = req.SetRequest().SetGet_blob_info();
    x_SetResolve(req2.SetBlob_id().SetBlob_id(), blob_id);
    x_ProcessRequest(result, req, 0);
    if ( CProcessor_ExtAnnot::IsExtAnnot(blob_id) ) {
        // workaround for possible incorrect reply on request for non-existent
        // external annotations
        if ( !lock.IsLoadedBlobState() ) {
            ERR_POST_X(5, "ExtAnnot blob state is not loaded: "<<blob_id);
            result.SetLoadedBlobState(blob_id, 0);
        }
    }
    return true;
}


bool CId2ReaderBase::LoadBlobVersion(CReaderRequestResult& result,
                                     const CBlob_id& blob_id)
{
    CLoadLockBlobVersion lock(result, blob_id);
    if ( lock.IsLoadedBlobVersion() ) {
        return true;
    }
    CID2_Request req;
    CID2_Request_Get_Blob_Info& req2 = req.SetRequest().SetGet_blob_info();
    x_SetResolve(req2.SetBlob_id().SetBlob_id(), blob_id);
    x_ProcessRequest(result, req, 0);
    if ( CProcessor_ExtAnnot::IsExtAnnot(blob_id) ) {
        // workaround for possible incorrect reply on request for non-existent
        // external annotations
        if ( !lock.IsLoadedBlobVersion() ) {
            ERR_POST_X(9, "ExtAnnot blob version is not loaded: "<<blob_id);
            result.SetLoadedBlobVersion(blob_id, 0);
        }
    }
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
    if ( result.IsLoadedSeqIds(seq_id) ) {
        return true;
    }
    return LoadStringSeq_ids(result, seq_id);
}


bool CId2ReaderBase::LoadBlobs(CReaderRequestResult& result,
                               const CSeq_id_Handle& seq_id,
                               TContentsMask mask,
                               const SAnnotSelector* sel)
{
    CLoadLockBlobIds ids(result, seq_id, sel);
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
    else if ( m_Dispatcher->GetWriter(result, CWriter::eBlobWriter) ) {
        return CReader::LoadBlobs(result, seq_id, mask, sel);
    }
    else {
        CID2_Request req;
        CID2_Request_Get_Blob_Info& req2 = req.SetRequest().SetGet_blob_info();
        x_SetResolve(req2.SetBlob_id().SetResolve().SetRequest(),
                     *seq_id.GetSeqId());
        x_SetDetails(req2.SetGet_data(), mask);
        x_SetExclude_blobs(req2, seq_id, result);
        x_ProcessRequest(result, req, sel);
        return ids.IsLoaded();
    }
}


bool CId2ReaderBase::LoadBlobs(CReaderRequestResult& result,
                               const CLoadLockBlobIds& blobs,
                               TContentsMask mask,
                               const SAnnotSelector* sel)
{
    size_t max_request_size = GetMaxChunksRequestSize();
    CID2_Request_Packet packet;
    CFixedBlob_ids blob_ids = blobs.GetBlob_ids();
    ITERATE ( CFixedBlob_ids, it, blob_ids ) {
        const CBlob_Info& info = *it;
        const CBlob_id& blob_id = *info.GetBlob_id();
        if ( !info.Matches(mask, sel) ) {
            continue; // skip this blob
        }
        CLoadLockBlob blob(result, blob_id);
        if ( blob.IsLoadedBlob() ) {
            continue;
        }

        if ( info.IsSetAnnotInfo() ) {
            CProcessor_AnnotInfo::LoadBlob(result, info);
            _ASSERT(blob.IsLoadedBlob());
            continue;
        }
        
        if ( CProcessor_ExtAnnot::IsExtAnnot(blob_id) ) {
            dynamic_cast<const CProcessor_ExtAnnot&>
                (m_Dispatcher->GetProcessor(CProcessor::eType_ExtAnnot))
                .Process(result, blob_id, kMain_ChunkId);
            _ASSERT(blob.IsLoadedBlob());
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
    return true;
}


bool CId2ReaderBase::LoadBlob(CReaderRequestResult& result,
                              const TBlobId& blob_id)
{
    CLoadLockBlob blob(result, blob_id);
    if ( blob.IsLoadedBlob() ) {
        return true;
    }

    if ( CProcessor_ExtAnnot::IsExtAnnot(blob_id) ) {
        dynamic_cast<const CProcessor_ExtAnnot&>
            (m_Dispatcher->GetProcessor(CProcessor::eType_ExtAnnot))
            .Process(result, blob_id, kMain_ChunkId);
        _ASSERT(blob.IsLoadedBlob());
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
    CLoadLockBlob blob(result, blob_id, chunk_id);
    if ( blob.IsLoadedChunk() ) {
        return true;
    }

    CID2_Request req;
    if ( chunk_id == kDelayedMain_ChunkId ) {
        CID2_Request_Get_Blob_Info& req2 = req.SetRequest().SetGet_blob_info();
        x_SetResolve(req2.SetBlob_id().SetBlob_id(), blob_id);
        req2.SetGet_data();
        x_ProcessRequest(result, req, 0);
        if ( !blob.IsLoadedChunk() ) {
            CLoadLockSetter setter(blob);
            if ( !setter.IsLoaded() ) {
                ERR_POST_X(2, "ExtAnnot chunk is not loaded: "<<blob_id);
                setter.SetLoaded();
            }
        }
    }
    else {
        CID2S_Request_Get_Chunks& req2 = req.SetRequest().SetGet_chunks();
        x_SetResolve(req2.SetBlob_id(), blob_id);

        if ( blob.GetKnownBlobVersion() > 0 ) {
            req2.SetBlob_id().SetVersion(blob.GetKnownBlobVersion());
        }
        req2.SetSplit_version(blob.GetSplitInfo().GetSplitVersion());
        req2.SetChunks().push_back(CID2S_Chunk_Id(chunk_id));
        x_ProcessRequest(result, req, 0);
    }
    return true;
}


static void LoadedChunksPacket(CReaderRequestResult& result,
                               CID2_Request_Packet& packet,
                               vector<TChunkId>& chunks,
                               const CBlob_id& blob_id)
{
    CLoadLockBlob blob(result, blob_id);
    NON_CONST_ITERATE(vector<TChunkId>, it, chunks) {
        blob.SelectChunk(*it);
        if ( !blob.IsLoadedChunk() ) {
            CLoadLockSetter setter(blob);
            if ( !setter.IsLoaded() ) {
                ERR_POST_X(3, "ExtAnnot chunk is not loaded: " << blob_id);
                setter.SetLoaded();
            }
        }
    }
    packet.Set().clear();
    chunks.clear();
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
    _ASSERT(blob.IsLoadedBlob());

    CID2_Request_Packet packet;

    CRef<CID2_Request> chunks_req(new CID2_Request);
    CID2S_Request_Get_Chunks& get_chunks =
        chunks_req->SetRequest().SetGet_chunks();

    x_SetResolve(get_chunks.SetBlob_id(), blob_id);
    if ( blob.GetKnownBlobVersion() > 0 ) {
        get_chunks.SetBlob_id().SetVersion(blob.GetKnownBlobVersion());
    }
    get_chunks.SetSplit_version(blob.GetSplitInfo().GetSplitVersion());
    CID2S_Request_Get_Chunks::TChunks& chunks = get_chunks.SetChunks();

    vector<TChunkId> ext_chunks;
    ITERATE(TChunkIds, id, chunk_ids) {
        blob.SelectChunk(*id);
        if ( blob.IsLoadedChunk() ) {
            continue;
        }
        if ( *id == kDelayedMain_ChunkId ) {
            CRef<CID2_Request> ext_req(new CID2_Request);
            CID2_Request_Get_Blob_Info& ext_req_data =
                ext_req->SetRequest().SetGet_blob_info();
            x_SetResolve(ext_req_data.SetBlob_id().SetBlob_id(), blob_id);
            ext_req_data.SetGet_data();
            packet.Set().push_back(ext_req);
            ext_chunks.push_back(*id);
            if ( LimitChunksRequests(max_request_size) &&
                 packet.Get().size() >= max_request_size ) {
                // Request collected chunks
                x_ProcessPacket(result, packet, 0);
                LoadedChunksPacket(result, packet, ext_chunks, blob_id);
            }
        }
        else {
            chunks.push_back(CID2S_Chunk_Id(*id));
            if ( LimitChunksRequests(max_request_size) &&
                 chunks.size() >= max_request_size ) {
                // Process collected chunks
                x_ProcessRequest(result, *chunks_req, 0);
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
        LoadedChunksPacket(result, packet, ext_chunks, blob_id);
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
        CLoadLockBlobIds ids(result, *id, 0);
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
    size_t processed_requests = 0;
    if ( m_AvoidRequest & fAvoidRequest_nested_get_blob_info ) {
        if ( !x_LoadSeq_idBlob_idsSet(result, seq_ids) ) {
            return false;
        }
        loaded_blob_ids = true;
    }

    set<CBlob_id> load_blob_ids;
    CID2_Request_Packet packet;
    ITERATE(TSeqIds, id, seq_ids) {
        if ( !loaded_blob_ids &&
             (m_AvoidRequest & fAvoidRequest_nested_get_blob_info) ) {
            if ( !x_LoadSeq_idBlob_idsSet(result, seq_ids) ) {
                return false;
            }
            loaded_blob_ids = true;
        }
        CLoadLockBlobIds ids(result, *id, 0);
        if ( ids && ids.IsLoaded() ) {
            // shortcut - we know Seq-id -> Blob-id resolution
            CFixedBlob_ids blob_ids = ids.GetBlob_ids();
            ITERATE ( CFixedBlob_ids, it, blob_ids ) {
                const CBlob_Info& info = *it;
                const CBlob_id& blob_id = *info.GetBlob_id();
                if ( (info.GetContentsMask() & fBlobHasCore) == 0 ) {
                    continue; // skip this blob
                }
                CLoadLockBlob blob(result, blob_id);
                if ( blob.IsLoadedBlob() ) {
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
                    processed_requests += packet.Set().size();
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
                processed_requests += packet.Set().size();
                x_ProcessPacket(result, packet, 0);
                packet.Set().clear();
            }
        }
    }
    if ( !packet.Get().empty() ) {
        processed_requests += packet.Get().size();
        x_ProcessPacket(result, packet, 0);
    }
    if ( !processed_requests && !loaded_blob_ids ) {
        return false;
    }
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


BEGIN_LOCAL_NAMESPACE;


class CProcessorResolver : public CID2ProcessorResolver
{
public:
    CProcessorResolver(CReadDispatcher& dispatcher,
                       CReaderRequestResult& result)
        : m_Dispatcher(dispatcher),
          m_RequestResult(result)
        {
        }

    virtual TIds GetIds(const CSeq_id& id)
        {
            TIds ret;
            CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(id);
            CLoadLockSeqIds lock(m_RequestResult, idh);
            if ( !lock.IsLoaded() ) {
                CReaderRequestResultRecursion recurse(m_RequestResult, true);
                m_Dispatcher.LoadSeq_idSeq_ids(m_RequestResult, idh);
            }
            CFixedSeq_ids ids = lock.GetSeq_ids();
            ITERATE ( CFixedSeq_ids, it, ids ) {
                ret.push_back(it->GetSeqId());
            }
            return ret;
        }

private:
    CReadDispatcher& m_Dispatcher;
    CReaderRequestResult& m_RequestResult;
};


END_LOCAL_NAMESPACE;


void CId2ReaderBase::x_SetContextData(CID2_Request& request)
{
    if ( request.GetRequest().IsInit() ) {
        CRef<CID2_Param> param(new CID2_Param);
        param->SetName("log:client_name");
        param->SetValue().push_back(GetDiagContext().GetAppName());
        request.SetParams().Set().push_back(param);
        if ( 1 ) {
            CRef<CID2_Param> param(new CID2_Param);
            param->SetName("id2:allow");
            // allow new blob-state field in several ID2 replies
            param->SetValue().push_back("*.blob-state");
            // enable VDB-based WGS sequences
            param->SetValue().push_back("vdb-wgs");
            request.SetParams().Set().push_back(param);
        }
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
        param->SetValue().push_back(rctx.GetNextSubHitID());
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
    int request_count = static_cast<int>(packet.Get().size());
    int end_serial_num = static_cast<int>(m_RequestSerialNumber.Add(request_count));
    while ( end_serial_num <= request_count ) {
        // int overflow, adjust to 1
        {{
            DEFINE_STATIC_FAST_MUTEX(sx_Mutex);
            CFastMutexGuard guard(sx_Mutex);
            int num = static_cast<int>(m_RequestSerialNumber.Get());
            if ( num <= request_count ) {
                m_RequestSerialNumber.Set(1);
            }
        }}
        end_serial_num = static_cast<int>(m_RequestSerialNumber.Add(request_count));
    }
    int start_serial_num = end_serial_num - request_count;
    {{
        int cur_serial_num = start_serial_num;
        NON_CONST_ITERATE ( CID2_Request_Packet::Tdata, it, packet.Set() ) {
            (*it)->SetSerial_number(cur_serial_num++);
        }
    }}
    vector<char> done(request_count);
    vector<SId2LoadedSet> loaded_sets(request_count);
    
    int remaining_count = request_count;
    NON_CONST_ITERATE ( TProcessors, it, m_Processors ) {
        if ( result.IsInProcessor() ) {
            break;
        }
        CProcessorResolver resolver(*m_Dispatcher, result);
        CID2Processor::TReplies replies =
            (*it)->ProcessSomeRequests(packet, &resolver);
        ITERATE ( CID2Processor::TReplies, it, replies ) {
            CRef<CID2_Reply> reply = *it;
            if ( GetDebugLevel() >= eTraceConn   ) {
                CDebugPrinter s(0, "CId2Reader");
                s << "Received from processor";
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
            int num = reply->GetSerial_number() - start_serial_num;
            if ( reply->IsSetDiscard() ||
                 num < 0 || num >= request_count || done[num] ) {
                // unknown serial num - bad reply
                NCBI_THROW_FMT(CLoaderException, eOtherError,
                               "CId2ReaderBase: bad reply serial number");
            }
            try {
                x_ProcessReply(result, loaded_sets[num], *reply);
            }
            catch ( CException& exc ) {
                NCBI_RETHROW(exc, CLoaderException, eOtherError,
                             "CId2ReaderBase: failed to process reply");
            }
            if ( reply->IsSetEnd_of_reply() ) {
                done[num] = true;
                x_UpdateLoadedSet(result, loaded_sets[num], sel);
                --remaining_count;
            }
        }
        if ( size_t(remaining_count) != packet.Get().size() ) {
            NCBI_THROW(CLoaderException, eOtherError,
                       "CId2ReaderBase: processor discrepancy");
        }
        if ( packet.Get().empty() ) {
            return;
        }
    }

    CConn conn(result, this);
    CRef<CID2_Reply> reply;
    try {
        // send request

        CProcessor::OffsetAllGisFromOM(packet);
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
            CProcessor::OffsetAllGisToOM(*reply);
            int num = reply->GetSerial_number() - start_serial_num;
            if ( reply->IsSetDiscard() ) {
                // discard whole reply for now
                continue;
            }
            if ( num < 0 || num >= request_count || done[num] ) {
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
        SetAndSaveStringSeq_ids(result, it->first,
                                CFixedSeq_ids(eTakeOwnership,
                                              it->second.second,
                                              it->second.first));
    }
    data.m_Seq_idsByString.clear();
    NON_CONST_ITERATE ( SId2LoadedSet::TSeq_idSeq_idsSet, it,
                        data.m_Seq_ids ) {
        SetAndSaveSeq_idSeq_ids(result, it->first,
                                CFixedSeq_ids(eTakeOwnership,
                                              it->second.second,
                                              it->second.first));
    }
    data.m_Seq_ids.clear();
    ITERATE ( SId2LoadedSet::TBlob_idSet, it, data.m_Blob_ids ) {
        CLoadLockBlobIds ids(result, it->first, sel);
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
                CID2S_Seq_annot_Info& annot_info = it3->GetNCObject();
                if ( !blob_annot_info ) {
                    blob_annot_info = new CBlob_Annot_Info;
                }
                if ( (it2->second.m_ContentMask & fBlobHasNamedAnnot) &&
                     annot_info.IsSetName() ) {
                    blob_annot_info->AddNamedAnnotName(annot_info.GetName());
                    // Heuristics to determine incorrect annot info records.
                    if ( (annot_info.IsSetAlign() || annot_info.IsSetFeat()) &&
                         annot_info.IsSetGraph() && ainfos.size() == 1 &&
                         !ExtractZoomLevel(annot_info.GetName(), 0, 0) ) {
                        // graphs are suppozed to be zoom tracks
                        for ( int zoom = 10; zoom < 1000000; zoom *= 10 ) {
                            CRef<CID2S_Seq_annot_Info> zoom_info;
                            zoom_info = SerialClone(annot_info);
                            zoom_info->ResetFeat();
                            zoom_info->ResetAlign();
                            zoom_info->SetName(CombineWithZoomLevel(annot_info.GetName(), zoom));
                            blob_annot_info->AddAnnotInfo(*zoom_info);
                        }
                        annot_info.ResetGraph();
                    }
                }

                if ( annot_info.IsSetName() &&
                     annot_info.IsSetSeq_loc() &&
                     (annot_info.IsSetAlign() ||
                      annot_info.IsSetGraph() ||
                      annot_info.IsSetFeat()) ) {
                    // complete annot info
                    blob_annot_info->AddAnnotInfo(annot_info);
                }
            }
            if ( blob_annot_info &&
                 !(blob_annot_info->GetAnnotInfo().empty() &&
                   blob_annot_info->GetNamedAnnotNames().empty()) ) {
                blob_info.SetAnnotInfo(blob_annot_info);
            }
            blob_ids.push_back(blob_info);
        }
        SetAndSaveSeq_idBlob_ids(result, it->first, sel, ids,
                                 CFixedBlob_ids(eTakeOwnership, blob_ids,
                                                state));
    }
}


inline void CId2ReaderBase::sx_CheckErrorFlag(const CID2_Error& error,
                                              TErrorFlags& error_flags,
                                              EErrorFlags test_flag,
                                              const char* marker1,
                                              const char* marker2)
{
    if ( !error.IsSetMessage() ) {
        // no message to parse
        return;
    }
    if ( error_flags & test_flag ) {
        // already set
        return;
    }
    SIZE_TYPE pos = NStr::FindNoCase(error.GetMessage(), marker1);
    if ( pos == NPOS) {
        // no marker
        return;
    }
    if ( marker2 &&
         NStr::FindNoCase(error.GetMessage(), marker2, pos) == NPOS ) {
        // no second marker
        return;
    }
    error_flags |= test_flag;
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
        if ( error.IsSetMessage() ) {
            sx_CheckErrorFlag(error, error_flags,
                              fError_inactivity_timeout, "timed", "out");
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
            sx_CheckErrorFlag(error, error_flags,
                              fError_warning_dead, "obsolete");
            sx_CheckErrorFlag(error, error_flags,
                              fError_suppressed_perm, "removed");
            sx_CheckErrorFlag(error, error_flags,
                              fError_suppressed_perm, "suppressed");
            sx_CheckErrorFlag(error, error_flags,
                              fError_suppressed_perm, "superceded"); // temp?
            sx_CheckErrorFlag(error, error_flags,
                              fError_suppressed_temp, "superseded"); // perm?
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
        if ( error.IsSetMessage() ) {
            sx_CheckErrorFlag(error, error_flags,
                              fError_withdrawn, "withdrawn");
            sx_CheckErrorFlag(error, error_flags,
                              fError_withdrawn, "removed");
        }
        if ( !(error_flags & fError_withdrawn) ) {
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
CId2ReaderBase::x_GetBlobStateFromID2(const CBlob_id& blob_id,
                                      SId2LoadedSet& loaded_set,
                                      int id2_state)
{
    TBlobState blob_state = 0;
    if ( id2_state & (1<<eID2_Blob_State_suppressed_temp) ) {
        blob_state |= CBioseq_Handle::fState_suppress_temp;
    }
    if ( id2_state & (1<<eID2_Blob_State_suppressed) ) {
        blob_state |= CBioseq_Handle::fState_suppress_perm;
    }
    if ( id2_state & (1<<eID2_Blob_State_dead) ) {
        blob_state |= CBioseq_Handle::fState_dead;
    }
    if ( id2_state & (1<<eID2_Blob_State_protected) ) {
        blob_state |= CBioseq_Handle::fState_confidential;
        blob_state |= CBioseq_Handle::fState_no_data;
    }
    if ( id2_state & (1<<eID2_Blob_State_withdrawn) ) {
        blob_state |= CBioseq_Handle::fState_withdrawn;
        blob_state |= CBioseq_Handle::fState_no_data;
    }
    if ( blob_state ) {
        loaded_set.m_BlobStates[blob_id] |= blob_state;
    }
    return blob_state;
}



CReader::TBlobState
CId2ReaderBase::x_GetBlobState(const CBlob_id& blob_id,
                               SId2LoadedSet& loaded_set,
                               const CID2_Reply& reply,
                               TErrorFlags* errors_ptr)
{
    SId2LoadedSet::TBlobStates::const_iterator it =
        loaded_set.m_BlobStates.find(blob_id);
    if ( it != loaded_set.m_BlobStates.end() ) {
        return it->second;
    }

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
    if ( errors & fError_suppressed_perm ) {
        blob_state |= CBioseq_Handle::fState_suppress_perm;
    }
    else if ( errors & fError_suppressed_temp ) {
        blob_state |= CBioseq_Handle::fState_suppress_temp;
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
    CLoadLockSeqIds ids(result, seq_id);
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
        SetAndSaveNoStringSeq_ids(result, seq_id, state);
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
            SetAndSaveStringSeq_ids(result, seq_id,
                                    CFixedSeq_ids(eTakeOwnership,
                                                  seq_ids,
                                                  state));
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
                SetAndSaveStringGi(result, seq_id, (**it).GetGi());
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


static bool sx_IsSpecialId(const CSeq_id& id)
{
    if ( !id.IsGeneral() ) {
        return false;
    }
    const string& db = id.GetGeneral().GetDb();
    return db == kSpecialId_length || db == kSpecialId_type;
}


void CId2ReaderBase::x_ProcessGetSeqIdSeqId(
    CReaderRequestResult& result,
    SId2LoadedSet& loaded_set,
    const CID2_Reply& main_reply,
    const CSeq_id_Handle& seq_id,
    const CID2_Reply_Get_Seq_id& reply)
{
    int state = 0;
    TErrorFlags errors = x_GetMessageError(main_reply);
    const CID2_Request_Get_Seq_id& req = reply.GetRequest();
    if ( errors & fError_no_data ) {
        state |= CBioseq_Handle::fState_no_data;
        // no Seq-ids
        if ( errors & fError_restricted ) {
            state |= CBioseq_Handle::fState_confidential;
        }
        if ( errors & fError_withdrawn ) {
            state |= CBioseq_Handle::fState_withdrawn;
        }
        SetAndSaveNoSeq_idSeq_ids(result, seq_id, state);
        
        if ( req.GetSeq_id_type() & req.eSeq_id_type_gi ) {
            SetAndSaveSeq_idGi(result, seq_id, ZERO_GI);
        }
        if ( req.GetSeq_id_type() & req.eSeq_id_type_text ) {
            SetAndSaveSeq_idAccVer(result, seq_id, CSeq_id_Handle());
        }
        if ( req.GetSeq_id_type() & req.eSeq_id_type_label ) {
            SetAndSaveSeq_idLabel(result, seq_id, "");
        }
        if ( req.GetSeq_id_type() & req.eSeq_id_type_taxid ) {
            SetAndSaveSeq_idTaxId(result, seq_id, -1);
        }
        if ( req.GetSeq_id_type() & req.eSeq_id_type_hash ) {
            SetAndSaveSequenceHash(result, seq_id, 0);
        }
        if ( req.GetSeq_id_type() & req.eSeq_id_type_seq_length ) {
            SetAndSaveSequenceLength(result, seq_id, kInvalidSeqPos);
        }
        if ( req.GetSeq_id_type() & req.eSeq_id_type_seq_mol ) {
            SetAndSaveSequenceType(result, seq_id, CSeq_inst::eMol_not_set);
        }
        return;
    }
    bool got_no_ids = false;
    if ( (req.GetSeq_id_type()&req.eSeq_id_type_all)==req.eSeq_id_type_all ) {
        CReader::TSeqIds seq_ids;
        ITERATE ( CID2_Reply_Get_Seq_id::TSeq_id, it, reply.GetSeq_id() ) {
            if ( req.GetSeq_id_type() != req.eSeq_id_type_all &&
                 sx_IsSpecialId(**it) ) {
                continue;
            }
            seq_ids.push_back(CSeq_id_Handle::GetHandle(**it));
        }
        if ( reply.IsSetEnd_of_reply() ) {
            got_no_ids = seq_ids.empty();
            SetAndSaveSeq_idSeq_ids(result, seq_id,
                                    CFixedSeq_ids(eTakeOwnership,
                                                  seq_ids,
                                                  state));
        }
        else {
            loaded_set.m_Seq_ids[seq_id].first = state;
            loaded_set.m_Seq_ids[seq_id].second.swap(seq_ids);
        }
    }
    if ( req.GetSeq_id_type() & req.eSeq_id_type_gi ) {
        ITERATE ( CID2_Reply_Get_Seq_id::TSeq_id, it, reply.GetSeq_id() ) {
            if ( (**it).IsGi() ) {
                SetAndSaveSeq_idGi(result, seq_id, (**it).GetGi());
                break;
            }
        }
    }
    if ( req.GetSeq_id_type() & req.eSeq_id_type_text ) {
        CSeq_id_Handle text_id;
        ITERATE ( CID2_Reply_Get_Seq_id::TSeq_id, it, reply.GetSeq_id() ) {
            if ( (**it).GetTextseq_Id() ) {
                text_id = CSeq_id_Handle::GetHandle(**it);
                break;
            }
        }
        SetAndSaveSeq_idAccVer(result, seq_id, text_id);
    }
    if ( req.GetSeq_id_type() & req.eSeq_id_type_label ) {
        ITERATE ( CID2_Reply_Get_Seq_id::TSeq_id, it, reply.GetSeq_id() ) {
            const CSeq_id& id = **it;
            if ( id.IsGeneral() ) {
                const CDbtag& dbtag = id.GetGeneral();
                const CObject_id& obj_id = dbtag.GetTag();
                if ( obj_id.IsStr() && dbtag.GetDb() == kSpecialId_label ) {
                    SetAndSaveSeq_idLabel(result, seq_id, obj_id.GetStr());
                    break;
                }
            }
        }
    }
    if ( req.GetSeq_id_type() & req.eSeq_id_type_taxid ) {
        ESave save = eDoNotSave;
        int taxid = -1;
        ITERATE ( CID2_Reply_Get_Seq_id::TSeq_id, it, reply.GetSeq_id() ) {
            const CSeq_id& id = **it;
            if ( id.IsGeneral() ) {
                const CDbtag& dbtag = id.GetGeneral();
                const CObject_id& obj_id = dbtag.GetTag();
                if ( obj_id.IsId() && dbtag.GetDb() == kSpecialId_taxid ) {
                    taxid = obj_id.GetId();
                    save = eSave;
                    break;
                }
            }
        }
        SetAndSaveSeq_idTaxId(result, seq_id, taxid, save);
    }
    if ( req.GetSeq_id_type() & req.eSeq_id_type_hash ) {
        ESave save = eDoNotSave;
        int hash = 0;
        ITERATE ( CID2_Reply_Get_Seq_id::TSeq_id, it, reply.GetSeq_id() ) {
            const CSeq_id& id = **it;
            if ( id.IsGeneral() ) {
                const CDbtag& dbtag = id.GetGeneral();
                const CObject_id& obj_id = dbtag.GetTag();
                if ( obj_id.IsId() && dbtag.GetDb() == kSpecialId_hash ) {
                    hash = obj_id.GetId();
                    save = eSave;
                    break;
                }
            }
        }
        SetAndSaveSequenceHash(result, seq_id, hash, save);
    }
    if ( req.GetSeq_id_type() & req.eSeq_id_type_seq_length ) {
        ESave save = eDoNotSave;
        TSeqPos length = kInvalidSeqPos;
        ITERATE ( CID2_Reply_Get_Seq_id::TSeq_id, it, reply.GetSeq_id() ) {
            const CSeq_id& id = **it;
            if ( id.IsGeneral() ) {
                const CDbtag& dbtag = id.GetGeneral();
                const CObject_id& obj_id = dbtag.GetTag();
                if ( obj_id.IsId() && dbtag.GetDb() == kSpecialId_length ) {
                    length = TSeqPos(obj_id.GetId());
                    save = eSave;
                    break;
                }
            }
        }
        if ( save != eDoNotSave || got_no_ids ) {
            SetAndSaveSequenceLength(result, seq_id, length, save);
        }
    }
    if ( req.GetSeq_id_type() & req.eSeq_id_type_seq_mol ) {
        ESave save = eDoNotSave;
        CSeq_inst::EMol type = CSeq_inst::eMol_not_set;
        ITERATE ( CID2_Reply_Get_Seq_id::TSeq_id, it, reply.GetSeq_id() ) {
            const CSeq_id& id = **it;
            if ( id.IsGeneral() ) {
                const CDbtag& dbtag = id.GetGeneral();
                const CObject_id& obj_id = dbtag.GetTag();
                if ( obj_id.IsId() && dbtag.GetDb() == kSpecialId_type ) {
                    type = CSeq_inst::EMol(obj_id.GetId());
                    save = eSave;
                    break;
                }
            }
        }
        if ( save != eDoNotSave || got_no_ids ) {
            SetAndSaveSequenceType(result, seq_id, type, save);
        }
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
    const CID2_Blob_Id& src_blob_id = reply.GetBlob_id();
    CBlob_id blob_id = GetBlobId(src_blob_id);
    TErrorFlags errors = 0;
    TBlobState blob_state;
    if ( reply.IsSetBlob_state() ) {
        blob_state = x_GetBlobStateFromID2(blob_id, loaded_set,
                                           reply.GetBlob_state());
    }
    else {
        blob_state = x_GetBlobState(blob_id, loaded_set, main_reply, &errors);
    }
    if ( blob_state & CBioseq_Handle::fState_no_data ) {
        SetAndSaveNoSeq_idBlob_ids(result, idh, 0, blob_state);
        return;
    }
    if ( (blob_state == 0) && (errors & fError_warning) ) {
        blob_state |= CBioseq_Handle::fState_other_error;
    }
    
    SId2LoadedSet::TBlob_idsInfo& ids = loaded_set.m_Blob_ids[idh];
    ids.first |= blob_state;
    if ( blob_state ) {
        loaded_set.m_BlobStates[blob_id] |= blob_state;
    }
    TContentsMask mask = 0;
    {{ // TODO: temporary logic, this info should be returned by server
        if ( blob_id.GetSubSat() == CID2_Blob_Id::eSub_sat_main ||
             blob_id.GetSat() == CProcessor_ExtAnnot::eSat_VDB_WGS ) {
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
    TChunkId chunk_id = kMain_ChunkId;
    const CID2_Blob_Id& src_blob_id = reply.GetBlob_id();
    TBlobId blob_id = GetBlobId(src_blob_id);

    TBlobVersion blob_version = 0;
    if ( src_blob_id.IsSetVersion() && src_blob_id.GetVersion() > 0 ) {
        blob_version = src_blob_id.GetVersion();
        SetAndSaveBlobVersion(result, blob_id, blob_version);
    }

    TBlobState blob_state;
    if ( reply.IsSetBlob_state() ) {
        blob_state = x_GetBlobStateFromID2(blob_id, loaded_set,
                                           reply.GetBlob_state());
    }
    else {
        blob_state = x_GetBlobState(blob_id, loaded_set, main_reply);
    }
    if ( blob_state & CBioseq_Handle::fState_no_data ) {
        SetAndSaveNoBlob(result, blob_id, chunk_id, blob_state);
        return;
    }
    
    if ( !blob_version ) {
        CLoadLockBlobVersion lock(result, blob_id);
        if ( !lock.IsLoadedBlobVersion() ) {
            // need some reference blob version to work with
            // but not save it into cache
            SetAndSaveBlobVersion(result, blob_id, 0);
            //state.SetLoadedBlobVersion(0);
        }
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
        ERR_POST_X(7, "CId2ReaderBase: ID2-Reply-Get-Blob: "
                   "no data in reply: "<<blob_id);
        SetAndSaveNoBlob(result, blob_id, chunk_id, blob_state);
        return;
    }

    if ( reply.GetSplit_version() != 0 ) {
        // split info will follow
        // postpone parsing this blob
        loaded_set.m_Skeletons[blob_id] = &data;
        return;
    }

    CLoadLockBlob blob(result, blob_id);
    if ( blob.IsLoadedBlob() ) {
        if ( blob.NeedsDelayedMainChunk() ) {
            chunk_id = kDelayedMain_ChunkId;
            blob.SelectChunk(chunk_id);
        }
        if ( blob.IsLoadedChunk() ) {
            m_AvoidRequest |= fAvoidRequest_nested_get_blob_info;
            ERR_POST_X(4, Info << "CId2ReaderBase: ID2-Reply-Get-Blob: "
                          "blob already loaded: "<<blob_id);
            return;
        }
    }

    if ( blob_state ) {
        result.SetAndSaveBlobState(blob_id, blob_state);
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
    _ASSERT(blob.IsLoadedChunk());
}


void CId2ReaderBase::x_ProcessGetSplitInfo(
    CReaderRequestResult& result,
    SId2LoadedSet& loaded_set,
    const CID2_Reply& main_reply,
    const CID2S_Reply_Get_Split_Info& reply)
{
    TChunkId chunk_id = kMain_ChunkId;
    const CID2_Blob_Id& src_blob_id = reply.GetBlob_id();
    TBlobId blob_id = GetBlobId(src_blob_id);
    TBlobVersion blob_version = 0;
    if ( src_blob_id.IsSetVersion() && src_blob_id.GetVersion() > 0 ) {
        blob_version = src_blob_id.GetVersion();
        SetAndSaveBlobVersion(result, blob_id, blob_version);
    }
    if ( !reply.IsSetData() ) {
        ERR_POST_X(11, "CId2ReaderBase: ID2S-Reply-Get-Split-Info: "
                       "no data in reply: "<<blob_id);
        return;
    }

    if ( !blob_version ) {
        CLoadLockBlobVersion lock(result, blob_id);
        if ( !lock.IsLoadedBlobVersion() ) {
            // need some reference blob version to work with
            // but not save it into cache
            SetAndSaveBlobVersion(result, blob_id, 0);
            //state.SetLoadedBlobVersion(0);
        }
    }

    CLoadLockBlob blob(result, blob_id);
    if ( blob.IsLoadedBlob() ) {
        if ( blob.NeedsDelayedMainChunk() ) {
            chunk_id = kDelayedMain_ChunkId;
            blob.SelectChunk(chunk_id);
        }
        if ( blob.IsLoadedChunk() ) {
            m_AvoidRequest |= fAvoidRequest_nested_get_blob_info;
            ERR_POST_X(10, Info<<"CId2ReaderBase: ID2S-Reply-Get-Split-Info: "
                       "blob already loaded: " << blob_id);
            return;
        }
    }

    TBlobState blob_state;
    if ( reply.IsSetBlob_state() ) {
        blob_state = x_GetBlobStateFromID2(blob_id, loaded_set,
                                           reply.GetBlob_state());
    }
    else {
        blob_state = x_GetBlobState(blob_id, loaded_set, main_reply);
    }
    if ( blob_state & CBioseq_Handle::fState_no_data ) {
        SetAndSaveNoBlob(result, blob_id, chunk_id, blob_state);
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
        result.SetAndSaveBlobState(blob_id, blob_state);
    }

    dynamic_cast<const CProcessor_ID2&>
        (m_Dispatcher->GetProcessor(CProcessor::eType_ID2))
        .ProcessData(result, blob_id, blob_state, chunk_id,
                     reply.GetData(), reply.GetSplit_version(), skel);

    _ASSERT(blob.IsLoadedChunk());
    loaded_set.m_Skeletons.erase(blob_id);
}


void CId2ReaderBase::x_ProcessGetChunk(
    CReaderRequestResult& result,
    SId2LoadedSet& /*loaded_set*/,
    const CID2_Reply& /*main_reply*/,
    const CID2S_Reply_Get_Chunk& reply)
{
    TBlobId blob_id = GetBlobId(reply.GetBlob_id());
    if ( !reply.IsSetData() ) {
        ERR_POST_X(14, "CId2ReaderBase: ID2S-Reply-Get-Chunk: "
                       "no data in reply: "<<blob_id);
        return;
    }

    if ( !CLoadLockBlob(result, blob_id).IsLoadedBlob() ) {
        ERR_POST_X(13, "CId2ReaderBase: ID2S-Reply-Get-Chunk: "
                       "blob is not loaded yet: " << blob_id);
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
