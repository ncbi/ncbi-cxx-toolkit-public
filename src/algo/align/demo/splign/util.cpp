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


#include <corelib/ncbi_limits.hpp>

#include "util.hpp"
#include "hf_hitparser.hpp"


BEGIN_NCBI_SCOPE


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


void DoFilter(vector<CHit>* hits) 
{
  if(hits->size() == 0) return;
  
  const static size_t g_max_hit_dist_subj = 700000;
  
  int group_id = 0;
  
  CHitParser hp (*hits, group_id);
  hp.m_Strand = CHitParser::eStrand_Auto;
  hp.m_SameOrder = true;
  hp.m_Method = CHitParser::eMethod_MaxScore_GroupSelect;
  hp.m_CombinationProximity_pre  = -1;
  hp.m_CombinationProximity_post = -1;
  hp.m_MinQueryCoverage = 0;
  hp.m_MinSubjCoverage = 0;
  hp.m_MaxHitDistQ = kMax_Int;
  hp.m_MaxHitDistS = g_max_hit_dist_subj;
  hp.m_Prot2Nucl = false;
  hp.m_SplitQuery = CHitParser::eSplitMode_Clear;
  hp.m_SplitSubject = CHitParser::eSplitMode_Clear;
  hp.m_group_identification_method = CHitParser::eQueryCoverage;
  hp.m_CovStep = 0.6;
  hp.m_OutputAllGroups = false;
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


void SetPatternFromHits(CSplign& splign, vector<CHit>* hits)
{
  sort(hits->begin(), hits->end(), CHit::PPrecedeQ);
  vector<size_t> pattern;
  for(size_t i = 0, n = hits->size(); i < n; ++i) {
    const CHit& h = (*hits)[i];
    pattern.push_back( h.m_ai[0] );
    pattern.push_back( h.m_ai[1] );
    pattern.push_back( h.m_ai[2] );
    pattern.push_back( h.m_ai[3] );
  }

  try {
    splign.SetPattern(pattern);
  }
  catch(exception&) {
    throw; // GCC hack
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
  out.push_back(c0);
  size_t count = 1;
  char buf[100];
  for(size_t k = 1; k < dim; ++k) {
    char c = p[k];
    if(c != c0) {
      c0 = c;
      if(count > 1) {
	sprintf(buf, "%d", count);
	out += buf;
      }
      count = 1;
      out.push_back(c0);
    }
    else {
      ++count;
    }
  }
  if(count > 1) {
    sprintf(buf, "%d", count);
    out += buf;
  }
  return out;
}

// primitive, but works in many cases
size_t TestPolyA(const vector<char>& mrna)
{
  const size_t dim = mrna.size();
  int i = dim - 1;
  for(; i >= 0; --i) {
    if(mrna[i] != 'A') break;
  }
  const size_t len = dim - i - 1;;
  return len > 3 ? i + 1 : kMax_UInt;
}

// could be improved of course...
void MakeIDX()
{
  noskipws(cin);
  char c;
  size_t pos;
  string s;
  while((c = cin.get()) && c != EOF) {
    if(c == '>') {
      pos = size_t(cin.tellg()) - 1;
      cin >> s;
      cout << s << '\t' << pos << endl;
    }
  }
}


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.3  2003/10/31 19:43:15  kapustin
 * Format and compatibility update
 *
 * ===========================================================================
 */
