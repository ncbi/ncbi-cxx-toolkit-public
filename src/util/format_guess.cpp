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


static bool isDNA_Alphabet(char ch)
{
    return ::strchr("ATGCN", ch) != 0;
}

// Check if letter belongs to amino acid alphabet
static bool isProtein_Alphabet(char ch)
{
    return ::strchr("ACDEFGHIKLMNPQRSTVWYBZX", ch) != 0;
}

// Check if character belongs to the CR/LF group of symbols
static inline bool isLineEnd(char ch)
{
    return ch == 0x0D || ch == 0x0A || ch == '\n';
}

bool x_SplitLines( const char* byte_buf, size_t byte_count, list< string>& lines )
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


bool x_IsLineRepeatMasker( const string& line ) 
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


bool x_IsInputRepeatMaskerWithHeader( list< string >& lines  )
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


bool x_IsInputRepeatMaskerWithoutHeader( list< string>& lines  )
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
        if ( ! x_IsLineRepeatMasker( *it ) ) {
            return false;
        }
    }
    
    return true;
}


bool x_IsInputRepeatMasker( const char* byte_buf, size_t byte_count ) 
{
    list<string> lines;
    if ( ! x_SplitLines( byte_buf, byte_count, lines ) ) {
        //  seemingly not even ASCII ...
        return false;
    }
    
    return x_IsInputRepeatMaskerWithHeader( lines ) || 
        x_IsInputRepeatMaskerWithoutHeader( lines  );
}


bool x_IsLinePhrapId( const string& line ) 
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


bool x_IsInputPhrapAce( const char* byte_buf, size_t byte_count )
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


bool x_IsLineGlimmer3(const string& line)
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


bool x_IsInputGlimmer3( const char* byte_buf, size_t byte_count )
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


bool x_IsLineGtf( const string& line )
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


bool x_IsLineAgp( const string& line ) 
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
    

bool x_IsInputGtf( const char* byte_buf, size_t byte_count )
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


bool x_IsInputAgp( const char* byte_buf, size_t byte_count )
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


bool x_IsLabelNewick( const string& label )
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


bool x_IsLineNewick( const string& cline )
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


bool x_IsInputNewick( const char* byte_buf, size_t byte_count )
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


bool x_IsInputAlignment( const char* byte_buf, size_t byte_count )
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
        

bool x_IsInputXml( const char* byte_buf, size_t byte_count )
{
    if (byte_count > 5) {
        const char* xml_sig = "<?XML";
        for (unsigned i = 0; i < 5; ++i) {
            unsigned char ch = byte_buf[i];
            char upch = toupper(ch);
            if (upch != xml_sig[i]) {
                return false;
            }
        }
        return true;
    }
    
    return false;
}


bool x_IsInputBinaryAsn( const char* byte_buf, size_t byte_count )
{
    //
    //  Criterion: Presence of any non-printing characters
    //
    for (unsigned int i = 0;  i < byte_count;  ++i) {
        if ( !isgraph((unsigned char) byte_buf[i])  &&  
            !isspace((unsigned char) byte_buf[i]) ) 
        {
            return true;
        }
    }
    return false;
}
    

CFormatGuess::ESequenceType 
CFormatGuess::SequenceType(const char* str, unsigned length)
{
    if (length == 0)
        length = (unsigned)::strlen(str);

    unsigned ATGC_content = 0;
    unsigned amino_acid_content = 0;

    for (unsigned i = 0; i < length; ++i) {
        unsigned char ch = str[i];
        char upch = toupper(ch);

        if (isDNA_Alphabet(upch)) {
            ++ATGC_content;
        }
        if (isProtein_Alphabet(upch)) {
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
    if (!buffer_size) {
        return eUnknown;
    }
    EFormat format = eUnknown;

    if ( x_IsInputRepeatMasker( (const char*)buffer, buffer_size ) ) {
        return eRmo;
    }
    if ( x_IsInputPhrapAce( (const char*)buffer, buffer_size ) ) {
        return ePhrapAce;
    }
    if ( x_IsInputGtf( (const char*)buffer, buffer_size ) ) {
        return eGtf;
    }
    if ( x_IsInputGlimmer3( (const char*)buffer, buffer_size ) ) {
        return eGlimmer3;
    }
    if ( x_IsInputAgp( (const char*)buffer, buffer_size ) ) {
        return eAgp;
    }
    if ( x_IsInputNewick( (const char*)buffer, buffer_size ) ) {
        return eNewick;
    }
    if ( x_IsInputXml( (const char*)buffer, buffer_size ) ) {
        return eXml;
    }
    if ( x_IsInputAlignment( (const char*)buffer, buffer_size ) ) {
        return eAlignment;
    }
    if ( x_IsInputBinaryAsn( (const char*)buffer, buffer_size ) ) {
        return eBinaryASN;
    }

    //
    //  The following is actually three tests rolled into one, based on symbol
    //  frequencies in the input sample. I am eaving them "as is".
    //
    unsigned int i = 0;
    unsigned ATGC_content = 0;
    unsigned amino_acid_content = 0;
    unsigned seq_length = (unsigned)buffer_size;

    unsigned alpha_content = 0;

    if (buffer[0] == '>') { // FASTA ?
        for (i = 0; (!isLineEnd(buffer[i])) && i < buffer_size; ++i) {
            // skip the first line (presumed this is free-text information)
            unsigned char ch = buffer[i];
            if (isalnum(ch) || isspace(ch)) {
                ++alpha_content;
            }
        }
        seq_length = (unsigned)buffer_size - i;
        if (seq_length == 0) {
            return eUnknown;   // No way to tell what format is this...
        }
    }

    for (i = 0; i < buffer_size; ++i) {
        unsigned char ch = buffer[i];
        char upch = toupper(ch);

        if (isalnum(ch) || isspace(ch)) {
            ++alpha_content;
        }
        if (isDNA_Alphabet(upch)) {
            ++ATGC_content;
        }
        if (isProtein_Alphabet(upch)) {
            ++amino_acid_content;
        }
        if (isLineEnd(ch)) {
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
    return format;

}


CFormatGuess::EFormat CFormatGuess::Format(CNcbiIstream& input)
{
    EFormat format = eUnknown;

    CT_POS_TYPE orig_pos = input.tellg();

    unsigned char buf[1024];

    input.read((char*)buf, sizeof(buf));
    size_t count = input.gcount();
    input.clear();  // in case we reached eof
    CStreamUtils::Pushback(input, (const CT_CHAR_TYPE*)buf, (streamsize)count);

    return Format(buf, count);
}

END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.30  2006/09/26 20:30:49  jcherry
 * ASN.1 text: look for "::=" instead of "{", and tolerate blank lines and
 * comment lines at top of file
 *
 * Revision 1.29  2006/09/25 14:58:06  kuznets
 * Added memory based method for format prediction. Fixed stream seek
 *
 * Revision 1.28  2006/08/17 13:20:56  ludwigf
 * ADDED: Simple test that will identify *some* alignment files.
 *
 * Revision 1.27  2006/06/22 17:58:28  dicuccio
 * Ignore leading comment lines in GTF/GFF files
 *
 * Revision 1.26  2006/06/15 17:45:40  dicuccio
 * Added recognition of Glimmer3 predictions
 *
 * Revision 1.25  2006/06/09 15:08:29  ludwigf
 * FIXED: Two warnings that I introduced in the last check in.
 *
 * Revision 1.24  2006/06/08 18:48:41  ludwigf
 * FIXED: "Guessing" of Newick Tree file format.
 *
 * Revision 1.23  2006/06/05 16:19:53  ludwigf
 * ADDED: File type check for PHRAP ACE restored with the dependency on
 *  regexp removed.
 *
 * Revision 1.22  2006/06/05 15:42:26  ludwigf
 * *** TEMPORARY *** : Removed file type check for PHRAP ACE as it introduced
 *  and unwanted dependency on regexp. This check will be back as soon as I
 *  rewrite the offending section of code.
 *
 * Revision 1.21  2006/06/05 15:10:08  ludwigf
 * ADDED: File type checks for PHRAP ACE, GTF, AGP, Newick tree.
 *
 * Revision 1.20  2005/06/22 15:45:56  kuznets
 * Added X as a legal protein character
 *
 * Revision 1.19  2005/06/03 17:04:45  lavr
 * Explicit (unsigned char) casts in ctype routines
 *
 * Revision 1.18  2005/05/12 15:08:21  lavr
 * Use explicit (unsigned char) conversion in <ctype.h>'s macros
 *
 * Revision 1.17  2004/05/17 21:06:02  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.16  2004/04/01 12:17:20  kuznets
 * Added 'N' to the legal DNA alphabet. (isDNA_Alphabet)
 *
 * Revision 1.15  2004/03/24 20:35:34  ucko
 * Use the correct type (CT_POS_TYPE rather than int) for tellg's return value.
 *
 * Revision 1.14  2004/03/23 23:04:23  jcherry
 * clear() stream after reading; seekg() in just one place
 *
 * Revision 1.13  2004/03/23 22:29:50  jcherry
 * Added Format(CNcbiIstream& input)
 *
 * Revision 1.12  2004/03/02 20:06:59  johnson
 * bug fix: missing loop initializers
 *
 * Revision 1.11  2004/03/01 15:49:54  dicuccio
 * Added explicit check for binary ASN
 *
 * Revision 1.10  2003/12/02 20:16:09  kuznets
 * Improved ASN binary recognition by checking ASN specific signatures
 *
 * Revision 1.9  2003/11/26 14:34:16  kuznets
 * Fine tuned ascii content coefficient to better recognize binary asns
 *
 * Revision 1.8  2003/11/07 17:16:23  ivanov
 * Fixed  warnings on 64-bit Workshop compiler
 *
 * Revision 1.7  2003/07/10 19:58:25  ivanov
 * Get rid of compilation warning: removed double variable declaration
 *
 * Revision 1.6  2003/07/08 20:30:50  kuznets
 * Fixed bug with different "\n" coding in DOS-Windows and Unix.
 *
 * Revision 1.5  2003/07/07 19:54:06  kuznets
 * Improved format recognition of short fasta files
 *
 * Revision 1.4  2003/06/20 20:58:04  kuznets
 * Cleaned up amino-acid alphabet recognition.
 *
 * Revision 1.3  2003/05/13 15:18:02  kuznets
 * added sequence type guessing function
 *
 * Revision 1.2  2003/05/09 14:08:28  ucko
 * ios_base:: -> IOS_BASE:: for gcc 2.9x compatibility
 *
 * Revision 1.1  2003/05/08 19:46:34  kuznets
 * Initial revision
 *
 * ===========================================================================
 */
