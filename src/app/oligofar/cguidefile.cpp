#include <ncbi_pch.hpp>
#include "cguidefile.hpp"
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

bool CGuideFile::NextHit( Uint8 ordinal, CQuery * query )
{
    if( m_buff.length() == 0 ) return false;

    while( m_buff.length() ) {
        istringstream in( m_buff );
		Uint8 qord;
        int resultType, soid, sord, spos1, mpos1, spos2, mpos2, sfwd1, sfwd2;
        char mbase1, mbase2, sdir1, sdir2;
		string sid, qid;
        in >> resultType >> qid >> sid >> spos1 >> sdir1 >> mpos1 >> mbase1;
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
		}
		else if( qid == query->GetId() ) 
			qord = ordinal;
		else break; // we consider here that guideFile is ordered same way as inputFile

        if( query->IsPairedRead() && resultType == 0 ) {
            in >> spos2 >> sdir2 >> mpos2 >> mbase2;

            if( in.fail() ) 
                THROW( runtime_error, "Bad format of guide file: line [" << m_buff << "] does not contain columns for read pair" );
        }
        
        if( !getline( m_in, m_buff ) ) m_buff.clear(); // read new buffer
        
        if( resultType != 0 ) continue;
        if( qord < ordinal ) continue;
        if( mpos1 != 0 ) continue;

		AdjustInput( sfwd1, sdir1, spos1, 1 );

        if( query->IsPairedRead() ) {

			AdjustInput( sfwd2, sdir2, spos2, 2 );

            if( mpos2 != 0 ) continue;
			// NB: rest of the block should be changed if one wants to make mutual orientation to be configurable
            if( ( sfwd2 ^ sfwd1 ) != 1 ) continue; 
            if( sfwd1 ) {
                if( spos1 < spos2 + (int)query->GetLength(1) - GetMaxDist() ) continue;
                if( spos1 > spos2 + (int)query->GetLength(1) - GetMinDist() ) continue;
            } else {
                if( spos2 < spos1 + (int)query->GetLength(0) - GetMaxDist() ) continue;
                if( spos2 > spos1 + (int)query->GetLength(0) - GetMinDist() ) continue;
            }
        }
        CHit * hit = 0;
        if( sfwd1 ) {
            if( query->IsPairedRead() ) {
                hit = new CHit( query, sord, 
                                100, spos1, spos1 + query->GetLength(0) - 1,
                                100, spos2 + query->GetLength(1) - 1, spos2 );
            } else {
                hit = new CHit( query, sord, 0, 100, spos1, spos1 + query->GetLength(0) - 1 );
            }
        } else {
            if( query->IsPairedRead() ) {
                hit = new CHit( query, sord, 
                                100, spos1 + query->GetLength(0) - 1, spos1, 
                                100, spos2, spos2 + query->GetLength(1) - 1 );
            } else {
                hit = new CHit( query, sord, 0, 100, spos1 + query->GetLength(0) - 1, spos1 );
            }
        }
		m_filter->PurgeHit( hit );
        return true;
    }
    return false;
}

