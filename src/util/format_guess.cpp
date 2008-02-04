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
#include <corelib/ncbifile.hpp>
#include <corelib/ncbistre.hpp>
#include <corelib/stream_utils.hpp>

BEGIN_NCBI_SCOPE

enum ESymbolType {
    fDNA_Alphabet       = 1<<0,
    fProtein_Alphabet   = 1<<1,
    fLineEnd            = 1<<2,
    fAlpha              = 1<<3,
    fDigit              = 1<<4,
    fSpace              = 1<<5,
    fInvalid            = 1<<6
};

enum EConfidence {
    eNo = 0,
    eMaybe,
    eYes
};


static unsigned char symbol_type_table[256];

static void init_symbol_type_table(void)
{
    if ( symbol_type_table[0] == 0 ) {
        for ( const char* s = "ATGCN"; *s; ++s ) {
            unsigned char c = *s;
            symbol_type_table[c] |= fDNA_Alphabet;
            c = tolower(c);
            symbol_type_table[c] |= fDNA_Alphabet;
        }
        for ( const char* s = "ACDEFGHIKLMNPQRSTVWYBZX"; *s; ++s ) {
            unsigned char c = *s;
            symbol_type_table[c] |= fProtein_Alphabet;
            c = tolower(c);
            symbol_type_table[c] |= fProtein_Alphabet;
        }
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


static bool x_SplitLines( const char* byte_buf, size_t byte_count,
                          list< string>& lines )
//
//  Note:   We are assuming that the data at hand is consistent in its line 
//          delimiters.
//          Thus, we should see '\n' but never '\r' for files created in a UNIX 
//           environment,
//          we should see '\r', but never '\n' for files coming from a Mac,
//          and we should see '\n' and '\r' only in the "\r\n" combination for 
//           Windows originated files.
//
{
    //
    //  Make sure the given data is ASCII before checking potential line breaks:
    //
    const size_t MIN_HIGH_RATIO = 20;
    size_t high_count = 0;
    for ( size_t i=0; i < byte_count; ++i ) {
        if ( 0x80 & byte_buf[i] ) {
            ++high_count;
        }
    }
    if ( 0 < high_count && byte_count / high_count < MIN_HIGH_RATIO ) {
        return false;
    }

    //
    //  Let's expect at least one line break in the given data:
    //
    string data( byte_buf, byte_count );
    
    lines.clear();
    if ( NStr::Split( data, "\r\n", lines ).size() > 1 ) {
        return true;
    }
    lines.clear();
    if ( NStr::Split( data, "\r", lines ).size() > 1 ) {
        return true;
    }
    lines.clear();
    if ( NStr::Split( data, "\n", lines ).size() > 1 ) {
        return true;
    }
    
    //
    //  Suspicious for non-binary files. Unfortunately, it can actually happen
    //  for Newick tree files.
    //
    lines.clear();
    lines.push_back( data );
    return true;
}


static bool x_IsLineRmo( const string& line ) 
{
    const size_t MIN_VALUES_PER_RECORD = 15;
    
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
    
    try {
        //  1: positive integer:
        list<string>::iterator it = values.begin();
        NStr::StringToUInt( *it );
    
        //  2: float:
        ++it;
        NStr::StringToDouble( *it );
    
        //  3: float:
        ++it;
        NStr::StringToDouble( *it );
    
        //  4: float:
        ++it;
        NStr::StringToDouble( *it );
        
        //  5: string, not checked
        ++it;
        
        //  6: positive integer:
        ++it;
        NStr::StringToUInt( *it );
        
        //  7: positive integer:
        ++it;
        NStr::StringToUInt( *it );
        
        //  8: positive integer, likely in paretheses, not checked:
        ++it;
        
        //  9: '+' or 'C':
        ++it;
        if ( *it != "+" && *it != "C" ) {
            return false;
        }
        
        //  and that's enough for now. But there are at least two more fields 
        //  with values that look testable.
        
    }
    catch ( ... ) {
        return false;
    }
        
    return true;
}


static bool x_IsInputRepeatMaskerWithHeader( list< string >& lines  )
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
    
    if ( lines.empty() ) {
        return false;
    }
    
    //
    //  Purge junk lines:
    //
    list<string>::iterator it = lines.begin();
    for  ( ; it != lines.end(); ++it ) {
        NStr::TruncateSpacesInPlace( *it );
        if ( *it != "" ) {
            break;
        }
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
    if ( it == lines.end() ) {
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
    if ( it == lines.end() ) {
        return false;
    }
            
    return true;
}


static bool x_IsInputRepeatMaskerWithoutHeader( list< string>& lines  )
{
    //
    //  Repeatmasker files consist of columnar data with a couple of lines
    //  of column labels prepended to it (but sometimes someone strips those
    //  labels).
    //  This function assumes the column labels have been stripped and attempts
    //  to identify RMO by checking the data itself.
    //
    
    if ( ! lines.empty() ) {
        //  the last line is probably incomplete. We won't even bother with it.
        lines.pop_back();
    }
    if ( lines.empty() ) {
        return false;
    }
    
    //
    //  We declare the data as RMO if we are able to parse every record in the 
    //  sample we got:
    //
    for ( list<string>::iterator it = lines.begin(); it != lines.end(); ++it ) {
        NStr::TruncateSpacesInPlace( *it );
        if ( *it == "" ) {
            continue;
        }
        if ( ! x_IsLineRmo( *it ) ) {
            return false;
        }
    }
    
    return true;
}


static bool x_IsInputRepeatMasker( const char* byte_buf, size_t byte_count ) 
{
    list<string> lines;
    if ( ! x_SplitLines( byte_buf, byte_count, lines ) ) {
        //  seemingly not even ASCII ...
        return false;
    }
    
    return x_IsInputRepeatMaskerWithHeader( lines ) || 
        x_IsInputRepeatMaskerWithoutHeader( lines  );
}



static bool x_IsLinePhrapId( const string& line ) 
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
        return ( 0 <= NStr::StringToNumeric( values[1] ) && 
          0 <= NStr::StringToNumeric( values[2] ) );
    }
    
    return false;
}


static bool x_IsInputPhrapAce( const char* byte_buf, size_t byte_count )
{
    list<string> lines;
    if ( ! x_SplitLines( byte_buf, byte_count, lines ) ) {
        //  seemingly not even ASCII ...
        return false;
    }
    
    ITERATE( list<string>, it, lines ) {
        if ( x_IsLinePhrapId( *it ) ) {
            return true;
        }
    }
    return false;
}


static bool x_IsLineGlimmer3(const string& line)
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
    try {
        NStr::StringToInt(*i++);
        NStr::StringToInt(*i++);
    }
    catch (...) {
        return false;
    }

    /// fourth column: int in the range of -3...3
    try {
        int frame = NStr::StringToInt(*i++);
        if (frame < -3  ||  frame > 3) {
            return false;
        }
    }
    catch (...) {
        return false;
    }

    /// fifth column: score; double
    try {
        NStr::StringToDouble(*i);
    }
    catch (...) {
    }

    return true;
}


static bool x_IsInputGlimmer3( const char* byte_buf, size_t byte_count )
{
    list<string> lines;
    if ( ! x_SplitLines( byte_buf, byte_count, lines ) ) {
        //  seemingly not even ASCII ...
        return false;
    }

    /// first line should be a FASTA defline
    list<string>::iterator it = lines.begin();
    if (it->empty()  ||  (*it)[0] != '>') {
        return false;
    }

    /// next lines should be easily parseable, with five columns
    for (++it;  it != lines.end();  ++it) {
        if (x_IsLineGlimmer3(*it)) {
            return true;
        }
    }

    return false;
}


static bool x_IsLineGtf( const string& line )
{
    vector<string> tokens;
    if ( NStr::Tokenize( line, " \t", tokens, NStr::eMergeDelims ).size() < 8 ) {
        return false;
    }
    try {
        NStr::StringToInt( tokens[3] );
        NStr::StringToInt( tokens[4] );
        if ( tokens[5] != "." ) {
            NStr::StringToDouble( tokens[5] );
        }
    }
    catch( ... ) {
        return false;
    }
        
    if ( tokens[6] != "+" && tokens[6] != "." && tokens[6] != "-" ) {
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


static bool x_IsLineAgp( const string& line ) 
{
    vector<string> tokens;
    if ( NStr::Tokenize( line, " \t", tokens, NStr::eMergeDelims ).size() < 8 ) {
        return false;
    }
    try {
        NStr::StringToInt( tokens[1] );
        NStr::StringToInt( tokens[2] );
        NStr::StringToInt( tokens[3] );
        
        if ( tokens[4].size() != 1 || NPOS == tokens[4].find_first_of( "ADFGPNOW" ) ) {
            return false;
        }
        if ( tokens[4] == "N" ) {
            NStr::StringToInt( tokens[5] ); // gap length
        }
        else {
            NStr::StringToInt( tokens[6] ); // component start
            NStr::StringToInt( tokens[7] ); // component end
            
            if ( tokens.size() != 9 ) {
                return false;
            }
            if ( tokens[8].size() != 1 || NPOS == tokens[8].find_first_of( "+-" ) ) {
                return false;
            }
        }
    }
    catch( ... ) {
        return false;
    }
    
    return true;
}
    

static bool x_IsInputGtf( const char* byte_buf, size_t byte_count )
{
    list<string> lines;
    if ( ! x_SplitLines( byte_buf, byte_count, lines ) ) {
        //  seemingly not even ASCII ...
        return false;
    }
    if ( ! lines.empty() ) {
        //  the last line is probably incomplete. We won't even bother with it.
        lines.pop_back();
    }
    if ( lines.empty() ) {
        return false;
    }

    list<string>::iterator it = lines.begin();
    for ( ;  it != lines.end();  ++it) {
        if ( !it->empty()  &&  (*it)[0] != '#') {
            break;
        }
    }
    
    for ( ;  it != lines.end();  ++it) {
        if ( !x_IsLineGtf( *it ) ) {
            return false;
        }
    }
    return true;
}


static bool x_IsInputAgp( const char* byte_buf, size_t byte_count )
{
    list<string> lines;
    if ( ! x_SplitLines( byte_buf, byte_count, lines ) ) {
        //  seemingly not even ASCII ...
        return false;
    }
    if ( ! lines.empty() ) {
        //  the last line is probably incomplete. We won't even bother with it.
        lines.pop_back();
    }
    if ( lines.empty() ) {
        return false;
    }
        
    ITERATE( list<string>, it, lines ) {
        if ( ! x_IsLineAgp( *it ) ) {
            return false;
        }
    }
    return true;
}


static bool x_IsLabelNewick( const string& label )
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


static bool x_IsLineNewick( const string& cline )
{
    //
    //  Note:
    //  Newick lines are a little tricky. They contain tree structure of the form
    //  (a,b), where each a or be can either be a another tree structure, or a
    //  label of the form 'ABCD'. The trickiness comes from the fact that these 
    //  beasts are highly recursive, to the point that our 1k read buffer may not
    //  even cover a single line in the file. Which means, we might only have a
    //  partial line to work with.
    //
    //  The test:
    //  Throw away all the labels, i.e. everything between an odd-numbered ' and
    //  an even numbered tick. After that, there should only remain '(', ')', ';', 
    //  ''', ',', or whitespace.
    //  Moreover, if there is a semicolon, it must be at the end of the line. 
    //
    string line = NStr::TruncateSpaces( cline );
    if ( line.empty() ) {
        return false;
    }
    string delimiters = " ,();";
    for ( size_t i=0; line[i] != 0; ++i ) {
    
        if ( NPOS != delimiters.find( line[i] ) ) {
            if ( line[i] == ';' && i != line.size() - 1 ) {
                return false;
            }
            else {
                continue;
            }
        }
        if ( line[i] == '[' || line[i] == ']' ) {
            return false;
        }
        size_t label_end = line.find_first_of( delimiters, i );
        string label = line.substr( i, label_end - i );
        if ( ! x_IsLabelNewick( label ) ) {
            return false;
        }
        if ( NPOS == label_end ) {
            return true;
        }
        i = label_end;
    }    
    return true;
}


static bool x_IsInputNewick( const char* byte_buf, size_t byte_count )
{
    // Maybe we get home early ...
    if ( byte_count > 0 && byte_buf[0] != '(' ) {
        return false;
    }
    
    list<string> lines;
    if ( ! x_SplitLines( byte_buf, byte_count, lines ) ) {
        //  seemingly not even ASCII ...
        return false;
    }
    if ( lines.empty() ) {
        return false;
    }
    
    string one_line;    
    ITERATE( list<string>, it, lines ) {
        one_line += *it;
    }
    
    if ( ! x_IsLineNewick( one_line ) ) {
        return false;
    }
    return true;
}


static bool x_IsInputAlignment( const char* byte_buf, size_t byte_count )
{
    // Alignment files come in all different shapes and broken formats,
    // and some of them are hard to recognize as such, in particular
    // if they have been hacked up in a text editor.
    
    // This functions only concerns itself with the ones that are
    // easy to recognize.
    
    // Note: We can live with false negatives. Avoid false positives
    // at all cost.
    
    list<string> lines;
    if ( ! x_SplitLines( byte_buf, byte_count, lines ) ) {
        //  seemingly not even ASCII ...
        return false;
    }
    if ( lines.empty() ) {
        return false;
    }
    
    ITERATE( list<string>, it, lines ) {
        if ( NPOS != it->find( "#NEXUS" ) ) {
            return true;
        }
        if ( NPOS != it->find( "CLUSTAL" ) ) {
            return true;
        }
    }
    return false;
}
        

static bool x_IsInputXml( const char* byte_buf, size_t byte_count )
{
    string input( byte_buf, byte_count );
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


static EConfidence x_IsInputBinaryAsn( const char* byte_buf, size_t byte_count )
{
    //
    //  Criterion: Presence of any non-printing characters
    //
    EConfidence conf = eNo;
    for (unsigned int i = 0;  i < byte_count;  ++i) {
        if ( !isgraph((unsigned char) byte_buf[i])  &&  
             !isspace((unsigned char) byte_buf[i]) ) 
        {
            if (byte_buf[i] == '\1') {
                conf = eMaybe;
            } else {
                return eYes;
            }
        }
    }
    return conf;
}


static bool x_IsInputDistanceMatrix(const char* byte_buf, size_t byte_count)
{
    // first split into a set of lines
    list<string> lines;
    if ( ! x_SplitLines( byte_buf, byte_count, lines ) ) {
        //  seemingly not even ASCII ...
        return false;
    }
    if ( lines.empty() ) {
        return false;
    }

    //
    // criteria are odd:
    //
    list<string>::const_iterator iter = lines.begin();
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
    for (size_t i = 1;  iter != lines.end();  ++i, ++iter) {
        toks.clear();
        NStr::Split(*iter, "\t ", toks);
        if (toks.size() != i) {
            /// we can ignore the last line ; it may be truncated
            list<string>::const_iterator it = iter;
            ++it;
            if (it != lines.end()) {
                return false;
            }
        }

        list<string>::const_iterator it = toks.begin();
        for (++it;  it != toks.end();  ++it) {
            try {
                NStr::StringToDouble(*it);
            }
            catch (CException&) {
                return false;
            }
        }
    }

    return true;
}

static bool x_IsInputTaxplot(const char* byte_buf, size_t byte_count)
{
    return false;
}


static bool x_IsLineFlatFileSequence(const string& line)
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

static bool x_IsInputFlatFileSequence(const char* byte_buf, size_t byte_count)
{
    list<string> lines;
    if ( !x_SplitLines(byte_buf, byte_count, lines) ) {
        //  seemingly not even ASCII ...
        return false;
    }
    if ( ! lines.empty() ) {
        //  the last line is probably incomplete. We won't even bother with it.
        lines.pop_back();
    }
    if (lines.empty()) {
        return false;
    }
        
    ITERATE (list<string>, it, lines) {
        if ( !x_IsLineFlatFileSequence(*it) ) {
            return false;
        }
    }
    return true;
}

static bool x_IsInputFiveColFeatureTable(const char* byte_buf,
                                         size_t byte_count)
{
    // first split into a set of lines
    list<string> lines;
    if ( ! x_SplitLines( byte_buf, byte_count, lines ) ) {
        //  seemingly not even ASCII ...
        return false;
    }
    if ( lines.empty() ) {
        return false;
    }

    // criteria:
    list<string>::const_iterator iter = lines.begin();

    // first non-empty line should look FASTA-ish and have 'Feature'
    // as the id, followed by a single token
    for ( ;  iter != lines.end();  ++iter) {
        if (iter->empty()) {
            continue;
        }

        if (iter->find(">Feature ") != 0) {
            return false;
        }
        if (iter->find_first_of(" \t", 9) != string::npos) {
            return false;
        }
        break;
    }

    return true;
}


static bool x_IsInputTable(const char* byte_buf, size_t byte_count)
{
    //  ------------------------------------------------------------------------
    //  NOTE 1:
    //  There is a bunch of file formats that are a special type of table and
    //  that we want to identify (like Repeat Masker output). So not to shade
    //  out those more special formats, this test should be performed only after
    //  all the more specialized table formats have been tested.
    //  ------------------------------------------------------------------------

    //  ------------------------------------------------------------------------
    //  NOTE 2:
    //  The original criterion for this test was "the same number of observed
    //  columns in every line".
    //  In order to weed out false positives the following *additional*
    //  conditions have been imposed:
    //  - there are at least two observed columns
    //  - the sample contains at least two non-comment lines.
    //  ------------------------------------------------------------------------

    // first split into a set of lines
    list<string> lines;
    if ( ! x_SplitLines( byte_buf, byte_count, lines ) ) {
        //  seemingly not even ASCII ...
        return false;
    }
    if ( lines.empty() ) {
        return false;
    }

    // criteria:
    list<string>::const_iterator iter = lines.begin();
    list<string> toks;

    /// determine the number of observed columns
    size_t ncols = 0;
    for ( ;  iter != lines.end();  ++iter) {
        if (iter->empty()  ||  (*iter)[0] == '#'  ||  (*iter)[0] == ';') {
            continue;
        }

        toks.clear();
        NStr::Split(*iter, " \t,", toks);
        ncols = toks.size();
        break;
    }
    if ( ncols < 2 ) {
        return false;
    }

    size_t nlines = 1;
    // verify that columns all have the same size
    // we can add an exception for the last line
    for ( ;  iter != lines.end();  ++iter) {
        if (iter->empty()  ||  (*iter)[0] == '#'  ||  (*iter)[0] == ';') {
            continue;
        }
        ++nlines;

        toks.clear();
        NStr::Split(*iter, " \t,", toks);
        if (toks.size() != ncols) {
            list<string>::const_iterator it = iter;
            ++it;
            if (it != lines.end()) {
                return false;
            }
        }
    }

    return ( nlines >= 2 );
}


CFormatGuess::ESequenceType 
CFormatGuess::SequenceType(const char* str, unsigned length)
{
    if (length == 0)
        length = (unsigned)::strlen(str);

    init_symbol_type_table();
    unsigned ATGC_content = 0;
    unsigned amino_acid_content = 0;

    for (unsigned i = 0; i < length; ++i) {
        unsigned char c = str[i];
        unsigned char type = symbol_type_table[c];
        if ( type & fDNA_Alphabet ) {
            ++ATGC_content;
        }
        if ( type & fProtein_Alphabet ) {
            ++amino_acid_content;
        }
    }

    double dna_content = (double)ATGC_content / (double)length;
    double prot_content = (double)amino_acid_content / (double)length;

    if (dna_content > 0.7) {
        return eNucleotide;
    }
    if (prot_content > 0.7) {
        return eProtein;
    }
    return eUndefined;
}


CFormatGuess::EFormat CFormatGuess::Format(const string& path)
{
    CNcbiIfstream input(path.c_str(), IOS_BASE::in | IOS_BASE::binary);
    if (!input.is_open()) {
        return eUnknown;
    }
    return Format(input);
}


CFormatGuess::EFormat 
CFormatGuess::Format(const unsigned char* buffer, 
                     size_t               buffer_size)
{
    if ( !buffer_size ) {
        return eUnknown;
    }
    EFormat format = eUnknown;
    EConfidence eBinAsn;

    const char* chbuf = (const char*)buffer;

    ///
    /// the order here is important!
    ///

    if ( x_IsInputRepeatMasker(chbuf, buffer_size) ) {
        return eRmo;
    }
    if ( x_IsInputPhrapAce(chbuf, buffer_size) ) {
        return ePhrapAce;
    }
    if ( x_IsInputGtf(chbuf, buffer_size) ) {
        return eGtf;
    }
    if ( x_IsInputGlimmer3(chbuf, buffer_size) ) {
        return eGlimmer3;
    }
    if ( x_IsInputAgp(chbuf, buffer_size) ) {
        return eAgp;
    }
    if ( x_IsInputNewick(chbuf, buffer_size) ) {
        return eNewick;
    }
    if ( x_IsInputXml(chbuf, buffer_size) ) {
        return eXml;
    }
    if ( x_IsInputAlignment(chbuf, buffer_size) ) {
        return eAlignment;
    }
    eBinAsn = x_IsInputBinaryAsn(chbuf, buffer_size);
    if ( eBinAsn == eYes ) {
        return eBinaryASN;
    }
    if ( x_IsInputDistanceMatrix(chbuf, buffer_size) ) {
        return eDistanceMatrix;
    }
    if ( x_IsInputTaxplot(chbuf, buffer_size) ) {
        return eTaxplot;
    }
    if ( x_IsInputFlatFileSequence(chbuf, buffer_size) ) {
        return eFlatFileSequence;
    }
    if ( x_IsInputFiveColFeatureTable(chbuf, buffer_size) ) {
        return eFiveColFeatureTable;
    }
    if ( x_IsInputTable(chbuf, buffer_size) ) {
        return eTable;
    }

    init_symbol_type_table();
    //
    //  The following is actually three tests rolled into one, based on symbol
    //  frequencies in the input sample. I am leaving them "as is".
    //
    unsigned int i = 0;
    unsigned ATGC_content = 0;
    unsigned amino_acid_content = 0;
    unsigned seq_length = (unsigned)buffer_size;
    unsigned txt_length = 0;

    unsigned alpha_content = 0;

    if (buffer[0] == '>') { // FASTA ?
        for (i = 0; i < buffer_size; ++i) {
            // skip the first line (presumed this is free-text information)
            unsigned char type = symbol_type_table[buffer[i]];
            if ( type & fLineEnd ) {
                break;
            }
            if ( type & (fAlpha | fDigit | fSpace) ) {
                ++alpha_content;
            }
        }
        seq_length = (unsigned)buffer_size - i;
        if (seq_length == 0) {
            return eUnknown;   // No way to tell what format is this...
        }
    }

    for (i = 0; i < buffer_size; ++i) {
        unsigned char type = symbol_type_table[buffer[i]];

        if ( type & (fAlpha | fDigit | fSpace) ) {
            ++alpha_content;
        }
        if ( type & fDNA_Alphabet ) {
            ++ATGC_content;
        }
        if ( type & fProtein_Alphabet ) {
            ++amino_acid_content;
        }
        if ( type & fLineEnd ) {
            ++alpha_content;
            --seq_length;
        }
    }

    double dna_content = (double)ATGC_content / (double)seq_length;
    double prot_content = (double)amino_acid_content / (double)seq_length;
    double a_content = (double)alpha_content / (double)buffer_size;
    if (buffer[0] == '>') {
        if (dna_content > 0.7 && a_content > 0.91) {
            return eFasta;  // DNA fasta file
        }
        if (prot_content > 0.7 && a_content > 0.91) {
            return eFasta;  // Protein fasta file
        }

        if (a_content > 0.8) {
            // Second FASTA hypothesis: short sequences - long descriptions
            // in this case buffer holds several small FASTA entries
            unsigned ATGC_content_txt = 0;
            unsigned amino_acid_content_txt = 0;

            ATGC_content = 0;
            amino_acid_content = 0;
            seq_length = txt_length = 0;

            CNcbiIstrstream istr(reinterpret_cast<const char *>(buffer), buffer_size);
            string line;
            while (!istr.fail()) {
                NcbiGetline(istr, line, "\n\r");
                if (line[0] == '>') {  // header line
                    txt_length += line.length();
                    for (size_t i = 0; i < line.length(); ++i) {
                        unsigned char c = line[i];
                        unsigned char type = symbol_type_table[c];

                        if ( type & fDNA_Alphabet ) {
                            ++ATGC_content_txt;
                        }
                        if ( type & fProtein_Alphabet ) {
                            ++amino_acid_content_txt;
                        }
                    } // for
                } else { // sequence line
                    seq_length += line.length();
                    for (size_t i = 0; i < line.length(); ++i) {
                        unsigned char c = line[i];
                        unsigned char type = symbol_type_table[c];

                        if ( type & fDNA_Alphabet ) {
                            ++ATGC_content;
                        }
                        if ( type & fProtein_Alphabet ) {
                            ++amino_acid_content;
                        }
                    } // for
                }
            } // while
            double dna_content = 
                (double)ATGC_content / (double)seq_length;
            double prot_content = 
                (double)amino_acid_content / (double)seq_length;
            double dna_content_txt = 
                (double)ATGC_content_txt / (double)txt_length;
            double prot_content_txt = 
                (double)amino_acid_content_txt / (double)txt_length;
            // check for high seq content in seq.lines and low in txt lines
            if (dna_content > 0.91 && dna_content_txt < 0.5) {
                return eFasta;  // DNA fasta file
            }
            if (prot_content > 0.91 && prot_content_txt < 0.5) {
                return eFasta;  // Protein fasta file
            }
        }
    }

    if (a_content > 0.80) {  // Text ASN ?
        // Check for "::=" as second field of first non-blank
        // line, treating comment-only lines ("--" first non-whitespace)
        // as blank.
        // Note: legal ASN.1 text files can fail this because
        // 1. The "::=" need not be separated from its neigbors
        //    by whitespace
        // 2. It need not be on the first non-blank, non-comment line
        CNcbiIstrstream istr(reinterpret_cast<const char *>(buffer), buffer_size);
        string line;
        vector<string> fields;
        while (!istr.fail()) {
            NcbiGetline(istr, line, "\n\r");
            fields.clear();
            NStr::Tokenize(line, " \t", fields, NStr::eMergeDelims);
            if (fields.size() > 0) {
                if (fields[0][0] == '-' && fields[0][1] == '-') {
                    continue;  // treat comment lines like blank lines
                }
                if (fields.size() >= 2) {
                    if (fields[1] == "::=") {
                        return eTextASN;
                    }
                }
                break;  // only interested in first such line
            }
        }
    }
    if ( eBinAsn == eMaybe ) {
        return eBinaryASN;
    }
    return format;

}


CFormatGuess::EFormat CFormatGuess::Format(CNcbiIstream& input)
{
    CT_POS_TYPE orig_pos = input.tellg();

    unsigned char buf[1024];

    input.read((char*)buf, sizeof(buf));
    size_t count = input.gcount();
    input.clear();  // in case we reached eof
    CStreamUtils::Stepback(input, (CT_CHAR_TYPE*)buf, (streamsize)count);

    return Format(buf, count);
}

//  ============================================================================
//  New Style Interface:
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
    : m_Stream( * new CNcbiIfstream( FileName.c_str() ) )
    , m_bOwnsStream( true )
{
    Initialize();
}

//  ----------------------------------------------------------------------------
CFormatGuess::CFormatGuess(
    CNcbiIfstream& Stream )
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
CFormatGuess::GuessFormat(
    EMode mode )
{
    if ( TestFormatRepeatMasker( mode ) ) {
        return eRmo;
    }
    if ( TestFormatPhrapAce( mode ) ) {
        return ePhrapAce;
    }
    if ( TestFormatGtf( mode ) ) {
        return eGtf;
    }
    if ( TestFormatGlimmer3( mode ) ) {
        return eGlimmer3;
    }
    if ( TestFormatAgp( mode ) ) {
        return eAgp;
    }
    if ( TestFormatNewick( mode ) ) {
        return eNewick;
    }
    if ( TestFormatXml( mode ) ) {
        return eXml;
    }
    if ( TestFormatAlignment( eQuick /* !!! */ ) ) {
        return eAlignment;
    }
    if ( TestFormatBinaryAsn( mode ) ) {
        return eBinaryASN;
    }
    if ( TestFormatDistanceMatrix( mode ) ) {
        return eDistanceMatrix;
    }
    if ( TestFormatTaxplot( mode ) ) {
        return eTaxplot;
    }
    if ( TestFormatFlatFileSequence( mode ) ) {
        return eFlatFileSequence;
    }
    if ( TestFormatFiveColFeatureTable( mode ) ) {
        return eFiveColFeatureTable;
    }
    if ( TestFormatTable( mode ) ) {
        return eTable;
    }
    if ( TestFormatFasta( mode ) ) {
        return eFasta;
    }
    if ( TestFormatTextAsn( mode ) ) {
        return eTextASN;
    }
    return eUnknown;
}

//  ----------------------------------------------------------------------------
bool
CFormatGuess::TestFormat(
    EFormat format,
    EMode mode )
{
    switch( format ) {
    
    case eRmo:
        return TestFormatRepeatMasker( mode );
    case ePhrapAce:
        return TestFormatPhrapAce( mode );
    case eGtf:
        return TestFormatGtf( mode );
    case eGlimmer3:
        return TestFormatGlimmer3( mode );
    case eAgp:
        return TestFormatAgp( mode );
    case eNewick:
        return TestFormatNewick( mode );
    case eXml:
        return TestFormatXml( mode );
    case eAlignment:
        return TestFormatAlignment( mode );
    case eBinaryASN:
        return TestFormatBinaryAsn( mode );
    case eDistanceMatrix:
        return TestFormatDistanceMatrix( mode );
    case eTaxplot:
        return TestFormatTaxplot( mode );
    case eFlatFileSequence:
        return TestFormatFlatFileSequence( mode );
    case eFiveColFeatureTable:
        return TestFormatFiveColFeatureTable( mode );
    case eTable:
        return TestFormatTable( mode );
    case eFasta:
        return TestFormatFasta( mode );
    case eTextASN:
        return TestFormatTextAsn( mode );

    default:
        NCBI_THROW( CCoreException, eInvalidArg, 
            "CFormatGuess::TestFormat(): Unsupported format ID." );
    }
}

//  ----------------------------------------------------------------------------
void
CFormatGuess::Initialize()
{
    m_pTestBuffer = 0;

    m_bStatsAreValid = false;
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
    if ( ! m_Stream.is_open()  ||  ! m_Stream.good() ) {
        return false;
    }
    m_pTestBuffer = new char[ s_iTestBufferSize ];
    m_Stream.read( m_pTestBuffer, s_iTestBufferSize );
    m_iTestDataSize = m_Stream.gcount();
    m_Stream.clear();  // in case we reached eof
    CStreamUtils::Stepback( m_Stream, m_pTestBuffer, m_iTestDataSize );
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
    //     with '>'
    //   m_iStatsCountDnaChars: number of characters counted in m_iStatsCountData
    //     from the DNA alphabet
    //   m_iStatsCountAaChars: number of characters counted in m_iStatsCountData
    //     from the AA alphabet
    //
    while ( ! TestBuffer.fail() ) {
        NcbiGetline( TestBuffer, strLine, "\n\r" );
        size_t size = strLine.size();
        bool is_header = size > 0 && strLine[0] == '>';
        for ( size_t i=0; i < size; ++i ) {
            unsigned char c = strLine[i];
            unsigned char type = symbol_type_table[c];

            if ( type & (fAlpha | fDigit | fSpace) ) {
                ++m_iStatsCountAlNumChars;
            }
            if ( !is_header ) {
                ++m_iStatsCountData;

                if ( type & fDNA_Alphabet ) {
                    ++m_iStatsCountDnaChars;
                }
                if ( type & fProtein_Alphabet ) {
                    ++m_iStatsCountAaChars;
                }
                if ( type & fLineEnd ) {
                    --m_iStatsCountData;
                }
            }
        }
    }
    m_bStatsAreValid = true;
    return true;
}

//  ----------------------------------------------------------------------------
bool
s_RepresentsAsnComment(
    const vector<string>& Fields )
{
    if ( Fields.size() == 0 ) {
        return true;
    }
    return ( NStr::StartsWith( Fields[0], "--" ) );
}

//  ----------------------------------------------------------------------------
bool
CFormatGuess::TestFormatRepeatMasker(
    EMode /* not used */ )
{
    if ( ! EnsureTestBuffer() ) {
        return false;
    }
    return x_IsInputRepeatMasker( m_pTestBuffer, m_iTestDataSize );
}

//  ----------------------------------------------------------------------------
bool
CFormatGuess::TestFormatPhrapAce(
    EMode /* not used */ )
{
    if ( ! EnsureTestBuffer() ) {
        return false;
    }
    return x_IsInputPhrapAce( m_pTestBuffer, m_iTestDataSize );
}

//  -----------------------------------------------------------------------------
bool
CFormatGuess::TestFormatGtf(
    EMode /* not used */ )
{
    if ( ! EnsureTestBuffer() ) {
        return false;
    }
    return x_IsInputGtf( m_pTestBuffer, m_iTestDataSize );
}
    
//  -----------------------------------------------------------------------------
bool
CFormatGuess::TestFormatGlimmer3(
    EMode /* not used */ )
{
    if ( ! EnsureTestBuffer() ) {
        return false;
    }
    return x_IsInputGlimmer3( m_pTestBuffer, m_iTestDataSize );
}
    
//  -----------------------------------------------------------------------------
bool
CFormatGuess::TestFormatAgp(
    EMode /* not used */ )
{
    if ( ! EnsureTestBuffer() ) {
        return false;
    }
    return x_IsInputAgp( m_pTestBuffer, m_iTestDataSize );
}
    
//  -----------------------------------------------------------------------------
bool
CFormatGuess::TestFormatNewick(
    EMode /* not used */ )
{
    if ( ! EnsureTestBuffer() ) {
        return false;
    }
    return x_IsInputNewick( m_pTestBuffer, m_iTestDataSize );
}
    
//  -----------------------------------------------------------------------------
bool
CFormatGuess::TestFormatXml(
    EMode /* not used */ )
{
    if ( ! EnsureTestBuffer() ) {
        return false;
    }
    return x_IsInputXml( m_pTestBuffer, m_iTestDataSize );
}
    
//  -----------------------------------------------------------------------------
bool
CFormatGuess::TestFormatAlignment(
    EMode /* not used */ )
{
    if ( ! EnsureTestBuffer() ) {
        return false;
    }
    return x_IsInputAlignment( m_pTestBuffer, m_iTestDataSize );
}

//  -----------------------------------------------------------------------------
bool
CFormatGuess::TestFormatBinaryAsn(
    EMode /* not used */ )
{
    if ( ! EnsureTestBuffer() ) {
        return false;
    }
    return x_IsInputBinaryAsn( m_pTestBuffer, m_iTestDataSize ) != eNo;
}

//  -----------------------------------------------------------------------------
bool
CFormatGuess::TestFormatDistanceMatrix(
    EMode /* not used */ )
{
    if ( ! EnsureTestBuffer() ) {
        return false;
    }
    return x_IsInputDistanceMatrix( m_pTestBuffer, m_iTestDataSize );
}

//  -----------------------------------------------------------------------------
bool
CFormatGuess::TestFormatTaxplot(
    EMode /* not used */ )
{
    if ( ! EnsureTestBuffer() ) {
        return false;
    }
    return x_IsInputTaxplot( m_pTestBuffer, m_iTestDataSize );
}

//  -----------------------------------------------------------------------------
bool
CFormatGuess::TestFormatFlatFileSequence(
    EMode /* not used */ )
{
    if ( ! EnsureTestBuffer() ) {
        return false;
    }
    return x_IsInputFlatFileSequence( m_pTestBuffer, m_iTestDataSize );
}

//  -----------------------------------------------------------------------------
bool
CFormatGuess::TestFormatFiveColFeatureTable(
    EMode /* not used */ )
{
    if ( ! EnsureTestBuffer() ) {
        return false;
    }
    return x_IsInputFiveColFeatureTable( m_pTestBuffer, m_iTestDataSize );
}

//  -----------------------------------------------------------------------------
bool
CFormatGuess::TestFormatTable(
    EMode /* not used */ )
{
    if ( ! EnsureTestBuffer() ) {
        return false;
    }
    return x_IsInputTable( m_pTestBuffer, m_iTestDataSize );
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
        return false;
    }

    // remaining decision based on text stats:
    double dAlNumFraction =  (double)m_iStatsCountAlNumChars / m_iTestDataSize;
    double dDnaFraction = (double)m_iStatsCountDnaChars / m_iStatsCountData;
    double dAaFraction = (double)m_iStatsCountAaChars / m_iStatsCountData;

    // want at least 80% text-ish overall:
    if ( dAlNumFraction < 0.80 ) {
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
        if ( s_RepresentsAsnComment( Fields  ) ) {
            continue;
        }      
        return ( Fields.size() >= 2 && Fields[1] == "::=" );
    }
    return false;
}

 END_NCBI_SCOPE
