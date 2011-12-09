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
* Author:  Kamen Todorov, NCBI
*
* File Description:
*   Alignment mix
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <objtools/alnmgr/alnmix.hpp>
#include <objtools/alnmgr/alnvec.hpp>
#include <objtools/alnmgr/alnmerger.hpp>

#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seqloc/Seq_id.hpp>

#include <serial/iterator.hpp>

#include <algorithm>

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE // namespace ncbi::objects::


CAlnMix::CAlnMix(void)
    : x_CalculateScore(0)
{
    x_Init();
}


CAlnMix::CAlnMix(CScope& scope,
                 TCalcScoreMethod calc_score)
    : m_Scope(&scope),
      x_CalculateScore(calc_score)
{
    if ( !x_CalculateScore ) {
        x_CalculateScore = &CAlnVec::CalculateScore;
    }
    x_Init();
}


CAlnMix::~CAlnMix(void)
{
}


void 
CAlnMix::x_Init()
{
    m_AlnMixSequences = m_Scope.IsNull() ? 
        new CAlnMixSequences() :
        new CAlnMixSequences(*m_Scope);
    m_AlnMixMatches   = new CAlnMixMatches(m_AlnMixSequences, x_CalculateScore);
    m_AlnMixMerger    = new CAlnMixMerger(m_AlnMixMatches, x_CalculateScore);
}


void 
CAlnMix::x_Reset()
{
    m_AlnMixMerger->Reset();
}


void 
CAlnMix::Add(const CSeq_align& aln, TAddFlags flags)
{
    if (m_InputAlnsMap.find((void *)&aln) == m_InputAlnsMap.end()) {
        // add only if not already added
        m_InputAlnsMap[(void *)&aln] = &aln;
        m_InputAlns.push_back(CConstRef<CSeq_align>(&aln));

        if (aln.GetSegs().IsDenseg()) {
            Add(aln.GetSegs().GetDenseg(), flags);
        } else if (aln.GetSegs().IsStd()) {
            CRef<CSeq_align> sa = aln.CreateDensegFromStdseg
                (m_Scope ? this : 0);
            Add(*sa, flags);
        } else if (aln.GetSegs().IsDisc()) {
            ITERATE (CSeq_align_set::Tdata,
                     aln_it,
                     aln.GetSegs().GetDisc().Get()) {
                Add(**aln_it, flags);
            }
        }
    }
}


void 
CAlnMix::Add(const CDense_seg &ds, TAddFlags flags)
{
    const CDense_seg* dsp = &ds;

    if (m_InputDSsMap.find((void *)dsp) != m_InputDSsMap.end()) {
        return; // it has already been added
    }
    x_Reset();
#if _DEBUG
    dsp->Validate(true);
#endif    

    // translate (extend with widths) the dense-seg if necessary
    if (flags & fForceTranslation  && !dsp->IsSetWidths()) {
        if ( !m_Scope ) {
            string errstr = string("CAlnMix::Add(): ") 
                + "Cannot force translation for Dense_seg "
                + NStr::NumericToString(m_InputDSs.size() + 1) + ". "
                + "Neither CDense_seg::m_Widths are supplied, "
                + "nor OM is used to identify molecule type.";
            NCBI_THROW(CAlnException, eMergeFailure, errstr);
        } else {
            m_InputDSs.push_back(x_ExtendDSWithWidths(*dsp));
            dsp = m_InputDSs.back();
        }
    } else {
        m_InputDSs.push_back(CConstRef<CDense_seg>(dsp));
    }

    if (flags & fCalcScore) {
        if ( !x_CalculateScore ) {
            // provide the default calc method
            x_CalculateScore = &CAlnVec::CalculateScore;
        }
    }
    if ( !m_Scope  &&  x_CalculateScore) {
        NCBI_THROW(CAlnException, eMergeFailure, "CAlnMix::Add(): "
                   "Score calculation requested without providing "
                   "a scope in the CAlnMix constructor.");
    }
    m_AddFlags = flags;

    m_InputDSsMap[(void *)dsp] = dsp;

    m_AlnMixSequences->Add(*dsp, flags);

    m_AlnMixMatches->Add(*dsp, flags);
}


CRef<CDense_seg>
CAlnMix::x_ExtendDSWithWidths(const CDense_seg& ds)
{
    if (ds.IsSetWidths()) {
        NCBI_THROW(CAlnException, eMergeFailure,
                   "CAlnMix::x_ExtendDSWithWidths(): "
                   "Widths already exist for the input alignment");
    }

    bool contains_AA = false, contains_NA = false;
    CRef<CAlnMixSeq> aln_seq;
    for (CDense_seg::TDim numrow = 0;  numrow < ds.GetDim();  numrow++) {
        m_AlnMixSequences->x_IdentifyAlnMixSeq(aln_seq, *ds.GetIds()[numrow]);
        if (aln_seq->m_IsAA) {
            contains_AA = true;
        } else {
            contains_NA = true;
        }
    }
    if (contains_AA  &&  contains_NA) {
        NCBI_THROW(CAlnException, eMergeFailure,
                   "CAlnMix::x_ExtendDSWithWidths(): "
                   "Incorrect input Dense-seg: Contains both AAs and NAs but "
                   "widths do not exist!");
    }        

    CRef<CDense_seg> new_ds(new CDense_seg());

    // copy from the original
    new_ds->Assign(ds);

    if (contains_NA) {
        // fix the lengths
        const CDense_seg::TLens& lens     = ds.GetLens();
        CDense_seg::TLens&       new_lens = new_ds->SetLens();
        for (CDense_seg::TNumseg numseg = 0; numseg < ds.GetNumseg(); numseg++) {
            if (lens[numseg] % 3) {
                string errstr =
                    string("CAlnMix::x_ExtendDSWithWidths(): ") +
                    "Length of segment " + NStr::IntToString(numseg) +
                    " is not divisible by 3.";
                NCBI_THROW(CAlnException, eMergeFailure, errstr);
            } else {
                new_lens[numseg] = lens[numseg] / 3;
            }
        }
    }

    // add the widths
    CDense_seg::TWidths&  new_widths  = new_ds->SetWidths();
    new_widths.resize(ds.GetDim(), contains_NA ? 3 : 1);
#if _DEBUG
    new_ds->Validate(true);
#endif
    return new_ds;
}


void
CAlnMix::ChooseSeqId(CSeq_id& id1, const CSeq_id& id2)
{
    CRef<CAlnMixSeq> aln_seq1, aln_seq2;
    m_AlnMixSequences->x_IdentifyAlnMixSeq(aln_seq1, id1);
    m_AlnMixSequences->x_IdentifyAlnMixSeq(aln_seq2, id2);
    if (aln_seq1->m_BioseqHandle != aln_seq2->m_BioseqHandle) {
        string errstr = 
            string("CAlnMix::ChooseSeqId(CSeq_id& id1, const CSeq_id& id2):")
            + " Seq-ids: " + id1.AsFastaString() 
            + " and " + id2.AsFastaString() 
            + " do not resolve to the same bioseq handle,"
            " but are used on the same 'row' in different segments."
            " This is legally allowed in a Std-seg, but conversion to"
            " Dense-seg cannot be performed.";
        NCBI_THROW(CAlnException, eInvalidSeqId, errstr);
    }
    CRef<CSeq_id> id1cref(&id1);
    CRef<CSeq_id> id2cref(&(const_cast<CSeq_id&>(id2)));
    if (CSeq_id::BestRank(id1cref) > CSeq_id::BestRank(id2cref)) {
        id1.Reset();
        SerialAssign<CSeq_id>(id1, id2);
    }
}    


void
CAlnMix::Merge(TMergeFlags flags)
{
    x_SetTaskName("Sorting");
    if (flags & fSortSeqsByScore) {
        if (flags & fSortInputByScore) {
            m_AlnMixSequences->SortByChainScore();
        } else {
            m_AlnMixSequences->SortByScore();
        }
    }
    if (flags & fSortInputByScore) {
        m_AlnMixMatches->SortByChainScore();
    } else {
        m_AlnMixMatches->SortByScore();
    }
    x_SetTaskName("Merging");
    m_AlnMixMerger->SetTaskProgressCallback(x_GetTaskProgressCallback());
    m_AlnMixMerger->Merge(flags);
}


const CDense_seg&
CAlnMix::GetDenseg() const
{
    return m_AlnMixMerger->GetDenseg();
}


const CSeq_align&
CAlnMix::GetSeqAlign() const
{
    return m_AlnMixMerger->GetSeqAlign();
}


END_objects_SCOPE // namespace ncbi::objects::
END_NCBI_SCOPE
