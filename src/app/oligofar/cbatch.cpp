#include <ncbi_pch.hpp>
#include "cbatch.hpp"
#include "caligner.hpp"
#include "cprogressindicator.hpp"
#include "ioutputformatter.hpp"
#include "cseqscanner.hpp"

USING_OLIGOFAR_SCOPES;

CBatch::~CBatch() {}

void CBatch::Start() 
{
    m_batchOrdinal = 0;
    SetPass(0);
}

void CBatch::SetPass( unsigned pass )
{
    ASSERT( pass < m_passParam->size() );
    m_pass = pass;

    ASSERT( m_filter );
    m_filter->SetMaxDist( GetPassParam(pass).GetMaxDist() );
    m_filter->SetMinDist( GetPassParam(pass).GetMinDist() );
    m_filter->SetPrefilter( GetPassParam(pass).GetAlignParam().GetSpliceSet().empty() );

    ASSERT( m_queryHash );
    m_queryHash->SetParam( GetPassParam(pass).GetHashParam() );
    m_queryHash->SetHaveSplices( GetPassParam( pass ).GetAlignParam().GetSpliceSet().size() ); 

    ASSERT( m_seqVecProcessor );

    ASSERT( m_seqScanner );
    m_seqScanner->SetMaxAmbiguities( GetPassParam(pass).GetMaxSubjAmb() );

    ASSERT( m_scoringFactory );
    m_scoringFactory->SetAlignParam( &GetPassParam(pass).GetAlignParam() );

    ASSERT( m_formatter );
}

int CBatch::AddQuery( CQuery * query )
{
    int cnt = 0;
    m_inputChunk.push_back( query ); // do not hash if we have guided hit
    if( query->HasHits() ) { ++m_guidedReads; }
    int hasheableReads = m_inputChunk.size() - m_guidedReads;
    if( hasheableReads && hasheableReads % m_readsPerRun == 0 || 
        int( m_inputChunk.size() ) >= 2*m_readsPerRun ) Purge();
    return cnt;
}

int CBatch::x_EstimateMismatchCount( const CQuery* query, bool matepair ) const
{
    const int kNoHitEquivalent = 1000;
    ASSERT( query != 0 );
    int matemask = 1 << matepair;
    const CHit * hit = query->GetTopHit(3); // paired reads have precedence - which means that it is more important
    if( hit == 0 ) hit = query->GetTopHit( matemask );
    if( hit == 0 ) return kNoHitEquivalent;
    if( query->HasComponent( matepair ) ) {
        if( hit->HasComponent( matepair ) ) { // should always be true
            return 0; //TODO: change!!! int( query->GetBestScore( matepair ) * ( 1.0 - hit->GetScore( matepair ) / 100.0 ) / m_mismatchPenalty );
        } else return kNoHitEquivalent;
    } else return 0; // no read - no mismatches, n'est pas?
}

void CBatch::x_Rehash( unsigned pass ) 
{
    double id = m_scoringFactory->GetScoreParam()->GetIdentityScore();
    double mm = m_scoringFactory->GetScoreParam()->GetMismatchPenalty(); 
    double go = m_scoringFactory->GetScoreParam()->GetGapBasePenalty();
    double ge = m_scoringFactory->GetScoreParam()->GetGapExtentionPenalty();

    int w = max(m_queryHash->GetScannerWindowLength(), GetPassParam(pass).GetHashParam().GetWindowSize());

    double Pm = w * id; // numeric_limits<double>::max();
    double Sm = 100.0;
    int nm = 0;
    int im = 0;
    int dm = 0;

    if( pass != 0 ) {
        int n = m_queryHash->GetMaxMismatches();
        int i = m_queryHash->GetMaxInsertions();
        int d = m_queryHash->GetMaxDeletions();
        int D = m_queryHash->GetMaxDistance();

        // here we do simple naive enumerating of all cases
        for( int nn = 0; nn <= n; ++nn ) {
            double pn = nn * (mm + id);
            for( int ii = 0; ii <= i; ++ii ) {
                if( (nn + ii) == D || ii == i ) {
                    double pi = ii ? go + ii*ge : 0;
                    if( pn + pi < Pm ) {
                        Pm = pn + pi;
                        nm = nn;
                        im = ii;
                        dm = 0;
                    }
                }
            }
            for( int dd = 0; dd <= d; ++dd ) {
                if( (nn + dd) == D || dd == d ) {
                    double pd = dd ? go + dd*(ge + id) : 0;
                    if( pn + pd < Pm ) {
                        Pm = pn + pd;
                        nm = nn;
                        dm = dd;
                        im = 0;
                    }
                }
            }
        }
        // Pm here has the smallest penalty
        Sm = 100 * (1 - (Pm)/ ((w) * id)); // consider that next after word we may have mismatch
        //Sm = 100 * (1 - (Pm + mm + id)/ ((w + 1) * id)); // consider that next after word we may have mismatch
    }
    // just to make comparison more correct we take half of minimal penalty considering longest read
    Sm -= min( id + mm, go + ge ) / 2 / 255 * w * 100; 
    // Here Pm should contain maximal penalty accountable for window with this pass hashing scheme
    
    SetPass( pass );

    ostringstream msg0;
    msg0 << "pass" << pass 
         << " n=" << m_queryHash->GetMaxMismatches() 
         << " i=" << m_queryHash->GetMaxInsertions()
         << " d=" << m_queryHash->GetMaxDeletions()
         << " E=" << m_queryHash->GetMaxDistance()
         << " w=" << m_queryHash->GetWindowSize() << "/" << m_queryHash->GetWordSize() << " (" << m_queryHash->GetIndexBits() << "b)"
         << " S=" << m_queryHash->GetStrideSize() 
         ;
    if( pass ) {
        msg0
    //     << " %s < " << Sm
             << " Pm < " << (-Pm)
             << " (n=" << nm << ";i=" << im << ";d=" << dm << ";w=" << w << ")";
    }

    CProgressIndicator progress( "Hashing " + msg0.str() );

    int entries = 0;
    m_hashedReads = 0;
    ITERATE( TInputChunk, i, m_inputChunk ) {
        ASSERT( *i );
        CHit * top = (*i)->GetTopHit();
        bool rehash = top == 0;
        if( !rehash ) {
            /*
            double S = top->GetTotalScore();
            unsigned length = (*i)->GetLength(0);
            if( (*i)->IsPairedRead() ) {
                S -= 100;
                length = min( length, (*i)->GetLength(1) );
            }
            if( S*p/10000 < 1 - Pm/(id*length) ) rehash = true;
            */
            /*
            if( (*i)->IsPairedRead() ) {
                if( top->GetTotalScore() - top->GetQuery()->GetBestScore() < Sm ) rehash = true;
            } else {
                if( top->GetTotalScore() < Sm ) rehash = true;
            }
            */
#if DEVELOPMENT_VER
            //cerr << DISPLAY( top->GetTotalScore() ) << DISPLAY( top->GetQuery()->ComputeBestScore( m_scoringFactory->GetScoreParam() ) ) << DISPLAY( Pm ) << "\n";
            if( top->GetTotalScore() - top->GetQuery()->ComputeBestScore( m_scoringFactory->GetScoreParam() ) < -Pm ) rehash = true;
#else
            if( top->GetTotalScore() - top->GetQuery()->GetBestScore() < -Pm ) rehash = true;
#endif
        }
        if( rehash ) {
            //(*i)->ClearHits();
            entries += m_queryHash->AddQuery( *i );
            m_hashedReads++;
            progress.Increment();
        }
    }
    progress.SetMessage( "Created " + NStr::IntToString( entries ) + " entries for " + msg0.str() );
    progress.Summary();
}

void CBatch::Purge()
{
    if( m_batchOrdinal >= m_minBatch && m_batchOrdinal <= m_maxBatch ) { 
        if( CProgressIndicator * p = m_readProgressIndicator) p->Format( CProgressIndicator::e_final );

        ASSERT( m_passParam->size() );
        for( m_pass = 0; m_pass < int( m_passParam->size() ); ++m_pass ) {
            x_Rehash( m_pass );
            if( m_hashedReads ) m_seqVecProcessor->Process( m_fastaFile );
            m_queryHash->Clear();
        }

        if( m_guidedReads && m_formatter->NeedSeqids() ) x_LoadSeqIds();

        CProgressIndicator p( "Purging " + NStr::Int8ToString( CHit::GetCount() ) + " hits for " + NStr::Int8ToString( CQuery::GetCount() ) + " entries" );
        ITERATE( TInputChunk, i, m_inputChunk ) {
            m_formatter->FormatQueryHits( *i );
        //    delete *i;
            p.Increment();
        }
        p.Summary();
    } else {
        //cerr << "* Skipping batch " << m_batchOrdinal << "\n";
        CProgressIndicator pp( "Skipping batch " + NStr::IntToString( m_batchOrdinal ) );
        ITERATE( TInputChunk, i, m_inputChunk ) {
            delete *i;
        }
        pp.Summary();
    }
    m_inputChunk.clear();
    m_hashedReads = 0;
    m_guidedReads = 0;
    m_ignoredReads = 0;
    m_hashEntries = 0;
    ++m_batchOrdinal;
}

void CBatch::x_LoadSeqIds()
{
    cerr << "Loading seq ids for guided hits..." << flush;
    m_queryHash->Clear();
    m_seqVecProcessor->Process( m_fastaFile );
    cerr << "\b\b\b: OK\n";
}
