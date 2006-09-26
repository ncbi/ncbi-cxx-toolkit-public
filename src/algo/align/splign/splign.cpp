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

#include <deque>
#include <math.h>
#include <algorithm>

BEGIN_NCBI_SCOPE

// define cut-off strategy at the terminii:

// (1) - pre-processing
// For non-covered ends longer than kNonCoveredEndThreshold use
// m_max_genomic_ext. For shorter ends use k * query_len^(1/kPower)

static const Uint4 kNonCoveredEndThreshold = 55;
static const double kPower = 3;

// (2) - post-processing
// exons shorter than kMinTermExonSize with identity lower than
// kMinTermExonIdty will be converted to gaps
static const size_t kMinTermExonSize = 28;
static const double kMinTermExonIdty = 0.90;

CSplign::CSplign( void )
{
    m_compartment_penalty = s_GetDefaultCompartmentPenalty();
    m_MinExonIdty = s_GetDefaultMinExonIdty();
    m_MinSingletonIdty = m_MinCompartmentIdty =s_GetDefaultMinCompartmentIdty();
    m_max_genomic_ext = s_GetDefaultMaxGenomicExtent();
    m_endgaps = true;
    m_strand = true;
    m_nopolya = false;
    m_cds_start = m_cds_stop = 0;
    m_model_id = 0;
    m_MaxCompsPerQuery = 0;
    m_MinPatternHitLength = 10;
}

CSplign::~CSplign()
{
}


CRef<CSplign::TAligner>& CSplign::SetAligner( void ) {
    return m_aligner;
}

CConstRef<CSplign::TAligner> CSplign::GetAligner( void ) const {
    return m_aligner;
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


size_t CSplign::s_GetDefaultMaxGenomicExtent(void)
{
    return 25000;
}


void CSplign::SetMaxGenomicExtent(size_t mge)
{
    m_max_genomic_ext = mge;
}


size_t CSplign::GetMaxGenomicExtent(void) const
{
    return m_max_genomic_ext;
}

double CSplign::GetMinExonIdentity( void ) const {
    return m_MinExonIdty;
}

double CSplign::s_GetDefaultMinExonIdty(void)
{
    return 0.75;
}


double CSplign::GetMinCompartmentIdentity(void) const {
    return m_MinCompartmentIdty;
}

double CSplign::GetMinSingletonIdentity(void) const {
    return m_MinSingletonIdty;
}

double CSplign::s_GetDefaultMinCompartmentIdty(void)
{
    return 0.5;
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
    m_compartment_penalty = penalty;
}

double CSplign::s_GetDefaultCompartmentPenalty(void)
{
    return 0.4;
}

double CSplign::GetCompartmentPenalty( void ) const 
{
    return m_compartment_penalty;
}


void CSplign::x_LoadSequence(vector<char>* seq, 
                             const objects::CSeq_id& seqid,
                             THit::TCoord start,
                             THit::TCoord finish,
                             bool retain)
{
    USING_SCOPE(objects);

    try {
    
        if(m_Scope.IsNull()) {
            NCBI_THROW(CAlgoAlignException, eInternal, "Splign scope not set");
        }

        CBioseq_Handle bh = m_Scope->GetBioseqHandle(seqid);

        if(retain && m_CanResetHistory) {
            m_Scope->ResetHistory();
        }

        if(bh) {

            CSeqVector sv = bh.GetSeqVector(CBioseq_Handle::eCoding_Iupac);
            const TSeqPos dim = sv.size();
            if(dim == 0) {
                NCBI_THROW(CAlgoAlignException, eNoData, 
                           string("Sequence is empty: ") 
                           + seqid.AsFastaString());
            }
            if(finish > dim) {
                finish = dim - 1;
            }
            if(start > finish) {
                NCBI_THROW(CAlgoAlignException, eInternal, 
                           "Invalid sequence interval requested.");
            }
            
            string s;
            sv.GetSeqData(start, finish + 1, s);
            seq->resize(1 + finish - start);
            copy(s.begin(), s.end(), seq->begin());
        }
        else {
            NCBI_THROW(CAlgoAlignException, eNoData, 
                       string("ID not found: ") + seqid.AsFastaString());
        }
        
        if(retain == false && m_CanResetHistory) {
            m_Scope->RemoveFromHistory(bh);
        }
        
    }
    
    catch(CAlgoAlignException& e) {
        e.SetSeverity(eDiag_Fatal);
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


void CSplign::x_SetPattern(THitRefs* phitrefs)
{
    typedef CHitComparator<THit> THitComparator;
    THitComparator sorter (THitComparator::eQueryMin);
    stable_sort(phitrefs->begin(), phitrefs->end(), sorter);

    const size_t max_intron = 1048575; // 2^20
    size_t prev = 0;
    ITERATE(THitRefs, ii, *phitrefs) {

        const THitRef& h = *ii;
        if(h->GetQuerySpan() < m_MinPatternHitLength) {
            continue;
        }

        if(prev > 0) {

            const bool consistent = h->GetSubjStrand()?
                (h->GetSubjStart() < prev + max_intron):
                (h->GetSubjStart() + max_intron > prev);
            if(!consistent) {
                const string errmsg = g_msg_CompartmentInconsistent
                    + string(" (extra long introns)");
                NCBI_THROW(CAlgoAlignException, eNoAlignment, errmsg);
            }
        }
        prev = h->GetSubjStop();
    }

    vector<size_t> pattern0;
    vector<pair<bool,double> > imperfect;
    for(size_t i = 0, n = phitrefs->size(); i < n; ++i) {

        const THitRef& h = (*phitrefs)[i];
        if(h->GetQuerySpan() >= m_MinPatternHitLength) {
            pattern0.push_back(h->GetQueryMin());
            pattern0.push_back(h->GetQueryMax());
            pattern0.push_back(h->GetSubjMin());
            pattern0.push_back(h->GetSubjMax());
            const double idty = h->GetIdentity();
            const bool imprf = idty < 1.00 || h->GetQuerySpan() != h->GetSubjSpan();
            imperfect.push_back(pair<bool,double>(imprf, idty));
        }
    }

    const char* Seq1 = &m_mrna.front();
    const size_t SeqLen1 = m_polya_start < kMax_UInt?
        m_polya_start: m_mrna.size();
    const char* Seq2 = &m_genomic.front();
    const size_t SeqLen2 = m_genomic.size();
    
    // verify some conditions on the input hit pattern
    size_t dim = pattern0.size();
    CNcbiOstrstream ostr_err;
    bool severe = false;
    if(dim % 4 == 0) {

        for(size_t i = 0; i < dim; i += 4) {
            
            if(pattern0[i] > pattern0[i+1] || pattern0[i+2] > pattern0[i+3]) {
                ostr_err << "Pattern hits must be specified in plus strand";
                severe = true;
                break;
            }
            
            if(i > 4) {
                if(pattern0[i] <= pattern0[i-3] || pattern0[i+2] <= pattern0[i-1]) {
                    ostr_err << g_msg_CompartmentInconsistent
                             << string(" (hits not sorted)");
                    break;
                }
            }
            
            if(pattern0[i+1] >= SeqLen1 || pattern0[i+3] >= SeqLen2) {
                ostr_err << "One or several pattern hits are out of range";
                break;
            }
        }

    }
    else {
        ostr_err << "Pattern must have a dimension multiple of four";
        severe = true;
    }
    
    if(severe) {
        ostr_err << " (query = " 
                 << phitrefs->front()->GetQueryId()->AsFastaString() 
                 << " , subj = "
                 << phitrefs->front()->GetSubjId()->AsFastaString() << ')'
                 << endl;
    }

    const string err = CNcbiOstrstreamToString(ostr_err);
    if(err.size() > 0) {
        if(severe) {
            NCBI_THROW(CAlgoAlignException, eBadParameter, err);
        }
        else {
            NCBI_THROW(CAlgoAlignException, eNoAlignment, err);
        }
    }
    else {

        m_alnmap.clear();
        m_pattern.clear();
        
        SAlnMapElem map_elem;
        map_elem.m_box[0] = map_elem.m_box[2] = 0;
        map_elem.m_pattern_start = map_elem.m_pattern_end = -1;
        
        // build the alignment map
        CBandAligner nwa;
        for(size_t i = 0; i < dim; i += 4) {    
            
            size_t L1, R1, L2, R2;
            size_t max_seg_size = 0;

            bool imprf = imperfect[i/4].first;
            if(imprf) {                

                const size_t len1 = pattern0[i+1] - pattern0[i] + 1;
                const size_t len2 = pattern0[i+3] - pattern0[i+2] + 1;
                const size_t maxlen = max(len1, len2);
                const size_t band = size_t((1 - imperfect[i/4].second) * maxlen) + 2;
                nwa.SetBand(band);
                nwa.SetSequences(Seq1 + pattern0[i],   len1,
                                 Seq2 + pattern0[i+2], len2,
                                 false);
                nwa.Run();
                max_seg_size = nwa.GetLongestSeg(&L1, &R1, &L2, &R2);
            }
            else {
                L1 = 0;
                R1 = pattern0[i+1] - pattern0[i];
                L2 = 0;
                R2 = pattern0[i+3] - pattern0[i+2];
                max_seg_size = R1 - L1 + 1;
            }

            if(max_seg_size) {

                // make the core
                {{
                    const size_t cut = (1 + R1 - L1) / 5;
                    const size_t l1 = L1 + cut, l2 = L2 + cut;
                    const size_t r1 = R1 - cut, r2 = R2 - cut;
                    if(l1 < r1 && l2 < r2) {
                        L1 = l1; L2 = l2; 
                        R1 = r1; R2 = r2;
                    }
                }}

                const size_t hitlen_q = pattern0[i + 1] - pattern0[i] + 1;
                const size_t hlq4 = hitlen_q/4;
                const size_t sh = hlq4;
                
                size_t delta = sh > L1? sh - L1: 0;
                size_t q0 = pattern0[i] + L1 + delta;
                size_t s0 = pattern0[i+2] + L2 + delta;
                
                const size_t h2s_right = hitlen_q - R1 - 1;
                delta = sh > h2s_right? sh - h2s_right: 0;
                size_t q1 = pattern0[i] + R1 - delta;
                size_t s1 = pattern0[i+2] + R2 - delta;
                
                if(q0 > q1 || s0 > s1) { // longest seg was probably too short
                    q0 = pattern0[i] + L1;
                    s0 = pattern0[i+2] + L2;
                    q1 = pattern0[i] + R1;
                    s1 = pattern0[i+2] + R2;
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
        if(h->GetQueryStrand() == false) {
            h->FlipStrands();
        }
    }
  
    if(m_aligner.IsNull()) {
        NCBI_THROW(CAlgoAlignException, eNotInitialized, g_msg_AlignedNotSpecified);
    }
    
    if(hitrefs.size() == 0) {
        NCBI_THROW(CAlgoAlignException, eNoData, g_msg_EmptyHitVectorPassed);
    }

    m_result.clear();

    THit::TId id_query (hitrefs.front()->GetQueryId());

    const TSeqPos mrna_size = objects::sequence::GetLength(*id_query, m_Scope);
    if(mrna_size == numeric_limits<TSeqPos>::max()) {
        NCBI_THROW(CAlgoAlignException, eNoData, 
                   string("Sequence not found: ") + id_query->AsFastaString());
    }
    
    const TSeqPos comp_penalty_bps = size_t(m_compartment_penalty * mrna_size);
    const TSeqPos min_matches = size_t(m_MinCompartmentIdty * mrna_size);
    const TSeqPos min_singleton_matches = size_t(m_MinSingletonIdty * mrna_size);

    // iterate through compartments
    CCompartmentAccessor<THit> comps (hitrefs.begin(), hitrefs.end(),
                                      comp_penalty_bps,
                                      min_matches,
                                      min_singleton_matches);

    size_t dim = comps.GetCount();    
    if(dim > 0) {
 
        // pre-load cDNA
        m_mrna.clear();
        x_LoadSequence(&m_mrna, *id_query, 0, 
                       numeric_limits<THit::TCoord>::max(), false);

        if(!m_strand) {
            // make reverse complimentary
            reverse (m_mrna.begin(), m_mrna.end());
            transform(m_mrna.begin(), m_mrna.end(), m_mrna.begin(), SCompliment());
        }

        // find and cache max ORF when in sense direction
        if(m_strand) {

            TStrIdToCDS::const_iterator ie = m_CdsMap.end();
            const string strid = id_query->AsFastaString();
            TStrIdToCDS::const_iterator ii = m_CdsMap.find(strid);
            if(ii == ie) {
            
                USING_SCOPE(objects);
                vector<CRef<CSeq_loc> > orfs;
                vector<string> start_codon;
                start_codon.push_back("ATG");
                COrf::FindOrfs(m_mrna, orfs, 150, 1, start_codon);
                TSeqPos max_len = 0;
                TSeqPos max_from = 0;
                TSeqPos max_to = 0;
                ITERATE (vector<CRef<CSeq_loc> >, orf, orfs) {
                    TSeqPos len = sequence::GetLength(**orf, NULL);
                    if ((*orf)->GetInt().GetStrand() != eNa_strand_minus) {
                        if (len > max_len) {
                            max_len = len;
                            max_from = (*orf)->GetInt().GetFrom();
                            max_to = (*orf)->GetInt().GetTo();
                        }
                    }
                }

                TCDS cds (m_cds_start = max_from, m_cds_stop = max_to);
                m_CdsMap[strid] = cds;
            }
            else {
                m_cds_start = ii->second.first;
                m_cds_stop = ii->second.second;
            }
        }

    }

    // compartments share the space between them
    size_t smin = 0, smax = kMax_UInt;
    bool same_strand = false;

    const THit::TCoord* box = comps.GetBox(0);
    if(m_MaxCompsPerQuery > 0 && dim > m_MaxCompsPerQuery) {
        dim = m_MaxCompsPerQuery;
    }

    for(size_t i = 0; i < dim; ++i, box += 4) {
        
        if(i+1 == dim) {
            smax = kMax_UInt;
            same_strand = false;
        }
        else {            
            bool strand_this = comps.GetStrand(i);
            bool strand_next = comps.GetStrand(i+1);
            same_strand = strand_this == strand_next;
            smax = same_strand? (box+4)[2] - 1: kMax_UInt;
        }
     
        try {

            THitRefs comp_hits;
            comps.Get(i, comp_hits);

            SAlignedCompartment ac = 
                x_RunOnCompartment(&comp_hits, smin, smax, 0 /* phitrefs */ );
            ac.m_id = ++m_model_id;
            ac.m_segments = m_segments;
            ac.m_error = false;
            ac.m_msg = "Ok";
            m_result.push_back(ac);
        }

        catch(CAlgoAlignException& e) {
            
            if(e.GetSeverity() == eDiag_Fatal) {
                throw;
            }

            m_result.push_back(SAlignedCompartment(0, true, e.GetMsg().c_str()));
            ++m_model_id;
        }
        smin = same_strand? box[3] + 1: 0;
    }
}


bool CSplign::AlignSingleCompartment(THitRefs* phitrefs,
                                     size_t subj_min, size_t subj_max,
                                     SAlignedCompartment* result,
                                     const THitRefs* phitrefs_all)
{
    m_mrna.resize(0);

    THit::TId id_query (phitrefs->front()->GetQueryId());

    x_LoadSequence(&m_mrna, *id_query, 0, 
                   numeric_limits<THit::TCoord>::max(), false);

    if(!m_strand) {

        reverse (m_mrna.begin(), m_mrna.end());
        transform(m_mrna.begin(), m_mrna.end(), m_mrna.begin(), SCompliment());
    }

    bool rv = true;
    try {
        SAlignedCompartment ac = 
            x_RunOnCompartment(phitrefs, subj_min, subj_max, phitrefs_all);
        ac.m_id = 1;
        ac.m_segments = m_segments;
        ac.m_error = false;
        ac.m_msg = "Ok";

        *result = ac;        
        m_mrna.resize(0);
    }

    catch(CAlgoAlignException& e) {

        m_mrna.resize(0);        

        if(e.GetSeverity() == eDiag_Fatal) {
            throw;
        }
        
        *result = SAlignedCompartment(0, true, e.GetMsg().c_str());
        rv = false;
    }

    return rv;
}

// naive polya detection
size_t CSplign::x_TestPolyA(void)
{
    const size_t dim = m_mrna.size();
    int i = dim - 1;
    for(; i >= 0; --i) {
        if(m_mrna[i] != 'A') break;
    }
    const size_t len = dim - i - 1;;
    return len > 3 ? i + 1 : kMax_UInt;
}


// PRE:  Hits (initial, not transformed) representing the compartment; 
//       maximum genomic sequence span;
//       pre-loaded and appropriately transformed query sequence.
// POST: A set of segments packed into the aligned compartment.

CSplign::SAlignedCompartment CSplign::x_RunOnCompartment(
    THitRefs* phitrefs, size_t range_left, size_t range_right,
    const THitRefs* phitrefs_all)
{    
    SAlignedCompartment rv;

    try {
        m_segments.clear();
    
        if(range_left > range_right) {
            NCBI_THROW(CAlgoAlignException, eInternal, g_msg_InvalidRange);
        }

        if(phitrefs_all == 0) {
            XFilter(phitrefs); // use all compartment hits
        }
        else {
            // 1. Mark (reset ids) the compartment hits
            // 2. Create two sets of 'hot' intervals using non-compartment hits
            //    of same strand and fitting the genomic span, and also from
            //    all overlapping intervals of the compartment hits
            // 3. Filter the compartment hits using the 'hot' intervals as follows:
            //    a. Drop the hit if 50% or more is covered by the intervals
            //    b. Truncate covered hit ends
            //    c. Ignore all other intervals
        }

        if(phitrefs->size() == 0) {
            NCBI_THROW(CAlgoAlignException, eNoAlignment, g_msg_NoHitsAfterFiltering);
        }
    
        const size_t mrna_size = m_mrna.size();
    
        if(m_strand == false) {
        
            // adjust the hits
            for(size_t i = 0, n = phitrefs->size(); i < n; ++i) {

                THitRef& h = (*phitrefs)[i];
                THit::TCoord a0 = mrna_size - h->GetQueryMin() - 1;
                THit::TCoord a1 = mrna_size - h->GetQueryMax() - 1;
                const bool new_strand = ! (h->GetSubjStrand());
                h->SetQueryStart(a1);
                h->SetQueryStop(a0);
                h->SetSubjStrand(new_strand);
            }
        }
    
        m_polya_start = m_nopolya? kMax_UInt: x_TestPolyA();
    
        if(m_polya_start < kMax_UInt) {

            // cleave off hits beyond cds
            CleaveOffByTail(phitrefs, m_polya_start); 
            if(phitrefs->size() == 0) {
                NCBI_THROW(CAlgoAlignException,eNoAlignment,g_msg_NoHitsBeyondPolya);
            }
        }
    
        // find regions of interest on mRna (query)
        // and contig (subj)
        THit::TCoord span [4];
        CHitFilter<THit>::s_GetSpan(*phitrefs, span);
        THit::TCoord qmin = span[0], qmax = span[1], 
            smin = span[2], smax = span[3];    

        // select terminal genomic extents based on uncovered end sizes
        THit::TCoord extent_left = x_GetGenomicExtent(qmin);
        THit::TCoord qspace = m_mrna.size() - qmax + 1;
        THit::TCoord extent_right = x_GetGenomicExtent(qspace);

        bool ctg_strand = phitrefs->front()->GetSubjStrand();

        if(ctg_strand) {

            smin = max(0, int(smin - extent_left));
            smax += extent_right;
        }
        else {
            smin = max(0, int(smin - extent_right));
            smax += extent_left;
        }

        // regardless of hits, all cDNA is aligned (without the tail, if any)
        qmin = 0;
        qmax = m_polya_start < kMax_UInt? m_polya_start - 1: mrna_size - 1;
    
        if(smin < range_left) {
            smin = range_left;
        }
        if(smax > range_right) {
            smax = range_right;
        }

        m_genomic.clear();
        x_LoadSequence(&m_genomic, *(phitrefs->front()->GetSubjId()), 
                       smin, smax, true);
    
        const THit::TCoord ctg_end = smin + m_genomic.size();
        if(ctg_end - 1 < smax) { // perhabs adjust smax
            smax = ctg_end - 1;
        }

        if(!ctg_strand) {

            // make reverse complementary
            // for the contig's area of interest
            reverse (m_genomic.begin(), m_genomic.end());
            transform(m_genomic.begin(), m_genomic.end(), m_genomic.begin(),
                      SCompliment());

            // flip hits
            NON_CONST_ITERATE(THitRefs, ii, *phitrefs) {

                THitRef& h = *ii;
                THit::TCoord a2 = smax - (h->GetSubjMax() - smin);
                THit::TCoord a3 = smax - (h->GetSubjMin() - smin);
                h->SetSubjStart(a2);
                h->SetSubjStop(a3);
            }
        }

        rv.m_QueryStrand = m_strand;
        rv.m_SubjStrand  = ctg_strand;
    
        // shift hits so that they originate from zero

        NON_CONST_ITERATE(THitRefs, ii, *phitrefs) {
            (*ii)->Shift(-qmin, -smin);
        }  
    
        x_SetPattern(phitrefs);
        x_Run(&m_mrna.front(), &m_genomic.front());

        const size_t seg_dim = m_segments.size();
        if(seg_dim == 0) {
            NCBI_THROW(CAlgoAlignException, eNoAlignment, g_msg_NoAlignment);
        }
    
        // try to extend the last segment into the PolyA area  
        if(m_polya_start < kMax_UInt && seg_dim && m_segments[seg_dim-1].m_exon) {
            CSplign::SSegment& s = 
                const_cast<CSplign::SSegment&>(m_segments[seg_dim-1]);
            const char* p0 = &m_mrna.front() + s.m_box[1] + 1;
            const char* q = &m_genomic.front() + s.m_box[3] + 1;
            const char* p = p0;
            const char* pe = &m_mrna.front() + mrna_size;
            const char* qe = &m_genomic.front() + m_genomic.size();
            for(; p < pe && q < qe; ++p, ++q) {
                if(*p != 'A') break;
                if(*p != *q) break;
            }
        
            const size_t sh = p - p0;
            if(sh) {
                // resize
                s.m_box[1] += sh;
                s.m_box[3] += sh;
                s.m_details.append(sh, 'M');
                s.Update(m_aligner);
            
                // fix annotation
                const size_t ann_dim = s.m_annot.size();
                if(ann_dim > 2 && s.m_annot[ann_dim - 3] == '>') {

                    s.m_annot[ann_dim - 2] = q < qe? *q: ' ';
                    ++q;
                    s.m_annot[ann_dim - 1] = q < qe? *q: ' ';
                }
            
                m_polya_start += sh;
            }
        }
    
        // look for PolyA in trailing segments:
        // if a segment is mostly 'A's then we add it to PolyA

        int j = seg_dim - 1, j0 = j;
        for(; j >= 0; --j) {
        
            const CSplign::SSegment& s = m_segments[j];

            const char* p0 = &m_mrna[qmin] + s.m_box[0];
            const char* p1 = &m_mrna[qmin] + s.m_box[1] + 1;
            size_t count = 0;
            for(const char* pc = p0; pc != p1; ++pc) {
                if(*pc == 'A') ++count;
            }
        
            double min_a_content = 0.799;
            // also check splices
            if(s.m_exon && j > 0 && m_segments[j-1].m_exon) {
                bool consensus = CSplign::SSegment
                   ::s_IsConsensusSplice(m_segments[j-1].GetDonor(), s.GetAcceptor());
                if(!consensus) {
                    min_a_content = 0.599;
                }
            }

            if(!s.m_exon) {
                min_a_content = s.m_len > 4? 0.599: -1;
            }
        

            if(double(count)/(p1 - p0) < min_a_content) {
                if(s.m_exon) break;
            }
            else {
                j0 = j - 1;
            }
        }

        if(j0 >= 0 && j0 < int(seg_dim - 1)) {
            m_polya_start = m_segments[j0].m_box[1] + 1;
        }

        // test if we have at least one exon before poly(a)
        bool some_exons = false;
        for(int i = 0; i <= j0; ++i ) {
            if(m_segments[i].m_exon) {
                some_exons = true;
                break;
            }
        }
        if(!some_exons) {
            NCBI_THROW(CAlgoAlignException, eNoAlignment,g_msg_NoExonsAboveIdtyLimit);
        }
    
        m_segments.resize(j0 + 1);

        // scratch it if the total coverage is too low
        double mcount = 0;
        ITERATE(TSegments, jj, m_segments) {
            if(jj->m_exon) {
                mcount += jj->m_idty * jj->m_len;
            }
        }
        if(mcount / (1 + qmax) < m_MinSingletonIdty) {
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
    } // try

    catch(CAlgoAlignException& e) {
        
        const CException::TErrCode errcode = e.GetErrCode();
        const bool severe = 
            errcode != CAlgoAlignException::eNoAlignment && 
            errcode != CAlgoAlignException::eMemoryLimit;
        if(severe) {
            e.SetSeverity(eDiag_Fatal);
        }
        throw;
    }

    return rv;
}


static const char s_kGap [] = "<GAP>";

// at this level and below, plus strand is assumed
// for both sequences
void CSplign::x_Run(const char* Seq1, const char* Seq2)
{
    typedef deque<SSegment> TSegments;
    TSegments segments;

//#define DBG_DUMP_PATTERN
#ifdef  DBG_DUMP_PATTERN
    cerr << "Pattern:" << endl;  
#endif

    for(size_t i = 0, map_dim = m_alnmap.size(); i < map_dim; ++i) {

        const SAlnMapElem& zone = m_alnmap[i];

        // setup sequences
        const size_t len1 = zone.m_box[1] - zone.m_box[0] + 1;
        const size_t len2 = zone.m_box[3] - zone.m_box[2] + 1;
        m_aligner->SetSequences(
            Seq1 + zone.m_box[0], len1,
            Seq2 + zone.m_box[2], len2,
            true);

        // prepare the pattern
        vector<size_t> pattern;
        if(zone.m_pattern_start >= 0) {

            copy(m_pattern.begin() + zone.m_pattern_start,
            m_pattern.begin() + zone.m_pattern_end + 1,
            back_inserter(pattern));
            for(size_t j = 0, pt_dim = pattern.size(); j < pt_dim; j += 4) {

#ifdef  DBG_DUMP_PATTERN
	      cerr << pattern[j] << '\t' << pattern[j+1] << '\t'
                   << "(len = " << (pattern[j+1] - pattern[j] + 1) << ")\t"
		   << pattern[j+2] << '\t' << pattern[j+3] 
                   << "(len = " << (pattern[j+3] - pattern[j+2] + 1) << ")\t"
                   << endl;
#endif
                pattern[j]   -= zone.m_box[0];
                pattern[j+1] -= zone.m_box[0];
                pattern[j+2] -= zone.m_box[2];
                pattern[j+3] -= zone.m_box[2];
            }
            if(pattern.size()) {
                m_aligner->SetPattern(pattern);
            }

            // setup esf
            m_aligner->SetEndSpaceFree(true, true, true, true);

            m_aligner->SetCDS(m_cds_start, m_cds_stop);
            
            // align
            m_aligner->Run();

//#define DBG_DUMP_TYPE2
#ifdef  DBG_DUMP_TYPE2
            {{
            CNWFormatter fmt (*m_aligner);
            string txt;
            fmt.AsText(&txt, CNWFormatter::eFormatType2);
            cerr << txt;
            }}  
#endif

            // create list of segments
            // FIXME: Move segmentation to CSplicedAligner.
            // Use it both here and in the formatter.
            CNWFormatter formatter (*m_aligner);
            string exons;
            formatter.AsText(&exons, CNWFormatter::eFormatExonTableEx);      

            CNcbiIstrstream iss_exons (exons.c_str());
            while(iss_exons) {
                string id1, id2, txt, repr;
                size_t q0, q1, s0, s1, size;
                double idty;
                iss_exons >> id1 >> id2 >> idty >> size
			  >> q0 >> q1 >> s0 >> s1 >> txt >> repr;
                if(!iss_exons) break;
                q0 += zone.m_box[0];
                q1 += zone.m_box[0];
                s0 += zone.m_box[2];
                s1 += zone.m_box[2];
                SSegment e;
                e.m_exon = true;
                e.m_box[0] = q0; e.m_box[1] = q1;
                e.m_box[2] = s0; e.m_box[3] = s1;
                e.m_annot = txt;
                e.m_details = repr;
                e.Update(m_aligner);
                segments.push_back(e);
            }

            // append a gap
            if(i + 1 < map_dim) {
                SSegment g;
                g.m_exon = false;
                g.m_box[0] = zone.m_box[1] + 1;
                g.m_box[1] = m_alnmap[i+1].m_box[0] - 1;
                g.m_box[2] = zone.m_box[3] + 1;
                g.m_box[3] = m_alnmap[i+1].m_box[2] - 1;
                g.m_idty = 0;
                g.m_len = g.m_box[1] - g.m_box[0] + 1;
                g.m_annot = s_kGap;
                g.m_details.resize(0);
                g.m_score = 0; // no score for <Gap>s
                segments.push_back(g);
            }
        }
    } // zone iterations end


//#define DUMP_ORIG_SEGS
#ifdef DUMP_ORIG_SEGS
    cerr << "Orig segments:" << endl;
    ITERATE(TSegments, ii, segments) {
        cerr << ii->m_exon << '\t' << ii->m_idty << '\t' << ii->m_len << '\t'
             << ii->m_box[0] << '\t' << ii->m_box[1] << '\t'
             << ii->m_box[2] << '\t' << ii->m_box[3] << '\t'
             << ii->m_annot << '\t' << ii->m_score << endl;
    }
#endif

    // segment-level postprocessing
    m_segments.resize(0);
    while(true) {

        if(m_segments.size() > 0) {
            segments.resize(m_segments.size());
            copy(m_segments.begin(), m_segments.end(), segments.begin());
            m_segments.resize(0);
        }

        size_t seg_dim = segments.size();
        if(seg_dim == 0) {
            return;
        }

        size_t exon_count0 = 0;
        ITERATE(TSegments, ii, segments) {
            if(ii->m_exon) ++exon_count0;
        }

        // First go from the ends and see if we
        // can improve boundary exons
        size_t k0 = 0;
        while(k0 < seg_dim) {

            SSegment& s = segments[k0];
            if(s.m_exon) {

                const size_t len = 1 + s.m_box[1] - s.m_box[0];
                const double min_idty = len >= kMinTermExonSize?
                    m_MinExonIdty: max(m_MinExonIdty, kMinTermExonIdty);

                if(s.m_idty < min_idty || m_endgaps) {
                    s.ImproveFromLeft(Seq1, Seq2, m_aligner);
                }

                if(s.m_idty >= min_idty) {
                    break;
                }
            }
            ++k0;
        }

        // fill the left-hand gap, if any
        if(segments[0].m_exon && segments[0].m_box[0] > 0) {
            segments.push_front(SSegment());
            SSegment& g = segments.front();
            g.m_exon = false;
            g.m_box[0] = 0;
            g.m_box[1] = segments[0].m_box[0] - 1;
            g.m_box[2] = 0;
            g.m_box[3] = segments[0].m_box[2] - 1;
            g.m_idty = 0;
            g.m_len = segments[0].m_box[0];
            g.m_annot = s_kGap;
            g.m_details.resize(0);
            g.m_score = 0;
            ++seg_dim;
            ++k0;
        }

        int k1 = int(seg_dim - 1);
        while(k1 >= int(k0)) {
            SSegment& s = segments[k1];
            if(s.m_exon) {

                const size_t len = 1 + s.m_box[1] - s.m_box[0];
                const double min_idty = len >= kMinTermExonSize?
                    m_MinExonIdty: max(m_MinExonIdty, kMinTermExonIdty);
                if(s.m_idty < min_idty || m_endgaps) {
                    s.ImproveFromRight(Seq1, Seq2, m_aligner);
                }
                if(s.m_idty >= min_idty) {
                    break;
                }
            }
            --k1;
        }

        const size_t SeqLen2 = m_genomic.size();
        const size_t SeqLen1 = m_polya_start == kMax_UInt? m_mrna.size():
            m_polya_start;

        // fill the right-hand gap, if any
        if( segments[seg_dim - 1].m_exon && 
            segments[seg_dim - 1].m_box[1] < SeqLen1 - 1) {

            SSegment g;
            g.m_exon = false;
            g.m_box[0] = segments[seg_dim - 1].m_box[1] + 1;
            g.m_box[1] = SeqLen1 - 1;
            g.m_box[2] = segments[seg_dim - 1].m_box[3] + 1;
            g.m_box[3] = SeqLen2 - 1;
            g.m_idty = 0;
            g.m_len = g.m_box[1] - g.m_box[0] + 1;
            g.m_annot = s_kGap;
            g.m_details.resize(0);
            g.m_score = 0;
            segments.push_back(g);
            ++seg_dim;
        }

        // turn to gaps exons with low identity
        for(size_t k = 0; k < seg_dim; ++k) {
            SSegment& s = segments[k];
            if(s.m_exon && s.m_idty < m_MinExonIdty) {
                s.m_exon = false;
                s.m_idty = 0;
                s.m_len = s.m_box[1] - s.m_box[0] + 1;
                s.m_annot = s_kGap;
                s.m_details.resize(0);
                s.m_score = 0;
            }
        }

        // turn to gaps short weak terminal exons
        {{
            // find the two leftmost exons
            size_t exon_count = 0;
            SSegment* term_segs[] = {0, 0};
            for(size_t i = 0; i < seg_dim; ++i) {
                SSegment& s = segments[i];
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
            size_t exon_count = 0;
            SSegment* term_segs[] = {0, 0};
            for(int i = seg_dim - 1; i >= 0; --i) {
                SSegment& s = segments[i];
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

        // turn to gaps extra-short exons preceeded/followed by gaps
        bool gap_prev = false;
        for(size_t k = 0; k < seg_dim; ++k) {
            SSegment& s = segments[k];
            if(s.m_exon == false) {
                gap_prev = true;
            }
            else {
                size_t length = s.m_box[1] - s.m_box[0] + 1;
                bool gap_next = false;
                if(k + 1 < seg_dim) {
                    gap_next = !segments[k+1].m_exon;
                }
                if(length <= 5 && (gap_prev || gap_next)) {
                    s.m_exon = false;
                    s.m_idty = 0;
                    s.m_len = s.m_box[1] - s.m_box[0] + 1;
                    s.m_annot = s_kGap;
                    s.m_details.resize(0);
                    s.m_score = 0;
                }
                gap_prev = false;
            }
        }

        // merge all adjacent gaps
        int gap_start_idx = -1;
        if(seg_dim && segments[0].m_exon == false) {
            gap_start_idx = 0;
        }
        for(size_t k = 0; k < seg_dim; ++k) {
            SSegment& s = segments[k];
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
                    SSegment& g = segments[gap_start_idx];
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
            SSegment& g = segments[gap_start_idx];
            g.m_box[1] = segments[seg_dim-1].m_box[1];
            g.m_box[3] = segments[seg_dim-1].m_box[3];
            g.m_len = g.m_box[1] - g.m_box[0] + 1;
            g.m_details.resize(0);
            m_segments.push_back(g);
        }

        size_t exon_count1 = 0;
        ITERATE(CSplign::TSegments, ii, m_segments) {
            if(ii->m_exon) ++exon_count1;
        }

        if(exon_count0 == exon_count1) break;
    }

//#define DUMP_PROCESSED_SEGS
#ifdef DUMP_PROCESSED_SEGS
    cerr << "Processed segments:" << endl;
    ITERATE(CSplign::TSegments, ii, m_segments) {
        cerr << ii->m_exon << '\t' << ii->m_idty << '\t' << ii->m_len << '\t'
             << ii->m_box[0] << '\t' << ii->m_box[1] << '\t'
             << ii->m_box[2] << '\t' << ii->m_box[3] << '\t'
             << ii->m_annot << '\t' << ii->m_score << endl;
    }
#endif

}


// try improving the segment by cutting it from the left
void CSplign::SSegment::ImproveFromLeft(const char* seq1, const char* seq2,
                                        CConstRef<CSplicedAligner> aligner)
{
    const size_t min_query_size = 4;
    
    int i0 = int(m_box[1] - m_box[0] + 1), i0_max = i0;
    if(i0 < int(min_query_size)) {
        return;
    }
    
    // find the top score suffix
    int i1 = int(m_box[3] - m_box[2] + 1), i1_max = i1;
    
    CNWAligner::TScore score_max = 0, s = 0;
    
    const CNWAligner::TScore wm =  1;
    const CNWAligner::TScore wms = -1;
    const CNWAligner::TScore wg =  0;
    const CNWAligner::TScore ws =  -1;
  
    string::reverse_iterator irs0 = m_details.rbegin(),
        irs1 = m_details.rend(), irs = irs0, irs_max = irs0;
    
    for( ; irs != irs1; ++irs) {
        
        switch(*irs) {
            
        case 'M': {
            s += wm;
            --i0;
            --i1;
        }
        break;
            
        case 'R': {
            s += wms;
            --i0;
            --i1;
        }
        break;
            
        case 'I': {
            s += ws;
            if(irs > irs0 && *(irs-1)!='I') s += wg;
            --i1;
        }
        break;

        case 'D': {
            s += ws;
            if(irs > irs0 && *(irs-1)!='D') s += wg;
            --i0;
        }
        }

        if(s >= score_max) {
            score_max = s;
            i0_max = i0;
            i1_max = i1;
            irs_max = irs;
        }
    }

    // work around a weird case of equally optimal
    // but detrimental for our purposes alignment
    // -check the actual sequence chars
    size_t head = 0;
    while(i0_max > 0 && i1_max > 0) {
        if(seq1[m_box[0]+i0_max-1] == seq2[m_box[2]+i1_max-1]) {
            --i0_max; --i1_max;
            ++head;
        }
        else {
            break;
        }
    }
    
    // if the resulting segment is still long enough
    if(m_box[1] - m_box[0] + 1 - i0_max >= min_query_size
       && i0_max > 0) {
        
        // resize
        m_box[0] += i0_max;
        m_box[2] += i1_max;
        const size_t L = m_details.size() - (irs_max - irs0 + 1);
        m_details.erase(0, L);
        m_details.insert(m_details.begin(), head, 'M');
        Update(aligner);
        
        // update the first two annotation symbols
        if(m_annot.size() > 2 && m_annot[2] == '<') {
            int  j1 = m_box[2] - 2;
            char c1 = j1 >= 0? seq2[j1]: ' ';
            m_annot[0] = c1;
            int  j2 = m_box[2] - 2;
            char c2 = j2 >= 0? seq2[j2]: ' ';
            m_annot[1] = c2;
        }
    }
}


// try improving the segment by cutting it from the right
void CSplign::SSegment::ImproveFromRight(const char* seq1, const char* seq2,
                                         CConstRef<CSplicedAligner> aligner)
{
    const size_t min_query_size = 4;
    
    if(m_box[1] - m_box[0] + 1 < min_query_size) {
        return;
    }
    
    // find the top score prefix
    int i0 = -1, i0_max = i0;
    int i1 = -1, i1_max = i1;

    CNWAligner::TScore score_max = 0, s = 0;
    
    const CNWAligner::TScore wm =  1;
    const CNWAligner::TScore wms = -1;
    const CNWAligner::TScore wg =  0;
    const CNWAligner::TScore ws =  -1;
    
    string::iterator irs0 = m_details.begin(),
        irs1 = m_details.end(), irs = irs0, irs_max = irs0;
    
    for( ; irs != irs1; ++irs) {
        
        switch(*irs) {
            
        case 'M': {
            s += wm;
            ++i0;
            ++i1;
        }
        break;
            
        case 'R': {
            s += wms;
            ++i0;
            ++i1;
        }
        break;
      
        case 'I': {
            s += ws;
            if(irs > irs0 && *(irs-1) != 'I') s += wg;
            ++i1;
        }
        break;

        case 'D': {
            s += ws;
            if(irs > irs0 && *(irs-1) != 'D') s += wg;
            ++i0;
        }
    }
        
        if(s >= score_max) {
            score_max = s;
            i0_max = i0;
            i1_max = i1;
            irs_max = irs;
        }
    }
    
    int dimq = int(m_box[1] - m_box[0] + 1);
    int dims = int(m_box[3] - m_box[2] + 1);
    
    // work around a weird case of equally optimal
    // but detrimental for our purposes alignment
    // -check the actual sequences
    size_t tail = 0;
    while(i0_max < dimq - 1  && i1_max < dims - 1) {
        if(seq1[m_box[0]+i0_max+1] == seq2[m_box[2]+i1_max+1]) {
            ++i0_max; ++i1_max;
            ++tail;
        }
        else {
            break;
        }
    }
    
    dimq += tail;
    dims += tail;
    
    // if the resulting segment is still long enough
    if(i0_max >= int(min_query_size) && i0_max < dimq - 1) {
        
        m_box[1] = m_box[0] + i0_max;
        m_box[3] = m_box[2] + i1_max;
        
        m_details.resize(irs_max - irs0 + 1);
        m_details.insert(m_details.end(), tail, 'M');
        Update(aligner);
        
        // update the last two annotation chars
        const size_t adim = m_annot.size();
        if(adim > 2 && m_annot[adim - 3] == '>') {
            m_annot[adim-2] = seq2[m_box[3] + 1];
            m_annot[adim-1] = seq2[m_box[3] + 2];
        }
    }
}


void CSplign::SSegment::Update(CConstRef<CSplicedAligner> aligner)
{
    // restore length and identity
    m_len = m_details.size();

    string::const_iterator ib = m_details.begin(), ie = m_details.end();
    size_t count = 0; // std::count() not supported on some platforms
    for(string::const_iterator ii = ib; ii != ie; ++ii) {
        if(*ii == 'M') ++count;
    }
    m_idty = double(count) / m_len;
    
    CNWAligner::TTranscript transcript (m_details.size());
    size_t i = 0;
    ITERATE(string, ii, m_details) {
        transcript[i++] = CNWAligner::ETranscriptSymbol(*ii); // 2b fixed
    }
    m_score = aligner->CNWAligner::ScoreFromTranscript(transcript);
}


const char* CSplign::SSegment::GetDonor() const 
{
    const size_t adim = m_annot.size();
    return
      (adim > 2 && m_annot[adim - 3] == '>')? (m_annot.c_str() + adim - 2): 0;
}


const char* CSplign::SSegment::GetAcceptor() const 
{
    const size_t adim = m_annot.size();
    return (adim > 3 && m_annot[2] == '<')? m_annot.c_str(): 0;
}


bool CSplign::SSegment::s_IsConsensusSplice(const char* donor,
                                            const char* acceptor)
{
  return donor && acceptor &&
    (donor[0] == 'G' && (donor[1] == 'C' || donor[1] == 'T'))
    &&
    (acceptor[0] == 'A' && acceptor[1] == 'G');
}


double CSplign::SAlignedCompartment::GetIdentity() const
{
    string trans;
    for(size_t i = 0, dim = m_segments.size(); i < dim; ++i) {
        const SSegment& s = m_segments[i];
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
    ITERATE(TSegments, ii, m_segments) {
        const SSegment& s = *ii;
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


void CSplign::x_ProcessTermSegm(SSegment** term_segs, Uint1 side) const
{            
    const size_t exon_size = 1 + term_segs[0]->m_box[1] -
        term_segs[0]->m_box[0];

    if(exon_size < kMinTermExonSize) {

        bool turn2gap = false;
        const double idty = term_segs[0]->m_idty;
        if(idty < kMinTermExonIdty) {
            turn2gap = true;
        }
        else {
            
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

            const size_t intron_len = b - a;

            const bool consensus = CSplign::SSegment::s_IsConsensusSplice(dnr, acc);

            const size_t max_ext = (idty < .96 || !consensus || exon_size < 16)? 
                m_max_genomic_ext: (5000 *  kMinTermExonSize);

            const size_t max_intron_len = x_GetGenomicExtent(exon_size, max_ext);
            if(intron_len > max_intron_len) {
                turn2gap = true;
            }
        }
        
        if(turn2gap) {

            // turn the segment into a gap
            SSegment& s = *(term_segs[0]);
            s.m_exon = false;
            s.m_idty = 0;
            s.m_len = exon_size;
            s.m_annot = s_kGap;
            s.m_details.resize(0);
            s.m_score = 0;
        }
    }
}


Uint4 CSplign::x_GetGenomicExtent(const Uint4 query_len, Uint4 max_ext) const 
{
    if(max_ext == 0) {
        max_ext = m_max_genomic_ext;
    }

    if(query_len >= kNonCoveredEndThreshold) {
        return GetMaxGenomicExtent();
    }
    else {
        const double k = pow(kNonCoveredEndThreshold, - 1. / kPower) * max_ext;
        const double rv = k * pow(query_len, 1. / kPower);
        return Uint4(rv);
    }
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


void CSplign::SAlignedCompartment::ToBuffer(TNetCacheBuffer* target) const
{
    using namespace splign_local;

    if(target == 0) {
        NCBI_THROW(CAlgoAlignException, eBadParameter, g_msg_NullPointerPassed);
    }

    const size_t core_size = sizeof m_id + sizeof m_error + m_msg.size() + 1
        + sizeof m_QueryStrand + sizeof m_SubjStrand;

    vector<char> core (core_size);

    char* p = &core.front();
    ElemToBuffer(m_id, p);
    ElemToBuffer(m_error, p);
    ElemToBuffer(m_msg, p);
    ElemToBuffer(m_QueryStrand, p);
    ElemToBuffer(m_SubjStrand, p);
    
    typedef vector<TNetCacheBuffer> TBuffers;
    TBuffers vb (m_segments.size());
    size_t ibuf = 0;
    ITERATE(TSegments, ii, m_segments) {
        ii->ToBuffer(&vb[ibuf++]);
    }

    size_t total_size = core_size + sizeof(size_t) * m_segments.size();
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

    const size_t min_size = sizeof m_id + sizeof m_error + 1 
        + sizeof m_QueryStrand + sizeof m_SubjStrand;
    if(source.size() < min_size) {
        NCBI_THROW(CAlgoAlignException, eInternal, g_msg_NetCacheBufferIncomplete);
    }
    
    const char* p =  &source.front();
    ElemFromBuffer(m_id, p);
    ElemFromBuffer(m_error, p);
    ElemFromBuffer(m_msg, p);
    ElemFromBuffer(m_QueryStrand, p);
    ElemFromBuffer(m_SubjStrand, p);
    const char* pe = &source.back();
    while(p <= pe) {
        size_t seg_buf_size = 0;
        ElemFromBuffer(seg_buf_size, p);
        m_segments.push_back(SSegment());
        SSegment& seg = m_segments.back();
        seg.FromBuffer(TNetCacheBuffer(p, p + seg_buf_size));
        p += seg_buf_size;
    }
}


void CSplign::SSegment::ToBuffer(TNetCacheBuffer* target) const
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


void CSplign::SSegment::FromBuffer(const TNetCacheBuffer& source)
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

END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.61  2006/09/26 15:29:16  kapustin
 * Complete alignment information can now be passed to x_RunOnCompartment() for additional filtering of compartment hits
 *
 * Revision 1.60  2006/08/29 20:21:23  kapustin
 * Iterate seg-level core post-processing until no new exons created
 *
 * Revision 1.59  2006/08/01 15:25:40  kapustin
 * Suppress a warning
 *
 * Revision 1.58  2006/07/18 19:36:58  kapustin
 * Retrieve longest ORF information when in sense direction. Use band-limited NW for best diag extraction.
 *
 * Revision 1.57  2006/06/05 12:52:23  kapustin
 * Screen off final alignments with overall identity below the threshold
 *
 * Revision 1.56  2006/05/22 16:01:12  kapustin
 * Adjust the term exon cut off procedure
 *
 * Revision 1.53  2006/04/19 14:47:10  kapustin
 * Eliminate hits shorter than the cut-off when pre-checking for max intron length
 *
 * Revision 1.52  2006/04/17 19:30:11  kapustin
 * Move intron max length check to x_SetPattern to assure proper hit order
 *
 * Revision 1.51  2006/04/12 16:32:50  kapustin
 * Fix an issue with truncated non-polya R-GAP's
 *
 * Revision 1.50  2006/04/05 13:55:22  dicuccio
 * Added destructor, forbiddent copy constructor
 *
 * Revision 1.49  2006/04/04 22:28:33  kapustin
 * Tackle error code/severity handling
 *
 * Revision 1.48  2006/03/21 16:20:50  kapustin
 * Various changes, mainly adjust the code with  other libs
 *
 * Revision 1.47  2006/02/14 15:41:25  kapustin
 * +AlignSingleCompartment()
 *
 * Revision 1.46  2006/02/13 19:31:54  kapustin
 * Do not pre-load mRNA
 *
 * Revision 1.45  2005/12/13 18:41:22  kapustin
 * Limit space to look beyond blast hit boundary with the max genomic extent 
 * for non-covered queries above kNonCoveredEndThreshold
 *
 * Revision 1.44  2005/12/01 18:31:55  kapustin
 * +CSplign::PreserveScope()
 *
 * Revision 1.43  2005/11/21 14:24:00  kapustin
 * Reset scope history after bioseq_handle is obtained
 *
 * Revision 1.42  2005/10/31 16:29:53  kapustin
 * Retrieve parameter defaults with static member methods
 *
 * Revision 1.41  2005/10/24 17:33:17  kapustin
 * Ensure input hits have positive query strand
 *
 * Revision 1.40  2005/10/19 17:56:35  kapustin
 * Switch to using ObjMgr+LDS to load sequence data
 *
 * Revision 1.39  2005/10/13 21:23:58  kapustin
 * Fix a typo - reverse the perfect hit flag
 *
 * Revision 1.38  2005/09/28 18:04:29  kapustin
 * Verify that perfect hits actually have equal sides. 
 * Use relative coordinates as pattern base
 *
 * Revision 1.37  2005/09/27 18:03:50  kapustin
 * Fix a bug in term segment identity improvement step (supposedly rare)
 *
 * Revision 1.36  2005/09/21 14:16:52  kapustin
 * Adjust box pointer type to avoid compilation errors with GCC/64
 *
 * Revision 1.35  2005/09/12 16:24:00  kapustin
 * Move compartmentization to xalgoalignutil.
 *
 * Revision 1.34  2005/09/06 17:52:52  kapustin
 * Add interface to max_extent member
 *
 * Revision 1.33  2005/08/29 14:14:49  kapustin
 * Retain last subject sequence in memory when in batch mode.
 *
 * Revision 1.32  2005/08/18 15:11:15  kapustin
 * Use fatal severety to report missing IDs
 *
 * Revision 1.31  2005/08/02 15:55:36  kapustin
 * +x_GetGenomicExtent()
 *
 * Revision 1.30  2005/07/05 16:50:47  kapustin
 * Adjust compartmentization and term genomic extent. 
 * Introduce min overall identity required for compartments to align.
 *
 * Revision 1.29  2005/06/02 13:30:17  kapustin
 * Adjust GetBox() for different orientations
 *
 * Revision 1.28  2005/06/01 18:57:29  kapustin
 * +SAlignedCompartment::GetBox()
 *
 * Revision 1.27  2005/05/10 18:02:36  kapustin
 * + x_ProcessTermSegm()
 *
 * Revision 1.26  2005/01/26 21:33:12  kapustin
 * ::IsConsensusSplce ==> CSplign::SSegment::s_IsConsensusSplice
 *
 * Revision 1.25  2004/12/16 23:12:26  kapustin
 * algo/align rearrangement
 *
 * Revision 1.24  2004/12/01 14:55:08  kapustin
 * +ElemToBuffer
 *
 * Revision 1.23  2004/11/29 14:37:16  kapustin
 * CNWAligner::GetTranscript now returns TTranscript and direction can be 
 * specified. x_ScoreByTanscript renamed to ScoreFromTranscript with two
 * additional parameters to specify starting coordinates.
 *
 * Revision 1.22  2004/09/27 17:12:38  kapustin
 * Move splign_compartment_finder.hpp to /include/algo/align/splign.
 * SetIntronLimits() => SetMaxIntron()
 *
 * Revision 1.21  2004/09/21 16:39:47  kapustin
 * Use separate constants to specify min term exon length and the min
 * non-covered query space
 *
 * Revision 1.20  2004/09/09 21:23:41  kapustin
 * Adjust min term exon size to use with fixed genomic extent
 *
 * Revision 1.19  2004/07/19 13:38:07  kapustin
 * Remove unused conditional code
 *
 * Revision 1.18  2004/06/29 20:51:52  kapustin
 * Use CRef to access CObject-derived members
 *
 * Revision 1.17  2004/06/23 18:56:38  kapustin
 * Increment model id for models with exceptions
 *
 * Revision 1.16  2004/06/21 18:43:20  kapustin
 * Tweak seg-level post-processing to reconcile boundary identity
 * improvement with rterm-exon cutting
 *
 * Revision 1.15  2004/06/16 21:02:43  kapustin
 * Set lower length limit for gaps treated as poly-A parts
 *
 * Revision 1.14  2004/06/07 13:46:46  kapustin
 * Rearrange seg-level postprocessing steps
 *
 * Revision 1.13  2004/06/03 19:27:54  kapustin
 * Add CSplign::GetIdentity(). Limit the genomic extension for small
 * terminal exons
 *
 * Revision 1.12  2004/05/24 16:13:57  gorelenk
 * Added PCH ncbi_pch.hpp
 *
 * Revision 1.11  2004/05/19 13:37:48  kapustin
 * Remove test dumping code
 *
 * Revision 1.10  2004/05/18 21:43:40  kapustin
 * Code cleanup
 *
 * Revision 1.9  2004/05/04 15:23:45  ucko
 * Split splign code out of xalgoalign into new xalgosplign.
 *
 * Revision 1.8  2004/05/03 15:22:18  johnson
 * added typedefs for public stl types
 *
 * Revision 1.7  2004/04/30 15:00:47  kapustin
 * Support ASN formatting
 *
 * Revision 1.6  2004/04/26 15:38:45  kapustin
 * Add model_id as a CSplign member
 *
 * Revision 1.5  2004/04/23 18:43:47  ucko
 * <cmath> -> <math.h>, since some older compilers (MIPSpro) lack the wrappers.
 *
 * Revision 1.4  2004/04/23 16:52:04  kapustin
 * Change the way we get donor address
 *
 * Revision 1.3  2004/04/23 14:37:44  kapustin
 * *** empty log message ***
 *
 * ===========================================================================
 */
