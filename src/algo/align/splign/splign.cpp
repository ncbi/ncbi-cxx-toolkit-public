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


#include "splign_compartment_finder.hpp"
#include "splign_util.hpp"

#include <algo/align/splign/splign.hpp>
#include <algo/align/nw_formatter.hpp>
#include <algo/align/align_exception.hpp>

#include <deque>
#include <math.h>
#include <algorithm>

BEGIN_NCBI_SCOPE


CSplign::CSplign( void )
{
    m_min_query_coverage = 0.5;
    m_compartment_penalty = 0.75;
    m_minidty = 0.75;
    m_endgaps = true;
    m_strand = true;
    m_max_genomic_ext = 100000;
    m_nopolya = false;
    m_model_id = 0;
}


void CSplign::SetAligner( CRef<CSplicedAligner>& aligner ) {
    m_aligner = aligner;
}

const CRef<CSplicedAligner>& CSplign::GetAligner( void ) const {
    return m_aligner;
}

void CSplign::SetSeqAccessor( CRef<CSplignSeqAccessor>& sa ) {
    m_sa = sa;
}

const CRef<CSplignSeqAccessor>& CSplign::GetSeqAccessor( void ) const {
    return m_sa;
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
        NCBI_THROW( CAlgoAlignException,
                    eBadParameter,
                    "Identity threshold must be between 0 and 1" );
    }
    else {
        m_minidty = idty;
    }
}

double CSplign::GetMinExonIdentity( void ) const {
    return m_minidty;
}


void CSplign::SetMaxGenomicExtension( size_t ext ) {
    m_max_genomic_ext = ext;
}

size_t CSplign::GetMaxGenomicExtension( void ) const {
    return m_max_genomic_ext;
}

void CSplign::SetMinQueryCoverage( double cov )
{
    if(cov < 0 || cov > 1) {
        NCBI_THROW( CAlgoAlignException,
                    eBadParameter,
                    "Min query coverage out of range");
    }
    m_min_query_coverage = cov;
}

double CSplign::GetMinQueryCoverage( void ) const 
{
    return m_min_query_coverage;
}

void CSplign::SetCompartmentPenalty(double penalty)
{
    if(penalty < 0 || penalty > 1) {
        NCBI_THROW( CAlgoAlignException,
                    eBadParameter,
                    "Min query coverage out of range");
    }
    m_compartment_penalty = penalty;
}

double CSplign::GetCompartmentPenalty( void ) const 
{
    return m_compartment_penalty;
}

void CSplign::x_SetPattern(THits* hits)
{  
    sort(hits->begin(), hits->end(), CHit::PPrecedeQ);
    vector<size_t> pattern0;
    for(size_t i = 0, n = hits->size(); i < n; ++i) {
        const CHit& h = (*hits)[i];
        if(1 + h.m_ai[1] - h.m_ai[0] >= 10) {
            pattern0.push_back( h.m_ai[0] );
            pattern0.push_back( h.m_ai[1] );
            pattern0.push_back( h.m_ai[2] );
            pattern0.push_back( h.m_ai[3] );
        }
    }

    const char* Seq1 = &m_mrna.front();
    const size_t SeqLen1 = m_polya_start < kMax_UInt?
        m_polya_start: m_mrna.size();
    const char* Seq2 = &m_genomic.front();
    const size_t SeqLen2 = m_genomic.size();
    
    size_t dim = pattern0.size();
    const char* err = 0;
    if(dim % 4 == 0) {
        for(size_t i = 0; i < dim; i += 4) {
            
            if(pattern0[i] > pattern0[i+1] || pattern0[i+2] > pattern0[i+3]) {
                err = "Pattern hits must be specified in plus strand";
                break;
            }
            
            if(i > 4) {
                if(pattern0[i]<=pattern0[i-3] || pattern0[i+2]<=pattern0[i-2]){
                    err = "Pattern hits coordinates must be sorted";
                    break;
                }
            }
            
            if(pattern0[i+1] >= SeqLen1 || pattern0[i+3] >= SeqLen2) {
                err = "One or several pattern hits are out of range";
                break;
            }
        }
    }
    else {
        err = "Pattern must have a dimension multiple of four";
    }
    
    if(err) {
        NCBI_THROW( CAlgoAlignException, eBadParameter, err );
    }
    else {
        m_alnmap.clear();
        m_pattern.clear();
        
        // copy from pattern0 to pattern so that each hit is not too large
        const size_t max_len = kMax_UInt; // turn this off: sometimes we really
                                          // need just the longest perf match
        vector<size_t> pattern;
        for(size_t i = 0; i < dim; i += 4) {
            size_t lenq = 1 + pattern0[i+1] - pattern0[i];
            if(lenq <= max_len) {
                copy(pattern0.begin() + i,
                     pattern0.begin() + i + 4,
                     back_inserter(pattern));
            }
            else {
                const size_t d = (lenq-1) / max_len + 1;
                const size_t inc = lenq / d + 1;
                for(size_t a = pattern0[i], b = a , c = pattern0[i+2], d = c;
                    a < pattern0[i+1]; (a = b + 1), (c = d + 1) ) {
                    b = a + inc - 1;
                    d = c + inc - 1;
                    if(b > pattern0[i+1] || d > pattern0[i+3]) {
                        b = pattern0[i+1];
                        d = pattern0[i+3];
                    }
                    pattern.push_back(a);
                    pattern.push_back(b);
                    pattern.push_back(c);
                    pattern.push_back(d);
                }
            }
        }
        
        dim = pattern.size();
        
        SAlnMapElem map_elem;
        map_elem.m_box[0] = map_elem.m_box[2] = 0;
        map_elem.m_pattern_start = map_elem.m_pattern_end = -1;
        
        // realign pattern hits and build the alignment map
        for(size_t i = 0; i < dim; i += 4) {    
            
            CNWAligner nwa ( Seq1 + pattern[i],
                             pattern[i+1] - pattern[i] + 1,
                             Seq2 + pattern[i+2],
                             pattern[i+3] - pattern[i+2] + 1 );
            nwa.Run();
            
            size_t L1, R1, L2, R2;
            const size_t max_seg_size = nwa.GetLongestSeg(&L1, &R1, &L2, &R2);
            if(max_seg_size) {
                
                const size_t hitlen_q = pattern[i + 1] - pattern[i] + 1;
                const size_t hlq3 = hitlen_q/3;
                const size_t sh = hlq3; // hlq3 < 30? hlq3: 30;
                
                size_t delta = sh > L1? sh - L1: 0;
                size_t q0 = pattern[i] + L1 + delta;
                size_t s0 = pattern[i+2] + L2 + delta;
                
                const size_t h2s_right = hitlen_q - R1 - 1;
                delta = sh > h2s_right? sh - h2s_right: 0;
                size_t q1 = pattern[i] + R1 - delta;
                size_t s1 = pattern[i+2] + R2 - delta;
                
                if(q0 > q1 || s0 > s1) { // longest seg was probably too short
                    q0 = pattern[i] + L1;
                    s0 = pattern[i+2] + L2;
                    q1 = pattern[i] + R1;
                    s1 = pattern[i+2] + R2;
                }
                
                m_pattern.push_back(q0); m_pattern.push_back(q1);
                m_pattern.push_back(s0); m_pattern.push_back(s1);
                
                const size_t pattern_dim = m_pattern.size();
                if(map_elem.m_pattern_start == -1) {
                    map_elem.m_pattern_start = pattern_dim - 4;;
                }
                map_elem.m_pattern_end = pattern_dim - 1;
            }
            
            map_elem.m_box[1] = pattern[i+1];
            map_elem.m_box[3] = pattern[i+3];
        }
        
        map_elem.m_box[1] = SeqLen1 - 1;
        map_elem.m_box[3] = SeqLen2 - 1;
        m_alnmap.push_back(map_elem);
    }
}


void CSplign::Run( THits* phits )
{
    if(!phits) {
        NCBI_THROW( CAlgoAlignException,
                    eInternal,
                    "Unexpected NULL pointers" );
    }

    THits& hits = *phits;
  
    if(m_sa.IsNull()) {
        NCBI_THROW( CAlgoAlignException,
                    eNotInitialized,
                    "Sequence accessor object not specified" );
    }
  
    if(m_aligner.IsNull()) {
      NCBI_THROW( CAlgoAlignException,
		  eNotInitialized,
		  "Spliced aligner object not specified" );
    }

    if(hits.size() == 0) {
      NCBI_THROW( CAlgoAlignException,
		  eNoData,
		  "Empty hit vector passed to CSplign" );
    }

    const string query ( hits[0].m_Query );
    const string subj  ( hits[0].m_Subj );

    m_result.clear();

    // pre-load the spliced sequence and calculate min coverage
    m_mrna.clear();
    m_sa->Load(query, &m_mrna, 0, kMax_UInt);
    const size_t mrna_size = m_mrna.size();
    const size_t min_coverage = size_t(m_min_query_coverage * mrna_size);
    const size_t comp_penalty_bps = size_t(m_compartment_penalty * mrna_size);
    
    // iterate through compartments
    CCompartmentAccessor comps (hits.begin(), hits.end(),
                                comp_penalty_bps, min_coverage);
    THits comp_hits;

    size_t smin = 0, smax = kMax_UInt;
    bool same_strand = false;

    for(size_t i = 0, dim = comps.GetCount(); i < dim; ++i) {

      comps.Get(i, comp_hits);
      
      // limit the space beyond the compartment
      // to avoid shared exons

      if(i+1 == dim) {
        smax = kMax_UInt;
        same_strand = false;
      }
      else {
        
        bool strand_this = comps.GetStrand(i);
        bool strand_next = comps.GetStrand(i+1);
        same_strand = strand_this == strand_next;

        if(same_strand) {

          const size_t* box0  = comps.GetBox(i);
          const size_t* box1  = box0 + 4;
          const size_t  dist  = box1[2] - box0[3];
          
          size_t qdim0, qdim1;
          if(strand_this) {
            qdim0 = mrna_size - box0[1] - 1;
            qdim1 = box1[0];
          }
          else {
            qdim0 = box0[0];
            qdim1 = mrna_size - box1[1] - 1;
          }
          
          const size_t  qdim = qdim0 + qdim1;
          
          if(qdim == 0) {
            smax = box1[2] - 1; 
          }
          else {
            const double a0 = double(qdim0) / qdim;
            smax = size_t(floor (box0[3] + a0*dist));
            if(smax == box1[2]) {
              --smax;
            }
          }
        }
        else { // !same_strand
          smax = kMax_UInt;
        }
      }
     
      try {
        SAlignedCompartment ac = x_RunOnCompartment(&comp_hits, smin, smax);
        ac.m_id = ++m_model_id;
        ac.m_segments = m_segments;
        ac.m_error = false;
        ac.m_msg = "Ok";
        m_result.push_back(ac);
      }
      catch(CException& e) {
        m_result.push_back(SAlignedCompartment(0, true, e.GetMsg().c_str()));
      }

      smin = same_strand? (smax + 1): 0;
    }
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


CSplign::SAlignedCompartment CSplign::x_RunOnCompartment( THits* hits,
                                                          size_t range_left,
                                                          size_t range_right )
{
  SAlignedCompartment rv;
  m_segments.clear();

  if(range_left > range_right) {
    NCBI_THROW( CAlgoAlignException, eInternal, "Invalid range data");
  }

  XFilter(hits);

  if(hits->size() == 0) {
    NCBI_THROW( CAlgoAlignException, eNoData,
		"No hits left after filtering");
  }

  const string query ( hits->front().m_Query );
  const string subj  ( hits->front().m_Subj );

  const size_t mrna_size = m_mrna.size();  

  if( !m_strand ) {

    // make reverse complimentary
    reverse (m_mrna.begin(), m_mrna.end());
    transform(m_mrna.begin(), m_mrna.end(), m_mrna.begin(),
	      SCompliment());

    // adjust the hits
    for(size_t i = 0, n = hits->size(); i < n; ++i) {
      CHit& h = (*hits)[i];
      bool plus = h.IsStraight();
      size_t a0 = mrna_size - h.m_ai[0] + 1;
      size_t a1 = mrna_size - h.m_ai[1] + 1;
      h.m_an[0] = h.m_ai[0] = a1;
      h.m_an[1] = h.m_ai[1] = a0;
      // change strand
      if(plus) {
	h.m_an[2] = h.m_ai[3];
	h.m_an[3] = h.m_ai[2];
      }
      else {
	h.m_an[2] = h.m_ai[2];
	h.m_an[3] = h.m_ai[3];
      }
    }
  }

  m_polya_start = m_nopolya? kMax_UInt: x_TestPolyA();

  if(m_polya_start < kMax_UInt) {
    CleaveOffByTail(hits, m_polya_start + 1); // cleave off hits beyond cds
    if(hits->size() == 0) {
      NCBI_THROW( CAlgoAlignException,
                  eNoData,
                  "No hits found beyond Poly(A), if any");
    }
  }

  // find regions of interest on mRna (query)
  // and contig (subj)
  size_t qmin, qmax, smin, smax;
  GetHitsMinMax(*hits, &qmin, &qmax, &smin, &smax);
  --qmin; --qmax; --smin; --smax;

  qmin = 0;
  qmax = m_polya_start < kMax_UInt? m_polya_start - 1: mrna_size - 1;
  smin = max(0, int(smin - m_max_genomic_ext));
  smax += m_max_genomic_ext;
 
  if(smin < range_left) {
    smin = range_left;
  }
  if(smax > range_right) {
    smax = range_right;
  }
  
  bool ctg_strand = (*hits)[0].IsStraight();

  m_genomic.clear();
  m_sa->Load(subj, &m_genomic, smin, smax);

  const size_t ctg_end = smin + m_genomic.size();
  if(ctg_end - 1 < smax) { // perhabs adjust smax
    smax = ctg_end - 1;
  }

  if(!ctg_strand) {
    // make reverse complementary
    // for the contig's area of interest
    reverse (m_genomic.begin(), m_genomic.end());
    transform(m_genomic.begin(), m_genomic.end(), m_genomic.begin(),
	      SCompliment());
    // flip the hits
    for(size_t i = 0, n = hits->size(); i < n; ++i) {
      CHit& h = (*hits)[i];
      size_t a2 = smax - (h.m_ai[3] - smin) + 2;
      size_t a3 = smax - (h.m_ai[2] - smin) + 2;
      h.m_an[2] = h.m_ai[2] = a2;
      h.m_an[3] = h.m_ai[3] = a3;
    }
  }

  rv.m_QueryStrand = m_strand;
  rv.m_SubjStrand  = ctg_strand;
  rv.m_qmin = qmin;
  rv.m_smin = smin;
  rv.m_smax = smax;
  rv.m_mrnasize = mrna_size;

  // shift hits so that they originate from qmin, smin;
  // also make them zero-based
  for(size_t i = 0, n = hits->size(); i < n; ++i) {
    CHit& h = (*hits)[i];
    h.m_an[0] = h.m_ai[0] -= qmin + 1;
    h.m_an[1] = h.m_ai[1] -= qmin + 1;
    h.m_an[2] = h.m_ai[2] -= smin + 1;
    h.m_an[3] = h.m_ai[3] -= smin + 1;
  }  
  
  x_SetPattern( hits );
  x_Run(&m_mrna.front(), &m_genomic.front());

  const size_t seg_dim = m_segments.size();
  if(seg_dim == 0) {
    NCBI_THROW( CAlgoAlignException, eNoData, "No alignment found.");
  }

  // try to extend the last segment into the PolyA area  
  if(m_polya_start < kMax_UInt && seg_dim && m_segments[seg_dim-1].m_exon) {
    CSplign::SSegment& s = const_cast<CSplign::SSegment&>(
				       m_segments[seg_dim-1]);
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
      s.RestoreIdentity();

      // correct annotation
      const size_t ann_dim = s.m_annot.size();
      if(ann_dim > 2 && s.m_annot[ann_dim - 3] == '>') {
          ++q;
          s.m_annot[ann_dim - 2] = q < qe? *q: ' ';
          ++q;
          s.m_annot[ann_dim - 1] = q < qe? *q: ' ';
      }

      m_polya_start += sh;
    }
  }

  int j = seg_dim - 1;

  // look for PolyA in trailing segments:
  // if a segment is mostly 'A's then we add it to PolyA
  for(; j >= 0; --j) {

    const CSplign::SSegment& s = m_segments[j];
    const char* p0 = &m_mrna[qmin] + s.m_box[0];
    const char* p1 = &m_mrna[qmin] + s.m_box[1] + 1;
    size_t count = 0;
    for(const char* pc = p0; pc != p1; ++pc) {
      if(*pc == 'A') ++count;
    }
    const size_t len = p1 - p0;

    double min_a_content = 0.799;
    // also check splices
    if(s.m_exon && j > 0 && m_segments[j-1].m_exon) {
      if(!IsConsensus(m_segments[j-1].GetDonor(), s.GetAcceptor())) {
        min_a_content = 0.599;
      }
    }
    if(!s.m_exon) {
        min_a_content = 0.599;
    }

    if(double(count)/len < min_a_content) {
        break;
    }
  }

  if(j >= 0 && j < int(seg_dim - 1)) {
    m_polya_start = m_segments[j].m_box[1] + 1;
  }

  // test if we have at least one exon
  bool some_exons = false;
  for(int i = 0; i <= j; ++i ) {
    if(m_segments[i].m_exon) {
      some_exons = true;
      break;
    }
  }
  if(!some_exons) {
    NCBI_THROW( CAlgoAlignException, eNoData,
                "No exons found above identity limit.");
  }

  m_segments.resize(j + 1);

  return rv;
}


void CSplign::x_Run(const char* Seq1, const char* Seq2)
{
    deque<SSegment> segments;

    for(size_t i = 0, map_dim = m_alnmap.size(); i < map_dim; ++i) {

        const SAlnMapElem& zone = m_alnmap[i];

        // setup sequences
        m_aligner->SetSequences(Seq1 + zone.m_box[0],
            zone.m_box[1] - zone.m_box[0] + 1,
            Seq2 + zone.m_box[2],
            zone.m_box[3] - zone.m_box[2] + 1,
            false);

        // prepare the pattern
        vector<size_t> pattern;
        if(zone.m_pattern_start >= 0) {

            copy(m_pattern.begin() + zone.m_pattern_start,
            m_pattern.begin() + zone.m_pattern_end + 1,
            back_inserter(pattern));
            for(size_t j = 0, pt_dim = pattern.size(); j < pt_dim; j += 4) {


// #define DBG_DUMP_PATTERN
#ifdef  DBG_DUMP_PATTERN
	      cerr << pattern[j] << '\t' << pattern[j+1] << '\t'
		   << pattern[j+2] << '\t' << pattern[j+3] << endl;
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
            
            // align
            m_aligner->Run();

// #define DBG_DUMP_TYPE2
#ifdef  DBG_DUMP_TYPE2
            {{
            CNWFormatter fmt (*m_aligner);
            string txt;
            fmt.AsText(&txt, CNWFormatter::eFormatType2);
            cerr << txt;
            }}  
#endif

            // create list of segments
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
                e.m_idty = idty;
                e.m_len = size;
                e.m_box[0] = q0; e.m_box[1] = q1;
                e.m_box[2] = s0; e.m_box[3] = s1;
                e.m_annot = txt;
                e.m_details = repr;
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
                g.m_annot = "<GAP>";
                g.m_details.append(Seq1+g.m_box[0], g.m_box[1]-g.m_box[0]+1);
                segments.push_back(g);
            }
        }
    } // zone iterations end

    // segment-level postprocessing

    size_t seg_dim = segments.size();
    if(seg_dim == 0) {
        return;
    }

    // First go from the ends and see if we
    // can improve boundary exons
    size_t k0 = 0;
    while(k0 < seg_dim) {
        SSegment& s = segments[k0];
        if(s.m_exon) {
            if(s.m_idty < m_minidty || m_endgaps) {
                s.ImproveFromLeft(Seq1, Seq2);
            }
            if(s.m_idty >= m_minidty) {
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
        g.m_annot = "<GAP>";
        g.m_details = string(Seq1 + g.m_box[0], g.m_box[1] + 1 - g.m_box[0]);
        ++seg_dim;
        ++k0;
    }

    int k1 = int(seg_dim - 1);
    while(k1 >= int(k0)) {
        SSegment& s = segments[k1];
        if(s.m_exon) {
            if(s.m_idty < m_minidty || m_endgaps) {
	            s.ImproveFromRight(Seq1, Seq2);
            }
            if(s.m_idty >= m_minidty) {
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
        g.m_annot = "<GAP>";
        g.m_details = string(Seq1 + g.m_box[0], g.m_box[1] + 1 - g.m_box[0]);
        segments.push_back(g);
        ++seg_dim;
    }

    // turn to gaps exons with low identity
    for(size_t k = 0; k < seg_dim; ++k) {
        SSegment& s = segments[k];
        if(s.m_exon && s.m_idty < m_minidty) {
            s.m_exon = false;
            s.m_idty = 0;
            s.m_len = s.m_box[1] - s.m_box[0] + 1;
            s.m_annot = "<GAP>";
            s.m_details.resize(0);
            s.m_details.append(Seq1 + s.m_box[0],
			       s.m_box[1] + 1 - s.m_box[0]);
        }
    }

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
            s.m_annot = "<GAP>";
            s.m_details.resize(0);
            s.m_details.append(Seq1 + s.m_box[0],
			       s.m_box[1] + 1 - s.m_box[0]);
	  }
	  gap_prev = false;
	}
    }

    // now merge all adjacent gaps
    int gap_start_idx = -1;
    if(segments.size() && segments[0].m_exon == false) {
        gap_start_idx = 0;
    }
    size_t segs_dim = segments.size();
    for(size_t k = 0; k < segs_dim; ++k) {
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
               g.m_details.append(Seq1 + g.m_box[0],
				  g.m_box[1] + 1 - g.m_box[0]);
               m_segments.push_back(g);
               gap_start_idx = -1;
           }
           m_segments.push_back(s);
        } 
    }
    if(gap_start_idx >= 0) {
        SSegment& g = segments[gap_start_idx];
        g.m_box[1] = segments[segs_dim-1].m_box[1];
        g.m_box[3] = segments[segs_dim-1].m_box[3];
        g.m_len = g.m_box[1] - g.m_box[0] + 1;
        g.m_details.resize(0);
        g.m_details.append(Seq1 + g.m_box[0], g.m_box[1] + 1 - g.m_box[0]);
        m_segments.push_back(g);
    }
}


// try improving the segment by cutting it from the left
void CSplign::SSegment::ImproveFromLeft(const char* seq1, const char* seq2)
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
    m_details.erase(0, m_details.size() - (irs_max - irs0 + 1));
    m_details.insert(m_details.begin(), head, 'M');
    RestoreIdentity();
    
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
void CSplign::SSegment::ImproveFromRight(const char* seq1, const char* seq2)
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
    RestoreIdentity();
    
    // update the last two annotation chars
    const size_t adim = m_annot.size();
    if(adim > 2 && m_annot[adim - 3] == '>') {
      m_annot[adim-2] = seq2[m_box[3] + 1];
      m_annot[adim-1] = seq2[m_box[3] + 2];
    }
  }
}


void CSplign::SSegment::RestoreIdentity()
{
    // restore length and identity
    m_len = m_details.size();
    string::const_iterator ib = m_details.begin(), ie = m_details.end();
    size_t count = 0; // not using std::count here due to known incompatibilty
    for(string::const_iterator ii = ib; ii != ie; ++ii) {
        if(*ii == 'M') ++count;
    }
    m_idty = double(count) / m_len;
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


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
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
