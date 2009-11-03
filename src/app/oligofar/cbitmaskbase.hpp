#ifndef OLIGOFAR__CBITMASKBASE__HPP
#define OLIGOFAR__CBITMASKBASE__HPP

#include "defs.hpp"

BEGIN_OLIGOFAR_SCOPES

class CBitmaskBase
{
public:
    CBitmaskBase( int maxamb, int wsize, int wstep ) 
        : m_size(0), m_data(0), m_maxAmb( maxamb ), 
          m_wSize( wsize ), m_wStep( wstep ) {}

    const char * Signature() const { return "oligoFAR.hash.word.bit.mask:0.0.0\0\0\0\0"; }
    Uint4 SignatureSize() const { return (strlen( Signature() ) + 1 + 3)&~3; }

    Uint4 GetMaxAmb() const { return m_maxAmb; }
    Uint4 GetWordSize() const { return m_wSize; }
    Uint4 GetWordStep() const { return m_wStep; }

protected:
    Uint8   m_size;
    Uint4 * m_data;
    Uint4   m_maxAmb;
    Uint4   m_wSize;
    Uint4   m_wStep;
};

END_OLIGOFAR_SCOPES

#endif
