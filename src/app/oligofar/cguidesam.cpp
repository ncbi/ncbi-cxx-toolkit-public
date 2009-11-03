#include <ncbi_pch.hpp>
#include "cguidesam.hpp"
#include "cscoreparam.hpp"
#include "string-util.hpp"
#include "cseqids.hpp"
#include "cquery.hpp"
#include "chit.hpp"

USING_OLIGOFAR_SCOPES;

void CGuideSamFile::x_ParseHeader()
{
    while( getline( m_in, m_buff ) ) {
        ++m_linenumber;
        if( m_buff.length() || m_buff[0] == '\n' || m_buff[0] == '\r' ) {
            if( m_buff[0] == '@' ) {
                if( m_buff.substr( 1,2 ) == "HD" ) {
                    const char * c = m_buff.c_str() + 3;
                    while( *c ) {
                        while( *c && isspace(*c) ) ++c;
                        if( strcmp( c, "VN:" ) == 0 ) {
                            c += 3;
                            m_samVersion = "";
                            while( *c && isspace( *c ) ) ++c;
                            while( *c && !isspace( *c ) ) m_samVersion += *c++;
                        } else if( strcmp( c, "SO:" ) == 0 ) {
                            c += 3;
                            m_sortOrder = "";
                            while( *c && isspace( *c ) ) ++c;
                            while( *c && !isspace( *c ) ) m_sortOrder += *c++;
                        } else if( strcmp( c, "GO:" ) == 0 ) {
                            c += 3;
                            m_groupOrder = "";
                            while( *c && isspace( *c ) ) ++c;
                            while( *c && !isspace( *c ) ) m_groupOrder += *c++;
                        } else while( *c && !isspace( *c ) ) ++c;
                    }
                }
            }
            else break;
        }
    }
    if( m_linenumber ) {
        if( m_sortOrder != "" || m_groupOrder != "" ) {
            if( m_sortOrder != "query" && m_groupOrder != "query" )
                THROW( runtime_error, "SAM guide file should be either sorted or grouped by query" );
        } else {
            cerr 
                << "Warning: neight sort nor group order is specified for the guide file;\n"
                << "         implying group order by query, but most of the file may be ignored if\n"
                << "         it's order differs from read file.\n";
        }
    }
}

CGuideSamFile::C_Key::C_Key( Uint8 seq1, int pos1, Uint8 seq2, int pos2, Uint1 flags, int lineno ) : m_flags( flags ), m_swap( false ) 
{ 
    if( flags & fPairedQuery ) {
        bool mate = false;
        if( flags & fPairedQuery ) 
            THROW( runtime_error, "SAM format error: SAM flags indicate that query is not paired, but hit is, line " << lineno );
        switch( int k = flags & (fSeqIsFirst|fSeqIsSecond) ) {
        default: THROW( logic_error, __FILE__ << ":" << __LINE__ );
        case 0: case fSeqIsSecond|fSeqIsFirst:
            THROW( runtime_error, "SAM format error: sequence can not be " 
                   << (k? "both first and second":"neither first nor second") 
                   << " in line " << lineno );
        case fSeqIsFirst: 
            if( flags & fSeqUnmapped ) { seq1 = NullSeq(); pos1 = 0; }
            if( flags & fMateUnmapped ) { seq2 = NullSeq(); pos2 = 0; }
            break;
        case fSeqIsSecond: 
            m_flags &= ~(fSeqIsSecond|fSeqIsFirst|fSeqReverse|fMateReverse|fSeqUnmapped|fMateUnmapped);
            m_flags |= fSeqIsFirst;
            if( flags & fSeqReverse ) m_flags |= fMateReverse;
            if( flags & fMateReverse ) m_flags |= fSeqReverse;
            if( flags & fSeqUnmapped ) { m_flags |= fMateUnmapped; seq1 = NullSeq(); pos1 = 0; }
            if( flags & fMateUnmapped ) { m_flags |= fSeqUnmapped; seq2 = NullSeq(); pos2 = 0; }
            swap( pos1, pos2 ); 
            swap( seq1, seq2 ); 
            m_swap = true;
            break;
        }
        m_seq[mate]  = seq1; m_pos[mate]  = pos1;
        m_seq[!mate] = seq2; m_pos[!mate] = pos2;
    } else {
        m_seq[0] = seq1;
        m_pos[0] = pos1;
        m_seq[1] = NullSeq();
        m_pos[1] = 0;
        ASSERT( (flags & fSeqIsSecond) == 0 );
        m_flags |= fMateUnmapped;
    }
}

bool CGuideSamFile::NextHit( Uint8 ordinal, CQuery * query )
{
    // Note: code may be modified to create query directly from guide file if the file contains sequence data
    if( m_buff.length() == 0 ) return false;
    THitMap hitMap;
    while( m_buff.length() ) {
        istringstream in( m_buff );

        vector<string> fields;
        Split( m_buff, "\t", back_inserter( fields ), false ); // TODO: nothing is said about what to do with two consecutive tabs...
        if( fields.size() < 6 ) 
            THROW( runtime_error, "SAM is supposed to have at least 6 columns, got " << 
                    fields.size() << " in [" << m_buff << "] in line " << m_linenumber );
        
        if( !getline( m_in, m_buff ) ) m_buff.clear(); // fetch next line
        ++m_linenumber;
        
//        cerr << DISPLAY( fields[eCol_qname] ) << DISPLAY( query->GetId() ) << endl;
        if( fields[eCol_qname] != query->GetId() ) {
            if( m_sortOrder == "query" ) 
                if( fields[eCol_qname] > query->GetId() ) break; 
                else THROW( runtime_error, "Error: SO:query is specified, but either it is wrong or it differs from read file order" );
            else break;
        }
        
        unsigned flags = NStr::StringToInt( fields[eCol_flags] );

        if( query->IsPairedRead() != bool( flags & fPairedQuery ) ) {
            THROW( runtime_error, "SAM format error: input indicates that query " << query->GetId() << " is " << (query->IsPairedRead()?"":"not ") << 
                    " paired, SAM flags indicates that it is " << (flags & fPairedQuery?"":"not ") << " paired, line " << m_linenumber );
        }

        Uint8 seq1 = m_seqIds->Register( fields[eCol_rname] );
        Uint8 seq2 = C_Key::NullSeq();
        int pos1 = NStr::StringToInt( fields[eCol_pos] );
        int pos2 = 0;

        if( flags & fPairedQuery ) {
            if( flags & fPairedHit ) {
                if( fields.size() < 7 )
                    THROW( runtime_error, "OligoFAR needs mate position in SAM file for paired reads" );
                seq2 = m_seqIds->Register( fields[eCol_mrnm] );
                pos2 = NStr::StringToInt( fields[eCol_mpos] );
            }
        }
        C_Key key( seq1, pos1, seq2, pos2, flags, m_linenumber );

        int count = 0;
        THitMap::iterator x = hitMap.find( key );
        string seq = fields.size() > eCol_seq ? fields[eCol_seq] : "";
        string qual = fields.size() > eCol_qual ? fields[eCol_qual] : "";
        string mism = "";

        for( unsigned i = eCol_tags; i < fields.size(); ++i ) {
            // spaces are allowed in tags (see SAM 1.2)
            if( fields[i].substr( 0, 5 ) == "MD:Z:" ) mism = fields[i].substr( 5 );
        }
        
        for( THitMap::iterator y = x; y != hitMap.end() && y->first == x->first; ++y ) {
            y->second.Adjust( key, fields[eCol_cigar], mism, seq, qual );
            ++count;
        }
        if( count == 0 ) 
            x = hitMap.insert( x, make_pair( key, C_Value( key, fields[eCol_cigar], mism, seq, qual ) ) );
    }

    // To build correct results we need to score and check geometry settings 
    ITERATE( THitMap, hit, hitMap ) 
        AddHit( query, hit->first, hit->second );

//    cerr << DISPLAY( hitMap.size() ) << endl;

    return false;
}
    
TTrVector CGuideSamFile::GetFullCigar( const TTrVector& cigar, const string& mismatches )
{
    // cigar  = 10A5^C10
    // misms  = 16M1D4M1I6M
    // result = 10M1R5M1D4M1I6M
    
    TTrVector fullcigar;
    const char * misms = mismatches.c_str();
    if( misms[0] ) {
        const char * m = misms;
        TTrVector::const_iterator c = cigar.begin();
        
        while( c != cigar.end() && c->GetEvent() == CTrBase::eEvent_SoftMask || c->GetEvent() == CTrBase::eEvent_HardMask ) 
            fullcigar.AppendItem( *c++ );
        
        int acc = 0;
        
        string errmsgbase;
        do {
            ostringstream s;
            s << "Inconsistent cigar " << cigar.ToString() << " and mismatch string " << misms;
            errmsgbase = s.str();
        } while( 0 );
        
        while( *m ) {
            if( c == cigar.end() ) 
                THROW( runtime_error, errmsgbase << ": cigar is too short" );
            if( isdigit( *m ) ) {
                int count = strtol( m, (char**)&m, 10 );
                if( c->GetEvent() != CTrBase::eEvent_Match ) 
                    THROW( runtime_error, errmsgbase << ": cigar M is expected" );
                acc -= c->GetCount() - count;
                fullcigar.AppendItem( CTrBase::eEvent_Match, min( (int) c->GetCount(), count ) );
                if( acc == 0 ) { ++c; continue; }
                else if( acc < 0 ) {
                    ++c;
                    // NB: overlap is not supported here... it is not clear what to do according to SAM specs
                    if( c == cigar.end() || c->GetEvent() != CTrBase::eEvent_Insertion ) 
                        THROW( runtime_error, errmsgbase << ": cigar I is expected" );
                    fullcigar.AppendItem( CTrBase::eEvent_Insertion, c->GetCount() );
                    ++c;
                } else { // acc > 0
                    // do nothing
                }
            } else if( strchr( "ACGTN", *m ) ) {
                fullcigar.AppendItem( CTrBase::eEvent_Replaced, 1 ); 
                ++c;
            } else if( *m == '^' ) {
                if( acc != 0 ) 
                    THROW( runtime_error, errmsgbase << ": misplaced deletion" ); 
                int k = c->GetCount();
                if( c->GetEvent() != CTrBase::eEvent_Deletion && c->GetEvent() != CTrBase::eEvent_Splice ) 
                    THROW( runtime_error, errmsgbase << ": N or D is expected" );
                while( strchr( "ACGTN", *++m ) ) {
                    fullcigar.AppendItem( c->GetEvent(), 1 );
                    --k;
                }
                if( k != 0 ) 
                    THROW( runtime_error, errmsgbase << ": different sizes of deletion or splice" );
                ++c;
            }
        }
        while( c != cigar.end() ) {
            if( c->GetEvent() != CTrBase::eEvent_SoftMask && c->GetEvent() != CTrBase::eEvent_HardMask ) 
                THROW( runtime_error, errmsgbase << ": cigar is too long" );
            fullcigar.AppendItem( *c++ );
        }
    } else {
        fullcigar = cigar;
    }
    return fullcigar;
}

double CGuideSamFile::ComputeScore( CQuery * query, const C_Value& value, int mate )
{
    // 2009/06/10 - SAM should either have CIGAR which inlcudes 'R', or should include tag MD:[0-9]+(([ACGTN]|\^[ACGTN]+)[0-9]+)*
    // TODO: involve query sequence to score computation later
    TTrVector& fullcigar = m_cigar[mate];
    fullcigar = GetFullCigar( value.GetCigar(mate), value.GetMismatches(mate) );
    m_len[mate] = 0;
    double score = 0;
    ITERATE( TTrVector, c, fullcigar ) {
        switch( c->GetEvent() ) {
            case CTrBase::eEvent_Overlap: m_len[mate] -= c->GetCount();
            case CTrBase::eEvent_NULL: 
            case CTrBase::eEvent_Padding: 
            case CTrBase::eEvent_HardMask: 
            case CTrBase::eEvent_SoftMask: break;
            case CTrBase::eEvent_Changed:
            case CTrBase::eEvent_Match:    score += m_scoreParam->GetIdentityScore() * c->GetCount(); m_len[mate] += c->GetCount(); break;
            case CTrBase::eEvent_Replaced: score -= m_scoreParam->GetMismatchPenalty() * c->GetCount(); m_len[mate] += c->GetCount(); break;
            case CTrBase::eEvent_Insertion: score -= m_scoreParam->GetGapBasePenalty() + m_scoreParam->GetGapExtentionPenalty() * c->GetCount(); break;
            case CTrBase::eEvent_Deletion: score -= m_scoreParam->GetGapBasePenalty() + m_scoreParam->GetGapExtentionPenalty() * c->GetCount(); 
            case CTrBase::eEvent_Splice:   m_len[mate] += c->GetCount(); 
                                           break;
        }
    }
//    cerr << DISPLAY( fullcigar.ToString() ) << DISPLAY( score ) << DISPLAY( query->GetBestScore( mate ) ) << endl;
    return 100 * score / query->GetBestScore( mate );
}
                                  
void CGuideSamFile::AddHit( CQuery * query, const C_Key& key, const C_Value& value )
{
    // compute score(s)
    double score1 = ComputeScore( query, value, 0 );
    double score2 = ComputeScore( query, value, 1 );
    // compute if it fits geometry
    if( key.IsPairedHit() != value.IsPairedHit() )
        THROW( runtime_error, "Failed to match all pairs for " << query->GetId() );
    if( !value.IsValid() ) 
        THROW( runtime_error, "Failed to match all hits for " << query->GetId() );
    int from1 = key.GetPos(0);
    int to1 = from1 + m_len[0] - 1;
    if( key.IsReverse(0) ) { swap( from1, to1 ); }
    if( key.IsPairedHit() ) {
        Uint1 hflags = ( key.IsOrderReverse() ? CHit::fOrder_reverse : CHit::fOrder_forward );
        hflags |= ( key.GetFlags() & fSeqReverse ) ? CHit::fRead1_reverse : CHit::fRead1_forward;
        hflags |= ( key.GetFlags() & fMateReverse ) ? CHit::fRead2_reverse : CHit::fRead2_forward;
        int from2 = key.GetPos(1);
        int to2 = from2 + m_len[1] - 1;
        if( key.IsReverse(1) ) { swap( from2, to2 ); }
        if( hflags != m_filter->GetGeometry() || key.GetSeq(0) != key.GetSeq(1) ) {
            // create single hits
            m_filter->PurgeHit( new CHit( query, key.GetSeq(0), 0, score1, from1, to1, 0, value.GetCigar(0) ) );
            m_filter->PurgeHit( new CHit( query, key.GetSeq(1), 1, score1, from2, to2, 0, value.GetCigar(1) ) );
        } else {
            // create double hit
            m_filter->PurgeHit( new CHit( query, key.GetSeq(0), 
                        score1, from1, to1, 0, 
                        score2, from2, to2, 0, 
                        value.GetCigar(0), value.GetCigar(1) ) );
  
        }
    } else {
        // create single hit
        m_filter->PurgeHit( new CHit( query, key.GetSeq(0), 0, score1, from1, to1, 0, value.GetCigar(0) ) );
    }
}

