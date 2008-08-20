#ifndef OLIGOFAR_CHIT__HPP
#define OLIGOFAR_CHIT__HPP

#include "defs.hpp"

BEGIN_OLIGOFAR_SCOPES

class CQuery;
class CHit
{
public:
    friend class CFilter;
    friend class CQuery;
    
    static Uint8 GetCount() { return s_count; }
            
    ~CHit();
    CHit( CQuery* q );
    CHit( CQuery* q, Uint4 seqOrd, bool pairmate, double score, int from, int to );
    CHit( CQuery* q, Uint4 seqOrd, double score1, int from1, int to1, double score2, int from2, int to2 );

    static int GetComponentMask( bool pairmate ) { return 1 << pairmate; }
    CQuery * GetQuery() const { return m_query; }
    Uint4 GetSeqOrd() const { return m_seqOrd; }
    int GetComponents() const { return m_components; }
    double GetTotalScore() const { return m_score[0] + m_score[1]; }
    double GetScore( bool pairmate ) const { return m_score[pairmate]; }
    int GetFrom( bool pairmate ) const { return m_from[pairmate]; }
    int GetTo( bool pairmate ) const { return m_to[pairmate]; }
    bool IsRevCompl( bool pairmate ) const { return m_from[pairmate] > m_to[pairmate]; }
    
    char GetStrand() const { return m_components & 1 ? IsRevCompl( 0 ) ? '-' : '+' : IsRevCompl( 1 ) ? '+' : '-'; }
    bool IsPaired() const { return GetComponents() == 3; }
    bool HasPairTo( bool pairmate ) const { return HasComponent( !pairmate ); }
    bool HasComponent( bool pairmate ) const { return m_components & (1 << pairmate); }
    bool IsNull() const { return m_components == 0; }

    CHit * GetNextHit() const { return m_next; }

    void SetTarget( bool pairmate, const char * from, const char * to );
    const string& GetTarget( bool pairmate ) const { return m_target[pairmate]; }

	bool TargetNotSet() const { return m_target[0].size() == 0 && m_target[1].size() == 0; }

protected:
    explicit CHit( const CHit& );
private:
    CQuery * m_query;
    CHit * m_next;
    float m_score[2];
    int m_from[2];
    int m_to[2];
    Uint4 m_seqOrd;
    string m_target[2];
    Uint1 m_components;
    static Uint8 s_count;
};

////////////////////////////////////////////////////////////////////////
// implementation

inline CHit::~CHit() { 
    ASSERT( !( IsNull() && m_next )); 
    delete m_next; 
    if( !IsNull() ) --s_count; 
}

inline CHit::CHit( CQuery* q ) : 
    m_query( q ), m_next( 0 ), m_seqOrd( ~0U ), m_components( 0 ) 
{
    m_score[0] = m_score[1] = 0;
    m_from[0] = m_from[1] = 0;
    m_to[0] = m_to[1] = 0;
//        ++s_count;
}

inline CHit::CHit( CQuery* q, Uint4 seqOrd, bool pairmate, double score, int from, int to ) 
    : m_query( q ), m_next( 0 ), m_seqOrd( seqOrd ), 
      m_components( GetComponentMask( pairmate ) ) 
{
    m_score[pairmate] = score;
    m_score[!pairmate] = 0;
    m_from[pairmate] = from;
    m_from[!pairmate] = 0;
    m_to[pairmate] = to;
    m_to[!pairmate] = 0;
    //m_target[0] = m_target[1] = "?";
    ++s_count;
}

END_OLIGOFAR_SCOPES

#endif
