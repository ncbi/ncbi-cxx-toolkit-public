#ifndef OLIGOFAR_CFETAMAP__HPP
#define OLIGOFAR_CFETAMAP__HPP

#include "cprogressindicator.hpp"
#include "cspacialranges.hpp"
#include "cseqids.hpp"

BEGIN_OLIGOFAR_SCOPES

class CFeatMap
{
public:
    typedef map<int,CSpacialRanges*> TData;
    CFeatMap() : m_seqIds(0) {}
    ~CFeatMap() { Clear(); }
    void Clear();
    void SetSeqIds( CSeqIds * seqids ) { m_seqIds = seqids; }
    void ReadFile( const string& fname ) { ifstream in( fname.c_str() ); ReadFile( in, fname ); }
    void ReadFile( istream& in, const string& name = "" );
    CSpacialRanges * GetRanges( int ord ) const;
    CSpacialRanges * GetRanges( const string& name ) const { ASSERT( m_seqIds ); return GetRanges( m_seqIds->Register( name ) ); }
    CSpacialRanges * GetRanges( const CSeqIds::TIds& ids ) const { ASSERT( m_seqIds ); return GetRanges( m_seqIds->Register( ids ) ); }
protected:
    TData m_data;
    CSeqIds * m_seqIds;
};

inline CSpacialRanges * CFeatMap::GetRanges( int ord ) const 
{
    TData::const_iterator i = m_data.find( ord );
    if( i == m_data.end() ) return 0;
    return i->second;
}

inline void CFeatMap::Clear()
{
    ITERATE( TData, d, m_data ) delete d->second;
    m_data.clear();
}

inline void CFeatMap::ReadFile( istream& in, const string& name )
{
    CProgressIndicator p( "Reading features" + (name.length() ? " from " + name : string("") ) );
    string buff;
    int line = 0;
    ASSERT( m_seqIds );
    TData::iterator i = m_data.end();
    while( getline( in, buff ) ) {
        ++line;
        NStr::TruncateSpaces( buff );
        p.Increment();
        if( buff.length() == 0 || buff[0] == '#' ) continue;
        istringstream il( buff );
        string chr;
        int from, to;
        il >> chr >> from >> to;
        if( il.fail() ) THROW( runtime_error, "feature file has wrong format in line " << line << " [" << buff << "]" );
        int ord = m_seqIds->Register( chr );
        if( i == m_data.end() || i->first != ord ) {
            i = m_data.find( ord );
            if( i == m_data.end() ) 
                i = m_data.insert( i, make_pair( ord, new CSpacialRanges( 0, numeric_limits<int>::max() - 2 ) ) );
        }
        i->second->AddRange( from - 1, to - 1 );
    }
    p.Summary();
}
        
END_OLIGOFAR_SCOPES

#endif
