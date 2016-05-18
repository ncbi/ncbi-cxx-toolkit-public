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
 * Authors:  Mike DiCuccio
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>

#include <objtools/data_loaders/asn_cache/asn_index.hpp>
#include <objtools/data_loaders/asn_cache/asn_cache_util.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/Org_ref.hpp>


BEGIN_NCBI_SCOPE

CAsnIndex::CAsnIndex(E_index_type type)
    : m_type(type)
{
    SetPageSize(64 * 1024);

    BindKey("seq_id", &m_SeqId);
    BindKey("version", &m_Version);
    BindKey("gi", &m_Gi);
    BindKey("timestamp", &m_Timestamp);

    if (type == e_main) {
        BindData("chunk", &m_ChunkId);
    }

    BindData("offs", &m_Offset);
    BindData("size", &m_Size);
    if (type == e_main){
        BindData("slen", &m_SeqLength);
        BindData("taxid", &m_TaxId);
    }
}


CAsnIndex::TSeqId CAsnIndex::GetSeqId() const
{
    return (TSeqId)m_SeqId;
}


CAsnIndex::TVersion CAsnIndex::GetVersion() const
{
    return (TVersion)m_Version;
}


CAsnIndex::TGi CAsnIndex::GetGi() const
{
    return (TGi)m_Gi;
}


CAsnIndex::TTimestamp CAsnIndex::GetTimestamp() const
{
    return (TTimestamp)m_Timestamp;
}


CAsnIndex::TChunkId CAsnIndex::GetChunkId() const
{
    return m_type == e_seq_id ? 0 : (TChunkId)m_ChunkId;
}


CAsnIndex::TOffset CAsnIndex::GetOffset() const
{
    return (TOffset)m_Offset;
}


CAsnIndex::TSize CAsnIndex::GetSize() const
{
    return (TSize)m_Size;
}


CAsnIndex::TSeqLength CAsnIndex::GetSeqLength() const
{
    return m_type == e_seq_id ? 0 : (TSeqLength)m_SeqLength;
}


CAsnIndex::TTaxId CAsnIndex::GetTaxId() const
{
    return m_type == e_seq_id ? 0 : (TTaxId)m_TaxId;
}


void CAsnIndex::SetSeqId(TSeqId val)
{
    m_SeqId = val;
}


void CAsnIndex::SetVersion(TVersion val)
{
    m_Version = val;
}


void CAsnIndex::SetGi(TGi val)
{
    m_Gi = val;
}


void CAsnIndex::SetTimestamp(TTimestamp val)
{
    m_Timestamp = val;
}


void CAsnIndex::SetChunkId(TChunkId val)
{
    if(m_type == e_main)
        m_ChunkId = val;
}


void CAsnIndex::SetOffset(TOffset val)
{
    m_Offset = val;
}


void CAsnIndex::SetSize(TSize val)
{
    m_Size = val;
}


void CAsnIndex::SetSeqLength(TSeqLength val)
{
    if(m_type == e_main)
        m_SeqLength = val;
}


void CAsnIndex::SetTaxId(TTaxId val)
{
    if(m_type == e_main)
        m_TaxId = val;
}


CNcbiOstream &operator<<(CNcbiOstream &ostr, const CAsnIndex::SIndexInfo &info)
{
                    ostr << info.seq_id << " | "
                        << info.version << " "
                        << info.gi << " "
                        << info.timestamp << " "
                        << info.chunk << " "
                        << info.offs << " "
                        << info.size << " "
                        << info.sequence_length << " "
                        << info.taxonomy_id;
                    return ostr;
}

size_t  IndexABioseq( const objects::CBioseq&   bioseq,
                        CAsnIndex &             index,
                        CAsnIndex::TTimestamp   timestamp,
                        CAsnIndex::TChunkId     chunk_id,
                        CAsnIndex::TOffset      offset,
                        CAsnIndex::TSize        size
                    )
{
    size_t  seqid_count = 0;
    CAsnIndex::TGi gi = 0;
    CAsnIndex::TSeqLength seq_length;
    CAsnIndex::TTaxId taxid;
    BioseqIndexData(bioseq, gi, seq_length, taxid);

    /// Next, extract all the other IDs (including the GI again).
    ITERATE (objects::CBioseq::TId, id_iter, bioseq.GetId()) {
        string id_str;
        Uint4 version = 0;
        GetNormalizedSeqId(**id_iter, id_str, version);

        index.SetSeqId(id_str);
        index.SetVersion(version);
        index.SetGi(gi);
        index.SetTimestamp(timestamp);
        index.SetChunkId(chunk_id);
        index.SetOffset(offset);
        index.SetSize(size);
        index.SetSeqLength(seq_length);
        index.SetTaxId(taxid);

        if (index.UpdateInsert() != eBDB_Ok) {
            std::string error_string = "Failed to add seq id ";
            error_string += id_str + " to " + (index.GetIndexType() == CAsnIndex::e_main ? "main" : "seqid");
            error_string += " index at " + index.GetFileName();
            NCBI_THROW( CException, eUnknown, error_string );
        }

        ++seqid_count;
    }
    
    return  seqid_count;
}

void     BioseqIndexData( const objects::CBioseq&   bioseq,
                          CAsnIndex::TGi&           gi,
                          CAsnIndex::TSeqLength&    seq_length,
			  CAsnIndex::TTaxId&    taxid)
{
    gi = 0;
    seq_length = 0;
    // extract tax-id and length, needed for locator. If length is not
    // available, store a length of 0 (this can happen for Named annotation blobs).
    if (bioseq.CanGetInst() && bioseq.GetInst().CanGetLength())
        seq_length = bioseq.GetInst().GetLength();

    /// Get taxid from bioseq if it is there; if not go up in Bioseq-set hierarchy
    /// to look for it
    taxid = bioseq.GetTaxId();
    for (CConstRef<objects::CBioseq_set> ancestor_set = bioseq.GetParentSet();
         ancestor_set && !taxid;
         ancestor_set = ancestor_set->GetParentSet())
    {
        if (ancestor_set->IsSetDescr()) {
            /// taxid from Org descriptor; should be given lower precedence than taxid
            /// from source descriptor
            int taxid_from_org = 0;
            ITERATE (objects::CBioseq_set::TDescr::Tdata, it,
                     ancestor_set->GetDescr().Get())
            {
                const objects::CSeqdesc& desc = **it;
                if (desc.IsOrg()) {
                    taxid_from_org = desc.GetOrg().GetTaxId();
                } else if (desc.IsSource() && desc.GetSource().IsSetOrg()) {
                    taxid = desc.GetSource().GetOrg().GetTaxId();
                }
                if (taxid) {
                    break;
                }
            }
            if (!taxid) {
                taxid = taxid_from_org;
            }
        }
    }

    /// We assume there are few Ids in the bioseq, so we make two passes:
    /// First, extract the GI since we index each seq ID with the GI.
    ITERATE (objects::CBioseq::TId, id_iter, bioseq.GetId()) {
        if ((*id_iter)->IsGi()) {
            gi = GI_TO(CAsnIndex::TGi, (*id_iter)->GetGi());
            break;
        }
    }
}

END_NCBI_SCOPE


