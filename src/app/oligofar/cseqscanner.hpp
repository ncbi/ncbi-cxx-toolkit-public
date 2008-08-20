#ifndef OLIGOFAR_CSEEQSCANNER__HPP
#define OLIGOFAR_CSEEQSCANNER__HPP

#include "cseqbuffer.hpp"
#include "cqueryhash.hpp"
#include "cseqvecprocessor.hpp"

BEGIN_OLIGOFAR_SCOPES

class CSnpDb;
class CSeqIds;
class CFilter;
class CSeqScanner : public CSeqVecProcessor::ICallback
{
public:
    typedef vector<CQuery*> TInputChunk;

    CSeqScanner( unsigned windowLength ) : 
        m_windowLength( windowLength ), m_maxAlternatives( 1024 ), m_maxSimplicity( 2.0 ), 
        m_seqIds(0), m_snpDb(0), m_filter(0), m_queryHash(0), m_inputChunk(0), m_ord(-1) {}
    
    virtual void SequenceBegin( const TSeqIds& seqIds, int oid );
    virtual void SequenceBuffer( CSeqBuffer* iupacna );
    virtual void SequenceEnd() {}

    void SetMaxSimplicity( double s ) { m_maxSimplicity = s; }
    void SetMaxAlternatives( Uint8 a ) { m_maxAlternatives = a; }

    void SetSnpDb( CSnpDb* snpdb ) { m_snpDb = snpdb; }
    void SetFilter( CFilter * filter ) { m_filter = filter; }
    void SetQueryHash( CQueryHash* queryhash ) { m_queryHash = queryhash; }
    void SetSeqIds( CSeqIds * bldr ) { m_seqIds = bldr; }
    void SetInputChunk( const TInputChunk& ic ) { m_inputChunk = &ic; }
            
	enum ERangeType { eType_skip, eType_direct, eType_iterate };
	typedef pair<int,int> TRange;
	typedef list<pair<TRange,ERangeType> > TRangeMap; 

protected:
    void ScanSequenceBuffer( const char * a, const char * A, unsigned off, unsigned end );
    void AssignTargetSequences( );
	void CreateRangeMap( TRangeMap& rangeMap, const char * a, const char * A );
	bool IsThisSequence( int oid ) const;

protected:
    unsigned m_windowLength;
    Uint8 m_maxAlternatives;
    double m_maxSimplicity;
    CSeqIds * m_seqIds;
    CSnpDb * m_snpDb;
    CFilter * m_filter;
    CQueryHash * m_queryHash;
    const TInputChunk * m_inputChunk;
	int m_ord;
};

END_OLIGOFAR_SCOPES

#endif
