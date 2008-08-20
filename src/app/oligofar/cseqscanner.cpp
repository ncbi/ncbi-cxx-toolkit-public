#include <ncbi_pch.hpp>
#include "cseqscanner.hpp"
#include "cprogressindicator.hpp"
#include "cqueryhash.hpp"
#include "cfilter.hpp"
#include "cseqids.hpp"
#include "csnpdb.hpp"
#include "dust.hpp"

USING_OLIGOFAR_SCOPES;

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

    if( m_queryHash->GetHashedQueriesCount() != 0 ) ScanSequenceBuffer( a, A, off, end );

    if( m_inputChunk ) {
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
	Uint8 altCount = 1;
	const char * y = a;
	for( const char * Y = a + m_windowLength; y < Y; ++y ) {
		altCount *= Ncbi4naAlternativeCount( *y );
	}
	int lastPos = -1;
	ERangeType lastType = eType_skip;
	// TODO: somewhere here a check on range length should be inserted
	for( const char * z = a; y < A;  ) {
		ERangeType type = ( altCount == 1 ) ? eType_direct : ( altCount < m_maxAlternatives ) ? eType_iterate : eType_skip ;
		if( lastType != type ) {
			if( lastPos != -1 ) {
				if( lastType != eType_skip && rangeMap.back().second != eType_skip && 
					rangeMap.back().first.second - rangeMap.back().first.first + z - a - lastPos < 10000 ) {
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
		altCount /= Ncbi4naAlternativeCount( *z++ );
		altCount *= Ncbi4naAlternativeCount( *y++ );
	}
	rangeMap.push_back( make_pair( TRange( lastPos, A - a ), lastType ) );
}

namespace 
{
    typedef array_set <CQueryHash::SHashAtom> TMatches;
    class CScanCallback
    {
    public:
        CScanCallback( TMatches& matches, CQueryHash& queryHash ) :
            m_matches( matches ), m_queryHash( queryHash ) {}
        void operator () ( Uint4 hash, Uint4 mism, Uint8 alt ) {
            m_mism = mism;
            m_queryHash.ForEach( hash, *this );
        }
        void operator () ( const CQueryHash::SHashAtom& a ) { m_matches.insert( a ); }
    protected:
        Uint4 m_mism;
        TMatches& m_matches;
        CQueryHash& m_queryHash;
    };
    
    
    void CSeqScanner::ScanSequenceBuffer( const char * a, const char * A, unsigned off, unsigned end )
    {
		TRangeMap rangeMap;
		CreateRangeMap( rangeMap, a, A );

        using namespace fourplanes;

        TMatches matches;
        CScanCallback callback( matches, *m_queryHash );
        unsigned mask = CBitHacks::WordFootprint<unsigned>( 2 * m_windowLength );

        string seqname;
        if( m_seqIds ) {
            seqname = m_seqIds->GetSeqDef( m_ord ).GetBestIdString();
        } else seqname = "sequence";

        CProgressIndicator p( "Processing " + seqname, "bases" );
        p.SetFinalValue( A - a );
		for( TRangeMap::const_iterator i = rangeMap.begin(); i != rangeMap.end(); ++i ) {
			if( i->second == eType_skip ) {
				p.SetCurrentValue( i->first.second );
			} else if( i->second == eType_direct ) {
				CHashCode hashCode( m_windowLength );
				CComplexityMeasure complexity;
				const char * x = a + i->first.first;
        		for( unsigned pos = 0 ; pos < m_windowLength && x < A; ++pos ) {
		            hashCode.AddBaseCode( Ncbi4na2Ncbi2na( *x++ ) );
        		    complexity.Add( hashCode.GetNewestTriplet() );
        		}
                for( int pos = off + i->first.first, end = off + i->first.second; pos < end; ++pos ) {
					ASSERT( (pos + a) < A );
					if( (pos + a + m_windowLength) != x ) THROW( logic_error, " x - a - w = " << ( x - a - m_windowLength ) << " while pos = " << pos << " and off = " << off );
                    if( complexity < m_maxSimplicity ) {
                        matches.clear();
                        callback( hashCode.GetHashValue(), 0, 1 );
                        ITERATE( TMatches, m, matches ) {
                            switch( m->strand ) {
                            case '+': m_filter->Match( *m, a, A, pos - m->offset ); break;
                            case '-': m_filter->Match( *m, a, A, pos + m->offset + m_windowLength - 1 ); break;
                            default: THROW( logic_error, "Invalid strand " << m->strand );
                            }
                        }
                    }
                    complexity.Del( hashCode.GetOldestTriplet() );
		            hashCode.AddBaseCode( Ncbi4na2Ncbi2na( *x++ ) );
                    complexity.Add( hashCode.GetNewestTriplet() );
                    p.Increment();
                }
			} else { // eType_iterate
                CHashGenerator hashGen( m_windowLength );
                CComplexityMeasure complexity;
				const char * x = a + i->first.first;
        		for( unsigned pos = 0 ; pos < m_windowLength && x < A; ++pos ) {
		            hashGen.AddBaseMask( *x++ );
        		    complexity.Add( hashGen.GetNewestTriplet() );
        		}
    
                for( int pos = off + i->first.first, end = off + i->first.second; pos < end; ++pos ) {
					ASSERT( (pos + a) < A );
					if( (pos + a + m_windowLength) != x ) THROW( logic_error, " x - a - w = " << ( x - a - m_windowLength ) << " while pos = " << pos << " and off = " << off );
                    if( hashGen.GetAlternativesCount() < m_maxAlternatives && complexity < m_maxSimplicity ) {
                        matches.clear();
                        for( CHashIterator h( hashGen, mask, mask ); h; ++h ) {
                            callback( *h, 0, hashGen.GetAlternativesCount() );
                        }
                        ITERATE( TMatches, m, matches ) {
                            switch( m->strand ) {
                            case '+': m_filter->Match( *m, a, A, pos - m->offset ); break;
                            case '-': m_filter->Match( *m, a, A, pos + m->offset + m_windowLength - 1 ); break;
                            default: THROW( logic_error, "Invalid strand " << m->strand );
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
}

