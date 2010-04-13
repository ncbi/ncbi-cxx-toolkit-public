#ifndef OLIGOFAR_CSEEQSCANNER__HPP
#define OLIGOFAR_CSEEQSCANNER__HPP

#include "cseqbuffer.hpp"
#include "cqueryhash.hpp"
#include "cseqvecprocessor.hpp"
#include "cprogressindicator.hpp"
#include "dust.hpp"
#include "fourplanes.hpp"

BEGIN_OLIGOFAR_SCOPES

class CSnpDb;
class CSeqIds;
class CFilter;
class CFeatMap;
class CProgressIndicator;
//class CComplexityMeasure;
class CSeqScanner : public CSeqVecProcessor::ICallback
{
public:
    typedef vector<CQuery*> TInputChunk;
    typedef array_set <CHashAtom> TMatches;

    CSeqScanner() : 
        m_maxAmbiguities( 4 ), 
        m_seqIds(0), m_snpDb(0), m_filter(0), m_queryHash(0), m_featMap(0), 
        m_inputChunk(0), m_minBlockLength(1000),
        m_ord(-1),
        m_progress( new CProgressIndicator( "Processing reference sequences", "seq" ) )/*, m_bisulfiteCuration(false)*/ {}
    ~CSeqScanner() {
        m_progress->Summary();
    }
    
    virtual void SequenceBegin( const TSeqIds& seqIds, int oid );
    virtual void SequenceBuffer( CSeqBuffer* iupacna );
    virtual void SequenceEnd() { m_progress->Increment(); }

    void SetMaxAmbiguities( int a ) { m_maxAmbiguities = a; }

//    void SetSodiumBisulfateCuration( bool on ) { m_bisulfiteCuration = on; }

    void SetFeatMap( CFeatMap * fmap ) { m_featMap = fmap; }
    void SetSnpDb( CSnpDb* snpdb ) { m_snpDb = snpdb; }
    void SetFilter( CFilter * filter ) { m_filter = filter; }
    void SetQueryHash( CQueryHash* queryhash ) { m_queryHash = queryhash; }
    void SetSeqIds( CSeqIds * bldr ) { m_seqIds = bldr; }
    void SetInputChunk( const TInputChunk& ic ) { m_inputChunk = &ic; }
    void SetMinBlockLength( int l ) { m_minBlockLength = l; }
            
	enum ERangeType { eType_skip, eType_direct, eType_iterate };
	typedef pair<int,int> TRange;
	typedef list<pair<TRange,ERangeType> > TRangeMap; 

protected:
    void ScanSequenceBuffer( const char * a, const char * A, unsigned off, unsigned end, CSeqCoding::ECoding );
    void AssignTargetSequences( );
	void CreateRangeMap( TRangeMap& rangeMap, const char * a, const char * A, int off );
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
        C_ScanImpl_Base( int winSize, int stride ) : //, double maxSimpl ) : 
            m_windowSize( winSize ), m_strideSize( stride ) {} //, m_maxSimplicity( maxSimpl ) {}
//        double GetComplexity() const { return m_complexityMeasure; }
        bool IsOk() const { return true; } //return m_complexityMeasure <= m_maxSimplicity; }
        // CHashParam  API
        int GetWindowSize() const { return m_windowSize; }
        int GetStrideSize() const { return m_strideSize; }
        
    protected:
        int m_windowSize;
        int m_strideSize;
//        double m_maxSimplicity;
//        CComplexityMeasure m_complexityMeasure;
    };

    class C_LoopImpl_Ncbi8naNoAmbiguities : public C_ScanImpl_Base
    {
    public:
        C_LoopImpl_Ncbi8naNoAmbiguities( int ws, int ss ) : //, double maxSimpl ) : 
            C_ScanImpl_Base( ws, ss /*, maxSimpl*/ ), m_hashCode( GetWindowSize() ) {}
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
        C_LoopImpl_Ncbi8naAmbiguities( int ws, int ss,
                                       /*double maxSimpl,*/
                                       int maxAmb, Uint8 mask8, const UintH& maskH ) : 
            C_ScanImpl_Base( ws, ss /*, maxSimpl*/ ), 
            m_hashGenerator( GetWindowSize() ), 
            m_maxAmbiguities( maxAmb ), m_mask8( mask8 ), m_maskH( maskH ) {}
        template<class Callback>
        void RunCallback( Callback& );
        void Prepare( char a );
        void Update( char a );
        bool IsOk() const { return C_ScanImpl_Base::IsOk() && m_hashGenerator.GetAmbiguityCount() <= m_maxAmbiguities; }
        const char * GetName() const { return "C_LoopImpl_Ncbi8naAmbiguities"; }
    protected:
        fourplanes::CHashGenerator m_hashGenerator;
        int m_maxAmbiguities;
        Uint8 m_mask8;
        UintH m_maskH;
    };

    class C_LoopImpl_ColorspNoAmbiguities : public C_LoopImpl_Ncbi8naNoAmbiguities
    {
    public:
        C_LoopImpl_ColorspNoAmbiguities( int ws, int ss /*, double maxSimpl */) : 
            C_LoopImpl_Ncbi8naNoAmbiguities( ws, ss /*, maxSimpl*/ ), m_lastBase(1) {}
        void Prepare( char a );
        void Update( char a );
        const char * GetName() const { return "C_LoopImpl_ColorspNoAmbiguities"; }
    protected:
        char m_lastBase;
    };

    class C_LoopImpl_ColorspAmbiguities : public C_LoopImpl_Ncbi8naAmbiguities
    {
    public:
        C_LoopImpl_ColorspAmbiguities( int ws, int ss,
                                       /*double maxSimpl, */
                                       int maxAmb, Uint8 mask8, const UintH& maskH ) : 
            C_LoopImpl_Ncbi8naAmbiguities( ws, ss/*, maxSimpl*/, maxAmb, mask8, maskH ),m_lastBase(1) {}
        void Prepare( char a );
        void Update( char a );
        const char * GetName() const { return "C_LoopImpl_ColorspAmbiguities"; }
    protected:
        char m_lastBase;
    };

protected:
    // void PerformSodiumBisulfiteCuration( char * dest, const char * src, size_t length, CSeqCoding::EStrand strand );

protected:
//    unsigned m_windowLength;
    int m_maxAmbiguities;
    CSeqIds * m_seqIds;
    CSnpDb * m_snpDb;
    CFilter * m_filter;
    CQueryHash * m_queryHash;
    CFeatMap * m_featMap;
    const TInputChunk * m_inputChunk;
    int  m_minBlockLength;
	int  m_ord;
    auto_ptr<CProgressIndicator> m_progress;
    //bool m_bisulfiteCuration;
    //int  m_strands;
};

END_OLIGOFAR_SCOPES

#endif
