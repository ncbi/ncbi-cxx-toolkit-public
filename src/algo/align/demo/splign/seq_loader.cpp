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
      int line_size = int(input->tellg()) - i0 - 1;
      if(line_size > 0) {
	if(buf[0] == '>') break;
	size_t size_old = seq->size();
	seq->resize(size_old + line_size);
	copy(buf, buf + line_size, seq->begin() + size_old);
      }
    }
  }
  else {
    // read only a portion of a sequence
    const size_t dst_seq_len = to - from + 1;
    seq->resize(dst_seq_len + sizeof buf);
    size_t pos0 = input->tellg(), pos1;
    size_t dst_read = 0, src_read = 0;
    int line_len = 0;
    while(*input) {
      input->getline(buf, sizeof buf, '\n');
      if(buf[0] == '>') {
	seq->resize(dst_read);
	return;
      }
      pos1 = input->tellg();
      line_len = int(pos1) - pos0 - 1;
      if(line_len > 0) {
	src_read += line_len;
      }
      else {
	continue;
      }
      if(src_read > from) {
	size_t start  = dst_read? 0: (line_len - (src_read - from));
	size_t finish = (src_read > to)?
	                (line_len - (src_read - to) + 1):
	                line_len;
	copy(buf + start, buf + finish, seq->begin() + dst_read);
	dst_read += finish - start;
	if(dst_read >= dst_seq_len) {
	  seq->resize(dst_seq_len);
	  return;
	}
      }
      pos0 = pos1;
    }
    seq->resize(dst_read);
  }
}



END_NCBI_SCOPE
