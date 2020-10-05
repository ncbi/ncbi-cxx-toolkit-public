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
 * Authors: Eugene Vasilchenko
 *
 * File Description: processor for data from OSG
 *
 */

#include <ncbi_pch.hpp>

#include "osg_getblob_base.hpp"

#include <objects/id2/id2__.hpp>
#include <objects/seqsplit/seqsplit__.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/bioseq_info/record.hpp>
#include "pubseq_gateway_convert_utils.hpp"

BEGIN_NCBI_NAMESPACE;
BEGIN_NAMESPACE(psg);
BEGIN_NAMESPACE(osg);

static CPSGS_OSGGetBlobBase::TID2SplitVersion kSplitInfo_SplitVersion = 999999999;

CPSGS_OSGGetBlobBase::CPSGS_OSGGetBlobBase()
{
}


CPSGS_OSGGetBlobBase::~CPSGS_OSGGetBlobBase()
{
}


void CPSGS_OSGGetBlobBase::x_SetSplitVersion(const CID2_Blob_Id& osg_blob_id,
                                             TID2SplitVersion split_version)
{
    m_PSGBlobId2SplitVersion[GetPSGBlobId(osg_blob_id)] = split_version;
}


CPSGS_OSGGetBlobBase::TID2SplitVersion
CPSGS_OSGGetBlobBase::x_GetSplitVersion(const CID2_Blob_Id& osg_blob_id)
{
    return m_PSGBlobId2SplitVersion[GetPSGBlobId(osg_blob_id)];
}


void CPSGS_OSGGetBlobBase::ProcessBlobReply(const CID2_Reply& reply)
{
    switch ( reply.GetReply().Which() ) {
    case CID2_Reply::TReply::e_Get_blob:
        if ( m_Blob ) {
            ERR_POST(GetName()<<": "
                     "Duplicate blob reply: "<<MSerial_AsnText<<reply);
        }
        m_Blob = &reply.GetReply().GetGet_blob();
        break;
    case CID2_Reply::TReply::e_Get_split_info:
        if ( m_SplitInfo ) {
            ERR_POST(GetName()<<": "
                     "Duplicate blob reply: "<<MSerial_AsnText<<reply);
        }
        m_SplitInfo = &reply.GetReply().GetGet_split_info();
        break;
    case CID2_Reply::TReply::e_Get_chunk:
        if ( m_Chunk ) {
            ERR_POST(GetName()<<": "
                     "Duplicate blob reply: "<<MSerial_AsnText<<reply);
        }
        m_Chunk = &reply.GetReply().GetGet_chunk();
        break;
    default:
        ERR_POST(GetName()<<": "
                 "Invalid blob reply: "<<MSerial_AsnText<<reply);
        break;
    }
}


void CPSGS_OSGGetBlobBase::x_SetBlobVersion(CBlobRecord& blob_props,
                                            const CID2_Blob_Id& blob_id)
{
    if ( blob_id.IsSetVersion() ) {
        blob_props.SetModified(int64_t(blob_id.GetVersion())*60000);
    }
}


void CPSGS_OSGGetBlobBase::x_SetBlobDataProps(CBlobRecord& blob_props,
                                              const CID2_Reply_Data& data)
{
    if ( data.GetData_compression() == data.eData_compression_gzip ) {
        blob_props.SetGzip(true);
    }
    blob_props.SetNChunks(data.GetData().size());
}


void CPSGS_OSGGetBlobBase::x_SetBlobState(CBlobRecord& blob_props,
                                          TID2BlobState blob_state)
{
    if ( blob_state & (1<<eID2_Blob_State_withdrawn) ) {
        blob_props.SetWithdrawn(true);
    }
    if ( blob_state & ((1<<eID2_Blob_State_suppressed)|
                       (1<<eID2_Blob_State_suppressed_temp)) ) {
        blob_props.SetSuppress(true);
    }
    if ( blob_state & (1<<eID2_Blob_State_dead) ) {
        blob_props.SetDead(true);
    }
}


void CPSGS_OSGGetBlobBase::x_SendBlobProps(const string& psg_blob_id,
                                           CBlobRecord& blob_props)
{
    size_t item_id = GetReply()->GetItemId();
    string data_to_send = ToJson(blob_props).Repr(CJsonNode::fStandardJson);
    if ( GetDebugLevel() >= eDebug_exchange ) {
        LOG_POST(GetDiagSeverity() << "OSG: "
                 "Sending blob_prop("<<psg_blob_id<<"): "<<data_to_send);
    }
    GetReply()->PrepareBlobPropData(item_id, GetName(), psg_blob_id, data_to_send);
    GetReply()->PrepareBlobPropCompletion(item_id, GetName(), 2);
}


void CPSGS_OSGGetBlobBase::x_SendChunkBlobProps(const string& id2_info,
                                                TID2ChunkId chunk_id,
                                                CBlobRecord& blob_props)
{
    size_t item_id = GetReply()->GetItemId();
    string data_to_send = ToJson(blob_props).Repr(CJsonNode::fStandardJson);
    if ( GetDebugLevel() >= eDebug_exchange ) {
        LOG_POST(GetDiagSeverity() << "OSG: "
                 "Sending chunk blob_prop("<<id2_info<<','<<chunk_id<<"): "<<data_to_send);
    }
    GetReply()->PrepareTSEBlobPropData(item_id, GetName(), chunk_id, id2_info, data_to_send);
    GetReply()->PrepareBlobPropCompletion(item_id, GetName(), 2);
}


void CPSGS_OSGGetBlobBase::x_SendBlobData(const string& psg_blob_id,
                                          const CID2_Reply_Data& data)
{
    size_t item_id = GetReply()->GetItemId();
    int chunk_no = 0;
    for ( auto& chunk : data.GetData() ) {
        GetReply()->PrepareBlobData(item_id, GetName(), psg_blob_id,
                                    (const unsigned char*)chunk->data(), chunk->size(), chunk_no++);
    }
    GetReply()->PrepareBlobCompletion(item_id, GetName(),
                                      psg_blob_id, chunk_no+1);
}


void CPSGS_OSGGetBlobBase::x_SendChunkBlobData(const string& id2_info,
                                               TID2ChunkId chunk_id,
                                               const CID2_Reply_Data& data)
{
    size_t item_id = GetReply()->GetItemId();
    int chunk_no = 0;
    for ( auto& chunk : data.GetData() ) {
        GetReply()->PrepareTSEBlobData(item_id, GetName(),
                                       (const unsigned char*)chunk->data(), chunk->size(), chunk_no++,
                                       chunk_id, id2_info);
    }
    GetReply()->PrepareTSEBlobCompletion(item_id, GetName(),
                                         chunk_id, id2_info, chunk_no+1);
}


string CPSGS_OSGGetBlobBase::x_GetSplitInfoPSGBlobId(const string& main_blob_id)
{
    return main_blob_id + '.';
}


string CPSGS_OSGGetBlobBase::x_GetChunkPSGBlobId(const string& main_blob_id,
                                                 TID2ChunkId chunk_id)
{
    return main_blob_id + '.' + to_string(chunk_id);
}


void CPSGS_OSGGetBlobBase::x_SendMainEntry(const CID2_Blob_Id& osg_blob_id,
                                           TID2BlobState blob_state,
                                           const CID2_Reply_Data& data)
{
    string main_blob_id = GetPSGBlobId(osg_blob_id);

    CBlobRecord main_blob_props;
    x_SetBlobVersion(main_blob_props, osg_blob_id);
    x_SetBlobState(main_blob_props, blob_state);
    x_SetBlobDataProps(main_blob_props, data);
    x_SendBlobProps(main_blob_id, main_blob_props);
    x_SendBlobData(main_blob_id, data);
}


void CPSGS_OSGGetBlobBase::x_SendSplitInfo(const CID2_Blob_Id& osg_blob_id,
                                           TID2BlobState blob_state,
                                           TID2SplitVersion split_version,
                                           const CID2_Reply_Data& data)
{
    // first send main blob props
    string main_blob_id = GetPSGBlobId(osg_blob_id);
    string id2_info = GetPSGId2Info(osg_blob_id, split_version);
    
    CBlobRecord main_blob_props;
    x_SetBlobVersion(main_blob_props, osg_blob_id);
    x_SetBlobState(main_blob_props, blob_state);
    main_blob_props.SetId2Info(id2_info);
    x_SendBlobProps(main_blob_id, main_blob_props);
    
    CBlobRecord split_info_blob_props;
    x_SetBlobDataProps(split_info_blob_props, data);
    x_SendChunkBlobProps(id2_info, kSplitInfo_SplitVersion, split_info_blob_props);
    x_SendChunkBlobData(id2_info, kSplitInfo_SplitVersion, data);
}


void CPSGS_OSGGetBlobBase::x_SendChunk(const CID2_Blob_Id& osg_blob_id,
                                       TID2ChunkId chunk_id,
                                       const CID2_Reply_Data& data)
{
    string id2_info = GetPSGId2Info(osg_blob_id, x_GetSplitVersion(osg_blob_id));
    
    CBlobRecord chunk_blob_props;
    x_SetBlobDataProps(chunk_blob_props, data);
    x_SendChunkBlobProps(id2_info, chunk_id, chunk_blob_props);
    x_SendChunkBlobData(id2_info, chunk_id, data);
}


void CPSGS_OSGGetBlobBase::SendBlob()
{
    if ( m_SplitInfo && !m_Blob ) {
        // split_info with blob inside
        x_SetSplitVersion(m_SplitInfo->GetBlob_id(), m_SplitInfo->GetSplit_version());
        x_SendSplitInfo(m_SplitInfo->GetBlob_id(),
                        x_GetBlobState(*m_SplitInfo),
                        m_SplitInfo->GetSplit_version(),
                        m_SplitInfo->GetData());
        SetFinalStatus(ePSGS_Found);
    }
    else if ( m_Blob && !m_SplitInfo && m_Blob->IsSetData() &&
              m_Blob->GetData().GetData_type() == CID2_Reply_Data::eData_type_id2s_split_info ) {
        // split_info with blob inside in a get-blob reply
        // TODO: really???
        x_SendSplitInfo(m_Blob->GetBlob_id(),
                        x_GetBlobState(*m_Blob),
                        0,
                        m_Blob->GetData());
        SetFinalStatus(ePSGS_Found);
    }
    else if ( m_Blob && !m_SplitInfo && m_Blob->IsSetData() ) {
        // blob only
        x_SendMainEntry(m_Blob->GetBlob_id(),
                        x_GetBlobState(*m_Blob),
                        m_Blob->GetData());
        SetFinalStatus(ePSGS_Found);
    }
    else if ( m_Blob && !m_SplitInfo && !m_Blob->IsSetData() ) {
        // no blob reply TODO
        SetFinalStatus(ePSGS_NotFound);
    }
    else if ( m_Blob && m_SplitInfo ) {
        // separate blob and m_SplitInfo TODO
        SetFinalStatus(ePSGS_NotFound);
    }
    else if ( m_Chunk ) {
        x_SendChunk(m_Chunk->GetBlob_id(),
                    m_Chunk->GetChunk_id(),
                    m_Chunk->GetData());
        SetFinalStatus(ePSGS_Found);
    }
    else {
        // nothing TODO
        SetFinalStatus(ePSGS_NotFound);
    }
}


/////////////////////////////////////////////////////////////////////////////
// Blob id parsing methods
/////////////////////////////////////////////////////////////////////////////


static const char kSubSatSeparator = '/';
static const int kOSG_Sat_WGS_min = 1000;
static const int kOSG_Sat_WGS_max = 1130;
static const int kOSG_Sat_SNP_min = 2001;
static const int kOSG_Sat_SNP_max = 2099;
static const int kOSG_Sat_CDD_min = 8087;
static const int kOSG_Sat_CDD_max = 8087;
//static const int kOSG_Sat_NAGraph_min = 8000;
//static const int kOSG_Sat_NAGraph_max = 8000;


static bool s_IsOSGBlob(Int4 sat, Int4 /*subsat*/, Int4 /*satkey*/)
{
    if ( sat >= kOSG_Sat_WGS_min &&
         sat <= kOSG_Sat_WGS_max ) {
        return true;
    }
    if ( sat >= kOSG_Sat_SNP_min &&
         sat <= kOSG_Sat_SNP_max ) {
        return true;
    }
    if ( sat >= kOSG_Sat_CDD_min &&
         sat <= kOSG_Sat_CDD_max ) {
        return true;
    }
    /*
    if ( sat >= kOSG_Sat_NAGraph_min &&
         sat <= kOSG_Sat_NAGraph_max ) {
        return true;
    }
    */
    return false;
}


static bool s_Skip(CTempString& str, char c)
{
    if ( str.empty() || str[0] != c ) {
        return false;
    }
    str = str.substr(1);
    return true;
}


static inline bool s_IsValidIntChar(char c)
{
    return c == '-' || (c >= '0' && c <= '9');
}


template<class Int>
static bool s_ParseInt(CTempString& str, Int& v)
{
    size_t int_size = 0;
    while ( int_size < str.size() && s_IsValidIntChar(str[int_size]) ) {
        ++int_size;
    }
    if ( !NStr::StringToNumeric(str.substr(0, int_size), &v,
                                NStr::fConvErr_NoThrow|NStr::fConvErr_NoErrMessage) ) {
        return false;
    }
    str = str.substr(int_size);
    return true;
}


static bool s_ParseOSGBlob(CTempString& s,
                           Int4& sat, Int4& subsat, Int4& satkey)
{
    if ( s.find(kSubSatSeparator) == NPOS ) {
        return false;
    }
    if ( !s_ParseInt(s, sat) ) {
        return false;
    }
    if ( !s_Skip(s, kSubSatSeparator) ) {
        return false;
    }
    if ( !s_ParseInt(s, subsat) ) {
        return false;
    }
    if ( !s_Skip(s, '.') ) {
        return false;
    }
    if ( !s_ParseInt(s, satkey) ) {
        return false;
    }
    return s_IsOSGBlob(sat, subsat, satkey);
}


static void s_FormatBlobId(ostream& s, const CID2_Blob_Id& blob_id)
{
    s << blob_id.GetSat()
      << kSubSatSeparator << blob_id.GetSub_sat()
      << '.' << blob_id.GetSat_key();
}


CRef<CID2_Blob_Id> CPSGS_OSGGetBlobBase::ParsePSGBlobId(const SPSGS_BlobId& blob_id)
{
    Int4 sat;
    Int4 subsat;
    Int4 satkey;
    CTempString s = blob_id.GetId();
    if ( !s_ParseOSGBlob(s, sat, subsat, satkey) || !s.empty() ) {
        return null;
    }
    CRef<CID2_Blob_Id> id(new CID2_Blob_Id);
    id->SetSat(sat);
    id->SetSub_sat(subsat);
    id->SetSat_key(satkey);
    return id;
}


string CPSGS_OSGGetBlobBase::GetPSGBlobId(const CID2_Blob_Id& blob_id)
{
    ostringstream s;
    s_FormatBlobId(s, blob_id);
    return s.str();
}


string CPSGS_OSGGetBlobBase::GetPSGId2Info(const CID2_Blob_Id& tse_id,
                                           TID2SplitVersion split_version)
{
    ostringstream s;
    s_FormatBlobId(s, tse_id);
    TID2BlobVersion blob_version = tse_id.IsSetVersion()? tse_id.GetVersion(): 0;
    s << '.' << blob_version << '.' << split_version;
    return s.str();
}


CPSGS_OSGGetBlobBase::SParsedId2Info
CPSGS_OSGGetBlobBase::ParsePSGId2Info(const string& id2_info)
{
    Int4 sat;
    Int4 subsat;
    Int4 satkey;
    TID2BlobVersion tse_version;
    TID2SplitVersion split_version;
    
    CTempString s = id2_info;
    if ( !s_ParseOSGBlob(s, sat, subsat, satkey) ||
         !s_Skip(s, '.') ||
         !s_ParseInt(s, tse_version) ||
         !s_Skip(s, '.') ||
         !s_ParseInt(s, split_version) ||
         !s.empty() ) {
        return SParsedId2Info{};
    }
    
    CRef<CID2_Blob_Id> id(new CID2_Blob_Id);
    id->SetSat(sat);
    id->SetSub_sat(subsat);
    id->SetSat_key(satkey);
    id->SetVersion(tse_version);
    return SParsedId2Info{id, split_version};
}


END_NAMESPACE(osg);
END_NAMESPACE(psg);
END_NCBI_NAMESPACE;
