#ifndef OLIGOFAR_CSEEQSCANNER__HPP
#define OLIGOFAR_CSEEQSCANNER__HPP

#include "cseqbuffer.hpp"
#include "cqueryhash.hpp"
#include "cseqvecprocessor.hpp"
#include "dust.hpp"
#include "fourplanes.hpp"

BEGIN_OLIGOFAR_SCOPES

class CSnpDb;
class CSeqIds;
class CFilter;
class CProgressIndicator;
class CComplexityMeasure;
class CSeqScanner : public CSeqVecProcessor::ICallback
{
public:
    typedef vector<CQuery*> TInputChunk;
    typedef array_set <CQueryHash::SHashAtom> TMatches;

    CSeqScanner( unsigned windowLength ) : 
        m_windowLength( windowLength ), m_maxAlternatives( 1024 ), m_maxSimplicity( 2.0 ), 
        m_seqIds(0), m_snpDb(0), m_filter(0), m_queryHash(0), m_inputChunk(0), m_minBlockLength(1000),
        m_ord(-1), m_run_old_scanning_code(false) {}
    
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
    void SetMinBlockLength( int l ) { m_minBlockLength = l; }
            
    void SetRunOldScanningCode( bool to ) { m_run_old_scanning_code = to; }

	enum ERangeType { eType_skip, eType_direct, eType_iterate };
	typedef pair<int,int> TRange;
	typedef list<pair<TRange,ERangeType> > TRangeMap; 

protected:
    void ScanSequenceBuffer( const char * a, const char * A, unsigned off, unsigned end ); // OLD!!!!! For performance tests only!!!
    void ScanSequenceBuffer( const char * a, const char * A, unsigned off, unsigned end, CSeqCoding::ECoding );
    void AssignTargetSequences( );
	void CreateRangeMap( TRangeMap& rangeMap, const char * a, const char * A );
	bool IsThisSequence( int oid ) const;

    template<class Details, class Callback>
    void x_MainLoop( Details& details, TMatches& matches, Callback& cbk, CProgressIndicator* , 
            int from, int toOpen, const char * a, const char * A, int off );
    template<class NoAmbiq, class Ambiq, class Callback>
    void x_RangemapLoop( const TRangeMap& rm, TMatches& matches,  Callback& cbk, CProgressIndicator*,
            const char * a, const char * A, int off );
    //void x_ProcessMatches( const TMatches& m, const char * a, const char * A, int pos );

    class C_ScanImpl_Base 
    {
    public:
        C_ScanImpl_Base( int windowLength, double maxSimpl ) : 
            m_windowLength( windowLength ), m_maxSimplicity( maxSimpl ) {}
        double GetComplexity() const { return m_complexityMeasure; }
        bool IsOk() const { return m_complexityMeasure <= m_maxSimplicity; }
    protected:
        int m_windowLength;
        double m_maxSimplicity;
        CComplexityMeasure m_complexityMeasure;
    };

    class C_LoopImpl_Ncbi8naNoAmbiguities : public C_ScanImpl_Base
    {
    public:
        C_LoopImpl_Ncbi8naNoAmbiguities( int windowLength, double maxSimpl ) : 
            C_ScanImpl_Base( windowLength, maxSimpl ), m_hashCode( windowLength ) {}
        template<class Callback>
        void RunCallback( Callback& );
        void Prepare( char a );
        void Update( char a );
        const char * GetName() const { return "C_LoopImpl_Ncbi8naNoAmbiguities"; }
    protected:
        fourplanes::CHashCode m_hashCode;
    };

    class C_LoopImpl_Ncbi8naAmbiguities : public C_ScanImpl_Base
    {
    public:
        C_LoopImpl_Ncbi8naAmbiguities( int windowLength, double maxSimpl, Uint8 maxAlt, unsigned mask ) : 
            C_ScanImpl_Base( windowLength, maxSimpl ), m_hashGenerator( windowLength ), m_maxAlternatives( maxAlt ), m_mask( mask ) {}
        template<class Callback>
        void RunCallback( Callback& );
        void Prepare( char a );
        void Update( char a );
        bool IsOk() const { return C_ScanImpl_Base::IsOk() && m_hashGenerator.GetAlternativesCount() <= m_maxAlternatives; }
        const char * GetName() const { return "C_LoopImpl_Ncbi8naAmbiguities"; }
    protected:
        fourplanes::CHashGenerator m_hashGenerator;
        Uint8 m_maxAlternatives;
        unsigned m_mask;
    };

    class C_LoopImpl_ColorspNoAmbiguities : public C_LoopImpl_Ncbi8naNoAmbiguities
    {
    public:
        C_LoopImpl_ColorspNoAmbiguities( int windowLength, double maxSimpl ) : 
            C_LoopImpl_Ncbi8naNoAmbiguities( windowLength, maxSimpl ) {}
        void Prepare( char a );
        void Update( char a );
        const char * GetName() const { return "C_LoopImpl_ColorspNoAmbiguities"; }
    };

    class C_LoopImpl_ColorspAmbiguities : public C_LoopImpl_Ncbi8naAmbiguities
    {
    public:
        C_LoopImpl_ColorspAmbiguities( int windowLength, double maxSimpl, Uint8 maxAlt, unsigned mask ) : 
            C_LoopImpl_Ncbi8naAmbiguities( windowLength, maxSimpl, maxAlt, mask ) {}
        void Prepare( char a );
        void Update( char a );
        const char * GetName() const { return "C_LoopImpl_ColorspAmbiguities"; }
    };

protected:
    unsigned m_windowLength;
    Uint8 m_maxAlternatives;
    double m_maxSimplicity;
    CSeqIds * m_seqIds;
    CSnpDb * m_snpDb;
    CFilter * m_filter;
    CQueryHash * m_queryHash;
    const TInputChunk * m_inputChunk;
    int m_minBlockLength;
	int m_ord;
    bool m_run_old_scanning_code;
};

END_OLIGOFAR_SCOPES

#endif
