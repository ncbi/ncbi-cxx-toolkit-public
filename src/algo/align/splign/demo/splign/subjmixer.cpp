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
*   Hit subject mixer class definition.
*
*/

#include "subjmixer.hpp"
#include "splign_app_exception.hpp"

#include <algorithm>

BEGIN_NCBI_SCOPE


CSubjMixer::CSubjMixer(vector<CHit>* hits)
{
  const size_t dim = hits->size();
  if(dim == 0) return;

  sort(hits->begin(), hits->end(), CHit::PQuerySubjSubjCoord);

  string subj;

  const size_t gap = 1000000; // distance btw adjacent mapping regions -
                              // must be large enough so that "max intron"
                              // restriction would keep subjects apart

  size_t low = 0;             // lower mapping region boundary
  size_t shift = 0;           // shift value to match lower boundary

  for(size_t i = 0; i < dim; ++i) {

    CHit& h = (*hits)[i];
    if(h.m_Subj != subj) {
      low = (i? (*hits)[i-1].m_ai[3]: 0) + gap;
      m_subjects.push_back(subj = h.m_Subj);
      m_shifts.push_back(shift = low - h.m_ai[2]);
    }
    h.m_ai[2] += shift;
    h.m_an[2] += shift;
    h.m_ai[3] += shift;
    h.m_an[3] += shift;
  }
}


void CSubjMixer::UnMix(vector<CHit>* hits)
{
  const size_t dim = hits->size();
  if(dim == 0) return;

  string subj;
  size_t shift = 0;
  for(size_t i = 0; i < dim; ++i) {
    
    if(subj != (*hits)[i].m_Subj) {
      subj = (*hits)[i].m_Subj;
      vector<string>::const_iterator it = find(m_subjects.begin(),
					       m_subjects.end(),
					       (*hits)[0].m_Subj);
      if(it == m_subjects.end()) {
	string msg ("Subject not found in subject mixer: ");
	msg += subj;
	NCBI_THROW(
		   CSplignAppException,
		   eInternal,
		   msg.c_str());
      }
      shift = m_shifts[it - m_subjects.begin()];
    }

    (*hits)[i].m_ai[2] -= shift;
    (*hits)[i].m_an[2] -= shift;
    (*hits)[i].m_ai[3] -= shift;
    (*hits)[i].m_an[3] -= shift;
  }
}

END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2003/11/20 17:58:20  kapustin
 * Make the code msvc6.0-friendly
 *
 * Revision 1.1  2003/10/30 19:37:20  kapustin
 * Initial toolkit revision
 *
 * ===========================================================================
 */
