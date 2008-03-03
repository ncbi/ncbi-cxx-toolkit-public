#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] =
    "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

/* ===========================================================================
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
 * Author:  Christiam Camacho / Kevin Bealer
 *
 */

/// @file blast_objmgr_tools.cpp
/// Functions in xblast API code that interact with object manager.

#include <ncbi_pch.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/util/sequence.hpp>
#include <objects/seqalign/seqalign__.hpp>
#include <objects/seqloc/seqloc__.hpp>

#include <algo/blast/api/blast_options.hpp>
#include "blast_setup.hpp"
#include "blast_objmgr_priv.hpp"
#include <algo/blast/core/blast_encoding.h>
#include <algo/blast/api/blast_seqinfosrc.hpp>
#include <algo/blast/api/blast_seqinfosrc_aux.hpp>

#include <serial/iterator.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include "blast_seqalign.hpp"

#include "dust_filter.hpp"
#include "repeats_filter.hpp"

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(blast)

// N.B.: the const is removed, but the v object is never really changed,
// therefore we keep it as a const argument
CBlastQuerySourceOM::CBlastQuerySourceOM(TSeqLocVector& v,
                                         EBlastProgramType     program)
    : m_TSeqLocVector(&v),
      m_OwnTSeqLocVector(false), 
      m_Options(0), 
      m_CalculatedMasks(true),
      m_Program(program)
{
    x_AutoDetectGeneticCodes();
}

CBlastQuerySourceOM::CBlastQuerySourceOM(TSeqLocVector& v,
                                         const CBlastOptions* opts)
    : m_TSeqLocVector(&v), 
      m_OwnTSeqLocVector(false), 
      m_Options(opts), 
      m_CalculatedMasks(false),
      m_Program(opts->GetProgramType())
{
    x_AutoDetectGeneticCodes();
}

CBlastQuerySourceOM::CBlastQuerySourceOM(CBlastQueryVector   & v,
                                         EBlastProgramType     program)
    : m_QueryVector(& v),
      m_OwnTSeqLocVector(false), 
      m_Options(0), 
      m_CalculatedMasks(false),
      m_Program(program)
{
    x_AutoDetectGeneticCodes();
}

CBlastQuerySourceOM::CBlastQuerySourceOM(CBlastQueryVector   & v,
                                         const CBlastOptions * opts)
    : m_QueryVector(& v),
      m_OwnTSeqLocVector(false), 
      m_Options(opts), 
      m_CalculatedMasks(false),
      m_Program(opts->GetProgramType())
{
    x_AutoDetectGeneticCodes();
}

void
CBlastQuerySourceOM::x_AutoDetectGeneticCodes(void)
{
    if ( ! (Blast_QueryIsTranslated(m_Program) || 
            Blast_SubjectIsTranslated(m_Program)) ) {
        return;
    }

    if (m_QueryVector.NotEmpty()) {
        for (CBlastQueryVector::size_type i = 0; 
             i < m_QueryVector->Size(); i++) {
            CRef<CBlastSearchQuery> query = 
                m_QueryVector->GetBlastSearchQuery(i);

            if (query->GetGeneticCodeId() != BLAST_GENETIC_CODE) {
                // presumably this has already been set, so skip fetching it
                // again
                continue;
            }

            const CSeq_id* id = query->GetQuerySeqLoc()->GetId();
            CSeqdesc_CI desc_it(query->GetScope()->GetBioseqHandle(*id), 
                                CSeqdesc::e_Source);
            if (desc_it) {
                try {
                    query->SetGeneticCodeId(desc_it->GetSource().GetGenCode());
                }
                catch(CUnassignedMember &) {
                    query->SetGeneticCodeId(BLAST_GENETIC_CODE);
                }
            }
        }
    } else {
        _ASSERT(m_TSeqLocVector);
        NON_CONST_ITERATE(TSeqLocVector, sseqloc, *m_TSeqLocVector) {

            if (sseqloc->genetic_code_id != BLAST_GENETIC_CODE) {
                // presumably this has already been set, so skip fetching it
                // again
                continue;
            }

            const CSeq_id* id = sseqloc->seqloc->GetId();
            CSeqdesc_CI desc_it(sseqloc->scope->GetBioseqHandle(*id),
                                CSeqdesc::e_Source);
            if (desc_it) {
                try {
                    sseqloc->genetic_code_id = desc_it->GetSource().GetGenCode();
                }
                catch(CUnassignedMember &) {
                    sseqloc->genetic_code_id = BLAST_GENETIC_CODE;
                }
            }
        }
    }
}

void
CBlastQuerySourceOM::x_CalculateMasks(void)
{
    /// Calculate the masks only once
    if (m_CalculatedMasks) {
        return;
    }
    
    // Without the options we cannot obtain the parameters to do the
    // filtering on the queries to obtain the masks
    if ( !m_Options ) {
        m_CalculatedMasks = true;
        return;
    }
    
    if (Blast_QueryIsNucleotide(m_Options->GetProgramType()) &&
        !Blast_QueryIsTranslated(m_Options->GetProgramType())) {
        
        if (m_Options->GetDustFiltering()) {
            if (m_QueryVector.NotEmpty()) {
                Blast_FindDustFilterLoc(*m_QueryVector,
                    static_cast<Uint4>(m_Options->GetDustFilteringLevel()),
                    static_cast<Uint4>(m_Options->GetDustFilteringWindow()),
                    static_cast<Uint4>(m_Options->GetDustFilteringLinker()));
            } else {
                Blast_FindDustFilterLoc(*m_TSeqLocVector, 
                    static_cast<Uint4>(m_Options->GetDustFilteringLevel()),
                    static_cast<Uint4>(m_Options->GetDustFilteringWindow()),
                    static_cast<Uint4>(m_Options->GetDustFilteringLinker()));
            }
        }
        if (m_Options->GetRepeatFiltering()) {
            if (m_QueryVector.NotEmpty()) {
                Blast_FindRepeatFilterLoc(*m_QueryVector,
                                          m_Options->GetRepeatFilteringDB());
            } else {
                Blast_FindRepeatFilterLoc(*m_TSeqLocVector,
                                          m_Options->GetRepeatFilteringDB());
            }
        }
    }
    
    m_CalculatedMasks = true;
}

CBlastQuerySourceOM::~CBlastQuerySourceOM()
{
    if (m_OwnTSeqLocVector && m_TSeqLocVector) {
        delete m_TSeqLocVector;
        m_TSeqLocVector = NULL;
    }
}

ENa_strand 
CBlastQuerySourceOM::GetStrand(int i) const
{
    if (m_QueryVector.NotEmpty()) {
        return sequence::GetStrand(*m_QueryVector->GetQuerySeqLoc(i),
                                   m_QueryVector->GetScope(i));
    } else {
        return sequence::GetStrand(*(*m_TSeqLocVector)[i].seqloc,
                                   (*m_TSeqLocVector)[i].scope);
    }
}

TMaskedQueryRegions
CBlastQuerySourceOM::GetMaskedRegions(int i)
{
    x_CalculateMasks();

    if (m_QueryVector.NotEmpty()) {
        return m_QueryVector->GetMaskedRegions(i);
    } else {
        return PackedSeqLocToMaskedQueryRegions((*m_TSeqLocVector)[i].mask,
                     m_Program, (*m_TSeqLocVector)[i].ignore_strand_in_mask);
    }
}

CConstRef<CSeq_loc>
CBlastQuerySourceOM::GetMask(int i)
{
    x_CalculateMasks();
    
    if (m_QueryVector.NotEmpty()) {
        return MaskedQueryRegionsToPackedSeqLoc( m_QueryVector->GetMaskedRegions(i) );
    } else {
        return (*m_TSeqLocVector)[i].mask;
    }
}

CConstRef<CSeq_loc>
CBlastQuerySourceOM::GetSeqLoc(int i) const
{
    if (m_QueryVector.NotEmpty()) {
        return m_QueryVector->GetQuerySeqLoc(i);
    } else {
        return (*m_TSeqLocVector)[i].seqloc;
    }
}

const CSeq_id*
CBlastQuerySourceOM::GetSeqId(int i) const
{
    if (m_QueryVector.NotEmpty()) {
        return & sequence::GetId(*m_QueryVector->GetQuerySeqLoc(i),
                                 m_QueryVector->GetScope(i));
    } else {
        return & sequence::GetId(*(*m_TSeqLocVector)[i].seqloc,
                                 (*m_TSeqLocVector)[i].scope);
    }
}

Uint4
CBlastQuerySourceOM::GetGeneticCodeId(int i) const
{
    if (m_QueryVector.NotEmpty()) {
        return m_QueryVector->GetBlastSearchQuery(i)->GetGeneticCodeId();
    } else {
        return (*m_TSeqLocVector)[i].genetic_code_id;
    }
}

SBlastSequence
CBlastQuerySourceOM::GetBlastSequence(int i,
                                      EBlastEncoding encoding,
                                      objects::ENa_strand strand,
                                      ESentinelType sentinel,
                                      string* warnings) const
{
    if (m_QueryVector.NotEmpty()) {
        return GetSequence(*m_QueryVector->GetQuerySeqLoc(i), encoding,
                           m_QueryVector->GetScope(i), strand, sentinel, warnings);
    } else {
        return GetSequence(*(*m_TSeqLocVector)[i].seqloc, encoding,
                           (*m_TSeqLocVector)[i].scope, strand, sentinel, warnings);
    }
}

TSeqPos
CBlastQuerySourceOM::GetLength(int i) const
{
    TSeqPos rv = numeric_limits<TSeqPos>::max();
    
    if (m_QueryVector.NotEmpty()) {
        rv = sequence::GetLength(*m_QueryVector->GetQuerySeqLoc(i),
                                 m_QueryVector->GetScope(i));
    } else if (! m_TSeqLocVector->empty()) {
        rv = sequence::GetLength(*(*m_TSeqLocVector)[i].seqloc,
                                 (*m_TSeqLocVector)[i].scope);
    }
    
    if (rv == numeric_limits<TSeqPos>::max()) {
        NCBI_THROW(CBlastException, eInvalidArgument,
                   string("Could not find length of query # ")
                   + NStr::IntToString(i) + " with Seq-id ["
                   + GetSeqId(i)->AsFastaString() + "]");
    }
    
    return rv;
}

TSeqPos
CBlastQuerySourceOM::Size() const
{
    if (m_QueryVector.NotEmpty()) {
        return static_cast<TSeqPos>(m_QueryVector->Size());
    } else {
        return static_cast<TSeqPos>(m_TSeqLocVector->size());
    }
}

void
SetupQueryInfo(TSeqLocVector& queries, 
               EBlastProgramType prog,
               objects::ENa_strand strand_opt,
               BlastQueryInfo** qinfo)
{
    SetupQueryInfo_OMF(CBlastQuerySourceOM(queries, prog), prog, strand_opt, qinfo);
}

void
SetupQueries(TSeqLocVector& queries, 
             BlastQueryInfo* qinfo, 
             BLAST_SequenceBlk** seqblk,
             EBlastProgramType prog,
             objects::ENa_strand strand_opt,
             TSearchMessages& messages)
{
    CBlastQuerySourceOM query_src(queries, prog);
    SetupQueries_OMF(query_src, qinfo, seqblk, prog, 
                     strand_opt, messages);
}

void
SetupSubjects(TSeqLocVector& subjects, 
              EBlastProgramType prog,
              vector<BLAST_SequenceBlk*>* seqblk_vec, 
              unsigned int* max_subjlen)
{
    CBlastQuerySourceOM subj_src(subjects, prog);
    SetupSubjects_OMF(subj_src, prog, seqblk_vec, max_subjlen);
}

/// Implementation of the IBlastSeqVector interface which obtains data from a
/// CSeq_loc and a CScope relying on the CSeqVector class
class CBlastSeqVectorOM : public IBlastSeqVector
{
public:
    CBlastSeqVectorOM(const CSeq_loc& seqloc, CScope& scope)
        : m_SeqLoc(seqloc), m_Scope(scope), m_SeqVector(seqloc, scope)
    {
        m_Strand = m_SeqLoc.GetStrand();
    }
    
    /** @inheritDoc */
    virtual void SetCoding(CSeq_data::E_Choice coding) {
        m_SeqVector.SetCoding(coding);
    }

    /** @inheritDoc */
    virtual Uint1 operator[] (TSeqPos pos) const { return m_SeqVector[pos]; }

    /** @inheritDoc */
    virtual void GetStrandData(objects::ENa_strand strand, unsigned char* buf) {
        x_FixStrand(strand);
        for (CSeqVector_CI itr(m_SeqVector, strand); itr; ++itr) {
            *buf++ = *itr;
        }
    }

    /** @inheritDoc */
    virtual SBlastSequence GetCompressedPlusStrand() {
        CSeqVector_CI iter(m_SeqVector);
        iter.SetRandomizeAmbiguities();
        iter.SetCoding(CSeq_data::e_Ncbi2na);

        SBlastSequence retval(size());
        for (TSeqPos i = 0; i < size(); i++) {
            retval.data.get()[i] = *iter++;
        }
        return retval;
    }

protected:
    /** @inheritDoc */
    virtual TSeqPos x_Size() const { 
        return m_SeqVector.size(); 
    }
    /** @inheritDoc */
    virtual void x_SetPlusStrand() {
        x_SetStrand(eNa_strand_plus);
    }
    /** @inheritDoc */
    virtual void x_SetMinusStrand() {
        x_SetStrand(eNa_strand_minus);
    }
    /** @inheritDoc 
     * @note for this class, this might be inefficient, please use
     * GetStrandData with the appropriate strand
     */
    void x_SetStrand(ENa_strand s) {
        x_FixStrand(s);
        _ASSERT(m_Strand == m_SeqVector.GetStrand());
        if (s != m_Strand) {
            m_SeqVector = CSeqVector(m_SeqLoc, m_Scope,
                                     CBioseq_Handle::eCoding_Ncbi, s);
        }
    }
    
    /// If the Seq-loc is on the minus strand and the user is 
    /// asking for the minus strand, we change the user's request
    /// to the plus strand. If we did not do this, we would get
    /// the plus strand (ie it would be reversed twice)
    /// @param strand strand to handle [in|out]
    void x_FixStrand(objects::ENa_strand& strand) const {
        if (eNa_strand_minus == strand && 
            eNa_strand_minus == m_SeqLoc.GetStrand()) {
            strand = eNa_strand_plus;
        }
    }

private:
    const CSeq_loc & m_SeqLoc;
    CScope         & m_Scope;
    CSeqVector       m_SeqVector;
};

SBlastSequence
GetSequence(const objects::CSeq_loc& sl, EBlastEncoding encoding, 
            objects::CScope* scope,
            objects::ENa_strand strand, 
            ESentinelType sentinel,
            std::string* warnings) 
{
    // Retrieves the correct strand (plus or minus), but not both
    CBlastSeqVectorOM sv = CBlastSeqVectorOM(sl, *scope);
    return GetSequence_OMF(sv, encoding, strand, sentinel, warnings);
}

/// Converts BlastHitList containing alignments for a single query to database
/// sequences into a CSeq_align_set
/// @param hit_list List of alignments [in]
/// @param prog Program type [in]
/// @param query Query sequence blast::SSeqLoc [in]
/// @param seqinfo_src abstraction to obtain sequence information [in]
/// @param is_gapped true if this is a gapped alignment [in]
/// @param is_ooframe true if this is an out-of-frame alignment [in]
/// @return empty CSeq_align_set if hit_list is NULL, otherwise conversion from
/// BlastHitList to CSeq_align_set
CSeq_align_set*
BLAST_HitList2CSeqAlign(const BlastHitList* hit_list,
                        EBlastProgramType prog,
                        const SSeqLoc &query,
                        const IBlastSeqInfoSrc* seqinfo_src,
                        bool is_gapped,
                        bool is_ooframe)
{
    CSeq_align_set* seq_aligns = new CSeq_align_set();

    if (!hit_list) {
        return CreateEmptySeq_align_set(seq_aligns);
    }

    TSeqPos query_length = sequence::GetLength(*query.seqloc, query.scope);
    CConstRef<CSeq_id> query_id(&sequence::GetId(*query.seqloc, query.scope));

    TSeqPos subj_length = 0;
    CConstRef<CSeq_id> subject_id;

    // stores a CSeq_align for each matching sequence
    vector<CRef<CSeq_align > > hit_align;

    for (int index = 0; index < hit_list->hsplist_count; index++) {
        BlastHSPList* hsp_list = hit_list->hsplist_array[index];
        if (!hsp_list)
            continue;

        // Sort HSPs with e-values as first priority and scores as 
        // tie-breakers, since that is the order we want to see them in 
        // in Seq-aligns.
        Blast_HSPListSortByEvalue(hsp_list);

        GetSequenceLengthAndId(seqinfo_src, hsp_list->oid, 
                               subject_id, &subj_length);

        vector<int> gi_list;
        GetFilteredRedundantGis(*seqinfo_src, hsp_list->oid, gi_list);
        
        if (is_gapped) {
                BLASTHspListToSeqAlign(prog,
                                       hsp_list,
                                       query_id,
                                       subject_id,
                                       query_length,
                                       subj_length,
                                       is_ooframe,
                                       gi_list,
                                       hit_align);
        } else {
                BLASTUngappedHspListToSeqAlign(prog,
                                               hsp_list,
                                               query_id,
                                               subject_id,
                                               query_length,
                                               subj_length,
                                               gi_list,
                                               hit_align);
        }
        ITERATE(vector<CRef<CSeq_align > >, iter, hit_align) {
           RemapToQueryLoc(*iter, *query.seqloc);
           seq_aligns->Set().push_back(*iter);
        }
    }
    return seq_aligns;
}

TSeqAlignVector 
PHIBlast_Results2CSeqAlign(const BlastHSPResults  * results, 
                           EBlastProgramType        prog,
                           const TSeqLocVector    & query,
                           const IBlastSeqInfoSrc * seqinfo_src,
                           const SPHIQueryInfo    * pattern_info)
{
    TSeqAlignVector retval;
    CRef<CSeq_align_set> wrap_list(new CSeq_align_set());

    /* Split results into an array of BlastHSPResults structures corresponding
       to different pattern occurrences. */
    BlastHSPResults* *phi_results = 
        PHIBlast_HSPResultsSplit(results, pattern_info);

    if (phi_results) {
        for (int pattern_index = 0; pattern_index < pattern_info->num_patterns;
             ++pattern_index) {
            CBlastHSPResults one_phi_results(phi_results[pattern_index]);

            if (one_phi_results) {
                // PHI BLAST does not work with multiple queries, so we only 
                // need to look at the first hit list.
                BlastHitList* hit_list = one_phi_results->hitlist_array[0];

                // PHI BLAST is always gapped, and never out-of-frame, hence
                // true and false values for the respective booleans in the next
                // call.
                CRef<CSeq_align_set> seq_aligns(
                    BLAST_HitList2CSeqAlign(hit_list, prog, query[0], 
                                            seqinfo_src, true, false));
                
                // Wrap this Seq-align-set in a discontinuous Seq-align and 
                // attach it to the final Seq-align-set.
                CRef<CSeq_align> wrap_align(new CSeq_align());
                wrap_align->SetType(CSeq_align::eType_partial);
                wrap_align->SetDim(2); // Always a pairwise alignment
                wrap_align->SetSegs().SetDisc(*seq_aligns);
                wrap_list->Set().push_back(wrap_align);
            }
        }
        sfree(phi_results);
    }
    
    // Put the final Seq-align-set into the vector
    retval.push_back(wrap_list);

    return retval;
}

TSeqAlignVector
BLAST_Results2CSeqAlign(const BlastHSPResults* results,
        EBlastProgramType prog,
        TSeqLocVector &query,
        const IBlastSeqInfoSrc* seqinfo_src,
        bool is_gapped, bool is_ooframe)
{
    _ASSERT(results->num_queries == (int)query.size());

    TSeqAlignVector retval;
    CConstRef<CSeq_id> query_id;
    
    // Process each query's hit list
    for (int index = 0; index < results->num_queries; index++) {
       BlastHitList* hit_list = results->hitlist_array[index];

       CRef<CSeq_align_set> seq_aligns(BLAST_HitList2CSeqAlign(hit_list, prog,
                                           query[index], seqinfo_src,
                                           is_gapped, is_ooframe));

       retval.push_back(seq_aligns);
       _TRACE("Query " << index << ": " << seq_aligns->Get().size()
              << " seqaligns");

    }

    return retval;
}

CRef<CPacked_seqint>
TSeqLocVector2Packed_seqint(const TSeqLocVector& sequences)
{
    CRef<CPacked_seqint> retval;
    if (sequences.empty()) {
        return retval;
    }

    retval.Reset(new CPacked_seqint);
    ITERATE(TSeqLocVector, seq, sequences) {
        const CSeq_id& id(sequence::GetId(*seq->seqloc, &*seq->scope));
        TSeqRange range(TSeqRange::GetWhole());
        if (seq->seqloc->IsWhole()) {
            try {
                range.Set(0, sequence::GetLength(*seq->seqloc, &*seq->scope));
            } catch (const CException&) {
                range = TSeqRange::GetWhole();
            }
        } else if (seq->seqloc->IsInt()) {
            try {
                range.SetFrom(sequence::GetStart(*seq->seqloc, &*seq->scope));
                range.SetTo(sequence::GetStop(*seq->seqloc, &*seq->scope));
            } catch (const CException&) {
                range = TSeqRange::GetWhole();
            }
        } else {
            NCBI_THROW(CBlastException, eNotSupported, 
                       "Unsupported Seq-loc type used for query");
        }
        retval->AddInterval(id, range.GetFrom(), range.GetTo());
    }
    return retval;
}

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */
