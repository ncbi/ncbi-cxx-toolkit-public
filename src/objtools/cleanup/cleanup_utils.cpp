/* $Id$
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
 * Author:  Mati Shomrat
 *
 * File Description:
 *   General utilities for data cleanup.
 *
 * ===========================================================================
 */
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include "cleanup_utils.hpp"

#include <objmgr/util/seq_loc_util.hpp>
#include <objmgr/util/sequence.hpp>
#include <objects/seq/Pubdesc.hpp>
#include <objects/pub/Pub_equiv.hpp>
#include <objects/pub/Pub.hpp>
#include <objects/biblio/Cit_sub.hpp>
#include <objects/biblio/Cit_gen.hpp>
#include <objects/biblio/Auth_list.hpp>
#include <objects/biblio/Affil.hpp>
#include <objects/biblio/Author.hpp>
#include <objects/biblio/Imprint.hpp>
#include <objects/general/Date.hpp>
#include <objects/general/Person_id.hpp>
#include <objects/general/Name_std.hpp>

#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/MolInfo.hpp>
#include <objects/seq/seq_loc_from_string.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/misc/sequence_macros.hpp>

#include <objmgr/seqdesc_ci.hpp>

#include <objtools/cleanup/cleanup_pub.hpp>
#include <util/regexp/ctre/ctre.hpp>
#include <util/regexp/ctre/replace.hpp>
#include <util/sequtil/sequtil_manip.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

#define IS_LOWER(c)     ('a'<=(c) && (c)<='z')
#define IS_UPPER(c)     ('A'<=(c) && (c)<='Z')

using namespace sequence;

bool CleanVisString( string &str )
{
    bool changed = false;

    if( str.empty() ) {
        return false;
    }

    // chop off initial junk
    {
        string::size_type first_good_char_pos = str.find_first_not_of(" ;,");
        if( first_good_char_pos == string::npos ) {
            // string is completely junk
            str.clear();
            return true;
        } else if( first_good_char_pos > 0 ) {
            copy( str.begin() + first_good_char_pos, str.end(), str.begin() );
            str.resize( str.length() - first_good_char_pos );
            changed = true;
        }
    }

    // chop off end junk

    string::size_type last_good_char_pos = str.find_last_not_of(" ;,");
    _ASSERT( last_good_char_pos != string::npos ); // we checked this case so it shouldn't happen
    if( last_good_char_pos == (str.length() - 1) ) {
        // nothing to chop of the end
        return changed;
    } else if( str[last_good_char_pos+1] == ';' ) {
        // special extra logic for semicolons because it might be part of
        // an HTML character like "&nbsp;"

        // see if there's a '&' before the semicolon
        // ( ' ' and ',' would break the '&' and make it irrelevant, though )
        string::size_type last_ampersand_pos = str.find_last_of("& ,", last_good_char_pos );
        if( last_ampersand_pos == string::npos ) {
            // no ampersand, so just chop off as normal
            str.resize( last_good_char_pos + 1 );
            return true;
        }
        switch( str[last_ampersand_pos] ) {
            case '&':
                // can't chop semicolon, so chop just after it
                if( (last_good_char_pos + 2) == str.length() ) {
                    // semicolon is at end, so no chopping occurs
                    return changed;
                } else {
                    // chop after semicolon
                    str.resize( last_good_char_pos + 2 );
                    return true;
                }
            case ' ':
            case ',':
                // ampersand (if any) is irrelevant due to intervening
                // space or comma
                str.resize( last_good_char_pos + 1 );
                return true;
            default:
                _ASSERT(false);
                return changed;  // should be impossible to reach here
        }

    } else {
        str.resize( last_good_char_pos + 1 );
        return true;
    }
}

bool CleanVisStringJunk( string &str, bool allow_ellipses )
{
    // This is based on the C function TrimSpacesAndJunkFromEnds.
    // Although it's updated to use iterators and such and to
    // return whether it changed the string, it should
    // have the same output.

    // TODO: This function is copy-pasted from TrimSpacesAndJunkFromEnds,
    // so we should do something about that since duplicate code is evil.

    if ( str.empty() ) {
        return false;
    }

    // make start_of_junk_pos hold the beginning of the "junk" at the end
    // (where junk is defined as one of several characters)
    // while we're at it, also check if the junk contains a tilde and/or period
    bool isPeriod = false;
    bool isTilde = false;
    int start_of_junk_pos = (int)str.length() - 1;
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

    bool changed = false;

    // if there's junk, chop it off (but leave period/tildes/ellipsis as appropriate)
    if ( start_of_junk_pos < (int)str.length() ) {

        // holds the suffix to add after we remove the junk
        const char * suffix = ""; // by default, just remove junk

        const int chars_in_junk = ( (int)str.length() - start_of_junk_pos );
        _ASSERT( chars_in_junk >= 1 );
        // allow one period at end
        if (isPeriod) {
            suffix = ".";
            if ( allow_ellipses && (chars_in_junk >= 3) &&
                str[start_of_junk_pos+1] == '.' && str[start_of_junk_pos+2] == '.' ) {
                suffix = "...";
            }
        } else if (isTilde ) {
            // allow double tilde(s) at the end
            if ( str[start_of_junk_pos] == '~' ) {
                const bool doubleTilde = ( (chars_in_junk >= 2) && str[start_of_junk_pos+1] == '~' );
                suffix = ( doubleTilde  ? "~~" : "" );
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


bool  RemoveSpacesBetweenTildes(string& str)
{
    static string whites(" \t\n\r");
    bool changed = false;
    SIZE_TYPE tilde1 = str.find('~');
    if (tilde1 == NPOS) {
        return changed; // no tildes in str.
    }
    SIZE_TYPE tilde2 = str.find_first_not_of(whites, tilde1 + 1);
    while (tilde2 != NPOS) {
        if (str[tilde2] == '~') {
            if ( tilde2 > tilde1 + 1) {
                // found two tildes with only spaces between them.
                str.erase(tilde1+1, tilde2 - tilde1 - 1);
                ++tilde1;
                changed = true;
            } else {
                // found two tildes side by side.
                tilde1 = tilde2;
            }
        } else {
            // found a tilde with non-space non-tilde after it.
            tilde1 = str.find('~', tilde2 + 1);
            if (tilde1 == NPOS) {
                return changed; // no more tildes in str.
            }
        }
        tilde2 = str.find_first_not_of(whites, tilde1 + 1);
    }
    return changed;

}


bool CleanDoubleQuote(string& str)
{
    bool changed = false;
    NON_CONST_ITERATE(string, it, str) {
        if (*it == '\"') {
            *it = '\'';
            changed = true;
        }
    }
    return changed;
}


void TrimInternalSemicolons (string& str)
{
    size_t pos, next_pos;

    pos = NStr::Find (str, ";");
    while (pos != string::npos) {
        next_pos = pos + 1;
        bool has_space = false;
        while (next_pos < str.length() && (str[next_pos] == ';' || str[next_pos] == ' ' || str[next_pos] == '\t')) {
            if (str[next_pos] == ' ') {
                has_space = true;
            }
            next_pos++;
        }
        if (next_pos == pos + 1 || (has_space && next_pos == pos + 2)) {
            // nothing to fix, advance semicolon search
            pos = NStr::Find (str, ";", next_pos);
        } else if (next_pos == str.length()) {
            // nothing but semicolons, spaces, and tabs from here to the end of the string
            // just truncate it
            str = str.substr(0, pos);
            pos = string::npos;
        } else {
            if (has_space) {
                str = str.substr(0, pos + 1) + " " + str.substr(next_pos);
            } else {
                str = str.substr(0, pos + 1) + str.substr(next_pos);
            }
            pos = NStr::Find (str, ";", pos + 1);
        }
    }
}

#define twocommas ((',') << 8 | (','))
#define twospaces ((' ') << 8 | (' '))
#define twosemicolons ((';') << 8 | (';'))
#define space_comma ((' ') << 8 | (','))
#define space_bracket ((' ') << 8 | (')'))
#define bracket_space (('(') << 8 | (' '))
#define space_semicolon ((' ') << 8 | (';'))
#define comma_space ((',') << 8 | (' '))
#define semicolon_space ((';') << 8 | (' '))

bool Asn2gnbkCompressSpaces(string& val)
{
    if (val.length() == 0) return false;

    char * str = new char[sizeof(char) * (val.length() + 1)];
    strcpy(str, val.c_str());

    char     ch;
    char *   dst;
    char *   ptr;

    char     curr;
    char     next;
    char *   in;
    char *   out;
    unsigned short   two_chars;


    in = str;
    out = str;

    curr = *in;
    in++;

    two_chars = curr;

    while (curr != '\0') {
        next = *in;
        in++;

        two_chars = (two_chars << 8) | next;

        if (two_chars == twocommas) {
            *out++ = curr;
            next = ' ';
            two_chars = next;
        }
        else if (two_chars == twospaces) {
        }
        else if (two_chars == twosemicolons) {
        }
        else if (two_chars == bracket_space) {
            next = curr;
            two_chars = curr;
        }
        else if (two_chars == space_bracket) {
        }
        else if (two_chars == space_comma) {
            *out++ = next;
            next = curr;
            *out++ = ' ';
            while (next == ' ' || next == ',') {
                next = *in;
                in++;
            }
            two_chars = next;
        }
        else if (two_chars == space_semicolon) {
            *out++ = next;
            next = curr;
            *out++ = ' ';
            while (next == ' ' || next == ';') {
                next = *in;
                in++;
            }
            two_chars = next;
        }
        else if (two_chars == comma_space) {
            *out++ = curr;
            *out++ = ' ';
            while (next == ' ' || next == ',') {
                next = *in;
                in++;
            }
            two_chars = next;
        }
        else if (two_chars == semicolon_space) {
            *out++ = curr;
            *out++ = ' ';
            while (next == ' ' || next == ';') {
                next = *in;
                in++;
            }
            two_chars = next;
        }
        else {
            *out++ = curr;
        }

        curr = next;
    }

    *out = '\0';

    /* TrimSpacesAroundString but allow leading/trailing tabs/newlines */

    if (str[0] != '\0') {
        dst = str;
        ptr = str;
        ch = *ptr;
        while (ch == ' ') {
            ptr++;
            ch = *ptr;
        }
        while (ch != '\0') {
            *dst = ch;
            dst++;
            ptr++;
            ch = *ptr;
        }
        *dst = '\0';
        dst = nullptr;
        ptr = str;
        ch = *ptr;
        while (ch != '\0') {
            if (ch != ' ') {
                dst = nullptr;
            }
            else if (!dst) {
                dst = ptr;
            }
            ptr++;
            ch = *ptr;
        }
        if (dst) {
            *dst = '\0';
        }
    }
    string new_val;
    new_val = str;
    delete[] str;

    if (!NStr::Equal(val, new_val)) {
#ifdef _DEBUG
#if 0
        printf("Use new string\n");
#endif
#endif
        val = new_val;
        return true;
    }
    else {
        return false;
    }
}

bool TrimSpacesSemicolonsAndCommas(string& val)
{
    if (val.length() == 0) return false;

    char * str = new char[sizeof(char) * (val.length() + 1)];
    strcpy(str, val.c_str());

    char *  amp;
    unsigned char    ch;    /* to use 8bit characters in multibyte languages */
    char *  dst;
    char *  ptr;

    dst = str;
    ptr = str;
    ch = *ptr;
    if (ch != '\0' && (ch <= ' ' || ch == ';' || ch == ',')) {
        while (ch != '\0' && (ch <= ' ' || ch == ';' || ch == ',')) {
            ptr++;
            ch = *ptr;
        }
        while (ch != '\0') {
            *dst = ch;
            dst++;
            ptr++;
            ch = *ptr;
        }
        *dst = '\0';
    }
    amp = nullptr;
    dst = nullptr;
    ptr = str;
    ch = *ptr;
    while (ch != '\0') {
        if (ch == '&') {
            amp = ptr;
            dst = nullptr;
        }
        else if (ch <= ' ') {
            if (!dst) {
                dst = ptr;
            }
            amp = nullptr;
        }
        else if (ch == ';') {
            if (!dst && !amp) {
                dst = ptr;
            }
        }
        else if (ch == ',') {
            if (!dst) {
                dst = ptr;
            }
            amp = nullptr;
        }
        else {
            dst = nullptr;
        }
        ptr++;
        ch = *ptr;
    }
    if (dst) {
        *dst = '\0';
    }

    string new_val;
    new_val = str;
    delete[] str;

    if (!NStr::Equal(val, new_val)) {
#ifdef _DEBUG
#if 0
        printf("Use new string\n");
#endif
#endif
        val = new_val;
        return true;
    }
    else {
        return false;
    }
}


bool RemoveSpaces(string& str)
{
    if (str.empty()) {
        return false;
    }

    size_t next = 0;

    NON_CONST_ITERATE(string, it, str) {
        if (!isspace((unsigned char)(*it))) {
            str[next++] = *it;
        }
    }
    if (next < str.length()) {
        str.resize(next);
        return true;
    }
    return false;
}

class CGetSeqLocFromStringHelper_ReadLocFromText : public CGetSeqLocFromStringHelper {
public:
    CGetSeqLocFromStringHelper_ReadLocFromText( CScope *scope )
        : m_scope(scope) { }

    virtual CRef<CSeq_loc> Seq_loc_Add(
        const CSeq_loc&    loc1,
        const CSeq_loc&    loc2,
        CSeq_loc::TOpFlags flags )
    {
        return sequence::Seq_loc_Add( loc1, loc2, flags, m_scope );
    }

private:
    CScope *m_scope;
};

CRef<CSeq_loc> ReadLocFromText(const string& text, const CSeq_id *id, CScope *scope)
{
    CGetSeqLocFromStringHelper_ReadLocFromText helper(scope);
    return GetSeqLocFromString(text, id, &helper);
}

typedef struct proteinabbrev {
     string abbreviation;
    char letter;
} ProteinAbbrevData;

static ProteinAbbrevData abbreviation_list[] =
{
    {"Ala", 'A'},
    {"Asx", 'B'},
    {"Cys", 'C'},
    {"Asp", 'D'},
    {"Glu", 'E'},
    {"Phe", 'F'},
    {"Gly", 'G'},
    {"His", 'H'},
    {"Ile", 'I'},
    {"Xle", 'J'},  /* was - notice no 'J', breaks naive meaning of index -Karl */
    {"Lys", 'K'},
    {"Leu", 'L'},
    {"Met", 'M'},
    {"Asn", 'N'},
    {"Pyl", 'O'},  /* was - no 'O' */
    {"Pro", 'P'},
    {"Gln", 'Q'},
    {"Arg", 'R'},
    {"Ser", 'S'},
    {"Thr", 'T'},
    {"Val", 'V'},
    {"Trp", 'W'},
    {"Sec", 'U'}, /* was - not in iupacaa */
    {"Xxx", 'X'},
    {"Tyr", 'Y'},
    {"Glx", 'Z'},
    {"TERM", '*'}, /* not in iupacaa */ /*changed by Tatiana 06.07.95?`*/
    {"OTHER", 'X'}
};

// Find the single-letter abbreviation for either the single letter abbreviation
// or three-letter abbreviation.
// Use X if the abbreviation is not found.

char x_ValidAminoAcid(string_view abbrev)
{
    if (abbrev.length() >= 3) {
        for (unsigned k = 0; k < ArraySize(abbreviation_list); ++k) {
            if (NStr::EqualNocase(abbrev, abbreviation_list[k].abbreviation)) {
                return abbreviation_list[k].letter;
            }
        }
    }

    if (abbrev.length() == 1) {
        for (unsigned k = 0; k < ArraySize(abbreviation_list); ++k) {
            if (abbrev[0] == abbreviation_list[k].letter) {
                return abbreviation_list[k].letter;
            }
        }
    }

    return 'X';
}


bool s_DbtagCompare (const CRef<CDbtag>& dbt1, const CRef<CDbtag>& dbt2)
{
    // is dbt1 < dbt2
    return dbt1->Compare(*dbt2) < 0;
}


bool s_DbtagEqual (const CRef<CDbtag>& dbt1, const CRef<CDbtag>& dbt2)
{
    // is dbt1 == dbt2
    return dbt1->Compare(*dbt2) == 0;
}

bool s_OrgrefSynCompare( const string & syn1, const string & syn2 )
{
    return NStr::CompareNocase(syn1, syn2) < 0;
}

bool s_OrgrefSynEqual( const string & syn1, const string & syn2 )
{
    return NStr::EqualNocase(syn1, syn2);
}
 

bool CCleanupUtils::IsAllDigits(const string& str)
{
    if (str.length() == 0) {
        return false;
    }
    bool all_digits = true;
    ITERATE(string, s, str) {
        if (!isdigit(*s)) {
            all_digits = false;
            break;
        }
    }
    return all_digits;
}

class CCharInSet {
public:
    CCharInSet( const string &list_of_characters ) {
        copy( list_of_characters.begin(), list_of_characters.end(),
            inserter( char_set, char_set.begin() ) );
    }

    bool operator()( const char ch ) const {
        return ( char_set.find(ch) != char_set.end() );
    }

private:
    set<char> char_set;
};


static
void s_TokenizeTRnaString (const string &tRNA_string, list<string> &out_string_list )
{
    out_string_list.clear();
    if ( tRNA_string.empty() ) return;

    // SGD Tx(NNN)c or Tx(NNN)c#, where x is the amino acid, c is the chromosome (A-P, Q for mito),
    // and optional # is presumably for individual tRNAs with different anticodons and the same
    // amino acid.
    if (ctre::match<"[Tt][A-Za-z]\\(...\\)[A-Za-z]\\d?\\d?">(tRNA_string)) {
        // parse SGD tRNA anticodon
        out_string_list.push_back(kEmptyStr);
        string &new_SGD_tRNA_anticodon = out_string_list.back();
        string raw_codon_part = tRNA_string.substr(3,3);
        NStr::ToUpper( raw_codon_part );
        string reverse_complement;
        CSeqManip::ReverseComplement( raw_codon_part, CSeqUtil::e_Iupacna, 0, 3, reverse_complement );
        new_SGD_tRNA_anticodon = string("(") + reverse_complement + ')';

        // parse SGD tRNA amino acid
        out_string_list.push_back(tRNA_string.substr(1,1));
        return;
    }

    string tRNA_string_copy = tRNA_string;
    // Note that we do NOT remove "*", since it might be a terminator tRNA symbol
    replace_if( tRNA_string_copy.begin(), tRNA_string_copy.end(),
        CCharInSet("-,;:()=\'_~"), ' ' );

    vector<string> tRNA_tokens;
    // " \t\n\v\f\r" are the standard whitespace chars
    // ( source: http://www.cplusplus.com/reference/clibrary/cctype/isspace/ )
    NStr::Split(tRNA_string_copy, " \t\n\v\f\r", tRNA_tokens, NStr::fSplit_MergeDelimiters | NStr::fSplit_Truncate);

    EDIT_EACH_STRING_IN_VECTOR( tRNA_token_iter, tRNA_tokens ) {
        string &tRNA_token = *tRNA_token_iter;
        // remove initial "tRNA", if any
        if ( NStr::StartsWith(tRNA_token, "tRNA", NStr::eNocase) ) {
            tRNA_token = tRNA_token.substr(4);
        }

        if (! tRNA_token.empty() ) {
            if (ctre::match<"[A-Za-z][A-Za-z][A-Za-z]\\d*">(tRNA_token)) {
                tRNA_token = tRNA_token.substr(0, 3);
            }
            out_string_list.push_back(tRNA_token);
        }
    }
}

typedef SStaticPair<const char *, int> TTrnaKey;

static constexpr TTrnaKey trna_key_to_subtype [] = {
    {  "Ala",            'A'  },
    {  "Alanine",        'A'  },
    {  "Arg",            'R'  },
    {  "Arginine",       'R'  },
    {  "Asn",            'N'  },
    {  "Asp",            'D'  },
    {  "Asp or Asn",     'B'  },
    {  "Asparagine",     'N'  },
    {  "Aspartate",      'D'  },
    { "Aspartic",        'D'  },
    { "Aspartic Acid",   'D'  },
    {  "Asx",            'B'  },
    {  "Cys",            'C'  },
    {  "Cysteine",       'C'  },
    {  "fMet",           'M'  },
    {  "Gln",            'Q'  },
    {  "Glu",            'E'  },
    {  "Glu or Gln",     'Z'  },
    {  "Glutamate",      'E'  },
    {  "Glutamic",       'E'  },
    {  "Glutamic Acid",  'E'  },
    {  "Glutamine",      'Q'  },
    {  "Glx",            'Z'  },
    {  "Gly",            'G'  },
    {  "Glycine",        'G'  },
    {  "His",            'H'  },
    {  "Histidine",      'H'  },
    {  "Ile",            'I'  },
    {  "Ile2",           'I'  },
    {  "iMet",           'M'  },
    {  "Isoleucine",     'I'  },
    {  "Leu",            'L'  },
    {  "Leu or Ile",     'J'  },
    {  "Leucine",        'L'  },
    {  "Lys",            'K'  },
    {  "Lysine",         'K'  },
    {  "Met",            'M'  },
    {  "Methionine",     'M'  },
    {  "OTHER",          'X'  },
    {  "Phe",            'F'  },
    {  "Phenylalanine",  'F'  },
    {  "Pro",            'P'  },
    {  "Proline",        'P'  },
    {  "Pyl",            'O'  },
    {  "Pyrrolysine",    'O'  },
    {  "Sec",            'U'  },
    {  "Selenocysteine", 'U'  },
    {  "Ser",            'S'  },
    {  "Serine",         'S'  },
    {  "Ter",            '*'  },
    {  "TERM",           '*'  },
    {  "Termination",    '*'  },
    {  "Thr",            'T'  },
    {  "Threonine",      'T'  },
    {  "Trp",            'W'  },
    {  "Tryptophan",     'W'  },
    {  "Tyr",            'Y'  },
    {  "Tyrosine",       'Y'  },
    {  "Val",            'V'  },
    {  "Valine",         'V'  },
    {  "Xle",            'J'  },
    {  "Xxx",            'X'  }
};


typedef CStaticPairArrayMap <const char*, int, PNocase_CStr> TTrnaMap;
DEFINE_STATIC_ARRAY_MAP(TTrnaMap, sm_TrnaKeys, trna_key_to_subtype);

/*
class PNocase_LessChar
{
public:
    bool operator()( const char ch1, const char ch2 ) const {
        return toupper(ch1) < toupper(ch2);
    }
};
*/

// This maps in the opposite direction of sm_TrnaKeys
class CAminoAcidCharToSymbol : public multimap<char, const char*, PNocase_LessChar>
{
public:
    CAminoAcidCharToSymbol( const TTrnaKey keys[], size_t num_keys )
    {
        for(size_t ii=0; ii < num_keys; ++ii ) {
            insert(value_type( keys[ii].second, keys[ii].first ));
        }
    }
};

// Replace this function eventually
const multimap<char, const char*, PNocase_LessChar>& g_GetTrnaInverseKeys()
{
    static auto trna_inverse_keys =
        CAminoAcidCharToSymbol(trna_key_to_subtype, 
            (sizeof(trna_key_to_subtype) / sizeof(trna_key_to_subtype[0])) );

    return trna_inverse_keys;
}


static
char s_FindTrnaAA( const string &str )
{
    if ( str.empty() ) return '\0';
    string tmp = str;
    NStr::TruncateSpacesInPlace(tmp);

    if( tmp.length() == 1 ) {
        // if the string is a valid one-letter code, just return that
        const auto& inverse_keys = g_GetTrnaInverseKeys();
        const char aminoAcidLetter = toupper(tmp[0]);
        if( inverse_keys.find(aminoAcidLetter) != inverse_keys.end() ) {
            return aminoAcidLetter;
        }
    } else {
        // translate 3-letter codes and full-names to one-letter codes
        TTrnaMap::const_iterator trna_iter = sm_TrnaKeys.find (tmp.c_str ());
        if( trna_iter != sm_TrnaKeys.end() ) {
            return trna_iter->second;
        }
    }

    return '\0';
}


optional<int> CCleanupUtils::GetIupacAa(const string& key) {
    if (auto it = sm_TrnaKeys.find(key.c_str()); it != sm_TrnaKeys.end()) {
        return it->second;
    }

    return {};
}

// based on C's ParseTRnaString
char CCleanupUtils::ParseSeqFeatTRnaString( const string &comment, bool *out_justTrnaText, string &tRNA_codon, bool noSingleLetter )
{
    if (out_justTrnaText) {
        *out_justTrnaText = false;
    }
    tRNA_codon.clear();

    if ( comment.empty() ) return '\0';

    CRef<CTrna_ext> tr( new CTrna_ext );

    char aa = '\0';
    list<string> head;
    s_TokenizeTRnaString (comment, head);
    bool justt = true;
    list<string>::const_iterator head_iter = head.begin();
    bool is_ambig = false;
    for( ; head_iter != head.end(); ++head_iter ) {
        const string &str = *head_iter;
        if( str.empty() ) continue;
        char curraa = '\0';
        if (noSingleLetter && str.length() == 1) {
            curraa = '\0';
        } else {
            curraa = s_FindTrnaAA (str);
        }
        if(curraa != '\0') {
            if (aa == '\0') {
                aa = curraa;
            } else if( curraa != aa) {
                is_ambig = true;
            }
        } else if ( ! NStr::EqualNocase ("tRNA", str) &&
            ! NStr::EqualNocase ("transfer", str) &&
            ! NStr::EqualNocase ("RNA", str) &&
            ! NStr::EqualNocase ("product", str) )
        {
            justt = false;
        }
    }
    if( is_ambig ) {
        aa = 0;
    }

    if (justt) {
        if( comment.find_first_of("0123456789") != string::npos ) {
            justt = false;
        }
    }
    if (out_justTrnaText) {
        *out_justTrnaText = justt;
    }
    return aa;
}




END_SCOPE(objects)
END_NCBI_SCOPE
