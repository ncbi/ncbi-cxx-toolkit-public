#include <ncbi_pch.hpp>
#include "cseqscanner.hpp"
#include "cscancallback.hpp"
#include "cprogressindicator.hpp"
#include "cqueryhash.hpp"
#include "cfilter.hpp"
#include "cseqids.hpp"
#include "csnpdb.hpp"
#include "dust.hpp"

USING_OLIGOFAR_SCOPES;
USING_SCOPE(fourplanes);

void CSeqScanner::SequenceBegin( const TSeqIds& seqIds, int oid )
{
    if( m_seqIds ) {
		ASSERT( oid >= 0 );
		m_ord = m_seqIds->Register( seqIds, oid );
	}
    if( m_snpDb ) {
        for( TSeqIds::const_iterator i = seqIds.begin(); i != seqIds.end(); ++i ) {
            if( (*i)->IsGi() ) {
                m_snpDb->First( (*i)->GetGi() );
                break;
            }
        }
    }
}

void CSeqScanner::SequenceBuffer( CSeqBuffer* buffer ) 
{
    char * a = const_cast<char*>( buffer->GetBeginPtr() ); 
// TODO: hack! still need tochange it
    const char * A = buffer->GetEndPtr();
    unsigned off = buffer->GetBeginPos();
    unsigned end = buffer->GetEndPos();
    
    if( m_snpDb ) {
        for( ; m_snpDb->Ok() ; m_snpDb->Next() ) {
            unsigned p = m_snpDb->GetPos();
            if( p < off || p >= end ) continue;
            a[p - off] = m_snpDb->GetBase();
        }
    }

    if( m_queryHash->GetHashedQueriesCount() != 0 ) 
        if( m_run_old_scanning_code )
            ScanSequenceBuffer( a, A, off, end );
        else 
            ScanSequenceBuffer( a, A, off, end, buffer->GetCoding() );

    if( m_inputChunk ) {
		// set target for all reads 
        ITERATE( TInputChunk, q, (*m_inputChunk) ) {
            for( CHit * h = (*q)->GetTopHit(); h; h = h->GetNextHit() ) {
				if( (int)h->GetSeqOrd() == m_ord && h->TargetNotSet() ) {
                	h->SetTarget( 0, a, A );
                	h->SetTarget( 1, a, A );
				}
            }
        }
    }
}

void CSeqScanner::CreateRangeMap( TRangeMap& rangeMap, const char * a, const char * A )
{
    if( m_queryHash == 0 ) return;
    
	Uint8 altCount = 1;
    int ambCount = 0;
    int allowedMismatches = m_queryHash->GetMaxMism();
    
	const char * y = a;

    int winLen = m_queryHash->GetWindowLength();

	for( const char * Y = a + winLen; y < Y; ++y ) {
        CNcbi8naBase b( *y & 0x0f );
        int k = b.GetAltCount();
        if( k > 1 ) {
            altCount *= k;
            ambCount++;
        }
	}
	int lastPos = -1;
	ERangeType lastType = eType_skip;
	// TODO: somewhere here a check on range length should be inserted
	for( const char * z = a; y < A;  ) {
		ERangeType type = ( altCount == 1 || ambCount <= allowedMismatches ) ? eType_direct : ( altCount < m_maxAlternatives ) ? eType_iterate : eType_skip ;
		if( lastType != type ) {
			if( lastPos != -1 ) {
				if( lastType != eType_skip && rangeMap.back().second != eType_skip && 
					rangeMap.back().first.second - rangeMap.back().first.first + z - a - lastPos < m_minBlockLength ) {
					// merge too short blocks
					rangeMap.back().first.second = z - a;
					rangeMap.back().second = eType_iterate; // whatever it is - we drive it to larger of the two
				} else {
					rangeMap.push_back( make_pair( TRange( lastPos, z - a ), lastType ) );
				}
			}
			lastPos = z - a;
			lastType = type;
		}
        CNcbi8naBase bz( 0x0f & *z++ );
        CNcbi8naBase by( 0x0f & *y++ );
		altCount /= bz.GetAltCount();
		altCount *= by.GetAltCount();
        ambCount -= bz.GetAltCount() > 1 ? 1 : 0;
        ambCount += by.GetAltCount() > 1 ? 1 : 0;
	}
	rangeMap.push_back( make_pair( TRange( lastPos, A - a ), lastType ) );
}

////////////////////////////////////////////////////////////////////////
// ncbi8na
template<class Callback>
inline void CSeqScanner::C_LoopImpl_Ncbi8naNoAmbiguities::RunCallback( Callback& callback )
{
    callback( Uint8( m_hashCode.GetHashValue() ), 0, 1 );
}

template<class Callback>
inline void CSeqScanner::C_LoopImpl_Ncbi8naAmbiguities::RunCallback( Callback& callback )
{
    for( CHashIterator h( m_hashGenerator, m_mask4, m_mask8 ); h; ++h ) {
        callback( Uint8( *h ), 0, m_hashGenerator.GetAlternativesCount() );
    }
}

inline void CSeqScanner::C_LoopImpl_Ncbi8naNoAmbiguities::Prepare( char x )
{
    m_hashCode.AddBaseCode( Ncbi4na2Ncbi2na( x ) );
    m_complexityMeasure.Add( m_hashCode.GetNewestTriplet() );
}

inline void CSeqScanner::C_LoopImpl_Ncbi8naAmbiguities::Prepare( char x )
{
    m_hashGenerator.AddBaseMask( x );
    m_complexityMeasure.Add( m_hashGenerator.GetNewestTriplet() );
}

inline void CSeqScanner::C_LoopImpl_Ncbi8naNoAmbiguities::Update( char x )
{
    m_complexityMeasure.Del( m_hashCode.GetOldestTriplet() );
    Prepare( x );
}

inline void CSeqScanner::C_LoopImpl_Ncbi8naAmbiguities::Update( char x )
{
    m_complexityMeasure.Del( m_hashGenerator.GetOldestTriplet() );
    Prepare( x );
}

////////////////////////////////////////////////////////////////////////
// colorspace
inline void CSeqScanner::C_LoopImpl_ColorspNoAmbiguities::Prepare( char x )
{
    m_hashCode.AddBaseCode( CColorTwoBase( CNcbi8naBase( m_lastBase ), CNcbi8naBase( x ) ).GetColorOrd() );
    m_complexityMeasure.Add( m_hashCode.GetNewestTriplet() );
    m_lastBase = x;
}

inline void CSeqScanner::C_LoopImpl_ColorspAmbiguities::Prepare( char x )
{
    // todo: logic should be modified here to take care of previous bases
    CNcbi8naBase z = CNcbi8naBase( x & 0xf ).GetAltCount() > 1 ? '\xf' : ( 1 << CColorTwoBase( x ).GetColorOrd() );
    m_hashGenerator.AddBaseMask( CColorTwoBase( CNcbi8naBase( m_lastBase ), z ).GetColorOrd() );
    m_complexityMeasure.Add( m_hashGenerator.GetNewestTriplet() );
    m_lastBase = x;
}

inline void CSeqScanner::C_LoopImpl_ColorspNoAmbiguities::Update( char x )
{
    // should be reimplemented since functions are not virtual
    m_complexityMeasure.Del( m_hashCode.GetOldestTriplet() );
    Prepare( x );
}

inline void CSeqScanner::C_LoopImpl_ColorspAmbiguities::Update( char x )
{
    // should be reimplemented since functions are not virtual
    m_complexityMeasure.Del( m_hashGenerator.GetOldestTriplet() );
    Prepare( x );
}

////////////////////////////////////////////////////////////////////////
//
template <class LoopImpl, class Callback>
void CSeqScanner::x_MainLoop( LoopImpl& loop, TMatches& matches, Callback& callback, CProgressIndicator* p, 
                            int from, int toOpen, const char * a, const char * A, int off )
{
    if( m_queryHash == 0 ) return;
    int winLen = m_queryHash->GetWindowLength();
//    cerr << __PRETTY_FUNCTION__ << DISPLAY( winLen ) << endl;
    
    if( from < 0 ) from = 0;
    if( toOpen > A - a ) toOpen = A - a;
    if( toOpen - from < int( winLen ) ) return;

    const char * x = a + from;
    for( const char * w = x + winLen; x < w ; ) loop.Prepare( *x++ );

    if( p ) p->SetCurrentValue( off + from );
    off -= winLen;
    for( const char * w = a + toOpen ; x < w ; ) {
        if( loop.IsOk() ) {
            matches.clear();
            loop.RunCallback( callback );
            int pos = x - a + off;
            ITERATE( TMatches, m, matches ) {
//                cerr << DISPLAY( winLen ) << DISPLAY( m->strand ) << DISPLAY( int(m->offset) ) << endl;
                switch( m->GetStrand() ) {
                case '+': m_filter->Match( *m, a, A, pos - m->GetOffset() ); break;
                case '-': m_filter->Match( *m, a, A, pos + m->GetOffset() + winLen - 1 ); break;
                default: THROW( logic_error, "Invalid strand " << m->GetStrand() );
                }
            }
        }
        loop.Update( *x++ );
        if( p ) p->Increment();
    }
}
    
template<class NoAmbiq, class Ambiq, class Callback>
void CSeqScanner::x_RangemapLoop( const TRangeMap& rm, TMatches& matches,  Callback& cbk, CProgressIndicator*p, const char * a, const char * A, int off )
{
    if( m_queryHash == 0 ) return;
    int winLen = m_queryHash->GetWindowLength();
//    cerr << __PRETTY_FUNCTION__ << DISPLAY( winLen ) << endl;

    Uint4 mask4 = CBitHacks::WordFootprint<Uint4>( 2 * winLen );
    Uint8 mask8 = CBitHacks::WordFootprint<Uint8>( 4 * winLen );
    for( TRangeMap::const_iterator i = rm.begin(); i != rm.end(); ++i ) {
        if( i->second == eType_skip ) {
            if( p ) p->SetCurrentValue( i->first.second );
        } else if( i->second == eType_direct ) {

            NoAmbiq impl( *m_queryHash, m_maxSimplicity );
            x_MainLoop( impl, matches, cbk, p, i->first.first, i->first.second, a, A, off );

        } else { // eType_iterate

            Ambiq impl( *m_queryHash, m_maxSimplicity, m_maxAlternatives, mask4, mask8 );
            x_MainLoop( impl, matches, cbk, p, i->first.first, i->first.second, a, A, off );
        }
    }
}

void CSeqScanner::ScanSequenceBuffer( const char * a, const char * A, unsigned off, unsigned end, CSeqCoding::ECoding coding )
{
    TRangeMap rangeMap;
    CreateRangeMap( rangeMap, a, A );

    TMatches matches;
    CScanCallback callback( matches, *m_queryHash );

    string seqname;
    if( m_seqIds ) {
        seqname = m_seqIds->GetSeqDef( m_ord ).GetBestIdString();
    } else seqname = "sequence";

    CProgressIndicator p( "Processing " + seqname, "bases" );
    p.SetFinalValue( A - a );

    switch( coding ) {
    case CSeqCoding::eCoding_ncbi8na: x_RangemapLoop<C_LoopImpl_Ncbi8naNoAmbiguities,C_LoopImpl_Ncbi8naAmbiguities>( rangeMap, matches, callback, &p, a, A, off ); break;
    case CSeqCoding::eCoding_colorsp: x_RangemapLoop<C_LoopImpl_ColorspNoAmbiguities,C_LoopImpl_ColorspAmbiguities>( rangeMap, matches, callback, &p, a, A, off ); break;
    default: THROW( logic_error, "Subject may be represented exclusively in ncbi8na or colorspace encoding, got " << coding );
    }

	m_filter->PurgeQueues();
    p.SetCurrentValue( A - a );
    p.Summary();
}

void CSeqScanner::ScanSequenceBuffer( const char * a, const char * A, unsigned off, unsigned end )
{
    cerr << "ATTENTION! OLD SEQUENCE SCANNER IS BEING USED! DON'T USE IT FOR PRODUCTION PURPOSES!\n";

    if( m_queryHash == 0 ) return;
    if( m_queryHash->GetOffset() ) THROW( runtime_error, "OLD SEQUENCE SCANNER DOES NOT SUPPORT LONG WINDOW SIZE!" );

    unsigned winLen = m_queryHash->GetWindowLength();

    TRangeMap rangeMap;
    CreateRangeMap( rangeMap, a, A );

    using namespace fourplanes;
    typedef CScanCallback::TMatches TMatches;

    TMatches matches;
    CScanCallback callback( matches, *m_queryHash );
    Uint8 mask = CBitHacks::WordFootprint<Uint8>( 2 * winLen );

    string seqname;
    if( m_seqIds ) {
        seqname = m_seqIds->GetSeqDef( m_ord ).GetBestIdString();
    }
    else seqname = "sequence";

    CProgressIndicator p( "Processing " + seqname, "bases" );
    p.SetFinalValue( A - a );
    
    for( TRangeMap::const_iterator i = rangeMap.begin(); i != rangeMap.end(); ++i ) {
        if( i->second == eType_skip ) {
            p.SetCurrentValue( i->first.second );
        } else if ( i->second == eType_direct ) {
            CHashCode hashCode( winLen );
            CComplexityMeasure complexity;
            const char * x = a + i->first.first;
            for( unsigned pos = 0 ; pos < winLen && x < A; ++pos ) {
                hashCode.AddBaseCode( Ncbi4na2Ncbi2na( *x++ ) );
                complexity.Add( hashCode.GetNewestTriplet() );
            }
            for( int pos = off + i->first.first, end = off + i->first.second - winLen; pos < end; ++pos ) {
                ASSERT( (pos + a) < A );
                
                if( (pos + a + winLen) != x ) THROW( logic_error, " x - a - w = " << ( x - a - winLen ) << " while pos = " << pos << " and off = " << off );
                if( complexity < m_maxSimplicity ) {
                    matches.clear();
                    callback( Uint4( hashCode.GetHashValue() ), 0, 1 );
                    ITERATE( TMatches, m, matches ) {
                        switch( m->GetStrand() ) {
                        case '+': m_filter->Match( *m, a, A, pos - m->GetOffset() );
                            break;
                        case '-': m_filter->Match( *m, a, A, pos + m->GetOffset() + winLen - 1 );
                            break;
                        default: THROW( logic_error, "Invalid strand " << m->GetStrand() );
                        }
                    }
                }
                complexity.Del( hashCode.GetOldestTriplet() );
                hashCode.AddBaseCode( Ncbi4na2Ncbi2na( *x++ ) );
                complexity.Add( hashCode.GetNewestTriplet() );
                p.Increment();
            }
        } else {
            // eType_iterate
            CHashGenerator hashGen( winLen );
            CComplexityMeasure complexity;
            const char * x = a + i->first.first;
            for( unsigned pos = 0 ; pos < winLen && x < A; ++pos ) {
                hashGen.AddBaseMask( *x++ );
                complexity.Add( hashGen.GetNewestTriplet() );
            }
    
            for( int pos = off + i->first.first, end = off + i->first.second; pos < end; ++pos ) {
                ASSERT( (pos + a) < A );
                if( (pos + a + winLen) != x ) THROW( logic_error, " x - a - w = " << ( x - a - winLen ) << " while pos = " << pos << " and off = " << off );
                if( hashGen.GetAlternativesCount() < m_maxAlternatives && complexity < m_maxSimplicity ) {
                    matches.clear();
                    for( CHashIterator h( hashGen, TPlaneMask( mask ), mask ); h; ++h ) {
                        callback( Uint4( *h ), 0, hashGen.GetAlternativesCount() );
                    }
                    ITERATE( TMatches, m, matches ) {
                        switch( m->GetStrand() ) {
                        case '+': m_filter->Match( *m, a, A, pos - m->GetOffset() );
                            break;
                        case '-': m_filter->Match( *m, a, A, pos + m->GetOffset() + winLen - 1 );
                            break;
                        default: THROW( logic_error, "Invalid strand " << m->GetStrand() );
                        }
                    }
                }
                complexity.Del( hashGen.GetOldestTriplet() );
                hashGen.AddBaseMask( *x++ );
                complexity.Add( hashGen.GetNewestTriplet() );
                p.Increment();
            }
        }
    }
    p.Summary();
    
}

