#ifndef OLIGOFAR_IALIGNMENTMAP__HPP
#define OLIGOFAR_IALIGNMENTMAP__HPP

#include "defs.hpp"
#include "cseqcoding.hpp"
#include "ctranscript.hpp"
#include <util/range.hpp>

BEGIN_OLIGOFAR_SCOPES

class INucSeq
{
public:
    virtual ~INucSeq() {}
    virtual const char * GetId() const = 0;
    virtual const char * GetSequenceData() const = 0;
    virtual int GetSequenceLength() const = 0;
    virtual CSeqCoding::ECoding GetSequenceCoding() const = 0;
};

class IShortRead : public INucSeq
{
public:
    enum EPairType {
        eRead_single = 0,
        eRead_first = 1,
        eRead_second = 3
    };
    virtual EPairType GetPairType() const = 0;
    virtual const IShortRead * GetMate() const = 0;
    const IShortRead * GetFirstRead() const {
        switch( GetPairType() ) {
            case eRead_single: 
            case eRead_first: return this;
            case eRead_second: return GetMate();
        }
    }
};

class IMappedAlignment
{
public:
    typedef CRange<int> TRange;
    virtual ~IMappedAlignment() {}
    virtual const char * GetId() const = 0;
    virtual const IShortRead * GetQuery() const = 0;
    virtual const INucSeq * GetSubject() const = 0;
    virtual TRange GetSubjectBounding() const = 0;
    virtual TRange GetQueryBounding() const = 0;
    virtual const TTrSequence GetCigar() const = 0;
    virtual bool IsReverseComplement() const = 0;
    virtual const IMappedAlignment * GetMate() const = 0;
};

class IAlignmentMap
{
public:
    class ICallback
    {
    public:
        virtual ~ICallback() {}
        virtual void OnAlignment( const IMappedAlignment * ) = 0;
        virtual void OnQuery( const IShortRead * ) = 0;
        virtual void OnSubject( const INucSeq * ) = 0;
    };
    enum EConstants { kSequenceBegin = 0, kSequenceEnd = ~Uint4(0) - 1 }; // -1 since it is closed region, and lenght should fit in int

    virtual ~IAlignmentMap() {}
    virtual void GetAlignmentsForQueryId( ICallback *, const char * id, int mates = 3 ) = 0;
    virtual void GetAlignmentsForSubjectId( ICallback *, const char * id, int strands = 3, int from = kSequenceBegin, int to = kSequenceEnd ) = 0;
    virtual void GetAllAlignments( ICallback * ) = 0;
    virtual void GetAllQueries( ICallback * ) = 0;
    virtual void GetAllSubjects( ICallback * ) = 0;

    virtual void AddAlignment( IMappedAlignment * ma ) = 0;
    virtual void AddSubject( INucSeq * ma ) = 0;
    virtual void AddQuery( IShortRead * ma ) = 0;
};


END_OLIGOFAR_SCOPES

#endif
