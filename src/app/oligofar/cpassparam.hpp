#ifndef OLIGOFAR__CPASSPARAM__HPP
#define OLIGOFAR__CPASSPARAM__HPP

#include "chashparam.hpp"
#include "calignparam.hpp"

BEGIN_OLIGOFAR_SCOPES

#define ITEM__(type,name) \
public: \
    const type& Get ## name () const { return m_ ## name; } \
    type& Set ## name () { return m_ ## name; } \
protected: \
    type m_ ## name

class CPassParam
{
public:
    typedef CPassParam TSelf;

    CPassParam() : 
        m_MaxSubjAmb( 4 ),
        m_PhrapCutoff( 4 ),
        m_MinPair( 100 ),
        m_MaxPair( 200 ),
        m_PairMargin( 20 ) {}

    ITEM__( CHashParam,     HashParam );
    ITEM__( CAlignParam,    AlignParam );
    ITEM__( int,            MaxSubjAmb );
    ITEM__( int,            PhrapCutoff );
    ITEM__( int,            MinPair );
    ITEM__( int,            MaxPair );
    ITEM__( int,            PairMargin );
public:
    int GetMinDist() const { return GetMinPair() - GetPairMargin(); }
    int GetMaxDist() const { return GetMaxPair() + GetPairMargin(); }
protected:

};

#undef ITEM__

END_OLIGOFAR_SCOPES

#endif
