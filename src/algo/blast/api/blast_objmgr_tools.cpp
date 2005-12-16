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

#include <algo/blast/api/blast_options.hpp>
#include "blast_setup.hpp"
#include "blast_objmgr_priv.hpp"
#include <algo/blast/core/blast_encoding.h>
#include <algo/blast/api/blast_seqinfosrc.hpp>

#include <serial/iterator.hpp>
#include "blast_seqalign.hpp"

// PSI-BLAST includes
#include <algo/blast/api/psiblast_options.hpp>
#include <algo/blast/api/psi_pssm_input.hpp>
#include <algo/blast/api/pssm_engine.hpp>
#include "bioseq_extract_data_priv.hpp"
#include <objects/scoremat/PssmWithParameters.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(blast)


CBlastQuerySourceOM::CBlastQuerySourceOM(const TSeqLocVector& v)
    : m_TSeqLocVector(v)
{}

ENa_strand 
CBlastQuerySourceOM::GetStrand(int i) const
{
    return sequence::GetStrand(*m_TSeqLocVector[i].seqloc,
                               m_TSeqLocVector[i].scope);
}

CConstRef<CSeq_loc>
CBlastQuerySourceOM::GetMask(int i) const
{
    return m_TSeqLocVector[i].mask;
}

CConstRef<CSeq_loc>
CBlastQuerySourceOM::GetSeqLoc(int i) const
{
    return m_TSeqLocVector[i].seqloc;
}

SBlastSequence
CBlastQuerySourceOM::GetBlastSequence(int i,
                                      EBlastEncoding encoding,
                                      objects::ENa_strand strand,
                                      ESentinelType sentinel,
                                      string* warnings) const
{
    return GetSequence(*m_TSeqLocVector[i].seqloc, encoding,
                       m_TSeqLocVector[i].scope, strand, sentinel, warnings);
}

TSeqPos
CBlastQuerySourceOM::GetLength(int i) const
{
    return sequence::GetLength(*m_TSeqLocVector[i].seqloc,
                               m_TSeqLocVector[i].scope);
}

TSeqPos
CBlastQuerySourceOM::Size() const
{
    return static_cast<TSeqPos>(m_TSeqLocVector.size());
}

void
SetupQueryInfo(const TSeqLocVector& queries, 
               EBlastProgramType prog,
               ENa_strand strand_opt,
               BlastQueryInfo** qinfo)
{
    SetupQueryInfo_OMF(CBlastQuerySourceOM(queries), prog, strand_opt, qinfo);
}

void
SetupQueries(const TSeqLocVector& queries, 
             const BlastQueryInfo* qinfo, 
             BLAST_SequenceBlk** seqblk,
             EBlastProgramType prog,
             ENa_strand strand_opt,
             const Uint1* genetic_code,
             TSearchMessages& messages)
{
    SetupQueries_OMF(CBlastQuerySourceOM(queries), qinfo, seqblk, prog, 
                     strand_opt, genetic_code, messages);
}

void
SetupSubjects(const TSeqLocVector& subjects, 
              EBlastProgramType prog,
              vector<BLAST_SequenceBlk*>* seqblk_vec, 
              unsigned int* max_subjlen)
{
    SetupSubjects_OMF(CBlastQuerySourceOM(subjects), prog, seqblk_vec, 
                      max_subjlen);
}

class CBlastSeqVectorOM : public IBlastSeqVector
{
public:
    CBlastSeqVectorOM(const CSeq_loc& seqloc, CScope& scope)
        : m_SeqLoc(seqloc), m_Scope(scope), m_SeqVector(seqloc, scope) {}
    
    void SetCoding(CSeq_data::E_Choice coding) {
        m_SeqVector.SetCoding(coding);
    }

    Uint1 operator[] (TSeqPos pos) const { return m_SeqVector[pos]; }

    SBlastSequence GetCompressedPlusStrand() {
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
    TSeqPos x_Size() const { 
        return m_SeqVector.size(); 
    }
    void x_SetPlusStrand() {
        x_SetStrand(eNa_strand_plus);
    }
    void x_SetMinusStrand() {
        x_SetStrand(eNa_strand_minus);
    }
    void x_SetStrand(ENa_strand s) {
        // If the Seq-loc is on the minus strand and the user is
        // asking for the minus strand, we change the user's request
        // to the plus strand.  If we did not do this, we would get
        // the plus strand (ie it would be reversed twice).
        
        if (eNa_strand_minus == s &&
            eNa_strand_minus == m_SeqLoc.GetStrand()) {
            s = eNa_strand_plus;
        }
        
        if (s != m_SeqVector.GetStrand()) {
            m_SeqVector = CSeqVector(m_SeqLoc, m_Scope,
                                     CBioseq_Handle::eCoding_Ncbi, s);
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
    EBlastProgramType prog, const SSeqLoc &query,
    const IBlastSeqInfoSrc* seqinfo_src, bool is_gapped, bool is_ooframe)
{
    CSeq_align_set* seq_aligns = new CSeq_align_set();

    if (!hit_list) {
        return CreateEmptySeq_align_set(seq_aligns);
    }

    TSeqPos query_length = sequence::GetLength(*query.seqloc, query.scope);
    CConstRef<CSeq_id> query_id(&sequence::GetId(*query.seqloc, query.scope));

    TSeqPos subj_length = 0;
    CConstRef<CSeq_id> subject_id;

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

        // Create a CSeq_align for each matching sequence
        CRef<CSeq_align> hit_align;
        if (is_gapped) {
            hit_align =
                BLASTHspListToSeqAlign(prog, hsp_list, query_id,
                                       subject_id, query_length, subj_length,
                                       is_ooframe);
        } else {
            hit_align =
                BLASTUngappedHspListToSeqAlign(prog, hsp_list, query_id,
                    subject_id, query_length, subj_length);
        }
        RemapToQueryLoc(hit_align, *query.seqloc);
        seq_aligns->Set().push_back(hit_align);
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
    ASSERT(results->num_queries == (int)query.size());

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

// Defined in psiblast_aux_priv.cpp
extern void
PsiBlastAddAncilliaryPssmData(CPssmWithParameters& pssm, 
                              const CBioseq& query, 
                              int gap_open, 
                              int gap_extend);

CRef<CPssmWithParameters> 
PsiBlastComputePssmFromAlignment(const CBioseq& query,
                                 CConstRef<CSeq_align_set> alignment,
                                 CRef<CScope> database_scope,
                                 const CPSIBlastOptionsHandle& opts_handle,
                                 PSIDiagnosticsRequest* diagnostics_request)
{
    // Extract PSSM engine options from options handle
    CPSIBlastOptions opts;
    PSIBlastOptionsNew(&opts);
    opts->pseudo_count = opts_handle.GetPseudoCount();
    opts->inclusion_ethresh = opts_handle.GetInclusionThreshold();

    CBlastQuerySourceBioseqSet query_source(query, true);
    string warnings;
    const SBlastSequence query_seq = 
        query_source.GetBlastSequence(0, eBlastEncodingProtein,
                                      eNa_strand_unknown,
                                      eSentinels, &warnings);
    ASSERT(warnings.empty());

    CPsiBlastInputData input(query_seq.data.get()+1,    // skip sentinel
                             query_seq.length-2,        // don't count sentinels
                             alignment, database_scope, 
                             *opts.Get(), 
                             opts_handle.GetMatrixName(),
                             diagnostics_request);

    CPssmEngine engine(&input);
    CRef<CPssmWithParameters> retval(engine.Run());

    PsiBlastAddAncilliaryPssmData(*retval, query, 
                                  opts_handle.GetGapOpeningCost(), 
                                  opts_handle.GetGapExtensionCost());
    return retval;
}
END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */

/*
* ===========================================================================
*
* $Log$
* Revision 1.64  2005/12/16 20:51:18  camacho
* Diffuse the use of CSearchMessage, TQueryMessages, and TSearchMessages
*
* Revision 1.63  2005/12/01 15:41:21  bealer
* - Fix blastn strand handling error.
*
* Revision 1.62  2005/11/15 22:49:15  camacho
* + function to build PSSM from PSI-BLAST results
*
* Revision 1.61  2005/11/09 20:56:26  camacho
* Refactorings to allow CPsiBl2Seq to produce Seq-aligns in the same format
* as CBl2Seq and reduce redundant code.
*
* Revision 1.60  2005/09/28 18:23:07  camacho
* Rearrangement of headers/functions to segregate object manager dependencies.
*
* Revision 1.59  2005/09/16 17:04:13  camacho
* + TSeqLocVector2Packed_seqint auxiliary function
*
* Revision 1.58  2005/07/21 17:17:28  bealer
* - OMF version of seqalign generation.
*
* Revision 1.57  2005/07/20 20:43:25  bealer
* - Minor constness change.
*
* Revision 1.56  2005/07/07 16:32:11  camacho
* Revamping of BLAST exception classes and error codes
*
* Revision 1.55  2005/06/23 16:18:46  camacho
* Doxygen fixes
*
* Revision 1.54  2005/06/20 17:32:47  camacho
* Add blast::GetSequence object manager-free interface
*
* Revision 1.53  2005/06/13 14:01:53  camacho
* Fix compiler warning
*
* Revision 1.52  2005/06/10 15:20:48  ucko
* Use consistent parameter names in SetupSubjects(_OMF).
*
* Revision 1.51  2005/06/10 14:54:56  camacho
* Implement SetupSubjects_OMF
*
* Revision 1.50  2005/06/09 20:34:51  camacho
* Object manager dependent functions reorganization
*
* Revision 1.49  2005/06/08 19:20:49  camacho
* Minor change in SetupQueries
*
* Revision 1.48  2005/06/08 17:28:56  madden
* Use functions from blast_program.c
*
* Revision 1.47  2005/05/26 14:33:03  dondosha
* Added PHIBlast_Results2CSeqAlign function for conversion of PHI BLAST results to a discontinuous Seq-align
*
* Revision 1.46  2005/05/24 20:02:42  camacho
* Changed signature of SetupQueries and SetupQueryInfo
*
* Revision 1.45  2005/05/12 19:13:50  camacho
* Remove unaccurate comment, slight clean up of SetupQueries
*
* Revision 1.44  2005/05/10 21:23:59  camacho
* Fix to prior commit
*
* Revision 1.43  2005/05/10 16:08:39  camacho
* Changed *_ENCODING #defines to EBlastEncoding enumeration
*
* Revision 1.42  2005/04/11 19:26:32  madden
* Remove extra EBlastProgramType variable
*
* Revision 1.41  2005/04/06 21:04:55  dondosha
* GapEditBlock structure removed, use BlastHSP which contains GapEditScript
*
* Revision 1.40  2005/03/31 16:15:03  dondosha
* Some doxygen fixes
*
* Revision 1.39  2005/03/07 20:55:23  dondosha
* Detect invalid residues in GetSequence and throw exception: this should never happen in normal circumstances because invalid residues are skipped by ReadFasta API
*
* Revision 1.38  2005/03/04 16:53:27  camacho
* more doxygen fixes
*
* Revision 1.37  2005/03/04 16:07:05  camacho
* doxygen fixes
*
* Revision 1.36  2005/03/01 20:52:42  camacho
* Doxygen fixes
*
* Revision 1.35  2005/02/18 15:06:02  shomrat
* CSeq_loc interface changes
*
* Revision 1.34  2005/01/27 18:30:24  camacho
* Remove unused variable
*
* Revision 1.33  2005/01/21 17:38:57  camacho
* Handle zero-length sequences
*
* Revision 1.32  2005/01/10 18:35:07  dondosha
* BlastMaskLocDNAToProtein and BlastMaskLocProteinToDNA moved to core with changed signatures
*
* Revision 1.31  2005/01/06 16:08:09  camacho
* + warnings output parameter to blast::GetSequence
*
* Revision 1.30  2005/01/06 15:41:35  camacho
* Add Blast_Message output parameter to SetupQueries
*
* Revision 1.29  2004/12/28 17:08:26  camacho
* fix compiler warning
*
* Revision 1.28  2004/12/28 16:47:43  camacho
* 1. Use typedefs to AutoPtr consistently
* 2. Remove exception specification from blast::SetupQueries
* 3. Use SBlastSequence structure instead of std::pair as return value to
*    blast::GetSequence
*
* Revision 1.27  2004/12/06 17:54:09  grichenk
* Replaced calls to deprecated methods
*
* Revision 1.26  2004/12/02 16:01:24  bealer
* - Change multiple-arrays to array-of-struct in BlastQueryInfo
*
* Revision 1.25  2004/11/30 16:59:16  dondosha
* Call BlastSeqLoc_RestrictToInterval to adjust lower case mask offsets when query restricted to interval
*
* Revision 1.24  2004/11/29 18:47:22  madden
* Fix for SetupQueryInfo for zero-length translated nucl. sequence
*
* Revision 1.23  2004/10/19 16:09:39  dondosha
* Sort HSPs by e-value before converting results to Seq-align, since they are sorted by score coming in.
*
* Revision 1.22  2004/10/06 18:42:08  dondosha
* Cosmetic fix for SunOS compiler warning
*
* Revision 1.21  2004/10/06 14:55:49  dondosha
* Use IBlastSeqInfoSrc interface to get ids and lengths of subject sequences
*
* Revision 1.20  2004/09/21 13:50:19  dondosha
* Translate query in 6 frames for RPS tblastn
*
* Revision 1.19  2004/09/13 15:55:04  madden
* Remove unused parameter from CSeqLoc2BlastSeqLoc
*
* Revision 1.18  2004/09/13 12:47:06  madden
* Changes for redefinition of BlastSeqLoc and BlastMaskLoc
*
* Revision 1.17  2004/09/08 20:10:35  dondosha
* Fix for searches against multiple subjects when some query-subject pairs have no hits
*
* Revision 1.16  2004/08/17 21:49:36  dondosha
* Removed unused variable
*
* Revision 1.15  2004/08/16 19:49:25  dondosha
* Small memory leak fix; catch CSeqVectorException and throw CBlastException out of setup functions
*
* Revision 1.14  2004/07/20 21:11:04  dondosha
* Fixed a memory leak in x_GetSequenceLengthAndId
*
* Revision 1.13  2004/07/19 21:04:20  dondosha
* Added back case when BlastSeqSrc returns a wrong type Seq-id - then retrieve a seqid string and construct CSeq_id from it
*
* Revision 1.12  2004/07/19 14:58:47  dondosha
* Renamed multiseq_src to seqsrc_multiseq, seqdb_src to seqsrc_seqdb
*
* Revision 1.11  2004/07/19 13:56:02  dondosha
* Pass subject SSeqLoc directly to BLAST_OneSubjectResults2CSeqAlign instead of BlastSeqSrc
*
* Revision 1.10  2004/07/15 14:50:09  madden
* removed commented out ASSERT
*
* Revision 1.9  2004/07/06 15:48:40  dondosha
* Use EBlastProgramType enumeration type instead of EProgram when calling C code
*
* Revision 1.8  2004/06/23 14:05:34  dondosha
* Changed CSeq_loc argument in CSeqLoc2BlastMaskLoc to pointer; fixed a memory leak in x_GetSequenceLengthAndId
*
* Revision 1.7  2004/06/22 16:45:14  camacho
* Changed the blast_type_* definitions for the TBlastProgramType enumeration.
*
* Revision 1.6  2004/06/21 15:28:16  jcherry
* Guard against trying to access unset strand of a Seq-interval
*
* Revision 1.5  2004/06/15 22:14:23  dondosha
* Correction in RemapToLoc call for subject Seq-loc on a minus strand
*
* Revision 1.4  2004/06/14 17:46:39  madden
* Use CSeqVector_CI and call SetRandomizeAmbiguities to properly handle gaps in subject sequence
*
* Revision 1.3  2004/06/07 21:34:55  dondosha
* Use 2 booleans for gapped and out-of-frame mode instead of scoring options in function arguments
*
* Revision 1.2  2004/06/07 18:26:29  dondosha
* Bit scores are now filled in HSP lists, so BlastScoreBlk is no longer needed when results are converted to seqalign
*
* Revision 1.1  2004/06/02 16:00:59  bealer
* - New file for objmgr dependent code.
*
* ===========================================================================
*/
