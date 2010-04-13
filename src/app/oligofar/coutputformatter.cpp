#include <ncbi_pch.hpp>
#include "coutputformatter.hpp"
#include "cguidefile.hpp"
#include "ialigner.hpp"
#include "chit.hpp"
#include "ctranscript.hpp"

USING_OLIGOFAR_SCOPES;

typedef TTrSequence TTranscript;

class CNcbi8naPrinter
{
public:
    CNcbi8naPrinter( const string& s ) : m_data(s) {}
    const char * data() const { return m_data.c_str(); }
    unsigned size() const { return m_data.size(); }
    string ToString() const;
protected:
    const string& m_data;
};

inline ostream& operator << ( ostream& o, const CNcbi8naPrinter& p ) 
{
    if( p.size() == 0 ) o.put('-'); 
    else for( unsigned i = 0; i < p.size(); ++i ) {
        o.put( CIupacnaBase( CNcbi8naBase( p.data()[i] ) ) );
    }
    return o;
}

class CNcbiqnaPrinter
{
public:
    CNcbiqnaPrinter( const string& s ) : m_data(s) {}
    const char * data() const { return m_data.c_str(); }
    unsigned size() const { return m_data.size(); }
protected:
    const string& m_data;
};

inline ostream& operator << ( ostream& o, const CNcbiqnaPrinter& p ) 
{
    if( p.size() == 0 ) o.put('-'); 
    else for( unsigned i = 0; i < p.size(); ++i ) {
        o.put( CIupacnaBase( CNcbiqnaBase( p.data()[i] ) ) );
    }
    return o;
}

class CNcbipnaPrinter
{
public:
    CNcbipnaPrinter( const char * data, unsigned len ) : m_data( data ), m_len( len ) {}
    const char * data() const { return m_data; }
    unsigned size() const { return m_len; }
protected:
    const char * m_data;
    unsigned m_len;
};

inline ostream& operator << ( ostream& o, const CNcbipnaPrinter& p ) 
{
    if( p.size() == 0 ) o.put('-'); 
    else for( unsigned i = 0; i < p.size(); ++i ) {
        o.put( CIupacnaBase( CNcbipnaBase( p.data() + i*5 ) ) );
    }
    return o;
}

const char * NullToDash( const char * str ) 
{
    return str && *str ? str : "-";
}

template<class Other>
class CEmptyToDash
{
public:
    CEmptyToDash( const Other& o ) : m_data( o ) {}
    friend ostream& operator << ( ostream& o, const CEmptyToDash<Other>& w ) {
        string x = w.m_data.ToString();
        if( x.length() ) o << x;
        else o << "-";
        return o;
    }
protected:
    const Other& m_data;
};


void COutputFormatter::FormatCore( const CHit * hit ) const
{
    ASSERT( hit->IsNull() == false );
#if DEVELOPMENT_VER
    double score1 = m_flags & fReportRawScore ? hit->GetScore(0) : hit->GetComponentFlags() & CHit::fComponent_1 ? 100*hit->GetScore(0)/hit->GetQuery()->ComputeBestScore(&m_scoreParam, 0) : 0;
    double score2 = m_flags & fReportRawScore ? hit->GetScore(1) : hit->GetComponentFlags() & CHit::fComponent_2 ? 100*hit->GetScore(1)/hit->GetQuery()->ComputeBestScore(&m_scoreParam, 1) : 0;
#else
    double score1 = m_flags & fReportRawScore ? hit->GetScore(0) : hit->GetComponentFlags() & CHit::fComponent_1 ? 100*hit->GetScore(0)/hit->GetQuery()->GetBestScore(0) : 0;
    double score2 = m_flags & fReportRawScore ? hit->GetScore(1) : hit->GetComponentFlags() & CHit::fComponent_2 ? 100*hit->GetScore(1)/hit->GetQuery()->GetBestScore(1) : 0;
#endif
    m_out 
        << hit->GetQuery()->GetId() << "\t"
        << GetSubjectId( hit->GetSeqOrd() ) << "\t" 
        << hit->GetComponentMask() << "\t"
        << (score1 + score2) << "\t";
    if( hit->GetComponentFlags() & CHit::fComponent_1 ) {
        m_out 
            << hit->GetFrom(0) + 1 << "\t"
            << hit->GetTo(0) + 1 << "\t"
            << (score1) << "\t";
    } else {
        m_out << "-\t-\t-\t";
    }
    if( hit->GetComponentFlags() & CHit::fComponent_2 ) {
        m_out 
            << hit->GetFrom(1) + 1 << "\t"
            << hit->GetTo(1) + 1 << "\t"
            << (score2) << "\t";
    } else {
        m_out << "-\t-\t-\t";
    }
    m_out 
        << hit->GetStrand() << "\t"
        << CNcbi8naPrinter( hit->GetTarget(0) ) << "\t"
        << CNcbi8naPrinter( hit->GetTarget(1) ) << "\t"
        << CEmptyToDash<TTranscript>( hit->GetTranscript(0) ) << "\t"
        << CEmptyToDash<TTranscript>( hit->GetTranscript(1) ) << "\n";
}

void COutputFormatter::FormatQueryHits( const CQuery * query )
{
    if( query == 0 ) return;
    if( int count = FormatQueryHits( query, 3, m_topCount ) ) {
        if( (m_flags & fReportPairesOnly) == 0 || count == m_topCount ) {
            FormatQueryHits( query, 1, count );
            FormatQueryHits( query, 2, count );
        }
    }
    delete query;
}

int COutputFormatter::FormatQueryHits( const CQuery * query, int mask, int topCount ) 
{
    if( topCount <= 0 ) return 0; // if paired read does not have hits at all we report this only once

    /*
    cerr << query->GetId() << DISPLAY( mask ) << DISPLAY( cutoff ) << DISPLAY( topCount ) << endl;
    if( query->GetTopHit(  3 ) ) cerr << DISPLAY( query->GetTopHit(  3 )->GetComponentScores( mask ) ) << endl;
    if( query->GetTopHit(mask) ) cerr << DISPLAY( query->GetTopHit(mask)->GetComponentScores( mask ) ) << endl;
    */
    if( CHit * hit = query->GetTopHit( mask ) ) { 
        double cutoff = max(
            query->GetTopHit( 3 )  ? query->GetTopHit( 3 ) ->GetComponentScores(mask) : 0 ,
            query->GetTopHit(mask) ? query->GetTopHit(mask)->GetComponentScores(mask) : 0 ) 
            * m_topPct/100;
        double ocutoff=  cutoff;
        if( (m_flags & fReportRawScore) == 0 ) {
#if DEVELOPMENT_VER
            ocutoff /= ((mask & 1 ? query->ComputeBestScore(&m_scoreParam, 0) : 0) + (mask & 2 ? query->ComputeBestScore(&m_scoreParam, 1) : 0 ));
#else
            ocutoff /= ((mask & 1 ? query->GetBestScore(0) : 0) + (mask & 2 ? query->GetBestScore(1) : 0 ));
#endif
            if( mask == 3 ) ocutoff*=2;
        }

        unsigned rank = 0;
        double lastScore = hit->GetTotalScore();
        for( int tt = 0 ; topCount > 0 && hit ; ++rank, ++tt ) {
            if( hit->IsNull() ) { 
                if( m_flags & fReportTerminator ) 
                    m_out << (rank) << "\tno_more\t" << query->GetId() << "\t-\t" << mask << "\t" << ocutoff << "+\t-\t-\t0\t-\t-\t0\t*\t-\t-\t-\t-\n";
                m_out << QuerySeparator(); 
                return topCount; 
            }
            ASSERT( hit->GetQuery() == query );
            ASSERT( hit->GetComponentMask() == mask );
            ASSERT( hit->GetNextHit() == 0 || hit->GetNextHit()->IsNull() || hit->GetNextHit()->GetTotalScore() <= hit->GetTotalScore() );
            unsigned rcnt = 1;
            if( hit->GetTotalScore() < cutoff ) {
                m_out << QuerySeparator();
                return topCount;
            }
            CHit * next = hit->GetNextHit();
            --topCount;
            for( ; topCount > 0 && next && (!next->IsNull()) && next->GetTotalScore() == hit->GetTotalScore(); next = next->GetNextHit(), ++rcnt, --topCount ) {
                ASSERT( next->GetQuery() == query );
                ASSERT( next->GetComponentMask() == mask );
                ASSERT( next->GetNextHit() == 0 || next->GetNextHit()->IsNull() || next->GetNextHit()->GetTotalScore() <= next->GetTotalScore() );
            };
            string srcnt = NStr::IntToString( rcnt );
            if( m_flags & fReportMany ) {
                if( next == 0 || next->GetTotalScore() == hit->GetTotalScore() ) srcnt += "+";
            }
            for( ; hit != next; hit = hit->GetNextHit() ) {
                ASSERT( hit->IsNull() == false );
                m_out
                    << rank << "\t"
                    << srcnt << "\t";
                FormatCore( hit );
                lastScore = hit->GetTotalScore();
                if( m_flags & (fReportDifferences/*|fReportAlignment*/) ) {
                    FormatDifferences( rank, hit, 0, m_flags );
                    if( query->IsPairedRead() ) FormatDifferences( rank, hit, 1, m_flags );
                }
            }
            
        }
        if( m_flags & fReportMore && ( hit == 0 || !hit->IsNull() ) ) 
            m_out << (rank-1) << "\tmore\t" << query->GetId() << "\t-\t" << mask << "\t" << lastScore << "-\t*\t*\t*\t*\t*\t*\t*\t*\t*\t*\t*\n";
        m_out << QuerySeparator();
    } else if( m_flags & fReportUnmapped && mask == 3 && query->GetTopHit(1) == 0 && query->GetTopHit(2) == 0 ) {
        string reasons( "none" );
        if( query->GetRejectReasons() & CQuery::fReject_short ) reasons += ",short";
        if( query->GetRejectReasons() & CQuery::fReject_loQual ) reasons += ",qual";
        if( query->GetRejectReasons() & CQuery::fReject_loCompl ) reasons += ",compl";
        m_out << "0\t" << reasons << "\t"<< query->GetId() << "\t-\t" << mask << "\t0\t-\t-\t0\t-\t-\t0\t*\t";
        
        if( query->GetCoding() == CSeqCoding::eCoding_ncbi8na ) {
            m_out << CNcbi8naPrinter( string(query->GetData(0), query->GetLength(0)) ) << "\t";
            if( query->HasComponent(1) ) 
                m_out << CNcbi8naPrinter( string(query->GetData(1), query->GetLength(1)) ) << "\t";
            else m_out << "-\t";
        } else if( query->GetCoding() == CSeqCoding::eCoding_ncbipna ) {
            m_out << CNcbipnaPrinter( query->GetData(0), query->GetLength(0) ) << "\t";
            if( query->HasComponent(1) ) 
                m_out << CNcbipnaPrinter( query->GetData(1), query->GetLength(1) ) << "\t";
            else m_out << "-\t";
        } else if( query->GetCoding() == CSeqCoding::eCoding_ncbiqna ) {
            m_out << CNcbiqnaPrinter( string(query->GetData(0), query->GetLength(0)) ) << "\t";
            if( query->HasComponent(1) ) 
                m_out << CNcbiqnaPrinter( string(query->GetData(1), query->GetLength(1)) ) << "\t";
            else m_out << "-\t";
        } else {
            // TODO: implement printer for Ncbipna
            m_out << "-\t-\t";
        }
        m_out << "-\t-\n"; // for CIGAR
        m_out << QuerySeparator();
        return -1;
    }
    
    return topCount;
}

void COutputFormatter::FormatHit( const CHit * hit ) 
{
    if( ! hit ) return;
    m_out 
        << "*\t" 
        << "hit\t";
    FormatCore( hit );
    /*
    if( m_flags & fReportAlignment ) {
        FormatDifferences( -1, hit, 0, fReportAlignment );
        FormatDifferences( -1, hit, 1, fReportAlignment );
    }
    */
}

BEGIN_OLIGOFAR_SCOPES

template<typename QCoding>
void COutputFormatter::CoTranslate( const char * q, const char * s, const CHit::TTranscript& trans, int rank, const CHit * hit, int matepair, int flags ) const 
{
    const int bsize = 4096;
    // buffers to accumulate differences
    char oqbuf[bsize];
    char osbuf[bsize];
    char oabuf[bsize];
    int opos = 0;
    int pos = -1;
    int qstep = QCoding::BytesPerBase();
    CSeqCoding::EStrand strand = hit->GetStrand( matepair );
    if( strand == CSeqCoding::eStrand_neg ) {
        q += (hit->GetQuery()->GetLength( matepair ) - 1)*qstep; 
        pos = hit->GetTo( matepair );
        qstep = -qstep;
    } else {
        pos = hit->GetFrom( matepair );
    }
    // we have to translate first, since even if we have match we may want to indicate difference
    // and we may want to merge adjacent differences to a single output
    for( TTranscript::const_iterator t = trans.begin(); t != trans.end() && *s ; ++t ) {
        int count = t->GetCount();
        ASSERT( count > 0 );
        switch( t->GetEvent() ) {
        case CTrBase::eEvent_Overlap:
        case CTrBase::eEvent_SoftMask:
        case CTrBase::eEvent_HardMask: q += qstep * count; break; // since query sequence is taken from separate file in full, H is same as S for us
        case CTrBase::eEvent_Splice:   s += count; pos += count; break; // just cut out intron
        case CTrBase::eEvent_Padding:  break; // ignore padding
        case CTrBase::eEvent_Insertion: 
            for( int i = 0; i < count; ++i, ++opos, (q += qstep) ) {
                oqbuf[opos] = CIupacnaBase( QCoding( q, strand ) );
                osbuf[opos] = '-';
                oabuf[opos] = t->GetCigar();
            }
            break;
        case CTrBase::eEvent_Deletion: 
            for( int i = 0; i < count; ++i, ++opos, ++s, ++pos ) {
                oqbuf[opos] = '-';
                osbuf[opos] = CIupacnaBase( CNcbi8naBase( s ) );
                oabuf[opos] = t->GetCigar();
            }
            break;
        case CTrBase::eEvent_Match:
        case CTrBase::eEvent_Changed:
        case CTrBase::eEvent_Replaced: 
            for( int i = 0; i < count; ++i, ++s, ++pos, (q += qstep) ) {
                oqbuf[opos] = CIupacnaBase( QCoding( q, strand ) );
                osbuf[opos] = CIupacnaBase( CNcbi8naBase( s ) );
                oabuf[opos] = t->GetCigar();
                if( t->GetEvent() == CTrBase::eEvent_Match && osbuf[opos] == oqbuf[opos] ) {
                    if( opos ) FormatDifference( rank, hit, matepair, oqbuf, osbuf, oabuf, pos - opos, opos, flags );
                    opos = 0;
                } else ++opos;
            }
            break;
        default: THROW( logic_error, "Invalid character [" << t->GetCigar() << "] in CIGAR output " << trans.ToString() );
        }
        ASSERT( opos < bsize );
    }
    if( opos ) FormatDifference( rank, hit, matepair, oqbuf, osbuf, oabuf, pos - opos, opos, flags );
}

inline char * Reverse( char * c, int cnt ) 
{
    for( int i = 0, j = cnt - 1; i < j; ++i, --j ) { swap( c[i], c[j] ); }
    return c;
}

inline char * ReverseComplement( char * c, int cnt ) 
{
    int i = 0, j = cnt - 1;
    for( ; i < j; ++i, --j ) { 
        CIupacnaBase ci( c + i ); 
        CIupacnaBase cj( c + j ); 
        c[i] = cj.Complement(); 
        c[j] = ci.Complement(); 
    } 
    if( i == j ) c[ i ] = CIupacnaBase( c + j ).Complement();
    return c;
}

inline char * Reverse( char * c, int cnt, CSeqCoding::EStrand s ) {
    return s == CSeqCoding::eStrand_neg ? Reverse( c, cnt ) : c;
}

inline char * ReverseComplement( char * c, int cnt, CSeqCoding::EStrand s ) {
    return s == CSeqCoding::eStrand_neg ? ReverseComplement( c, cnt ) : c;
}

template<>
void COutputFormatter::CoTranslate<CColorTwoBase>( const char * q, const char * s, const CHit::TTranscript& trans, int rank, const CHit * hit, int matepair, int flags ) const 
{
    const int bsize = 4096;
    // buffers to accumulate differences
    char oqbuf[bsize];
    char osbuf[bsize];
    char oabuf[bsize];

    int opos = 0;
    int pos = -1;
    int qstep = CColorTwoBase::BytesPerBase();
    int sstep = 1;
    CSeqCoding::EStrand strand = hit->GetStrand( matepair );

    TTranscript tt( trans );
    if( strand == CSeqCoding::eStrand_neg ) {
        tt.Reverse();
        sstep = -1;
        s += tt.ComputeSubjectLength() - 1;
        pos = hit->GetFrom( matepair );
    } else {
        pos = hit->GetFrom( matepair );
    }
   
    // we have to translate first, since even if we have match we may want to indicate difference
    // and we may want to merge adjacent differences to a single output
    CNcbi8naBase lastBase( '\x0' );
    ITERATE( TTranscript, it, tt ) {
        int count = it->GetCount(); 
        ASSERT( count > 0 );
        switch( it->GetEvent() ) {
        case CTrBase::eEvent_Overlap: 
        case CTrBase::eEvent_SoftMask: 
        case CTrBase::eEvent_HardMask: q += qstep * count; break;
        case CTrBase::eEvent_Splice:   s += sstep * count; pos += count * sstep; break; // just cut out intron
        case CTrBase::eEvent_Padding:  break; // ignore padding
        case CTrBase::eEvent_Insertion:
                  for( int i = 0; i < count; ++i, (q += qstep) ) {
                      CColorTwoBase c2b( *q );
                      CNcbi8naBase bc( c2b.GetBaseCalls() );
                      lastBase = bc ? bc : CNcbi8naBase( lastBase, c2b );
                      oqbuf[opos] = CIupacnaBase( lastBase );
                      osbuf[opos] = '-';
                      oabuf[opos] = it->GetCigar();
                      ++opos;
                  }
                  break;
        case CTrBase::eEvent_Deletion: 
                  for( int i = 0; i < count; ++i, (pos += sstep), (s += sstep) ) {
                      oqbuf[opos] = '-';
                      osbuf[opos] = CIupacnaBase( CNcbi8naBase( s, strand ) );
                      oabuf[opos] = it->GetCigar();
                      ++opos;
                  }
                  break;
        case CTrBase::eEvent_Match: 
        case CTrBase::eEvent_Changed: 
        case CTrBase::eEvent_Replaced: 
                  for( int i = 0; i < count; ++i, (s += sstep), (pos += sstep), (q += qstep) ) {
                      oabuf[opos] = it->GetCigar();
                      if( it->GetEvent() == CTrBase::eEvent_Match ) {
                          CColorTwoBase c2b( q );
                          lastBase = CNcbi8naBase( s );
                          oqbuf[opos] = osbuf[opos] = tolower( char(CIupacnaBase( lastBase )) );
                      } else {
                          if( CNcbi8naBase bc = CNcbi8naBase( CColorTwoBase( q ).GetBaseCalls() ) ) {
                              oqbuf[opos] = CIupacnaBase( lastBase = bc.Get( strand ) );
                          } else {
                              CColorTwoBase c2b( q );
                              CNcbi8naBase bc( lastBase, c2b );
                              lastBase = bc;
                              oqbuf[opos] = CIupacnaBase( bc );
                          }
                          osbuf[opos] = CIupacnaBase( CNcbi8naBase( s ) );
                      }
                      if( oqbuf[opos] == osbuf[opos] ) {
                          if( opos ) {
                              FormatDifference( rank, hit, matepair, 
                                      ReverseComplement( oqbuf, opos, strand ), 
                                      ReverseComplement( osbuf, opos, strand ),
                                      Reverse( oabuf, opos, strand ), 
                                      strand == CSeqCoding::eStrand_pos ? pos - opos : pos, opos, flags ); 
                              opos = 0;
                          }
                      } else ++opos;
                  }
            break;
        default: THROW( logic_error, "Invalid event [" << it->GetCigar() << "] in CIGAR output " << hit->GetTranscript( matepair ).ToString() );
        }
        ASSERT( opos < bsize );
    }
    oqbuf[opos] = oabuf[opos] = osbuf[opos] = 0;
    if( opos ) {
        FormatDifference( rank, hit, matepair, 
                ReverseComplement( oqbuf, opos, strand ), 
                ReverseComplement( osbuf, opos, strand ),
                Reverse( oabuf, opos, strand ), 
                strand == CSeqCoding::eStrand_pos ? pos - opos : pos, opos, flags ); 
    }
}

END_OLIGOFAR_SCOPES

void COutputFormatter::FormatDifferences( int rank, const CHit * hit, int matepair, int flags ) const
{
    if( 0 == ( flags & fReportDifferences ) ) return;
    if( hit->HasComponent( matepair ) == false ) return;
    //if( hit->GetScore( matepair ) == 100 ) return;
    const CQuery * query = hit->GetQuery();

    //int qstep = query->GetCoding() == CSeqCoding::eCoding_ncbipna ? 5 : 1;
    const CHit::TTranscript& t( hit->GetTranscript( matepair ) );
    const char * s = hit->GetTarget( matepair );
    const char * q = query->GetData( matepair );

    if( !*s ) return;
    switch( query->GetCoding() ) {
    case CSeqCoding::eCoding_ncbi8na: CoTranslate<CNcbi8naBase>( q, s, t, rank, hit, matepair, flags ); break;
    case CSeqCoding::eCoding_ncbipna: CoTranslate<CNcbipnaBase>( q, s, t, rank, hit, matepair, flags ); break;
    case CSeqCoding::eCoding_ncbiqna: CoTranslate<CNcbiqnaBase>( q, s, t, rank, hit, matepair, flags ); break;
    case CSeqCoding::eCoding_colorsp: CoTranslate<CColorTwoBase>( q, s, t, rank, hit, matepair, flags ); break;
    default: THROW( logic_error, "Unexpected query sequence coding" );
    }
}
    
void COutputFormatter::FormatDifference( int rank, const CHit* hit, int matepair, const char * qry, const char * sbj, const char * aln, int pos, int count, int flags ) const
{
    m_out 
        << rank << "\tdiff\t" 
        << hit->GetQuery()->GetId() << "\t" 
        << GetSubjectId( hit->GetSeqOrd() ) << "\t" 
        << (hit->GetComponentFlags() >> CHit::kComponents_bit) << "\t" 
        << hit->GetTotalScore() << "\t";

    if( matepair != 0 ) m_out << "-\t-\t-\t";
    m_out << (pos + 1) << "\t" << (pos + count) << "\t" << hit->GetScore( matepair ) << "\t";
    if( matepair == 0 ) m_out << "-\t-\t-\t";

    m_out << hit->GetStrand() << "\t";

    if( matepair != 0 ) m_out << "-\t";

    m_out.write( sbj, count );
    m_out << "=>";
    m_out.write( qry, count );
    m_out << "\t-\t";
    m_out.write( aln, count );
    m_out << "\t";
    if( matepair == 0 ) m_out << "-";
    m_out << "\n";
}

