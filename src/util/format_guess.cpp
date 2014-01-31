/*  $Id$
 * ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnology Information
 *
 *  This software/database is a "United States Government Work" under the
 *  terms of the United States Copyright Act.  It was written as part of
 *  the author's official duties as a United States Government employee and
 *  thus cannot be copyrighted.  This software/database is freely available
 *  to the public for use. The National Library of Medicine and the U.S.
 *  Government have not placed any restriction on its use or reproduction.
 *
 *  Although all reasonable efforts have been taken to ensure the accuracy
 *  and reliability of the software and data, the NLM and the U.S.
 *  Government do not and cannot warrant the performance or results that
 *  may be obtained by using this software or data. The NLM and the U.S.
 *  Government disclaim all warranties, express or implied, including
 *  warranties of performance, merchantability or fitness for any particular
 *  purpose.
 *
 *  Please cite the author in any work or product based on this material.
 *
 * ===========================================================================
 *
 * Author: Anatoliy Kuznetsov
 *
 * File Description:  Implemented methods to identify file formats.
 *
 */

#include <ncbi_pch.hpp>
#include <util/format_guess.hpp>
#include <util/util_exception.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/ncbistre.hpp>
#include <corelib/stream_utils.hpp>

BEGIN_NCBI_SCOPE

enum ESymbolType {
    fDNA_Main_Alphabet  = 1<<0, ///< Just ACGTUN-.
    fDNA_Ambig_Alphabet = 1<<1, ///< Anything else representable in ncbi4na.
    fProtein_Alphabet   = 1<<2, ///< Allows BZX*-, but not JOU.
    fLineEnd            = 1<<3,
    fAlpha              = 1<<4,
    fDigit              = 1<<5,
    fSpace              = 1<<6,
    fInvalid            = 1<<7
};

enum EConfidence {
    eNo = 0,
    eMaybe,
    eYes
};


//  ============================================================================
//  Helper routine--- file scope only:
//  ============================================================================

static unsigned char symbol_type_table[256];

//  ----------------------------------------------------------------------------
static bool s_IsTokenPosInt(
    const string& strToken )
{
    return ( -1 != NStr::StringToNonNegativeInt( strToken ) );
}

//  ----------------------------------------------------------------------------
static bool s_IsTokenInteger(
    const string& strToken )
//  ----------------------------------------------------------------------------
{
    if ( ! strToken.empty() && strToken[0] == '-' ) {
        return s_IsTokenPosInt( strToken.substr( 1 ) );
    }
    return s_IsTokenPosInt( strToken );
}

//  ----------------------------------------------------------------------------
static bool s_IsTokenDouble(
    const string& strToken )
{
    string token( strToken );
    NStr::ReplaceInPlace( token, ".", "1", 0, 1 );
    if ( token.size() > 1 && token[0] == '-' ) {
        token[0] = '1';
    }
    return s_IsTokenPosInt( token );
}

//  ----------------------------------------------------------------------------
static void init_symbol_type_table(void)
{
    if ( symbol_type_table[0] == 0 ) {
        for ( const char* s = "ACGNTU"; *s; ++s ) {
            unsigned char c = *s;
            symbol_type_table[c] |= fDNA_Main_Alphabet;
            c = tolower(c);
            symbol_type_table[c] |= fDNA_Main_Alphabet;
        }
        for ( const char* s = "BDHKMRSVWY"; *s; ++s ) {
            unsigned char c = *s;
            symbol_type_table[c] |= fDNA_Ambig_Alphabet;
            c = tolower(c);
            symbol_type_table[c] |= fDNA_Ambig_Alphabet;
        }
        for ( const char* s = "ACDEFGHIKLMNPQRSTVWYBZX"; *s; ++s ) {
            unsigned char c = *s;
            symbol_type_table[c] |= fProtein_Alphabet;
            c = tolower(c);
            symbol_type_table[c] |= fProtein_Alphabet;
        }
        symbol_type_table[(unsigned char)'-']
            |= fDNA_Main_Alphabet | fProtein_Alphabet;
        symbol_type_table[(unsigned char)'*'] |= fProtein_Alphabet;
        for ( const char* s = "\r\n"; *s; ++s ) {
            unsigned char c = *s;
            symbol_type_table[c] |= fLineEnd;
        }
        for ( int c = 1; c < 256; ++c ) {
            if ( isalpha(c) )
                symbol_type_table[c] |= fAlpha;
            if ( isdigit(c) )
                symbol_type_table[c] |= fDigit;
            if ( isspace(c) )
                symbol_type_table[c] |= fSpace;
        }
        symbol_type_table[0] |= fInvalid;
    }
}

//  ----------------------------------------------------------------------------
int
CFormatGuess::s_CheckOrder[] =
//  ----------------------------------------------------------------------------
{
    //  must list all EFormats except eUnknown and eFormat_max. Will cause
    //  assertion if violated!
    //
    eBam, // must precede eGZip!
    eZip,
    eGZip,
    eBZip2,
    eLzo,
    eSra,
    eRmo,
    eVcf,
    eGtf,
    eGvf,
    eGff3,
    eGff2,
    eGlimmer3,
    eAgp,
    eXml,
    eWiggle,
    eBed,
    eBed15,
    eNewick,
    eHgvs,
    eAlignment,
    eDistanceMatrix,
    eFlatFileSequence,
    eFiveColFeatureTable,
    eSnpMarkers,
    eFasta,
    eTextASN,
    eTaxplot,
    ePhrapAce,
    eTable,
    eBinaryASN,
};


// This array must stay in sync with enum EFormat, but that's not
// supposed to change in the middle anyway, so the explicit size
// should suffice to avoid accidental skew.
const char* const CFormatGuess::sm_FormatNames[CFormatGuess::eFormat_max] = {
    "unknown",
    "binary ASN.1",
    "RepeatMasker",
    "GFF/GTF Poisoned",
    "Glimmer3",
    "AGP",
    "XML",
    "WIGGLE",
    "BED",
    "BED15",
    "Newick",
    "alignment",
    "distance matrix",
    "flat-file sequence",
    "five-column feature table",
    "SNP Markers",
    "FASTA",
    "text ASN.1",
    "Taxplot",
    "Phrap ACE",
    "table",
    "GTF",
    "GFF3",
    "GFF2",
    "HGVS",
    "GVF",
    "zip",
    "gzip",
    "bzip2",
    "lzo",
    "SRA",
    "BAM",
    "VCF",
};

const char*
CFormatGuess::GetFormatName(EFormat format)
{
    unsigned int i = static_cast<unsigned int>(format);
    if (i >= static_cast <unsigned int>(eFormat_max)) {
        NCBI_THROW(CUtilException, eWrongData,
                   "CFormatGuess::GetFormatName: out-of-range format value "
                   + NStr::IntToString(i));
    }
    return sm_FormatNames[i];
}


//  ============================================================================
//  Old style class interface:
//  ============================================================================

//  ----------------------------------------------------------------------------
CFormatGuess::ESequenceType
CFormatGuess::SequenceType(const char* str, unsigned length,
                           ESTStrictness strictness)
{
    if (length == 0)
        length = (unsigned)::strlen(str);

    init_symbol_type_table();
    unsigned int main_nuc_content = 0, ambig_content = 0, bad_nuc_content = 0,
        amino_acid_content = 0, exotic_aa_content = 0, bad_aa_content = 0;

    for (unsigned i = 0; i < length; ++i) {
        unsigned char c = str[i];
        unsigned char type = symbol_type_table[c];
        if ( type & fDNA_Main_Alphabet ) {
            ++main_nuc_content;
        } else if ( type & fDNA_Ambig_Alphabet ) {
            ++ambig_content;
        } else if ( !(type & (fSpace | fDigit)) ) {
            ++bad_nuc_content;
        }

        if ( type & fProtein_Alphabet ) {
            ++amino_acid_content;
        } else if ( type & fAlpha ) {
            ++exotic_aa_content;
        } else if ( !(type & (fSpace | fDigit)) ) {
            ++bad_aa_content;
        }
    }

    switch (strictness) {
    case eST_Lax:
    {
        double dna_content = (double)main_nuc_content / (double)length;
        double prot_content = (double)amino_acid_content / (double)length;

        if (dna_content > 0.7) {
            return eNucleotide;
        }
        if (prot_content > 0.7) {
            return eProtein;
        }
    }

    case eST_Default:
        if (bad_nuc_content + ambig_content <= main_nuc_content / 9
            ||  (bad_nuc_content + ambig_content <= main_nuc_content / 3  &&
                 bad_nuc_content <= (main_nuc_content + ambig_content) / 19)) {
            // >=90% main alph. (ACGTUN-) or >=75% main and >=95% 4na-encodable
            return eNucleotide;
        } else if (bad_aa_content + exotic_aa_content
                   <= amino_acid_content / 9) {
            // >=90% relatively standard protein residues.  (JOU don't count.)
            return eProtein;
        }

    case eST_Strict: // Must be 100% encodable
        if (bad_nuc_content == 0  &&  ambig_content <= main_nuc_content / 3) {
            return eNucleotide;
        } else if (bad_aa_content == 0
                   &&  exotic_aa_content <= amino_acid_content / 9) {
            return eProtein;
        }
    }

    return eUndefined;
}


//  ----------------------------------------------------------------------------
CFormatGuess::EFormat CFormatGuess::Format(const string& path, EOnError onerror)
{
    CNcbiIfstream input(path.c_str(), IOS_BASE::in | IOS_BASE::binary);
    return Format(input);
}

//  ----------------------------------------------------------------------------
CFormatGuess::EFormat CFormatGuess::Format(CNcbiIstream& input, EOnError onerror)
{
    CFormatGuess FG( input );
    return FG.GuessFormat( onerror );
}


//  ============================================================================
//  New style object interface:
//  ============================================================================

//  ----------------------------------------------------------------------------
CFormatGuess::CFormatGuess()
    : m_Stream( * new CNcbiIfstream )
    , m_bOwnsStream( true )
{
    Initialize();
}

//  ----------------------------------------------------------------------------
CFormatGuess::CFormatGuess(
    const string& FileName )
    : m_Stream( * new CNcbiIfstream( FileName.c_str(), ios::binary ) )
    , m_bOwnsStream( true )
{
    Initialize();
}

//  ----------------------------------------------------------------------------
CFormatGuess::CFormatGuess(
    CNcbiIstream& Stream )
    : m_Stream( Stream )
    , m_bOwnsStream( false )
{
    Initialize();
}

//  ----------------------------------------------------------------------------
CFormatGuess::~CFormatGuess()
{
    delete[] m_pTestBuffer;
    if ( m_bOwnsStream ) {
        delete &m_Stream;
    }
}

//  ----------------------------------------------------------------------------
CFormatGuess::EFormat
CFormatGuess::GuessFormat( EMode )
{
    return GuessFormat(eDefault);
}

//  ----------------------------------------------------------------------------
CFormatGuess::EFormat
CFormatGuess::GuessFormat(
    EOnError onerror )
{
    if (!x_TestInput(m_Stream, onerror)) {
        return eUnknown;
    }
    EMode mode = eQuick;
    unsigned int uFormatCount = sizeof( s_CheckOrder ) / sizeof( int );

    // First, try to use hints
    if ( !m_Hints.IsEmpty() ) {
        for (unsigned int f = 0; f < uFormatCount; ++f) {
            EFormat fmt = EFormat( s_CheckOrder[ f ] );
            if (m_Hints.IsPreferred(fmt)  &&  x_TestFormat(fmt, mode)) {
                return fmt;
            }
        }
    }

    // Check other formats, skip the ones that are disabled through hints
    for (unsigned int f = 0; f < uFormatCount; ++f) {
        EFormat fmt = EFormat( s_CheckOrder[ f ] );
        if ( ! m_Hints.IsDisabled(fmt)  &&  x_TestFormat(fmt, mode) ) {
            return fmt;
        }
    }
    return eUnknown;
}

//  ----------------------------------------------------------------------------
bool
CFormatGuess::TestFormat( EFormat format, EMode )
{
    return TestFormat( format, eDefault);
}

//  ----------------------------------------------------------------------------
bool
CFormatGuess::TestFormat(
    EFormat format,
    EOnError onerror )
{
    if (format != eUnknown && !x_TestInput(m_Stream, onerror)) {
        return false;
    }
    EMode mode = eQuick;
    return x_TestFormat(format, mode);
}

//  ----------------------------------------------------------------------------
bool CFormatGuess::x_TestFormat(EFormat format, EMode mode)
{
    // First check if the format is disabled
    if ( m_Hints.IsDisabled(format) ) {
        return false;
    }

    switch( format ) {

    case eBinaryASN:
        return TestFormatBinaryAsn( mode );
    case eRmo:
        return TestFormatRepeatMasker( mode );
    case eGtf:
        return TestFormatGtf( mode );
    case eGvf:
		return TestFormatGvf( mode );
	case eGff3:
        return TestFormatGff3( mode );
    case eGff2:
        return TestFormatGff2( mode );
    case eGlimmer3:
        return TestFormatGlimmer3( mode );
    case eAgp:
        return TestFormatAgp( mode );
    case eXml:
        return TestFormatXml( mode );
    case eWiggle:
        return TestFormatWiggle( mode );
    case eBed:
        return TestFormatBed( mode );
    case eBed15:
        return TestFormatBed15( mode );
    case eNewick:
        return TestFormatNewick( mode );
    case eAlignment:
        return TestFormatAlignment( mode );
    case eDistanceMatrix:
        return TestFormatDistanceMatrix( mode );
    case eFlatFileSequence:
        return TestFormatFlatFileSequence( mode );
    case eFiveColFeatureTable:
        return TestFormatFiveColFeatureTable( mode );
    case eSnpMarkers:
        return TestFormatSnpMarkers( mode );
    case eFasta:
        return TestFormatFasta( mode );
    case eTextASN:
        return TestFormatTextAsn( mode );
    case eTaxplot:
        return TestFormatTaxplot( mode );
    case ePhrapAce:
        return TestFormatPhrapAce( mode );
    case eTable:
        return TestFormatTable( mode );
    case eHgvs:
        return TestFormatHgvs( mode );
    case eZip:
        return TestFormatZip( mode );
    case eGZip:
        return TestFormatGZip( mode );
    case eBZip2:
        return TestFormatBZip2( mode );
    case eLzo:
        return TestFormatLzo( mode );
    case eSra:
        return TestFormatSra( mode );
    case eBam:
        return TestFormatBam( mode );
    case eVcf:
        return TestFormatVcf( mode );
    default:
        NCBI_THROW( CCoreException, eInvalidArg,
            "CFormatGuess::x_TestFormat(): Unsupported format ID." );
    }
}

//  ----------------------------------------------------------------------------
void
CFormatGuess::Initialize()
{
    NCBI_ASSERT(eFormat_max-2 == sizeof( s_CheckOrder ) / sizeof( int ),
        "Indices in s_CheckOrder do not match format count ---"
        "update s_CheckOrder to list all formats" 
    );
    NCBI_ASSERT(eFormat_max == sizeof(sm_FormatNames) / sizeof(const char*)
                &&  sm_FormatNames[eFormat_max - 1] != NULL,
                "sm_FormatNames does not list all possible formats");
    m_pTestBuffer = 0;

    m_bStatsAreValid = false;
    m_bSplitDone = false;
    m_iStatsCountData = 0;
    m_iStatsCountAlNumChars = 0;
    m_iStatsCountDnaChars = 0;
    m_iStatsCountAaChars = 0;
}

//  ----------------------------------------------------------------------------
bool
CFormatGuess::EnsureTestBuffer()
{
    if ( m_pTestBuffer ) {
        return true;
    }
    if ( ! m_Stream.good() ) {
        return false;
    }

    // Fix to the all-comment problem.
    // Read a test buffer,
    // Test it for being all comment
    // If its all comment, read a twice as long buffer
    // Stop when its no longer all comment, end of the stream,
    //   or Multiplier hits 1024 
    int Multiplier = 1;
    while(true) {
        m_pTestBuffer = new char[ Multiplier * s_iTestBufferSize ];
        m_Stream.read( m_pTestBuffer, Multiplier * s_iTestBufferSize );
        m_iTestDataSize = m_Stream.gcount();
        m_Stream.clear();  // in case we reached eof
        CStreamUtils::Stepback( m_Stream, m_pTestBuffer, m_iTestDataSize );
        
        if (IsAllComment()) {
            Multiplier *= 2;
            delete [] m_pTestBuffer;
            m_pTestBuffer = NULL;
            if (Multiplier >= 1024 || m_iTestDataSize < ((Multiplier/2) * s_iTestBufferSize) )  {
                return false;
            }
            continue;
        } else {
            break;
        }
    }

    return true;
}

//  ----------------------------------------------------------------------------
bool
CFormatGuess::EnsureStats()
{
    if ( m_bStatsAreValid ) {
        return true;
    }
    if ( ! EnsureTestBuffer() ) {
        return false;
    }
    if ( m_iTestDataSize == 0 ) {
        m_bStatsAreValid = true;
        return true;
    }

    CNcbiIstrstream TestBuffer(
        reinterpret_cast<const char*>( m_pTestBuffer ), m_iTestDataSize );
    string strLine;

    init_symbol_type_table();
    // Things we keep track of:
    //   m_iStatsCountAlNumChars: number of characters that are letters or
    //     digits
    //   m_iStatsCountData: number of characters not part of a line starting
    //     with '>', ignoring whitespace
    //   m_iStatsCountDnaChars: number of characters counted in m_iStatsCountData
    //     from the DNA alphabet
    //   m_iStatsCountAaChars: number of characters counted in m_iStatsCountData
    //     from the AA alphabet
    //
    while ( ! TestBuffer.fail() ) {
        NcbiGetlineEOL( TestBuffer, strLine );
// code in CFormatGuess::Format counts line ends
// so, we will count them here as well
        if (!strLine.empty()) {
            strLine += '\n';
        }
        size_t size = strLine.size();
        bool is_header = size > 0 && strLine[0] == '>';
        for ( size_t i=0; i < size; ++i ) {
            unsigned char c = strLine[i];
            unsigned char type = symbol_type_table[c];

            if ( type & (fAlpha | fDigit | fSpace) ) {
                ++m_iStatsCountAlNumChars;
            }
            if ( !is_header ) {
                if ( !(type & fSpace) ) {
                    ++m_iStatsCountData;
                }

                if ( type & fDNA_Main_Alphabet ) {
                    ++m_iStatsCountDnaChars;
                }
                if ( type & fProtein_Alphabet ) {
                    ++m_iStatsCountAaChars;
                }
                if ( type & fLineEnd ) {
                    ++m_iStatsCountAlNumChars;
                    --m_iStatsCountData;
                }
            }
        }
    }
    m_bStatsAreValid = true;
    return true;
}

//  ----------------------------------------------------------------------------
bool CFormatGuess::x_TestInput( CNcbiIstream& input, EOnError onerror )
{
    if (!input) {
        if (onerror == eThrowOnBadSource) {
            NCBI_THROW(CUtilException,eNoInput,"Unreadable input stream");
        }
        return false;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool
CFormatGuess::TestFormatRepeatMasker(
    EMode /* not used */ )
{
    if ( ! EnsureStats() || ! EnsureSplitLines() ) {
        return false;
    }
    return IsInputRepeatMaskerWithHeader() ||
        IsInputRepeatMaskerWithoutHeader();
}

//  ----------------------------------------------------------------------------
bool
CFormatGuess::TestFormatPhrapAce(
    EMode /* not used */ )
{
    if ( ! EnsureTestBuffer() || ! EnsureSplitLines() ) {
        return false;
    }

    ITERATE( list<string>, it, m_TestLines ) {
        if ( IsLinePhrapId( *it ) ) {
            return true;
        }
    }
    return false;
}

//  -----------------------------------------------------------------------------
bool
CFormatGuess::TestFormatGtf(
    EMode /* not used */ )
{
    if ( ! EnsureTestBuffer() || ! EnsureSplitLines() ) {
        return false;
    }

    unsigned int uGtfLineCount = 0;
    list<string>::iterator it = m_TestLines.begin();

    for ( ;  it != m_TestLines.end();  ++it) {
        //
        //  Make sure to ignore any UCSC track and browser lines prior to the
        //  start of data
        //
        if ( it->empty() || (*it)[0] == '#' ) {
            continue;
        }
        if ( !uGtfLineCount && NStr::StartsWith( *it, "browser " ) ) {
            continue;
        }
        if ( !uGtfLineCount && NStr::StartsWith( *it, "track " ) ) {
            continue;
        }
        if ( ! IsLineGtf( *it ) ) {
            return false;
        }
        ++uGtfLineCount;
    }
    return (uGtfLineCount != 0);
}

//  -----------------------------------------------------------------------------
bool
CFormatGuess::TestFormatGvf(
    EMode /* not used */ )
{
    if ( ! EnsureTestBuffer() || ! EnsureSplitLines() ) {
        return false;
    }

    unsigned int uGvfLineCount = 0;
    list<string>::iterator it = m_TestLines.begin();

    for ( ;  it != m_TestLines.end();  ++it) {
        //
        //  Make sure to ignore any UCSC track and browser lines prior to the
        //  start of data
        //
        if ( it->empty() || (*it)[0] == '#' ) {
            if (NStr::StartsWith(*it, "##gvf-version")) {
                return true;
            }
			continue;
		}
		if ( !uGvfLineCount && NStr::StartsWith( *it, "browser " ) ) {
            continue;
        }
        if ( !uGvfLineCount && NStr::StartsWith( *it, "track " ) ) {
            continue;
        }
        if ( ! IsLineGvf( *it ) ) {
            return false;
        }
        ++uGvfLineCount;
    }
    return (uGvfLineCount != 0);
}


//  -----------------------------------------------------------------------------
bool
CFormatGuess::TestFormatGff3(
    EMode /* not used */ )
{
    if ( ! EnsureTestBuffer() || ! EnsureSplitLines() ) {
        return false;
    }

    unsigned int uGffLineCount = 0;
    list<string>::iterator it = m_TestLines.begin();

    for ( ;  it != m_TestLines.end();  ++it) {
        //
        //  Make sure to ignore any UCSC track and browser lines prior to the
        //  start of data
        //
        if ( it->empty() || (*it)[0] == '#' ) {
            continue;
        }
        if ( !uGffLineCount && NStr::StartsWith( *it, "browser " ) ) {
            continue;
        }
        if ( !uGffLineCount && NStr::StartsWith( *it, "track " ) ) {
            continue;
        }
        if ( ! IsLineGff3( *it ) ) {
            return false;
        }
        ++uGffLineCount;
    }
    return (uGffLineCount != 0);
}


//  -----------------------------------------------------------------------------
bool
CFormatGuess::TestFormatGff2(
    EMode /* not used */ )
{
    if ( ! EnsureTestBuffer() || ! EnsureSplitLines() ) {
        return false;
    }

    unsigned int uGffLineCount = 0;
    list<string>::iterator it = m_TestLines.begin();

    for ( ;  it != m_TestLines.end();  ++it) {
        //
        //  Make sure to ignore any UCSC track and browser lines prior to the
        //  start of data
        //
        if ( it->empty() || (*it)[0] == '#' ) {
            continue;
        }
        if ( !uGffLineCount && NStr::StartsWith( *it, "browser " ) ) {
            continue;
        }
        if ( !uGffLineCount && NStr::StartsWith( *it, "track " ) ) {
            continue;
        }
        if ( ! IsLineGff2( *it ) ) {
            return false;
        }
        ++uGffLineCount;
    }
    return (uGffLineCount != 0);
}


//  -----------------------------------------------------------------------------
bool
CFormatGuess::TestFormatGlimmer3(
    EMode /* not used */ )
{
    if ( ! EnsureTestBuffer() || ! EnsureSplitLines() ) {
        return false;
    }

    /// first line should be a FASTA defline
    list<string>::iterator it = m_TestLines.begin();
    if (it->empty()  ||  (*it)[0] != '>') {
        return false;
    }
    
    /// there should be additional data lines, and they should be easily parseable, 
    ///  with five columns
    ++it;
    if (it == m_TestLines.end()) {
        return false;
    }
    for ( /**/;  it != m_TestLines.end();  ++it) {
        if ( !IsLineGlimmer3( *it ) ) {
            return false;
        }
    }
    return true;
}

//  -----------------------------------------------------------------------------
bool
CFormatGuess::TestFormatAgp(
    EMode /* not used */ )
{
    if ( ! EnsureTestBuffer() || ! EnsureSplitLines() ) {
        return false;
    }
    ITERATE( list<string>, it, m_TestLines ) {
        if ( !IsLineAgp( *it ) ) {
            return false;
        }
    }
    return true;
}

//  -----------------------------------------------------------------------------
bool
CFormatGuess::TestFormatNewick(
    EMode /* not used */ )
{
//  -----------------------------------------------------------------------------
    // newick trees can be found in nexus files. check for that first as a special case
    if ( ! EnsureTestBuffer() || ! EnsureSplitLines() ) {
        return false;
    }

    // Alignment files come in all different shapes and broken formats,
    // and some of them are hard to recognize as such, in particular
    // if they have been hacked up in a text editor.

    // This functions only concerns itself with the ones that are
    // easy to recognize.

    // Note: We can live with false negatives. Avoid false positives
    // at all cost.

    bool is_nexus = false;
    bool has_trees = false;
    ITERATE( list<string>, it, m_TestLines ) {
        if ( NPOS != it->find( "#NEXUS" ) ) {
            is_nexus = true;
        }

        // case insensitive
        if (NPOS != NStr::FindNoCase(*it, "begin trees;")) { 
            has_trees = true;
        }
    }

    // In a nexus file with a tree, we will just read in the tree (ignoring for now
    // the alignment)
    if (is_nexus ) {
        if (has_trees)
            return true;
        return false;
    }

    //  special newick consideration:
    //  newick files may come with all data cramped into a single run-on line,
    //  that single oversized line may not have a line terminator
    const size_t maxSampleSize = 8*1024-1;
    size_t sampleSize = 0;
    char* pSample = new char[maxSampleSize+1];
    AutoArray<char> autoDelete(pSample);

    m_Stream.read(pSample, maxSampleSize);
    sampleSize = (size_t)m_Stream.gcount();
    m_Stream.clear();  // in case we reached eof
    CStreamUtils::Stepback(m_Stream, pSample, sampleSize);
    if (0 == sampleSize) {
        return false;
    }

    pSample[sampleSize] = 0;
    if (!IsSampleNewick(pSample)) { // tolerant of embedded line breaks
        return false;
    }
    return true;
}

//  -----------------------------------------------------------------------------
bool
CFormatGuess::TestFormatBinaryAsn(
    EMode /* not used */ )
{
    if ( ! EnsureTestBuffer() ) {
        return false;
    }

    //
    //  Criterion: Presence of any non-printing characters
    //
    EConfidence conf = eNo;
    for (int i = 0;  i < m_iTestDataSize;  ++i) {
        if ( !isgraph((unsigned char) m_pTestBuffer[i])  &&
             !isspace((unsigned char) m_pTestBuffer[i]) )
        {
            if (m_pTestBuffer[i] == '\1') {
                conf = eMaybe;
            } else {
                return true;
            }
        }
    }
    return (conf == eYes);
}


//  -----------------------------------------------------------------------------
bool
CFormatGuess::TestFormatDistanceMatrix(
    EMode /* not used */ )
{
    if ( ! EnsureTestBuffer() || ! EnsureSplitLines() ) {
        return false;
    }

    //
    // criteria are odd:
    //
    list<string>::const_iterator iter = m_TestLines.begin();
    list<string> toks;

    /// first line: one token, one number
    NStr::Split(*iter++, "\t ", toks);
    if (toks.size() != 1  ||
        toks.front().find_first_not_of("0123456789") != string::npos) {
        return false;
    }

    // now, for remaining ones, we expect an alphanumeric item first,
    // followed by a set of floating-point values.  Unless we are at the last
    // line, the number of values should increase monotonically
    for (size_t i = 1;  iter != m_TestLines.end();  ++i, ++iter) {
        toks.clear();
        NStr::Split(*iter, "\t ", toks);
        if (toks.size() != i) {
            /// we can ignore the last line ; it may be truncated
            list<string>::const_iterator it = iter;
            ++it;
            if (it != m_TestLines.end()) {
                return false;
            }
        }

        list<string>::const_iterator it = toks.begin();
        for (++it;  it != toks.end();  ++it) {
            if ( ! s_IsTokenDouble( *it ) ) {
                return false;
            }
        }
    }

    return true;
}

//  -----------------------------------------------------------------------------
bool
CFormatGuess::TestFormatFlatFileSequence(
    EMode /* not used */ )
{
    if ( ! EnsureTestBuffer() || ! EnsureSplitLines() ) {
        return false;
    }

    ITERATE (list<string>, it, m_TestLines) {
        if ( !IsLineFlatFileSequence( *it ) ) {
            return false;
        }
    }
    return true;
}

//  -----------------------------------------------------------------------------
bool
CFormatGuess::TestFormatFiveColFeatureTable(
    EMode /* not used */ )
{
    if ( ! EnsureTestBuffer() || ! EnsureSplitLines() ) {
        return false;
    }

    ITERATE( list<string>, it, m_TestLines ) {
        if (it->empty()) {
            continue;
        }

        if (it->find(">Feature ") != 0) {
            return false;
        }
        if (it->find_first_of(" \t", 9) != string::npos) {
            return false;
        }
        break;
    }

    return true;
}

//  -----------------------------------------------------------------------------
bool
CFormatGuess::TestFormatXml(
    EMode /* not used */ )
{
    if ( ! EnsureTestBuffer() ) {
        return false;
    }

    string input( m_pTestBuffer, (size_t)m_iTestDataSize );
    NStr::TruncateSpacesInPlace( input, NStr::eTrunc_Begin );

    //
    //  Test 1: If it starts with typical XML decorations such as "<?xml..."
    //  then respect that:
    //
    if ( NStr::StartsWith( input, "<?XML", NStr::eNocase ) ) {
        return true;
    }
    if ( NStr::StartsWith( input, "<!DOCTYPE", NStr::eNocase ) ) {
        return true;
    }

    //
    //  Test 2: In the absence of XML specific declarations, check whether the
    //  input starts with the opening tag of a well known set of doc types:
    //
    static const char* known_types[] = {
        "<Blast4-request>"
    };
    const int num_types = sizeof( known_types ) / sizeof( const char* );

    for ( int i=0; i < num_types; ++i ) {
        if ( NStr::StartsWith( input, known_types[i], NStr::eCase ) ) {
            return true;
        }
    }

    return false;
}

//  -----------------------------------------------------------------------------
bool
CFormatGuess::TestFormatAlignment(
    EMode /* not used */ )
{
    if ( ! EnsureTestBuffer() || ! EnsureSplitLines() ) {
        return false;
    }

    // Alignment files come in all different shapes and broken formats,
    // and some of them are hard to recognize as such, in particular
    // if they have been hacked up in a text editor.

    // This functions only concerns itself with the ones that are
    // easy to recognize.

    // Note: We can live with false negatives. Avoid false positives
    // at all cost.

    ITERATE( list<string>, it, m_TestLines ) {
        if ( NPOS != it->find( "#NEXUS" ) ) {
            return true;
        }
        if ( NPOS != it->find( "CLUSTAL" ) ) {
            return true;
        }
    }
    return false;
}

//  -----------------------------------------------------------------------------
 bool 
 CFormatGuess::x_TestTableDelimiter(const string& delims)
 {
    list<string>::const_iterator iter = m_TestLines.begin();
    list<string> toks;

    // Skip initial lines since not all headers start with comments like # or ;:
    // Don't skip though if file is very short - add up to 3, 1 for each line 
    // over 5:
    for (size_t i=5; i<7; ++i)
        if (m_TestLines.size() > i) ++iter;

    /// determine the number of observed columns
    size_t ncols = 0;
    bool found = false;
    for ( ;  iter != m_TestLines.end()  &&  ! found;  ++iter) {
        if (iter->empty()  ||  (*iter)[0] == '#'  ||  (*iter)[0] == ';') {
            continue;
        }

        toks.clear();
        NStr::Split(*iter, delims, toks);
        ncols = toks.size();
        found = true;
    }
    if ( ncols < 2 ) {
        return false;
    }

    size_t nlines = 1;
    // verify that columns all have the same size
    // we can add an exception for the last line
    for ( ;  iter != m_TestLines.end();  ++iter) {
        if (iter->empty()  ||  (*iter)[0] == '#'  ||  (*iter)[0] == ';') {
            continue;
        } 

        toks.clear();
        NStr::Split(*iter, delims, toks);
        if (toks.size() != ncols) {
            list<string>::const_iterator it = iter;
            ++it;
            if (it != m_TestLines.end() || (m_iTestDataSize < s_iTestBufferSize) ) {
                return false;
            }
        } else {
            ++nlines;
        }
    }
    return ( nlines >= 2 );
 }

bool
CFormatGuess::TestFormatTable(
    EMode /* not used */ )
{
    if ( ! EnsureTestBuffer() || ! EnsureSplitLines() ) {
        return false;
    }

    //
    //  NOTE 1:
    //  There is a bunch of file formats that are a special type of table and
    //  that we want to identify (like Repeat Masker output). So not to shade
    //  out those more special formats, this test should be performed only after
    //  all the more specialized table formats have been tested.
    //

    //
    //  NOTE 2:
    //  The original criterion for this test was "the same number of observed
    //  columns in every line".
    //  In order to weed out false positives the following *additional*
    //  conditions have been imposed:
    //  - there are at least two observed columns
    //  - the sample contains at least two non-comment lines.
    //

    //' ' ' \t' '\t' ',' '|'
    if (x_TestTableDelimiter(" "))
        return true;
    else if (x_TestTableDelimiter(" \t"))
        return true;
    else if (x_TestTableDelimiter("\t"))
        return true;
    else if (x_TestTableDelimiter(","))
        return true;
    else if (x_TestTableDelimiter("|"))
        return true;

    return false;
}

//  -----------------------------------------------------------------------------
bool
CFormatGuess::TestFormatFasta(
    EMode /* not used */ )
{
    if ( ! EnsureStats() ) {
        return false;
    }

    // reject obvious misfits:
    if ( m_iTestDataSize == 0 || m_pTestBuffer[0] != '>' ) {
        return false;
    }
    if ( m_iStatsCountData == 0 ) {
        if (0.75 > double(m_iStatsCountAlNumChars)/double(m_iTestDataSize) ) {
            return false;
        }
        return ( NStr::Find( m_pTestBuffer, "|" ) <= 10 );
    }

    // remaining decision based on text stats:
    double dAlNumFraction =  (double)m_iStatsCountAlNumChars / m_iTestDataSize;
    double dDnaFraction = (double)m_iStatsCountDnaChars / m_iStatsCountData;
    double dAaFraction = (double)m_iStatsCountAaChars / m_iStatsCountData;

    // want at least 80% text-ish overall:
    if ( dAlNumFraction < 0.8 ) {
        return false;
    }

    // want more than 91 percent of either DNA content or AA content in what we
    // presume is data:
    if ( dDnaFraction > 0.91 || dAaFraction > 0.91 ) {
        return true;
    }
    return false;
}

//  ----------------------------------------------------------------------------
bool
CFormatGuess::TestFormatTextAsn(
    EMode /* not used */ )
{
    if ( ! EnsureStats() ) {
        return false;
    }

    // reject obvious misfits:
    if ( m_iTestDataSize == 0 || m_pTestBuffer[0] == '>' ) {
        return false;
    }

    // criteria:
    // at least 80% text-ish,
    // "::=" as the 2nd field of the first non-blank non comment line.
    //
    double dAlNumFraction =  (double)m_iStatsCountAlNumChars / m_iTestDataSize;
    if ( dAlNumFraction < 0.80 ) {
        return false;
    }

    CNcbiIstrstream TestBuffer(
        reinterpret_cast<const char*>( m_pTestBuffer ), m_iTestDataSize );
    string strLine;

    while ( ! TestBuffer.fail() ) {
        vector<string> Fields;
        NcbiGetline( TestBuffer, strLine, "\n\r" );
        NStr::Tokenize( strLine, " \t", Fields, NStr::eMergeDelims );
        if ( IsAsnComment( Fields  ) ) {
            continue;
        }
        return ( Fields.size() >= 2 && Fields[1] == "::=" );
    }
    return false;
}

//  -----------------------------------------------------------------------------
bool
CFormatGuess::TestFormatTaxplot(
    EMode /* not used */ )
{
    return false;
}

//  -----------------------------------------------------------------------------
bool
CFormatGuess::TestFormatSnpMarkers(
    EMode /* not used */ )
{
    if ( ! EnsureTestBuffer() || ! EnsureSplitLines() ) {
        return false;
    }
    ITERATE( list<string>, it, m_TestLines ) {
        string str = *it;
        int rsid, chr, pos, numMatched;
        numMatched = sscanf( it->c_str(), "rs%d\t%d\t%d", &rsid, &chr, &pos);
        if ( numMatched == 3) {
            return true;
        }
    }
    return false;  
}


//  ----------------------------------------------------------------------------
bool
CFormatGuess::TestFormatBed(
    EMode /* not used */ )
{
    if ( ! EnsureStats() || ! EnsureSplitLines() ) {
        return false;
    }

    bool bTrackLineFound( false );    
	bool bHasStartAndStop ( false );
    size_t columncount = 0;
    ITERATE( list<string>, it, m_TestLines ) {
        string str = NStr::TruncateSpaces( *it );
        if ( str.empty() ) {
            continue;
        }
		
		// 'chr 8' fixup, the bedreader does this too
		if (str.find("chr ") == 0 || 
			str.find("Chr ") == 0 || 
			str.find("CHR ") == 0)
			str.erase(3, 1);

        //
        //  while occurrence of the following decorations _is_ a good sign, they could
        //  also be indicator for a variety of other UCSC data formats
        //
        if ( NStr::StartsWith( str, "track" ) ) {
            bTrackLineFound = true;
            continue;
        }
        if ( NStr::StartsWith( str, "browser" ) ) {
            continue;
        }
        if ( NStr::StartsWith( str, "#" ) ) {
            continue;
        }

        vector<string> columns;
        NStr::Tokenize( str, " \t", columns, NStr::eMergeDelims );
        if (columns.size() < 3 || columns.size() > 12) {
            return false;
        }
        if ( columns.size() != columncount ) {
            if ( columncount == 0 ) {
                columncount = columns.size();
            }
            else {
                return false;
            }
        }
		if(columns.size() >= 3) {
			if (s_IsTokenPosInt(columns[1]) &&
                s_IsTokenPosInt(columns[2])) {
				bHasStartAndStop = true;
			}
		}
    }

    return (bHasStartAndStop || bTrackLineFound);
}

//  ----------------------------------------------------------------------------
bool
CFormatGuess::TestFormatBed15(
    EMode /* not used */ )
{
    if ( ! EnsureStats() || ! EnsureSplitLines() ) {
        return false;
    }

    bool LineFound = false;
    size_t columncount = 15;
    ITERATE( list<string>, it, m_TestLines ) {
        if ( NStr::TruncateSpaces( *it ).empty() ) {
            continue;
        }
        //
        //  while occurrence of the following decorations _is_ a good sign, they could
        //  also be indicator for a variety of other UCSC data formats
        //
        if ( NStr::StartsWith( *it, "track" ) ) {
            continue;
        }
        if ( NStr::StartsWith( *it, "browser" ) ) {
            continue;
        }
        if ( NStr::StartsWith( *it, "#" ) ) {
            continue;
        }

        vector<string> columns;
        NStr::Tokenize( *it, " \t", columns, NStr::eMergeDelims );
        if ( columns.size() != columncount ) {
            return false;
        } else {
            if (!s_IsTokenPosInt(columns[1]) ||   //chr start
                !s_IsTokenPosInt(columns[2]) ||   //chr end
                !s_IsTokenPosInt(columns[4]) ||   //score
                !s_IsTokenPosInt(columns[6]) ||   //thick draw start
                !s_IsTokenPosInt(columns[7]))     //thick draw end
                    return false;
            string strand = NStr::TruncateSpaces(columns[5]);
            
            if (strand != "+" && strand != "-")
                return false;

            LineFound = true;
        }
    }
    return LineFound;
}

//  ----------------------------------------------------------------------------
bool
CFormatGuess::TestFormatWiggle(
    EMode /* not used */ )
{
    if ( ! EnsureStats() || ! EnsureSplitLines() ) {
        return false;
    }
    ITERATE( list<string>, it, m_TestLines ) {
        if ( NStr::StartsWith( *it, "track" ) ) {
            if ( NStr::Find( *it, "type=wiggle_0" ) != NPOS ) {
                return true;
            }
            if ( NStr::Find( *it, "type=bedGraph" ) != NPOS ) {
                return true;
            }
        }
        if ( NStr::StartsWith(*it, "fixedStep") ) { /* MSS-140 */
            if ( NStr::Find(*it, "chrom=")  &&  NStr::Find(*it, "start=") ) {
                return true;
            } 
        }
        if ( NStr::StartsWith(*it, "variableStep") ) { /* MSS-140 */
            if ( NStr::Find(*it, "chrom=") ) {
                return true;
            }
            return true;
        }
    }
    return false;
}

//  ----------------------------------------------------------------------------
bool
CFormatGuess::TestFormatHgvs(
    EMode /* not used */ )
{
    if ( ! EnsureTestBuffer() || ! EnsureSplitLines() ) {
        return false;
    }

    unsigned int uHgvsLineCount = 0;
    list<string>::iterator it = m_TestLines.begin();

    for ( ;  it != m_TestLines.end();  ++it) {
        if ( it->empty() || (*it)[0] == '#' ) {
            continue;
        }
        if ( ! IsLineHgvs( *it ) ) {
            return false;
        }
        ++uHgvsLineCount;
    }
    return (uHgvsLineCount != 0);
}


//  ----------------------------------------------------------------------------
bool
CFormatGuess::TestFormatZip(
    EMode /* not used */ )
{
    if ( ! EnsureTestBuffer() ) {
        return false;
    }

    // check if the first two bytes match with the zip magic number: 0x504B,
    // or BK and the next two bytes match with any of 0x0102, 0x0304, 0x0506
    // and 0x0708.
    if ( m_iTestDataSize < 4) {
        return false;
    }

    if (m_pTestBuffer[0] == 'P'  &&  m_pTestBuffer[1] == 'K'  &&
        ((m_pTestBuffer[2] == (char)1  &&  m_pTestBuffer[3] == (char)2)  ||
         (m_pTestBuffer[2] == (char)3  &&  m_pTestBuffer[3] == (char)4)  ||
         (m_pTestBuffer[2] == (char)5  &&  m_pTestBuffer[3] == (char)6) ||
         (m_pTestBuffer[2] == (char)7  &&  m_pTestBuffer[3] == (char)8) ) ) {
        return true;
    }

    return false;
}


//  ----------------------------------------------------------------------------
bool
CFormatGuess::TestFormatGZip(
    EMode /* not used */ )
{
    if ( ! EnsureTestBuffer() ) {
        return false;
    }

    // check if the first two bytes match the gzip magic number: 0x1F8B
    if ( m_iTestDataSize < 2) {
        return false;
    }

    if (m_pTestBuffer[0] == (char)31  &&  m_pTestBuffer[1] == (char)139) {
        return true;
    }

    return false;
}


//  ----------------------------------------------------------------------------
bool
CFormatGuess::TestFormatBZip2(
    EMode /* not used */ )
{
    if ( ! EnsureTestBuffer() ) {
        return false;
    }

    // check if the first two bytes match with the bzip2 magic number: 0x425A,
    // or 'BZ' and the next two bytes match with 0x68(h) and 0x31-39(1-9)
    if ( m_iTestDataSize < 4) {
        return false;
    }

    if (m_pTestBuffer[0] == 'B'  &&  m_pTestBuffer[1] == 'Z'  &&
        m_pTestBuffer[2] == 'h'  &&  m_pTestBuffer[3] >= '1'  &&
        m_pTestBuffer[3] <= '9') {
        return true;
    }

    return false;
}


//  ----------------------------------------------------------------------------
bool
CFormatGuess::TestFormatLzo(
    EMode /* not used */ )
{
    if ( ! EnsureTestBuffer() ) {
        return false;
    }

    if (m_iTestDataSize >= 3  &&  m_pTestBuffer[0] == 'L'  &&
        m_pTestBuffer[1] == 'Z'  &&  m_pTestBuffer[2] == 'O') {
        if (m_iTestDataSize == 3  ||
            (m_iTestDataSize > 3  &&  m_pTestBuffer[3] == '\0')) {
            return true;
        }
    }

    if (m_iTestDataSize >= 4  &&  m_pTestBuffer[1] == 'L'  &&
        m_pTestBuffer[2] == 'Z'  &&  m_pTestBuffer[3] == 'O') {
        if (m_iTestDataSize == 4  ||
            (m_iTestDataSize > 4  &&  m_pTestBuffer[4] == '\0')) {
            return true;
        }
    }

    return false;
}


bool CFormatGuess::TestFormatSra(EMode /* not used */ )
{
    if ( !EnsureTestBuffer()  ||  m_iTestDataSize < 16
        ||  CTempString(m_pTestBuffer, 8) != "NCBI.sra") {
        return false;
    }

    if (m_pTestBuffer[8] == '\x05'  &&  m_pTestBuffer[9] == '\x03'
        &&  m_pTestBuffer[10] == '\x19'  &&  m_pTestBuffer[11] == '\x88') {
        return true;
    } else if (m_pTestBuffer[8] == '\x88'  &&  m_pTestBuffer[9] == '\x19'
        &&  m_pTestBuffer[10] == '\x03'  &&  m_pTestBuffer[11] == '\x05') {
        return true;
    } else {
        return false;
    }
}

bool CFormatGuess::TestFormatBam(EMode mode)
{
    // Check for a gzip header whose first (only) extra field spans
    // at least six bytes and has the tag BC.
    return (TestFormatGZip(mode)  &&  m_iTestDataSize >= 18
            &&  (m_pTestBuffer[3] & 4) != 0 // extra field present
            &&  (static_cast<unsigned char>(m_pTestBuffer[10]) >= 6
                 ||  m_pTestBuffer[11] != 0) // at least six bytes
            &&  m_pTestBuffer[12] == 'B'  &&  m_pTestBuffer[13] == 'C');
}

//  ----------------------------------------------------------------------------
bool CFormatGuess::TestFormatVcf(
    EMode)
//  ----------------------------------------------------------------------------
{
    // Currently, only look for the header line identifying the VCF version.
    // Waive requirement this be the first line, but still expect it to by
    // in the initial sample.
    if ( ! EnsureStats() || ! EnsureSplitLines() ) {
        return false;
    }

    ITERATE( list<string>, it, m_TestLines ) {
        if (NStr::StartsWith(*it, "##fileformat=VCFv")) {
            return true;
        }
    }
    return false;
}

//  ----------------------------------------------------------------------------
bool CFormatGuess::IsInputRepeatMaskerWithHeader()
{
    //
    //  Repeatmasker files consist of columnar data with a couple of lines
    //  of column labels prepended to it (but sometimes someone strips those
    //  labels).
    //  This function tries to identify repeatmasker data by those column
    //  label lines. They should be the first non-blanks in the file.
    //
    string labels_1st_line[] = { "SW", "perc", "query", "position", "matching", "" };
    string labels_2nd_line[] = { "score", "div.", "del.", "ins.", "sequence", "" };

    //
    //  Purge junk lines:
    //
    list<string>::iterator it = m_TestLines.begin();
    for  ( ; it != m_TestLines.end(); ++it ) {
        NStr::TruncateSpacesInPlace( *it );
        if ( *it != "" ) {
            break;
        }
    }

    if ( it == m_TestLines.end() ) {
        return false;
    }

    //
    //  Verify first line of labels:
    //
    size_t current_offset = 0;
    for ( size_t i=0; labels_1st_line[i] != ""; ++i ) {
        current_offset = NStr::FindCase( *it, labels_1st_line[i], current_offset );
        if ( current_offset == NPOS ) {
            return false;
        }
    }

    //
    //  Verify second line of labels:
    //
    ++it;
    if ( it == m_TestLines.end() ) {
        return false;
    }
    current_offset = 0;
    for ( size_t j=0; labels_2nd_line[j] != ""; ++j ) {
        current_offset = NStr::FindCase( *it, labels_2nd_line[j], current_offset );
        if ( current_offset == NPOS ) {
            return false;
        }
    }

    //
    //  Should have at least one extra line:
    //
    ++it;
    if ( it == m_TestLines.end() ) {
        return false;
    }

    return true;
}


//  ----------------------------------------------------------------------------
bool CFormatGuess::IsInputRepeatMaskerWithoutHeader()
{
    //
    //  Repeatmasker files consist of columnar data with a couple of lines
    //  of column labels prepended to it (but sometimes someone strips those
    //  labels).
    //  This function assumes the column labels have been stripped and attempts
    //  to identify RMO by checking the data itself.
    //

    //
    //  We declare the data as RMO if we are able to parse every record in the
    //  sample we got:
    //
    ITERATE( list<string>, it, m_TestLines ) {
        string str = NStr::TruncateSpaces( *it );
        if ( str == "" ) {
            continue;
        }
        if ( ! IsLineRmo( str ) ) {
            return false;
        }
    }

    return true;
}


//  ----------------------------------------------------------------------------
bool
CFormatGuess::IsSampleNewick(
    const string& cline )
//  ----------------------------------------------------------------------------
{
    //  NOTE:
    //  See http://evolution.genetics.washington.edu/phylip/newick_doc.html
    //
    //  Note that Newick tree tend to be written out as a single long line. Thus,
    //  we are most likely only seeing the first part of a tree.
    //

    //  NOTE:
    //  MSS-112 introduced the concept of multitree files is which after the ";" 
    //  another tree may start. The new logic accepts files as Newick if they 
    //  are Newick up to and including the first semicolon. It does not look
    //  beyond.

    string line = NStr::TruncateSpaces( cline );
    if ( line.empty()  ||  line[0] != '(') {
        return false;
    }
    {{
        //  Strip out comments:
        string trimmed;
        bool in_comment = false;
        for ( size_t ii=0; line.c_str()[ii] != 0; ++ii ) {
            if ( ! in_comment ) {
                if ( line.c_str()[ii] != '[' ) {
                    trimmed += line.c_str()[ii];
                }
                else {
                    in_comment = true;
                }
            }
            else /* in_comment */ {
                if ( line.c_str()[ii] == ']' ) {
                    in_comment = false;
                }
            }
        }
        line = trimmed;
    }}
    {{
        //  Compress quoted labels:
        string trimmed;
        bool in_quote = false;
        for ( size_t ii=0; line.c_str()[ii] != 0; ++ii ) {
            if ( ! in_quote ) {
                if ( line.c_str()[ii] != '\'' ) {
                    trimmed += line.c_str()[ii];
                }
                else {
                    in_quote = true;
                    trimmed += 'A';
                }
            }
            else { /* in_quote */
                if ( line.c_str()[ii] == '\'' ) {
                    in_quote = false;
                }
            }
        }
        line = trimmed;
    }}
    {{
        //  Strip distance markers:
        string trimmed;
        size_t ii=0;
        while ( line.c_str()[ii] != 0 ) {
            if ( line.c_str()[ii] != ':' ) {
                trimmed += line.c_str()[ii++];
            }
            else {
                ii++;
                if ( line.c_str()[ii] == '-'  || line.c_str()[ii] == '+' ) {
                    ii++;
                }
                while ( '0' <= line.c_str()[ii] && line.c_str()[ii] <= '9' ) {
                    ii++;
                }
                if ( line.c_str()[ii] == '.' ) {
                    ii++;
                    while ( '0' <= line.c_str()[ii] && line.c_str()[ii] <= '9' ) {
                        ii++;
                    }
                }
            }
        }
        line = trimmed;
    }}
    {{
        //  Rough lexical analysis of what's left. Bail immediately on fault:
        if (line.empty()  ||  line[0] != '(') {
            return false;
        }
        size_t paren_count = 1;
        for ( size_t ii=1; line.c_str()[ii] != 0; ++ii ) {
            switch ( line.c_str()[ii] ) {
                default: 
                    break;
                case '(':
                    ++paren_count;
                    break;
                case ')':
                    if ( paren_count == 0 ) {
                        return false;
                    }
                    --paren_count;
                    break;
                case ',':
                    if ( paren_count == 0 ) {
                        return false;
                    }
                    break;
                case ';':
//                    if ( line[ii+1] != 0 ) {
//                        return false;
//                    }
                    break;
            }
        }
    }}
    return true; 
}


//  ----------------------------------------------------------------------------
bool CFormatGuess::IsLineFlatFileSequence(
    const string& line )
{
    // blocks of ten residues (or permitted punctuation characters)
    // with a count at the start or end; require at least four
    // (normally six)
    SIZE_TYPE pos = line.find_first_not_of("0123456789 \t");
    if (pos == NPOS  ||  pos + 45 >= line.size()) {
        return false;
    }

    for (SIZE_TYPE i = 0;  i < 45;  ++i) {
        char c = line[pos + i];
        if (i % 11 == 10) {
            if ( !isspace(c) ) {
                return false;
            }
        } else {
            if ( !isalpha(c)  &&  c != '-'  &&  c != '*') {
                return false;
            }
        }
    }

    return true;
}


//  ----------------------------------------------------------------------------
bool CFormatGuess::IsLabelNewick(
    const string& label )
{
    //  Starts with a string of anything other than "[]:", optionally followed by
    //  a single ':', followed by a number, optionally followed by a dot and
    //  another number.
    if ( NPOS != label.find_first_of( "[]" ) ) {
        return false;
    }
    size_t colon = label.find( ':' );
    if ( NPOS == colon ) {
        return true;
    }
    size_t dot = label.find_first_not_of( "0123456789", colon + 1 );
    if ( NPOS == dot ) {
        return true;
    }
    if ( label[ dot ] != '.' ) {
        return false;
    }
    size_t end = label.find_first_not_of( "0123456789", dot + 1 );
    return ( NPOS == end );
}


//  ----------------------------------------------------------------------------
bool CFormatGuess::IsLineAgp( 
    const string& strLine )
{
    //
    //  Note: The reader allows for line and endline comments starting with a '#'.
    //  So we accept them here, too.
    //
    string line( strLine );
    size_t uCommentStart = NStr::Find( line, "#" );

    if ( NPOS != uCommentStart ) {
        line = line.substr( 0, uCommentStart );
    }
    NStr::TruncateSpacesInPlace( line );
    if ( line.empty() ) {
        return true;
    }

    vector<string> tokens;
    if ( NStr::Tokenize( line, " \t", tokens, NStr::eMergeDelims ).size() < 8 ) {
        return false;
    }

    if ( tokens[1].size() > 1 && tokens[1][0] == '-' ) {
        tokens[1][0] = '1';
    }
    if ( -1 == NStr::StringToNonNegativeInt( tokens[1] ) ) {
        return false;
    }

    if ( tokens[2].size() > 1 && tokens[2][0] == '-' ) {
        tokens[2][0] = '1';
    }
    if ( -1 == NStr::StringToNonNegativeInt( tokens[2] ) ) {
        return false;
    }

    if ( tokens[3].size() > 1 && tokens[3][0] == '-' ) {
        tokens[3][0] = '1';
    }
    if ( -1 == NStr::StringToNonNegativeInt( tokens[3] ) ) {
        return false;
    }

    if ( tokens[4].size() != 1 || NPOS == tokens[4].find_first_of( "ADFGPNOW" ) ) {
        return false;
    }
    if ( tokens[4] == "N" ) {
        if ( -1 == NStr::StringToNonNegativeInt( tokens[5] ) ) {
            return false;
        }
    }
    else {
        if ( -1 == NStr::StringToNonNegativeInt( tokens[6] ) ) {
            return false;
        }
        if ( -1 == NStr::StringToNonNegativeInt( tokens[7] ) ) {
            return false;
        }            
        if ( tokens.size() != 9 ) {
            return false;
        }
        if ( tokens[8].size() != 1 || NPOS == tokens[8].find_first_of( "+-" ) ) {
            return false;
        }
    }

    return true;
}


//  ----------------------------------------------------------------------------
bool CFormatGuess::IsLineGlimmer3(
    const string& line )
{
    list<string> toks;
    NStr::Split(line, "\t ", toks);
    if (toks.size() != 5) {
        return false;
    }

    list<string>::iterator i = toks.begin();

    /// first column: skip (ascii identifier)
    ++i;

    /// second, third columns: both ints
    if ( ! s_IsTokenInteger( *i++ ) ) {
        return false;
    }
    if ( ! s_IsTokenInteger( *i++ ) ) {
        return false;
    }

    /// fourth column: int in the range of -3...3
    if ( ! s_IsTokenInteger( *i ) ) {
        return false;
    }
    int frame = NStr::StringToInt( *i++ );
    if (frame < -3  ||  frame > 3) {
        return false;
    }

    /// fifth column: score; double
    if ( ! s_IsTokenDouble( *i ) ) {
        return false;
    }

    return true;
}


//  ----------------------------------------------------------------------------
bool CFormatGuess::IsLineGtf(
    const string& line )
{
    vector<string> tokens;
    if ( NStr::Tokenize( line, " \t", tokens, NStr::eMergeDelims ).size() < 8 ) {
        return false;
    }
    if ( ! s_IsTokenPosInt( tokens[3] ) ) {
        return false;
    }
    if ( ! s_IsTokenPosInt( tokens[4] ) ) {
        return false;
    }
    if ( ! s_IsTokenDouble( tokens[5] ) ) {
        return false;
    }
    if ( tokens[6].size() != 1 || NPOS == tokens[6].find_first_of( ".+-" ) ) {
        return false;
    }
    if ( tokens[7].size() != 1 || NPOS == tokens[7].find_first_of( ".0123" ) ) {
        return false;
    }
    if ( tokens.size() < 9 || 
         (NPOS == tokens[8].find( "gene_id" ) && NPOS == tokens[8].find( "transcript_id" ) ) ) {
        return false;
    }
    return true;
}


//  ----------------------------------------------------------------------------
bool CFormatGuess::IsLineGvf(
    const string& line )
{
    vector<string> tokens;
    if ( NStr::Tokenize( line, " \t", tokens, NStr::eMergeDelims ).size() < 8 ) {
        return false;
    }
    if ( ! s_IsTokenPosInt( tokens[3] ) ) {
        return false;
    }
    if ( ! s_IsTokenPosInt( tokens[4] ) ) {
        return false;
    }
	{{
		list<string> terms;
		terms.push_back("snv");
		terms.push_back("cnv");
		terms.push_back("copy_number_variation");
		terms.push_back("gain");
		terms.push_back("copy_number_gain");
		terms.push_back("loss");
		terms.push_back("copy_number_loss");
		terms.push_back("loss_of_heterozygosity");
		terms.push_back("complex");
		terms.push_back("complex_substitution");
		terms.push_back("complex_sequence_alteration");
		terms.push_back("indel");
		terms.push_back("insertion");
		terms.push_back("inversion");
		terms.push_back("substitution");
		terms.push_back("deletion");
		terms.push_back("duplication");
		terms.push_back("translocation");
		terms.push_back("upd");
		terms.push_back("uniparental_disomy");
		terms.push_back("maternal_uniparental_disomy");
		terms.push_back("paternal_uniparental_disomy");
		terms.push_back("tandom_duplication");
		terms.push_back("structural_variation");
		terms.push_back("sequence_alteration");
		ITERATE(list<string>, termiter, terms) {
			if(NStr::EqualNocase(*termiter, tokens[2]))
				return true;
		}
	}}
	if ( ! s_IsTokenDouble( tokens[5] ) ) {
        return false;
    }
    if ( tokens[6].size() != 1 || NPOS == tokens[6].find_first_of( ".+-" ) ) {
        return false;
    }
    if ( tokens[7].size() != 1 || NPOS == tokens[7].find_first_of( ".0123" ) ) {
        return false;
    }
	if(tokens.size() >= 9) {
		list<string> terms;
		terms.push_back("start_range");
		terms.push_back("end_range");
		terms.push_back("variant_seq");
		terms.push_back("genotype");
		ITERATE(list<string>, termiter, terms) {
			if(NStr::EqualNocase(*termiter, tokens[8]))
				return true;
		}
	}

    return false;
}


//  ----------------------------------------------------------------------------
bool CFormatGuess::IsLineGff3(
    const string& line )
{
    vector<string> tokens;
    if ( NStr::Tokenize( line, " \t", tokens, NStr::eMergeDelims ).size() < 8 ) {
        return false;
    }
    if ( ! s_IsTokenPosInt( tokens[3] ) ) {
        return false;
    }
    if ( ! s_IsTokenPosInt( tokens[4] ) ) {
        return false;
    }
    if ( ! s_IsTokenDouble( tokens[5] ) ) {
        return false;
    }
    if ( tokens[6].size() != 1 || NPOS == tokens[6].find_first_of( ".+-" ) ) {
        return false;
    }
    if ( tokens[7].size() != 1 || NPOS == tokens[7].find_first_of( ".0123" ) ) {
        return false;
    }
    if ( tokens.size() < 9 || tokens[8].empty()) {
        return false;
    }
    if ( tokens.size() >= 9 && tokens[8].size() > 1) {
        const string& col9 = tokens[8];
        if ( NPOS == NStr::FindNoCase(col9, "ID") &&
             NPOS == NStr::FindNoCase(col9, "Parent") &&
             NPOS == NStr::FindNoCase(col9, "Target") &&
             NPOS == NStr::FindNoCase(col9, "Name") &&
             NPOS == NStr::FindNoCase(col9, "Alias") &&
             NPOS == NStr::FindNoCase(col9, "Note") &&
             NPOS == NStr::FindNoCase(col9, "Dbxref") &&
             NPOS == NStr::FindNoCase(col9, "Xref") ) {
            return false;
        }
    }

    return true;
}


//  ----------------------------------------------------------------------------
bool CFormatGuess::IsLineGff2(
    const string& line )
{
    vector<string> tokens;
    if ( NStr::Tokenize( line, " \t", tokens, NStr::eMergeDelims ).size() < 8 ) {
        return false;
    }
    if ( ! s_IsTokenPosInt( tokens[3] ) ) {
        return false;
    }
    if ( ! s_IsTokenPosInt( tokens[4] ) ) {
        return false;
    }
    if ( ! s_IsTokenDouble( tokens[5] ) ) {
        return false;
    }
    if ( tokens[6].size() != 1 || NPOS == tokens[6].find_first_of( ".+-" ) ) {
        return false;
    }
    if ( tokens[7].size() != 1 || NPOS == tokens[7].find_first_of( ".0123" ) ) {
        return false;
    }
    return true;
}


//  ----------------------------------------------------------------------------
bool CFormatGuess::IsLinePhrapId(
    const string& line )
{
    vector<string> values;
    if ( NStr::Tokenize( line, " \t", values, NStr::eMergeDelims ).empty() ) {
        return false;
    }

    //
    //  Old style: "^DNA \\w+ "
    //
    if ( values[0] == "DNA" ) {
        return true;
    }

    //
    //  New style: "^AS [0-9]+ [0-9]+"
    //
    if ( values[0] == "AS" ) {
        return ( 0 <= NStr::StringToNonNegativeInt( values[1] ) &&
          0 <= NStr::StringToNonNegativeInt( values[2] ) );
    }

    return false;
}


//  ----------------------------------------------------------------------------
bool CFormatGuess::IsLineRmo(
    const string& line )
{
    const size_t MIN_VALUES_PER_RECORD = 14;

    //
    //  Make sure there is enough stuff on that line:
    //
    list<string> values;
    if ( NStr::Split( line, " \t", values ).size() < MIN_VALUES_PER_RECORD ) {
        return false;
    }

    //
    //  Look at specific values and make sure they are of the correct type:
    //

    //  1: positive integer:
    list<string>::iterator it = values.begin();
    if ( ! s_IsTokenPosInt( *it ) ) {
        return false;
    }

    //  2: float:
    ++it;
    if ( ! s_IsTokenDouble( *it ) ) {
        return false;
    }

    //  3: float:
    ++it;
    if ( ! s_IsTokenDouble( *it ) ) {
        return false;
    }

    //  4: float:
    ++it;
    if ( ! s_IsTokenDouble( *it ) ) {
        return false;
    }

    //  5: string, not checked
    ++it;

    //  6: positive integer:
    ++it;
    if ( ! s_IsTokenPosInt( *it ) ) {
        return false;
    }

    //  7: positive integer:
    ++it;
    if ( ! s_IsTokenPosInt( *it ) ) {
        return false;
    }

    //  8: positive integer, likely in paretheses, not checked:
    ++it;

    //  9: '+' or 'C':
    ++it;
    if ( *it != "+" && *it != "C" ) {
        return false;
    }

    //  and that's enough for now. But there are at least two more fields 
    //  with values that look testable.

    return true;
}


//  ----------------------------------------------------------------------------
bool
CFormatGuess::IsAsnComment(
    const vector<string>& Fields )
{
    if ( Fields.size() == 0 ) {
        return true;
    }
    return ( NStr::StartsWith( Fields[0], "--" ) );
}

//  ----------------------------------------------------------------------------
bool
CFormatGuess::EnsureSplitLines()
//  ----------------------------------------------------------------------------
{
    if ( m_bSplitDone ) {
        return !m_TestLines.empty();
    }
    m_bSplitDone = true;

    //
    //  Make sure the given data is ASCII before checking potential line breaks:
    //
    const size_t MIN_HIGH_RATIO = 20;
    size_t high_count = 0;
    for ( streamsize i=0; i < m_iTestDataSize; ++i ) {
        if ( 0x80 & m_pTestBuffer[i] ) {
            ++high_count;
        }
    }
    if ( 0 < high_count && m_iTestDataSize / high_count < MIN_HIGH_RATIO ) {
        return false;
    }

    //
    //  Let's expect at least one line break in the given data:
    //
    string data( m_pTestBuffer, (size_t)m_iTestDataSize );
    m_TestLines.clear();

    if ( string::npos != data.find( "\r\n" ) ) {
        NStr::Split( data, "\r\n", m_TestLines );
    }
    else if ( string::npos != data.find( "\n" ) ) {
        NStr::Split( data, "\n", m_TestLines );
    }
    else if ( string::npos != data.find( "\r" ) ) {
        NStr::Split( data, "\r", m_TestLines );
    }
    else {
        //single truncated line
        return false;
    }

    if ( m_iTestDataSize == s_iTestBufferSize   &&  m_TestLines.size() > 1 ) {
        m_TestLines.pop_back();
    }
    return !m_TestLines.empty();
}

//  ----------------------------------------------------------------------------
bool
CFormatGuess::IsAsciiText()
{
    const double REQUIRED_ASCII_RATIO = 0.9;

    // first stab - are we text?  comments are only valid if we are text
    size_t count = 0;
    size_t count_print = 0;
    for (int i = 0;  i < m_iTestDataSize;  ++i, ++count) {
        if (isprint((unsigned char) m_pTestBuffer[i])) {
            ++count_print;
        }
    }
    if (count_print < count * REQUIRED_ASCII_RATIO) {
		return false;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool
CFormatGuess::IsAllComment()
{
    if (!IsAsciiText()) {
        return false;
    }

    m_bSplitDone = false;
    m_TestLines.clear();
    EnsureSplitLines();

    ITERATE(list<string>, it, m_TestLines) {
        if(it->empty()) {
            continue;
        }
        else if(NStr::StartsWith(*it, "#")) {
            continue;
        }
        else if(NStr::StartsWith(*it, "--")) {
            continue;
        }
        else {
            return false;
        }
    }
    
    return true;
}

//  ----------------------------------------------------------------------------
bool CFormatGuess::IsLineHgvs(
    const string& line )
{
    // This simple check can mistake Newwick, so Newwick is checked first
    //  /:(g|c|r|p|m|mt|n)\./  as in NC_000001.9:g.1234567C>T
    int State = 0;
    ITERATE(string, Iter, line) {
        char Char = *Iter;
        char Next = '\0';
        string::const_iterator NextI = Iter;
        ++NextI;
        if(NextI != line.end())
            Next = *NextI;
        
        if(State == 0) {
            if(Char == ':')
                State = 1;
        } else if(State == 1) {
            if (Char == 'g' ||
                Char == 'c' ||
                Char == 'r' ||
                Char == 'p' ||
                Char == 'n' ||
                Char == 'm' ) {
                State = 2;
                if (Char=='m' && Next == 't') {
                    ++Iter;
                }
            }
        } else if(State == 2) {
            if(Char == '.') 
                State = 3;
        }
    }
    
    return (State == 3);    
}



END_NCBI_SCOPE
