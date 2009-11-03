#ifndef OLIGOFAR__IALIGNER__HPP
#define OLIGOFAR__IALIGNER__HPP

#include "cseqcoding.hpp"
#include "ctranscript.hpp"

BEGIN_OLIGOFAR_SCOPES

class CScoringFactory;
class IAligner
{
public:
    typedef TTrSequence TTranscript;
    virtual ~IAligner() {}


    // this should be used for following purposes:
    // * retrieve scoring parameters 
    // * retrieve scoring function according to:
    //   - query strand
    //   - bisulfite treatment mode according to hash word type (AG|TC)
    //   - all this for the given coding
    virtual CScoringFactory * GetScoringFactory() const = 0;
    
    virtual void SetWordDistance( int k ) = 0;

    virtual void SetQueryCoding( CSeqCoding::ECoding ) = 0;
    virtual void SetSubjectCoding( CSeqCoding::ECoding ) = 0;
    virtual void SetQueryStrand( CSeqCoding::EStrand ) = 0;
    virtual void SetSubjectStrand( CSeqCoding::EStrand ) = 0;
    virtual void SetQuery( const char * begin, int len ) = 0;
    virtual void SetSubject( const char * begin, int len ) = 0;
    virtual void SetPenaltyLimit( double limit ) = 0;
    virtual void SetSubjectAnchor( int first, int last ) = 0; // last is included, should clear guide transcript 
    virtual void SetQueryAnchor( int first, int last ) = 0; // last is included, should clear guide transcript
    virtual void SetSubjectGuideTranscript( const TTranscript& tr ) = 0;

    virtual bool Align() = 0;
    
    virtual double GetPenalty() const = 0;
    virtual int GetSubjectFrom() const = 0;
    virtual int GetSubjectTo() const = 0;
    virtual int GetQueryFrom() const = 0;
    virtual int GetQueryTo() const = 0;
    virtual const TTranscript& GetSubjectTranscript() const = 0;
};

END_OLIGOFAR_SCOPES

#endif
