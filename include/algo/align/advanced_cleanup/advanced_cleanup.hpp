#ifndef GPIPE_ALIGN_PROC__ADVANCED_CLEANUP__HPP
#define GPIPE_ALIGN_PROC__ADVANCED_CLEANUP__HPP

/* $Id$
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
 * Author:  Eyal Mozes
 *
 * File Description:
 *
 */

#include <corelib/ncbistd.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>

#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Seq_align_set.hpp>

#include <algo/align/splign/splign.hpp>
#include <algo/align/util/compartment_finder.hpp>
#include <algo/align/prosplign/compartments.hpp>
#include <algo/align/prosplign/prosplign.hpp>

BEGIN_NCBI_SCOPE


///////////////////////////////////////////////////////////////////////

class CAdvancedAlignCleanup
{
public:
    static void SetupArgDescriptions(CArgDescriptions &arg_desc);

    enum EQueryType {
        eInfer,
        eGenomic,
        eRna,
        eProtein
    };

    enum ESplignDirRun {
        eDirSense,
        eDirAntisense,
        eDirBoth
    };

    typedef CCompartmentAccessor<CSplign::THit> TAccessor;
    typedef TAccessor::TCoord                   TCoord;
    typedef pair<TCoord,TCoord>                 TCoordRange;

    struct CSplignCompartment {
        objects::CSeq_id_Handle query;
        TSeqPos query_length;
        objects::CSeq_id_Handle subject;
        CSplign::THitRefs hits;
        TCoordRange coord_range;
        TSeqRange range;

        static unsigned s_MaxRnaIntronSize;
        static unsigned s_MinRnaTotalCoverage;

        bool HasRnaCharacteristics();
    };

    typedef map<objects::CSeq_id_Handle, CSeq_align_set::Tdata> TAlignsBySubject;

    typedef map<objects::CSeq_id_Handle, TAlignsBySubject> TAlignsBySeqIds;

    typedef vector< pair<TSeqPos, CRef< objects::CSeq_align > > > TAlignsByPos;


    CAdvancedAlignCleanup();
    ~CAdvancedAlignCleanup();
    void Reset();

    void SetParams(const CArgs& args);
    void SetScope(const CRef<objects::CScope> &scope);

    void SetHardMaskRanges(objects::CSeq_id_Handle idh, const CSplign::TSeqRangeColl& mask_ranges) {
        m_Splign.SetHardMaskRanges(idh, mask_ranges);
    }

    /// Divide list of RNA alignments into Splign compartments
    /// @param one_pair - If true, all alignments are guaranteed to have the
    ///      same query and subject
    void Cleanup(const objects::CSeq_align_set::Tdata& input_aligns,
                 objects::CSeq_align_set::Tdata& cleaned_aligns,
                 EQueryType query_type = eInfer,
                 bool with_best_placement = true,
                 bool one_pair = false,
                 ESplignDirRun splign_direction = eDirBoth);

    void Cleanup(const TAlignsBySubject& query_aligns,
                 objects::CSeq_align_set::Tdata& cleaned_aligns,
                 EQueryType query_type = eInfer,
                 bool with_best_placement = true,
                 ESplignDirRun splign_direction = eDirBoth);

    void DivideByQuerySubjectPairs(const objects::CSeq_align_set::Tdata& input_aligns,
                                   TAlignsBySeqIds &aligns_by_pair);

    /// Divide list of RNA alignments into Splign compartments
    /// @param one_pair - If true, all alignments are guaranteed to have the
    ///      same query and subject
    void GetSplignCompartments(const objects::CSeq_align_set::Tdata& input_aligns,
                               list<CSplignCompartment> &compartments,
                               bool one_pair = false);

    /// Divide list of genomic alignments into compartments
    /// @param one_pair - If true, all alignments are guaranteed to have the
    ///      same query and subject
    void GetGenomicCompartments(const objects::CSeq_align_set::Tdata& input_aligns,
                                list< CRef<objects::CSeq_align_set> > &compartments,
                                bool one_pair = false);

    void GetProsplignCompartments(const objects::CSeq_align_set::Tdata& input_aligns,
                                  prosplign::TCompartments &compartments,
                                  bool one_pair = false,
                                  TAlignsByPos *aligns_by_pos = NULL);

    CRef<objects::CSeq_align>
    RunSplignOnCompartment(const CSplignCompartment &compart,
                           ESplignDirRun dir);

    void CleanupGenomicCompartment(const objects::CSeq_align_set::Tdata& compart,
                                   objects::CSeq_align_set::Tdata& cleaned_aligns,
                                   bool add_scores = true);

    void BestPlacement(objects::CSeq_align_set::Tdata &aligns);

private:
    CRef<objects::CScope> m_Scope;

    CSplign m_Splign;

    double m_Penalty;
    double m_MinIdty;
    double m_MinSingletonIdty;
    TSeqPos m_MinSingletonIdtyBps;
    TSeqPos m_MaxIntron;
    bool m_NoXF;
    int m_GenomicCompartOptions;
    unsigned m_MaxCompartmentFailures;
    unsigned m_CompartmentFailureCount;

    CRef<CProSplign>              m_ProSplign;
    CRef<CProSplignScoring>       m_ProSplignScoring;
    CRef<CProSplignOutputOptions> m_ProSplignOutputOptions;
    unique_ptr<prosplign::CCompartOptions>     m_CompartOptions;
    map<objects::CSeq_id_Handle, vector<pair<TSeqPos, TSeqPos> > > m_seq_gaps;
    bool m_ProsplignGaps;
    bool m_ProsplignUnknownGaps;
    TSeqPos m_ProsplignRunThreshold;

    class CSplignAlignmentHit : public CBlastTabular
    {
    public:
        CSplignAlignmentHit(const objects::CSeq_align &align);

        ~CSplignAlignmentHit();

        CRef<objects::CSeq_align> GetAlign() const { return m_AlignRef; }

    private:
        CRef<objects::CSeq_align> m_AlignRef;
    };

    bool x_CleanupProsplignCompartment(const objects::CSeq_annot &compartment,
                  const TAlignsByPos &aligns_by_pos,
                  objects::CSeq_align_set::Tdata& cleaned_aligns,
                  TSeqRange &genomic_range);

    void x_CleanupProsplignAsGenomic(const TAlignsByPos &aligns_by_pos,
                  const TSeqRange &genomic_range,
                  objects::CSeq_align_set::Tdata& cleaned_aligns);

    void x_SplignCompartmentsToGenomicFormat(
         const list<CSplignCompartment> &splign_compartments,
         list< CRef<objects::CSeq_align_set> > &genomic_compartments);

    void x_AddStandardAlignmentScores(objects::CSeq_align& align);
};



///////////////////////////////////////////////////////////////////////


END_NCBI_SCOPE

#endif  // GPIPE_ALIGN_PROC__ADVANCED_CLEANUP__HPP

