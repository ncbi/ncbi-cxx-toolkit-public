#include <ncbi_pch.hpp>
#include "cguidefile.hpp"
#include "cscoreparam.hpp"
#include "cseqids.hpp"
#include "cquery.hpp"
#include "chit.hpp"

USING_OLIGOFAR_SCOPES;

void CGuideFile::AdjustInput( int& fwd, char dir, int& p1, int which ) const
{
    switch( dir ) {
    case '1': case '+': fwd = 1; break;
    case 0: case '-': fwd = 0; break;
    default: THROW( runtime_error, "Failed to parse this version of guide file: sdir" << (which) << " is neither of (0,1,+,-) [" << m_buff << "]" );
    }
    --p1;
}

CGuideFile::TRange CGuideFile::ComputeRange( bool fwd, int pos, int len ) const
{
    if( fwd ) return TRange( pos, pos + len - 1 );
    else return TRange( pos + len - 1, pos );
}

int CGuideFile::x_CountMismatches( const string& smposx ) const
{
    if( smposx == "-" ) return 0;
    istringstream i( smposx );
    string smpos;
    int mcnt = 0;
    while( getline( i, smpos, ',' ) ) {
        if( NStr::StringToInt( smpos ) > 0 ) 
            if( ++mcnt > m_maxMismatch ) return mcnt;
    }
    return mcnt;
}
    
void CGuideFile::SetMismatchPenalty( const CScoreParam& sc ) 
{ 
    m_mismatchPenalty = (-sc.GetMismatchPenalty())/sc.GetIdentityScore() - 1;
}

bool CGuideFile::NextHit( Uint8 ordinal, CQuery * query )
{
    if( m_buff.length() == 0 ) return false;
    if( m_maxMismatch > 2 ) 
        THROW( logic_error, "Max mismatch in guide file can not be greater then 2" );

    while( m_buff.length() ) {
        istringstream in( m_buff );
        Uint8 qord;
        int resultType, soid, sord, spos1, spos2, sfwd1, sfwd2, mcnt1(0), mcnt2(0);
        char mbase1, mbase2, sdir1, sdir2;
        string smpos1, smpos2;
        string sid, qid;
        in >> resultType >> qid >> sid >> spos1 >> sdir1 >> smpos1 >> mbase1;
        if( in.fail() ) 
            THROW( runtime_error, "Bad format of guide file: line [" << m_buff << "] does not contain minimal set of columns" );

        bool use_ids = false;
        if( sid.length() == 0 )
            THROW( runtime_error, "Bad format of guide file: empty sequence id column in [" << m_buff << "]" );
        if( isdigit( sid[0] ) ) {
            soid = NStr::StringToInt( sid );
            sord = m_seqIds->Register( soid );
        } else {
            use_ids = true;
            sord = m_seqIds->Register( sid );
        }
        ASSERT( sord >= 0 );

        if( !use_ids ) {
            qord = NStr::StringToInt( qid );
            if( qord > ordinal ) break; // preserve the buffer - will try again next call
            if( qord < m_lastQord )
                THROW( runtime_error, "Bad sort order for guide file: it should be exactly the same as for input file" );
            m_lastQord = qord;
        }
        else if( qid == query->GetId() ) 
            qord = ordinal;
        else break; // we consider here that guideFile is ordered same way as inputFile

        if( query->IsPairedRead() && resultType == 0 ) {
            in >> spos2 >> sdir2 >> smpos2 >> mbase2;

            if( in.fail() ) 
                THROW( runtime_error, "Bad format of guide file: line [" << m_buff << "] does not contain columns for read pair" );
        }
        
        if( !getline( m_in, m_buff ) ) m_buff.clear(); // read new buffer
        
        if( resultType != 0 ) continue;
        if( qord < ordinal ) continue;
        try {
            mcnt1 = x_CountMismatches( smpos1 );
            if( mcnt1 > m_maxMismatch ) continue;
        } catch( exception& ) {
            THROW( runtime_error, "Bad format of mismatch list of guide file line [" << m_buff << "]" );
        }

        AdjustInput( sfwd1, sdir1, spos1, 1 );

        if( query->IsPairedRead() ) {

            AdjustInput( sfwd2, sdir2, spos2, 2 );

            try {
                mcnt2 = x_CountMismatches( smpos2 );
                if( mcnt2 > m_maxMismatch ) continue;
            } catch( exception& ) {
                THROW( runtime_error, "Bad format of mismatch list of guide file line [" << m_buff << "]" );
            }

            TRange r1 = ComputeRange( sfwd1 != 0, spos1, query->GetLength(0) );
            TRange r2 = ComputeRange( sfwd2 != 0, spos2, query->GetLength(1) );

            if( ! m_filter->CheckGeometry( r1.first, r1.second, r2.first, r2.second ) ) continue;
        }
        CHit * hit = 0;
        double score1 = 100.0 - mcnt1*m_mismatchPenalty*100/query->GetLength(0);
        if( query->IsPairedRead() ) {
            // Note: for paired reads we have only paired hits
            double score2 = 100.0 - mcnt1*m_mismatchPenalty*100/query->GetLength(1);
            if( sfwd1 ) {
                if( sfwd2 ) {
                    hit = new CHit( query, sord, 
                                    score1, spos1, spos1 + query->GetLength(0) - 1, 0,
                                    score2, spos2, spos2 + query->GetLength(1) - 1, 0, "", "" );
                } else {
                    hit = new CHit( query, sord, 
                                    score1, spos1, spos1 + query->GetLength(0) - 1, 0,
                                    score2, spos2 + query->GetLength(1) - 1, spos2, 0, "", "" );
                }
            } else {
                if( sfwd2 ) {
                    hit = new CHit( query, sord, 
                                    score1, spos1 + query->GetLength(0) - 1, spos1, 0,
                                    score2, spos2, spos2 + query->GetLength(1) - 1, 0, "", "" );
                } else {
                    hit = new CHit( query, sord, 
                                    score1, spos1 + query->GetLength(0) - 1, spos1, 0,
                                    score2, spos2 + query->GetLength(1) - 1, spos2, 0, "", "" );
                }
            }
        } else {
            // Note: for paired reads we have only paired hits
            if( sfwd1 ) {
                hit = new CHit( query, sord, 0, score1, spos1, spos1 + query->GetLength(0) - 1, 0, "" );
            } else {
                hit = new CHit( query, sord, 0, score1, spos1 + query->GetLength(0) - 1, spos1, 0, "" );
            }
        }
        m_filter->PurgeHit( hit );
        return true;
    }
    return false;
}

