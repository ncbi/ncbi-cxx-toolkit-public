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
* Author:  Yuri Kapustin
*
* File Description:  Helper functions
*                   
* ===========================================================================
*/

#include "splign_util.hpp"
#include "splign_hitparser.hpp"

#include <algorithm>
#include <math.h>

BEGIN_NCBI_SCOPE


#ifdef GENOME_PIPELINE

CInfTable::CInfTable(size_t cols): m_cols(cols)
{
  m_data.resize(m_cols, 0);
}


size_t CInfTable::Load(const string& filename)
{
  ifstream ifs (filename.c_str());
  size_t read = 0;
  char line [1024];
  while(ifs) {
    line[0] = 0;
    ifs.getline(line, sizeof line, '\n');
    if(line[0]) {
      if(line[0] != '#') {

	if(!x_ReadColumns(line)) {
	  return 0;
	}

	string accession;
	SInfo info;
	bool parsed = x_ParseLine(line, accession, info);
	if(!parsed) {
	  return 0;
	}
	m_map[accession] = info;
	++read;
      }
    }
  }
  return read;
}


bool CInfTable::x_ReadColumns(char* line)
{
  char* p0 = line;
  char* pe = p0 + strlen(line);
  char* p = p0;
  for(size_t i = 0; i < m_cols; ++i) {
    p = find(p0, pe, '\t');
    if(i+1 < m_cols && p == pe) {
      return false;
    }
    m_data[i] = p0;
    *p = 0;
    p0 = p + 1;
  }
  return true;
}


bool CInfTable::GetInfo(const string& id, SInfo& info) {
  map<string, SInfo>::const_iterator ii = m_map.find(id);
  if(ii != m_map.end()) {
    info = ii->second;
    return true;
  }
  return false;
}


bool CInf_mRna::x_ParseLine (const char* line,
			     string& accession, SInfo& info)
{
  if(!line || !*line) {
    return false;
  }

  accession = x_GetCol(0);

  const char* buf_size = x_GetCol(2);
  sscanf(buf_size, "%d", &info.m_size);

  const char* buf = x_GetCol(3);
  SInfo::EStrand strand = SInfo::ePlus;
  if(buf[0]=='m') {
    strand = SInfo::eMinus;
  }
  buf = x_GetCol(4);
  if(buf[0] != '-') {
    char c;
    sscanf(buf, "%d,%d%c",
	   &info.m_polya_start,
	   &info.m_polya_end,
	   &c);
    if(c == 'm') {
      strand = SInfo::eMinus;
    }
  }
  buf = x_GetCol(9);
  if(strcmp(buf, "wrong_strand") == 0) {
    strand = SInfo::eUnknown;
  }
  info.m_strand = strand;

  return true;
}


bool CInf_EST::x_ParseLine (const char* line, string& accession, SInfo& info)
{
  if(!line || !*line) {
    return false;
  }

  accession = x_GetCol(0);
  const char* buf = x_GetCol(3);
  switch(buf[0]) {
      case '5': info.m_strand = SInfo::ePlus;    break;
      case '3': info.m_strand = SInfo::eMinus;   break;
      default:  info.m_strand = SInfo::eUnknown; break;
  }

  buf = x_GetCol(6);
  if(buf[0] != '-') {
    char c;
    sscanf(buf, "%d,%d%c",
	   &info.m_polya_start,
	   &info.m_polya_end,
	   &c);
    if(c == 'm' && info.m_strand == SInfo::ePlus ||
       c == 'p' && info.m_strand == SInfo::eMinus) {
      info.m_strand = SInfo::eUnknown;
    }
  }

  return true;
}


#endif // GENOME_PIPELINE


bool IsConsensus(const char* donor, const char* acceptor)
{
  return donor && acceptor &&
    (donor[0] == 'G' && (donor[1] == 'C' || donor[1] == 'T'))
    &&
    (acceptor[0] == 'A' && acceptor[1] == 'G');
}


void CleaveOffByTail(vector<CHit>* hits, size_t polya_start)
{
  const size_t hit_dim = hits->size();
  vector<size_t> vdel;
  for(size_t i = 0; i < hit_dim; ++i) {
    CHit& hit = (*hits)[i];
    if(size_t(hit.m_ai[0]) >= polya_start) {
      vdel.push_back(i);
    }
    else if(size_t(hit.m_ai[1]) >= polya_start) {
      hit.Move(1, polya_start-1, false);
      if(hit.IsConsistent() == false) {
	vdel.push_back(i);
      }
    }
  }
  const size_t del_dim = vdel.size();
  if(del_dim > 0) {
    for( size_t i = 0, j = 0, k = 0; j < hit_dim; ) {
      if(k < del_dim && j == vdel[k]) {
	++k;
	++j;
      }
      else if(i < j) {
	(*hits)[i++] = (*hits)[j++];
      }
      else { // i == j
	++i; ++j;
      }
    }
    hits->resize(hit_dim - del_dim);
  }
}


void XFilter(vector<CHit>* hits) 
{
  if(hits->size() == 0) return;
  int group_id = 0;
  CHitParser hp (*hits, group_id);
  hp.m_Strand = CHitParser::eStrand_Both;
  hp.m_SameOrder = false;
  hp.m_Method = CHitParser::eMethod_MaxScore;
  hp.m_CombinationProximity_pre  = -1;
  hp.m_CombinationProximity_post = -1;
  hp.m_MinQueryCoverage = 0;
  hp.m_MinSubjCoverage = 0;
  hp.m_MaxHitDistQ = kMax_Int;
  hp.m_MaxHitDistS = kMax_Int;
  hp.m_Prot2Nucl = false;
  hp.m_SplitQuery = CHitParser::eSplitMode_Clear;
  hp.m_SplitSubject = CHitParser::eSplitMode_Clear;
  hp.m_group_identification_method = CHitParser::eNone;
  hp.m_Query = (*hits)[0].m_Query;
  hp.m_Subj =  (*hits)[0].m_Subj;
  
  hp.Run( CHitParser::eMode_Normal );
  *hits = hp.m_Out;
}


void GetHitsMinMax(const vector<CHit>& vh,
		   size_t* qmin, size_t* qmax,
		   size_t* smin, size_t* smax)
{
  const size_t hit_dim = vh.size();
  
  *qmin = kMax_UInt, *qmax = 0;
  *smin = *qmin, *smax = *qmax;
  
  for(size_t i = 0; i < hit_dim; ++i) {
    if(size_t(vh[i].m_ai[0]) < *qmin)
      *qmin = vh[i].m_ai[0];
    if(size_t(vh[i].m_ai[1]) > *qmax)
      *qmax = vh[i].m_ai[1];
    if(size_t(vh[i].m_ai[2]) < *smin)
      *smin = vh[i].m_ai[2];
    if(size_t(vh[i].m_ai[3]) > *smax)
      *smax = vh[i].m_ai[3];
  }
}


// apply run-length encoding
string RLE(const string& in)
{
  string out;
  const size_t dim = in.size();
  if(dim == 0) {
    return kEmptyStr;
  }
  const char* p = in.c_str();
  char c0 = p[0];
  out.append(1, c0);
  size_t count = 1;
  for(size_t k = 1; k < dim; ++k) {
    char c = p[k];
    if(c != c0) {
      c0 = c;
      if(count > 1) {
        out += NStr::IntToString(count);
      }
      count = 1;
      out.append(1, c0);
    }
    else {
      ++count;
    }
  }
  if(count > 1) {
    out += NStr::IntToString(count);
  }
  return out;
}


CNWAligner::TScore ScoreByTranscript(const CNWAligner& aligner,
				     const char* transcript)
{
  const int Wg  = aligner.GetWg();
  const int Ws  = aligner.GetWs();
  const int Wm  = aligner.GetWm();
  const int Wms = aligner.GetWms();

  const size_t dim = strlen(transcript);
  if(dim == 0) return 0;

  CNWAligner::TScore score = 0;

  char state1, state2;

  switch(transcript[0]) {
    case 'M':
    case 'R':  state1 = state2 = 0;
    break;
    case 'I':  state1 = 1; state2 = 0; score += Wg;
    break;
    case 'D':  state1 = 0; state2 = 1; score += Wg;
    break;
    default: {
      CNcbiOstrstream oss;
      oss << "Unknown symbol in transcript: " << transcript[0];
      const string err_msg =  CNcbiOstrstreamToString(oss);
      NCBI_THROW(CAlgoAlignException, eInternal, err_msg.c_str());
    }
  }

  for(size_t i = 0; i < dim; ++i) {

    switch(transcript[i]) {

      case 'M': {
        state1 = state2 = 0;
        score += Wm;
      }
      break;

      case 'R': {
        state1 = state2 = 0;
        score += Wms;
      }
      break;

      case 'I': {
        if(state1 != 1) score += Wg;
        state1 = 1; state2 = 0;
        score += Ws;
      }
      break;

      case 'D': {
        if(state2 != 1) score += Wg;
        state1 = 0; state2 = 1;
        score += Ws;
      }
      break;

      default: {
        char buf [1024];
        sprintf(buf, "Unknown symbol in transcript: %c", transcript[i]);
        NCBI_THROW(CAlgoAlignException, eInternal, buf);
      }
    }
  }

  return score;
}


END_NCBI_SCOPE

/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.5  2004/04/23 18:43:58  ucko
 * <cmath> -> <math.h>, since some older compilers (MIPSpro) lack the wrappers.
 *
 * Revision 1.4  2004/04/23 16:50:48  kapustin
 * sprintf->CNcbiOstrstream
 *
 * Revision 1.3  2004/04/23 14:37:44  kapustin
 * *** empty log message ***
 *
 *
 * ===========================================================================
 */
