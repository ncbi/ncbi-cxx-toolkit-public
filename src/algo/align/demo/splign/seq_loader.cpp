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

#include <fstream>
#include "seq_loader.hpp"
#include "splign_app_exception.hpp"

#include <corelib/ncbi_limits.hpp>

BEGIN_NCBI_SCOPE

void CSeqLoader::Open(const string& filename_data, const string& filename_idx)
{
  m_inputstream.open(filename_data.c_str());
  if(!m_inputstream) {
    NCBI_THROW(
	       CSplignAppException,
	       CSplignAppException::eCannotOpenFile,
	       filename_data.c_str());
  }
  
  m_external_index = filename_idx != kEmptyStr;

  if(m_external_index) {
    CNcbiIfstream idxstream (filename_idx.c_str());
    if(idxstream) {
      while(idxstream) { // read index
        string query;
        size_t offset;
        idxstream >> query >> offset;
        m_idx[query] = offset;
      }
    }
    else {      
      NCBI_THROW(
		 CSplignAppException,
		 CSplignAppException::eCannotOpenFile,
		 filename_idx.c_str());
    }
  }
  else {
    NCBI_THROW(
	       CSplignAppException,
	       CSplignAppException::eInternal,
	       "Auto-indexing not yet supported. "
	       "Please specify index file for each "
	       "multi-FastA input file.");
  }
}


void CSeqLoader::Load(const string& id, vector<char>* seq,
		      size_t from, size_t to)
{
  istream* input = 0;

  if(m_external_index) {
    
    map<string,size_t>::const_iterator im = m_idx.find(id);
    if(im == m_idx.end()) {
      string msg ("Unable to locate ");
      msg += id;
      NCBI_THROW(
		 CSplignAppException,
		 CSplignAppException::eInternal,
		 msg.c_str());
    }
    else {
      m_inputstream.seekg(im->second);
      input = &m_inputstream;
    }
  }
  else {
    NCBI_THROW(
	       CSplignAppException,
		 CSplignAppException::eInternal,
	       "Auto-indexing not yet supported. "
	       "Please specify index file for each "
	       "multi-FastA input file.");
  }
  
  seq->clear();
  
  char buf [1024];
  input->getline(buf, sizeof buf, '\n'); // info line
  if(!input) {
    NCBI_THROW(
	       CSplignAppException,
	       CSplignAppException::eCannotReadFile,
	       "Unable to read sequence data");
  }

  if(from == 0 && to == kMax_UInt) {
    // read entire sequence until the next one or eof
    while(*input) {
      size_t i0 = input->tellg();
      input->getline(buf, sizeof buf, '\n');
      size_t i1 = input->tellg();
      if(i1 - i0 > 1) {
	size_t line_size = i1 - i0 - 1;
	if(buf[0] == '>') break;
	size_t size_old = seq->size();
	seq->resize(size_old + line_size);
	copy(buf, buf + line_size, seq->begin() + size_old);
      }
      else if (i0 == i1) {
	break; // GCC hack
      }
    }
  }
  else {
    // read only a portion of a sequence
    const size_t dst_seq_len = to - from + 1;
    seq->resize(dst_seq_len + sizeof buf);
    size_t i0 = input->tellg(), i1;
    size_t dst_read = 0, src_read = 0;
    while(*input) {
      input->getline(buf, sizeof buf, '\n');
      if(buf[0] == '>') {
	seq->resize(dst_read);
	return;
      }
      i1 = input->tellg();

      if(i1 - i0 > 1) {
	src_read += i1 - i0 - 1;
      }
      else if(i1 - i0 == 1) {
	continue;
      }
      else { 
	break; // GCC hack
      }

      if(src_read > from) {
	size_t line_size = i1 - i0 - 1;
	size_t start  = dst_read? 0: (line_size - (src_read - from));
	size_t finish = (src_read > to)?
	                (line_size - (src_read - to) + 1):
	                line_size;
	copy(buf + start, buf + finish, seq->begin() + dst_read);
	dst_read += finish - start;
	if(dst_read >= dst_seq_len) {
	  seq->resize(dst_seq_len);
	  return;
	}
      }
      i0 = i1;
    }
    seq->resize(dst_read);
  }
}


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2003/10/31 19:43:15  kapustin
 * Format and compatibility update
 *
 * ===========================================================================
 */
