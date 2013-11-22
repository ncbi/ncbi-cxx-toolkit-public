#ifndef ALGO_GNOMON___CHAINER__HPP
#define ALGO_GNOMON___CHAINER__HPP

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
 * Authors:  Alexandre Souvorov
 *
 * File Description:
 *
 */

#include <algo/gnomon/gnomon_model.hpp>

BEGIN_SCOPE(ncbi);

class CArgDescriptions;
class CArgs;

BEGIN_SCOPE(gnomon);

class CHMMParameters;

struct SMinScor {
    double m_min;
    double m_i5p_penalty;
    double m_i3p_penalty;
    double m_cds_bonus;
    double m_length_penalty;
    double m_minprotfrac;
    double m_endprotfrac;
    int m_prot_cds_len;
    int m_cds_len;
    double m_utr_clip_threshold;
    int m_minsupport;
    int m_minsupport_mrna;
    int m_minsupport_rnaseq;
    int m_minlen;
};


// redefine STL algorithms to take Function Object pointer to allow for inheritance

template <class Container, class Predicate>
void remove_if(Container& c, Predicate* __pred)
{
    typedef typename Container::iterator Iterator;
    Iterator __first = c.begin();
    Iterator __last = c.end();
    while (__first != __last) {
        if ((*__pred)(*__first)) {
            (__first)->Status() |= CGeneModel::eSkipped;
            (__first)->AddComment(__pred->GetComment());
            __first = c.erase(__first);
        } else
            ++__first;
    }
    delete __pred;
}

template <class Container, class UnaryFunction>
void transform(Container& c,  UnaryFunction* op)
{
    typedef typename Container::iterator Iterator;
    Iterator __first = c.begin();
    Iterator __last = c.end();
    for (;__first != __last;++__first)
	(*op)(*__first);
    delete op;
}

struct TransformFunction {
    virtual ~TransformFunction() {}
    void operator()(CGeneModel& a) { transform_model(a); }
    void operator()(CAlignModel& a) { transform_align(a); }

    virtual void transform_model(CGeneModel& a) {}
    virtual void transform_align(CAlignModel& a) { transform_model(a); }
};
struct Predicate {
    virtual ~Predicate() {}
    virtual string GetComment() { return "reason not given"; }
    bool operator()(CGeneModel& a) { return  model_predicate(a); }
    bool operator()(CAlignModel& a) { return align_predicate(a); }

    virtual bool model_predicate(CGeneModel& a) { return false; }
    virtual bool align_predicate(CAlignModel& a) { return model_predicate(a); }
};

class NCBI_XALGOGNOMON_EXPORT CGnomonAnnotator_Base {
public:
    CGnomonAnnotator_Base();
    virtual ~CGnomonAnnotator_Base();

    void SetHMMParameters(CHMMParameters* params);
    void EnableSeqMasking();
    void SetGenomic(const CResidueVec& seq);
    void SetGenomic(const CSeq_id& seqid, objects::CScope& scope, const string& mask_annots = kEmptyStr, const TInDels* contig_fix_idels = 0);
    CGnomonEngine& GetGnomon();
    void MapAlignmentsToEditedContig(TAlignModelList& alignments) const;
    void MapModelsToEditedContig(TGeneModelList& models) const;
    void MapModelsToOrigContig(TGeneModelList& models) const;
    static TInDels GetGenomicGaps(const TGeneModelList& models);

    typedef map<int,TInDels::const_iterator> TGgapInfo;

protected:
    CAlignModel MapOneModelToEditedContig(const CGeneModel& align) const;

    bool m_masking;
    CRef<CHMMParameters> m_hmm_params;
    auto_ptr<CGnomonEngine> m_gnomon;
    CAlignMap* m_edited_contig_map;
    TInDels m_editing_indels;      //original coordinates
    TGgapInfo m_inserted_seqs;     // edited coord to indelinfo
};

////////////////////////////////////////////////////////////////////////
class CChainerArgUtil;

class NCBI_XALGOGNOMON_EXPORT CChainer : public CGnomonAnnotator_Base {
public:
    CChainer();
    ~CChainer();

    void SetIntersectLimit(int value);
    void SetTrim(int trim);
    void SetMinPolyA(int minpolya);
    SMinScor& SetMinScor();
    void SetMinInframeFrac(double mininframefrac);
    map<string, pair<bool,bool> >& SetProtComplet();
    map<string,TSignedSeqRange>& SetMrnaCDS();
    void SetGenomicRange(const TAlignModelList& alignments);
    void SetNumbering(int idnext, int idinc);
    void SetOnlyBestFs(bool onlybestfs);

    TransformFunction* TrimAlignment();
    TransformFunction* DoNotBelieveShortPolyATail();
    Predicate* OverlapsSameAccessionAlignment(TAlignModelList& alignments);
    Predicate* ConnectsParalogs(TAlignModelList& alignments);
    TransformFunction* ProjectCDS(objects::CScope& scope);
    TransformFunction* DoNotBelieveFrameShiftsWithoutCdsEvidence();
    void SetConfirmedStartStopForProteinAlignments(TAlignModelList& alignments);
    void DropAlignmentInfo(TAlignModelList& alignments, TGeneModelList& models);
    void FilterOutChimeras(TGeneModelList& clust);
    void ScoreCDSes_FilterOutPoorAlignments(TGeneModelList& clust);
    void FilterOutInferiorProtAlignmentsWithIncompatibleFShifts(TGeneModelList& clust);
    void ReplicateFrameShifts(TGeneModelList& models);
    void CutParts(TGeneModelList& models);

    TGeneModelList MakeChains(TGeneModelList& models);

private:
    // Prohibit copy constructor and assignment operator
    CChainer(const CChainer& value);
    CChainer& operator= (const CChainer& value);

    class CChainerImpl;
    auto_ptr<CChainerImpl> m_data;

    friend class CChainerArgUtil;
};

struct MarkupCappedEst : public TransformFunction {
    MarkupCappedEst(const set<string>& _caps, int _capgap);

    const set<string>& caps;
    int capgap;

    virtual void transform_align(CAlignModel& align);
};

struct MarkupTrustedGenes : public TransformFunction {
    MarkupTrustedGenes(const set<string>& _trusted_genes);
    const set<string>& trusted_genes;

    virtual void transform_align(CAlignModel& align);
};

struct ProteinWithBigHole : public Predicate {
    ProteinWithBigHole(double hthresh, double hmaxlen, CGnomonEngine& gnomon);
    double hthresh, hmaxlen;
    CGnomonEngine& gnomon;

    virtual bool model_predicate(CGeneModel& align);
};

struct CdnaWithHole : public Predicate {
    virtual bool model_predicate(CGeneModel& align);
};

struct HasShortIntron : public Predicate {
    HasShortIntron(CGnomonEngine& gnomon);
    CGnomonEngine& gnomon;
    virtual bool model_predicate(CGeneModel& align);
};

struct HasLongIntron : public Predicate {
    HasLongIntron(CGnomonEngine& gnomon);
    CGnomonEngine& gnomon;
    virtual bool model_predicate(CGeneModel& align);
};

struct CutShortPartialExons : public TransformFunction {
    CutShortPartialExons(int minex);
    int minex;

    virtual void transform_align(CAlignModel& align);
};

struct HasNoExons : public Predicate {
    virtual bool model_predicate(CGeneModel& align);
};

struct SingleExon_AllEst : public Predicate {
    virtual bool model_predicate(CGeneModel& align);
};

struct SingleExon_Noncoding : public Predicate {
    virtual bool model_predicate(CGeneModel& align);
};

struct LowSupport_Noncoding : public Predicate {
    LowSupport_Noncoding(int _minsupport);
    int minsupport;
    virtual bool model_predicate(CGeneModel& align);
};

class NCBI_XALGOGNOMON_EXPORT CChainerArgUtil {
public:
    static void SetupArgDescriptions(CArgDescriptions* arg_desc);
    static void ArgsToChainer(CChainer* chainer, const CArgs& args, objects::CScope& scope);
};

END_SCOPE(gnomon)
END_SCOPE(ncbi)


#endif  // ALGO_GNOMON___CHAINER__HPP
