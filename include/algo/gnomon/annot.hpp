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
#include <algo/gnomon/chainer.hpp>

#include <objects/seqloc/Seq_loc.hpp>

BEGIN_NCBI_SCOPE

class CArgDescriptions;
class CArgs;

BEGIN_SCOPE(gnomon)

class CAltSplice;

class NCBI_XALGOGNOMON_EXPORT CModelFilters {
public:
    CModelFilters();

    void FilterOutSingleExonEST(TGeneModelList& chains);
    void FilterOutSimilarsWithLowerScore(TGeneModelList& cls, int tolerance, TGeneModelList& bad_aligns);
    void FilterOutLowSupportModels(TGeneModelList& cls, int minsupport, int minCDS, bool allow_partialgenes, TGeneModelList& bad_aligns);
    void FilterOutTandemOverlap(TGeneModelList&cls, double fraction, TGeneModelList& bad_aligns);
    void FilterOutPartialModels(TGeneModelList&cls, TGeneModelList& bad_aligns);
};

class NCBI_XALGOGNOMON_EXPORT CGeneSelector {
public:
    CGeneSelector();
    bool IsPartialAltsAllowed() const { return allow_partialalts; }

    TGeneModelList SelectGenes(TGeneModelList& chains, TGeneModelList& bad_aligns);
    void ReselectGenes(TGeneModelList& models, TGeneModelList& bad_aligns);
    void RenumGenes(TGeneModelList& models, int& gennum, int geninc);

private:

    void FindGenesPass1(const TGeneModelList& cls, list<CAltSplice>& alts,
                        list<const CGeneModel*>& possibly_alternative, TGeneModelList& rejected);
    void FindGenesPass2(const list<const CGeneModel*>& possibly_alternative, list<CAltSplice>& alts,
                        TGeneModelList& bad_aligns);
    void FindGenesPass3(const TGeneModelList& rejected, list<CAltSplice>& alts,
                        TGeneModelList& bad_aligns);
    enum ECompat { eNotCompatible, eAlternative, eNested, eExternal, eOtherGene };
    ECompat CheckCompatibility(const CAltSplice& gene, const CGeneModel& algn);

    void FindAllCompatibleGenes(TGeneModelList& cls, list<CAltSplice>& alts,
                                TGeneModelList& bad_aligns);

    int minIntergenic;
    double altfrac;
    int composite;
    bool allow_opposite_strand;
    bool allow_partialalts;

    friend class CGeneSelectorArgUtil;
};

class NCBI_XALGOGNOMON_EXPORT CGnomonAnnotator : public CGnomonAnnotator_Base {
public:
    CGnomonAnnotator();
    ~CGnomonAnnotator();

    bool GnomonNeeded() const { return do_gnomon; }

    void Predict(TGeneModelList& models, TGeneModelList& bad_aligns);
    void Predict(TGeneModelList& models, TGeneModelList& bad_aligns, TSignedSeqPos left, TSignedSeqPos right);

public:
    int mincontig;
    int tolerance;

    int minCdsLen;
    int minsupport;

private:
    void RemoveShortHolesAndRescore(TGeneModelList chains);
    void Predict(TSignedSeqPos llimit, TSignedSeqPos rlimit, TGeneModelList::const_iterator il, TGeneModelList::const_iterator ir,
                 TGeneModelList& models,
                 bool leftmostwall, bool rightmostwall, bool leftmostanchor, bool rightmostanchor,
                 TGeneModelList& bad_aligns);

    double TryWithoutObviouslyBadAlignments(TGeneModelList& aligns, TGeneModelList& suspect_aligns, TGeneModelList& bad_aligns,
                                            bool leftwall, bool rightwall, bool leftanchor, bool rightanchor,
                                            TSignedSeqPos left, TSignedSeqPos right,
                                            TSignedSeqRange& tested_range);
    double TryToEliminateOneAlignment(TGeneModelList& suspect_aligns, TGeneModelList& bad_aligns,
                                      bool leftwall, bool rightwall, bool leftanchor, bool rightanchor);
    double TryToEliminateAlignmentsFromTail(TGeneModelList& suspect_aligns, TGeneModelList& bad_aligns,
                                            bool leftwall, bool rightwall, bool leftanchor, bool rightanchor);
    double ExtendJustThisChain(CGeneModel& chain, TSignedSeqPos left, TSignedSeqPos right);
    
    bool do_gnomon;
    int window;
    int margin;
    bool wall;
    double mpp;
    double nonconsensp;

    friend class CGnomonAnnotatorArgUtil;
};

struct RemoveTrailingNs : public TransformFunction {
    RemoveTrailingNs(const CResidueVec& seq);

    virtual void transform_model(CGeneModel& a);
private:
    const CResidueVec& seq;
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

class NCBI_XALGOGNOMON_EXPORT CGeneSelectorArgUtil {
public:
    static void SetupArgDescriptions(CArgDescriptions* arg_desc);
    static void ReadArgs(CGeneSelector* annot, const CArgs& args);
};

class NCBI_XALGOGNOMON_EXPORT CGnomonAnnotatorArgUtil {
public:
    static void SetupArgDescriptions(CArgDescriptions* arg_desc);
    static void ReadArgs(CGnomonAnnotator* annot, const CArgs& args);
};

END_SCOPE(gnomon)
END_NCBI_SCOPE

#endif  // __GNOMON__ANNOT__HPP

