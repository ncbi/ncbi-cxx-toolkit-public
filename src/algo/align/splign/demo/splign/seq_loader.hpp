#ifndef ALGO_DEMO_SPLIGN_SEQ_LOADER__HPP
#define ALGO_DEMO_SPLIGN_SEQ_LOADER__HPP


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

#endif
