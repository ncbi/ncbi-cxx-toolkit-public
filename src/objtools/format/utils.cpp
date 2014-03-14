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
* Author:  Mati Shomrat, NCBI
*
* File Description:
*   shared utility functions
*
*/
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include <util/strsearch.hpp>

#include <objects/general/Date.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/general/Date.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/Seq_ext.hpp>
#include <objects/seq/Delta_ext.hpp>
#include <objects/seq/Delta_seq.hpp>
#include <objects/seq/Seq_literal.hpp>
#include <objects/seq/MolInfo.hpp>
#include <objects/seq/seqport_util.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/util/sequence.hpp>
#include <objects/general/general_macros.hpp>
#include <algorithm>
#include "utils.hpp"


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

bool IsPartOfUrl(
    const string& sentence,
    size_t pos )
{
    string separators( "( \t\r\n" );
    const static string legal_path_chars(
        "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz_-." );
    
    //
    //  Weed out silly input:
    //
    if ( sentence == "" || pos > sentence.length() - 1 ) {
        return false;
    }
    if ( string::npos != separators.find( sentence[ pos ] ) ) {
        return false;
    }

    // Do easy tests first:

    //  We require the tilde to show up in a pattern like
    //  "/~[0..9A..Za..z_-.]+". This is inherited from the C toolkit flat file
    //  generator:
    //
    if ( (pos < 1) || (sentence[ pos-1 ] != '/') ) {
        return false;
    }
    
    //
    //  Find the start of the "word" that surrounds the given position:
    //
    separators += '~';
    string::size_type left_edge = sentence.find_last_of( separators, pos-1 );
    if ( left_edge == string::npos ) {
        left_edge = 0;
    }
    else {
        ++left_edge;
    }
    
    //
    //  If it's a URL, it better start with a protocol specifier we approve of:
    //
    static const string sc_ProtocolSpecifiers[] = {
      "URL:",
      "http:",
    };
    DEFINE_STATIC_ARRAY_MAP(CStaticArraySet<string>, vProtocolSpecifiers, sc_ProtocolSpecifiers);
    size_t colon = sentence.find( ':', left_edge );
    if ( colon == string::npos ) {
        return false;
    }
    string strMaybeUrl = sentence.substr( left_edge, colon - left_edge + 1 );
    if ( vProtocolSpecifiers.find( strMaybeUrl ) == vProtocolSpecifiers.end() ) {
        return false;
    }
    
    ++pos;
    if ( string::npos == legal_path_chars.find( sentence[ pos ] ) ) {
        return false;
    }
    
    for ( ++pos; sentence[ pos ] != 0; ++pos ) {
        if ( string::npos == legal_path_chars.find( sentence[ pos ] ) ) {
            return ( sentence[ pos ] == '/' );
        }
    }
    
    return false; /* never found the terminating '/' */
};     

void ExpandTildes(string& s, ETildeStyle style)
{

    if ( style == eTilde_tilde ) {
        return;
    }

    SIZE_TYPE start = 0, tilde, length = s.length();

    tilde = s.find('~', start);
    if (tilde == NPOS) {  // no tilde
        return;
    }

    string result;

    while ( (start < length)  &&  (tilde = s.find('~', start)) != NPOS ) {
        result.append(s, start, tilde - start);
        char next = (tilde + 1) < length ? s[tilde + 1] : 0;
        switch ( style ) {
        case eTilde_space:
            if ( (tilde + 1 < length  &&  isdigit((unsigned char) next) )  ||
                 (tilde + 2 < length  &&  (next == ' '  ||  next == '(')  &&
                  isdigit((unsigned char) s[tilde + 2]))) {
                result += '~';
            } else {
                result += ' ';
            }
            start = tilde + 1;
            break;
            
        case eTilde_newline:
            if ( tilde + 1 < length  &&  s[tilde + 1] == '~' ) {
                result += '~';
                start = tilde + 2;
            } else {
                result += "\n";
                start = tilde + 1;
            }
            break;

        case eTilde_note:
            if ( tilde + 1 < length  &&  s[tilde + 1] == '~' ) {
                result += '~';
                start = tilde + 2;
            } else {
                // plain "~" expands to ";\n", unless it's after a space or semi-colon, in
                // which case it becomes a plain "\n"
                char prevChar = ( tilde >= 1 ? s[tilde - 1] : '\0' );

                if( ' ' == prevChar || ';' == prevChar ) {
                    result += '\n';
                } else {
                    result += ";\n";
                }
                start = tilde + 1;
            }
            break;

        case eTilde_comment:
            if (tilde > 0  &&  s[tilde - 1] == '`') {
                result.replace(result.length() - 1, 1, 1,'~');
            }
            else if ( IsPartOfUrl( s, tilde ) ) {
                result += '~';
            } 
            else {
                result += "\n";
            }
            start = tilde + 1;
            break;

        default: // just keep it, for lack of better ideas
            result += '~';
            start = tilde + 1;
            break;
        }
    }
    if (start < length) {
        result.append(s, start, NPOS);
    }
    s.swap(result);
}


void ConvertQuotes(string& str)
{
    replace(str.begin(), str.end(), '\"', '\'');
}


string ConvertQuotes(const string& str)
{
    string retval = str;
    ConvertQuotes(retval);
    return retval;
}

// Strips all spaces in string in following manner. If the function
// meet several spaces (spaces and tabs) in succession it replaces them
// with one space. Strips all spaces after '(' and before ( ')' or ',' ).
void StripSpaces(string& str)
{
    if (str.empty()) {
        return;
    }

    string::iterator end = str.end();
    string::iterator it = str.begin();
    string::iterator new_str = it;
    while (it != end) {
        *new_str++ = *it;
        if ( (*it == ' ')  ||  (*it == '\t')  ||  (*it == '(') ) {
            for (++it; *it == ' ' || *it == '\t'; ++it) continue;
            if (*it == ')' || *it == ',') {
                if( *(new_str - 1) != '(' ) { // this if protects against the case "(...bunch of spaces and tabs...)".  Otherwise, the first '(' is erased
                    --new_str;
                }
            }
        } else {
            ++it;
        }
    }
    str.erase(new_str, str.end());
}


bool RemovePeriodFromEnd(string& str, bool keep_ellipsis)
{
    
    // NB: this is likely a better solution; however, the C toolkit differs...
    //string::size_type pos = str.find_last_not_of(".,;:() ");
    // string::size_type pos = str.find_last_not_of(".,;: ");
    //string::size_type pos = str.find_last_not_of(".");
    //string::size_type pos2 = str.find("...", pos);
    //// string::size_type pos3 = str.find_first_of(".", pos);
    //if (pos < str.size() - 1) {
    //    str.erase(pos + 1);
    //    if (keep_ellipsis  &&  pos2 != string::npos) {
    //        str += "...";
    //    }
    //}
    //return ( pos != string::npos );

    const string::size_type len = str.length();

    if( keep_ellipsis ) {
        if( len >= 3 && str[len-1] == '.' && str[len-2] == '.' && str[len-3] == '.' ) {
            return false;
        }
    }

    // chop off period if there's one at the end
    if( len >= 1 && str[len-1] == '.' ) {
        str.resize( len - 1 );
        return true;
    } else {
        return false;
    }

    /* string::size_type pos2 = str.find_last_not_of(";,.");
    string::size_type pos3 = str.find_last_not_of(" ", pos2);
    if (pos3 < pos2) {
        str.erase(pos3 + 1);
        pos2 = str.find_last_not_of(";,.");
    }

    string::size_type pos = str.find_last_not_of(".");
    if (pos2 < str.size() - 1) {
        if (keep_ellipsis) {
            /// trim the end to an actual ellipsis
            if (str.length() - pos2 > 3) {
                if (pos2 < pos) {
                    str.erase(pos2 + 1);
                    str += "...";
                    return true;
                }
                pos += 3;
            }
            else if (pos2 < pos) {
                pos = pos2;
            }
        } else if (pos2 < pos) {
            pos = pos2;
        }
        if (pos < str.size() - 1) {
            str.erase(pos + 1);
            return true;
        }
    } */

    /**
    static const string kEllipsis = "...";

    if ( NStr::EndsWith(str, '.') ) {
        if ( !keep_ellipsis  ||  !NStr::EndsWith(str, kEllipsis) ) {
            str.erase(str.length() - 1);
            return true;
        }
    }
    **/
    // return false;
}


void AddPeriod(string& str)
{
    size_t pos = str.find_last_not_of(" \t~.\n");
    str.erase(pos + 1);
    str += '.';
}


void TrimSpaces(string& str, int indent)
{
    if (str.empty()  ||  str.length()  <= (size_t)indent) {
        return;
    }
    if (indent < 0) {
        indent = 0;
    }

    int end = str.length() - 1;
    while (end >= indent  &&  isspace((unsigned char) str[end])) {
        end--;
    }
    if (end < indent) {
        str.erase(indent);
    } else {
        str.erase(end + 1);
    }
}

// needed because not all compilers will just let you pass "isgraph" to STL find_if
class CIsGraph
{
public:
    bool operator()( const char c ) {
        return isgraph(c);
    }
};

// This will compress multiple spaces in a row.
// It also translates unprintable characters to spaces.
// If trim_beginning, strips all spaces and unprintables from beginning of string.
// If trim_end, strips all spaces and unprintables from end of string.
// returns the string you gave it.
string &CompressSpaces( string& str, const bool trim_beginning, const bool trim_end )
{
    if( str.empty() ) {
        return str;
    }

    // set up start_iter and end_iter to determine the range in which we're looking

    string::iterator start_iter = str.begin();
    if( trim_beginning ) {
        start_iter = find_if( str.begin(), str.end(), CIsGraph() );
    }
    if( str.end() == start_iter ) {
        str.clear();
        return str;
    }

    string::iterator end_iter = str.end();
    if( trim_end ) {
        string::reverse_iterator rev_iter = find_if( str.rbegin(), str.rend(), CIsGraph() );
        end_iter = str.begin() + ( str.rend() - rev_iter );
    }
    if( str.begin() == end_iter ) {
        str.clear();
        return str;
    }

    // The main part, where we compress spaces
    string newstr; // result will end up here
    newstr.reserve( end_iter - start_iter );

    // efficiency note: If the efficiency of unique_copy followed by transform becomes 
    // burdensome, we may have to replace these 2 calls with one raw loop that does
    // what those calls do ( a sloppier and more bug-prone ( but faster ), prospect)

    // copy such that consecutive spaces or control characters are compressed to one space
    char last_ch_was_printable = true;
    for( string::iterator iter = start_iter; iter < end_iter; ++iter ) {
        const char ch = *iter;
        if( isgraph(ch) ) {
            // visible characters get copied straight
            newstr += ch;
            last_ch_was_printable = true;
        } else {
            // unprintable chars become space, and they're only appended if the last char was 
            // printable
            if( last_ch_was_printable ) {
                newstr += ' ';
            }
            last_ch_was_printable = false;
        }
    }

    str.swap( newstr );
    return str;
}


// returns true if it changed the string
bool TrimSpacesAndJunkFromEnds(string& str, bool allow_ellipsis)
{
    // TODO: This commented out code represents how ellipsis trimming
    // should work.  However, for compatibility with C, we're using a
    // (in my opinion) suboptimal algorithm.  We can switch over later.

    //if (str.empty()) {
    //    return;
    //}

    //size_t strlen = str.length();
    //size_t begin = 0;

    //// trim unprintable characters (and space) off the beginning
    //while (begin != strlen) {
    //    unsigned char ch = str[begin];
    //    if (ch > ' ') {
    //        break;
    //    } else {
    //        ++begin;
    //    }
    //}

    //// we're done if we trimmed the string to nothing
    //if (begin == strlen) {
    //    str.erase();
    //    return;
    //}

    //// trim junk off the end (while we're at it, record whether we're chopping off a period)
    //size_t end = strlen - 1;
    //bool has_period = false;
    //while (end > begin) {
    //    unsigned char ch = str[end];
    //    if (ch <= ' '  ||  ch == '.'  ||  ch ==  ','  ||  ch == '~'  ||  ch == ';') {
    //        has_period = (has_period  ||  ch == '.');
    //        --end;
    //    } else {
    //        break;
    //    }
    //}

    //// check whether we're about to chop off an ellipsis, so we remember to add it back
    //// TODO: There's got to be a more efficient way of doing this
    //const bool weChoppedOffAnEllipsis = ( NPOS != NStr::Find(str, "...", end) );

    //// do the actual chopping here
    //str = str.substr( begin, end + 1 );

    //// restore chopped off ellipsis or period, if any
    //if ( allow_ellipsis && weChoppedOffAnEllipsis ) {
    //    str += "...";
    //} else if (has_period) {
    //    // re-add any periods if we had one before
    //    str += '.';
    //}

    // This is based on the C function TrimSpacesAndJunkFromEnds.
    // Although it's updated to use iterators and such and to
    // return whether it changed the string, it should
    // have the same output, except:
    // - We do NOT chop off a semicolon if we determine that it's
    //   part of an HTML escape char (e.g. "&bgr;" ).
    // - There are some changes in how tildes are handled;
    //   this algo is less likely to remove them.

    if ( str.empty() ) {
        return false;
    }

    // make start_of_junk_pos hold the beginning of the "junk" at the end
    // (where junk is defined as one of several characters)
    // while we're at it, also check if the junk contains a tilde and/or period
    bool isPeriod = false;
    bool isTilde = false;
    int start_of_junk_pos = str.length() - 1;
    for( ; start_of_junk_pos >= 0 ; --start_of_junk_pos ) {
        const char ch = str[start_of_junk_pos];
        if (ch <= ' ' || ch == '.' || ch == ',' || ch == '~' || ch == ';') {
            // found junk character

            // also, keep track of whether the junk includes a period and/or tilde
            isPeriod = (isPeriod || ch == '.');
            isTilde = (isTilde || ch == '~');
        } else {
            // found non-junk character.  Last junk character is just after this
            ++start_of_junk_pos;
            break;
        }
    }
    // special case of the whole string being junk
    if( start_of_junk_pos < 0 ) {
        start_of_junk_pos = 0;
    }

    // check for ';' that's part of an HTML escape char like "&bgr;" and
    // skip over it (i.e., don't remove it) if so
    if( start_of_junk_pos < (int)str.length() && str[start_of_junk_pos] == ';' ) {
        // we assume no HTML escape char will be longer than this
        static const int kMaxCharsToLookAt = 20;

        // go backwards, looking for the ampersand
        int amp_iter = (start_of_junk_pos - 1);
        for( ; amp_iter >= 0 && ((start_of_junk_pos - amp_iter) < kMaxCharsToLookAt); --amp_iter ) {
            const char ch = str[amp_iter];
            if( isalnum(ch) || ch == '#' ) {
                // just keep going
            } else if( ch == '&' ) {
                // The semicolon ends an HTML escape character, so we skip it
                ++start_of_junk_pos;
                break;
            } else {
                // The semicolon does NOT end an HTML escape character, so we might remove it
                break;
            }
        }
    }

    bool changed = false;

    // if there's junk, chop it off (but leave period/tildes/ellipsis as appropriate)
    if ( start_of_junk_pos < (int)str.length() ) {

        // holds the suffix to add after we remove the junk
        const char * suffix = ""; // by default, just remove junk

        const int chars_in_junk = ( str.length() - start_of_junk_pos );
        _ASSERT( chars_in_junk >= 1 );

        // allow one period at end
        if (isPeriod) {
            // check if we should put an ellipsis, or just a period
            const bool putEllipsis = ( allow_ellipsis && (chars_in_junk >= 3) && 
                str[start_of_junk_pos+1] == '.' && str[start_of_junk_pos+2] == '.' );

            suffix = ( putEllipsis ? "..." : "." );
        } else if (isTilde ) {
            // allow tilde(s)
            // (This should work on single- AND double-tildes because
            // we don't know whether or not tilde-expansion was called before this 
            // point )
            if ( str[start_of_junk_pos] == '~' ) {
                const bool doubleTilde = ( (chars_in_junk >= 2) && str[start_of_junk_pos+1] == '~' );
                suffix = ( doubleTilde  ? "~~" : "~" );
            }
        }
        if( suffix[0] != '\0' ) {
            if( 0 != str.compare( start_of_junk_pos, INT_MAX, suffix) ) {
                str.erase( start_of_junk_pos );
                str += suffix;
                changed = true;
            }
        } else if ( start_of_junk_pos < (int)str.length() ) {
            str.erase( start_of_junk_pos );
            changed = true;
        }
    }

    // copy the part after the initial whitespace to the destination
    string::iterator input_iter = str.begin();
    while ( input_iter != str.end() && *input_iter <= ' ') {
        ++input_iter;
    }
    if( input_iter != str.begin() ) {
        str.erase( str.begin(), input_iter );
        changed = true;
    }

    return changed;
}


static bool s_IsWholeWord(const string& str, size_t pos)
{
    // NB: To preserve the behavior of the C toolkit we only test on the left.
    // This was an old bug in the C toolkit that was never fixed and by now
    // has become the expected behavior.
    return (pos > 0  &&  pos <= str.size()) ?
        isspace((unsigned char) str[pos - 1])  ||  ispunct((unsigned char) str[pos - 1]) : true;
}


void JoinString(string& to, const string& prefix, const string& str, bool noRedundancy)
{
    if ( str.empty() ) {
        return;
    }

    if ( to.empty() ) {
        to += str;
        return;
    }
    
    size_t pos = NPOS;
    if (noRedundancy) {
        //for ( pos = NStr::Find(to, str); pos != NPOS; pos += str.length()) {
        for ( pos = NStr::Find(to, str);
              pos != NPOS;  pos = NStr::Find(to, str, pos + 1)) {
            if (s_IsWholeWord(to, pos)) {
                return;
            }
        }        
    }

    //LOG_POST(Error << "adding: to=" << to << "  prefix=" << prefix << "  str=" << str);

    if( NStr::StartsWith(prefix, ";") && NStr::EndsWith(to, ";") ) {
        to += prefix.substr(1);
    } else {
        to += prefix;
    }
    to += str;
}


string JoinString(const list<string>& l, const string& delim, bool noRedundancy)
{
    if ( l.empty() ) {
        return kEmptyStr;
    }

    /**
    string result;
    set<CTempString> strings;
    ITERATE (list<string>, it, l) {
        if ( !noRedundancy  ||
             strings.insert(CTempString(*it)).second) {
            if ( !result.empty() ) {
                result += delim;
            }
            result += *it;
        }
    }
    **/

    string result = l.front();
    list<string>::const_iterator it = l.begin();
    while ( ++it != l.end() ) {
        JoinString(result, delim, *it, noRedundancy);
    }

    return result;
}


// Validate the correct format of an accession string.
static bool s_IsValidAccession(const string& acc)
{
    static const size_t kMaxAccLength = 16;

    if ( acc.empty() ) {
        return false;
    }

    if ( acc.length() >= kMaxAccLength ) {
        return false;
    }

    // first character must be uppercase letter
    if ( !(isalpha((unsigned char) acc[0])  &&  isupper((unsigned char) acc[0])) ) {
        return false;
    }

    size_t num_alpha   = 0,
           num_undersc = 0,
           num_digits  = 0;

    const char* ptr = acc.c_str();
    if ( NStr::StartsWith(acc, "NZ_") ) {
        ptr += 3;
    }
    for ( ; isalpha((unsigned char)(*ptr)); ++ptr, ++num_alpha );
    for ( ; *ptr == '_'; ++ptr, ++num_undersc );
    for ( ; isdigit((unsigned char)(*ptr)); ++ptr, ++num_digits );

    if ( (*ptr != '\0')  &&  (*ptr != ' ')  &&  (*ptr != '.') ) {
        return false;
    }

    switch ( num_undersc ) {
    case 0:
        {{
            if ( (num_alpha == 1  &&  num_digits == 5)  ||
                 (num_alpha == 2  &&  num_digits == 6)  ||
                 (num_alpha == 3  &&  num_digits == 5)  || 
                 (num_alpha == 4  &&  num_digits == 8)  || 
                 (num_alpha == 4  &&  num_digits == 9) ) {
                return true;
            }
        }}
        break;

    case 1:
        {{
            if( num_alpha == 3 && num_digits == 6 && 
                NStr::StartsWith(acc, "MAP_") ) 
            {
                return true;
            }

            // RefSeq accession
            if ( (num_alpha != 2)  ||
                 (num_digits != 6  &&  num_digits != 8  &&  num_digits != 9) ) {
                return false;
            }
            
            char first_letter = acc[0];
            char second_letter = acc[1];

            if ( first_letter == 'N' ) {
                if ( second_letter == 'C'  ||  second_letter == 'G'  ||
                     second_letter == 'M'  ||  second_letter == 'R'  ||
                     second_letter == 'P'  ||  second_letter == 'W'  ||
                     second_letter == 'T' ) {
                    return true;
                }
            } else if ( first_letter == 'X' ) {
                if ( second_letter == 'M'  ||  second_letter == 'R'  ||
                     second_letter == 'P' ) {
                    return true;
                }
            } else if ( first_letter == 'Z'  ||  first_letter == 'A'  ||
                        first_letter == 'Y' ) {
                return (second_letter == 'P');
            } else if ( first_letter == 'W' ) {
                if ( second_letter == 'P' ) {
                    return true;
                }
            }
        }}
        break;

    default:
        return false;
    }

    return false;
}


static bool s_IsValidDotVersion(const string& accn)
{
    size_t pos = accn.find('.');
    if (pos == NPOS) {
        return false;
    }
    size_t num_digis = 0;
    for (++pos; pos < accn.size(); ++pos) {
        if (isdigit((unsigned char) accn[pos])) {
            ++num_digis;
        } else {
            return false;
        }
    }

    return (num_digis >= 1);
}


bool IsValidAccession(const string& accn, EAccValFlag flag)
{
    bool valid = s_IsValidAccession(accn);
    if (valid  &&  flag == eValidateAccDotVer) {
        valid = s_IsValidDotVersion(accn);
    }
    return valid;
}


void DateToString(const CDate& date, string& str, EDateToString format_choice )
{
    // One day we should make regular format default to JAN, since "JUN" seems
    // kind of arbitrary.
    static const string regular_format  = "%{%2D%|01%}-%{%3N%|JUN%}-%Y";
    static const string cit_sub_format = "%{%2D%|??%}-%{%3N%|???%}-%{%4Y%|/???%}";
    static const string patent_format  = "%{%2D%|01%}-%{%3N%|JAN%}-%Y";

    const string& format = ( format_choice == eDateToString_cit_sub ?
        cit_sub_format :
        ( format_choice == eDateToString_patent ? patent_format : regular_format ) );

    string date_str;
    date.GetDate(&date_str, format);
    NStr::ToUpper(date_str);

    str.append(date_str);
}

void GetDeltaSeqSummary(const CBioseq_Handle& seq, SDeltaSeqSummary& summary)
{
    if ( !seq.IsSetInst()                                ||
         !seq.IsSetInst_Repr()                           ||
         !(seq.GetInst_Repr() == CSeq_inst::eRepr_delta) ||
         !seq.IsSetInst_Ext()                            ||
         !seq.GetInst_Ext().IsDelta() ) {
        return;
    }

    SDeltaSeqSummary temp;
    CScope& scope = seq.GetScope();

    const CDelta_ext::Tdata& segs = seq.GetInst_Ext().GetDelta().Get();
    temp.num_segs = segs.size();
    
    size_t len = 0;

    CNcbiOstrstream text;

    CDelta_ext::Tdata::const_iterator curr = segs.begin();
    CDelta_ext::Tdata::const_iterator end = segs.end();
    CDelta_ext::Tdata::const_iterator next;
    for ( ; curr != end; curr = next ) {
        {{
            // set next to one after curr
            next = curr; ++next;
        }}
        size_t from = len + 1;
        switch ( (*curr)->Which() ) {
        case CDelta_seq::e_Loc:
            {{
                const CDelta_seq::TLoc& loc = (*curr)->GetLoc();
                if ( loc.IsNull() ) {  // gap
                    ++temp.num_gaps;
                    text << "* " << from << ' ' << len 
                         << " gap of unknown length~";
                } else {  // count length
                    size_t tlen = sequence::GetLength(loc, &scope);
                    len += tlen;
                    temp.residues += tlen;
                    text << "* " << setw(8) << from << ' ' << setw(8) << len 
                         << ": contig of " << tlen << " bp in length~";
                }
            }}  
            break;
        case CDelta_seq::e_Literal:
            {{
                const CDelta_seq::TLiteral& lit = (*curr)->GetLiteral();
                size_t lit_len = lit.CanGetLength() ? lit.GetLength() : 0;
                len += lit_len;
                if ( lit.CanGetSeq_data() ) {
                    temp.residues += lit_len;
                    while ( next != end  &&  (*next)->IsLiteral()  &&
                        (*next)->GetLiteral().CanGetSeq_data() ) {
                        const CDelta_seq::TLiteral& next_lit = (*next)->GetLiteral();
                        size_t next_len = next_lit.CanGetLength() ?
                            next_lit.GetLength() : 0;
                        lit_len += next_len;
                        len += next_len;
                        temp.residues += next_len;
                        ++next;
                    }
                    text << "* " << setw(8) << from << ' ' << setw(8) << len 
                         << ": contig of " << lit_len << " bp in length~";
                } else {
                    bool unk = false;
                    ++temp.num_gaps;
                    if ( lit.CanGetFuzz() ) {
                        const CSeq_literal::TFuzz& fuzz = lit.GetFuzz();
                        if ( fuzz.IsLim()  &&  
                             fuzz.GetLim() == CInt_fuzz::eLim_unk ) {
                            unk = true;
                            ++temp.num_faked_gaps;
                            if ( from > len ) {
                                text << "*                    gap of unknown length~";
                            } else {
                                text << "* " << setw(8) << from << ' ' << setw(8) << len 
                                     << ": gap of unknown length~";
                            }
                        }
                    }
                    if ( !unk ) {
                        text << "* " << setw(8) << from << " " << setw(8) << len
                             << ": gap of " << lit_len << " bp~";
                    }
                }
            }}
            break;

        default:
            break;
        }
    }
    summary = temp;
    summary.text = CNcbiOstrstreamToString(text);
}


const string& GetTechString(int tech)
{
    static const string concept_trans_str = "conceptual translation";
    static const string seq_pept_str = "direct peptide sequencing";
    static const string both_str = "conceptual translation with partial peptide sequencing";
    static const string seq_pept_overlap_str = "sequenced peptide, ordered by overlap";
    static const string seq_pept_homol_str = "sequenced peptide, ordered by homology";
    static const string concept_trans_a_str = "conceptual translation supplied by author";
    
    switch ( tech ) {
    case CMolInfo::eTech_concept_trans:
        return concept_trans_str;

    case CMolInfo::eTech_seq_pept :
        return seq_pept_str;

    case CMolInfo::eTech_both:
        return both_str;

    case CMolInfo::eTech_seq_pept_overlap:
        return seq_pept_overlap_str;

    case CMolInfo::eTech_seq_pept_homol:
        return seq_pept_homol_str;

    case CMolInfo::eTech_concept_trans_a:
        return concept_trans_a_str;

    default:
        return kEmptyStr;
    }

    return kEmptyStr;
}


bool s_IsModelEvidanceUop(const CUser_object& uo)
{
    return (uo.CanGetType()  &&  uo.GetType().IsStr()  &&
        uo.GetType().GetStr() == "ModelEvidence");
}


const CUser_object* s_FindModelEvidanceUop(const CUser_object& uo)
{
    if ( s_IsModelEvidanceUop(uo) ) {
        return &uo;
    }

    const CUser_object* temp = 0;
    ITERATE (CUser_object::TData, ufi, uo.GetData()) {
        const CUser_field& uf = **ufi;
        if ( !uf.CanGetData() ) {
            continue;
        }
        const CUser_field::TData& data = uf.GetData();

        switch ( data.Which() ) {
        case CUser_field::TData::e_Object:
            temp = s_FindModelEvidanceUop(data.GetObject());
            break;

        case CUser_field::TData::e_Objects:
            ITERATE (CUser_field::TData::TObjects, obj, data.GetObjects()) {
                temp = s_FindModelEvidanceUop(**obj);
                if ( temp != 0 ) {
                    break;
                }
            }
            break;

        default:
            break;
        }
        if ( temp != 0 ) {
            break;
        }
    }

    return temp;
}


bool s_GetModelEvidance(const CBioseq_Handle& bsh, SModelEvidance& me)
{
    CConstRef<CUser_object> moduop;
    bool result = false;

    for (CSeqdesc_CI it(bsh, CSeqdesc::e_User);  it;  ++it) {
        moduop.Reset(s_FindModelEvidanceUop(it->GetUser()));
        if (moduop.NotEmpty()) {
            result = true;
            CConstRef<CUser_field> ufp;
            if( moduop->HasField("Contig Name") ) {
                ufp = &(moduop->GetField("Contig Name"));
                if ( ufp.NotEmpty()  &&  ufp->IsSetData()  &&  ufp->GetData().IsStr() ) {
                    me.name = ufp->GetData().GetStr();
                }
            }
            if( moduop->HasField("Assembly") ) {
                ufp = &(moduop->GetField("Assembly"));
                if ( ufp.NotEmpty()  &&  ufp->IsSetData()  &&  ufp->GetData().IsFields() ) {
                    ITERATE(CUser_field::C_Data::TFields, fld_itr, ufp->GetData().GetFields()) {
                        const CUser_field& field = **fld_itr;
                        ITERATE(CUser_field::C_Data::TFields, inr_itr, field.GetData().GetFields()) {
                            const CUser_field& ufld = **inr_itr;
                            if ( !ufld.IsSetLabel()  ||  !ufld.GetLabel().IsStr() ) continue;
                            const string& label = ufld.GetLabel().GetStr();
                            if (label != "accession") continue;
                            const CUser_field::C_Data& data = ufld.GetData();
                            if (data.IsStr()) {
                                const string& accn = data.GetStr();
                                me.assembly.push_back(accn);
                            }
                        }
                    }
                }
            }
            if ( moduop->HasField("Method") ) {
                ufp = &(moduop->GetField("Method"));
                if ( ufp.NotEmpty()  &&  ufp->IsSetData()  &&  ufp->GetData().IsStr() ) {
                    me.method = ufp->GetData().GetStr();
                }
            }
            if ( moduop->HasField("Counts") ) {
                ufp = &(moduop->GetField("Counts"));
                if ( ufp->HasField("mRNA")) {
                     me.mrnaEv = true;
                }
                if ( ufp->HasField("EST")) {
                     me.estEv = true;
                }
            }
            if ( moduop->HasField("mRNA") ) {
                me.mrnaEv = true;
            }
            if ( moduop->HasField("EST") ) {
                me.estEv = true;
            }
            if( moduop->HasField("Contig Gi") ) {
                ufp = &(moduop->GetField("Contig Gi"));
                if ( ufp.NotEmpty()  &&  ufp->IsSetData()  &&  ufp->GetData().IsInt() ) {
                    me.gi = GI_FROM(CUser_field::C_Data::TInt, ufp->GetData().GetInt());
                }
            }
            if( moduop->HasField("Contig Span") ) {
                ufp = &(moduop->GetField("Contig Span"));
                if ( ufp.NotEmpty()  &&  ufp->IsSetData()  &&  ufp->GetData().IsInts() 
                    && ufp->IsSetNum() && ufp->GetNum() == 2 && ufp->GetData().GetInts().size() == 2 ) 
                {
                    const CUser_field::C_Data::TInts & int_list = ufp->GetData().GetInts();
                    me.span.first  = int_list[0];
                    me.span.second = int_list[1];
                }
            }
        }
    }

    // if me.name is missing version, try to update from me.gi
    if( me.gi > ZERO_GI && me.name.find('.') == string::npos ) {
        CSeq_id_Handle accver_idh = bsh.GetScope().GetAccVer( CSeq_id_Handle::GetGiHandle(me.gi) );
        if( accver_idh ) {
            CConstRef<CSeq_id> accver_seq_id = accver_idh.GetSeqIdOrNull();
            if( accver_seq_id ) {
                const CTextseq_id *text_id = accver_seq_id->GetTextseq_Id();
                if( text_id && text_id->IsSetAccession() && text_id->IsSetVersion() ) {
                    me.name = text_id->GetAccession() + "." + NStr::IntToString(text_id->GetVersion());
                }
            }
        }
    }

    return result;
}


bool GetModelEvidance(const CBioseq_Handle& bsh, SModelEvidance& me)
{
    if ( s_GetModelEvidance(bsh, me) ) {
        return true;
    }

    if ( CSeq_inst::IsAa(bsh.GetInst_Mol()) ) {
        CBioseq_Handle nuc = sequence::GetNucleotideParent(bsh);
        if ( nuc  ) {
            return s_GetModelEvidance(nuc, me);
        }
    }

    return false;
}


// in Ncbistdaa order
static const char* kAANames[] = {
    "---", "Ala", "Asx", "Cys", "Asp", "Glu", "Phe", "Gly", "His", "Ile",
    "Lys", "Leu", "Met", "Asn", "Pro", "Gln", "Arg", "Ser", "Thr", "Val",
    "Trp", "OTHER", "Tyr", "Glx", "Sec", "TERM", "Pyl", "Xle"
};


const char* GetAAName(unsigned char aa, bool is_ascii)
{
    if (is_ascii) {
        aa = CSeqportUtil::GetMapToIndex
            (CSeq_data::e_Ncbieaa, CSeq_data::e_Ncbistdaa, aa);
    }
    return (aa < sizeof(kAANames)/sizeof(*kAANames)) ? kAANames[aa] : "OTHER";
}

//////////////////////////////////////////////////////////////////////////////

EResolveOrder GetResolveOrder(CScope& scope,
                              const CSeq_id_Handle& mrna,
                              const CSeq_id_Handle& prot,
                              CBioseq_Handle& mrna_bsh,
                              CBioseq_Handle& prot_bsh)
{
    EResolveOrder order = eResolve_NotFound;

    if (order == eResolve_NotFound) {
        CRef<CScope> local_scope(new CScope(*CObjectManager::GetInstance()));
        local_scope->AddDefaults();

        CBioseq_Handle possible_mrna = local_scope->GetBioseqHandle(mrna);
        CBioseq_Handle possible_prot;
        if (possible_mrna) {
            possible_prot =
                possible_mrna.GetTopLevelEntry().GetBioseqHandle(prot);
        }
        if (possible_mrna  &&  possible_prot) {
            order = eResolve_RnaFirst;
        }
    }

    if (order == eResolve_NotFound) {
        CRef<CScope> local_scope(new CScope(*CObjectManager::GetInstance()));
        local_scope->AddDefaults();

        CBioseq_Handle possible_prot = local_scope->GetBioseqHandle(prot);
        CBioseq_Handle possible_mrna;
        if (possible_prot) {
            possible_mrna =
                possible_prot.GetTopLevelEntry().GetBioseqHandle(mrna);
        }

        if (possible_mrna  &&  possible_prot) {
            order = eResolve_ProtFirst;
        }
    }

    switch (order) {
    case eResolve_NotFound:
        mrna_bsh = CBioseq_Handle();
        prot_bsh = CBioseq_Handle();
        break;

    case eResolve_RnaFirst:
        mrna_bsh = scope.GetBioseqHandle(mrna);
        prot_bsh = scope.GetBioseqHandle(prot);
        break;

    case eResolve_ProtFirst:
        prot_bsh = scope.GetBioseqHandle(prot);
        mrna_bsh = scope.GetBioseqHandle(mrna);
        break;
    }

    return order;
}

//////////////////////////////////////////////////////////////////////////////
// HTML utils and strings

//  ============================================================================
//  Link locations:
//  ============================================================================
const string strLinkBaseNuc( 
    "http://www.ncbi.nlm.nih.gov/nuccore/" );
const string strLinkBaseProt( 
    "http://www.ncbi.nlm.nih.gov/protein/" );

const string strLinkBaseEntrezViewer(
    "http://www.ncbi.nlm.nih.gov/entrez/viewer.fcgi?val=" );

const string strLinkBaseTaxonomy( 
    "http://www.ncbi.nlm.nih.gov/Taxonomy/Browser/wwwtax.cgi?" );
const string strLinkBaseTransTable(
    "http://www.ncbi.nlm.nih.gov/Taxonomy/Utils/wprintgc.cgi?mode=c#SG" );
const string strLinkBasePubmed(
    "http://www.ncbi.nlm.nih.gov/pubmed/" );
const string strLinkBaseExpasy(
    "http://www.expasy.org/enzyme/" );
const string strLinkBaseNucSearch(
    "http://www.ncbi.nlm.nih.gov/sites/entrez?db=Nucleotide&amp;cmd=Search&amp;term=" );
const string strLinkBaseGenomePrj(
    "http://www.ncbi.nlm.nih.gov/bioproject/" );
const string strLinkBaseLatLon(
    "http://www.ncbi.nlm.nih.gov/projects/Sequin/latlonview.html" );
const string strLinkBaseGeneOntology (
    "http://amigo.geneontology.org/cgi-bin/amigo/go.cgi?view=details&depth=1&query=GO:" );
const string strLinkBaseGeneOntologyRef (
    "http://www.geneontology.org/cgi-bin/references.cgi#GO_REF:" );
const string strLinkBaseUSPTO(
    "http://patft.uspto.gov/netacgi/nph-Parser?patentnumber=" );

const string strDocLink(
    "http://www.ncbi.nlm.nih.gov/genome/annotation_euk/process/" );

bool ConvertQuotesNotInHTMLTags( string &str )
{   
    bool changes_made = false;

    bool in_tag = false;
    size_t idx = 0;
    for( ; idx < str.length(); ++idx ) {
        switch( str[idx] ) {
        case '<':
            // heuristic
            in_tag = true;
            break;
        case '>':
            in_tag = false;
            break;
        case '"':
            if( ! in_tag ) {
                str[idx] = '\'';
                changes_made = true;
            }
            break;
        }
    }

    return changes_made;
}

// make sure we're not "double-sanitizing"
// (e.g. "&gt;" to "&amp;gt;")
//  ============================================================================
static bool
s_ShouldWeEscapeAmpersand( 
    string::const_iterator str_iter, // yes, COPY not reference
    const string::const_iterator &str_iter_end )
//  ============================================================================
{
    _ASSERT(*str_iter == '&');
    
    // This is a long-winded way of checking if str_iter
    // is at "&gt;", "&lt;", "&quot;" or "&amp;"
    // I'm concerned about regexes being too slow.

    ++str_iter;
    if( str_iter != str_iter_end ) {
        switch( *str_iter ) {
            case 'g':
            case 'l':
                ++str_iter;
                if( str_iter != str_iter_end && *str_iter == 't' ) {
                    ++str_iter;
                    if( str_iter != str_iter_end && *str_iter == ';'  ) {
                        return false;
                    }
                }
                break;
            case 'a':
                ++str_iter;
                if( str_iter != str_iter_end && *str_iter == 'm' ) {
                    ++str_iter;
                    if( str_iter != str_iter_end && *str_iter == 'p' ) {
                        ++str_iter;
                        if( str_iter != str_iter_end && *str_iter == ';' ) {
                            return false;
                        }
                    }
                }
                break;
            case 'q':
                ++str_iter;
                if( str_iter != str_iter_end && *str_iter == 'u' ) {
                    ++str_iter;
                    if( str_iter != str_iter_end && *str_iter == 'o' ) {
                        ++str_iter;
                        if( str_iter != str_iter_end && *str_iter == 't' ) {
                            ++str_iter;
                            if( str_iter != str_iter_end && *str_iter == ';' ) {
                                return false;
                            }
                        }
                    }
                }
                break;
            default:
                return true;
        }
    }
    return true;
}

// see if the '<' opens an HTML tag (currently we 
// only check for a few kinds of tags )
//  ============================================================================
static bool
s_IsTagStart( 
    const string::const_iterator &str_iter,
    const string::const_iterator &str_iter_end )
//  ============================================================================
{
    static const string possible_tag_starts[] = {
        "<a href=",
        "<acronym title",
        "</a>",
        "</acronym"
    }; 
    static const size_t num_possible_tag_starts = 
        (sizeof(possible_tag_starts) / sizeof(possible_tag_starts[0]));

    // check every string it might start with
    for( int possible_str_idx = 0; possible_str_idx < num_possible_tag_starts; ++possible_str_idx ) {
        const string &expected_str = possible_tag_starts[possible_str_idx];

        string::size_type idx = 0;
        string::const_iterator check_str_iter = str_iter;
        for( ; check_str_iter != str_iter_end && idx < expected_str.length(); ++idx, ++check_str_iter ) {
            if( *check_str_iter != expected_str[idx] ) {
                break;
            }
        }

        if( idx == expected_str.length() ) {
            return true;
        }
    }

    // we're in a tag if we matched the whole expected_str
    return false;
}

//  ============================================================================
void
TryToSanitizeHtml( string &str )
//  ============================================================================
{
    string result;
    
    // The "* 1.1" should keep up efficient in most cases since data tends not to have
    // too many characters that need escaping.
    result.reserve( 1 + (int)( str.length() * 1.1 ) ); 

    // we only sanitize when we're not in an url
    bool in_html_tag = false;
    ITERATE( string, str_iter, str ) {
        // see if we're entering an HTML tag
        if( ! in_html_tag && *str_iter =='<' && s_IsTagStart(str_iter, str.end()) ) {
            in_html_tag = true;
        }

        // now that we know whether we're in a tag,
        // process characters appropriately.
        if( in_html_tag ) {
            switch( *str_iter ) {
            case '&':
                // make sure we're not "double-sanitizing"
                // (e.g. "&gt;" to "&amp;gt;")
                if( s_ShouldWeEscapeAmpersand(str_iter, str.end()) ) {
                    result += "&amp;";
                } else {
                    result += '&';
                }
                break;
            default:
                result += *str_iter;
                break;
            }
        } else {
            switch( *str_iter ) {
            case '<':
                result += "&lt;";
                break;
            case '>':
                result += "&gt;";
                break;
            default:
                result += *str_iter;
                break;
            }
        }

        // see if we're exiting an HTML tag
        if( in_html_tag && *str_iter == '>' ) {
            // tag is closed now
            // (Note: does this consider cases where '>' is in quotes?)
            in_html_tag = false;
        }
    }

    // swap is faster than assignment
    str.swap( result );
}

void 
TryToSanitizeHtmlList( std::list<std::string> &strs )
{
    NON_CONST_ITERATE( std::list<std::string>, str_iter, strs ) {
        TryToSanitizeHtml( *str_iter );
    }
}

bool 
CommentHasSuspiciousHtml( const string &str )
{
    // list is not complete, still need to take proper precautions
    static const string bad_html_strings[] = { 
        "<script", "<object", "<applet", "<embed", "<form", 
        "javascript:", "vbscript:"
    };

    // load matching fsa if not already done
    static CTextFsa fsa;
    if( ! fsa.IsPrimed() ) {
        int ii = 0;
        for( ; ii < sizeof(bad_html_strings)/sizeof(bad_html_strings[0]) ; ++ii ) {
            fsa.AddWord( bad_html_strings[ii] );
        }
        fsa.Prime();
    }

    // do the match
    int current_state = 0;
    for ( SIZE_TYPE str_idx = 0 ; str_idx < str.length(); ++str_idx) {
        const char ch = str[str_idx];
        int next_state = fsa.GetNextState (current_state, ch);
        if (fsa.IsMatchFound (next_state)) {
            return true;
        }
        current_state = next_state;
    }

    return false;
}


END_SCOPE(objects)
END_NCBI_SCOPE
