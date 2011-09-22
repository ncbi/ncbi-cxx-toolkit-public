/*
*  $Id$
*
* =========================================================================
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
*  Government do not and cannt warrant the performance or results that
*  may be obtained by using this software or data. The NLM and the U.S.
*  Government disclaim all warranties, express or implied, including
*  warranties of performance, merchantability or fitness for any particular
*  purpose.
*
*  Please cite the author in any work or product based on this material.
*
* =========================================================================
*
*  Author: Vyacheslav Chetvernin
*
* =========================================================================
*/

#include <ncbi_pch.hpp>
#include <algo/align/prosplign/prosplign.hpp>
#include <algo/align/prosplign/prosplign_exception.hpp>

#include "scoring.hpp"
#include "PSeq.hpp"
#include "NSeq.hpp"
#include "nucprot.hpp"
#include "Ali.hpp"
#include "AliSeqAlign.hpp"
#include "Info.hpp"

#include <objects/seqloc/seqloc__.hpp>
#include <objects/seqfeat/seqfeat__.hpp>
#include <objmgr/util/seq_loc_util.hpp>
#include <objmgr/util/sequence.hpp>
#include <objtools/alnmgr/alntext.hpp>
#include <objmgr/seq_vector.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(ncbi::objects);
USING_SCOPE(ncbi::prosplign);

const string CProSplignOptions_Base::default_score_matrix_name = "BLOSUM62";

void CProSplignOptions_Base::SetupArgDescriptions(CArgDescriptions* arg_desc)
{
    if (arg_desc->Exist("score_matrix"))
        return;
    arg_desc->AddDefaultKey
        ("score_matrix",
         "score_matrix",
         "Aminoacid substitution matrix",
         CArgDescriptions::eString,
         CProSplignScoring::default_score_matrix_name);
}

CProSplignOptions_Base::CProSplignOptions_Base()
{
    SetScoreMatrix(default_score_matrix_name);
}

CProSplignOptions_Base::CProSplignOptions_Base(const CArgs& args)
{
    SetScoreMatrix(args["score_matrix"].AsString());
}

CProSplignOptions_Base& CProSplignOptions_Base::SetScoreMatrix(const string& matrix_name)
{
    score_matrix_name = matrix_name;
    return *this;
}
const string& CProSplignOptions_Base::GetScoreMatrix() const
{
    return score_matrix_name;
}

void CProSplignScoring::SetupArgDescriptions(CArgDescriptions* arg_desc)
{
    CProSplignOptions_Base::SetupArgDescriptions(arg_desc);

    arg_desc->AddDefaultKey
        ("min_intron_len",
         "min_intron_len",
         "min_intron_len",
         CArgDescriptions::eInteger,
         NStr::IntToString(CProSplignScoring::default_min_intron_len));
    arg_desc->AddDefaultKey
        ("gap_opening",
         "gap_opening",
         "Gap Opening Cost",
         CArgDescriptions::eInteger,
         NStr::IntToString(CProSplignScoring::default_gap_opening));
    arg_desc->AddDefaultKey
        ("gap_extension",
         "gap_extension",
         "Gap Extension Cost for one aminoacid (three bases)",
         CArgDescriptions::eInteger,
         NStr::IntToString(CProSplignScoring::default_gap_extension));
    arg_desc->AddDefaultKey
        ("frameshift_opening",
         "frameshift_opening",
         "Frameshift Opening Cost",
         CArgDescriptions::eInteger,
         NStr::IntToString(CProSplignScoring::default_frameshift_opening));
    arg_desc->AddDefaultKey
        ("intron_GT",
         "intron_GT",
         "GT/AG intron opening cost",
         CArgDescriptions::eInteger,
         NStr::IntToString(CProSplignScoring::default_intron_GT));
    arg_desc->AddDefaultKey
        ("intron_GC",
         "intron_GC",
         "GC/AG intron opening cost",
         CArgDescriptions::eInteger,
         NStr::IntToString(CProSplignScoring::default_intron_GC));
    arg_desc->AddDefaultKey
        ("intron_AT",
         "intron_AT",
         "AT/AC intron opening cost",
         CArgDescriptions::eInteger,
         NStr::IntToString(CProSplignScoring::default_intron_AT));
    arg_desc->AddDefaultKey
        ("intron_non_consensus",
         "intron_non_consensus",
         "Non Consensus Intron opening Cost",
         CArgDescriptions::eInteger,
         NStr::IntToString(CProSplignScoring::default_intron_non_consensus));
    arg_desc->AddDefaultKey
        ("inverted_intron_extension",
         "inverted_intron_extension",
         "intron_extension cost for 1 base = 1/(inverted_intron_extension*3)",
         CArgDescriptions::eInteger,
         NStr::IntToString(CProSplignScoring::default_inverted_intron_extension));
}
///////////////////////////////////////////////////////////////////////////
CProSplignScoring::CProSplignScoring() : CProSplignOptions_Base()
{
    SetMinIntronLen(default_min_intron_len);
    SetGapOpeningCost(default_gap_opening);
    SetGapExtensionCost(default_gap_extension);
    SetFrameshiftOpeningCost(default_frameshift_opening);
    SetGTIntronCost(default_intron_GT);
    SetGCIntronCost(default_intron_GC);
    SetATIntronCost(default_intron_AT);
    SetNonConsensusIntronCost(default_intron_non_consensus);
    SetInvertedIntronExtensionCost(default_inverted_intron_extension);
}

CProSplignScoring::CProSplignScoring(const CArgs& args) : CProSplignOptions_Base(args)
{
    SetMinIntronLen(args["min_intron_len"].AsInteger());
    SetGapOpeningCost(args["gap_opening"].AsInteger());
    SetGapExtensionCost(args["gap_extension"].AsInteger());
    SetFrameshiftOpeningCost(args["frameshift_opening"].AsInteger());
    SetGTIntronCost(args["intron_GT"].AsInteger());
    SetGCIntronCost(args["intron_GC"].AsInteger());
    SetATIntronCost(args["intron_AT"].AsInteger());
    SetNonConsensusIntronCost(args["intron_non_consensus"].AsInteger());
    SetInvertedIntronExtensionCost(args["inverted_intron_extension"].AsInteger());
}
void CProSplignOutputOptions::SetupArgDescriptions(CArgDescriptions* arg_desc)
{
    CProSplignOptions_Base::SetupArgDescriptions(arg_desc);

    arg_desc->AddFlag("full", "output global alignment as is (all postprocessing options are ingoned)");
    arg_desc->AddDefaultKey
        ("cut_flank_partial_codons",
         "cut_flank_partial_codons",
         "cut partial codons and adjacent mismatches",
         CArgDescriptions::eBoolean,
         CProSplignOutputOptions::default_cut_flank_partial_codons?"true":"false");
    arg_desc->AddDefaultKey
        ("flank_positives",
         "flank_positives",
         "postprocessing: any length flank of a good piece should not be worse than this",
         CArgDescriptions::eInteger,
         NStr::IntToString(CProSplignOutputOptions::default_flank_positives));
    arg_desc->SetConstraint("flank_positives", new CArgAllow_Integers(0, 100));
    arg_desc->AddDefaultKey
        ("total_positives",
         "total_positives",
         "postprocessing: good piece total percentage threshold",
         CArgDescriptions::eInteger,
         NStr::IntToString(CProSplignOutputOptions::default_total_positives));
    arg_desc->SetConstraint("total_positives", new CArgAllow_Integers(0, 100));
    arg_desc->AddDefaultKey
        ("max_bad_len",
         "max_bad_len",
         "postprocessing: any part of a good piece longer than max_bad_len should not be worse than min_positives",
         CArgDescriptions::eInteger,
         NStr::IntToString(CProSplignOutputOptions::default_max_bad_len));
    arg_desc->SetConstraint("max_bad_len", new CArgAllow_Integers(0, 10000));
    arg_desc->AddDefaultKey
        ("min_positives",
         "min_positives",
         "postprocessing: any part of a good piece longer than max_bad_len should not be worse than min_positives",
         CArgDescriptions::eInteger,
         NStr::IntToString(CProSplignOutputOptions::default_min_positives));
    arg_desc->SetConstraint("min_positives", new CArgAllow_Integers(0, 100));

    arg_desc->AddDefaultKey
        ("min_exon_ident",
         "pct",
         "postprocessing: any full or partial exon in the output won't have lower percentage of identity",
         CArgDescriptions::eInteger,
         NStr::IntToString(CProSplignOutputOptions::default_min_exon_id));
    arg_desc->SetConstraint("min_exon_ident", new CArgAllow_Integers(0, 100));
    arg_desc->AddDefaultKey
        ("min_exon_positives",
         "pct",
         "postprocessing: any full or partial exon in the output won't have lower percentage of positives",
         CArgDescriptions::eInteger,
         NStr::IntToString(CProSplignOutputOptions::default_min_exon_pos));
    arg_desc->SetConstraint("min_exon_positives", new CArgAllow_Integers(0, 100));

    arg_desc->AddDefaultKey
        ("min_flanking_exon_len",
         "min_flanking_exon_len",
         "postprocessing: minimum number of bases in the first and last exon",
         CArgDescriptions::eInteger,
         NStr::IntToString(CProSplignOutputOptions::default_min_flanking_exon_len));
    arg_desc->SetConstraint("min_flanking_exon_len", new CArgAllow_Integers(3,10000));
    arg_desc->AddDefaultKey
        ("min_good_len",
         "min_good_len",
         "postprocessing: good piece should not be shorter",
         CArgDescriptions::eInteger,
         NStr::IntToString(CProSplignOutputOptions::default_min_good_len));
    arg_desc->SetConstraint("min_good_len", new CArgAllow_Integers(3,10000));
    arg_desc->AddDefaultKey
        ("start_bonus",
         "start_bonus",
         "postprocessing: reward for start codon match",
         CArgDescriptions::eInteger,
         NStr::IntToString(CProSplignOutputOptions::default_start_bonus));
    arg_desc->SetConstraint("start_bonus", new CArgAllow_Integers(0, 1000));
    arg_desc->AddDefaultKey
        ("stop_bonus",
         "stop_bonus",
         "postprocessing: reward for stop codon at the end",
         CArgDescriptions::eInteger,
         NStr::IntToString(CProSplignOutputOptions::default_stop_bonus));
    arg_desc->SetConstraint("stop_bonus", new CArgAllow_Integers(0, 1000));
}

///////////////////////////////////////////////////////////////////////////
CProSplignOutputOptions::CProSplignOutputOptions(EMode mode) : CProSplignOptions_Base()
{
    switch (mode) {
    case eWithHoles:
        SetCutFlankPartialCodons(default_cut_flank_partial_codons);

        SetFlankPositives(default_flank_positives);
        SetTotalPositives(default_total_positives);
        
        SetMaxBadLen(default_max_bad_len);
        SetMinPositives(default_min_positives);
        
        SetMinExonId(default_min_exon_id);
        SetMinExonPos(default_min_exon_pos);

        SetMinFlankingExonLen(default_min_flanking_exon_len);
        SetMinGoodLen(default_min_good_len);
        
        SetStartBonus(default_start_bonus);
        SetStopBonus(default_stop_bonus);

        break;
    case ePassThrough:
        SetCutFlankPartialCodons(false);

        SetFlankPositives(0);
        SetTotalPositives(0);
        
        SetMaxBadLen(0);
        SetMinPositives(0);
        
        SetMinExonId(0);
        SetMinExonPos(0);

        SetMinFlankingExonLen(0);
        SetMinGoodLen(0);
        
        SetStartBonus(0);
        SetStopBonus(0);
    }
}

CProSplignOutputOptions::CProSplignOutputOptions(const CArgs& args) : CProSplignOptions_Base(args)
{
    if (args["full"]) {
        SetCutFlankPartialCodons(false);

        SetFlankPositives(0);
        SetTotalPositives(0);
        
        SetMaxBadLen(0);
        SetMinPositives(0);
        
        SetMinExonId(0);
        SetMinExonPos(0);

        SetMinFlankingExonLen(0);
        SetMinGoodLen(0);
        
        SetStartBonus(0);
        SetStopBonus(0);
    } else {
        SetCutFlankPartialCodons(args["cut_flank_partial_codons"].AsBoolean());
        SetFlankPositives(args["flank_positives"].AsInteger());
        SetTotalPositives(args["total_positives"].AsInteger());
        SetMaxBadLen(args["max_bad_len"].AsInteger());
        SetMinPositives(args["min_positives"].AsInteger());

        SetMinExonId(args["min_exon_ident"].AsInteger());
        SetMinExonPos(args["min_exon_positives"].AsInteger());

        SetMinFlankingExonLen(args["min_flanking_exon_len"].AsInteger());
        SetMinGoodLen(args["min_good_len"].AsInteger());
        SetStartBonus(args["start_bonus"].AsInteger());
        SetStopBonus(args["stop_bonus"].AsInteger());
    }
}

CProSplignScoring& CProSplignScoring::SetMinIntronLen(int val)
{
    min_intron_len = val;
    return *this;
}
int CProSplignScoring::GetMinIntronLen() const
{
    return min_intron_len;
}
CProSplignScoring& CProSplignScoring::SetGapOpeningCost(int val)
{
    gap_opening = val;
    return *this;
}
int CProSplignScoring::GetGapOpeningCost() const
{
    return gap_opening;
}

CProSplignScoring& CProSplignScoring::SetGapExtensionCost(int val)
{
    gap_extension = val;
    return *this;
}
int CProSplignScoring::GetGapExtensionCost() const
{
    return gap_extension;
}

CProSplignScoring& CProSplignScoring::SetFrameshiftOpeningCost(int val)
{
    frameshift_opening = val;
    return *this;
}
int CProSplignScoring::GetFrameshiftOpeningCost() const
{
    return frameshift_opening;
}

CProSplignScoring& CProSplignScoring::SetGTIntronCost(int val)
{
    intron_GT = val;
    return *this;
}
int CProSplignScoring::GetGTIntronCost() const
{
    return intron_GT;
}
CProSplignScoring& CProSplignScoring::SetGCIntronCost(int val)
{
    intron_GC = val;
    return *this;
}
int CProSplignScoring::GetGCIntronCost() const
{
    return intron_GC;
}
CProSplignScoring& CProSplignScoring::SetATIntronCost(int val)
{
    intron_AT = val;
    return *this;
}
int CProSplignScoring::GetATIntronCost() const
{
    return intron_AT;
}

CProSplignScoring& CProSplignScoring::SetNonConsensusIntronCost(int val)
{
    intron_non_consensus = val;
    return *this;
}
int CProSplignScoring::GetNonConsensusIntronCost() const
{
    return intron_non_consensus;
}

CProSplignScoring& CProSplignScoring::SetInvertedIntronExtensionCost(int val)
{
    inverted_intron_extension = val;
    return *this;
}
int CProSplignScoring::GetInvertedIntronExtensionCost() const
{
    return inverted_intron_extension;
}

bool CProSplignOutputOptions::IsPassThrough() const
{
    return GetTotalPositives() == 0 && GetFlankPositives() == 0;
}

CProSplignOutputOptions& CProSplignOutputOptions::SetCutFlankPartialCodons(bool val)
{
    cut_flank_partial_codons = val;
    return *this;
}
bool CProSplignOutputOptions::GetCutFlankPartialCodons() const
{
    return cut_flank_partial_codons;
}

CProSplignOutputOptions& CProSplignOutputOptions::SetMinExonId(int val)
{
    min_exon_id = val;
    return *this;
}
int CProSplignOutputOptions::GetMinExonId() const
{
    return min_exon_id;
}

CProSplignOutputOptions& CProSplignOutputOptions::SetMinExonPos(int val)
{
    min_exon_pos = val;
    return *this;
}
int CProSplignOutputOptions::GetMinExonPos() const
{
    return min_exon_pos;
}

CProSplignOutputOptions& CProSplignOutputOptions::SetFlankPositives(int val)
{
    flank_positives = val;
    return *this;
}
int CProSplignOutputOptions::GetFlankPositives() const
{
    return flank_positives;
}

CProSplignOutputOptions& CProSplignOutputOptions::SetTotalPositives(int val)
{
    total_positives = val;
    return *this;
}
int CProSplignOutputOptions::GetTotalPositives() const
{
    return total_positives;
}

CProSplignOutputOptions& CProSplignOutputOptions::SetMaxBadLen(int val)
{
    max_bad_len = val;
    return *this;
}
int CProSplignOutputOptions::GetMaxBadLen() const
{
    return max_bad_len;
}

CProSplignOutputOptions& CProSplignOutputOptions::SetMinPositives(int val)
{
    min_positives = val;
    return *this;
}

int CProSplignOutputOptions::GetMinPositives() const
{
    return min_positives;
}

CProSplignOutputOptions& CProSplignOutputOptions::SetMinFlankingExonLen(int val)
{
    min_flanking_exon_len = val;
    return *this;
}

int CProSplignOutputOptions::GetMinFlankingExonLen() const
{
    return min_flanking_exon_len;
}
CProSplignOutputOptions& CProSplignOutputOptions::SetMinGoodLen(int val)
{
    min_good_len = val;
    return *this;
}

int CProSplignOutputOptions::GetMinGoodLen() const
{
    return min_good_len;
}

CProSplignOutputOptions& CProSplignOutputOptions::SetStartBonus(int val)
{
    start_bonus = val;
    return *this;
}

int CProSplignOutputOptions::GetStartBonus() const
{
    return start_bonus;
}
CProSplignOutputOptions& CProSplignOutputOptions::SetStopBonus(int val)
{
    stop_bonus = val;
    return *this;
}

int CProSplignOutputOptions::GetStopBonus() const
{
    return stop_bonus;
}


////////////////////////////////////////////////////////////////////////////////



class CProSplign::CImplementation {
public:
    static CImplementation* create(CProSplignScoring scoring, bool intronless, bool one_stage, bool just_second_stage, bool old);
    CImplementation(CProSplignScoring scoring) :
        m_scoring(scoring), m_matrix(m_scoring.GetScoreMatrix(), m_scoring.sm_koef)
    {
    }
    virtual ~CImplementation() {}
    virtual CImplementation* clone()=0;

    // returns score, bigger is better.
    // if genomic strand is unknown call twice with opposite strands and compare scores
    int FindGlobalAlignment_stage1(CScope& scope, const CSeq_id& protein, const CSeq_loc& genomic);
    CRef<CSeq_align> FindGlobalAlignment_stage2();
    CRef<CSeq_align> FindGlobalAlignment(CScope& scope, const CSeq_id& protein, const CSeq_loc& genomic_orig)
    {
        CSeq_loc genomic;
        genomic.Assign(genomic_orig);
        FindGlobalAlignment_stage1(scope, protein, genomic);
        return FindGlobalAlignment_stage2();
    }

    bool HasStartOnNuc(const CSpliced_seg& sps);
    bool HasStopOnNuc(const CSpliced_seg& sps);
    void SeekStartStop(CSeq_align& seq_align);

    const CProSplignScaledScoring& GetScaleScoring() const 
    {
        return m_scoring;
    }

    virtual const vector<pair<int, int> >& GetExons() const
    {
        NCBI_THROW(CProSplignException, eGenericError, "method relevant only for two stage prosplign");
    }
    virtual vector<pair<int, int> >& SetExons()
    {
        NCBI_THROW(CProSplignException, eGenericError, "method relevant only for two stage prosplign");
    }
    virtual void GetFlanks(bool& lgap, bool& rgap) const
    {
        NCBI_THROW(CProSplignException, eGenericError, "method relevant only for two stage prosplign");
    }
    virtual void SetFlanks(bool lgap, bool rgap)
    {
        NCBI_THROW(CProSplignException, eGenericError, "method relevant only for two stage prosplign");
    }

private:
    virtual int stage1() = 0;
    virtual void stage2(CAli& ali) = 0;

protected:
    CProSplignScaledScoring m_scoring;
    CSubstMatrix m_matrix;

    CScope* m_scope;
    const CSeq_id* m_protein;
    CRef<CSeq_loc> m_genomic;
    auto_ptr<CPSeq> m_protseq;
    auto_ptr<CNSeq> m_cnseq;
};

class COneStage : public CProSplign::CImplementation {
public:
    COneStage(CProSplignScoring scoring) : CProSplign::CImplementation(scoring) {}
    virtual COneStage* clone() { return new COneStage(*this); }

private:
    virtual int stage1();
    virtual void stage2(CAli& ali);

    CTBackAlignInfo<CBMode> m_bi;
};

int COneStage::stage1()
{
    m_bi.Init((int)m_protseq->seq.size(), (int)m_cnseq->size());//backtracking
    return AlignFNog(m_bi, m_protseq->seq, *m_cnseq, m_scoring, m_matrix);
}

void COneStage::stage2(CAli& ali)
{
    BackAlignNog(m_bi, ali);
}

class CTwoStage : public CProSplign::CImplementation {
public:
    CTwoStage(CProSplignScoring scoring, bool just_second_stage) :
        CProSplign::CImplementation(scoring),
        m_just_second_stage(just_second_stage), m_lgap(false), m_rgap(false)  {}

    virtual const vector<pair<int, int> >& GetExons() const
    {
        return m_igi;
    }
    virtual vector<pair<int, int> >& SetExons()
    {
        return m_igi;
    }
    virtual void GetFlanks(bool& lgap, bool& rgap) const
    {
        lgap = m_lgap;
        rgap = m_rgap;
    }
    virtual void SetFlanks(bool lgap, bool rgap)
    {
        m_lgap = lgap;
        m_rgap = rgap;
    }
protected:
    bool m_just_second_stage;
    vector<pair<int, int> > m_igi;
    bool m_lgap;//true if the first one in igi is a gap
    bool m_rgap;//true if the last one in igi is a gap
};

class CTwoStageOld : public CTwoStage {
public:
    CTwoStageOld(CProSplignScoring scoring, bool just_second_stage) : CTwoStage(scoring,just_second_stage) {}
    virtual CTwoStageOld* clone() { return new CTwoStageOld(*this); }
private:
    virtual int stage1();
    virtual void stage2(CAli& ali);
};

class CTwoStageNew : public CTwoStage {
public:
    CTwoStageNew(CProSplignScoring scoring, bool just_second_stage) : CTwoStage(scoring,just_second_stage) {}
    virtual CTwoStageNew* clone() { return new CTwoStageNew(*this); }
private:
    virtual int stage1();
    virtual void stage2(CAli& ali);
};

int CTwoStageOld::stage1()
{
    if (m_just_second_stage)
        return 0;
    int score = FindIGapIntrons(m_igi, m_protseq->seq, *m_cnseq,
                                m_scoring.GetGapOpeningCost(),
                                m_scoring.GetGapExtensionCost(),
                                m_scoring.GetFrameshiftOpeningCost(), m_scoring, m_matrix);
    m_lgap = !m_igi.empty() && m_igi.front().first == 0;
    m_rgap = !m_igi.empty() && m_igi.back().first + m_igi.back().second == int(m_cnseq->size());
    return score;
}

void CTwoStageOld::stage2(CAli& ali)
{
    CNSeq cfrnseq;
    cfrnseq.Init(*m_cnseq, m_igi);
            
    CBackAlignInfo bi;
    bi.Init((int)m_protseq->seq.size(), (int)cfrnseq.size()); //backtracking
        
    FrAlign(bi, m_protseq->seq, cfrnseq,
            m_scoring.GetGapOpeningCost(),
            m_scoring.GetGapExtensionCost(),
            m_scoring.GetFrameshiftOpeningCost(), m_scoring, m_matrix);

    FrBackAlign(bi, ali);
    CAli new_ali(m_igi, m_lgap, m_rgap, ali);
    ali = new_ali;
}

int CTwoStageNew::stage1()
{
    if (m_just_second_stage)
        return 0;
    return FindFGapIntronNog(m_igi, m_protseq->seq, *m_cnseq, m_lgap, m_rgap, m_scoring, m_matrix);
}

void CTwoStageNew::stage2(CAli& ali)
{
    CNSeq cfrnseq;
    cfrnseq.Init(*m_cnseq, m_igi);
            
    CBackAlignInfo bi;
    bi.Init((int)m_protseq->seq.size(), (int)cfrnseq.size()); //backtracking
        
    FrAlignFNog1(bi, m_protseq->seq, cfrnseq, m_scoring, m_matrix, m_lgap, m_rgap);

    FrBackAlign(bi, ali);
    CAli new_ali(m_igi, m_lgap, m_rgap, ali);
    ali = new_ali;
}

class CIntronless : public CProSplign::CImplementation {
public:
    CIntronless(CProSplignScoring scoring) : CProSplign::CImplementation(scoring) {}
private:
    virtual void stage2(CAli& ali);
protected:
    CBackAlignInfo m_bi;
};

class CIntronlessOld : public CIntronless {
public:
    CIntronlessOld(CProSplignScoring scoring) : CIntronless(scoring) {}
    virtual CIntronlessOld* clone() { return new CIntronlessOld(*this); }
private:
    virtual int stage1();
};

class CIntronlessNew : public CIntronless {
public:
    CIntronlessNew(CProSplignScoring scoring) : CIntronless(scoring) {}
    virtual CIntronlessNew* clone() { return new CIntronlessNew(*this); }
private:
    virtual int stage1();
};

int CIntronlessOld::stage1()
{
    m_bi.Init((int)m_protseq->seq.size(), (int)m_cnseq->size());//backtracking
    return FrAlign(m_bi, m_protseq->seq, *m_cnseq,
                   m_scoring.GetGapOpeningCost(),
                   m_scoring.GetGapExtensionCost(),
                   m_scoring.GetFrameshiftOpeningCost(), m_scoring, m_matrix); 
}

int CIntronlessNew::stage1()
{ 
    m_bi.Init((int)m_protseq->seq.size(), (int)m_cnseq->size());//backtracking
    return FrAlignFNog1(m_bi, m_protseq->seq, *m_cnseq, m_scoring, m_matrix);
}

void CIntronless::stage2(CAli& ali)
{
    FrBackAlign(m_bi, ali);
}

CProSplign::CImplementation* CProSplign::CImplementation::create(CProSplignScoring scoring, bool intronless, bool one_stage, bool just_second_stage, bool old)
{
    if (intronless) {
        if (old)
            return new CIntronlessOld(scoring);
        else
            return new CIntronlessNew(scoring);
    } else {
        if (one_stage) {
            return new COneStage(scoring);
        } else {
            if (old)
                return new CTwoStageOld(scoring, just_second_stage);
            else
                return new CTwoStageNew(scoring, just_second_stage);
        }
    }
}


const vector<pair<int, int> >& CProSplign::GetExons() const
{
    return m_implementation->GetExons();
}

vector<pair<int, int> >& CProSplign::SetExons()
{
    return m_implementation->SetExons();
}

void CProSplign::GetFlanks(bool& lgap, bool& rgap) const
{
    m_implementation->GetFlanks(lgap, rgap);
}

void CProSplign::SetFlanks(bool lgap, bool rgap)
{
    m_implementation->SetFlanks(lgap, rgap);
}


CProSplign::CProSplign( CProSplignScoring scoring, bool intronless) :
    m_implementation(CImplementation::create(scoring,intronless,false,false,false))
{
}

CProSplign::CProSplign( CProSplignScoring scoring, bool intronless, bool one_stage, bool just_second_stage, bool old) :
    m_implementation(CImplementation::create(scoring,intronless,one_stage,just_second_stage,old))
{
}

CProSplign::~CProSplign()
{
}

namespace {
/// true if first and last aa are aligned, nothing about inside holes
bool IsProteinSpanWhole(const CSpliced_seg& sps)
{
    CSpliced_seg::TExons exons = sps.GetExons();
    if (exons.empty())
        return false;
    const CProt_pos& prot_start_pos = exons.front()->GetProduct_start().GetProtpos();
    const CProt_pos& prot_stop_pos = exons.back()->GetProduct_end().GetProtpos();
       
    return prot_start_pos.GetAmin()==0 && prot_start_pos.GetFrame()==1 &&
        prot_stop_pos.GetAmin()+1 == sps.GetProduct_length() && prot_stop_pos.GetFrame() == 3;
}
}

CRef<CSeq_align> CProSplign::FindGlobalAlignment(CScope& scope, const CSeq_id& protein, const CSeq_loc& genomic_orig)
{
    CSeq_loc genomic;
    genomic.Assign(genomic_orig);
    const CSeq_id* nucid = genomic.GetId();
    if (nucid == NULL)
        NCBI_THROW(CProSplignException, eGenericError, "genomic seq-loc has multiple ids or no id at all");

    if (genomic.IsWhole()) {
        // change to Interval, because Whole doesn't allow strand change - it's always unknown.
        genomic.SetInt().SetFrom(0);
        genomic.SetInt().SetTo(sequence::GetLength(*nucid, &scope)-1);
    }

    CRef<CSeq_align> result;

    switch (genomic.GetStrand()) {
    case eNa_strand_plus:
    case eNa_strand_minus:
        result = m_implementation->FindGlobalAlignment(scope, protein, genomic);
        break;
    case eNa_strand_unknown:
    case eNa_strand_both:
    case eNa_strand_both_rev:
        // do both
        {
            auto_ptr<CImplementation> plus_data(m_implementation->clone());
            genomic.SetStrand(eNa_strand_plus);
            int plus_score = plus_data->FindGlobalAlignment_stage1(scope, protein, genomic);

            genomic.SetStrand(eNa_strand_minus);
            int minus_score = m_implementation->FindGlobalAlignment_stage1(scope, protein, genomic);
            
            if (minus_score <= plus_score)
                m_implementation = plus_data;
        }

        result = m_implementation->FindGlobalAlignment_stage2();
        break;
    default:
        genomic.SetStrand(eNa_strand_plus);
        result = m_implementation->FindGlobalAlignment(scope, protein, genomic);
        break;
    }

    //remove genomic bounds if set
    if (result->CanGetBounds()) {
        NON_CONST_ITERATE(CSeq_align::TBounds, b, result->SetBounds()) {
            if ((*b)->GetId() != NULL && (*b)->GetId()->Match(*nucid)) {
                result->SetBounds().erase(b);
                break;
            }
        }
    }
    //add genomic_orig as genomic bounds
    CRef<CSeq_loc> genomic_bounds(new CSeq_loc);
    genomic_bounds->Assign(genomic_orig);
    result->SetBounds().push_back(genomic_bounds);

    return result;
}

int CProSplign::CImplementation::FindGlobalAlignment_stage1(CScope& scope, const CSeq_id& protein, const CSeq_loc& genomic)
{
    int gcode = 1;
    try {
        gcode = sequence::GetOrg_ref(sequence::GetBioseqFromSeqLoc(genomic, scope)).GetGcode();
    } catch (...) {}
    m_matrix.SetTranslationTable(new CTranslationTable(gcode));


    /*
    cout<<"gcode: "<<gcode<<endl;
    exit(1);
    */

    m_scope = &scope;
    m_protein = &protein;
    m_genomic.Reset(new CSeq_loc);
    m_genomic->Assign(genomic);
    m_protseq.reset(new CPSeq(*m_scope, *m_protein));
    m_cnseq.reset(new CNSeq(*m_scope, *m_genomic));

    return stage1();
}

CRef<CSeq_align> CProSplign::CImplementation::FindGlobalAlignment_stage2()
{
    CAli ali;
    stage2(ali);

    CAliToSeq_align cpa(ali, *m_scope, *m_protein, *m_genomic);
    CRef<CSeq_align> seq_align = cpa.MakeSeq_align(*m_protseq, *m_cnseq);

    SeekStartStop(*seq_align);

    if (!IsProteinSpanWhole(seq_align->GetSegs().GetSpliced()))
        seq_align->SetType(CSeq_align::eType_disc);

    return seq_align;
}

CRef<objects::CSeq_align> CProSplign::RefineAlignment(CScope& scope, const CSeq_align& seq_align, CProSplignOutputOptions output_options)
{
    CRef<CSeq_align> refined_align(new CSeq_align);
    refined_align->Assign(seq_align);

    if (output_options.IsPassThrough())
        return refined_align;

    CProteinAlignText alignment_text(scope, seq_align, output_options.GetScoreMatrix());
    list<CNPiece> good_parts = FindGoodParts( alignment_text.GetMatch(), alignment_text.GetProtein(), output_options);

    prosplign::RefineAlignment(scope, *refined_align, good_parts/*, output_options.GetCutFlankPartialCodons()*/);

    if (good_parts.size()!=1 || !IsProteinSpanWhole(refined_align->GetSegs().GetSpliced())) {
        refined_align->SetType(CSeq_align::eType_disc);
    }

    prosplign::SeekStartStop(*refined_align, scope);
    prosplign::SetScores(*refined_align, scope, output_options.GetScoreMatrix());

    return refined_align;
}

CRef<objects::CSeq_align> CProSplign::BlastAlignment(CScope& scope, const CSeq_align& seq_align, int score_cutoff, int score_dropoff)
{
    CRef<CSeq_align> refined_align(new CSeq_align);
    refined_align->Assign(seq_align);

    CProteinAlignText alignment_text(scope, seq_align, m_implementation->GetScaleScoring().GetScoreMatrix());
    list<CNPiece> good_parts = BlastGoodParts( alignment_text, m_implementation->GetScaleScoring(), score_cutoff, score_dropoff );

    prosplign::RefineAlignment(scope, *refined_align, good_parts);

    if (good_parts.size()!=1 || !IsProteinSpanWhole(refined_align->GetSegs().GetSpliced())) {
        refined_align->SetType(CSeq_align::eType_disc);
    }

    m_implementation->SeekStartStop(*refined_align);
    prosplign::SetScores(*refined_align, scope, m_implementation->GetScaleScoring().GetScoreMatrix());

    return refined_align;
}


namespace {
bool CProSplign::CImplementation::HasStartOnNuc(const CSpliced_seg& sps)
{
    const CSpliced_exon& exon = *sps.GetExons().front();
    if (exon.GetProduct_start().GetProtpos().GetFrame()!=1)
        return false;
    const CSpliced_exon_chunk& chunk = *exon.GetParts().front();
    if (chunk.IsProduct_ins() || chunk.IsGenomic_ins())
        return false;
    int len = 0;
    if (chunk.IsDiag()) {
        len = chunk.GetDiag();
    } else if (chunk.IsMatch()) {
        len = chunk.GetMatch();
    } else if (chunk.IsMismatch()) {
        len = chunk.GetMismatch();
    }
    if (len < 3)
        return false;

    CSeq_id nucid;
    nucid.Assign(sps.GetGenomic_id());
    CSeq_loc genomic_seqloc(nucid,exon.GetGenomic_start(), exon.GetGenomic_end(),sps.GetGenomic_strand());

    CSeqVector genomic_seqvec(genomic_seqloc, *m_scope, CBioseq_Handle::eCoding_Iupac);
    CSeqVector_CI genomic_ci(genomic_seqvec);

    string buf;
    genomic_ci.GetSeqData(buf, 3);
    if(buf.size() != 3) return false;
    
    return m_matrix.GetTranslationTable().TranslateStartTriplet(buf) == 'M';
}

bool CProSplign::CImplementation::HasStopOnNuc(const CSpliced_seg& sps)
{
    const CSpliced_exon& exon = *sps.GetExons().back();
    if (exon.GetProduct_end().GetProtpos().GetFrame()!=3)
        return false;

    if (sps.GetGenomic_strand()==eNa_strand_minus &&
        exon.GetGenomic_start()<3)
        return false;

    //need to check before because TSeqPos is unsigned
    if(sps.GetGenomic_strand()!=eNa_strand_plus && exon.GetGenomic_start()<3) return false;

    TSeqPos stop_codon_start = sps.GetGenomic_strand()==eNa_strand_plus?exon.GetGenomic_end()+1:exon.GetGenomic_start()-3;
    TSeqPos stop_codon_end = sps.GetGenomic_strand()==eNa_strand_plus?exon.GetGenomic_end()+3:exon.GetGenomic_start()-1;

    CSeq_id nucid;
    nucid.Assign(sps.GetGenomic_id());

    TSeqPos seq_end = sequence::GetLength(nucid, m_scope)-1;
    //if (sps.GetGenomic_strand()==eNa_strand_plus?seq_end<stop_codon_end:stop_codon_start<0) //wrong. stop_codon_start is insigned
    if (sps.GetGenomic_strand()==eNa_strand_plus && seq_end<stop_codon_end)
        return false;

    CSeq_loc genomic_seqloc(nucid,stop_codon_start, stop_codon_end,sps.GetGenomic_strand());

    CSeqVector genomic_seqvec(genomic_seqloc, *m_scope, CBioseq_Handle::eCoding_Iupac);
    CSeqVector_CI genomic_ci(genomic_seqvec);

    string buf;
    genomic_ci.GetSeqData(buf, 3);
    if(buf.size() != 3) return false;    

    return m_matrix.GetTranslationTable().TranslateTriplet(buf) == '*';
    //return buf.size()==3 && (buf=="TAA" || buf=="TGA" || buf=="TAG");
}
}


void CProSplign::CImplementation::SeekStartStop(CSeq_align& seq_align)
{
    CSpliced_seg& sps = seq_align.SetSegs().SetSpliced();

    if (sps.IsSetModifiers()) {
        for (CSpliced_seg::TModifiers::iterator m = sps.SetModifiers().begin(); m != sps.SetModifiers().end(); ) {
            if ((*m)->IsStart_codon_found() || (*m)->IsStop_codon_found())
                m = sps.SetModifiers().erase(m);
            else
                ++m;
        }
        if (sps.GetModifiers().empty())
            sps.ResetModifiers();
    }

    if (!sps.SetExons().empty()) {
        //start, stop
        if(HasStartOnNuc(sps)) {
            CRef<CSpliced_seg_modifier> modi(new CSpliced_seg_modifier);
            modi->SetStart_codon_found(true);
            sps.SetModifiers().push_back(modi);

            CSpliced_exon& exon = *sps.SetExons().front();
            if (exon.GetProduct_start().GetProtpos().GetAmin()==0) {
                CSeq_id protid;
                protid.Assign(sps.GetProduct_id());
                CPSeq pseq(*m_scope,protid);

                CRef<CSpliced_exon_chunk> chunk = exon.SetParts().front();
                _ASSERT( !chunk->IsMatch() || pseq.HasStart() );
                if (pseq.HasStart() && !chunk->IsMatch()) {
                    _ASSERT( chunk->IsDiag() );
                    int len = chunk->GetDiag();
                    _ASSERT( len >= 3 );
                    if (len > 3) {
                        chunk->SetDiag(len-3);
                        chunk.Reset(new CSpliced_exon_chunk);
                        exon.SetParts().push_front(chunk);
                    }
                    chunk->SetMatch(3);
                }
            }
        }
        if(HasStopOnNuc(sps)) {
            CRef<CSpliced_seg_modifier> modi(new CSpliced_seg_modifier);
            modi->SetStop_codon_found(true);
            sps.SetModifiers().push_back(modi);
        }
    }
}


END_NCBI_SCOPE
