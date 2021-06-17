#ifndef OLIGOFAR_CHIT__HPP
#define OLIGOFAR_CHIT__HPP

#include "defs.hpp"
#include "cseqcoding.hpp"
#include "ctranscript.hpp"

BEGIN_OLIGOFAR_SCOPES

class CQuery;
class CSamRecord;
class CHit
{
public:
    typedef TTrSequence TTranscript;

    ~CHit();

    CHit( CQuery* q );
    CHit( CQuery* q, Uint4 seqOrd, int pairmate, 
            double score, int from, int to, int convtbl, 
            const TTranscript& transcript );
    CHit( CQuery* q, Uint4 seqOrd, 
            double score1, int from1, int to1, int convTbl1, 
            double score2, int from2, int to2, int convTbl2, 
            const TTranscript& tr1, const TTranscript& tr2 );
    // assumes that a and b are correct and represent paired hit of the same read on the same contig
    CHit( CQuery *, Uint4 seqord, double score1, double score2, 
            CSamRecord * a, CSamRecord * b );

    void SetPairmate( int pairmate, double score, int from, int to, int convTbl, const TTranscript& tr );

    Uint4 GetSeqOrd() const { return m_seqOrd; }

    CQuery * GetQuery() const { return m_query; }
    double GetTotalScore() const { return m_score[0] + m_score[1]; }
    double GetScore( int pairmate ) const { return m_score[pairmate]; }
            
    CHit * GetNextHit() const { return m_next; }

    bool TargetNotSet() const { return m_target[0] == 0 && m_target[1] == 0; }
    void SetTarget( int pairmate, const char * from, const char * to );

    const char * GetCachedSam( int pairmate ) const { if( m_flags & fCachesSAMlines ) return m_target[pairmate]; else return ""; }
    // target is ncbi8na, but can not start with gap (ASCII(0)), so "" is a good indicator that it is not present
    const char * GetTarget( int pairmate ) const { if( m_flags & fCachesSAMlines ) return ""; if( char * x = m_target[pairmate] ) return x; else return ""; }
    const TTranscript& GetTranscript( int pairmate ) const { return m_transcript[pairmate]; }

    /* 
       Note: from and to for alignments do not affect algorithms, and are used only for output formatting,
       while hit length (especially for paired reads) matters, as well as individual reads strands combination.
       
       So we preserve full length and supplementary fields rather then froms and tos
    */

    enum EFlags {
        // GetGeometryFlag() depends on that 
        // fRead1_reverse,fRead2_reverse, and fOrder_reverse
        // are the smallest bits
        fRead1_reverse   = 0x01,
        fRead2_reverse   = 0x02, // NB: it is used in x_InitComponent that fact that (fRead1_reverse << 1) == fRead2_reverse
        fOrder_reverse   = 0x04, // means that min( from2, to2 ) < min( from1, to1 )
        fReads_overlap   = 0x08, // this flag indicates ERROR; a number of functions depend on this
        fOther           = 0xf0,
        fRead1_forward   = 0x00,
        fRead2_forward   = 0x00,
        fOrder_forward   = 0x00,
        fComponent_1     = 0x10,
        fComponent_2     = 0x20,
        fPairedHit       = 0x30,
        fPurged          = 0x80,
        fMaskGeometry    = fRead1_reverse|fRead2_reverse|fOrder_reverse, 
        kRead1_strand_bit = 0,
        kRead2_strand_bit = 1,
        kOrder_strand_bit = 2,
        kAlign_convTbl1_bit = 8,
        kAlign_convTbl2_bit = 10,
        fAlign_convTablesMask = (3 << kAlign_convTbl1_bit) | (3 << kAlign_convTbl2_bit), 
        fCachesSAMlines = 0x8000,
        fDeleted = 0x4000,
        kComponents_bit  = 4,
        fNONE            = 0x00
    };

    enum EGeometry {
        fGeometry_Fwd1_Fwd2 = (fRead1_forward|fRead2_forward|fOrder_forward),
        fGeometry_Fwd1_Rev2 = (fRead1_forward|fRead2_reverse|fOrder_forward),
        fGeometry_Rev1_Fwd2 = (fRead1_reverse|fRead2_forward|fOrder_forward),
        fGeometry_Rev1_Rev2 = (fRead1_reverse|fRead2_reverse|fOrder_forward),
        fGeometry_Fwd2_Fwd1 = (fRead1_forward|fRead2_forward|fOrder_reverse),
        fGeometry_Rev2_Fwd1 = (fRead1_forward|fRead2_reverse|fOrder_reverse),
        fGeometry_Fwd2_Rev1 = (fRead1_reverse|fRead2_forward|fOrder_reverse),
        fGeometry_Rev2_Rev1 = (fRead1_reverse|fRead2_reverse|fOrder_reverse),
        fGeometry_NOT_SET = 0
    };
    
    bool IsNull() const { return GetComponentFlags() == 0; } //return m_length[0] == 0 && m_length[1] == 0; }
    bool IsPairedHit() const { return (m_flags & fPairedHit) == fPairedHit; }

    static int ComputeRangeLength( int , int, int, int );
    static int GetOtherComponent( int pairmate ) { return !pairmate; }
    static Uint8 GetCount() { return s_count; }

    bool IsPurged() const { return ( m_flags & fPurged ) != 0; }

    int  GetFrom( int pairmate ) const;
    int  GetTo( int pairmate ) const;
    int  GetLength( int pairmate ) const { return m_length[pairmate]; }
    int  GetConvTbl( int pairmate ) const { return 3 & (m_flags >> ( pairmate ? kAlign_convTbl2_bit : kAlign_convTbl1_bit )); } 

    CSeqCoding::EStrand GetStrand( int pairmate ) const;
    
    bool Equals( const CHit * other ) const;
    bool EqualsSameQ( const CHit * other ) const;

    bool ClustersWith( const CHit * other ) const;
    bool ClustersWithSameQ( const CHit * other ) const;
    bool ClustersWithSameS( const CHit * other ) const;
    bool ClustersWithSameQS( const CHit * other ) const;
    
    bool IsReverseStrand( int pairmate ) const { return ( m_flags & ( pairmate != 0 ? fRead2_reverse : fRead1_reverse ) ) != 0; }
    bool HasComponent( int pairmate ) const { return m_length[pairmate] != 0 ; }
    bool HasPairTo( int pairmate ) const { return HasComponent( GetOtherComponent( pairmate ) ); }

    int  GetComponentMask() const { return GetComponentFlags() >> CHit::kComponents_bit; }
    int  GetComponentFlags() const { return m_flags&fPairedHit; }
    char GetStrand() const { return IsReverseStrand( HasComponent(0) ? 0 : 1 ) ? '-' : '+'; }
    double GetComponentScores( int cflags ) const { 
        cflags <<= kComponents_bit; return (cflags & fComponent_1 ? GetScore(0) : 0) + (cflags & fComponent_2 ? GetScore(1) : 0 ); }

    int GetFrom() const { return m_fullFrom; }
    int GetTo() const { return m_fullTo; }
    int GetRangeLength() const { return m_fullTo - m_fullFrom + 1; }

    bool IsReverseOrder() const { return (m_flags & fOrder_reverse) != 0; }
    bool IsOverlap() const { return (m_flags & fReads_overlap) != 0; } // NB: overlap hits are BAD, one can't say individual reads positions

    Uint2 GetFilterGeometry() const { return (fOrder_reverse & m_flags ? ~m_flags : m_flags) & fMaskGeometry; }
    Uint2 GetGeometry() const { return fMaskGeometry & m_flags; }
    static Uint2 ComputeGeometry( int from1, int to1, int from2, int to2 );

    int GetUpperHitMinPos() const;

    void PrintDebug( ostream& out ) const;

    class C_NextCtl
    {
    protected:
        friend class CFilter;
        C_NextCtl( CHit* hit ) : m_hit( hit ) {}
        const CHit * operator -> () const { return m_hit; }
        const CHit& operator * () const { return *m_hit; }
        CHit * operator -> () { return m_hit; }
        CHit& operator * () { return *m_hit; }
        void SetNext( CHit * hit ) const;
        void SetPurged() const { m_hit->m_flags |= fPurged; }
    protected:
        CHit * m_hit;
    };

protected:
    void x_SetValues( int min1, int max1, int min2, int max2 );
    void x_InitComponent( int mate, CSamRecord * a );
    static void x_Register( const CHit * hit, bool on );

private:
    explicit CHit( const CHit& );
    CHit& operator = (const CHit& );

private:
    CQuery * m_query;
    CHit * m_next;
    Uint4 m_seqOrd;
    float m_score[2];

    int m_fullFrom;
    int m_fullTo;

    Uint1 m_length[2]; // for components 1 and 2, 0 for no alignment
    Uint2 m_flags;

    // TODO: optimize memory usage and fragmentation at some point
    char * m_target[2]; // length should be... length[i] ;-)
    TTranscript m_transcript[2];

    static Uint8 s_count;
};

////////////////////////////////////////////////////////////////////////
// implementation

inline CSeqCoding::EStrand CHit::GetStrand( int pairmate ) const 
{
    return pairmate ?
        m_flags & fRead2_reverse ? CSeqCoding::eStrand_neg : CSeqCoding::eStrand_pos :
        m_flags & fRead1_reverse ? CSeqCoding::eStrand_neg : CSeqCoding::eStrand_pos ;
}

inline int CHit::GetFrom( int pairmate ) const
{
    switch( m_flags & fMaskGeometry ) {
    case fRead1_forward|fRead2_forward|fOrder_forward: return pairmate ? m_fullTo - m_length[1] + 1 : m_fullFrom;
    case fRead1_forward|fRead2_forward|fOrder_reverse: return pairmate ? m_fullFrom : m_fullTo - m_length[1] + 1;
    case fRead1_forward|fRead2_reverse|fOrder_forward: return pairmate ? m_fullTo : m_fullFrom;
    case fRead1_forward|fRead2_reverse|fOrder_reverse: return pairmate ? m_fullFrom + m_length[1] - 1 : m_fullTo - m_length[0] + 1;
    case fRead1_reverse|fRead2_forward|fOrder_forward: return pairmate ? m_fullTo - m_length[1] + 1 : m_fullFrom + m_length[0] - 1;
    case fRead1_reverse|fRead2_forward|fOrder_reverse: return pairmate ? m_fullFrom : m_fullTo;
    case fRead1_reverse|fRead2_reverse|fOrder_forward: return pairmate ? m_fullTo : m_fullFrom + m_length[0] - 1;
    case fRead1_reverse|fRead2_reverse|fOrder_reverse: return pairmate ? m_fullFrom + m_length[1] - 1 : m_fullTo;
    default: THROW( logic_error, "Oops in CHit::GetFrom" );
    }
}

inline int CHit::GetTo( int pairmate ) const
{
    // here logically we invert fRead?_reverse flags, i.e. it should return same values 
    // as m_flags ^= (fRead1_reverse|fRead2_reverse); GetFrom( pairmate ); 
    switch( m_flags & fMaskGeometry ) {
    case fRead1_reverse|fRead2_reverse|fOrder_forward: return pairmate ? m_fullTo - m_length[1] + 1 : m_fullFrom;
    case fRead1_reverse|fRead2_reverse|fOrder_reverse: return pairmate ? m_fullFrom : m_fullTo - m_length[1] + 1;
    case fRead1_reverse|fRead2_forward|fOrder_forward: return pairmate ? m_fullTo : m_fullFrom;
    case fRead1_reverse|fRead2_forward|fOrder_reverse: return pairmate ? m_fullFrom + m_length[1] - 1 : m_fullTo - m_length[0] + 1;
    case fRead1_forward|fRead2_reverse|fOrder_forward: return pairmate ? m_fullTo - m_length[1] + 1 : m_fullFrom + m_length[0] - 1;
    case fRead1_forward|fRead2_reverse|fOrder_reverse: return pairmate ? m_fullFrom : m_fullTo;
    case fRead1_forward|fRead2_forward|fOrder_forward: return pairmate ? m_fullTo : m_fullFrom + m_length[0] - 1;
    case fRead1_forward|fRead2_forward|fOrder_reverse: return pairmate ? m_fullFrom + m_length[1] - 1 : m_fullTo;
    default: THROW( logic_error, "Oops in CHit::GetTo" );
    }
}

inline void CHit::C_NextCtl::SetNext( CHit * hit ) const 
{
    ASSERT( m_hit != 0 && m_hit->IsNull() == false );
    ASSERT( (m_hit->m_flags & fDeleted) == 0 );
    ASSERT( (m_hit->m_flags & fPurged) != 0 );
    if( hit && !hit->IsNull() ) {
        ASSERT( (hit->m_flags & fDeleted) == 0 );
        ASSERT( (hit->m_flags & fPurged) != 0 );
    }
    // Let's stick to simple delegation of permissions
    m_hit->m_next = hit;
}

END_OLIGOFAR_SCOPES

#endif
