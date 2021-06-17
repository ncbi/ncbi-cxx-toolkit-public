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
                << "Warning: neighter sort nor group order is specified for the guide file;\n"
                << "         implying group order by query, but most of the file may be ignored if\n"
                << "         it's order differs from read file.\n";
        }
    }
}

bool CGuideSamFile::NextHit( Uint8 ordinal, CQuery * query )
{
    // Note: code may be modified to create query directly from guide file if the file contains sequence data
    if( m_buff.length() == 0 ) return false;
    typedef pair<string,int> TRefLoc;
    typedef multimap<TRefLoc,CSamRecord*> TPairedHits;
    TPairedHits pairedHits;
    while( m_buff.length() ) {
        auto_ptr<CSamRecord> hit( new CSamRecord( m_buff ) );
        // cerr << DISPLAY( query->GetId() ) << DISPLAY( hit->GetQName() ) << DISPLAY( m_linenumber ) << "\n";
        if( strcmp( hit->GetQName(), query->GetId() ) ) break;
        if( !getline( m_in, m_buff ) ) m_buff.clear();
        ++m_linenumber;

        if( hit->IsUnmapped() ) {
            AddQuery( query, hit.release() );
        } else if( hit->IsPairedHit() ) {
            // match-and-add paired
            TRefLoc refloc( hit->GetRName(), hit->GetPos() );
            TPairedHits::iterator x = pairedHits.find( refloc );
            bool found = false;
            for( ; x != pairedHits.end() && x->first == refloc; ++x ) {
                if( x->second->IsHitMateOf( *hit ) ) {
                    AddPair( query, x->second, hit.release() ); // should delete both x->second and hit
                    pairedHits.erase( x );
                    found = true;
                    break;
                }
            }
            if( ! found ) {
                TRefLoc mrefloc( hit->GetRName(), hit->GetMatePos() );
                pairedHits.insert( make_pair( mrefloc, hit.release() ) );
            }
        } else {
            // add single
            AddSingle( query, hit.release() );
        }
    }
    if( pairedHits.size() ) {
        cerr << "Warning: failed to find pairs for " << pairedHits.size() << " SAM hits for query " << pairedHits.begin()->second->GetQName() << ":\n";
        ITERATE( TPairedHits, ph, pairedHits ) {
            cerr << "->\t";
            ph->second->WriteAsSam( cerr );
            delete ph->second;
        }
    }

    return false;
}

void CGuideSamFile::AddQuery( CQuery *, CSamRecord * hit )
{
    delete hit;
}

void CGuideSamFile::AddSingle( CQuery * query, CSamRecord * sam ) 
{
    double id = m_scoreParam->GetIdentityScore();
    double mm = m_scoreParam->GetMismatchPenalty();
    double go = m_scoreParam->GetGapOpeningPenalty();
    double ge = m_scoreParam->GetGapExtentionPenalty();
    CHit * hit = new CHit( query, m_seqIds->Register( sam->GetRName() ), sam->ComputeScore( id, mm, go, ge ), 0, sam, 0 );
    m_filter->PurgeHit( hit );
    delete sam;
}

void CGuideSamFile::AddPair( CQuery * query, CSamRecord * asam, CSamRecord * bsam )
{
    double id = m_scoreParam->GetIdentityScore();
    double mm = m_scoreParam->GetMismatchPenalty();
    double go = m_scoreParam->GetGapOpeningPenalty();
    double ge = m_scoreParam->GetGapExtentionPenalty();

    if( asam->GetFlags() & CSamBase::fSeqIsSecond ) swap( asam, bsam );
    Uint1 hflags = asam->GetPos() < bsam->GetPos() ? CHit::fOrder_forward : CHit::fOrder_reverse;
    hflags |= asam->IsReverse() ? CHit::fRead1_reverse : CHit::fRead1_forward;
    hflags |= bsam->IsReverse() ? CHit::fRead2_reverse : CHit::fRead2_forward;
    Uint8 seqord = m_seqIds->Register( asam->GetRName() );
    if( hflags != m_filter->GetGeometry() ) {
        asam->SetSingle();
        bsam->SetSingle();
        m_filter->PurgeHit( new CHit( query, seqord, asam->ComputeScore( id, mm, go, ge ), 0, asam, 0 ) );
        m_filter->PurgeHit( new CHit( query, seqord, bsam->ComputeScore( id, mm, go, ge ), 0, bsam, 0 ) );
    } else if( asam->IsHitMateOf( *bsam ) ) {
        m_filter->PurgeHit( new CHit( query, seqord, asam->ComputeScore( id, mm, go, ge ), bsam->ComputeScore( id, mm, go, ge ), asam, bsam ) );
    }
    delete asam;
    delete bsam;
}

/*
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
#if DEVELOPMENT_VER
    return 100 * score / query->ComputeBestScore( m_scoreParam, mate );
#else
    return 100 * score / query->GetBestScore( mate );
#endif
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
*/
