#ifndef ALGO_DEMO_SPLIGN_SUBJMIXER__HPP
#define ALGO_DEMO_SPLIGN_SUBJMIXER__HPP

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
*   The purpose of this class is to map different subjects within a same query
*   to non-overlapping regions of sequence so that they could be processed
*   uniformely by group identification algorithms. And vice versa, given hits
*   and mapping information, restore original subject names.
*/

#include <vector>
#include "hf_hit.hpp"

BEGIN_NCBI_SCOPE

class CSubjMixer
{
public:

  // mix subjects
  CSubjMixer (vector<CHit>* hits);
  
  // unmix subjects
  void UnMix (vector<CHit>* hits);

private:

  vector<string>  m_subjects; // associated subjects
  vector<size_t>  m_shifts; // interval shift from original
  
};


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2003/10/30 19:37:20  kapustin
 * Initial toolkit revision
 *
 * ===========================================================================
 */

#endif
