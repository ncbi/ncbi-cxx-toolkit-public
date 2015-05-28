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
*  Author: Christiam Camacho
*
* ===========================================================================
*/

/** @file cached_sequence.cpp
 * Defines the CCachedSequence class
 */
#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <ncbi_pch.hpp>
#include "cached_sequence.hpp"
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/Delta_seq.hpp>
#include <objects/seq/Delta_ext.hpp>
#include <objects/seq/Seq_ext.hpp>
#include <objects/seq/Seq_literal.hpp>
#include <objmgr/impl/tse_chunk_info.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

CCachedSequence::CCachedSequence(IBlastDbAdapter& db,
                                 const CSeq_id_Handle&  idh,
                                 int oid,
                                 bool use_fixed_size_slices,
                                 TSeqPos slice_size /* = kSequenceSliceSize */)
    : m_SIH(idh), m_BlastDb(db), m_OID(oid),
    m_UseFixedSizeSlices(use_fixed_size_slices),
    m_SliceSize(slice_size)
{
    m_TSE.Reset();
    m_Length = m_BlastDb.GetSeqLength(m_OID);

    CRef<CBioseq> bioseq(m_BlastDb.GetBioseqNoData(m_OID, ZERO_GI, &*(m_SIH.GetSeqId())));

    CConstRef<CSeq_id> first_id( bioseq->GetFirstId() );
    _ASSERT(first_id);
    if ( first_id ) {
        m_SIH = CSeq_id_Handle::GetHandle(*first_id);
    }
    
    bioseq->SetInst().SetLength(m_Length);
    bioseq->SetInst().SetMol((m_BlastDb.GetSequenceType() == CSeqDB::eProtein)
                             ? CSeq_inst::eMol_aa
                             : CSeq_inst::eMol_na);
    
    m_TSE.Reset(new CSeq_entry);
    m_TSE->SetSeq(*bioseq);
}

static CBioseq::TId s_ExtractSeqIds(const CBioseq& bioseq)
{
    CBioseq::TId retval;

    CRef<CBlast_def_line_set> blast_deflines =
        CSeqDB::ExtractBlastDefline(bioseq);
    if ( blast_deflines.Empty() ) {
        return retval;
    }
    NON_CONST_ITERATE(CBlast_def_line_set::Tdata, one_defline,
                      blast_deflines->Set()) {
        if (! (*one_defline)->CanGetSeqid()) {
            continue;
        }
        NON_CONST_ITERATE(CBlast_def_line::TSeqid, seqid, 
                          (*one_defline)->SetSeqid()) {
            retval.push_back(*seqid);
        }
    }
    return retval;
}

void 
CCachedSequence::RegisterIds(CBlastDbDataLoader::TIdMap & idmap)
{
    _ASSERT(m_TSE->IsSeq());
    
    CBioseq::TId ids = s_ExtractSeqIds(m_TSE->SetSeq());
    if (ids.empty()) {
        ids = m_TSE->SetSeq().SetId();
    }
    
    ITERATE(CBioseq::TId, seqid, ids) {
        idmap[CSeq_id_Handle::GetHandle(**seqid)] = m_OID;
    }
}

void CCachedSequence::x_AddFullSeq_data()
{
    _ASSERT(m_Length);
    CRef<CSeq_data> seqdata = m_BlastDb.GetSequence(m_OID, 0, m_Length);
    _ASSERT(seqdata.NotEmpty());
    m_TSE->SetSeq().SetInst().SetSeq_data(*seqdata);
}

void CCachedSequence::SplitSeqData(TCTSE_Chunk_InfoVector& chunks)
{
    CSeq_inst& inst = m_TSE->SetSeq().SetInst();
    if ( m_Length <= kFastSequenceLoadSize && 
         m_SliceSize != kRmtSequenceSliceSize) { // N/A for remote BLAST loader
        // single Seq-data, no need to use Delta
        inst.SetRepr(CSeq_inst::eRepr_raw);
        x_AddFullSeq_data();
    }
    else if ( m_Length <= m_SliceSize ) {
        // single Seq-data, no need to use Delta
        inst.SetRepr(CSeq_inst::eRepr_raw);
        x_AddSplitSeqChunk(chunks, m_SIH, 0, m_Length);
    }
    else {
        // multiple Seq-data, we'll have to use Delta
        inst.SetRepr(CSeq_inst::eRepr_delta);
        CDelta_ext::Tdata& delta = inst.SetExt().SetDelta().Set();
        TSeqPos slice_size = m_SliceSize, pos = 0;
        while (pos < m_Length) {
            TSeqPos end = m_Length;
            if ((end - pos) > slice_size) {
                end = pos + slice_size;
            }
            x_AddSplitSeqChunk(chunks, m_SIH, pos, end);
            CRef<CDelta_seq> dseq(new CDelta_seq);
            dseq->SetLiteral().SetLength(end - pos);
            delta.push_back(dseq);

            pos += slice_size;
            if ( !m_UseFixedSizeSlices ) {
                slice_size *= kSliceGrowthFactor;
            }
        }
    }
}

void CCachedSequence::x_AddSplitSeqChunk(TCTSE_Chunk_InfoVector& chunks,
                                        const CSeq_id_Handle& id,
                                        TSeqPos               begin,
                                        TSeqPos               end)
{
    // Create location for the chunk
    CTSE_Chunk_Info::TLocationSet loc_set;
    CTSE_Chunk_Info::TLocationRange rg =
        CTSE_Chunk_Info::TLocationRange(begin, end-1);
    
    CTSE_Chunk_Info::TLocation loc(id, rg);
    loc_set.push_back(loc);
    
    // Create new chunk for the data
    CRef<CTSE_Chunk_Info> chunk(new CTSE_Chunk_Info(begin));
    
    // Add seq-data
    chunk->x_AddSeq_data(loc_set);

    chunks.push_back(chunk);
}

CRef<CSeq_literal>
CreateSeqDataChunk(IBlastDbAdapter& blastdb, 
                   int oid, 
                   TSeqPos begin, 
                   TSeqPos end)
{
    CRef<CSeq_data> seqdata = blastdb.GetSequence(oid, begin, end);
    CRef<CSeq_literal> retval(new CSeq_literal);
    retval->SetLength(end - begin);
    retval->SetSeq_data(*seqdata);
    return retval;
}

END_SCOPE(objects)
END_NCBI_SCOPE

