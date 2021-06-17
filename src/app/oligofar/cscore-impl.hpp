#ifndef OLIGOFAR_CSCORE_IPML_HPP
#define OLIGOFAR_CSCORE_IPML_HPP

#include "iscore.hpp"

BEGIN_OLIGOFAR_SCOPES

class AScoreWithParam : public IScore
{
public:
    AScoreWithParam( const CScoreParam * param ) : m_scoreParam( param ) {}
    const CScoreParam * GetScoreParam() const { return m_scoreParam; }
protected:
    const CScoreParam * m_scoreParam;
};

class CCvtQryNcbi8naEquiv
{
public:
    static unsigned char GetBase( char base ) { return base; }
};
    
template<typename Type,int cnt>
class CShr
{
public:
    Type operator () ( const Type& val ) const { return val >> cnt; }
};

template<typename Type,int cnt>
class CShl
{
public:
    Type operator () ( const Type& val ) const { return val << cnt; }
};

template<int mask, typename Shift>
class CCvtQryNcbi8naBisulfite
{
public:
    CCvtQryNcbi8naBisulfite() { for( unsigned i = 0; i < 16; ++i ) m_tbl[i] = i | m_shift( i & mask );  }
    unsigned char GetBase( char base ) const { return m_tbl[int(base)]; }
protected:
    Shift m_shift;
    unsigned char m_tbl[16];
};

typedef CCvtQryNcbi8naBisulfite<8,CShr<Uint1,2> > CCvtQryNcbi8naTC;
typedef CCvtQryNcbi8naBisulfite<1,CShl<Uint1,2> > CCvtQryNcbi8naAG;

class CCvtSubjNcbi8naFwd
{
public:
    unsigned char GetBase( const char * base ) const { return *base; }
    CSeqCoding::EStrand GetStrand() const { return CSeqCoding::eStrand_pos; }
    int GetDibaseInc() const { return -1; }
};

class CCvtSubjNcbi8naRev
{
public:
    unsigned char GetBase( const char * base ) const { return CNcbi8naBase( base ).Complement(); }
    CSeqCoding::EStrand GetStrand() const { return CSeqCoding::eStrand_neg; }
    int GetDibaseInc() const { return +1; }
};

template<class TCvtQry, class TCvtSubj>
class CNcbi8naScore : public AScoreWithParam
{
public:
    CNcbi8naScore( const CScoreParam * sp ) : AScoreWithParam( sp ) {}
    double BestScore( const char * query ) const { return m_scoreParam->GetIdentityScore(); }
    double Score( const char * query, const char * subj ) const {
        unsigned char q = m_cvtQry.GetBase( *query );
        unsigned char s = m_cvtSubj.GetBase( subj );
        return q & s ? m_scoreParam->GetIdentityScore( s ) : -m_scoreParam->GetMismatchPenalty();
    }
protected:
    TCvtQry  m_cvtQry;
    TCvtSubj m_cvtSubj;
};

template<class TCvtQry, class TCvtSubj>
class CNcbiqnaScore : public AScoreWithParam
{
public:
    CNcbiqnaScore( const CScoreParam * sp ) : AScoreWithParam( sp ) {}
    double BestScore( const char * query ) const { return m_scoreParam->GetIdentityScore(); }
    double Score( const char * query, const char * subj ) const {
        unsigned char q = m_cvtQry.GetBase( CNcbi8naBase( CNcbiqnaBase( *query ) ) );
        unsigned char s = m_cvtSubj.GetBase( subj );
        return q & s ? m_scoreParam->GetIdentityScore( s ) : WorstScore( query );
    }
    double WorstScore( const char * query ) const { 
        double ret = -m_scoreParam->GetMismatchScoreForPhrap(CNcbiqnaBase( query ).GetPhrapScore()); 
        if( ret >= 0 ) {
            THROW( logic_error, "Unexpected value in the worst score: " << DISPLAY( ret ) 
                    << DISPLAY( CIupacnaBase( CNcbiqnaBase( query ) ) )
                    << DISPLAY( CNcbiqnaBase( query ).GetPhrapScore() ) 
                    << "\t" << hex << setw(2) << setfill('0') << Uint2( Uint1(*query) ) );
        }
        return ret;
    }
protected:
    TCvtQry  m_cvtQry;
    TCvtSubj m_cvtSubj;
};

class CCvtQryNcbipnaEquiv
{
public:
    static unsigned char GetBase( const char * base, int c ) { return base[c]; }
};

// TODO: this scoring will not work precisely as supposed with bisulfite treatment...
// it effectively will penalize Ts or As. 

template<int from, int to>
class CCvtQryNcbipnaBisulfite
{
public:
    CCvtQryNcbipnaBisulfite() { for( unsigned i = 0; i < 5; ++i ) m_tbl[i] = i; m_tbl[from] = to; }
    unsigned char GetBase( const char * base, int c ) const { return max( base[c], base[m_tbl[c]] ); }
protected:
    unsigned char m_tbl[5];
};

typedef CCvtQryNcbipnaBisulfite<3,1> CCvtQryNcbipnaTC;
typedef CCvtQryNcbipnaBisulfite<0,2> CCvtQryNcbipnaAG;

template<class TCvtQry, class TCvtSubj>
class CNcbipnaScore : public AScoreWithParam
{
public:
    CNcbipnaScore( const CScoreParam * sp ) : AScoreWithParam( sp ) {}
    double BestScore( const char * query ) const { return double(query[4])/255; }
    double Score( const char * query, const char * subj ) const {
        unsigned char s = m_cvtSubj.GetBase( subj );
        double bs = BestScore( query );
        double id = + m_scoreParam->GetIdentityScore( s ) / 255 * bs;
        double mi = - m_scoreParam->GetMismatchPenalty() / 255 / 3;
        double sc = 
            ( s & 1 ? id : mi ) * m_cvtQry.GetBase( query, 0 ) +
            ( s & 2 ? id : mi ) * m_cvtQry.GetBase( query, 1 ) +
            ( s & 4 ? id : mi ) * m_cvtQry.GetBase( query, 2 ) +
            ( s & 8 ? id : mi ) * m_cvtQry.GetBase( query, 3 );
        ASSERT( bs >= sc );
        return sc;
    }
protected:
    TCvtQry  m_cvtQry;
    TCvtSubj m_cvtSubj;
};

class CDibaseScore_Base
{
protected:
    CDibaseScore_Base( const CScoreParam * sp ) {
        fill( m_scoretbl, m_scoretbl + 1024, - sp->GetMismatchPenalty() );
        for( int s0 = 0; s0 < 16; ++s0 ) {
            for( int s1 = 0; s1 < 16; ++s1 ) {
                for( const char * c0 = m_ncbi8natbl[s0]; *c0 != '\xff'; ++c0 ) {
                    for( const char * c1 = m_ncbi8natbl[s1]; *c1 != '\xff'; ++c1 ) {
                        CColorTwoBase cb = CColorTwoBase( CNcbi2naBase( *c0 ), CNcbi2naBase( *c1 ) );
                        double& x = m_scoretbl[s0 + (s1 << 4) + (cb.GetColorOrd() << 8)];
                        x = max( x, min( sp->GetIdentityScore( s0 ), sp->GetIdentityScore( s1 ) ) );
                    }
                }
            }
        }
    }
    double m_scoretbl[1024];
    static const char * m_ncbi8natbl[16];
};

template<class TCvtQry, class TCvtSubj>
class CDibaseScore : public CNcbi8naScore<TCvtQry,TCvtSubj>, public CDibaseScore_Base
{
public:
    typedef CNcbi8naScore<TCvtQry,TCvtSubj> TSuper;
    CDibaseScore( const CScoreParam * sp ) : CNcbi8naScore<TCvtQry,TCvtSubj>( sp ), CDibaseScore_Base( sp ) {}
    double GetScore( bool match, unsigned char subjNcbi8na ) const {
        return match ? 
            +TSuper::m_scoreParam->GetIdentityScore( subjNcbi8na ) : 
            -TSuper::m_scoreParam->GetMismatchPenalty();
    }
    double Score( const char * query, const char * subj ) const {
        CColorTwoBase q( *query );
        if( CNcbi8naBase( q.GetBaseCalls() ) ) {
            CNcbi8naBase s( TSuper::m_cvtSubj.GetBase( subj ) );
            return GetScore( TSuper::m_cvtQry.GetBase( q.GetBaseCalls() ) & s, s ); 
        } else {
            // TODO: use m_cvtQry somewhere here
            return m_scoretbl[(q.GetColorOrd() << 8) + (subj[TSuper::m_cvtSubj.GetDibaseInc()] << 4) + subj[0]];
        }
    }
};

END_OLIGOFAR_SCOPES

#endif
