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

#include "osg_resolve.hpp"

#include <objects/general/Dbtag.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Textseq_id.hpp>
#include <objects/id2/id2__.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/bioseq_info/record.hpp>
#include "pubseq_gateway_convert_utils.hpp"
#include "osg_connection.hpp"
#include "osg_getblob_base.hpp"

BEGIN_NCBI_NAMESPACE;
BEGIN_NAMESPACE(psg);
BEGIN_NAMESPACE(osg);


CPSGS_OSGResolveBase::CPSGS_OSGResolveBase()
    : m_BioseqInfoFlags(0)
{
}


CPSGS_OSGResolveBase::~CPSGS_OSGResolveBase()
{
}


void CPSGS_OSGResolveBase::SetSeqId(CSeq_id& id, int seq_id_type, const string& seq_id)
{
    // TODO: seq_id_type
    id.Set(seq_id);
    //id.Set(CSeq_id::eFasta_AsTypeAndContent, CSeq_id_Base::E_Choice(seq_id_type), seq_id);
}


static const char kSpecialId_label[] = "LABEL";
static const char kSpecialId_taxid[] = "TAXID";
static const char kSpecialId_hash[] = "HASH";
static const char kSpecialId_length[] = "Seq-inst.length";
static const char kSpecialId_type[] = "Seq-inst.mol";


void CPSGS_OSGResolveBase::ProcessResolveReply(const CID2_Reply& reply)
{
    CBioseqInfoRecord::TSeqIds seq_ids;
    if ( reply.GetReply().IsGet_seq_id() ) {
        auto& reply_ids = reply.GetReply().GetGet_seq_id();
        auto& req_id = reply_ids.GetRequest();
        TGi gi = ZERO_GI;
        for ( auto& id : reply_ids.GetSeq_id() ) {
            if ( id->IsGeneral() ) {
                const CDbtag& dbtag = id->GetGeneral();
                const CObject_id& obj_id = dbtag.GetTag();
                if ( dbtag.GetDb() == kSpecialId_label ) {
                    //m_BioseqInfo.SetLabel(obj_id.GetStr());
                    //m_BioseqInfoFlags |= ;
                    continue;
                }
                if ( dbtag.GetDb() == kSpecialId_taxid ) {
                    m_BioseqInfo.SetTaxId(obj_id.GetId());
                    m_BioseqInfoFlags |= SPSGS_ResolveRequest::fPSGS_TaxId;
                    continue;
                }
                if ( dbtag.GetDb() == kSpecialId_hash ) {
                    m_BioseqInfo.SetHash(obj_id.GetId());
                    m_BioseqInfoFlags |= SPSGS_ResolveRequest::fPSGS_Hash;
                    continue;
                }
                if ( dbtag.GetDb() == kSpecialId_length ) {
                    m_BioseqInfo.SetLength(obj_id.GetId());
                    m_BioseqInfoFlags |= SPSGS_ResolveRequest::fPSGS_Length;
                    continue;
                }
                if ( dbtag.GetDb() == kSpecialId_type ) {
                    m_BioseqInfo.SetMol(obj_id.GetId());
                    m_BioseqInfoFlags |= SPSGS_ResolveRequest::fPSGS_MoleculeType;
                    continue;
                }
            }
            else if ( id->IsGi() ) {
                gi = id->GetGi();
                m_BioseqInfo.SetGI(GI_TO(CBioseqInfoRecord::TGI,gi));
                m_BioseqInfoFlags |= SPSGS_ResolveRequest::fPSGS_Gi;
                continue;
            }
            else if ( auto text_id = id->GetTextseq_Id() ) {
                // only versioned accession goes to canonical id
                if ( !(m_BioseqInfoFlags & SPSGS_ResolveRequest::fPSGS_CanonicalId) &&
                     text_id->IsSetAccession() && text_id->IsSetVersion() ) {
                    m_BioseqInfo.SetSeqIdType(id->Which());
                    m_BioseqInfo.SetAccession(text_id->GetAccession());
                    m_BioseqInfo.SetVersion(text_id->GetVersion());
                    if ( text_id->IsSetName() ) {
                        m_BioseqInfo.SetName(text_id->GetName());
                    }
                    m_BioseqInfoFlags |= SPSGS_ResolveRequest::fPSGS_CanonicalId;
                    continue;
                }
            }
            string content;
            id->GetLabel(&content, CSeq_id::eFastaContent);
            seq_ids.insert(make_tuple(id->Which(), move(content)));
        }
        if ( gi != ZERO_GI ) {
            // gi goes either to canonical id or to other ids
            CSeq_id gi_id(CSeq_id::e_Gi, gi);
            string content;
            gi_id.GetLabel(&content, CSeq_id::eFastaContent);
            if ( !(m_BioseqInfoFlags & SPSGS_ResolveRequest::fPSGS_CanonicalId) ) {
                // set canonical id from gi
                m_BioseqInfo.SetAccession(content);
                m_BioseqInfo.SetVersion(0);
                m_BioseqInfo.SetSeqIdType(gi_id.Which());
                m_BioseqInfoFlags |= SPSGS_ResolveRequest::fPSGS_CanonicalId;
            }
            else {
                // to other ids
                seq_ids.insert(make_tuple(gi_id.Which(), move(content)));
            }
        }
        if ( (req_id.GetSeq_id_type() & req_id.eSeq_id_type_all) == req_id.eSeq_id_type_all &&
             !seq_ids.empty() ) {
            m_BioseqInfo.SetSeqIds(move(seq_ids));
            // all ids are requested, so we should get GI and acc.ver too if they exist
            m_BioseqInfoFlags |= (SPSGS_ResolveRequest::fPSGS_SeqIds |
                                  SPSGS_ResolveRequest::fPSGS_CanonicalId |
                                  SPSGS_ResolveRequest::fPSGS_Gi);
        }
        else if ( req_id.GetSeq_id_type() == req_id.eSeq_id_type_any ) {
            // TODO?
        }
    }
    else if ( reply.GetReply().IsGet_blob_id()) {
        auto& reply_ids = reply.GetReply().GetGet_blob_id();
        if ( reply_ids.IsSetBlob_id() && CPSGS_OSGGetBlobBase::IsOSGBlob(reply_ids.GetBlob_id()) ) {
            const CID2_Blob_Id& blob_id = reply_ids.GetBlob_id();
            m_BlobId = CPSGS_OSGGetBlobBase::GetPSGBlobId(blob_id);
            m_BioseqInfoFlags |= SPSGS_ResolveRequest::fPSGS_BlobId;
            if ( blob_id.IsSetVersion() ) {
                // ID2 version is minutes since UNIX epoch
                // PSG date_changed is ms since UNIX epoch
                m_BioseqInfo.SetDateChanged(blob_id.GetVersion()*60000);
                m_BioseqInfoFlags |= SPSGS_ResolveRequest::fPSGS_DateChanged;
            }
            CID2_Reply_Get_Blob_Id::TBlob_state id2_state = 0;
            if ( reply_ids.IsSetBlob_state() ) {
                id2_state = reply_ids.GetBlob_state();
            }
            int psg_state = 0;
            if ( id2_state == 0 ) {
                psg_state = 10;
            }
            else if ( id2_state & 4 ) {
                psg_state = 5;
            }
            else if ( id2_state & 8 ) {
                psg_state = 0;
            }
            m_BioseqInfo.SetSeqState(psg_state);
            m_BioseqInfoFlags |= SPSGS_ResolveRequest::fPSGS_State;
        }
    }
    else {
        ERR_POST(GetName()<<": "
                 "Unknown reply "<<MSerial_AsnText<<reply);
    }
}


CPSGS_OSGResolveBase::EOutputFormat CPSGS_OSGResolveBase::GetOutputFormat(EOutputFormat format)
{
    if ( format == SPSGS_ResolveRequest::ePSGS_NativeFormat ||
         format == SPSGS_ResolveRequest::ePSGS_UnknownFormat ) {
        format = SPSGS_ResolveRequest::ePSGS_JsonFormat;
    }
    return format;
}


void CPSGS_OSGResolveBase::SendResult(const string& data_to_send,
                                      EOutputFormat output_format)
{
    size_t item_id = GetReply()->GetItemId();
    GetReply()->PrepareBioseqData(item_id, GetName(), data_to_send, output_format);
    GetReply()->PrepareBioseqCompletion(item_id, GetName(), 2);
}


void CPSGS_OSGResolveBase::SendBioseqInfo(EOutputFormat output_format)
{
    output_format = GetOutputFormat(output_format);

    /*
    if (bioseq_resolution.m_ResolutionResult == ePSGS_BioseqDB ||
        bioseq_resolution.m_ResolutionResult == ePSGS_BioseqCache)
        AdjustBioseqAccession(bioseq_resolution);
    */

    string data_to_send;
    if ( output_format == SPSGS_ResolveRequest::ePSGS_JsonFormat ) {
        data_to_send = ToJson(m_BioseqInfo, m_BioseqInfoFlags, m_BlobId).Repr(CJsonNode::fStandardJson);
        if ( GetDebugLevel() >= eDebug_exchange ) {
            LOG_POST(GetDiagSeverity() << "OSG: "
                     "Sending reply "<<data_to_send);
        }
    } else {
        data_to_send = ToBioseqProtobuf(m_BioseqInfo);
    }

    SendResult(data_to_send, output_format);
}


/////////////////////////////////////////////////////////////////////////////
// Common Seq-id identification code
/////////////////////////////////////////////////////////////////////////////

// WGS accession parameters
static const size_t kTypePrefixLen = 4; // "WGS:" or "TSA:"
static const size_t kNumLettersV1 = 4;
static const size_t kNumLettersV2 = 6;
static const size_t kVersionDigits = 2;
static const size_t kPrefixLenV1 = kNumLettersV1 + kVersionDigits;
static const size_t kPrefixLenV2 = kNumLettersV2 + kVersionDigits;
static const size_t kMinRowDigitsV1 = 6;
static const size_t kMaxRowDigitsV1 = 7;
static const size_t kMinRowDigitsV2 = 7;
static const size_t kMaxRowDigitsV2 = 9;

static const size_t kMinProtAccLen = 8; // 3+5
static const size_t kMaxProtAccLen = 10; // 3+7


static bool IsWGSGeneral(const CDbtag& dbtag)
{
    const string& db = dbtag.GetDb();
    if ( db.size() != kTypePrefixLen+kNumLettersV1 /* WGS:AAAA */ &&
         db.size() != kTypePrefixLen+kPrefixLenV1  /* WGS:AAAA01 */ &&
         db.size() != kTypePrefixLen+kNumLettersV2 /* WGS:AAAAAA */ &&
         db.size() != kTypePrefixLen+kPrefixLenV2  /* WGS:AAAAAA01 */ ) {
        return false;
    }
    if ( !NStr::StartsWith(db, "WGS:", NStr::eNocase) &&
         !NStr::StartsWith(db, "TSA:", NStr::eNocase) ) {
        return false;
    }
    return true;
}


enum EAlligSeqType {
    fAllow_contig   = 1,
    fAllow_scaffold = 2,
    fAllow_protein  = 4
};
typedef int TAllowSeqType;

static bool IsWGSAccession(const string& acc,
                           const CTextseq_id& id,
                           TAllowSeqType allow_seq_type)
{
    if ( acc.size() < kPrefixLenV1 + kMinRowDigitsV1 ||
         acc.size() > kPrefixLenV2 + kMaxRowDigitsV2 + 1 ) { // one for type letter
        return false;
    }
    size_t num_letters;
    for ( num_letters = 0; num_letters < kNumLettersV2; ++num_letters ) {
        if ( !isalpha(acc[num_letters]&0xff) ) {
            break;
        }
    }
    if ( num_letters != kNumLettersV1 && num_letters != kNumLettersV2 ) {
        return false;
    }
    size_t prefix_len = num_letters + kVersionDigits;
    for ( size_t i = num_letters; i < prefix_len; ++i ) {
        if ( !isdigit(acc[i]&0xff) ) {
            return false;
        }
    }
    SIZE_TYPE row_pos = prefix_len;
    switch ( acc[row_pos] ) { // optional type letter
    case 's':
    case 'S':
        // scaffold
        if ( !(allow_seq_type & fAllow_scaffold) ) {
            return false;
        }
        ++row_pos;
        break;
    case 'p':
    case 'P':
        // protein
        if ( !(allow_seq_type & fAllow_protein) ) {
            return false;
        }
        ++row_pos;
        break;
    default:
        // contig
        if ( !(allow_seq_type & fAllow_contig) ) {
            return false;
        }
        break;
    }
    size_t row_digits = acc.size() - row_pos;
    if ( num_letters == kNumLettersV1 ) {
        if ( row_digits < kMinRowDigitsV1 || row_digits > kMaxRowDigitsV1 ) {
            return false;
        }
    }
    else {
        if ( row_digits < kMinRowDigitsV2 || row_digits > kMaxRowDigitsV2 ) {
            return false;
        }
    }
    Uint8 row = 0;
    for ( size_t i = row_pos; i < acc.size(); ++i ) {
        char c = acc[i];
        if ( c < '0' || c > '9' ) {
            return false;
        }
        row = row*10+(c-'0');
    }
    if ( !row ) {
        return false;
    }
    return true;
}


static bool IsWGSProtAccession(const CTextseq_id& id)
{
    const string& acc = id.GetAccession();
    if ( acc.size() < kMinProtAccLen || acc.size() > kMaxProtAccLen ) {
        return false;
    }
    return true;
}


static bool IsWGSAccession(const CTextseq_id& id)
{
    if ( id.IsSetName() ) {
        // first try name reference if it has WGS format like AAAA01P000001
        // as it directly contains WGS accession
        return IsWGSAccession(id.GetName(), id, fAllow_protein);
    }
    if ( !id.IsSetAccession() ) {
        return false;
    }
    const string& acc = id.GetAccession();
    CSeq_id::EAccessionInfo type = CSeq_id::IdentifyAccession(acc);
    switch ( type & CSeq_id::eAcc_division_mask ) {
        // accepted accession types
    case CSeq_id::eAcc_wgs:
    case CSeq_id::eAcc_wgs_intermed:
    case CSeq_id::eAcc_tsa:
    case CSeq_id::eAcc_targeted:
        if ( type & CSeq_id::fAcc_prot ) {
            return IsWGSProtAccession(id);
        }
        else {
            return IsWGSAccession(acc, id, fAllow_contig|fAllow_scaffold);
        }
    case CSeq_id::eAcc_other:
        // Some EMBL WGS accession aren't identified as WGS, so we'll try lookup anyway
        if ( type == CSeq_id::eAcc_embl_prot ||
             (type == CSeq_id::eAcc_gb_prot && acc.size() == 10) ) { // TODO: remove
            return IsWGSProtAccession(id);
        }
        return false;
    default:
        // non-WGS accessions
        return false;
    }
}


bool CPSGS_OSGResolveBase::CanBeWGS(int seq_id_type, const string& seq_id)
{
    try {
        CSeq_id id;
        SetSeqId(id, seq_id_type, seq_id);
        if ( id.IsGi() ) {
            return true;
        }
        else if ( id.IsGeneral() ) {
            return IsWGSGeneral(id.GetGeneral());
        }
        else if ( auto text_id = id.GetTextseq_Id() ) {
            return IsWGSAccession(*text_id);
        }
        return false;
    }
    catch ( exception& /*ignored*/ ) {
        return false;
    }
}


END_NAMESPACE(osg);
END_NAMESPACE(psg);
END_NCBI_NAMESPACE;
