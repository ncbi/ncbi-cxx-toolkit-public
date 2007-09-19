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

#include <math.h>
#include <algorithm>

BEGIN_NCBI_SCOPE

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
}

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
    m_MinPatternHitLength = 13;
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
                NCBI_THROW(CAlgoAlignException,
                           eNoSeqData, 
                           string("Sequence is empty: ") 
                           + seqid.AsFastaString());
            }
            if(finish > dim) {
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
        
        if(retain == false && m_CanResetHistory) {
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
    const size_t max_intron (1u << 20);
    size_t prev (0);
    NON_CONST_ITERATE(THitRefs, ii, *phitrefs) {

        THitRef& h (*ii);

        if(h->GetQuerySpan() < m_MinPatternHitLength) {
            h.Reset(0);
            continue;
        }

        if(prev > 0) {
            
            const bool consistent (h->GetSubjStrand()?
                                  (h->GetSubjStart() < prev + max_intron):
                                  (h->GetSubjStart() + max_intron > prev));

            if(!consistent) {
                const string errmsg (g_msg_CompartmentInconsistent
                                     + string(" (extra long introns)"));
                NCBI_THROW(CAlgoAlignException, eIntronTooLong, errmsg);
            }
        }
        prev = h->GetSubjStop();
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
            const bool imprf  (idty < 1.00 || h->GetQuerySpan() != h->GetSubjSpan());
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
    bool bad_input (false);
    if(dim % 4 == 0) {

        for(size_t i = 0; i < dim; i += 4) {
            
            if(pattern0[i] > pattern0[i+1] || pattern0[i+2] > pattern0[i+3]) {
                ostr_err << "Pattern hits must be specified in plus strand";
                bad_input = true;
                break;
            }
            
            if(i > 4) {
                if(pattern0[i] <= pattern0[i-3] || pattern0[i+2] <= pattern0[i-1]) {
                    ostr_err << g_msg_CompartmentInconsistent
                             << string(" (hits not sorted)");
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
                break;
            }
        }

    }
    else {
        ostr_err << "Pattern dimension must be a multiple of four";
        bad_input = true;
    }
    
    if(bad_input) {
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
                const size_t sh (hitlen_q / 4);
                
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

        USING_SCOPE(objects);

        vector<char> seq;
        if(seq_data == 0) {
            x_LoadSequence(&seq, *id, 0, numeric_limits<THit::TCoord>::max(),false);
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

    const TSeqPos mrna_size (objects::sequence::GetLength(*id_query, m_Scope));
    if(mrna_size == numeric_limits<TSeqPos>::max()) {
        NCBI_THROW(CAlgoAlignException, eNoSeqData, 
                   string("Sequence not found: ") + id_query->AsFastaString());
    }
    
    const TSeqPos comp_penalty_bps = size_t(m_compartment_penalty * mrna_size);
    const TSeqPos min_matches = size_t(m_MinCompartmentIdty * mrna_size);
    const TSeqPos min_singleton_matches = size_t(m_MinSingletonIdty * mrna_size);

    // iterate through compartments
    CCompartmentAccessor<THit> comps ( hitrefs.begin(), hitrefs.end(),
                                       comp_penalty_bps,
                                       min_matches,
                                       min_singleton_matches,
                                       true );

    pair<size_t,size_t> dim (comps.GetCounts()); // (count_total, count_unmasked)
    if(dim.second > 0) {
 
        // pre-load cDNA
        m_mrna.clear();
        x_LoadSequence(&m_mrna, *id_query, 0, 
                       numeric_limits<THit::TCoord>::max(), false);

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
                smax = same_strand? (box+4)[2] - 1: kMax_UInt;
            }
     
            try {
                
                THitRefs comp_hits;
                comps.Get(i, comp_hits);
            
                if(comps.GetStatus(i)) {
            
                    SAlignedCompartment ac (x_RunOnCompartment(&comp_hits,smin,smax));

                    ac.m_Id = ++m_model_id;
                    ac.m_Segments = m_segments;
                    ac.m_Status = SAlignedCompartment::eStatus_Ok;
                    ac.m_Msg = "Ok";
                    ac.m_Cds_start = m_cds_start;
                    ac.m_Cds_stop = m_cds_stop;
                    ac.m_QueryLen = m_mrna.size();
                    ac.m_PolyA = (m_polya_start < kMax_UInt? m_polya_start : 0);
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

            smin = same_strand? box[3] + 1: 0;
        }
    }
}


bool CSplign::AlignSingleCompartment(THitRefs* phitrefs,
                                     size_t subj_min, size_t subj_max,
                                     SAlignedCompartment* result)
{
    m_mrna.resize(0);

    THit::TId id_query (phitrefs->front()->GetQueryId());

    x_LoadSequence(&m_mrna, *id_query, 0, 
                   numeric_limits<THit::TCoord>::max(), false);

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

        ac.m_Id = ++m_model_id;
        ac.m_Segments = m_segments;
        ac.m_Status = SAlignedCompartment::eStatus_Ok;
        ac.m_Msg = "Ok";
        ac.m_Cds_start = m_cds_start;
        ac.m_Cds_stop = m_cds_stop;
        ac.m_QueryLen = m_mrna.size();
        ac.m_PolyA = (m_polya_start < kMax_UInt? m_polya_start : 0);

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
    
        m_polya_start = m_nopolya? kMax_UInt: x_TestPolyA();
    
        if(m_polya_start < kMax_UInt) {

            // cleave off hits beyond cds
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
                                      m_polya_start: mrna_size) - qmax + 1);
        const THit::TCoord extent_right_m1 (x_GetGenomicExtent(rspace));
        
        // m2: estimate genomic extents using compartment hits
        THit::TCoord fixed_left (numeric_limits<THit::TCoord>::max() / 2), 
                     fixed_right(fixed_left);

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
        const THit::TCoord extent_right (min(extent_right_m1, extent_right_m2));

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
    
        const THit::TCoord ctg_end (smin + m_genomic.size());
        if(ctg_end - 1 < smax) { // adjust smax if beyond the end
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
        rv.m_Score = x_Run(&m_mrna.front(), &m_genomic.front());

        const size_t seg_dim (m_segments.size());
        if(seg_dim == 0) {
            NCBI_THROW(CAlgoAlignException, eNoAlignment, g_msg_NoAlignment);
        }

        // try to extend the last segment into the PolyA area  
        if(m_polya_start < kMax_UInt && seg_dim && m_segments[seg_dim-1].m_exon) {

            TSegment& s (const_cast<TSegment&>(m_segments[seg_dim-1]));
            const char* p0 = &m_mrna.front() + s.m_box[1] + 1;
            const char* q = &m_genomic.front() + s.m_box[3] + 1;
            const char* p = p0;
            const char* pe = &m_mrna.front() + mrna_size;
            const char* qe = &m_genomic.front() + m_genomic.size();
            for(; p < pe && q < qe; ++p, ++q) {
                if(*p != 'A') break;
                if(*p != *q) break;
            }
        
            const size_t sh (p - p0);
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

        int j (seg_dim - 1), j0 (j);
        for(; j >= 0; --j) {
        
            const TSegment& s (m_segments[j]);
            const char* p0 (&m_mrna[qmin] + s.m_box[0]);
            const char* p1 (&m_mrna[qmin] + s.m_box[1] + 1);
            const size_t len_chars (p1 - p0);
            size_t count (0);
            for(const char* pc (p0); pc < p1; ++pc) {
                if(*pc == 'A') ++count;
            }

            double min_a_content (0.76); // min 'A' content in a polyA
            // also check splices
            if(s.m_exon && j > 0 && m_segments[j-1].m_exon) {

                bool consensus (TSegment::s_IsConsensusSplice(
                                m_segments[j-1].GetDonor(), s.GetAcceptor()));
                if(!consensus || len_chars <= 6) {
                    min_a_content = 0.599;
                }
                if(len_chars < 3) {
                    min_a_content = 0.49;
                }
            }

            if(!s.m_exon) {
                min_a_content = (s.m_len <= 4)? 0.49: 0.599;
            }

            if(double(count)/len_chars < min_a_content) {
                if(s.m_exon && s.m_len > 4) break;
            }
            else {
                j0 = j - 1;
            }
        }

        if(j0 >= 0 && j0 < int(seg_dim - 1)) {
            m_polya_start = m_segments[j0].m_box[1] + 1;
        }

        // test if we have at least one exon before poly(a)
        bool some_exons (false);
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
        double mcount (0);
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


static const char s_kGap [] = "<GAP>";

// at this level and below, plus strand is assumed for both sequences
CSplign::TAligner::TScore CSplign::x_Run(const char* Seq1, const char* Seq2)
{
    typedef deque<TSegment> TSegmentDeque;
    TSegmentDeque segments;

//#define DBG_DUMP_PATTERN
#ifdef  DBG_DUMP_PATTERN
    cerr << "Pattern:" << endl;  
#endif

    TAligner::TScore rv (0);

    for(size_t i = 0, map_dim = m_alnmap.size(); i < map_dim; ++i) {

        const SAlnMapElem& zone = m_alnmap[i];

        // setup sequences
        const size_t len1 (zone.m_box[1] - zone.m_box[0] + 1);
        const size_t len2 (zone.m_box[3] - zone.m_box[2] + 1);
        
        m_aligner->SetSequences(
            Seq1 + zone.m_box[0], len1,
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
        
        for(size_t j = 0, pt_dim = pattern.size(); j < pt_dim; j += 4) {

#ifdef  DBG_DUMP_PATTERN
            cerr << pattern[j] << '\t' << pattern[j+1] << '\t'
                 << "(len = " << (pattern[j+1] - pattern[j] + 1) << ")\t"
                 << pattern[j+2] << '\t' << pattern[j+3] 
                 << "(len = " << (pattern[j+3] - pattern[j+2] + 1) << ")\t"
                 << endl;
#undef DBG_DUMP_PATTERN
#endif

            pattern[j]   -= zone.m_box[0];
            pattern[j+1] -= zone.m_box[0];
            pattern[j+2] -= zone.m_box[2];
            pattern[j+3] -= zone.m_box[2];
        }

        // setup esf
        m_aligner->SetPattern(pattern);
        m_aligner->SetEndSpaceFree(true, true, true, true);
        if(m_strand) {
            m_aligner->SetCDS(m_cds_start, m_cds_stop);
        }
        else {
            m_aligner->SetCDS(len1 - m_cds_start - 1, len1 - m_cds_stop - 1);
        }

        rv += m_aligner->Run();

//#define DBG_DUMP_TYPE2
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
        }
    } // zone iterations end


//#define DUMP_ORIG_SEGS
#ifdef DUMP_ORIG_SEGS
    cerr << "Orig segments:" << endl;
    ITERATE(TSegmentDeque, ii, segments) {
        cerr << ii->m_box[0] << '\t' << ii->m_box[1] << '\t'
             << ii->m_box[2] << '\t' << ii->m_box[3] << '\t'
             << ii->m_annot << '\t' << ii->m_score << endl;
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
                ii->m_exon = false;
                ii->m_idty = 0;
                ii->m_details.resize(0);
                ii->m_annot = s_kGap;
                ii->m_score = 0;
            }
        }
    }
    
    m_segments.resize(0);
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

        // Go from the ends and see if we can improve term exons
        size_t k0 (0);
        while(k0 < seg_dim) {

            TSegment& s = segments[k0];
            if(s.m_exon) {

                const size_t len (1 + s.m_box[1] - s.m_box[0]);
                const double min_idty (len >= kMinTermExonSize?
                                       m_MinExonIdty:
                                       max(m_MinExonIdty, kMinTermExonIdty));
                
                if(s.m_idty < min_idty || m_endgaps) {
                    s.ImproveFromLeft(Seq1, Seq2, m_aligner);
                }

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

                if(s.m_idty < min_idty || m_endgaps) {
                    s.ImproveFromRight(Seq1, Seq2, m_aligner);
                }

                if(s.m_idty >= min_idty) {
                    break;
                }
            }
            --k1;
        }

        // turn to gaps exons with low identity
        for(size_t k = 0; k < seg_dim; ++k) {
            TSegment& s = segments[k];
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

        // turn to gaps extra-short exons preceeded/followed by gaps
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

        // indicate any slack space on the left
        if(segments[0].m_box[0] > 0) {
            
            TSegment g;
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

            segments.push_front(g);
            ++seg_dim;
        }

        // same on the right
        TSegment& seg_last (segments.back());

        if(seg_last.m_box[1] + 1 < SeqLen1) {
            
            TSegment g;
            g.m_exon = false;
            g.m_box[0] = seg_last.m_box[1] + 1;
            g.m_box[1] = SeqLen1 - 1;
            g.m_box[2] = seg_last.m_box[3] + 1;
            g.m_box[3] = SeqLen2 - 1;
            g.m_idty = 0;
            g.m_len = g.m_box[1] - g.m_box[0] + 1;
            g.m_annot = s_kGap;
            g.m_details.resize(0);
            g.m_score = 0;

            segments.push_back(g);
            ++seg_dim;
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

//#define DUMP_PROCESSED_SEGS
#ifdef DUMP_PROCESSED_SEGS
    cerr << "Processed segments:" << endl;
    ITERATE(TSegments, ii, m_segments) {
        cerr << ii->m_box[0] << '\t' << ii->m_box[1] << '\t'
             << ii->m_box[2] << '\t' << ii->m_box[3] << '\t'
             << ii->m_annot << '\t' << ii->m_score << endl;
    }
#endif

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

    if(exon_size < kMinTermExonSize) {

        const double idty (term_segs[0]->m_idty);
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

            const size_t intron_len (b - a);

            const bool consensus (TSegment::s_IsConsensusSplice(dnr, acc));

            size_t max_ext ((idty < .96 || !consensus || exon_size < 16)? 
                            m_max_genomic_ext: (5000 *  kMinTermExonSize));

            if(!consensus && exon_size < 10) {
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
            s.m_exon = false;
            s.m_idty = 0;
            s.m_len = exon_size;
            s.m_annot = s_kGap;
            s.m_details.resize(0);
            s.m_score = 0;
        }
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


END_NCBI_SCOPE
