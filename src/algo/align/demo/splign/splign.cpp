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

#include <deque>

#include <algo/align/nw_formatter.hpp>

#include "splign.hpp"
#include "splign_app_exception.hpp"


BEGIN_NCBI_SCOPE

CSplign::CSplign(CSplicedAligner* aligner)
{
  m_aligner = aligner;
  m_minidty = 0;
  m_endgaps = false;
  m_Seq1 = m_Seq2 = 0;
  m_SeqLen1 = m_SeqLen2 = 0;
}


void CSplign::SetSequences(const char* seq1, size_t len1,
			   const char* seq2, size_t len2)
{
  if(!seq1 || !seq2) {
    NCBI_THROW(
	       CSplignException,
	       eBadParameter,
	       "NULL sequence pointer(s) passed");
  }
  m_Seq1 = seq1;
  m_SeqLen1 = len1;
  m_Seq2 = seq2;
  m_SeqLen2 = len2;
}


void CSplign::EnforceEndGapsDetection(bool enforce)
{
  m_endgaps = enforce;
}


void CSplign::SetPattern(const vector<size_t>& pattern)
{
  if(!m_Seq1 || !m_Seq2) {
    NCBI_THROW(
	       CSplignException,
	       eNotInitialized,
	       "Sequences must be set before pattern");
  }

  size_t dim = pattern.size();
  const char* err = 0;
  if(dim % 4 == 0) {
    for(size_t i = 0; i < dim; i += 4) {
      
      if( pattern[i] > pattern[i+1] || pattern[i+2] > pattern[i+3] ) {
	err = "Pattern hits must be specified in plus strand";
	break;
      }
      
      if(i > 4) {
	if(pattern[i] <= pattern[i-3] || pattern[i+2] <= pattern[i-2]){
	  err = "Pattern hits coordinates must be sorted";
	  break;
	}
      }
            
      if(pattern[i+1] >= m_SeqLen1 || pattern[i+3] >= m_SeqLen2) {
	err = "One or several pattern hits are out of range";
	break;
      }
    }
  }
  else {
    err = "Pattern must have a dimension multiple of four";
  }

  if(err) {
    NCBI_THROW( CSplignException, eBadParameter, err );
  }
  else {
    m_alnmap.clear();
    m_pattern.clear();

    SAlnMapElem map_elem;
    map_elem.m_box[0] = map_elem.m_box[2] = 0;
    map_elem.m_pattern_start = map_elem.m_pattern_end = -1;

    // realign pattern hits and build the alignment map
    for(size_t i = 0; i < dim; i += 4) {    

      if(i >= 4) {
	const size_t dist = pattern[i] - pattern[i-3] + 1;
	if(dist > 10) {
	  const size_t q0 = map_elem.m_box[1], q1 = pattern[i];
	  const size_t s0 = map_elem.m_box[3], s1 = pattern[i+2];
	  if(x_EvalMinExonIdty(q0, q1, s0, s1) < m_minidty) {
	    m_alnmap.push_back(map_elem);
	    map_elem.m_box[0] = pattern[i];
	    map_elem.m_box[2] = pattern[i+2];
	    map_elem.m_pattern_start = map_elem.m_pattern_end = -1;
	  }
	}
      }

      CNWAligner nwa ( m_Seq1 + pattern[i],
		       pattern[i+1] - pattern[i] + 1,
		       m_Seq2 + pattern[i+2],
		       pattern[i+3] - pattern[i+2] + 1 );
      nwa.Run();

      size_t L1, R1, L2, R2;
      const size_t max_seg_size = nwa.GetLongestSeg(&L1, &R1, &L2, &R2);
      if(max_seg_size) {

	const size_t hitlen_q = pattern[i + 1] - pattern[i] + 1;
	const size_t hlq4 = hitlen_q/4;
	const size_t sh = hlq4 < 30? hlq4: 30;

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

    map_elem.m_box[1] = m_SeqLen1 - 1;
    map_elem.m_box[3] = m_SeqLen2 - 1;
    m_alnmap.push_back(map_elem);
  }
}


void CSplign::SetMinExonIdentity(double idty)
{
  if(!(0 <= idty && idty <= 1)) {
    NCBI_THROW( CSplignException,
		eBadParameter,
		"Identity threshold must be between 0 and 1" );
  }
  else {
    m_minidty = idty;
  }
}

double CSplign::x_EvalMinExonIdty(size_t q0, size_t q1, size_t s0, size_t s1)
{
  // first estimate whether we want to evaluate it
  const size_t dimq = 1 + q1 - q0, dims = 1 + s1 - s0;
  if(double(dimq)*dims > 100000000) {
    return -1;
  }
  
  // pre-align the regions
  m_aligner->SetSequences(m_Seq1 + q0, dimq, m_Seq2 + s0, dims, false);
  m_aligner->SetEndSpaceFree(true, true, true, true);
  m_aligner->Run();
  CNWFormatter formatter(*m_aligner);
  string exons;
  formatter.AsText(&exons, CNWFormatter::eFormatExonTableEx);      
  CNcbiIstrstream iss_exons (exons.c_str());
  double min_idty = 1.;
  while(iss_exons) {
    string id1, id2, txt, repr;
    size_t i0, i1, j0, j1, size;
    double idty;
    iss_exons >> id1 >> id2 >> idty >> size
	      >> i0 >> i1 >> j0 >> j1 >> txt >> repr;
    if(!iss_exons) break;
    if(idty < min_idty) {
      min_idty = idty;
    }
  }

  return min_idty;
}


const vector<CSplign::SSegment>* CSplign::Run(void)
{
    if(!m_Seq1 || !m_Seq2) {
    NCBI_THROW( CSplignException,
        eNotInitialized,
        "One or both sequences not set" );
    }
    if(!m_aligner) {
    NCBI_THROW( CSplignException,
        eNotInitialized,
        "Spliced aligner object not set" );
    }

    m_out.clear();
    deque<SSegment> segments;
    for(size_t i = 0, map_dim = m_alnmap.size(); i < map_dim; ++i) {

        const SAlnMapElem& zone = m_alnmap[i];

        // setup sequences
        m_aligner->SetSequences(m_Seq1 + zone.m_box[0],
            zone.m_box[1] - zone.m_box[0] + 1,
            m_Seq2 + zone.m_box[2],
            zone.m_box[3] - zone.m_box[2] + 1,
            false);

        // prepare the pattern
        vector<size_t> pattern;
        if(zone.m_pattern_start >= 0) {

            copy(m_pattern.begin() + zone.m_pattern_start,
            m_pattern.begin() + zone.m_pattern_end + 1,
            back_inserter(pattern));
            for(size_t j = 0, pt_dim = pattern.size(); j < pt_dim; j += 4) {
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
                g.m_details.append(m_Seq1 + g.m_box[0],
                    g.m_box[1] - g.m_box[0] + 1);
                segments.push_back(g);
            }
        }
    }

    // Do some post-processing:
    // walk through exons and maybe turn some of
    // them into gaps.

    size_t seg_dim = segments.size();
    if(seg_dim == 0) {
        return &m_out;
    }

    // First go from the ends end see if we
    // can improve boundary exons
    size_t k0 = 0;
    while(k0 < seg_dim) {
        SSegment& s = segments[k0];
        if(s.m_exon) {
            if(s.m_idty < m_minidty || m_endgaps) {
                s.ImproveFromLeft(m_aligner, m_Seq1, m_Seq2);
            }
            if(s.m_idty >= m_minidty) {
                break;
            }
        }
        ++k0;
    }

    // fill the left-hand gap, if any
    if(segments[0].m_exon && segments[0].m_box[0] > 0) {

        SSegment g;
        g.m_exon = false;
        g.m_box[0] = 0;
        g.m_box[1] = segments[0].m_box[0] - 1;
        g.m_box[2] = 0;
        g.m_box[3] = segments[0].m_box[2] - 1;
        g.m_idty = 0;
        g.m_len = segments[0].m_box[0];
        g.m_annot = "<GAP>";
        g.m_details.append( m_Seq1 + g.m_box[0],
                            g.m_box[1] + 1 - g.m_box[0]);
        segments.push_front(g);
        ++seg_dim;
        ++k0;
    }

    int k1 = int(seg_dim - 1);
    while(k1 >= int(k0)) {
        SSegment& s = segments[k1];
        if(s.m_exon) {
            if(s.m_idty < m_minidty || m_endgaps) {
	            s.ImproveFromRight(m_aligner, m_Seq1, m_Seq2);
            }
            if(s.m_idty >= m_minidty) {
	            break;
            }
        }
        --k1;
    }

    // fill the right-hand gap, if any
    if( segments[seg_dim - 1].m_exon && 
        segments[seg_dim - 1].m_box[1] < m_SeqLen1 - 1) {

        SSegment g;
        g.m_exon = false;
        g.m_box[0] = segments[seg_dim - 1].m_box[1] + 1;
        g.m_box[1] = m_SeqLen1 - 1;
        g.m_box[2] = segments[seg_dim - 1].m_box[3] + 1;
        g.m_box[3] = m_SeqLen2 - 1;
        g.m_idty = 0;
        g.m_len = g.m_box[1] - g.m_box[0] + 1;
        g.m_annot = "<GAP>";
        g.m_details.append(m_Seq1 + g.m_box[0], g.m_box[1] + 1 - g.m_box[0]);
        segments.push_back(g);
        ++seg_dim;
    }

    for(size_t k = 0; k < seg_dim; ++k) {
        SSegment& s = segments[k];
        if(s.m_exon && s.m_idty < m_minidty) {
            s.m_exon = false;
            s.m_idty = 0;
            s.m_len = s.m_box[1] - s.m_box[0] + 1;
            s.m_annot = "<GAP>";
            s.m_details.resize(0);
            s.m_details.append(m_Seq1 + s.m_box[0], s.m_box[1] + 1 - s.m_box[0]);
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
               g.m_details.append(m_Seq1 + g.m_box[0], g.m_box[1] + 1 - g.m_box[0]);
               m_out.push_back(g);
               gap_start_idx = -1;
           }
           m_out.push_back(s);
        } 
    }
    if(gap_start_idx >= 0) {
        SSegment& g = segments[gap_start_idx];
        g.m_box[1] = segments[segs_dim-1].m_box[1];
        g.m_box[3] = segments[segs_dim-1].m_box[3];
        g.m_len = g.m_box[1] - g.m_box[0] + 1;
        g.m_details.resize(0);
        g.m_details.append(m_Seq1 + g.m_box[0], g.m_box[1] + 1 - g.m_box[0]);
        m_out.push_back(g);
    }

    return &m_out;
}


// try improving the segment by cutting it from the left
void CSplign::SSegment::ImproveFromLeft(const CNWAligner* aligner,
					const char* seq1,const char* seq2)
{
  const size_t min_query_size = 4;

  int i0 = int(m_box[1] - m_box[0] + 1), i0_max = i0;
  if(i0 < int(min_query_size)) {
    return;
  }

  // find the top score suffix
  int i1 = int(m_box[3] - m_box[2] + 1), i1_max = i1;
  
  CNWAligner::TScore score_max = 0, s = 0;
  
  const CNWAligner::TScore wm =  aligner->GetWm();
  const CNWAligner::TScore wms = aligner->GetWms();
  const CNWAligner::TScore wg =  aligner->GetWg();
  const CNWAligner::TScore ws =  aligner->GetWs();
  
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
    
    // possibly delete first two annotation symbols
    if(m_annot.size() > 2 && m_annot[0] != '<') {
      m_annot.erase(0,2);
    }
  }
}


// try improving the segment by cutting it from the right
void CSplign::SSegment::ImproveFromRight(const CNWAligner* aligner,
					 const char* seq1, const char* seq2)
{
  const size_t min_query_size = 4;

  if(m_box[1] - m_box[0] + 1 < min_query_size) {
    return;
  }

  // find the top score prefix
  int i0 = -1, i0_max = i0;
  int i1 = -1, i1_max = i1;

  CNWAligner::TScore score_max = 0, s = 0;

  const CNWAligner::TScore wm =  aligner->GetWm();
  const CNWAligner::TScore wms = aligner->GetWms();
  const CNWAligner::TScore wg =  aligner->GetWg();
  const CNWAligner::TScore ws =  aligner->GetWs();

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
    
    // possibly delete last two annotation chars
    size_t adim = m_annot.size();
    if(adim > 2 && m_annot[adim - 1] != '>') {
      m_annot.resize(adim - 2);
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


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.6  2003/12/04 19:26:37  kapustin
 * Account for zero-length segment vector
 *
 * Revision 1.5  2003/11/20 17:58:20  kapustin
 * Make the code msvc6.0-friendly
 *
 * Revision 1.4  2003/11/20 14:33:42  kapustin
 * Increase space allowance for gap pre-alignment
 *
 * Revision 1.3  2003/11/10 19:22:31  kapustin
 * Change esf mode to full
 *
 * Revision 1.2  2003/10/31 19:43:15  kapustin
 * Format and compatibility update
 *
 * Revision 1.1  2003/10/30 19:37:20  kapustin
 * Initial toolkit revision
 *
 * ===========================================================================
 */
