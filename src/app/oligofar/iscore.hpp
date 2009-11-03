#ifndef OLIGOFAR__ISCORING__HPP
#define OLIGOFAR__ISCORING__HPP

#include "defs.hpp"
#include "cscoreparam.hpp"
#include "cseqcoding.hpp"
#include "cbithacks.hpp"

BEGIN_OLIGOFAR_SCOPES

////////////////////////////////////////////////////////////////////////
// General interface for CAligner
class IScore
{
public:
    virtual ~IScore() {}
    
    virtual const CScoreParam * GetScoreParam() const = 0;
    virtual double BestScore( const char * query ) const = 0;
    virtual double Score( const char * query, const char * subj ) const = 0;

    bool   Match( const char * query, const char * subj ) const { return Score( query, subj ) > 0; }
    double Penalty( const char * query, const char * subj ) const 
        { return BestScore( query ) - Score( query, subj ); }
};

END_OLIGOFAR_SCOPES

#endif
