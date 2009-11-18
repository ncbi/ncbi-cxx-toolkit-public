#ifndef __GNOMON__ANNOT__HPP
#define __GNOMON__ANNOT__HPP

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
 * Authors:  Vyacheslav Chetvernin
 *
 * File Description: 
 *
 * Builds annotation models out of chained alignments:
 * selects good chains as alternatively spliced genes,
 * selects good chains inside other chains introns,
 * other chains filtered to leave one chain per placement,
 * gnomon is run to improve chains and predict models in regions w/o chains
 *
 */

#include <algo/gnomon/gnomon_model.hpp>
#include <algo/gnomon/gnomon.hpp>

#include <objects/seqloc/Seq_loc.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(gnomon)

class CAltSplice;

class NCBI_XALGOGNOMON_EXPORT CGnomonAnnotator {
public:
    CGnomonAnnotator();
    void FilterOutSingleExonEST(TGeneModelList& chains);
    void FilterOutSimilarsWithLowerScore(TGeneModelList& cls, int tolerance, TGeneModelList& bad_aligns);
    void FilterOutLowSupportModels(TGeneModelList& cls, int minsupport, int minCDS, bool allow_partialgenes, TGeneModelList& bad_aligns);
    void FilterOutTandemOverlap(TGeneModelList&cls, double fraction, TGeneModelList& bad_aligns);

    TGeneModelList SelectCompleteModels(TGeneModelList& chains,
                              int minIntergenic, double altfrac,
                              int composite, bool opposite,
                              bool allow_partialalts, bool allow_partialgenes, TGeneModelList& bad_aligns);

    void Predict(CConstRef<CHMMParameters> hmm_params, const CResidueVec& seq,
                 TGeneModelList& models,
                 int window, int margin, bool wall, double mpp, double nonconsensp,
                 TGeneModelList& bad_aligns);

private:

    void RemoveShortHolesAndRescore(TGeneModelList chains, const CGnomonEngine& gnomon);
    void FindGenesPass1(const TGeneModelList& cls, list<CAltSplice>& alts,
                        int minIntergenic, double altfrac, int composite,
                        bool allow_opposite_strand, bool allow_partialalts,
                        list<const CGeneModel*>& possibly_alternative, TGeneModelList& rejected);
    void FindGenesPass2(const list<const CGeneModel*>& possibly_alternative, list<CAltSplice>& alts,
                        int minIntergenic, double altfrac, int composite,
                        bool allow_opposite_strand, bool allow_partialalts,
                        TGeneModelList& bad_aligns);
    void FindGenesPass3(const TGeneModelList& rejected, list<CAltSplice>& alts,
                        int minIntergenic, double altfrac, int composite,
                        bool allow_opposite_strand, bool allow_partialalts,
                        TGeneModelList& bad_aligns);
    enum ECompat { eNotCompatible, eAlternative, eNested, eExternal, eOtherGene };
    ECompat CheckCompatibility(const CAltSplice& gene, const CGeneModel& algn, int minIntergenic, double altfrac, int composite, bool allow_opposite_strand, bool allow_partialalts);

    void FindAllCompatibleGenes(TGeneModelList& cls, list<CAltSplice>& alts,
                                int minIntergenic, double altfrac, int composite, bool allow_opposite_strand, bool allow_partialalts,
                                TGeneModelList& bad_aligns);

    void Predict(CConstRef<CHMMParameters> hmm_params, const CResidueVec& seq,
                 TSignedSeqPos llimit, TSignedSeqPos rlimit, TGeneModelList::const_iterator il, TGeneModelList::const_iterator ir,
                 TGeneModelList& models,
                 int window, int margin, bool leftmostwall, bool rightmostwall, bool leftmostanchor, bool rightmostanchor,
                 double mpp, double nonconsensp, TGeneModelList& bad_aligns);

    double TryWithoutObviouslyBadAlignments(TGeneModelList& aligns, TGeneModelList& suspect_aligns, TGeneModelList& bad_aligns,
                                            CGnomonEngine& gnomon,
                                            bool leftwall, bool rightwall, bool leftanchor, bool rightanchor,
                                            TSignedSeqPos left, TSignedSeqPos right,
                                            double mpp, double nonconsensp, TSignedSeqRange& tested_range);
    double TryToEliminateOneAlignment(TGeneModelList& suspect_aligns, TGeneModelList& bad_aligns,
                                      CGnomonEngine& gnomon,
                                      bool leftwall, bool rightwall, bool leftanchor, bool rightanchor,
                                      double mpp, double nonconsensp);
    double TryToEliminateAlignmentsFromTail(TGeneModelList& suspect_aligns, TGeneModelList& bad_aligns,
                                                              CGnomonEngine& gnomon,
                                                              bool leftwall, bool rightwall, bool leftanchor, bool rightanchor,
                                                              double mpp, double nonconsensp);
    double ExtendJustThisChain(CGeneModel& chain, CGnomonEngine& gnomon, TSignedSeqPos left, TSignedSeqPos right,
                               double mpp, double nonconsensp);
    
};

class CModelCompare {
public:
    static bool CanBeConnectedIntoOne(const CGeneModel& a, const CGeneModel& b);
    static size_t CountCommonSplices(const CGeneModel& a, const CGeneModel& b);
    static bool AreSimilar(const CGeneModel& a, const CGeneModel& b, int tolerance);
    static bool BadOverlapTest(const CGeneModel& a, const CGeneModel& b);
    static bool RangeNestedInIntron(TSignedSeqRange r, const CGeneModel& algn);
    static bool HaveCommonExonOrIntron(const CGeneModel& a, const CGeneModel& b);
};

END_SCOPE(gnomon)
END_NCBI_SCOPE

#endif  // __GNOMON__ANNOT__HPP

