#ifndef ALGO_DEMO_SPLIGN_SEQ_LOADER__HPP
#define ALGO_DEMO_SPLIGN_SEQ_LOADER__HPP

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
*   CSeqLoader class
*
*/


#include <corelib/ncbistd.hpp>

BEGIN_NCBI_SCOPE

// Multi-FastA sequence loader
class CSeqLoader
{
public:

  CSeqLoader(): m_external_index(false) {}

  void Open (const string& filename_data,
	     const string& filename_idx = kEmptyStr);
  
  void Load(const string& id,
	    vector<char>* seq,
	    size_t from, size_t to);
  
private:
  
  CNcbiIfstream       m_inputstream;
  bool                m_external_index;
  map<string, size_t> m_idx;
  
};

END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2003/10/31 19:43:15  kapustin
 * Format and compatibility update
 *
 * ===========================================================================
 */

#endif

