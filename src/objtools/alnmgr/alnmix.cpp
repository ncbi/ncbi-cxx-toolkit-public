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
                + NStr::IntToString(m_InputDSs.size() + 1) + ". "
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
    if (flags & fSortSeqsByScore) {
        m_AlnMixSequences->SortByScore();
    }
    m_AlnMixMatches->SortByScore();
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

/*
* ===========================================================================
*
* $Log$
* Revision 1.122  2005/03/08 16:21:41  todorov
* Added CAlnMix::EAddFlags to m_AlnMixMatches->Add
*
* Revision 1.121  2005/03/08 00:02:50  todorov
* Provide CScope when calling the CAlnMixSequences constructor, in case CScope
* was provided in CAlnMix.
*
* Revision 1.120  2005/03/01 17:28:49  todorov
* Rearranged CAlnMix classes
*
* Revision 1.119  2005/02/17 14:45:15  ucko
* Tweak to fix compilation with GCC 2.95.
*
* Revision 1.118  2005/02/16 21:27:16  todorov
* Abstracted the CalculateScore method so that it could be delegated to
* the caller.
* Fixed a bug related to widths not been provided when creating seq
* vectors.
* Fixed a few signed/unsigned comparison warnings.
*
* Revision 1.117  2005/02/10 21:29:26  todorov
* Throwing exeption for proper row not found in case of
* fQuerySeqMergeOnly only if !fTruncateOverlaps, since full truncation
* (AKA match ignore) might have occurred for each match at the given
* point in time.
*
* Revision 1.116  2005/02/08 15:58:37  todorov
* Added handling for best reciprocal hits.
*
* Revision 1.115  2004/11/02 18:31:01  todorov
* Changed (mostly shortened) a few internal members' names
* for convenience and consistency
*
* Revision 1.114  2004/11/02 18:04:36  todorov
* CAlnMixSeq += m_ExtraRowIdx
* x_CreateSegmentsVector() += conditionally compiled (_ALNMGR_TRACE) sections
*
* Revision 1.113  2004/10/26 17:46:32  todorov
* Prevented iterator invalidation in case refseq needs to be moved to first row
*
* Revision 1.112  2004/10/18 15:10:45  todorov
* Use of OM now only depends on whether scope was provided at construction time
*
* Revision 1.111  2004/10/13 17:51:33  todorov
* rm conditional compilation logic
*
* Revision 1.110  2004/10/13 16:29:17  todorov
* 1) Added x_SegmentStartItsConsistencyCheck.
* 2) Within x_SecondRowFits, iterate through seq2->m_ExtraRow to find
*    the proper strand, if such exists.
*
* Revision 1.109  2004/10/12 19:55:14  rsmith
* make x_CompareAlnSeqScores arguments match the container it compares on.
*
* Revision 1.108  2004/09/27 16:18:16  todorov
* + truncate segments of sequences on multiple frames
*
* Revision 1.107  2004/09/23 18:55:31  todorov
* 1) avoid an introduced m_DsCnt mismatch; 2) + reeval x_SecondRowFits for some cases
*
* Revision 1.106  2004/09/22 17:00:53  todorov
* +CAlnMix::fPreserveRows
*
* Revision 1.105  2004/09/22 14:26:35  todorov
* changed TSegments & TSegmentsContainer from vectors to lists
*
* Revision 1.104  2004/09/09 23:10:41  todorov
* Fixed the multi-framed ref sequence case
*
* Revision 1.103  2004/09/01 17:36:07  todorov
* extended the unable-to-load-data exception msg even further
*
* Revision 1.102  2004/09/01 15:38:05  todorov
* extended exception msg in case unable to load data for segment
*
* Revision 1.101  2004/06/28 20:09:27  todorov
* Initialize m_DsIdx. Also bug fix when shifting to extra row
*
* Revision 1.100  2004/06/23 22:22:23  todorov
* Fixed condition logic
*
* Revision 1.99  2004/06/23 18:31:10  todorov
* Calculate the prevailing strand per row (using scores)
*
* Revision 1.98  2004/06/22 01:24:04  ucko
* Restore subtraction accidentally dropped in last commit; should
* resolve infinite loops.
*
* Revision 1.97  2004/06/21 20:35:48  todorov
* Fixed a signed/unsigned bug when truncating the len with delta
*
* Revision 1.96  2004/06/14 21:01:59  todorov
* 1) added iterators bounds checks; 2) delta check
*
* Revision 1.95  2004/05/25 19:16:33  todorov
* initialize second_row_fits
*
* Revision 1.94  2004/05/25 16:00:10  todorov
* remade truncation of overlaps
*
* Revision 1.93  2004/05/21 21:42:51  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.92  2004/04/13 18:02:10  todorov
* Added some limit checks when inc/dec iterators
*
* Revision 1.91  2004/03/30 23:27:32  todorov
* Switch from CAlnMix::x_RankSeqId() to CSeq_id::BestRank()
*
* Revision 1.90  2004/03/30 20:41:29  todorov
* Rearranged the rank of seq-ids according to the C toolkit rearrangement
*
* Revision 1.89  2004/03/29 17:05:06  todorov
* extended exception msg
*
* Revision 1.88  2004/03/09 17:40:07  kuznets
* Compilation bug fix CAlnMix::ChooseSeqId
*
* Revision 1.87  2004/03/09 17:16:16  todorov
* + SSeqIdChooser implementation
*
* Revision 1.86  2004/01/16 23:59:45  todorov
* + missing width in seg calcs
*
* Revision 1.85  2003/12/22 19:30:35  kuznets
* Fixed compilation error (operator ambiguity) (MSVC)
*
* Revision 1.84  2003/12/22 18:30:37  todorov
* ObjMgr is no longer created internally. Scope should be passed as a reference in the ctor
*
* Revision 1.83  2003/12/18 19:46:27  todorov
* iterate -> ITERATE
*
* Revision 1.82  2003/12/12 22:42:53  todorov
* Init frames and use refseq instead of seq1 which may be refseqs child
*
* Revision 1.81  2003/12/10 16:14:38  todorov
* + exception when merge with no input
*
* Revision 1.80  2003/12/08 21:28:03  todorov
* Forced Translation of Nucleotide Sequences
*
* Revision 1.79  2003/11/24 17:11:32  todorov
* SetWidths only if necessary
*
* Revision 1.78  2003/11/03 14:43:44  todorov
* Use CDense_seg::Validate()
*
* Revision 1.77  2003/10/03 19:22:55  todorov
* Extended exception msg in case of empty row
*
* Revision 1.76  2003/09/16 14:45:56  todorov
* more informative exception strng
*
* Revision 1.75  2003/09/12 16:18:36  todorov
* -unneeded checks (">=" for unsigned)
*
* Revision 1.74  2003/09/12 15:42:36  todorov
* +CRef to delay obj deletion while iterating on it
*
* Revision 1.73  2003/09/08 20:44:21  todorov
* expression bug fix
*
* Revision 1.72  2003/09/08 19:48:55  todorov
* signed vs unsigned warnings fixed
*
* Revision 1.71  2003/09/08 19:24:04  todorov
* fix strand
*
* Revision 1.70  2003/09/08 19:18:32  todorov
* plus := all strands != minus
*
* Revision 1.69  2003/09/06 02:39:34  todorov
* OBJECTS_ALNMGR___ALNMIX__DBG -> _DEBUG
*
* Revision 1.68  2003/08/28 19:54:58  todorov
* trailing gap on master fix
*
* Revision 1.67  2003/08/20 19:35:32  todorov
* Added x_ValidateDenseg
*
* Revision 1.66  2003/08/20 14:34:58  todorov
* Support for NA2AA Densegs
*
* Revision 1.65  2003/08/01 20:49:26  todorov
* Changed the control of the main Merge() loop
*
* Revision 1.64  2003/07/31 18:50:29  todorov
* Fixed a couple of truncation problems; Added a match deletion
*
* Revision 1.63  2003/07/15 20:54:01  todorov
* exception type fixed
*
* Revision 1.62  2003/07/09 22:58:12  todorov
* row index bug fix in case of fillunaln
*
* Revision 1.61  2003/07/01 17:40:20  todorov
* fFillUnaligned bug fix
*
* Revision 1.60  2003/06/26 21:35:48  todorov
* + fFillUnalignedRegions
*
* Revision 1.59  2003/06/25 15:17:31  todorov
* truncation consistent for the whole segment now
*
* Revision 1.58  2003/06/24 15:24:14  todorov
* added optional truncation of overlaps
*
* Revision 1.57  2003/06/20 03:06:39  todorov
* Setting the seqalign type
*
* Revision 1.56  2003/06/19 18:37:19  todorov
* typo fixed
*
* Revision 1.55  2003/06/09 20:54:20  todorov
* Use of ObjMgr is now optional
*
* Revision 1.54  2003/06/03 20:56:52  todorov
* Bioseq handle validation
*
* Revision 1.53  2003/06/03 16:07:05  todorov
* Fixed overlap consistency check in case strands differ
*
* Revision 1.52  2003/06/03 14:38:26  todorov
* warning fixed
*
* Revision 1.51  2003/06/02 17:39:40  todorov
* Changes in rev 1.49 were lost. Reapplying them
*
* Revision 1.50  2003/06/02 16:06:40  dicuccio
* Rearranged src/objects/ subtree.  This includes the following shifts:
*     - src/objects/asn2asn --> arc/app/asn2asn
*     - src/objects/testmedline --> src/objects/ncbimime/test
*     - src/objects/objmgr --> src/objmgr
*     - src/objects/util --> src/objmgr/util
*     - src/objects/alnmgr --> src/objtools/alnmgr
*     - src/objects/flat --> src/objtools/flat
*     - src/objects/validator --> src/objtools/validator
*     - src/objects/cddalignview --> src/objtools/cddalignview
* In addition, libseq now includes six of the objects/seq... libs, and libmmdb
* replaces the three libmmdb? libs.
*
* Revision 1.49  2003/05/30 17:42:03  todorov
* 1) Bug fix in x_SecondRowFits
* 2) x_CreateSegmentsVector now uses a stack to order the segs
*
* Revision 1.48  2003/05/23 18:11:42  todorov
* More detailed exception txt
*
* Revision 1.47  2003/05/20 21:20:59  todorov
* mingap minus strand multiple segs bug fix
*
* Revision 1.46  2003/05/15 23:31:26  todorov
* Minimize gaps bug fix
*
* Revision 1.45  2003/05/09 16:41:27  todorov
* Optional mixing of the query sequence only
*
* Revision 1.44  2003/04/24 16:15:57  vasilche
* Added missing includes and forward class declarations.
*
* Revision 1.43  2003/04/15 14:21:12  vasilche
* Fixed order of member initializers.
*
* Revision 1.42  2003/04/14 18:03:19  todorov
* reuse of matches bug fix
*
* Revision 1.41  2003/04/01 20:25:31  todorov
* fixed iterator-- behaviour at the begin()
*
* Revision 1.40  2003/03/28 16:47:28  todorov
* Introduced TAddFlags (fCalcScore for now)
*
* Revision 1.39  2003/03/26 16:38:24  todorov
* mix independent densegs
*
* Revision 1.38  2003/03/18 17:16:05  todorov
* multirow inserts bug fix
*
* Revision 1.37  2003/03/12 15:39:47  kuznets
* iterate -> ITERATE
*
* Revision 1.36  2003/03/10 22:12:02  todorov
* fixed x_CompareAlnMatchScores callback
*
* Revision 1.35  2003/03/06 21:42:38  todorov
* bug fix in x_Reset()
*
* Revision 1.34  2003/03/05 21:53:24  todorov
* clean up miltiple mix case
*
* Revision 1.33  2003/03/05 17:42:41  todorov
* Allowing multiple mixes + general case speed optimization
*
* Revision 1.32  2003/03/05 16:18:17  todorov
* + str len err check
*
* Revision 1.31  2003/02/24 19:01:31  vasilche
* Use faster version of CSeq_id::Assign().
*
* Revision 1.30  2003/02/24 18:46:38  todorov
* Fixed a - strand row info creation bug
*
* Revision 1.29  2003/02/11 21:32:44  todorov
* fMinGap optional merging algorithm
*
* Revision 1.28  2003/02/04 00:05:16  todorov
* GetSeqData neg strand range bug
*
* Revision 1.27  2003/01/27 22:30:30  todorov
* Attune to seq_vector interface change
*
* Revision 1.26  2003/01/23 16:30:18  todorov
* Moved calc score to alnvec
*
* Revision 1.25  2003/01/22 21:00:21  dicuccio
* Fixed compile error - transposed #endif and }
*
* Revision 1.24  2003/01/22 19:39:13  todorov
* 1) Matches w/ the 1st non-gapped row added only; the rest used to calc seqs'
* score only
* 2) Fixed a couple of potential problems
*
* Revision 1.23  2003/01/17 18:56:26  todorov
* Perform merge algorithm even if only one input denseg
*
* Revision 1.22  2003/01/16 17:40:19  todorov
* Fixed strand param when calling GetSeqVector
*
* Revision 1.21  2003/01/10 17:37:28  todorov
* Fixed a bug in fNegativeStrand
*
* Revision 1.20  2003/01/10 15:12:13  dicuccio
* Ooops.  Methinks I should read my e-mail more thoroughly - thats '!= NULL', not
* '== NULL'.
*
* Revision 1.19  2003/01/10 15:11:12  dicuccio
* Small bug fixes: changed 'iterate' -> 'non_const_iterate' in x_Merge(); changed
* logic of while() in x_CreateRowsVector() to avoid compiler warning.
*
* Revision 1.18  2003/01/10 00:42:53  todorov
* Optional sorting of seqs by score
*
* Revision 1.17  2003/01/10 00:11:37  todorov
* Implemented fNegativeStrand
*
* Revision 1.16  2003/01/07 17:23:49  todorov
* Added conditionally compiled validation code
*
* Revision 1.15  2003/01/02 20:03:48  todorov
* Row strand init & check
*
* Revision 1.14  2003/01/02 16:40:11  todorov
* Added accessors to the input data
*
* Revision 1.13  2003/01/02 15:30:17  todorov
* Fixed the order of checks in x_SecondRowFits
*
* Revision 1.12  2002/12/30 20:55:47  todorov
* Added range fitting validation to x_SecondRowFits
*
* Revision 1.11  2002/12/30 18:08:39  todorov
* 1) Initialized extra rows' m_StartIt
* 2) Fixed a bug in x_SecondRowFits
*
* Revision 1.10  2002/12/27 23:09:56  todorov
* Additional inconsistency checks in x_SecondRowFits
*
* Revision 1.9  2002/12/27 17:27:13  todorov
* Force positive strand in all cases but negative
*
* Revision 1.8  2002/12/27 16:39:13  todorov
* Fixed a bug in the single Dense-seg case.
*
* Revision 1.7  2002/12/23 18:03:51  todorov
* Support for putting in and getting out Seq-aligns
*
* Revision 1.6  2002/12/19 00:09:23  todorov
* Added optional consolidation of segments that are gapped on the query.
*
* Revision 1.5  2002/12/18 18:58:17  ucko
* Tweak syntax to avoid confusing MSVC.
*
* Revision 1.4  2002/12/18 03:46:00  todorov
* created an algorithm for mixing alignments that share a single sequence.
*
* Revision 1.3  2002/10/25 20:02:41  todorov
* new fTryOtherMethodOnFail flag
*
* Revision 1.2  2002/10/24 21:29:13  todorov
* adding Dense-segs instead of Seq-aligns
*
* Revision 1.1  2002/08/23 14:43:52  ucko
* Add the new C++ alignment manager to the public tree (thanks, Kamen!)
*
*
* ===========================================================================
*/
