#ifndef OLIGOFAR_CQUERYHASH__HPP
#define OLIGOFAR_CQUERYHASH__HPP

#include "uinth.hpp"
#include "cquery.hpp"
#include "fourplanes.hpp"
#include "cpermutator8b.hpp"
#include "cbitmaskaccess.hpp"
#include "array_set.hpp"
#include "cwordhash.hpp"
#include "chashparam.hpp"
#include "cintron.hpp"

#include <deque>

BEGIN_OLIGOFAR_SCOPES

////////////////////////////////////////////////////////////////////////
// 0....|....1....|....2
// 111010111111110111111 18 bits
// aaaaaaaaaaa            9 bits
//           bbbbbbbbbbb 10 bits
class CHashPopulator;
class CQueryHash
{
public:
    typedef array_set<Uint1> TSkipPositions;
    typedef vector<CHashAtom> TMatchSet;

    enum EStatus {
        eStatus_empty,
        eStatus_dirty,
        eStatus_ready
    };

    enum EIndelLocation { eIndel_anywhere = 0 };

    ~CQueryHash();
    CQueryHash( int maxm = 2, int maxa = 4 );

    void SetHashWordBitmask( CBitmaskAccess * wbm ) { m_wbmAccess = wbm; }
    void SetWindowSize( int winSize );
    void SetWindowStep( int winStep );
    void SetWindowStart( int winStart );
    void SetWordSize( int wordSz );
    void SetStrideSize( int stride );
    void SetMaxMismatches( int mismatches );
    void SetMaxAmbiguities( int ambiguities );
    void SetMaxInsertions( int ins );
    void SetMaxDeletions( int del );
    void SetMaxDistance( int dist );
    void SetIndelPosition( int pos );
    void SetIndexBits( int bits );
    void SetStrands( int strands );
    void SetMaxSimplicity( double simpl );
    void SetMaxWindowCount( int windows );
    void SetNaHSO3mode( bool on = true );
    void SetHaveSplices( bool on = true ) { m_haveSplices = on; }

    void SetParam( const CHashParam& param );
    void GetParam( CHashParam& dest ) const;

    void SetExpectedReadCount( int count ) { m_expectedCount = count; }

    CBitmaskAccess * GetHashWordBitmask() const { return m_wbmAccess; }
    int  GetScannerWindowLength() const { return m_scannerWindowLength; }
    int  GetMaxWindowCount() const { return m_maxWindowCount; }
    int  GetWindowSize() const { return m_windowSize; }
    int  GetWindowStep() const { return m_windowStep; }
    int  GetWindowStart() const { return m_windowStart; }
    int  GetWordSize() const { return m_wordSize; }
    int  GetWordOffset() const { return m_wordOffset; }
    int  GetStrideSize() const { return m_strideSize; }
    int  GetMaxMismatches() const { return m_maxMism; }
    int  GetMaxAmbiguities() const { return m_maxAmb; }
    int  GetMaxInsertions() const { return m_maxIns; }
    int  GetMaxDeletions() const { return m_maxDel; }
    int  GetMaxDistance() const { return m_maxDist; }
    int  GetIndelPosition() const { return m_indelPos; }
    bool GetNaHSO3mode() const { return m_nahso3mode; }
    int  GetIndexBits() const { return m_hashTable.GetIndexBits(); }
    int  GetStrands() const { return m_strands; }
    int  GetExpectedReadCount() const { return m_expectedCount; }
    int  GetHashedQueriesCount() const { return m_hashedCount; }
    double GetMaxSimplicity() const { return m_maxSimplicity; }
    bool HaveSplices() const { return m_haveSplices; }
    
    int GetAbsoluteMaxMism() const { return m_permutators.size() - 1; }
    bool CanOptimizeOffset() const { return m_skipPositions.size() == 0 && m_haveSplices == false; }

    template<class iterator>  void SetSkipPositions( iterator begin, iterator end );
    template<class container> void SetSkipPositions( const container& c );
    
    bool HasSkipPositions() const { return !m_skipPositions.empty(); }
    const TSkipPositions& GetSkipPositions() const { return m_skipPositions; }

    void Clear() { m_status = eStatus_empty; m_hashedCount = 0; m_hashTable.Clear(); }
    void Freeze();
    void CheckConstraints();

    int AddQuery( CQuery * query );
    int AddQuery( CQuery * query, int component, int position, int wordcnt = 1, int wstep = 1 );

    /* ========================================================================
     * This code is not used and may be outdated and even have errors
     * ========================================================================
    template<class Callback> void ForEach4gnl( const UintH& ncbi4na, Callback& cbk ) const;
    template<class Callback> void ForEach4( const UintH& ncbi4na, Callback& cbk ) const;
    template<class Callback> void ForEach4( UintH hash, Callback& callback, Uint1 mask, Uint1 flags ) const;
    template<class Callback> void ForEach4( const UintH& ncbi4na, Callback& cbk ) {
        if( m_status == eStatus_dirty ) Freeze();
        ForEach4gnl( ncbi4na, cbk );
    }
    * ======================================================================== */
    template<class Callback> void ForEach2gnl( const Uint8& ncbi2na, Callback& cbk ) const;
    template<class Callback> void ForEach2str( const Uint8& ncbi2na, Callback& cbk ) const;
    template<class Callback> void ForEach2int( Uint8 hash, Callback& callback, Uint1 mask, Uint1 flags ) const;
    template<class Callback> void ForEach2( const Uint8& ncbi2na, Callback& cbk ) const { 
        ForEach2gnl( ncbi2na, cbk ); 
    }
    template<class Callback> void ForEach2( const Uint8& ncbi2na, Callback& cbk ) {
        if( m_status == eStatus_dirty ) Freeze();
        ForEach2gnl( ncbi2na, cbk );
    }

    Uint2  ComputeHashPreallocSz() const;
    Uint8  ComputeEntryCountPerReadWord( Uint2 w, bool removeDupes = false ) const; // no ambiguities are allowed
    Uint8  ComputeEntryCountPerReadMmOnly( Uint2 w, int maxMm ) const;
    Uint8  ComputeEntryCountPerRead( bool removeDupes = false ) const;
    int    ComputeHasherWindowLength();
    int    ComputeHasherWindowStep();
    int    ComputeScannerWindowLength();
    int    GetHasherWindowLength() const { return m_windowLength; }
    int    GetHasherWindowStep() const { return m_windowStep > 0 ? m_windowStep : GetHasherWindowLength(); }
    UintH  ComputeAlternatives( UintH w, int l ) const; // used to optimize window
    int    ComputeAmbiguitiesNcbi8na( UintH w, int len ) const;
    double ComputeComplexityNcbi8na( UintH w, int len, int& amb ) const;
    double ComputeComplexityNcbi2na( Uint8 w, int len ) const;

    template<class Uint, int bpb> void CompressFwd( Uint& window ) const;
    template<class Uint, int bpb> void CompressRev( Uint& window ) const;

    void SetNcbipnaToNcbi4naScore( Uint2 score ) { m_ncbipnaToNcbi4naScore = score; }
    void SetNcbiqnaToNcbi4naScore( Uint2 score ) { m_ncbiqnaToNcbi4naScore = score; }

protected:

    template<class Callback>
    class C_ScanCallback
    {
    public:
        C_ScanCallback( const CQueryHash& caller, Callback& cbk, Uint1 mask, Uint1 flags ) : 
            m_caller( caller ), m_callback( cbk ), m_mask( mask ), m_flags( flags ) {}
        void operator () ( Uint8 hash, int mism, unsigned amb ) const;
    protected:
        const CQueryHash& m_caller;
        Callback& m_callback;
        Uint1 m_mask;
        Uint1 m_flags;
    };

    class C_ListInserter
    {
    public:
        C_ListInserter( TMatchSet& ms ) : m_matchSet( ms ) {}
        void operator () ( const CHashAtom& atom ) { m_matchSet.push_back( atom ); }
    protected:
        TMatchSet& m_matchSet;
    };

    typedef UintH TCvt( const unsigned char *, int, unsigned short );
    typedef Uint2 TDecr( Uint2 );

    bool CheckWordConstraints();
    bool CheckWordConstraints() const;
    
    int  x_AddQuery( CQuery * query, int component );
    int  x_AddQuery( CQuery * query, int component, int off, int maxWinCnt, int wstep );
    int  PopulateWindow( CHashPopulator& dest, UintH fwindow, int offset, int ambiguities );
    void PopulateInsertion2( CHashPopulator& dest, const UintH& maskH, const UintH& fwindow, int pos );
    void PopulateInsertion1( CHashPopulator& dest, const UintH& maskH, const UintH& fwindow, int pos );
    void PopulateInsertions( CHashPopulator& dest, const UintH& maskH, const UintH& fwindow );
    void PopulateDeletions ( CHashPopulator& dest, const UintH& maskH, const UintH& fwindow );
    void PopulateDeletion2 ( CHashPopulator& dest, const UintH& maskH, const UintH& fwindow, int pos );
    void PopulateDeletion1 ( CHashPopulator& dest, const UintH& maskH, const UintH& fwindow, int pos );
    int  GetNcbi4na( UintH& window, CSeqCoding::ECoding, const unsigned char * data, unsigned length );

    int x_GetNcbi4na_ncbi8na( UintH& window, const unsigned char * data, unsigned length );
    int x_GetNcbi4na_colorsp( UintH& window, const unsigned char * data, unsigned length );
    int x_GetNcbi4na_ncbipna( UintH& window, const unsigned char * data, unsigned length );
    int x_GetNcbi4na_ncbiqna( UintH& window, const unsigned char * data, unsigned length );
    int x_GetNcbi4na_quality( UintH& window, const unsigned char * data, unsigned length, TCvt * fun, int incr, unsigned short score, TDecr * decr );

    static Uint2 x_UpdateNcbipnaScore( Uint2 score ) { return score /= 2; }
    static Uint2 x_UpdateNcbiqnaScore( Uint2 score ) { return score - 1; }
    static TCvt  x_Ncbipna2Ncbi4na;
    static TCvt  x_Ncbiqna2Ncbi4na;

    //static Uint8 x_Convert2( Uint8 word, unsigned len, Uint8 from, Uint8 to );
    static Uint8 x_Convert2( Uint8 word, unsigned len, const unsigned char * tbl );
    static UintH x_Convert4( UintH word, unsigned len, UintH mask, int shr );
    static UintH x_Convert4tc( UintH word, unsigned len );
    static UintH x_Convert4ag( UintH word, unsigned len );

//    template<class Callback> void ForEach( Uint8 ncbi2na, Callback& cbk, Uint1 mask, Uint1 flags );

protected:

    EStatus m_status;
    int m_windowLength;
    int m_windowStep;
    int m_windowStart;
    int m_scannerWindowLength;
    int m_windowSize;
    int m_strideSize;
    int m_wordSize;
    int m_maxWindowCount;
    int m_strands;
    int m_expectedCount;
    int m_hashedCount;
    int m_maxAmb;
    int m_maxMism;
    int m_maxIns;
    int m_maxDel;
    int m_maxDist;
    int m_indelPos;
    bool m_nahso3mode;
    bool m_haveSplices;
    double m_maxSimplicity;
    Uint2 m_ncbipnaToNcbi4naScore;
    Uint2 m_ncbiqnaToNcbi4naScore;
    TSkipPositions m_skipPositions;
    CWordHash m_hashTable;
    vector<CPermutator8b*> m_permutators;
    // all masks are ncbi2na, i.e. 2 bits per base
    Uint8 m_wordMask; 
    int m_wordOffset;
    CBitmaskAccess * m_wbmAccess;

    mutable Uint8 m_hashPopulatorReverve;
    mutable TMatchSet m_listA, m_listB; // to keep memory space reserved between ForEach calls
    static const unsigned char * s_convert2tc;
    static const unsigned char * s_convert2ag;
    static const unsigned char * s_convert2eq;
};

////////////////////////////////////////////////////////////////////////
// inline implementation follows

inline CQueryHash::~CQueryHash() 
{
    for( int i = 0; i < int( m_permutators.size() ); ++i )
        delete m_permutators[i];
}

inline CQueryHash::CQueryHash( int maxm, int maxa ) :
    m_status( eStatus_empty ),
    m_windowLength( 22 ),
    m_windowStep( 1 ),
    m_windowStart( 0 ),
    m_scannerWindowLength( 22 ),
    m_windowSize( 22 ),
    m_strideSize( 1 ),
    m_wordSize( 11 ),
    m_maxWindowCount( 1 ),
    m_strands( 3 ),
    m_expectedCount( 1000000 ),
    m_hashedCount( 0 ),
    m_maxAmb( maxa ),
    m_maxMism( min(1, maxm) ),
    m_maxIns( 0 ),
    m_maxDel( 0 ),
    m_maxDist( 0 ),
    m_indelPos( eIndel_anywhere ),
    m_nahso3mode( false ),
    m_haveSplices( false ),
    m_maxSimplicity( 2.0 ),
    m_ncbipnaToNcbi4naScore( 0x7f ),
    m_ncbiqnaToNcbi4naScore( 3 ),
    m_hashTable( m_wordSize*2 ),
    m_wordMask( CBitHacks::WordFootprint<Uint8>( m_wordSize*2 ) ),
    m_wordOffset( m_windowSize - m_wordSize ),
    m_wbmAccess( 0 )
{
    SetMaxMismatches( maxm );
}

template<class iterator>
inline void CQueryHash::SetSkipPositions( iterator begin, iterator end ) 
{
    m_skipPositions.clear();
    for( ; begin != end ; ++begin ) {
        if( *begin <= 0 ) continue;
        m_skipPositions.insert( *begin );
    }
    if( m_maxWindowCount > 1 && m_skipPositions.size() )
        THROW( logic_error, "Skip positions are not supported with max word count > 1" );
    ComputeScannerWindowLength();
}
    
template<class container>
inline void CQueryHash::SetSkipPositions( const container& c ) 
{
    SetSkipPositions( c.begin(), c.end() );
}

inline void CQueryHash::SetWindowSize( int winSize ) 
{ 
    ASSERT( m_status == eStatus_empty );
    m_windowSize = winSize; 
    if( m_wordSize > m_windowSize ) {
        m_wordSize = m_windowSize;
        m_wordMask = CBitHacks::WordFootprint<Uint8>( 2*m_wordSize );
        m_wordOffset = 0;
    } else m_wordOffset = m_windowSize - m_wordSize;
//    ComputeScannerWindowLength();
}

inline void CQueryHash::SetWindowStep( int step )
{
    ASSERT( m_status == eStatus_empty );
    ASSERT( step >= 0 );
    m_windowStep = step;
}

inline void CQueryHash::SetWindowStart( int start )
{
    ASSERT( m_status == eStatus_empty );
    ASSERT( start >= 0 );
    m_windowStart = start;
}

inline void CQueryHash::SetMaxWindowCount( int winCnt ) 
{ 
    ASSERT( m_status == eStatus_empty );
    m_maxWindowCount = winCnt; 
    if( m_maxWindowCount > 1 && m_skipPositions.size() )
        THROW( logic_error, "Skip positions are not supported with max word count > 1" );
    m_wordOffset = m_windowSize - m_wordSize;
}

inline void CQueryHash::SetWordSize( int wordSize ) 
{ 
    ASSERT( m_status == eStatus_empty );
    m_wordSize = wordSize; 
    m_wordMask = CBitHacks::WordFootprint<Uint8>( 2*m_wordSize );
    if( m_wordSize > m_windowSize ) m_windowSize = m_wordSize;
    m_wordOffset = m_windowSize - m_wordSize;
    ComputeScannerWindowLength();
}

inline int CQueryHash::AddQuery( CQuery * query )
{
    if( m_status == eStatus_empty ) CheckWordConstraints();
    
    m_status = eStatus_dirty;
    int ret = x_AddQuery( query, 0 );
    if( query->HasComponent( 1 ) ) ret += x_AddQuery( query, 1 );
    if( ret ) ++m_hashedCount;
    return ret;
}

inline int CQueryHash::AddQuery( CQuery * query, int component, int woffset, int wcount, int wstep )
{
    if( m_status == eStatus_empty ) CheckWordConstraints();
    m_status = eStatus_dirty;

    return x_AddQuery( query, component, woffset, wcount, wstep );
}


inline void CQueryHash::SetStrideSize( int stride ) 
{ 
    ASSERT( m_status == eStatus_empty ); 
    m_strideSize = stride; 
}
    
inline void CQueryHash::SetMaxMismatches( int mismatches ) 
{ 
    ASSERT( m_status == eStatus_empty ); 
    if( int(m_permutators.size()) <= mismatches ) {
        int oldsz = m_permutators.size();
        m_permutators.resize( mismatches + 1 );
        for( int i = oldsz; i <= mismatches; ++i ) 
            m_permutators[i] = new CPermutator8b( i, m_nahso3mode ? 32 : m_maxAmb );
    }
    m_maxMism = mismatches;
    if( m_maxDist < mismatches ) m_maxDist = mismatches;
}

inline void CQueryHash::SetMaxAmbiguities( int ambiguities ) 
{ 
    ASSERT( m_status == eStatus_empty ); 
    unsigned amb = m_nahso3mode ? 32 : ambiguities;
    m_maxAmb = ambiguities;
    for( unsigned i = 0; i < m_permutators.size(); ++i ) {
        if( m_permutators[i]->GetMaxAmbiguities() != amb ) {
            delete m_permutators[i];
            m_permutators[i] = new CPermutator8b( i, m_maxAmb );
        }
    }
}

inline void CQueryHash::SetMaxInsertions( int ins )      
{ 
    ASSERT( m_status == eStatus_empty ); 
    m_maxIns = ins; 
    if( m_maxDist < ins ) m_maxDist = ins;
}

inline void CQueryHash::SetMaxDeletions( int del )      
{ 
    ASSERT( m_status == eStatus_empty ); 
    m_maxDel = del; 
    if( m_maxDist < del ) m_maxDist = del;
}

inline void CQueryHash::SetMaxDistance( int dist )      
{ 
    ASSERT( m_status == eStatus_empty ); 
    m_maxDist = max( max( m_maxIns, m_maxDel ), max( m_maxMism, dist ) ); 
}

inline void CQueryHash::SetIndelPosition( int pos )      
{ 
    ASSERT( m_status == eStatus_empty ); 
    m_indelPos = pos; 
}

inline void CQueryHash::SetNaHSO3mode( bool on )      
{ 
    ASSERT( m_status == eStatus_empty ); 
    m_nahso3mode = on; 
    SetMaxAmbiguities( GetMaxAmbiguities() );
}

inline void CQueryHash::SetIndexBits( int bits )         
{ 
    ASSERT( m_status == eStatus_empty ); 
    m_hashTable.SetIndexBits( bits ); 
}

inline void CQueryHash::SetStrands( int strands )        
{ 
    ASSERT( m_status == eStatus_empty ); 
    m_strands = strands; 
}
    
inline void CQueryHash::SetMaxSimplicity( double simpl ) 
{ 
    ASSERT( m_status == eStatus_empty ); 
    m_maxSimplicity = simpl; 
}

inline Uint8 CQueryHash::ComputeEntryCountPerReadMmOnly( Uint2 w, int maxMm ) const
{
    Uint8 ret = 1;
    for( int i = 0; i < maxMm; ++i ) ret *= (w - i)*3;
    return ret;
}

inline Uint8 CQueryHash::ComputeEntryCountPerReadWord( Uint2 w, bool removeDupes ) const
{
    Uint8 ret = ComputeEntryCountPerReadMmOnly( w, GetMaxMismatches() );
    if( GetMaxInsertions() == 2 ) {
        Uint8 i2 = 16;
        if( m_indelPos == eIndel_anywhere ) {
            i2 *= (w - 2);
            if( removeDupes ) i2 -= (w - 3)*4 + 1;
        }
        i2 *= ComputeEntryCountPerReadMmOnly( w, min( GetMaxMismatches(), GetMaxDistance() - 2 ) );
        ret += i2;
    }
    if( GetMaxInsertions() ) {
        Uint8 i1 = 4;
        if( m_indelPos == eIndel_anywhere ) {
            i1 *= (w - 1);
            if( removeDupes ) i1 -= (w - 2) + 1;
        }
        i1 *= ComputeEntryCountPerReadMmOnly( w, min( GetMaxMismatches(), GetMaxDistance() - 1 ) );
        ret += i1;
    }
    if( GetMaxDeletions() == 2 ) {
        Uint8 d2 = 1;
        if( m_indelPos == eIndel_anywhere ) d2 *= (w - 1);
        d2 *= ComputeEntryCountPerReadMmOnly( w, min( GetMaxMismatches(), GetMaxDistance() - 2 ) );
        ret += d2;
    }
    if( GetMaxDeletions() ) {
        Uint8 d1 = 1;
        if( m_indelPos == eIndel_anywhere ) d1 *= w;
        d1 *= ComputeEntryCountPerReadMmOnly( w, min( GetMaxMismatches(), GetMaxDistance() - 1 ) );
        ret += d1;
    }
    return ret;
}
    
inline Uint8 CQueryHash::ComputeEntryCountPerRead( bool removeDupes ) const 
{ 
    Uint8 ret = ComputeEntryCountPerReadWord( m_wordSize/*m_windowSize + m_strideSize - 1*/, removeDupes ); // TODO: check what is used an wsize
    if( m_wordOffset ) ret *= 2; // for each word
    if( (m_strands&3) == 3 ) ret *= 2; // for each strand
    ret *= m_maxWindowCount;
    return ret;
}

inline int CQueryHash::ComputeHasherWindowLength()
{
    ASSERT( m_strideSize == 1 || m_skipPositions.size() == 0 );
    m_windowLength = m_windowSize + m_strideSize - 1 + GetMaxDeletions();
    ITERATE( TSkipPositions, p, m_skipPositions ) {
        if( *p <= m_windowLength ) ++m_windowLength; // m_skipPositions are sorted, unique and > 0 (1-based)
        else break;
    }
    return m_windowLength;
}

inline int CQueryHash::ComputeHasherWindowStep()
{
    return m_windowStep > 0 ? m_windowStep : ComputeHasherWindowLength();
}

inline int CQueryHash::ComputeScannerWindowLength()
{
    ASSERT( m_strideSize == 1 || m_skipPositions.size() == 0 );
    int winlen = m_windowSize; // indel and strides are taken into account in hash
    ITERATE( TSkipPositions, p, m_skipPositions ) { // just extend window to allow skipping
        if( *p <= winlen ) ++winlen; // m_skipPositions are sorted, unique and > 0 (1-based)
        else break;
    }
    return m_scannerWindowLength = winlen;
}

inline Uint2 CQueryHash::ComputeHashPreallocSz() const{ return 1; }

inline bool CQueryHash::CheckWordConstraints()
{
    ComputeHasherWindowLength();
    ComputeScannerWindowLength();
    m_hashTable.SetPreallocSz( ComputeHashPreallocSz() );
    m_hashPopulatorReverve = ComputeEntryCountPerRead();
    return ((const CQueryHash*)this)->CQueryHash::CheckWordConstraints();
}

inline bool CQueryHash::CheckWordConstraints() const
{
    ASSERT( m_skipPositions.size() == 0 || m_strideSize == 1 );
//    ASSERT( m_strideSize < m_wordSize );
//    ASSERT( m_strideSize < m_wordOffset + 1|| m_wordOffset == 0 );
    ASSERT( m_wordSize <= m_windowSize );
    ASSERT( m_wordSize * 2 >= m_windowSize );
    ASSERT( m_wordSize * 2 - m_hashTable.GetIndexBits() <= 16 ); // requirement is based on that CHashAtom may store only 16 bits
    ASSERT( m_windowLength <= 32 );
    return true;
}

template <typename Uint, int bpb>
inline void CQueryHash::CompressFwd( Uint& w ) const
{
    int x = 0;
    ITERATE( TSkipPositions, p, m_skipPositions ) {
        int pp = *p - m_windowStart;
        if( pp < 0 ) continue;
        if( pp > x ) w = CBitHacks::DeleteBits<Uint,bpb>( w, pp - ++x );
        if( pp > m_windowLength ) break;
    }
}

template <typename Uint, int bpb>
void CQueryHash::CompressRev( Uint& w ) const
{
    int x = m_scannerWindowLength;
    ITERATE( TSkipPositions, p, m_skipPositions ) {
        int pp = *p - m_windowStart;
        if( pp < 0 ) continue;
        if( pp < x ) w = CBitHacks::DeleteBits<Uint,bpb>( w, x - pp );
        if( pp > m_windowLength ) break;
    }
}

#if 0
template<class Callback>
void CQueryHash::ForEach4gnl( const UintH& hash, Callback& callback ) const 
{ 
    if( m_nahso3mode ) {
        ForEach4<Callback>( this->x_Convert4tc( hash, m_windowSize ), callback );
        ForEach4<Callback>( this->x_Convert4ag( hash, m_windowSize ), callback );
    } else { 
        ForEach4<Callback>( hash, callback );
    }
}

template<class Callback>
void CQueryHash::ForEach4( const UintH& hash, Callback& callback ) const 
{ 
    /* ........................................................................
       We change strategy here: instead of putting logic on varying hash window 
       by seqscanner, we to it in hash - so that hash is a black box

       Remember, that scanner WindowLength = either 
        (a) WindowSize + 0 * (StrideSize + IndelAllowed) 
       or
        (b) WindowSize + (number of skip positions - self conjugated)
       because it is not allowed to have strides and skip positions

       That's the window scanner should use.
       ........................................................................ */
    ASSERT( m_status != eStatus_dirty );

    if( m_strands == 3 & !HasSkipPositions() )
        ForEach4( hash, callback, 0, 0 );
    else {
        if( m_strands & 1 ) {
            UintH _hash = hash;
            if( HasSkipPositions() ) CompressFwd<UintH,4>( _hash );
            ForEach4( _hash, callback, CHashAtom::fMask_strand, CHashAtom::fFlag_strandFwd );
        }
        if( m_strands & 2 ) {
            UintH _hash = hash;
            if( HasSkipPositions() ) CompressRev<UintH,4>( _hash );
            ForEach4( _hash, callback, CHashAtom::fMask_strand, CHashAtom::fFlag_strandRev );
        }
    }
}
template<class Callback>
void CQueryHash::ForEach4( UintH hash, Callback& callback, Uint1 mask, Uint1 flags ) const
{
    THROW( logic_error, "ForEach4 should be modified in the way ForEach2 works in " << __FILE__ << ":" << __LINE__ );
    //Compress4fwd( hash );
    C_ScanCallback<Callback> cbk( *this, callback, mask, flags );
    m_permutators[0]->ForEach( m_windowSize, hash, cbk );
}

#endif
template<class Callback>
void CQueryHash::ForEach2gnl( const Uint8& hash, Callback& callback ) const 
{ 
    if( m_nahso3mode ) {
        ForEach2str<Callback>( x_Convert2( hash, m_windowSize, s_convert2tc ), callback );
        ForEach2str<Callback>( x_Convert2( hash, m_windowSize, s_convert2ag ), callback );
    } else {
        ForEach2str<Callback>( hash, callback );
    }
}

template<class Callback>
void CQueryHash::ForEach2str( const Uint8& hash, Callback& callback ) const 
{ 
    /* ........................................................................
       We change strategy here: instead of putting logic on varying hash window 
       by seqscanner, we to it in hash - so that hash is a black box

       Remember, that scanner WindowLength = either 
        (a) WindowSize + 0 * (StrideSize + IndelAllowed) 
       or
        (b) WindowSize + (number of skip positions - self conjugated)
       because it is not allowed to have strides and skip positions

       That's the window scanner should use.
       ........................................................................ */
    ASSERT( m_status != eStatus_dirty );

    if( m_strands == 3 && !HasSkipPositions() ) {
        ForEach2int( hash, callback, 0, 0 );
    } else {
        if( m_strands & 1 ) {
            Uint8 _hash = hash;
            if( HasSkipPositions() ) CompressFwd<Uint8,2>( _hash );
            ForEach2int( _hash, callback, CHashAtom::fMask_strand, CHashAtom::fFlag_strandFwd );
        }
        if( m_strands & 2 ) {
            Uint8 _hash = hash;
            if( HasSkipPositions() ) CompressRev<Uint8,2>( _hash );
            ForEach2int( _hash, callback, CHashAtom::fMask_strand, CHashAtom::fFlag_strandRev );
        }
    }
}

template<class Callback>
void CQueryHash::C_ScanCallback<Callback>::operator () ( Uint8 hash, int, unsigned ) const
{
    m_caller.ForEach2( hash, m_callback, m_mask, m_flags );
}

template<class Callback>
void CQueryHash::ForEach2int( Uint8 hash, Callback& callback, Uint1 mask, Uint1 flags ) const
{
    if( GetWordOffset() == 0 ) {
        m_hashTable.ForEach( hash, callback, mask, flags );
    } else {
        mask |= CHashAtom::fMask_wordId;

        Uint1 flags0 = flags | CHashAtom::fFlag_wordId0;
        Uint1 flags1 = flags | CHashAtom::fFlag_wordId1;

        TMatchSet& listA( m_listA );
        TMatchSet& listB( m_listB );

        listA.resize(0);
        listB.resize(0);

        Uint8 hashA = Uint8( hash >> ( GetWordOffset() * 2 ) );
        Uint8 hashB = Uint8( hash & CBitHacks::WordFootprint<Uint8>( 2*GetWordSize() ) ); //( ( Uint8(1) << ( 2 * GetWordSize() )) - 1 ) );

        C_ListInserter iA( listA );
        C_ListInserter iB( listB );
        
        m_hashTable.ForEach( hashA, iA, mask, flags0); 
        m_hashTable.ForEach( hashB, iB, mask, flags1); 

        if( listA.size() == 0 || listB.size() == 0 ) return;

        sort( listA.begin(), listA.end(), CHashAtom::LessQueryCoiterate );
        sort( listB.begin(), listB.end(), CHashAtom::LessQueryCoiterate );

        /*
        cerr << __FUNCTION__ << DISPLAY( listA.size() ) << DISPLAY( listB.size() ) << "\n";
        ITERATE( TMatchSet, a, listA ) cerr << "A: " << *a << "\n";
        ITERATE( TMatchSet, b, listB ) cerr << "B: " << *b << "\n";
        */

        TMatchSet::const_iterator a = listA.begin(), A = listA.end();
        TMatchSet::const_iterator b = listB.begin(), B = listB.end();

        while( a != A && b != B ) {

            if( CHashAtom::LessQueryCoiterate( *a, *b ) ) { ++a; continue; }
            if( CHashAtom::LessQueryCoiterate( *b, *a ) ) { ++b; continue; }

            //callback( a->GetStrandId() == CHashAtom::fFlag_strandFwd ? *b : *a ); // one time is enough
            CHashAtom w( *a, *b );
            callback( w );

            TMatchSet::const_iterator x = a; 

            do if( ! ( ++a < A ) ) return; while( *a == *x );
            do if( ! ( ++b < B ) ) return; while( *b == *x );
        }
    }
}

/*
inline Uint8 CQueryHash::x_Convert2( Uint8 word, unsigned len, Uint8 from, Uint8 to )
{
    Uint8 mask = 3;
    for( unsigned i = 0; i < len; ++i, (mask <<= 2) ) {
        if( ( word & mask ) == ( from & mask ) ) {
            word = (word & ~mask) | ( to & mask );
        }
    }
    return word;
}
*/

inline Uint8 CQueryHash::x_Convert2( Uint8 word, unsigned len, const unsigned char * tbl ) 
{
    //THROW( logic_error, "Please check usage of this function!!! it is not supposed to be used at all (2009.05.04)" );
    Uint8 ret = 0;
    for( int i = 0, I = len*2; i < I; i += 8 ) 
        ret |= tbl[(unsigned char)(word >> i)] << i;
    return ret;
}

inline UintH CQueryHash::x_Convert4( UintH word, unsigned len, UintH mask, int shr )
{
    //THROW( logic_error, "Please check usage of this function!!! it is not supposed to be used at all (2009.05.04)" );
    if( shr > 0 )
        word = (( word & mask ) >> (+shr) ) | ( word & ~mask );
    else
        word = (( word & mask ) << (-shr) ) | ( word & ~mask );
    return word;
}

inline UintH CQueryHash::x_Convert4tc( UintH word, unsigned len )
{
    static UintH allT( 0x8888888888888888ULL, 0x8888888888888888ULL );
    return (( word & allT ) >> 2 ) | ( word & ~allT );
}

inline UintH CQueryHash::x_Convert4ag( UintH word, unsigned len )
{
    static UintH allA( 0x1111111111111111ULL, 0x1111111111111111ULL );
    return (( word & allA ) << 2 ) | ( word & ~allA );
}

inline void CQueryHash::SetParam( const CHashParam& param ) 
{
    SetWordSize( param.GetWordSize() );
    SetWindowSize( param.GetWindowSize() );
    SetWindowStep( param.GetWindowStep() );
    SetWindowStart( param.GetWindowStart() );
    SetMaxWindowCount( param.GetWindowCount() );
    SetStrideSize( param.GetStrideSize() );
    SetIndexBits( param.GetHashBits() );
    SetMaxMismatches( param.GetHashMismatches() );
    SetMaxInsertions( param.GetHashInsertions() );
    SetMaxDeletions( param.GetHashDeletions() );
    SetMaxDistance( param.GetHashMaxDistance() );
    SetIndelPosition( param.GetHashIndelPosition() );
    SetSkipPositions( param.GetSkipPositions() );
    SetNcbipnaToNcbi4naScore( Uint2( 255 * pow( 10.0, double( param.GetPhrapCutoff())/10) ) );
    SetNcbiqnaToNcbi4naScore( param.GetPhrapCutoff() );
    SetMaxSimplicity( param.GetMaxSimplicity() );
    if( m_wbmAccess ) m_wbmAccess->SetRqWordSize( param.GetWindowSize() );
    ComputeHasherWindowLength();
    CheckConstraints();
}

inline void CQueryHash::GetParam( CHashParam& param ) const
{
    param.SetWindowSize( GetWindowSize() );
    param.SetWindowStep( GetWindowStep() );
    param.SetWindowStart( GetWindowStart() );
    param.SetWindowCount( GetMaxWindowCount() );
    param.SetStrideSize( GetStrideSize() );
    param.SetWordSize( GetWordSize() );
    param.SetHashBits( GetIndexBits() );
    param.SetHashMismatches( GetMaxMismatches() );
    param.SetHashInsertions( GetMaxInsertions() );
    param.SetHashDeletions( GetMaxDeletions() );
    param.SetMaxHashDistance( GetMaxDistance() );
    param.SetHashIndelPosition( GetIndelPosition() );
    param.SetSkipPositions( GetSkipPositions() );
    param.SetPhrapCutoff( m_ncbiqnaToNcbi4naScore );
    param.SetMaxSimplicity( m_maxSimplicity );
}

END_OLIGOFAR_SCOPES

#endif
