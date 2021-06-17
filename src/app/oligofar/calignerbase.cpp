#include <ncbi_pch.hpp>
#include "calignerbase.hpp"

USING_OLIGOFAR_SCOPES;

bool CAlignerBase::gsx_printDebug = false;

CScoreTbl CAlignerBase::s_defaultScoreTbl( 1, -1, -3, -1.5 );

CAlignerBase::CAlignerBase( CScoreTbl& scoretbl ) :
    m_scoreTbl( 0 ),
    m_qbegin( 0 ),
    m_sbegin( 0 ),
    m_qend( 0 ),
    m_send( 0 ),
    m_identities( 0 ),
    m_mismatches( 0 ),
    m_insertions( 0 ),
    m_deletions( 0 ),
    m_score( 0 ),
    m_bestScore( 0 )
{}

CAlignerBase& CAlignerBase::ResetScores() 
{
    m_identities = m_mismatches = m_insertions = m_deletions = 0;
    m_score = 0;
    m_bestScore = 0;
    m_transcript.resize(0);
    m_queryString.resize(0);
    m_subjectString.resize(0);
    m_alignmentString.resize(0);
    return *this;
}

