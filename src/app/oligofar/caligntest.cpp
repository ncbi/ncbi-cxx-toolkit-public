#include <ncbi_pch.hpp>
#include "caligntest.hpp"
#include "aligners.hpp"

#include "cprogressindicator.hpp"
#include "ialigner.hpp"
#include "string-util.hpp"

#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>

#ifndef _WIN32
#include <sys/resource.h>
#else
#define strtoll( a, b, c ) strtoui64( a, b, c )
#endif

USING_OLIGOFAR_SCOPES;

CAlignTest::CAlignTest( int argc, char ** argv ) : 
    CApp( argc, argv ),
    m_algo( eAlignment_fast ),
    m_flags( 0 ),
    m_offsetQuery( 0 ),
    m_offsetSubject( 0 ),
    m_xDropoff( 2 ),
    m_identityScore( 1 ),
    m_mismatchScore( -1 ),
    m_gapOpeningScore( -3 ),
    m_gapExtentionScore( -1.5 )
{}
                                                   
void CAlignTest::Help( const char * arg )
{
    cout << "usage: [-hV] [-r h|s|f] [-c +|-] [-q rc] [-s rc] [-R +|-] [-Q qoffset] [-S soffset] query subj\n"
        << "where:\n"
        << "  --algorithm=h|s|f       -r h|s|f      algorithm to use (HSP, Smith-Waterman, Fast) [" << char( m_algo ) << "]\n"
        << "  --colorspace=yes|no     -c yes|no     align in colorspace [" << (m_flags & fAlign_colorspace ? "yes" : "no") << "]\n"
        << "  --query-strand=+|-      -q +|-        use query strand [" << (m_flags & fQuery_reverse ? "-":"+" ) << "]\n"
        << "  --subject-strand=+|-    -s +|-        use subject strand [" << (m_flags & fSubject_reverse ? "-":"+" ) << "]\n"
        << "  --query-offset=off      -Q off        use position off on query as seeding position [" << m_offsetQuery << "]\n"
        << "  --subject-offset=off    -S off        use position off on subject as seeding position [" << m_offsetQuery << "]\n"
        << "  --score-identity=val    -I val        identity score [" << m_identityScore << "]\n"
        << "  --score-mismatch=val    -M val        mismatch score [" << m_mismatchScore << "]\n"
        << "  --score-gap-opening=val -G val        gap opening score [" << m_gapOpeningScore << "]\n"
        << "  --score-gap-extention=v -g val        gap extention score [" << m_gapExtentionScore << "]\n"
        << "  --x-dropoff=val         -X val        maximal gap width for -rs [" << m_xDropoff << "]\n"
        ;
}

const option * CAlignTest::GetLongOptions() const
{
    static struct option opt[] = {
        {"help", 0, 0, 'h'},
        {"version", 0, 0, 'V'},
        {"algorithm", 1, 0, 'r'},
        {"colorspace", 1, 0, 'c'},
        {"query-strand", 1, 0, 'q'},
        {"subject-strand", 1, 0, 's'},
        {"query-offset", 1, 0, 'Q'},
        {"subject-offset", 1, 0, 'S'},
        {"score-identity", 1, 0, 'I'},
        {"score-mismatch", 1, 0, 'M'},
        {"score-gap-opening", 1, 0, 'G'},
        {"score-gap-extention", 1, 0, 'g'},
        {"x-dropoff", 1, 0, 'X'},
        {0,0,0,0}
    };
    return opt;
}

const char * CAlignTest::GetOptString() const
{
    return "hVr:c:q:s:Q:S:I:M:G:g:X:";
}

int CAlignTest::ParseArg( int opt, const char * arg, int longindex )
{
    switch( opt ) {
    case 'r': m_algo = *arg == 'h' ? eAlignment_HSP : *arg == 's' ? eAlignment_SW : *arg == 'f' ? eAlignment_fast : ((cerr << "Warning: unknown alignment algorithm " << arg << endl), m_algo ); break;
    case 'c': SetFlags( fAlign_colorspace, arg ); break;
    case 'q': SetFlags( fQuery_reverse, arg ); break;
    case 's': SetFlags( fSubject_reverse, arg ); break;
    case 'Q': m_offsetQuery = Convert( arg ); break;
    case 'S': m_offsetSubject = Convert( arg ); break;
    case 'X': m_xDropoff = Convert( arg ); break;
    case 'I': m_identityScore = Convert( arg ); break;
    case 'M': m_mismatchScore = Convert( arg ); break;
    case 'G': m_gapOpeningScore = Convert( arg ); break;
    case 'g': m_gapExtentionScore = Convert( arg ); break;
    default: return CApp::ParseArg( opt, arg, longindex );
    }
    return 0;
}

int CAlignTest::Execute()
{
    CScoreTbl scoreTbl( m_identityScore, m_mismatchScore, m_gapOpeningScore, m_gapExtentionScore );
    auto_ptr<IAligner> aligner( CreateAligner( m_algo, &scoreTbl ) );
    CAlignerBase::SetPrintDebug( true );

    if( m_flags & fAlign_colorspace )
        THROW( logic_error, "Colorspace option is not immplemented" );

    if( GetArgIndex() + 2 > GetArgCount() )
        THROW( runtime_error, "Two sequences are required!" );

    if( m_offsetQuery < 0 || m_offsetSubject < 0 )
        THROW( runtime_error, "Offset should never be negative" );

    const char * qs = GetArg( GetArgIndex() );
    const char * ss = GetArg( GetArgIndex() + 1 );
    int ql = strlen( qs );
    int sl = strlen( ss );

    cerr << DISPLAY( qs ) << DISPLAY( ql ) << endl;
    cerr << DISPLAY( ss ) << DISPLAY( sl ) << endl;

    if( ql == 0 || sl == 0 ) 
        THROW( runtime_error, "Sequences should have at least one base!" );

    vector<char> query;
    vector<char> subject;
    query.reserve( ql );
    subject.reserve( sl );

    while( *qs ) query.push_back( CNcbi8naBase( CIupacnaBase( *qs++ ) ) );
    while( *ss ) subject.push_back( CNcbi8naBase( CIupacnaBase( *ss++ ) ) );

    qs = &query[0];
    ss = &subject[0];

    qs += m_offsetQuery; ql -= m_offsetQuery;
    ss += m_offsetSubject; sl -= m_offsetSubject;

    if( ql <= 0 || sl <= 0 ) 
        THROW( runtime_error, "Offset is too big" );

    if( GetFlags( fQuery_reverse ) ) { qs = qs + ql; ql = -ql; }
    if( GetFlags( fSubject_reverse ) ) { ss = ss + sl; sl = -sl; }

    int aflags = CAlignerBase::fComputePicture | CAlignerBase::fComputeScore | CAlignerBase::fPictureSubjectStrand;

    aligner->SetBestPossibleQueryScore( min( ql, sl ) * m_identityScore );
    aligner->Align( CSeqCoding::eCoding_ncbi8na, qs, ql, CSeqCoding::eCoding_ncbi8na, ss, sl, aflags );

    const CAlignerBase& abase = aligner->GetAlignerBase();

    cout << ( GetFlags( fQuery_reverse ) ? 3 : 5 ) << "'=" << abase.GetQueryString() << "=" << ( GetFlags( fQuery_reverse ) ? 5 : 3 ) << "'\n";
    cout << "   " << abase.GetAlignmentString() << "  "
        << "  i=" << abase.GetIdentityCount() 
        << ", m=" << abase.GetMismatchCount() 
        << ", g=" << abase.GetIndelCount() 
        << ", s=" << abase.GetRawScore() << "/" << abase.GetBestQueryScore() << "=" << abase.GetScore() << "%\n";
    cout << "5'=" << abase.GetSubjectString() << "=3'\n";

    return 0;
}

IAligner * CAlignTest::NewAligner( EAlignmentAlgo algo, int xdropoff ) const
{
//    if( xdropoff == 0 ) return new CAligner_HSP();
    switch( algo ) {
    case eAlignment_fast: return new CAligner_fast();
    case eAlignment_HSP: return new CAligner_HSP();
    case eAlignment_SW:
        do {
            auto_ptr<CAligner_SW> swalign( new CAligner_SW() );
            swalign->SetMatrix().resize( 2*xdropoff + 1 );
            return swalign.release();
        } while(0); break;
    default: THROW( logic_error, "Bad value for m_algo" );
    }
}

IAligner * CAlignTest::CreateAligner( EAlignmentAlgo algo, CScoreTbl * scoreTbl ) const 
{
    auto_ptr<IAligner> aligner( NewAligner( algo, m_xDropoff ) );
    ASSERT( aligner.get() );
    aligner->SetScoreTbl( *scoreTbl );
    return aligner.release();
}
