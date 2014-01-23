/* $Id$
* ===========================================================================
*
*                            public DOMAIN NOTICE                          
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
* Author:  Yuri Kapustin
*
* File Description:
*   CSplign class implementation
*
*/


#include <ncbi_pch.hpp>

#include "splign_util.hpp"
#include "messages.hpp"

#include <algo/align/util/hit_comparator.hpp>
#include <algo/align/util/compartment_finder.hpp>
#include <algo/align/nw/nw_band_aligner.hpp>
#include <algo/align/nw/nw_spliced_aligner16.hpp>
#include <algo/align/nw/nw_formatter.hpp>
#include <algo/align/nw/align_exception.hpp>
#include <algo/align/splign/splign.hpp>

#include <algo/sequence/orf.hpp>

#include <objmgr/scope.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/util/seq_loc_util.hpp>

#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/seqalign/Spliced_seg.hpp>
#include <objects/seqalign/Spliced_exon.hpp>
#include <objects/seqalign/Spliced_exon_chunk.hpp>
#include <objects/seqalign/Product_pos.hpp>
#include <objects/seqalign/Splice_site.hpp>
#include <objects/seqalign/Score.hpp>
#include <objects/seqalign/Score_set.hpp>
#include <objects/general/Object_id.hpp>

#include <math.h>
#include <algorithm>


BEGIN_NCBI_SCOPE

USING_SCOPE(objects);

namespace {

    // define cut-off strategy at the terminii:
    
    // (1) - pre-processing
    // For non-covered ends longer than kNonCoveredEndThreshold use
    // m_max_genomic_ext. For shorter ends use k * query_len^(1/kPower)
    
    const Uint4  kNonCoveredEndThreshold (55);
    const double kPower (2.5);
    
    // (2) - post-processing
    // term exons shorter than kMinTermExonSize with identity lower than
    // kMinTermExonIdty will be converted to gaps
    const size_t kMinTermExonSize (28);
    const double kMinTermExonIdty (0.9);

    //if flanking exon is closer than kFlankExonProx to the beggining/(end or polyA) of mRNA, it is NOT treated as adjecent to a gap  
    const int kFlankExonProx (20);

    //maximim length to cut to splice (exons near gaps)
    const int kMaxCutToSplice (6);

    const CSplign::THit::TCoord
                 kMaxCoord (numeric_limits<CSplign::THit::TCoord>::max());


    //original Yuri's EST scores

    const int kEstMatchScore (1000);
    const int kEstMismatchScore (-1011);
    const int kEstGapOpeningScore(-1460);
    const int kEstGapExtensionScore(-464);

    const int kEstGtAgSpliceScore(-4988);
    const int kEstGcAgSpliceScore(-5999);
    const int kEstAtAcSpliceScore(-7010);
    const int kEstNonConsensusSpliceScore(-13060);
}



//original Yuri's mRNA scores

CSplign::EScoringType CSplign::s_GetDefaultScoringType(void) {
    return eMrnaScoring;
}

int CSplign::s_GetDefaultMatchScore(void) {
    return 1000;
}

int CSplign::s_GetDefaultMismatchScore(void) {
    return -1044;
}

int CSplign::s_GetDefaultGapOpeningScore(void) {
    return -3070;
}

int CSplign::s_GetDefaultGapExtensionScore(void) {
    return -173;
}

int CSplign::s_GetDefaultGtAgSpliceScore(void) {
    return -4270;
}

int CSplign::s_GetDefaultGcAgSpliceScore(void) {
    return -5314;
}

int CSplign::s_GetDefaultAtAcSpliceScore(void) {
    return -6358;
}

int CSplign::s_GetDefaultNonConsensusSpliceScore(void) {
    return -7395;
}

CSplign::CSplign(void):
    m_CanResetHistory (true),
    //basic scores
    m_ScoringType(s_GetDefaultScoringType()),
    m_MatchScore(s_GetDefaultMatchScore()),
    m_MismatchScore(s_GetDefaultMismatchScore()),
    m_GapOpeningScore(s_GetDefaultGapOpeningScore()),
    m_GapExtensionScore(s_GetDefaultGapExtensionScore()),
    m_GtAgSpliceScore(s_GetDefaultGtAgSpliceScore()),
    m_GcAgSpliceScore(s_GetDefaultGcAgSpliceScore()),
    m_AtAcSpliceScore(s_GetDefaultAtAcSpliceScore()),
    m_NonConsensusSpliceScore(s_GetDefaultNonConsensusSpliceScore()),
    //end of basic scores   
    m_MinExonIdty(s_GetDefaultMinExonIdty()),
    m_MinPolyaExtIdty(s_GetDefaultPolyaExtIdty()),
    m_MinPolyaLen(s_GetDefaultMinPolyaLen()),
    m_CompartmentPenalty(s_GetDefaultCompartmentPenalty()),
    m_MinCompartmentIdty(s_GetDefaultMinCompartmentIdty()),
    m_MinSingletonIdty(s_GetDefaultMinCompartmentIdty()),
    m_MinSingletonIdtyBps(numeric_limits<size_t>::max()),
    m_TestType(kTestType_production_default),
    m_endgaps (true),
    m_strand (true),
    m_nopolya (false),
    m_cds_start (0),
    m_cds_stop (0),
    m_max_genomic_ext (s_GetDefaultMaxGenomicExtent()),
    m_MaxIntron (CCompartmentFinder<THit>::s_GetDefaultMaxIntron()),
    m_model_id (0),
    m_MaxCompsPerQuery (0),
    m_MinPatternHitLength (13)
{
}

CSplign::~CSplign()
{
}

static CVersion* s_CreateVersion(void)
{
    return new CVersion(CVersionInfo(1, 39, 8));
};


CVersion& CSplign::s_GetVersion(void)
{
    static CSafeStatic<CVersion> s_Version(s_CreateVersion, 0);
    return s_Version.Get();
}


CRef<CSplign::TAligner>& CSplign::SetAligner( void ) {
    return m_aligner;
}


CConstRef<CSplign::TAligner> CSplign::GetAligner( void ) const {
    return m_aligner;
}

void CSplign::SetAlignerScores(void) {
    CRef<TAligner>& aligner = SetAligner();
    aligner->SetWm  (GetMatchScore());
    aligner->SetWms (GetMismatchScore());
    aligner->SetWg  (GetGapOpeningScore());
    aligner->SetWs  (GetGapExtensionScore());
    aligner->SetScoreMatrix(NULL);
    aligner->SetWi(0, GetGtAgSpliceScore());
    aligner->SetWi(1, GetGcAgSpliceScore());
    aligner->SetWi(2, GetAtAcSpliceScore());
    aligner->SetWi(3, GetNonConsensusSpliceScore());
}

CRef<CSplicedAligner> CSplign::s_CreateDefaultAligner(void) {
    CRef<CSplicedAligner> aligner(
        static_cast<CSplicedAligner*>(new CSplicedAligner16));
    return aligner;
}


CRef<CSplicedAligner> CSplign::s_CreateDefaultAligner(bool low_query_quality)
{
    CRef<CSplicedAligner> aligner(s_CreateDefaultAligner());
        
    if(low_query_quality) {
        aligner->SetWm  (kEstMatchScore);
        aligner->SetWms (kEstMismatchScore);
        aligner->SetWg  (kEstGapOpeningScore);
        aligner->SetWs  (kEstGapExtensionScore);
        aligner->SetScoreMatrix(NULL);
        aligner->SetWi(0, kEstGtAgSpliceScore);
        aligner->SetWi(1, kEstGcAgSpliceScore);
        aligner->SetWi(2, kEstAtAcSpliceScore);
        aligner->SetWi(3, kEstNonConsensusSpliceScore);
    }
    else {
        aligner->SetWm  (s_GetDefaultMatchScore());
        aligner->SetWms (s_GetDefaultMismatchScore());
        aligner->SetWg  (s_GetDefaultGapOpeningScore());
        aligner->SetWs  (s_GetDefaultGapExtensionScore());
        aligner->SetScoreMatrix(NULL);
        aligner->SetWi(0, s_GetDefaultGtAgSpliceScore());
        aligner->SetWi(1, s_GetDefaultGcAgSpliceScore());
        aligner->SetWi(2, s_GetDefaultAtAcSpliceScore());
        aligner->SetWi(3, s_GetDefaultNonConsensusSpliceScore());
    }

    return aligner;
}

//note: SetScoringType call with mRNA or EST type is going to switch basic scores to preset values

void CSplign::SetScoringType(EScoringType type) 
{
    m_ScoringType = type;
    if(type == eMrnaScoring) {
        SetMatchScore(s_GetDefaultMatchScore());
        SetMismatchScore(s_GetDefaultMismatchScore());
        SetGapOpeningScore(s_GetDefaultGapOpeningScore());
        SetGapExtensionScore(s_GetDefaultGapExtensionScore());
        SetGtAgSpliceScore(s_GetDefaultGtAgSpliceScore());
        SetGcAgSpliceScore(s_GetDefaultGcAgSpliceScore());
        SetAtAcSpliceScore(s_GetDefaultAtAcSpliceScore());
        SetNonConsensusSpliceScore(s_GetDefaultNonConsensusSpliceScore());
    } else if(type == eEstScoring) {
        SetMatchScore(kEstMatchScore);
        SetMismatchScore(kEstMismatchScore);
        SetGapOpeningScore(kEstGapOpeningScore);
        SetGapExtensionScore(kEstGapExtensionScore);
        SetGtAgSpliceScore(kEstGtAgSpliceScore);
        SetGcAgSpliceScore(kEstGcAgSpliceScore);
        SetAtAcSpliceScore(kEstAtAcSpliceScore);
        SetNonConsensusSpliceScore(kEstNonConsensusSpliceScore);
    }
}   
    
CSplign::EScoringType CSplign::GetScoringType(void) const {
    return m_ScoringType;
}

void   CSplign::SetMatchScore(int score) {
    m_MatchScore = score;
}

int CSplign::GetMatchScore(void) const {
    return m_MatchScore;
}

void   CSplign::SetMismatchScore(int score) {
    m_MismatchScore = score;
}

int CSplign::GetMismatchScore(void) const {
    return m_MismatchScore;
}

void   CSplign::SetGapOpeningScore(int score) {
    m_GapOpeningScore = score;
}

int CSplign::GetGapOpeningScore(void) const {
    return m_GapOpeningScore;
}

void   CSplign::SetGapExtensionScore(int score) {
    m_GapExtensionScore = score;
}

int CSplign::GetGapExtensionScore(void) const {
    return m_GapExtensionScore;
}
//     
void   CSplign::SetGtAgSpliceScore(int score) {
    m_GtAgSpliceScore = score;
}

int CSplign::GetGtAgSpliceScore(void) const {
    return m_GtAgSpliceScore;
}

void   CSplign::SetGcAgSpliceScore(int score) {
    m_GcAgSpliceScore = score;
}

int CSplign::GetGcAgSpliceScore(void) const {
    return m_GcAgSpliceScore;
}

void   CSplign::SetAtAcSpliceScore(int score) {
    m_AtAcSpliceScore = score;
}

int CSplign::GetAtAcSpliceScore(void) const {
    return m_AtAcSpliceScore;
}

void   CSplign::SetNonConsensusSpliceScore(int score) {
    m_NonConsensusSpliceScore = score;
}

int CSplign::GetNonConsensusSpliceScore(void) const {
    return m_NonConsensusSpliceScore;
}

void CSplign::SetEndGapDetection( bool on ) {
    m_endgaps = on;
}

bool CSplign::GetEndGapDetection( void ) const {
    return m_endgaps;
}

void CSplign::SetPolyaDetection( bool on ) {
    m_nopolya = !on;
}

bool CSplign::GetPolyaDetection( void ) const {
    return !m_nopolya;
}

void CSplign::SetStrand( bool strand ) {
    m_strand = strand;
}

bool CSplign::GetStrand( void ) const {
    return m_strand;
}

void CSplign::SetMinExonIdentity( double idty )
{
    if(!(0 <= idty && idty <= 1)) {
        NCBI_THROW(CAlgoAlignException, eBadParameter, g_msg_BadIdentityThreshold);
    }
    else {
        m_MinExonIdty = idty;
    }
}

void CSplign::SetPolyaExtIdentity( double idty )
{
    if(!(0 <= idty && idty <= 1)) {
        NCBI_THROW(CAlgoAlignException, eBadParameter, g_msg_BadIdentityThreshold);
    }
    else {
        m_MinPolyaExtIdty = idty;
    }
}

void CSplign::SetMinPolyaLen(size_t len) {
    m_MinPolyaLen = len;
}

void CSplign::SetMinCompartmentIdentity( double idty )
{
    if(!(0 <= idty && idty <= 1)) {
        NCBI_THROW(CAlgoAlignException, eBadParameter, g_msg_BadIdentityThreshold);
    }
    else {
        m_MinCompartmentIdty = idty;
    }
}

void CSplign::SetMinSingletonIdentity(double idty)
{
    if(!(0 <= idty && idty <= 1)) {
        NCBI_THROW(CAlgoAlignException, eBadParameter, g_msg_BadIdentityThreshold);
    }
    else {
        m_MinSingletonIdty = idty;
    }
}

void CSplign::SetMinSingletonIdentityBps(size_t idty_bps)
{
    m_MinSingletonIdtyBps = idty_bps;
}

size_t CSplign::s_GetDefaultMaxGenomicExtent(void)
{
    return 35000;
}


void CSplign::SetMaxGenomicExtent(size_t mge)
{
    m_max_genomic_ext = mge;
}


size_t CSplign::GetMaxGenomicExtent(void) const
{
    return m_max_genomic_ext;
}


void CSplign::SetMaxIntron(size_t max_intron)
{
    m_MaxIntron = max_intron;
}


size_t CSplign::GetMaxIntron(void) const
{
    return m_MaxIntron;
}


double CSplign::GetMinExonIdentity( void ) const {
    return m_MinExonIdty;
}

double CSplign::s_GetDefaultMinExonIdty(void)
{
    return 0.75;
}

double CSplign::GetPolyaExtIdentity( void ) const {
    return m_MinPolyaExtIdty;
}

double CSplign::s_GetDefaultPolyaExtIdty(void)
{
    return 1.;
}

size_t CSplign::GetMinPolyaLen( void ) const {
    return m_MinPolyaLen;
}

size_t CSplign::s_GetDefaultMinPolyaLen(void)
{
    return 1;
}

double CSplign::GetMinCompartmentIdentity(void) const {
    return m_MinCompartmentIdty;
}

double CSplign::GetMinSingletonIdentity(void) const {
    return m_MinSingletonIdty;
}

size_t CSplign::GetMinSingletonIdentityBps(void) const {
    return m_MinSingletonIdtyBps;
}

double CSplign::s_GetDefaultMinCompartmentIdty(void)
{
    return 0.70;
}

void CSplign::SetTestType(const string& test_type) {
    m_TestType = test_type;
}

string CSplign::GetTestType(void) const {
    return m_TestType;
}

void CSplign::SetMaxCompsPerQuery(size_t m) {
    m_MaxCompsPerQuery = m;
}

size_t CSplign::GetMaxCompsPerQuery(void) const {
    return m_MaxCompsPerQuery;
}



CRef<objects::CScope> CSplign::GetScope(void) const
{
    return m_Scope;
}


CRef<objects::CScope>& CSplign::SetScope(void)
{
    return m_Scope;
}


void CSplign::PreserveScope(bool preserve_scope)
{
    m_CanResetHistory = ! preserve_scope;
}


void CSplign::SetCompartmentPenalty(double penalty)
{
    if(penalty < 0 || penalty > 1) {
        NCBI_THROW(CAlgoAlignException, eBadParameter, g_msg_QueryCoverageOutOfRange);
    }
    m_CompartmentPenalty = penalty;
}

double CSplign::s_GetDefaultCompartmentPenalty(void)
{
    return 0.55;
}

double CSplign::GetCompartmentPenalty( void ) const 
{
    return m_CompartmentPenalty;
}

void CSplign::x_LoadSequence(vector<char>* seq, 
                             const objects::CSeq_id& seqid,
                             THit::TCoord start,
                             THit::TCoord finish,
                             bool retain)
{

    try {
    
        if(m_Scope.IsNull()) {
            NCBI_THROW(CAlgoAlignException, eInternal, "Splign scope not set");
        }

        CBioseq_Handle bh (m_Scope->GetBioseqHandle(seqid));

        if(retain && m_CanResetHistory) {
            m_Scope->ResetHistory(); // this does not remove the sequence
                                     // referenced to by 'bh'
        }

        if(bh) {

            CSeqVector sv (bh.GetSeqVector(CBioseq_Handle::eCoding_Iupac));
            const TSeqPos dim (sv.size());
            if(dim == 0) {
                NCBI_THROW(CAlgoAlignException,
                           eNoSeqData, 
                           string("Sequence is empty: ") 
                           + seqid.AsFastaString());
            }
            if(finish >= dim) {
                finish = dim - 1;
            }

            if(start > finish) {
                CNcbiOstrstream ostr;
                ostr << "Invalid sequence interval requested for "
                     << seqid.GetSeqIdString(true) << ":\t"
                     << start << '\t' << finish;
                const string err = CNcbiOstrstreamToString(ostr);
                NCBI_THROW(CAlgoAlignException, eNoSeqData, err);
            }
            
            string s;
            sv.GetSeqData(start, finish + 1, s);
            seq->resize(1 + finish - start);
            copy(s.begin(), s.end(), seq->begin());
        }
        else {
            NCBI_THROW(CAlgoAlignException, eNoSeqData, 
                       string("ID not found: ") + seqid.AsFastaString());
        }
        
        if(!retain && m_CanResetHistory) {
            m_Scope->RemoveFromHistory(bh);
        }       
    }
    
    catch(CAlgoAlignException& e) {
        NCBI_RETHROW_SAME(e, "CSplign::x_LoadSequence(): Sequence data problem");
    }
}


void CSplign::ClearMem(void)
{
    m_Scope.Reset(NULL);
    m_pattern.clear();
    m_alnmap.clear();
    m_genomic.clear();
    m_mrna.clear();
}


CSplign::THitRef CSplign::sx_NewHit(THit::TCoord q0, THit::TCoord q,
                                    THit::TCoord s0, THit::TCoord s)
{
    THitRef hr (new THit);
    hr->SetQueryStart(q0);
    hr->SetSubjStart(s0);
    hr->SetQueryStop(q - 1);
    hr->SetSubjStop(s - 1);
    hr->SetLength(q - q0);
    hr->SetMismatches(0);
    hr->SetGaps(0);
    hr->SetEValue(0);
    hr->SetScore(2*(q - q0));
    hr->SetIdentity(1);
    return hr;
}


void CSplign::x_SplitQualifyingHits(THitRefs* phitrefs)
{
    const char * Seq1 (&m_mrna.front());
    const char * Seq2 (&m_genomic.front());
    THitRefs rv;
    ITERATE(THitRefs, ii, (*phitrefs)) {

        const THitRef & h (*ii);
        const double idty (h->GetIdentity());
        const bool diag (h->GetGaps() == 0 && h->GetQuerySpan() == h->GetSubjSpan());
        if(idty == 1 || idty < .95 || h->GetLength() < 100 || !diag) {
            rv.push_back(h);
        }
        else {

            int q0 (-1), s0 (-1), q1 (h->GetQueryMax());
            int q (h->GetQueryMin()), s (h->GetSubjMin());
            size_t new_hits (0);
            while(q <= q1) {
                if(Seq1[q++] != Seq2[s++]) {
                    if(q0 != -1 && q >= q0 + int(m_MinPatternHitLength)) {
                        THitRef hr (sx_NewHit(q0, q, s0, s));
                        hr->SetQueryId(h->GetQueryId());
                        hr->SetSubjId(h->GetSubjId());
                        rv.push_back(hr);
                        ++new_hits;
                    }
                    q0 = s0 = -1;
                }
                else {
                    if(q0 == -1) {
                        q0 = q;
                        s0 = s;
                    }
                }
            }

            if(q0 != -1 && q >= q0 + int(m_MinPatternHitLength)) {
                THitRef hr (sx_NewHit(q0, q, s0, s));
                hr->SetQueryId(h->GetQueryId());
                hr->SetSubjId(h->GetSubjId());
                rv.push_back(hr);
                ++new_hits;
            }

            if(new_hits == 0) {
                rv.push_back(h);
            }
        }
    }

    *phitrefs = rv;
}


void CSplign::x_SetPattern(THitRefs* phitrefs)
{
    m_alnmap.clear();
    m_pattern.clear();

    // sort the input by min query coordinate
    typedef CHitComparator<THit> THitComparator;
    THitComparator sorter (THitComparator::eQueryMin);
    stable_sort(phitrefs->begin(), phitrefs->end(), sorter);

    // check that no two consecutive hits are farther than the max intron
    // (extra short hits skipped)
    // (throw out a hit if it intersects with previous on subject (genome), )
    size_t prev (0);
    TSeqPos prevSmin, prevSmax;
    NON_CONST_ITERATE(THitRefs, ii, *phitrefs) {

        THitRef& h (*ii);

        if(h->GetQuerySpan() < m_MinPatternHitLength) {
            h.Reset(0);
            continue;
        }

        if(prev > 0) {

        const bool non_intersect = ( prevSmax < h->GetSubjMin() ) || ( prevSmin > h->GetSubjMax() );
        if(!non_intersect) {//throw out intersecting hit
            h.Reset(0);
            continue;
        }

            const bool consistent (h->GetSubjStrand()?
                                  (h->GetSubjStart() < prev + m_MaxIntron):
                                  (h->GetSubjStart() + m_MaxIntron > prev));

            if(!consistent) {
                const string errmsg (g_msg_CompartmentInconsistent
                                     + string(" (extra long introns)"));
                NCBI_THROW(CAlgoAlignException, eIntronTooLong, errmsg);
            }
        }

        prev = h->GetSubjStop();
        prevSmin = h->GetSubjMin();
        prevSmax = h->GetSubjMax();
    }

    phitrefs->erase(remove_if(phitrefs->begin(), phitrefs->end(),
                              CHitFilter<THit>::s_PNullRef),
                    phitrefs->end());

    // save each hit longer than the minimum and test whether the hit is perfect
    vector<size_t> pattern0;
    vector<pair<bool,double> > imperfect;
    double max_idty (0.0);
    for(size_t i (0), n (phitrefs->size()); i < n; ++i) {

        const THitRef & h ((*phitrefs)[i]);
        const bool valid (true);
        if(valid) {

            pattern0.push_back(h->GetQueryMin());
            pattern0.push_back(h->GetQueryMax());
            pattern0.push_back(h->GetSubjMin());
            pattern0.push_back(h->GetSubjMax());
            const double idty (h->GetIdentity());
            const bool imprf  (idty < 1.00 
                               || h->GetQuerySpan() != h->GetSubjSpan()
                               || h->GetMismatches() > 0
                               || h->GetGaps() > 0);
            imperfect.push_back(pair<bool,double>(imprf, idty));
            if(idty > max_idty) {
                max_idty = idty;
            }
        }
    }

    if(max_idty < .85 && pattern0.size() >= 4) {
        m_BoundingRange = pair<size_t, size_t>(pattern0[2], pattern0.back());
    }
    else {
        m_BoundingRange = pair<size_t, size_t>(0, 0);
    }

    const size_t dim (pattern0.size());

    const char* Seq1 (&m_mrna.front());
    const size_t SeqLen1 (m_polya_start < kMax_UInt? m_polya_start: m_mrna.size());
    const char* Seq2 (&m_genomic.front());
    const size_t SeqLen2 (m_genomic.size());
    
    // verify conditions on the input hit pattern
    CNcbiOstrstream ostr_err;
    bool some_error (false), bad_input (false);
    if(dim % 4 == 0) {

        for(size_t i (0); i < dim; i += 4) {

            if(pattern0[i] > pattern0[i+1] || pattern0[i+2] > pattern0[i+3]) {
                ostr_err << "Pattern hits must be specified in plus strand";
                some_error = bad_input = true;
                break;
            }
            
            if(i > 4) {
                if(pattern0[i] <= pattern0[i-3] || pattern0[i+2] <= pattern0[i-1]) {
                    ostr_err << g_msg_CompartmentInconsistent
                             << string(" (hits not sorted)");
                    some_error = true;
                    break;
                }
            }
            
            const bool br1 (pattern0[i+1] >= SeqLen1);
            const bool br2 (pattern0[i+3] >= SeqLen2);
            if(br1 || br2) {

                ostr_err << "Pattern hits out of range ("
                         << "query = " 
                         << phitrefs->front()->GetQueryId()->GetSeqIdString(true)
                         << "subj = " 
                         << phitrefs->front()->GetSubjId()->GetSeqIdString(true)
                         << "):" << endl;

                if(br1) {
                    ostr_err << "\tquery_pattern_max = " << pattern0[i+1]
                             << "; query_len = " << SeqLen1 << endl;
                }

                if(br2) {
                    ostr_err << "\tsubj_pattern_max = " << pattern0[i+3]
                             << "; subj_len = " << SeqLen2 << endl;
                }

                some_error= true;
                break;
            }
        }

    }
    else {
        ostr_err << "Pattern dimension must be a multiple of four";
        some_error = bad_input = true;
    }
    
    if(some_error) {
        ostr_err << " (query = " 
                 << phitrefs->front()->GetQueryId()->AsFastaString() 
                 << " , subj = "
                 << phitrefs->front()->GetSubjId()->AsFastaString() << ')'
                 << endl;
    }

    const string err = CNcbiOstrstreamToString(ostr_err);
    if(err.size() > 0) {
        if(bad_input) {
            NCBI_THROW(CAlgoAlignException, eBadParameter, err);
        }
        else {
            NCBI_THROW(CAlgoAlignException, ePattern, err);
        }
    }

    SAlnMapElem map_elem;
    map_elem.m_box[0] = map_elem.m_box[2] = 0;
    map_elem.m_pattern_start = map_elem.m_pattern_end = -1;

    // build the alignment map
    CBandAligner nwa;
    for(size_t i = 0; i < dim; i += 4) {    
            
        size_t L1, R1, L2, R2;
        size_t max_seg_size (0);

        const bool imprf (imperfect[i/4].first);
        if(imprf) {                

            // TODO:
            // a better approach is to find indels and mismatches
            // and split at the SplitQualifyingHits() stage
            // to pass here only perfect diags
            const size_t len1 (pattern0[i+1] - pattern0[i] + 1);
            const size_t len2 (pattern0[i+3] - pattern0[i+2] + 1);
            const size_t maxlen (max(len1, len2));
            const size_t lendif (len1 < len2? len2 - len1: len1 - len2);
            size_t band (size_t((1 - imperfect[i/4].second) * maxlen) + 2);
            if(band < lendif) band += lendif;
            nwa.SetBand(band);
            nwa.SetSequences(Seq1 + pattern0[i],   len1,
                             Seq2 + pattern0[i+2], len2,
                             false);
            nwa.Run();
            max_seg_size = nwa.GetLongestSeg(&L1, &R1, &L2, &R2);
        }
        else {

            L1 = 1;
            R1 = pattern0[i+1] - pattern0[i] - 1;
            L2 = 1;
            R2 = pattern0[i+3] - pattern0[i+2] - 1;
            max_seg_size = R1 - L1 + 1;
        }

        if(max_seg_size) {

            // make the core
            {{
                size_t cut ((1 + R1 - L1) / 5);
                if(cut > 20) cut = 20;

                const size_t l1 (L1 + cut), l2 (L2 + cut);
                const size_t r1 (R1 - cut), r2 (R2 - cut);
                if(l1 < r1 && l2 < r2) {
                    L1 = l1; L2 = l2; 
                    R1 = r1; R2 = r2;
                }
            }}

            size_t q0 (pattern0[i] + L1);
            size_t s0 (pattern0[i+2] + L2);
            size_t q1 (pattern0[i] + R1);
            size_t s1 (pattern0[i+2] + R2);

            if(imprf) {

                const size_t hitlen_q (pattern0[i + 1] - pattern0[i] + 1);
                const size_t sh (size_t(hitlen_q / 4));
                
                size_t delta (sh > L1? sh - L1: 0);
                q0 += delta;
                s0 += delta;
                    
                const size_t h2s_right (hitlen_q - R1 - 1);
                delta = sh > h2s_right? sh - h2s_right: 0;
                q1 -= delta;
                s1 -= delta;
                
                if(q0 > q1 || s0 > s1) {
                    
                    // the longest segment too short
                    q0 = pattern0[i] + L1;
                    s0 = pattern0[i+2] + L2;
                    q1 = pattern0[i] + R1;
                    s1 = pattern0[i+2] + R2;
                }
            }

            m_pattern.push_back(q0); m_pattern.push_back(q1);
            m_pattern.push_back(s0); m_pattern.push_back(s1);
                
            const size_t pattern_dim = m_pattern.size();
            if(map_elem.m_pattern_start == -1) {
                map_elem.m_pattern_start = pattern_dim - 4;
            }
            map_elem.m_pattern_end = pattern_dim - 1;
        }
            
        map_elem.m_box[1] = pattern0[i+1];
        map_elem.m_box[3] = pattern0[i+3];
    }
        
    map_elem.m_box[1] = SeqLen1 - 1;
    map_elem.m_box[3] = SeqLen2 - 1;
    m_alnmap.push_back(map_elem);
}


CSplign::TOrfPair CSplign::GetCds(const THit::TId& id, const vector<char> * seq_data)
{
    TOrfPair rv (TOrf(0, 0), TOrf(0, 0));

    TStrIdToOrfs::const_iterator ie (m_OrfMap.end());
    const string strid (id->AsFastaString());
    TStrIdToOrfs::const_iterator ii (m_OrfMap.find(strid));

    if(ii != ie) {
        rv = ii->second;
    }
    else {


        vector<char> seq;
        if(seq_data == 0) {
            x_LoadSequence(&seq, *id, 0, kMaxCoord, false);
            seq_data = & seq;
        }

        // Assign CDS to the max ORF longer than 90 bps and starting from ATG
        //
        vector<CRef<CSeq_loc> > orfs;
        vector<string> start_codon;
        start_codon.push_back("ATG");

        COrf::FindOrfs(*seq_data, orfs, 90, 1, start_codon);
        TSeqPos max_len_plus  (0), max_len_minus  (0);
        TSeqPos max_from_plus (0), max_from_minus (0);
        TSeqPos max_to_plus   (0), max_to_minus   (0);
        ITERATE (vector<CRef<CSeq_loc> >, orf, orfs) {

            const TSeqPos len           (sequence::GetLength(**orf, NULL));
            const ENa_strand orf_strand ((*orf)->GetInt().GetStrand());
            const bool antisense        (orf_strand == eNa_strand_minus);
        
            if(antisense) {
                if(len > max_len_minus) {
                    max_len_minus    = len;
                    max_from_minus   = (*orf)->GetInt().GetTo();
                    max_to_minus     = (*orf)->GetInt().GetFrom();
                }
            }
            else {
                if(len > max_len_plus) {
                    max_len_plus    = len;
                    max_from_plus   = (*orf)->GetInt().GetFrom();
                    max_to_plus     = (*orf)->GetInt().GetTo();
                }
            }
        }

        if(max_len_plus > 0) {
            rv.first = TOrf(max_from_plus, max_to_plus);
        }

        if(max_len_minus > 0) {
            rv.second = TOrf(max_from_minus, max_to_minus);
        }

        m_OrfMap[strid] = rv;
    }

    return rv;
}


void CSplign::x_FinalizeAlignedCompartment(SAlignedCompartment & ac)
{
    ac.m_Id         = ++m_model_id;
    ac.m_Segments   = m_segments;
    ac.m_Status     = SAlignedCompartment::eStatus_Ok;
    ac.m_Msg        = "Ok";
    ac.m_Cds_start  = m_cds_start;
    ac.m_Cds_stop   = m_cds_stop;
    ac.m_QueryLen   = m_mrna.size();
    ac.m_PolyA      = (m_polya_start < kMax_UInt? m_polya_start : 0);
}


// PRE:  Input Blast hits.
// POST: TResults - a vector of aligned compartments.
void CSplign::Run(THitRefs* phitrefs)
{
    if(!phitrefs) {
        NCBI_THROW(CAlgoAlignException, eInternal, "Unexpected NULL pointers");
    }

    THitRefs& hitrefs = *phitrefs;
    
    // make sure query hit is in plus strand
    NON_CONST_ITERATE(THitRefs, ii, hitrefs) {

        THitRef& h = *ii;
        if(h.NotNull() && h->GetQueryStrand() == false) {
            h->FlipStrands();
        }
    }
  
    if(m_aligner.IsNull()) {
        NCBI_THROW(CAlgoAlignException, eNotInitialized, g_msg_AlignedNotSpecified);
    }
    
    if(hitrefs.size() == 0) {
        NCBI_THROW(CAlgoAlignException, eNoHits, g_msg_EmptyHitVectorPassed);
    }

    m_result.clear();

    THit::TId id_query (hitrefs.front()->GetQueryId());

    const THit::TCoord mrna_size (objects::sequence::GetLength(*id_query, m_Scope));
    if(mrna_size == kMaxCoord) {
        NCBI_THROW(CAlgoAlignException, eNoSeqData, 
                   string("Sequence not found: ") + id_query->AsFastaString());
    }
    
    // iterate through compartments
    const THit::TCoord min_singleton_idty_final (
                       min(size_t(m_MinSingletonIdty * mrna_size),
                           m_MinSingletonIdtyBps));

    CCompartmentAccessor<THit> comps (THit::TCoord(m_CompartmentPenalty * mrna_size),
                                      THit::TCoord(m_MinCompartmentIdty * mrna_size),
                                      min_singleton_idty_final,
                                      true);
    comps.SetMaxIntron(m_MaxIntron);
    comps.Run(hitrefs.begin(), hitrefs.end());

    pair<size_t,size_t> dim (comps.GetCounts()); // (count_total, count_unmasked)
    if(dim.second > 0) {
 
        // pre-load cDNA
        m_mrna.clear();
        x_LoadSequence(&m_mrna, *id_query, 0, kMaxCoord, false);

        const TOrfPair orfs (GetCds(id_query, & m_mrna));
        if(m_strand) {
            m_cds_start  = orfs.first.first;
            m_cds_stop = orfs.first.second;
        }
        else {
            m_cds_start  = orfs.second.first;
            m_cds_stop = orfs.second.second;
        }

        if(!m_strand) {
            // make a reverse complimentary
            reverse (m_mrna.begin(), m_mrna.end());
            transform(m_mrna.begin(), m_mrna.end(), m_mrna.begin(), SCompliment());
        }

        // compartments share the space between them
        size_t smin (0), smax (kMax_UInt);
        bool same_strand (false);

        const THit::TCoord* box (comps.GetBox(0));
        if(m_MaxCompsPerQuery > 0 && dim.second > m_MaxCompsPerQuery) {
            dim.second = m_MaxCompsPerQuery;
        }
        
        for(size_t i (0); i < dim.first; ++i, box += 4) {
            
            if(i + 1 == dim.first) {
                smax = kMax_UInt;
                same_strand = false;
            }
            else {            
                bool strand_this (comps.GetStrand(i));
                bool strand_next (comps.GetStrand(i+1));
                same_strand = strand_this == strand_next;
                smax = same_strand? (box + 4)[2]: kMax_UInt;
            }
     
            try {

                if(smax < box[3]) {
                    // alert if not ordered by lower subject coordinate
                    NCBI_THROW(CAlgoAlignException, eInternal, 
                               "Unexpected order of compartments");                    
                }

                if(comps.GetStatus(i)) {
                    THitRefs comp_hits;
                    comps.Get(i, comp_hits);

                    if(smax < box[3]) smax = box[3];
                    if(smin > box[2]) smin = box[2];

                    SAlignedCompartment ac (x_RunOnCompartment(&comp_hits, smin,smax));
                    x_FinalizeAlignedCompartment(ac);
                    m_result.push_back(ac);
                }
            }

            catch(CAlgoAlignException& e) {
                
                if(e.GetSeverity() == eDiag_Fatal) {
                    throw;
                }
                
                m_result.push_back(SAlignedCompartment(0, e.GetMsg().c_str()));

                const CException::TErrCode errcode (e.GetErrCode());
                if(errcode != CAlgoAlignException::eNoAlignment) {
                    m_result.back().m_Status = SAlignedCompartment::eStatus_Error;
                }

                ++m_model_id;
            }

            smin = same_strand? box[3]: 0;
        }
    }
}


bool CSplign::AlignSingleCompartment(CRef<objects::CSeq_align> compartment,
                                     SAlignedCompartment* result)
{
    const CRef<CSeq_loc> seqloc (compartment->GetBounds().front());
    const size_t subj_min (seqloc->GetStart(eExtreme_Positional));
    const size_t subj_max (seqloc->GetStop(eExtreme_Positional));

    THitRefs hitrefs;
    ITERATE(CSeq_align_set::Tdata, ii, compartment->GetSegs().GetDisc().Get()) {
    }

    return AlignSingleCompartment(&hitrefs, subj_min, subj_max, result);
}


bool CSplign::AlignSingleCompartment(THitRefs* phitrefs,
                                     size_t subj_min, size_t subj_max,
                                     SAlignedCompartment* result)
{
    m_mrna.resize(0);

    THit::TId id_query (phitrefs->front()->GetQueryId());

    x_LoadSequence(&m_mrna, *id_query, 0, kMaxCoord, false);

    const TOrfPair orfs (GetCds(id_query, & m_mrna));
    if(m_strand) {
        m_cds_start  = orfs.first.first;
        m_cds_stop   = orfs.first.second;
    }
    else {
        m_cds_start  = orfs.second.first;
        m_cds_stop   = orfs.second.second;
    }

    if(!m_strand) {
        reverse (m_mrna.begin(), m_mrna.end());
        transform(m_mrna.begin(), m_mrna.end(), m_mrna.begin(), SCompliment());
    }

    bool rv (true);
    try {

        SAlignedCompartment ac (x_RunOnCompartment(phitrefs, subj_min, subj_max));
        x_FinalizeAlignedCompartment(ac);
        *result = ac;        
        m_mrna.resize(0);
    }

    catch(CAlgoAlignException& e) {

        m_mrna.resize(0);        

        if(e.GetSeverity() == eDiag_Fatal) {
            throw;
        }
        
        *result = SAlignedCompartment(0, e.GetMsg().c_str());

        const CException::TErrCode errcode (e.GetErrCode());
        if(errcode != CAlgoAlignException::eNoAlignment) {
            result->m_Status = SAlignedCompartment::eStatus_Error;
        }

        ++m_model_id;
        rv = false;
    }

    return rv;
}

bool CSplign::IsPolyA(const char * seq, size_t polya_start, size_t dim) {
    const double kMinPercAInPolya (0.80);
    if( polya_start + GetMinPolyaLen() > dim ) return false;
    size_t cnt = 0;
    for(size_t i = polya_start; i<dim; ++i) {
        if(seq[i] == 'A') ++cnt;
    }
    if(cnt >= (dim - polya_start)*kMinPercAInPolya) return true;
    return false;
}

// naive polya detection; sense direction assumed
size_t CSplign::s_TestPolyA(const char * seq, size_t dim, size_t cds_stop)
{
    const size_t kMaxNonA (3), kMinAstreak (5);
    int i (dim - 1), i0 (dim);
    for(size_t count_non_a (0), astreak (0); i >= 0 && count_non_a < kMaxNonA; --i) {

        if(seq[i] != 'A') {
            ++count_non_a;
            astreak = 0;
        }
        else {
            if(++astreak >= kMinAstreak) {
                i0 = i;
            }
        }
    }

    const size_t len (dim - i0);
    size_t rv;
    if(len >= kMinAstreak) {
        rv = i0;
        if(0 < cds_stop && cds_stop < dim && rv <= cds_stop) {
            rv = cds_stop + 1;
        }
    }
    else {
        rv = kMax_UInt;
    }

    return rv;
}


// PRE:  Hits (initial, not transformed) representing the compartment; 
//       maximum genomic sequence span;
//       pre-loaded and appropriately transformed query sequence.
// POST: A set of segments packed into the aligned compartment.

CSplign::SAlignedCompartment CSplign::x_RunOnCompartment(THitRefs* phitrefs, 
                                                         size_t range_left, 
                                                         size_t range_right)
{    
    SAlignedCompartment rv;
 
    try {
        m_segments.clear();
    
        if(range_left > range_right) {
            NCBI_THROW(CAlgoAlignException, eInternal, g_msg_InvalidRange);
        }

        if(phitrefs->size() == 0) {
            NCBI_THROW(CAlgoAlignException, eNoHits, g_msg_NoHitsAfterFiltering);
        }
    
        const size_t mrna_size (m_mrna.size());
    
        if(m_strand == false) {
        
            // adjust the hits
            for(size_t i (0), n (phitrefs->size()); i < n; ++i) {

                THitRef& h ((*phitrefs)[i]);
                THit::TCoord a0 (mrna_size - h->GetQueryMin() - 1);
                THit::TCoord a1 (mrna_size - h->GetQueryMax() - 1);
                const bool new_strand (!(h->GetSubjStrand()));
                h->SetQueryStart(a1);
                h->SetQueryStop(a0);
                h->SetSubjStrand(new_strand);
            }
        }
        
        m_polya_start = m_nopolya?
            kMax_UInt:
            s_TestPolyA(&m_mrna.front(), m_mrna.size(), m_cds_stop);
    
        // cleave off hits beyond polya
        if(m_polya_start < kMax_UInt) {
            CleaveOffByTail(phitrefs, m_polya_start); 
        }

        // keep short terminal hits out of the pattern
        THitRefs::iterator ii (phitrefs->begin()), jj (phitrefs->end() - 1);
        const size_t min_termhitlen1 (m_MinPatternHitLength);
        const size_t min_termhitlen2 (2*m_MinPatternHitLength);
        bool b0 (true), b1 (true);
        while(b0 && b1 && ii < jj) {
            
            while(ii->IsNull() && ii < jj) ++ii;
            while(jj->IsNull() && ii < jj) --jj;
            
            if(ii < jj) {
                
                const double hit_idty ((*ii)->GetIdentity());
                const size_t min_termhitlen (
                   hit_idty < .9999? min_termhitlen2: min_termhitlen1);

                if((*ii)->GetQuerySpan() < min_termhitlen) {
                    ii++ -> Reset(0);
                }
                else {
                    b0 = false;
                }
            }
            
            if(ii < jj) {
                
                const double hit_idty ((*jj)->GetIdentity());
                const size_t min_termhitlen (
                   hit_idty < .9999? min_termhitlen2: min_termhitlen1);

                if((*jj)->GetQuerySpan() < min_termhitlen) {
                    jj-- -> Reset(0);
                }
                else {
                    b1 = false;
                }
            }
        }
        
        phitrefs->erase(remove_if(phitrefs->begin(), phitrefs->end(),
                                  CHitFilter<THit>::s_PNullRef),
                        phitrefs->end());

        if(phitrefs->size() == 0) {
            NCBI_THROW(CAlgoAlignException, eNoHits,
                       g_msg_NoHitsAfterFiltering);
        }


        // find regions of interest on mRna (query) and contig (subj)
        THit::TCoord span [4];
        CHitFilter<THit>::s_GetSpan(*phitrefs, span);
        THit::TCoord qmin (span[0]), qmax (span[1]), smin (span[2]), smax (span[3]);

        const bool ctg_strand (phitrefs->front()->GetSubjStrand());

        // m1: estimate terminal genomic extents based on uncovered end sizes
        const THit::TCoord extent_left_m1 (x_GetGenomicExtent(qmin));
        const THit::TCoord rspace   ((m_polya_start < kMax_UInt?
                                      m_polya_start: mrna_size) - qmax - 1 );
        const THit::TCoord extent_right_m1 (x_GetGenomicExtent(rspace));
        
        // m2: estimate genomic extents using compartment hits
        THit::TCoord fixed_left (kMaxCoord / 2), fixed_right(fixed_left);

        const size_t kTermLenCutOff_m2 (10);
        const bool fix_left  (qmin   <= kTermLenCutOff_m2);
        const bool fix_right (rspace <= kTermLenCutOff_m2);
        if(fix_left || fix_right) {

            if(phitrefs->size() > 1) {

                // select based on the max intron length 
                THit::TCoord max_intron (0);
                THit::TCoord prev_start (phitrefs->front()->GetSubjStart());

                ITERATE(THitRefs, ii, (*phitrefs)) {

                    const THit::TCoord cur_start ((*ii)->GetSubjStart());
                    const THit::TCoord intron    (cur_start >= prev_start?
                                                  cur_start - prev_start:
                                                  prev_start - cur_start);
                    if(intron > max_intron) {
                        max_intron = intron;
                    }
                    prev_start = cur_start;
                }

                const double factor (2.5);
                if(fix_left)  { fixed_left  = THit::TCoord(max_intron * factor); }
                if(fix_right) { fixed_right = THit::TCoord(max_intron * factor); }
            }
            else {
                // stay conservative for single-hit compartments
                const THit::TCoord single_hit_extent (300);
                if(fix_left)  { fixed_left  = single_hit_extent; }
                if(fix_right) { fixed_right = single_hit_extent; }
            }
        }

        const THit::TCoord extent_left_m2  (100 + max(fixed_left,  qmin));
        const THit::TCoord extent_right_m2 (100 + max(fixed_right, rspace));

        const THit::TCoord extent_left (min(extent_left_m1, extent_left_m2));
        THit::TCoord extent_right (min(extent_right_m1, extent_right_m2));

		//add polya length to extent
		THit::TCoord poly_length = m_polya_start < kMax_UInt ? mrna_size - m_polya_start : 0;
		if(extent_right < poly_length) extent_right = poly_length;

        if(ctg_strand) {
            smin = max(0, int(smin - extent_left));
            smax += extent_right;
        }
        else {
            smin = max(0, int(smin - extent_right));
            smax += extent_left;
        }

        // regardless of hits, entire cDNA is aligned (without the tail, if any)
        qmin = 0;
        qmax = m_polya_start < kMax_UInt? m_polya_start - 1: mrna_size - 1;
    
        // make sure to obey the genomic range specified
        if(smin < range_left) {
            smin = range_left;
        }
        if(smax > range_right) {
            smax = range_right;
        }

        m_genomic.clear();
        x_LoadSequence(&m_genomic, *(phitrefs->front()->GetSubjId()), 
                       smin, smax, true);

        // adjust smax if beyond the end
        const THit::TCoord ctg_end (smin + m_genomic.size());
        if(smax >= ctg_end) {
            smax = ctg_end > 0? ctg_end - 1: 0;
        }

        if(ctg_strand == false) {

            // make reverse complementary
            // for the contig's area of interest
            reverse (m_genomic.begin(), m_genomic.end());
            transform(m_genomic.begin(), m_genomic.end(), m_genomic.begin(),
                      SCompliment());
        }

        NON_CONST_ITERATE(THitRefs, ii, *phitrefs) {

            THitRef& h (*ii);

            const THit::TCoord hsmin (h->GetSubjMin());
            const THit::TCoord hsmax (h->GetSubjMax());
            if(!(smin <= hsmin && hsmax <= smax)) {
                CNcbiOstrstream ostr;
                ostr << "Invalid pattern hit:\n" << *h
                     << "\n Interval = (" << smin << ", " << smax << ')';
                const string errmsg = CNcbiOstrstreamToString(ostr);
                NCBI_THROW(CAlgoAlignException, ePattern, errmsg);
            }
            
            if(ctg_strand == false) {

                THit::TCoord a2 (smax - (hsmax - smin));
                THit::TCoord a3 (smax - (hsmin - smin));
                h->SetSubjStart(a2);
                h->SetSubjStop(a3);
            }
        }

        rv.m_QueryStrand = m_strand;
        rv.m_SubjStrand  = ctg_strand;
    
        // shift hits so that they originate from zero
        NON_CONST_ITERATE(THitRefs, ii, *phitrefs) {
            (*ii)->Shift(-(Int4)qmin, -(Int4)smin);
        }

        x_SplitQualifyingHits(phitrefs);
        x_SetPattern(phitrefs);
        rv.m_Score = x_Run(&m_mrna.front(), &m_genomic.front());

        const size_t seg_dim (m_segments.size());
        if(seg_dim == 0) {
            NCBI_THROW(CAlgoAlignException, eNoAlignment, g_msg_NoAlignment);
        }

		//look for the last exon
        int last_exon = -1;
        for(int i = m_segments.size(); i > 0;  ) {
			--i;
            if(m_segments[i].m_exon) {
                last_exon = i;
				break;
            }
        }

        if(last_exon == -1) {//no exons found
            NCBI_THROW(CAlgoAlignException, eNoAlignment,g_msg_NoExonsAboveIdtyLimit);
        }


		// try to extend the last exon as long as it's a good match (min exon identity at the end required)
		TSegment& s (const_cast<TSegment&>(m_segments[last_exon]));

        const char* p0 = &m_mrna.front() + s.m_box[1] + 1;
        const char* q0 = &m_genomic.front() + s.m_box[3] + 1;
        const char* p = p0;
		const char* q = q0;
        const char* pe = &m_mrna.front() + mrna_size;
        const char* qe = &m_genomic.front() + m_genomic.size();

        int match_num = 0;
		size_t sh = 0, ct =0;
		for(; p < pe && q < qe; ++p, ++q, ++ct) {
            if(*p == *q) {
        		++match_num;
		    	if( match_num >= (ct+1)*GetMinExonIdentity() ) { // % ident
	    	    	sh = ct+1;
    			} 
            } 
        }

        // cut low identity flank region in extention
        const double kMinExonFlankIdty (GetPolyaExtIdentity());
        if(sh) {
            p = p0+(sh-1);
            q = q0+(sh-1);
            ct = 1;
            match_num = 0;
            for(;p>=p0;--p,--q,++ct) {
                if(*p == *q) {
            		++match_num;
                } else {
                    if( match_num < ct*kMinExonFlankIdty) {//cut flank
                        sh = p - p0;
                        ct = 1;
                        match_num = 0;
                    }
                }
            }
        }

        if(sh) {
            // resize
            s.m_box[1] += sh;
            s.m_box[3] += sh;
    		for(ct = 0,p = p0, q = q0; ct < sh; ++p, ++q, ++ct) {
    		if(*p == *q) {
    			s.m_details.append(1, 'M');
				} else {
					s.m_details.append(1, 'R');
				}
			}
            s.Update(m_aligner);
            
            // fix annotation
            const size_t ann_dim = s.m_annot.size();
            if(ann_dim > 2 && s.m_annot[ann_dim - 3] == '>') {
                s.m_annot[ann_dim - 2] = q < qe? *q: ' ';
                s.m_annot[ann_dim - 1] = q < (qe-1)? *(q+1): ' ';
            }           
        }
    
        m_segments.resize(last_exon + 1);

        //check if the rest is polya or a gap
        THit::TCoord coord = s.m_box[1] + 1;
        m_polya_start = kMax_UInt;
        if(coord < mrna_size ) {//there is unaligned flanking part of mRNA 
            if(!m_nopolya && IsPolyA(&m_mrna.front(), coord, m_mrna.size())) {//polya
                m_polya_start = coord;
            } else {//gap

                if(GetTestType() == kTestType_20_28_90_cut20) {
                    if(  ( mrna_size - (int)s.m_box[1] - 1 ) >= kFlankExonProx ) {//gap, cut to splice 
                        int seq1_pos = (int)s.m_box[1];
                        int seq2_pos = (int)s.m_box[3];
                        int det_pos = s.m_details.size() - 1;
                        int min_det_pos = det_pos - kMaxCutToSplice;
                        int min_pos = (int)s.m_box[0] + 8;//exon should not be too short
                        while(seq1_pos >= min_pos && det_pos >= min_det_pos) {
                            if( (size_t)(seq2_pos + 2) < m_genomic.size() && s.m_details[det_pos] == 'M' && 
                                   toupper(m_genomic[seq2_pos+1]) == 'G' &&  toupper(m_genomic[seq2_pos+2]) == 'T'  ) {//GT point
                                if( (size_t)det_pos + 1 < s.m_details.size() ) {//resize
                                    s.m_box[1] = seq1_pos;
                                    s.m_box[3] = seq2_pos;
                                    s.m_details.resize(det_pos + 1);
                                    s.Update(m_aligner);
                                    // update the last two annotation symbols
                                    size_t adim = s.m_annot.size();
                                    if(adim > 0 && s.m_annot[adim-1] == '>') {
                                        s.m_annot += "GT";
                                    } else if(adim > 2 && s.m_annot[adim-3] == '>') {
                                        s.m_annot[adim-2] = 'G';
                                        s.m_annot[adim-1] = 'T';
                                    }
                                    coord = seq1_pos+1;
                                }
                                break;
                            }
                            switch(s.m_details[det_pos]) {
                            case 'M' :
                                --seq1_pos;
                                --seq2_pos;
                                break;
                            case 'R' :
                                --seq1_pos;
                                --seq2_pos;
                                break;
                            case 'I' :
                                --seq2_pos;
                                break;
                            case 'D' :
                                --seq1_pos;
                                break;
                            }
                            --det_pos;
                        }
                    }
                }


                TSegment ss;
                ss.m_box[0] = coord;
                ss.m_box[1] = mrna_size - 1;
                ss.SetToGap();
                m_segments.push_back(ss);
            }
        }

            

        // scratch it if the total coverage is too low
        double mcount (0);
        ITERATE(TSegments, jj, m_segments) {
            if(jj->m_exon) {
                mcount += jj->m_idty * jj->m_len;
            }
        }

        const THit::TCoord min_singleton_idty_final (
                 min(size_t(m_MinSingletonIdty * qmax), m_MinSingletonIdtyBps));

        if(mcount < min_singleton_idty_final) {
            NCBI_THROW(CAlgoAlignException, eNoAlignment, g_msg_NoAlignment);
        }

        // convert coordinates back to original
        NON_CONST_ITERATE(TSegments, jj, m_segments) {
        
            if(rv.m_QueryStrand) {
                jj->m_box[0] += qmin;
                jj->m_box[1] += qmin;
            }
            else {
                jj->m_box[0] = mrna_size - jj->m_box[0] - 1;
                jj->m_box[1] = mrna_size - jj->m_box[1] - 1;
            }
        
            if(jj->m_exon) {
                if(rv.m_SubjStrand) {
                    jj->m_box[2] += smin;
                    jj->m_box[3] += smin;
                }
                else {
                    jj->m_box[2] = smax - jj->m_box[2];
                    jj->m_box[3] = smax - jj->m_box[3];
                }
            }
        }

        if(!rv.m_QueryStrand && m_polya_start > 0 && m_polya_start < mrna_size) {
            m_polya_start = mrna_size - m_polya_start - 1;
        }
    } // try

    catch(CAlgoAlignException& e) {
        
        const CException::TErrCode errcode (e.GetErrCode());
        bool severe (true);
        switch(errcode) {
        case CAlgoAlignException::eNoAlignment:
        case CAlgoAlignException::eMemoryLimit:
        case CAlgoAlignException::eNoHits:
        case CAlgoAlignException::eIntronTooLong:
        // case CAlgoAlignException::ePattern:
            severe = false;
            break;
        }

        if(severe) {
            e.SetSeverity(eDiag_Fatal);
        }
        throw;
    }

    return rv;
}

// at this level and below, plus strand is assumed for both sequences
float CSplign::x_Run(const char* Seq1, const char* Seq2)
{
    typedef deque<TSegment> TSegmentDeque;
    TSegmentDeque segments;

//#define DBG_DUMP_PATTERN
#ifdef  DBG_DUMP_PATTERN
    cerr << "Pattern:" << endl;  
#endif

    const size_t map_dim (m_alnmap.size());
    if(map_dim != 1) {
        NCBI_THROW(CAlgoAlignException, eInternal, "Multiple maps not supported");
    }

    float rv (0);
    size_t cds_start (0), cds_stop (0);
    for(size_t i (0); i < map_dim; ++i) {

        const SAlnMapElem& zone (m_alnmap[i]);

        // setup sequences
        const size_t len1 (zone.m_box[1] - zone.m_box[0] + 1);
        const size_t len2 (zone.m_box[3] - zone.m_box[2] + 1);
        
        // remap cds if antisense
        if(m_strand) {
            cds_start = m_cds_start;
            cds_stop  = m_cds_stop;
        }
        else {
            cds_start = len1 - m_cds_start - 1;
            cds_stop  = len1 - m_cds_stop - 1;
        }

        m_aligner->SetSequences(Seq1 + zone.m_box[0], len1,
                                Seq2 + zone.m_box[2], len2,
                                false);

        // prepare the pattern
        vector<size_t> pattern;
        if(m_pattern.size() > 0) {

            if(zone.m_pattern_start < 0) {
                NCBI_THROW(CAlgoAlignException, eInternal,
                           "CSplign::x_Run(): Invalid alignment pattern");
            }
            
            copy(m_pattern.begin() + zone.m_pattern_start,
                 m_pattern.begin() + zone.m_pattern_end + 1,
                 back_inserter(pattern));
        }
        
        for(size_t j (0), pt_dim (pattern.size()); j < pt_dim; j += 4) {

#ifdef  DBG_DUMP_PATTERN
            cerr << (1 + pattern[j]) << '\t' << (1 + pattern[j+1]) << '\t'
                 << "(len = " << (pattern[j+1] - pattern[j] + 1) << ")\t"
                 << (1 + pattern[j+2]) << '\t' << (1 + pattern[j+3]) 
                 << "(len = " << (pattern[j+3] - pattern[j+2] + 1) << ")\t"
                 << endl;
#undef DBG_DUMP_PATTERN
#endif

            pattern[j]   -= zone.m_box[0];
            pattern[j+1] -= zone.m_box[0];
            pattern[j+2] -= zone.m_box[2];
            pattern[j+3] -= zone.m_box[2];
        }

        // setup the aligner
        m_aligner->SetPattern(pattern);
        m_aligner->SetEndSpaceFree(true, true, true, true);
        m_aligner->SetCDS(cds_start, cds_stop);

        rv += m_aligner->Run();
        m_aligner->CheckPreferences();

// #define DBG_DUMP_TYPE2
#ifdef  DBG_DUMP_TYPE2
        {{
            CNWFormatter fmt (*m_aligner);
            string txt;
            fmt.AsText(&txt, CNWFormatter::eFormatType2);
            cerr << txt;
        }}  
#undef DBG_DUMP_TYPE2
#endif

        CNWFormatter formatter (*m_aligner);
        formatter.MakeSegments(&segments);

        // append a gap
        if(i + 1 < map_dim) {
            segments.push_back(TSegment());
            TSegment& g (segments.back());
            g.m_box[0] = zone.m_box[1] + 1;
            g.m_box[1] = m_alnmap[i+1].m_box[0] - 1;
            g.m_box[2] = zone.m_box[3] + 1;
            g.m_box[3] = m_alnmap[i+1].m_box[2] - 1;
            g.SetToGap();
        }
    } // zone iterations end


//#define DUMP_ORIG_SEGS
#ifdef DUMP_ORIG_SEGS
    cerr << "Orig segments:" << endl;
    ITERATE(TSegmentDeque, ii, segments) {
        cerr << ii->m_exon << '\t' << ii->m_idty << '\t' << ii->m_len << '\t'
             << ii->m_box[0] << '\t' << ii->m_box[1] << '\t'
             << ii->m_box[2] << '\t' << ii->m_box[3] << '\t'
             << ii->m_annot  << '\t' << ii->m_score  << endl;
    }
#endif

    if(segments.size() == 0) {
        NCBI_THROW(CAlgoAlignException, eNoAlignment, g_msg_NoAlignment);
    }

    // segment-level postprocessing

    const size_t SeqLen2 (m_genomic.size());
    const size_t SeqLen1 (m_polya_start == kMax_UInt?
                          m_mrna.size():
                          m_polya_start);

    // if the limiting range is set, clear all segments beyond the range
    if(m_BoundingRange.second > 0) {

        NON_CONST_ITERATE(TSegmentDeque, ii, segments) {
            if(ii->m_exon  &&
               (ii->m_box[3] < m_BoundingRange.first
                || ii->m_box[2] > m_BoundingRange.second))
            {
                ii->SetToGap();
            }
        }
    }

    m_segments.resize(0);
 
    string test_type = GetTestType();


    while(true) {

        if(m_segments.size() > 0) {
            segments.resize(m_segments.size());
            copy(m_segments.begin(), m_segments.end(), segments.begin());
            m_segments.resize(0);
        }

        size_t seg_dim (segments.size());
        if(seg_dim == 0) {
            return 0;
        }

        size_t exon_count0 (0);
        ITERATE(TSegmentDeque, ii, segments) {
            if(ii->m_exon) ++exon_count0;
        }


        //extend 100% near gaps and flanks
        if (test_type == kTestType_20_28_90_cut20 )  { // test mode
            bool first_exon = true;
            TSegment *last_exon = NULL;
            for(size_t k0 = 0; k0 < seg_dim; ++k0) {                
                TSegment& s = segments[k0];
                int ext_len = 0;
                int ext_max = 0;
                if(s.m_exon) {
                    if(first_exon) {
                        first_exon = false;
                        ext_len = s.CanExtendLeft(m_mrna, m_genomic);
                        s.ExtendLeft(m_mrna, m_genomic, ext_len, m_aligner);
                    } else if( !segments[k0-1].m_exon ) {//extend near gap
                      //extend previous exon to right
                        ext_len = last_exon->CanExtendRight(m_mrna, m_genomic);
                        //exons should not intersect
                        ext_max = min(s.m_box[0] - last_exon->m_box[1], s.m_box[2] - last_exon->m_box[3]) - 1;
                        last_exon->ExtendRight(m_mrna, m_genomic, min(ext_len, ext_max), m_aligner);
                      //extend current exon to left
                        ext_len = s.CanExtendLeft(m_mrna, m_genomic);
                        ext_max = min(s.m_box[0] - last_exon->m_box[1], s.m_box[2] - last_exon->m_box[3]) - 1;
                        s.ExtendLeft(m_mrna, m_genomic, min(ext_len, ext_max), m_aligner);
                    }
                    last_exon = &s;
                }
            }
            if(last_exon != 0) {
                int ext_len = last_exon->CanExtendRight(m_mrna, m_genomic);
                last_exon->ExtendRight(m_mrna, m_genomic, ext_len, m_aligner);
            }
        }
                        

        //PARTIAL TRIMMING OF TERMINAL EXONS
        if( test_type == kTestType_production_default) {//default production
            // Go from the ends and see if we can improve term exons
            //note that it continue trimming of exons until s.m_idty is high
            size_t k0 (0);
            while(k0 < seg_dim) {

                TSegment& s = segments[k0];
                if(s.m_exon) {
                    
                    const size_t len (1 + s.m_box[1] - s.m_box[0]);
                    const double min_idty (len >= kMinTermExonSize?
                                           m_MinExonIdty:
                                           max(m_MinExonIdty, kMinTermExonIdty));
                    s.ImproveFromLeft(Seq1, Seq2, m_aligner);                
                    if(s.m_idty >= min_idty) {
                        break;
                    }
                }
                ++k0;
            }

            int k1 (seg_dim - 1);
            while(k1 >= int(k0)) {

                TSegment& s (segments[k1]);
                if(s.m_exon) {

                    const size_t len (1 + s.m_box[1] - s.m_box[0]);
                    const double min_idty (len >= kMinTermExonSize?
                                           m_MinExonIdty:
                                           max(m_MinExonIdty, kMinTermExonIdty));
                    s.ImproveFromRight(Seq1, Seq2, m_aligner);                
                     if(s.m_idty >= min_idty) {
                        break;
                    }
                }
                --k1;
            }
        } else if (test_type == kTestType_20_28_90 || test_type == kTestType_20_28_90_cut20 )  { // test mode
            //trim terminal exons only
            bool first_exon = true;
            TSegment *last_exon = NULL;
            NON_CONST_ITERATE(TSegmentDeque, ii, segments) {
                if(ii->m_exon) {
                    //first exon
                    if(first_exon) {
                        if(test_type == kTestType_20_28_90_cut20) {
                            ii->ImproveFromLeft1(Seq1, Seq2, m_aligner);                
                        } else {
                            ii->ImproveFromLeft(Seq1, Seq2, m_aligner);                
                        }
                        first_exon = false;
                    }
                    last_exon = &*ii;//single exon is being trimmed from both sides.            
                }
            }
            //last exon
            if( last_exon != 0 ) {
                if(test_type == kTestType_20_28_90_cut20) {
                    last_exon->ImproveFromRight1(Seq1, Seq2, m_aligner);                
                } else {
                    last_exon->ImproveFromRight(Seq1, Seq2, m_aligner);                
                }
            }

        } else {
              string msg = "test type \"" + test_type + "\" is not supported.";
              NCBI_THROW(CAlgoAlignException, eBadParameter, msg.c_str());
        }

        //partial trimming, exons near <GAP>s, terminal exons are trimmed already
        for(size_t k0 = 0; k0 < seg_dim; ++k0) {
            if(!segments[k0].m_exon) {
                if( k0 > 0 && segments[k0-1].m_exon) {
                    if(test_type == kTestType_20_28_90_cut20) {
                            segments[k0-1].ImproveFromRight1(Seq1, Seq2, m_aligner);                
                    } else {
                            segments[k0-1].ImproveFromRight(Seq1, Seq2, m_aligner);                
                    }
                }
                if( k0 + 1 < seg_dim && segments[k0+1].m_exon) {
                    if(test_type == kTestType_20_28_90_cut20) {
                            segments[k0+1].ImproveFromLeft1(Seq1, Seq2, m_aligner);                
                    } else {
                            segments[k0+1].ImproveFromLeft(Seq1, Seq2, m_aligner);                
                    }
                }
            }
        }

        //CORRECTIONS AFTER PARTIAL TRIMMING
        // indicate any slack space on the left
        if(segments[0].m_box[0] > 0) {
            
            TSegment g;
            g.m_box[0] = 0;
            g.m_box[1] = segments[0].m_box[0] - 1;
            g.m_box[2] = 0;
            g.m_box[3] = segments[0].m_box[2] - 1;
            g.SetToGap();
            segments.push_front(g);
            ++seg_dim;
        }
        
        // same on the right        
        TSegment& seg_last (segments.back());
        
        if(seg_last.m_box[1] + 1 < SeqLen1) {
            
            TSegment g;
            g.m_box[0] = seg_last.m_box[1] + 1;
            g.m_box[1] = SeqLen1 - 1;
            g.m_box[2] = seg_last.m_box[3] + 1;
            g.m_box[3] = SeqLen2 - 1;
            g.SetToGap();
            segments.push_back(g);
            ++seg_dim;
        }


        //WHOLE EXON TRIMMING

        //drop low complexity terminal exons
        {{
            bool first_exon = true;
            TSegment *last_exon = NULL;
            NON_CONST_ITERATE(TSegmentDeque, ii, segments) {
                if(ii->m_exon) {
                    //first exon
                    if(first_exon) {
                        if(ii->IsLowComplexityExon(Seq1) ) {
                            ii->SetToGap();
                        }
                        first_exon = false;
                    } else {//make sure that if exon is single, it is checked once
                        last_exon = &*ii;            
                    }
                }
            }
            //last exon
            if( last_exon != 0 ) {
                if(last_exon->IsLowComplexityExon(Seq1) ) {
                    last_exon->SetToGap();
                }
            } 
        }}

        // turn to gaps exons with low identity
        for(size_t k (0); k < seg_dim; ++k) {
            TSegment& s (segments[k]);
            if(s.m_exon == false) continue;

            bool drop (false);

            if(s.m_idty < m_MinExonIdty) {
                drop = true; // always make gaps on low identity
            } else if (s.m_idty < .9 && s.m_len < 20) {
                // 20_90 rule for short exons preceded/followed by non-consensus splices
                bool nc_prev (false), nc_next (false);
                if(k > 0 && segments[k-1].m_exon) {
                    nc_prev = ! TSegment::s_IsConsensusSplice(
                                    segments[k-1].GetDonor(),
                                    s.GetAcceptor());
                }
                if(k + 1 < seg_dim && segments[k+1].m_exon) {
                    nc_next = ! TSegment::s_IsConsensusSplice(
                                    s.GetDonor(),
                                    segments[k+1].GetAcceptor());
                }
                drop = nc_prev || nc_next;
            }

                
            //20_28_90 TEST MODE
           // turn to gaps exons with combination of shortness and low identity
           if (test_type == kTestType_20_28_90 || test_type == kTestType_20_28_90_cut20 )  { // test mode
                // deal with exons adjacent to gaps
                enum EAdjustExon {
                    eNo,
                    eSoft, //for exons close to mRNA edge
                    eHard
                } adj;
                adj = eNo;

                if(k == 0) {//the first segment is an exon 
                    if( (int)s.m_box[0] >= kFlankExonProx ) {
                        adj = eHard;
                    } else {
                        if(adj == eNo) adj = eSoft;
                    }
                }

                if( k + 1 == seg_dim ) {//the last segment is an exon
                    if( ( (int)SeqLen1 - (int)s.m_box[1] - 1 ) >= kFlankExonProx ) {
                        adj = eHard;
                    } else {
                        if(adj == eNo) adj = eSoft;//prevent switch from Hard to Soft
                    }
                }

                if(k > 0 && ( ! segments[k-1].m_exon ) ) {//gap on left
                    if( (int)s.m_box[0] >= kFlankExonProx ) {
                        adj = eHard;
                    } else {
                        if(adj == eNo) adj = eSoft;
                    }
                }

                if(k + 1 < seg_dim && (! segments[k+1].m_exon ) ) {//gap on right
                    if(  ( (int)SeqLen1 - (int)s.m_box[1] - 1 ) >= kFlankExonProx ) {
                        adj = eHard;
                    } else {
                        if(adj == eNo) adj = eSoft;
                    }
                }
                
                if(adj == eSoft) {//20_90 rule
                    if (s.m_idty < .9 && s.m_len < 20) {
                        drop = true;
                    }
                } else if(adj == eHard) {
                    if( s.m_len < 20 ) {// 20 rule
                        drop = true;
                    }
                    if ( s.m_idty < kMinTermExonIdty && s.m_len < kMinTermExonSize  ) {// 28_90 rule
                        drop = true;
                    }
                }
           }
           if(drop) {
               s.SetToGap();
           }
        }
            

        // turn to gaps short weak terminal exons
        {{
            // find the two leftmost exons
            size_t exon_count (0);
            TSegment* term_segs[] = {0, 0};
            for(size_t i = 0; i < seg_dim; ++i) {
                TSegment& s = segments[i];
                if(s.m_exon) {
                    term_segs[exon_count] = &s;
                    if(++exon_count == 2) {
                        break;
                    }
                }
            }

            if(exon_count == 2) {
                x_ProcessTermSegm(term_segs, 0);
            }
        }}

        {{
            // find the two rightmost exons
            size_t exon_count (0);
            TSegment* term_segs[] = {0, 0};
            for(int i = seg_dim - 1; i >= 0; --i) {
                TSegment& s = segments[i];
                if(s.m_exon) {
                    term_segs[exon_count] = &s;
                    if(++exon_count == 2) {
                        break;
                    }
                }
            }

            if(exon_count == 2) {
                x_ProcessTermSegm(term_segs, 1);
            }
        }}

        // turn to gaps extra-short exons preceded/followed by gaps
          bool gap_prev (false);
          for(size_t k (0); k < seg_dim; ++k) {
              
              TSegment& s (segments[k]);
              if(s.m_exon == false) {
                  gap_prev = true;
              }
              else {
                  size_t length (s.m_box[1] - s.m_box[0] + 1);
                  bool gap_next (false);
                  if(k + 1 < seg_dim) {
                      gap_next = !segments[k+1].m_exon;
                  }
                  if(length <= 10 && (gap_prev || gap_next)) {
                      s.SetToGap();
                  }
                  gap_prev = false;
              }
          }

        // merge all adjacent gaps
        int gap_start_idx (-1);
        if(seg_dim && segments[0].m_exon == false) {
            gap_start_idx = 0;
        }

        for(size_t k (0); k < seg_dim; ++k) {
            TSegment& s (segments[k]);
            if(!s.m_exon) {
                if(gap_start_idx == -1) {
                    gap_start_idx = int(k);
                    if(k > 0) {
                        s.m_box[0] = segments[k-1].m_box[1] + 1;
                        s.m_box[2] = segments[k-1].m_box[3] + 1;
                    }
                }
            }
            else {
                if(gap_start_idx >= 0) {
                    TSegment& g = segments[gap_start_idx];
                    g.m_box[1] = s.m_box[0] - 1;
                    g.m_box[3] = s.m_box[2] - 1;
                    g.m_len = g.m_box[1] - g.m_box[0] + 1;
                    g.m_details.resize(0);
                    m_segments.push_back(g);
                    gap_start_idx = -1;
                }
                m_segments.push_back(s);
            } 
        }

        if(gap_start_idx >= 0) {
            TSegment& g (segments[gap_start_idx]);
            g.m_box[1] = segments[seg_dim-1].m_box[1];
            g.m_box[3] = segments[seg_dim-1].m_box[3];
            g.m_len = g.m_box[1] - g.m_box[0] + 1;
            g.m_details.resize(0);
            m_segments.push_back(g);
        }

        size_t exon_count1 (0);
        ITERATE(TSegments, ii, m_segments) {
            if(ii->m_exon) ++exon_count1;
        }

        if(exon_count0 == exon_count1) break;
    }

    //cut to AG/GT
    {{
      
        if (test_type == kTestType_20_28_90_cut20 )  { // test mode
            bool first_exon = true;
            size_t sdim = m_segments.size();
            int last_exon_index = -1;
            for(size_t k0 = 0; k0 < sdim; ++k0) {                
                if(m_segments[k0].m_exon) {
                    last_exon_index = (int)k0;
                }
            }
            for(size_t k0 = 0; k0 < sdim; ++k0) {                
                TSegment& s = m_segments[k0];
                bool cut_from_left = false;
                bool cut_from_right = false;
                if(s.m_exon) {
                    //check left
                    if(first_exon) {
                        if( (int)s.m_box[0] >= kFlankExonProx ) {//gap on left
                            cut_from_left = true;
                        }
                        first_exon = false;
                    } else if( ! segments[k0-1].m_exon ) {//gap on left
                        cut_from_left = true;
                    }
                    //check right
                    if( last_exon_index == (int)k0 ) {
                        if(  ( (int)SeqLen1 - (int)s.m_box[1] - 1 ) >= kFlankExonProx ) {//gap on right
                            cut_from_right = true;
                        }
                    } else if(k0 + 1 < sdim && (! segments[k0+1].m_exon ) ) {//gap on right
                        cut_from_right = true;
                    }

                    //try to cut from left
                    if(cut_from_left) {
                        int seq1_pos = (int)s.m_box[0];
                        int seq2_pos = (int)s.m_box[2];
                        int det_pos = 0;
                        int max_pos = (int)s.m_box[1] - 8;//exon should not be too short
                        while(seq1_pos <= max_pos && det_pos <= kMaxCutToSplice) {
                            if( seq2_pos > 1 && s.m_details[det_pos] == 'M' && 
                                   toupper(Seq2[seq2_pos-2]) == 'A' &&  toupper(Seq2[seq2_pos-1]) == 'G'  ) {//AG point
                                if(det_pos > 0) {//resize
                                    s.m_box[0] = seq1_pos;
                                    s.m_box[2] = seq2_pos;
                                    s.m_details.erase(0, det_pos);
                                    s.Update(m_aligner);
                                    // update the first two annotation symbols
                                    if(s.m_annot.size() > 0 && s.m_annot[0] == '<') {
                                        s.m_annot = "AG" + s.m_annot;
                                    } else if(s.m_annot.size() > 2 && s.m_annot[2] == '<') {
                                        s.m_annot[0] = 'A';
                                        s.m_annot[1] = 'G';
                                    }
                                }
                                break;
                            }
                            switch(s.m_details[det_pos]) {
                            case 'M' :
                                ++seq1_pos;
                                ++seq2_pos;
                                break;
                            case 'R' :
                                ++seq1_pos;
                                ++seq2_pos;
                                break;
                            case 'I' :
                                ++seq2_pos;
                                break;
                            case 'D' :
                                ++seq1_pos;
                                break;
                            }
                            ++det_pos;
                        }
                    }

                    //try to cut from right
                    if(cut_from_right) {
                        int seq1_pos = (int)s.m_box[1];
                        int seq2_pos = (int)s.m_box[3];
                        int det_pos = s.m_details.size() - 1;
                        int min_det_pos = det_pos - kMaxCutToSplice;
                        int min_pos = (int)s.m_box[0] + 8;//exon should not be too short
                        while(seq1_pos >= min_pos && det_pos >= min_det_pos) {
                            if( (size_t)(seq2_pos + 2) < m_genomic.size() && s.m_details[det_pos] == 'M' && 
                                   toupper(Seq2[seq2_pos+1]) == 'G' &&  toupper(Seq2[seq2_pos+2]) == 'T'  ) {//GT point
                                if( (size_t)det_pos + 1 < s.m_details.size() ) {//resize
                                    s.m_box[1] = seq1_pos;
                                    s.m_box[3] = seq2_pos;
                                    s.m_details.resize(det_pos + 1);
                                    s.Update(m_aligner);
                                    // update the last two annotation symbols
                                    size_t adim = s.m_annot.size();
                                    if(adim > 0 && s.m_annot[adim-1] == '>') {
                                        s.m_annot += "GT";
                                    } else if(adim > 2 && s.m_annot[adim-3] == '>') {
                                        s.m_annot[adim-2] = 'G';
                                        s.m_annot[adim-1] = 'T';
                                    }
                                }
                                break;
                            }
                            switch(s.m_details[det_pos]) {
                            case 'M' :
                                --seq1_pos;
                                --seq2_pos;
                                break;
                            case 'R' :
                                --seq1_pos;
                                --seq2_pos;
                                break;
                            case 'I' :
                                --seq2_pos;
                                break;
                            case 'D' :
                                --seq1_pos;
                                break;
                            }
                            --det_pos;
                        }
                    }
                }
            }
        }
    }}


//#define DUMP_PROCESSED_SEGS
#ifdef DUMP_PROCESSED_SEGS
    cerr << "Processed segments:" << endl;
    ITERATE(TSegments, ii, m_segments) {
        cerr << ii->m_box[0] << '\t' << ii->m_box[1] << '\t'
             << ii->m_box[2] << '\t' << ii->m_box[3] << '\t'
             << ii->m_annot << '\t' << ii->m_score << endl;
    }
#endif

    rv /= m_aligner->GetWm();
    return rv;
}


double CSplign::SAlignedCompartment::GetIdentity() const
{
    string trans;
    for(size_t i (0), dim (m_Segments.size()); i < dim; ++i) {
        const TSegment & s (m_Segments[i]);
        if(s.m_exon) {
            trans.append(s.m_details);
        }
        else {
            trans.append(s.m_len, 'D');
        }
    }
    size_t matches = 0;
    ITERATE(string, ii, trans) {
        if(*ii == 'M') {
            ++matches;
        }
    }
    return double(matches) / trans.size();
}



void CSplign::SAlignedCompartment::GetBox(Uint4* box) const
{
    box[0] = box[2] = kMax_UInt;
    box[1] = box[3] = 0;
    ITERATE(TSegments, ii, m_Segments) {
        const TSegment& s (*ii);
        if(s.m_exon) {
            
            Uint4 a, b;
            if(s.m_box[0] <= s.m_box[1]) {
                a = s.m_box[0];
                b = s.m_box[1];
            }
            else {
                b = s.m_box[0];
                a = s.m_box[1];
            }
            if(a < box[0]) {
                box[0] = a;
            }
            if(b > box[1]) {
                box[1] = b;
            }

            if(s.m_box[2] <= s.m_box[3]) {
                a = s.m_box[2];
                b = s.m_box[3];
            }
            else {
                b = s.m_box[2];
                a = s.m_box[3];
            }
            if(a < box[2]) {
                box[2] = a;
            }
            if(b > box[3]) {
                box[3] = b;
            }
        }
    }
}


bool CSplign::x_ProcessTermSegm(TSegment** term_segs, Uint1 side) const
{            
    bool turn2gap (false);

    const size_t exon_size (1 + term_segs[0]->m_box[1] -
                            term_segs[0]->m_box[0]);

    const double idty (term_segs[0]->m_idty);


    if( GetTestType() == kTestType_production_default ) {//default production
        if(exon_size < kMinTermExonSize && idty < kMinTermExonIdty ) {
            turn2gap = true;
        }
    }

    if(exon_size < kMinTermExonSize) {     
            
            // verify that the intron is not too long

            size_t a, b;
            const char *dnr, *acc;
            if(side == 0) {
                a = term_segs[0]->m_box[3];
                b = term_segs[1]->m_box[2];
                dnr = term_segs[0]->GetDonor();
                acc = term_segs[1]->GetAcceptor();
            }
            else {
                a = term_segs[1]->m_box[3];
                b = term_segs[0]->m_box[2];
                dnr = term_segs[1]->GetDonor();
                acc = term_segs[0]->GetAcceptor();
            }

            const size_t intron_len (b - a);

            const bool consensus (TSegment::s_IsConsensusSplice(dnr, acc));

            size_t max_ext ((idty < .96 || !consensus || exon_size < 16)? 
                            m_max_genomic_ext: (5000 *  kMinTermExonSize));

            if(consensus) {
                if(exon_size < 8) {
                    max_ext = 10 * exon_size;
                }
            }
            else if(exon_size < 16) {
                max_ext = 1;
            }

            const size_t max_intron_len (x_GetGenomicExtent(exon_size, max_ext));
            if(intron_len > max_intron_len) {
                turn2gap = true;
            }
    }
      
    if(turn2gap) {
        
        // turn the segment into a gap  
        TSegment& s = *(term_segs[0]);
        s.SetToGap();
        s.m_len = exon_size;
    }

    return turn2gap;
}


Uint4 CSplign::x_GetGenomicExtent(const Uint4 query_len, Uint4 max_ext) const 
{
    if(max_ext == 0) {
        max_ext = m_max_genomic_ext;
    }

    Uint4 rv (0);
    if(query_len >= kNonCoveredEndThreshold) {
        rv = m_max_genomic_ext;
    }
    else {
        const double k (pow(kNonCoveredEndThreshold, - 1. / kPower) * max_ext);
        const double drv (k * pow(query_len, 1. / kPower));
        rv = Uint4(drv);
    }

    return rv;
}


////////////////////////////////////

namespace splign_local {

    template<typename T>
    void ElemToBuffer(const T& n, char*& p)
    {
        *(reinterpret_cast<T*>(p)) = n;
        p += sizeof(n);
    }
    
    template<>
    void ElemToBuffer(const string& s, char*& p)
    {
        copy(s.begin(), s.end(), p);
        p += s.size();
        *p++ = 0;
    }
    
    template<typename T>
    void ElemFromBuffer(T& n, const char*& p)
    {
        n = *(reinterpret_cast<const T*>(p));
        p += sizeof(n);
    }
    
    template<>
    void ElemFromBuffer(string& s, const char*& p)
    {
        s = p;
        p += s.size() + 1;
    }
}


void CNWFormatter::SSegment::ToBuffer(TNetCacheBuffer* target) const
{
    using namespace splign_local;

    if(target == 0) {
        NCBI_THROW(CAlgoAlignException, eBadParameter, g_msg_NullPointerPassed );
    }
    
    const size_t total_size = sizeof m_exon + sizeof m_idty + 
        sizeof m_len + sizeof m_box + m_annot.size() + 1 +
        m_details.size() + 1 + sizeof m_score;

    target->resize(total_size);
    
    char* p = &target->front();
    ElemToBuffer(m_exon, p);
    ElemToBuffer(m_idty, p);
    ElemToBuffer(m_len, p);
    for(size_t i = 0; i < 4; ++i) {
        ElemToBuffer(m_box[i], p);
    }
    ElemToBuffer(m_annot, p);
    ElemToBuffer(m_details, p);
    ElemToBuffer(m_score, p);
}


void CNWFormatter::SSegment::FromBuffer(const TNetCacheBuffer& source)
{
    using namespace splign_local;

    const size_t min_size = sizeof m_exon + sizeof m_idty + sizeof m_len + 
        + sizeof m_box + 1 + 1 + sizeof m_score;

    if(source.size() < min_size) {
        NCBI_THROW(CAlgoAlignException, eInternal, g_msg_NetCacheBufferIncomplete);
    }
    
    const char* p = &source.front();
    ElemFromBuffer(m_exon, p);
    ElemFromBuffer(m_idty, p);
    ElemFromBuffer(m_len, p);

    for(size_t i = 0; i < 4; ++i) {
        ElemFromBuffer(m_box[i], p);
    }
    
    ElemFromBuffer(m_annot, p);
    ElemFromBuffer(m_details, p);
    ElemFromBuffer(m_score, p);
}


void CSplign::SAlignedCompartment::ToBuffer(TNetCacheBuffer* target) const
{
    using namespace splign_local;

    if(target == 0) {
        NCBI_THROW(CAlgoAlignException, eBadParameter, g_msg_NullPointerPassed);
    }

    const size_t core_size (
        sizeof m_Id + sizeof m_Status + m_Msg.size() + 1
        + sizeof m_QueryStrand + sizeof m_SubjStrand
        + sizeof m_Cds_start + sizeof m_Cds_stop
        + sizeof m_QueryLen
        + sizeof m_PolyA
        + sizeof m_Score);

    vector<char> core (core_size);

    char* p = &core.front();
    ElemToBuffer(m_Id, p);
    ElemToBuffer(m_Status, p);
    ElemToBuffer(m_Msg, p);
    ElemToBuffer(m_QueryStrand, p);
    ElemToBuffer(m_SubjStrand, p);
    ElemToBuffer(m_Cds_start, p);
    ElemToBuffer(m_Cds_stop, p);
    ElemToBuffer(m_QueryLen, p);
    ElemToBuffer(m_PolyA, p);
    ElemToBuffer(m_Score, p);
    
    typedef vector<TNetCacheBuffer> TBuffers;
    TBuffers vb (m_Segments.size());
    size_t ibuf (0);
    ITERATE(TSegments, ii, m_Segments) {
        ii->ToBuffer(&vb[ibuf++]);
    }

    size_t total_size (core_size + sizeof(size_t) * m_Segments.size());
    ITERATE(TBuffers, ii, vb) {
        total_size += ii->size();
    }

    target->resize(total_size);
    TNetCacheBuffer::iterator it = target->begin();
    copy(core.begin(), core.end(), it);
    it += core_size;

    ITERATE(TBuffers, ii, vb) {
        char* p = &(*it);
        const size_t seg_buf_size = ii->size();
        *((size_t*)p) = seg_buf_size;
        it += sizeof (size_t);
        copy(ii->begin(), ii->end(), it);
        it += seg_buf_size;
    }
}


void CSplign::SAlignedCompartment::FromBuffer(const TNetCacheBuffer& source)
{
    using namespace splign_local;

    const size_t min_size (
          sizeof m_Id 
          + sizeof m_Status
          + 1 
          + sizeof m_QueryStrand + sizeof m_SubjStrand
          + sizeof m_Cds_start + sizeof m_Cds_stop
          + sizeof m_QueryLen
          + sizeof m_PolyA
          + sizeof m_Score );

    if(source.size() < min_size) {
        NCBI_THROW(CAlgoAlignException, eInternal, g_msg_NetCacheBufferIncomplete);
    }
    
    const char* p (&source.front());
    ElemFromBuffer(m_Id, p);
    ElemFromBuffer(m_Status, p);
    ElemFromBuffer(m_Msg, p);
    ElemFromBuffer(m_QueryStrand, p);
    ElemFromBuffer(m_SubjStrand, p);
    ElemFromBuffer(m_Cds_start, p);
    ElemFromBuffer(m_Cds_stop, p);
    ElemFromBuffer(m_QueryLen, p);
    ElemFromBuffer(m_PolyA, p);
    ElemFromBuffer(m_Score, p);

    const char* pe (&source.back());
    while(p <= pe) {
        size_t seg_buf_size (0);
        ElemFromBuffer(seg_buf_size, p);
        m_Segments.push_back(TSegment());
        TSegment& seg (m_Segments.back());
        seg.FromBuffer(TNetCacheBuffer(p, p + seg_buf_size));
        p += seg_buf_size;
    }
}

string GetDonor(const objects::CSpliced_exon& exon) {
    if( exon.CanGetDonor_after_exon() && exon.GetDonor_after_exon().CanGetBases() ) {
        return exon.GetDonor_after_exon().GetBases();
    }
    return string();
}    

string GetAcceptor(const objects::CSpliced_exon& exon) {
    if( exon.CanGetAcceptor_before_exon() && exon.GetAcceptor_before_exon().CanGetBases()  ) {
        return exon.GetAcceptor_before_exon().GetBases();
    }
    return string();
}

//returns true for GT/AG, GC/AG AND AT/AC        
bool IsConsSplice(const string& donor, const string acc) {
    if(donor.length()<2 || acc.length()<2) return false;
    if(toupper(acc.c_str()[0]) != 'A') return false;
    switch(toupper(acc.c_str()[1])) {
    case 'C':
        if( toupper(donor.c_str()[0] == 'A') && toupper(donor.c_str()[1] == 'T') ) return true;
        else return false;
        break;
    case 'G':
        if( toupper(donor.c_str()[0] == 'G') ) {
            char don2 = toupper(donor.c_str()[1]);
            if(don2 == 'T' || don2 == 'C') return true;
        }
        return false;
        break;
    default:
        return false;
        break;
    }
    return false;
}
           
    
size_t CSplign::s_ComputeStats(CRef<objects::CSeq_align_set> sas,
                               TScoreSets * output_stats,
                               TOrf cds,
                               EStatFlags flags)
{

    const
    bool valid_input (sas.GetPointer() && sas->CanGet() && sas->Get().size() 
                      && sas->Get().front()->CanGetSegs()
                      && sas->Get().front()->GetSegs().IsSpliced()
                      && sas->Get().front()->GetSegs().GetSpliced().GetProduct_type() 
                      == CSpliced_seg::eProduct_type_transcript
                      && output_stats);

    if(!valid_input) {
        NCBI_THROW(CAlgoAlignException, eBadParameter,
                   "CSplign::s_ComputeStats(): Invalid input");
    }

    output_stats->resize(0);
    
    ITERATE(CSeq_align_set::Tdata, ii1, sas->Get()) {
        CRef<CScore_set> ss (s_ComputeStats(*ii1, false, cds, flags));
        output_stats->push_back(ss);
    }

    return output_stats->size();
}


namespace {
    const int kFrame_not_set (-10);
    const int kFrame_end     (-5);
    const int kFrame_lost    (-20);
}


CRef<objects::CScore_set> CSplign::s_ComputeStats(CRef<objects::CSeq_align> sa,
                                                  bool embed_scoreset,
                                                  TOrf cds,
                                                  EStatFlags flags)
{


    if(!(flags & (eSF_BasicNonCds | eSF_BasicCds))) {
        NCBI_THROW(CException, eUnknown,
                   "CSplign::s_ComputeStats(): mode not yet supported.");
    }

    typedef CSeq_align::TSegs::TSpliced TSpliced;
    const TSpliced & spliced (sa->GetSegs().GetSpliced());
    if(spliced.GetProduct_type() != CSpliced_seg::eProduct_type_transcript) {
        NCBI_THROW(CAlgoAlignException, eBadParameter,
                   "CSplign::s_ComputeStats(): Unsupported product type");
    }

    const bool cds_stats ((flags & eSF_BasicCds) && (cds.first + cds.second > 0));
    const bool qstrand (spliced.GetProduct_strand() != eNa_strand_minus);
    if(cds_stats) {
        const bool cds_strand (cds.first < cds.second);
        if(qstrand ^ cds_strand) {
            NCBI_THROW(CAlgoAlignException, eBadParameter,
                       "CSplign::s_ComputeStats(): Transcript orientation not "
                       "matching specified CDS orientation.");
        }
    }

    typedef TSpliced::TExons TExons;
    const TExons & exons (spliced.GetExons());

    size_t matches (0),
        aligned_query_bases (0),  // matches, mismatches and indels
        aln_length_exons (0),
        aln_length_gaps (0),
        splices_total (0),        // twice the number of introns
        splices_consensus (0);

    const TSeqPos  qlen (spliced.GetProduct_length());
    const TSeqPos polya (spliced.CanGetPoly_a()?
                         spliced.GetPoly_a(): (qstrand? qlen: TSeqPos(-1)));
    const TSeqPos prod_length_no_polya (qstrand? polya: qlen - 1 - polya);
        
    typedef CSpliced_exon TExon;
    TSeqPos qprev (qstrand? TSeqPos(-1): qlen);
    string donor;
    string xcript;
    ITERATE(TExons, ii2, exons) {

        const TExon & exon (**ii2);
        const TSeqPos qmin (exon.GetProduct_start().GetNucpos()),
            qmax (exon.GetProduct_end().GetNucpos());

        const TSeqPos qgap (qstrand? qmin - qprev - 1: qprev - qmax - 1);

        if(qgap > 0) {
            aln_length_gaps += qgap;
            donor.clear();
            if(cds_stats) xcript.append(qgap, 'X');
        }
        else if (ii2 != exons.begin()) {
            splices_total += 2;
            if(IsConsSplice(donor, GetAcceptor(exon))) { splices_consensus += 2; }
        }

        typedef TExon::TParts TParts;
        const TParts & parts (exon.GetParts());
        string errmsg;
        ITERATE(TParts, ii3, parts) {
            const CSpliced_exon_chunk & part (**ii3);
            const CSpliced_exon_chunk::E_Choice choice (part.Which());
            TSeqPos len (0);
            switch(choice) {
            case CSpliced_exon_chunk::e_Match:
                len = part.GetMatch();
                matches += len;
                aligned_query_bases += len;
                if(cds_stats) xcript.append(len, 'M');
                break;
            case CSpliced_exon_chunk::e_Mismatch:
                len = part.GetMismatch();
                aligned_query_bases += len;
                if(cds_stats) xcript.append(len, 'R');
                break;
            case CSpliced_exon_chunk::e_Product_ins:
                len = part.GetProduct_ins();
                aligned_query_bases += len;
                if(cds_stats) xcript.append(len, 'D');
                break;
            case CSpliced_exon_chunk::e_Genomic_ins:
                len = part.GetGenomic_ins();
                if(cds_stats) xcript.append(len, 'I');
                break;
            default:
                errmsg = "Unexpected spliced exon chunk part: "
                    + part.SelectionName(choice);
                NCBI_THROW(CAlgoAlignException, eBadParameter, errmsg);
            }
            aln_length_exons += len;
        }

        donor = GetDonor(exon);
        qprev = qstrand? qmax: qmin;
    } // TExons

    const TSeqPos qgap (qstrand? polya - qprev - 1: qprev - polya - 1);
    aln_length_gaps += qgap;
    if(cds_stats) xcript.append(qgap, 'X');

    // set individual scores
    CRef<CScore_set> ss (new CScore_set);
    CScore_set::Tdata & scores (ss->Set());
        
    if(flags & eSF_BasicNonCds) {
        {
            CRef<CScore> score_matches (new CScore());
            score_matches->SetId().SetId(eCS_Matches);
            score_matches->SetValue().SetInt(matches);
            scores.push_back(score_matches);
        }
        {
            CRef<CScore> score_overall_identity (new CScore());
            score_overall_identity->SetId().SetId(eCS_OverallIdentity);
            score_overall_identity->SetValue().
                SetReal(double(matches)/(aln_length_exons + aln_length_gaps));
            scores.push_back(score_overall_identity);
        }
        {
            CRef<CScore> score_splices (new CScore());
            score_splices->SetId().SetId(eCS_Splices);
            score_splices->SetValue().SetInt(splices_total);
            scores.push_back(score_splices);
        }
        {
            CRef<CScore> score_splices_consensus (new CScore());
            score_splices_consensus->SetId().SetId(eCS_ConsensusSplices);
            score_splices_consensus->SetValue().SetInt(splices_consensus);
            scores.push_back(score_splices_consensus);
        }
        {
            CRef<CScore> score_coverage (new CScore());
            score_coverage->SetId().SetId(eCS_ProductCoverage);
            score_coverage->SetValue().
                SetReal(double(aligned_query_bases) / prod_length_no_polya);
            scores.push_back(score_coverage);
        }
        {
            CRef<CScore> score_exon_identity (new CScore());
            score_exon_identity->SetId().SetId(eCS_ExonIdentity);
            score_exon_identity->SetValue().
                SetReal(double(matches) / aln_length_exons);
            scores.push_back(score_exon_identity);
        }
    }

    if(cds_stats) {

        if(!qstrand && qlen <= 0) {
            NCBI_THROW(CAlgoAlignException, eBadParameter,
                       "CSplign::s_ComputeStats(): Cannot compute "
                       "inframe stats - transcript length not set.");
        }

        int qpos (qstrand? -1: int(qlen));
        int qinc (qstrand? +1: -1);
        int frame (kFrame_not_set);
        size_t aln_length_cds (0);
        size_t matches_frame[] = {0, 0, 0, 0, 0};
        const int cds_start (cds.first), cds_stop (cds.second);
        for(string::const_iterator ie (xcript.end()), ii(xcript.begin());
            ii != ie && frame != kFrame_end; ++ii)
        {

            switch(*ii) {

            case 'M':
                qpos += qinc;
                if(frame == kFrame_not_set && qpos == cds_start) frame = 0;
                if(qpos == cds_stop) frame = kFrame_end;
                if(frame >= -2) {
                    ++aln_length_cds;
                    ++matches_frame[frame + 2];
                }
                break;

            case 'R':
                qpos += qinc;
                if(frame == kFrame_not_set && qpos == cds_start) frame = 0;
                if(qpos == cds_stop) frame = kFrame_end;
                if(frame >= -2) ++aln_length_cds;
                break;

            case 'D':
                qpos += qinc;
                if(frame == kFrame_not_set && qpos == cds_start) frame = 0;
                if(qpos == cds_stop) frame = kFrame_end;
                if(frame >= -2) {
                    ++aln_length_cds;
                    frame = (frame + 1) % 3;
                }
                break;

            case 'I':
                if(frame >= -2) {
                    ++aln_length_cds;
                    frame = (frame - 1) % 3;
                }
                break;

            case 'X':
                qpos += qinc;
                if( (qstrand && cds_start <= qpos && qpos < cds_stop) ||
                    (!qstrand && cds_start >= qpos && qpos > cds_stop) )
                {
                    frame = kFrame_lost;
                    ++aln_length_cds;
                }
                break;
            }
        }

        {
            CRef<CScore> score_matches_inframe (new CScore());
            score_matches_inframe->SetId().SetId(eCS_InframeMatches);
            score_matches_inframe->SetValue().SetInt(matches_frame[2]);
            scores.push_back(score_matches_inframe);
        }

        {
            CRef<CScore> score_inframe_identity (new CScore());
            score_inframe_identity->SetId().SetId(eCS_InframeIdentity);
            score_inframe_identity->SetValue().
                SetReal(double(matches_frame[2]) / aln_length_cds);
            scores.push_back(score_inframe_identity);
        }
    }


    if(embed_scoreset) {
        CSeq_align::TScore & sa_score (sa->SetScore());
        sa_score.resize(scores.size());
        copy(scores.begin(), scores.end(), sa_score.begin());
    }

    return ss;
}


END_NCBI_SCOPE


