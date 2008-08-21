#ifndef OLIGOFAR_CGUIDEFILE__HPP
#define OLIGOFAR_CGUIDEFILE__HPP

#include "defs.hpp"
#include "cfilter.hpp"

BEGIN_OLIGOFAR_SCOPES

/* ************************************************************************
    This code reads guide file of following format:

    1. type of result:
    for non-paired search - always 0;
    for paired search: 0 - paired match; 1 - non-paired match for the first 
    member of the pair; 2 - non-paired match for the second member of the pair;
    2. query ordinal number;
    3. subject ordinal number;
    4. subject offset (for the first member of the pair if paired match);
    5. 0 - reverse strand; 1 - forward strand (for the first member of the 
    pair if paired match);
    6. mismatch position in the query (0 for exact match) (for the first 
    member of the pair if paired match);
    7. subject base at mismatch position ('-' for exact match) (for the 
    first member of the pair if paired match);
    8. if paired match - subject offset of the second member of the pair;
    9. if paired match - strand of the second member of the pair;
    10. if paired match - mismatch position of the second member of the pair;
    11. if paired match - subject base at mismatch position for the second 
    member of the pair.
    ************************************************************************ */

/* It should ignore everything that has anything byt 0 in first column and 
   anything but 0 in columns 6 and 10 (if any). Also column 5 xor column 9 should 
   give 1 (strands should be opposite) and if column 5 = 1 then column 4 should be 
   less then column 8, alse column 4 should be greater then column 8 */

class CSeqIds;
class CGuideFile 
{
public:
	typedef map<string,int> TId2Ord;
	typedef deque<string> TIdList;

	CGuideFile( const string& fileName, CFilter& filter, CSeqIds& seqIds );
    
    bool NextHit( Uint8 ordinal, CQuery * query );
    
	int GetMaxDist() const { return m_filter->GetMaxDist(); }
	int GetMinDist() const { return m_filter->GetMinDist(); }

protected:

	void AdjustInput( int& fwd, char dir, int& p1, int which ) const;

protected:
    ifstream m_in;
    string m_buff;
	CFilter * m_filter;
	CSeqIds * m_seqIds;
};

inline CGuideFile::CGuideFile( const string& fileName, CFilter& filter, CSeqIds& seqIds )
	: m_filter( &filter ), m_seqIds( &seqIds )
{
	ASSERT( m_filter );
	ASSERT( m_seqIds );
	if( fileName.length() ) {
	    m_in.open( fileName.c_str() );
		if( m_in.fail() ) THROW( runtime_error, "Failed to open file " << fileName << ": " << strerror(errno) );
	    getline( m_in, m_buff );
	}
}

END_OLIGOFAR_SCOPES

#endif

