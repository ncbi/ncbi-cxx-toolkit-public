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

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

#define IS_LOWER(c)     ('a'<=(c) && (c)<='z')
#define IS_UPPER(c)     ('A'<=(c) && (c)<='Z')

using namespace sequence;

//  ----------------------------------------------------------------------------
//  File scope helper functions:
//  ----------------------------------------------------------------------------
bool s_IsAllUpperCase( const string& str )
{
    for(string::size_type i=0; i<str.length(); i++) {
        if( !IS_UPPER(str[i])) return false;
    }
    return true;
}

string s_NormalizeInitials( const string& raw_initials )
{
    //
    //  Note:
    //  Periods _only_ after CAPs to avoid decorating hyphens (which _are_ 
    //  legal in the "initials" part.
    //
    string normal_initials;
    for ( const char* p=raw_initials.c_str(); *p != 0; ++p ) {
        normal_initials += *p;
        if ( IS_UPPER(*p) ) {
            normal_initials += '.';
        }
    }
    return normal_initials;
}

string s_NormalizeSuffix( const string& raw_suffix )
{
    //
    //  Note: (2008-02-13) Suffixes I..VI no longer have trailing periods.
    //
    if ( raw_suffix == "1d" ) {
        return "1st";
    }
    if ( raw_suffix == "2d" ) {
        return "2nd";
    }
    if ( raw_suffix == "3d" ) {
        return "3rd";
    }
    if ( raw_suffix == "Sr" ) {
        return "Sr.";
    }
    if ( raw_suffix == "Jr" ) {
        return "Jr.";
    }
    
    return raw_suffix;
}

void s_SplitMLAuthorName(string name, string& last, string& initials, string& suffix)
{
    NStr::TruncateSpacesInPlace( name );
    if ( name.empty() ) {
        return;
    }

    vector<string> parts;
    NStr::Tokenize( name, " ", parts, NStr::eMergeDelims );
    if ( parts.empty() ) {
        return;
    }
    if ( parts.size() == 1 ) {
        //
        //  Designate the only part we have as the last name.
        //
        last = parts[0];
        return;
    }

    
    const string& last_part = parts[ parts.size()-1 ];
    const string& second_to_last_part = parts[ parts.size()-2 ];

    if ( parts.size() == 2 ) {
        //
        //  Designate the first part as the last name and the second part as the
        //  initials.
        //
        last = parts[0];
        initials = s_NormalizeInitials( last_part );
        return;
    }

    //
    //  At least three parts.
    //
    //  If the second to last part is all CAPs then those are the initials. The 
    //  last part is the suffix, and everything up to the initials is the last 
    //  name.
    //
    if ( s_IsAllUpperCase( second_to_last_part ) ) {
        last = NStr::Join( vector<string>( parts.begin(), parts.end()-2 ), " " );
        initials = s_NormalizeInitials( second_to_last_part );
        suffix = s_NormalizeSuffix( last_part );
        return;
    }

    //
    //  Fall through:
    //  Guess that the last part is the initials and everything leading up to it 
    //  is a (rather unusual) last name.
    //
    last = NStr::Join( vector<string>( parts.begin(), parts.end()-1 ), " " );
    initials = s_NormalizeInitials( last_part );
    return;

    //  ------------------------------------------------------------------------
    //  CASE NOT HANDLED:
    //
    //  (1) Initials with a blank in them. UNFIXABLE!
    //  (2) Initials with non CAPs in them. Probably fixable through a 
    //      white list of allowable exceptions. Tedious, better let the indexers
    //      fix it.
    //  ------------------------------------------------------------------------
}

bool CleanString(string& str, bool rm_trailing_period)
{
    size_t orig_slen = str.size();
    NStr::TruncateSpacesInPlace(str, NStr::eTrunc_Begin);
    size_t slen = 0;
    while (!str.empty()  &&  slen != str.size()) {
        slen = str.size();
        NStr::TruncateSpacesInPlace(str, NStr::eTrunc_End);
        RemoveTrailingJunk(str);
        if (rm_trailing_period) {
            RemoveTrailingPeriod(str);
        }
    }
    TrimInternalSemicolons(str);
    if (orig_slen != str.size()) {
        return true;
    }
    return false;
}

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

    bool changed = false;

    // if there's junk, chop it off (but leave period/tildes/ellipsis as appropriate)
    if ( start_of_junk_pos < (int)str.length() ) {

        // holds the suffix to add after we remove the junk
        const char * suffix = ""; // by default, just remove junk

        const int chars_in_junk = ( str.length() - start_of_junk_pos );
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

bool CleanStringList(list< string >& string_list)
{
    bool rval = false;
    list< string >::iterator it = string_list.begin();

    while (it != string_list.end()) {
        // trim leading spaces/unprintable characters, semicolons, and commas
        size_t start_junk_len = 0;
        const char *start = (*it).c_str();
        while (*start != 0 && (*start <= ' ' || *start == ';' || *start == ',')) {
            start_junk_len++;
            start++;
        }
        if (start_junk_len > 0) {
            (*it) = (*it).substr(start_junk_len);
            rval = true;
        }

        // trim trailing spaces/unprintable characters, commas.
        // trim trailing semicolons if they are not preceded by an ampersand
        // followed by characters greater than space
        size_t len_good = 0;
        size_t pos = 0;
        bool in_amp_phrase = false;
        const char *cp = (*it).c_str();
        while (*cp != 0) {
            if (*cp <= ' ' || *cp == ',') {
                // not ok for end
                in_amp_phrase = false;
            } else if (*cp == '&') {
                in_amp_phrase = true;
            } else if (*cp == ';') {
                // keep only if in ampersand phrase
                if (in_amp_phrase) {
                    len_good = pos + 1;
                }
                in_amp_phrase = false;
            } else {
                len_good = pos + 1;
            }
            pos++;
            cp++;
        }
        if (len_good < (*it).length()) {
            (*it) = (*it).substr(0, len_good);
            rval = true;
        }

        if (NStr::IsBlank (*it)) {
            it = string_list.erase(it);
        } else {
            // keep in list if not a duplicate of previous item
            list< string >::iterator it2 = string_list.begin();
            bool found = false;
            while (it2 != it && !found) {
                if (NStr::EqualCase (*it, *it2)) {
                    found = true;
                }
                ++it2;
            }
            if (found) {
                it = string_list.erase(it);
                rval = true;
            } else {
                ++it;
            }
        }
    }
    return rval;
}


bool RemoveTrailingPeriod(string& str)
{
    if (str[str.length() - 1] == '.') {
        bool remove = true;
        size_t period = str.length() - 1;
        size_t amp = str.find_last_of('&');
        if (amp != NPOS) {
            remove = false;
            for (size_t i = amp + 1; i < period; ++i) {
                if (isspace((unsigned char) str[i])) {
                    remove = true;
                    break;
                }
            }
        }
        if (remove) {
            str.resize(period);
            return true;
        }
    }
    return false;
}


bool RemoveTrailingJunk(string& str)
{
    SIZE_TYPE end_str =  str.find_last_not_of(" \t\n\r,~;");
    if (end_str == NPOS) {
        end_str = 0; // everything is junk.
    } else {
        ++end_str; // indexes the first character to remove.
    }
    if (end_str >= str.length()) {
        return false; // nothing to remove.
    }
    str.erase(end_str);
    return true;
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


bool OnlyPunctuation (string str)
{
    bool found_other = false;
    for (unsigned int offset = 0; offset < str.length() && ! found_other; offset++) {
        if (str[offset] > ' ' && str[offset] != '.' && str[offset] != ','
            && str[offset] != '~' && str[offset] != ';') {
            return false;
        }
    }
    return true;
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

CRef<CSeq_loc> ReadLocFromText(string text, const CSeq_id *id, CScope *scope)
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

char ValidAminoAcid (string abbrev)
{
    char ch = 'X';
    
    for (unsigned int k = 0; k < sizeof(abbreviation_list) / sizeof (ProteinAbbrevData); k++) {
        if (NStr::EqualNocase (abbrev, abbreviation_list[k].abbreviation)) {
            ch = abbreviation_list[k].letter;
            break;
        }
    }
    
    if (abbrev.length() == 1) {
        for (unsigned int k = 0; k < sizeof(abbreviation_list) / sizeof (ProteinAbbrevData); k++) {
            if (abbrev.c_str()[0] == abbreviation_list[k].letter) {
                ch = abbreviation_list[k].letter;
                break;
            }
        }
    }
    
    return ch;
}


bool AuthListsMatch(const CAuth_list::TNames& list1, const CAuth_list::TNames& list2, bool all_authors_match)
{
    if (list1.Which() != list2.Which()) {
        return false;
    } else if (list1.Which() == CAuth_list::TNames::e_Std) {
        if (all_authors_match) {
            if (list1.GetStd().size() != list2.GetStd().size()) {
                return false;
            } else {
                for ( CAuth_list::TNames::TStd::const_iterator it1 = (list1.GetStd()).begin(), 
                                                               it1_end = (list1.GetStd()).end(),
                                                               it2 = (list2.GetStd()).begin(),
                                                               it2_end = list2.GetStd().end();
                      it1 != it1_end && it2 != it2_end;
                      ++it1, ++it2) {
                    // each name must match, in order
                    string name1, name2;
                    name1.erase();
                    name2.erase();
                    (*it1)->GetName().GetLabel(&name1, CPerson_id::eGenbank);
                    (*it2)->GetName().GetLabel(&name2, CPerson_id::eGenbank);
                    if (!NStr::Equal(name1, name2)) {
                        return false;
                    }
                }
                return true;
            }
        } else {
            // first name must match
            string name1, name2;
            name1.erase();
            name2.erase();
            list1.GetStd().front()->GetName().GetLabel(&name1, CPerson_id::eGenbank);
            list2.GetStd().front()->GetName().GetLabel(&name1, CPerson_id::eGenbank);
            return NStr::Equal(name1, name2);
        }
    } else if (list1.Which() == CAuth_list::TNames::e_Ml) {
        if (all_authors_match) {
            if (list1.GetMl().size() != list2.GetMl().size()) {
                return false;
            } else {
                for ( CAuth_list::TNames::TMl::const_iterator it1 = (list1.GetMl()).begin(), 
                                                               it1_end = (list1.GetMl()).end(),
                                                               it2 = (list2.GetMl()).begin(),
                                                               it2_end = list2.GetMl().end();
                      it1 != it1_end && it2 != it2_end;
                      ++it1, ++it2) {
                    // each name must match, in order
                    if (!NStr::Equal((*it1), (*it2))) {
                        return false;
                    }
                }
                return true;
            }
        } else {
            // first name must match
            return NStr::Equal(list1.GetMl().front(), list2.GetMl().front());
        }
        
    } else if (list1.Which() == CAuth_list::TNames::e_Str) {
        if (all_authors_match) {
            if (list1.GetStr().size() != list2.GetStr().size()) {
                return false;
            } else {
                for ( CAuth_list::TNames::TStr::const_iterator it1 = (list1.GetStr()).begin(), 
                                                               it1_end = (list1.GetStr()).end(),
                                                               it2 = (list2.GetStr()).begin(),
                                                               it2_end = list2.GetStr().end();
                      it1 != it1_end && it2 != it2_end;
                      ++it1, ++it2) {
                    // each name must match, in order
                    if (!NStr::Equal((*it1), (*it2))) {
                        return false;
                    }
                }
                return true;
            }
        } else {
            // first name must match
            return NStr::Equal(list1.GetStr().front(), list2.GetStr().front());
        }
    } else {
        return false;
    } 
}


bool CitSubsMatch(const CCit_sub& sub1, const CCit_sub& sub2)
{
    // dates must match
    if (sub1.CanGetDate()) {
        if (sub2.CanGetDate()) {
            if (sub1.GetDate().Compare(sub2.GetDate()) != CDate::eCompare_same) {
                return false;
            }
        } else {
            return false;
        }
    } else if (sub2.CanGetDate()) {
        return false;
    }
    
    // descriptions must match
    if (sub1.CanGetDescr() && sub2.CanGetDescr()) {
        if (!NStr::Equal(sub1.GetDescr(), sub2.GetDescr())) {
            return false;
        }
    } else if (sub1.CanGetDescr() || sub2.CanGetDescr()) {
        // one has a description, the other does not
        return false;
    }
    
    // author lists must be set and must match
    // if both affiliations set, must match
    if (! sub1.IsSetAuthors() 
        || ! sub2.IsSetAuthors()
        || ! sub1.GetAuthors().IsSetNames()
        || ! sub2.GetAuthors().IsSetNames()
        || !AuthListsMatch (sub1.GetAuthors().GetNames(), sub2.GetAuthors().GetNames(), true)) {
        return false;
    } else if (sub1.GetAuthors().IsSetAffil() && sub2.GetAuthors().IsSetAffil()
               && !sub1.GetAuthors().GetAffil().Equals(sub2.GetAuthors().GetAffil())) {
        return false;
    } else {
        return true;
    }
    
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

CRef<CSeq_loc> MakeFullLengthLocation(CBioseq_Handle bh, CScope* scope, CRef<CSeq_loc> new_loc, bool &first)
{
    bool is_master_seq = false;

    // if this is the master sequence, add whole locations for each of the parts
    CSeq_entry_Handle seh = bh.GetParentEntry();
    if (seh) {
        seh = seh.GetParentEntry();
    }
    if (seh && seh.IsSet()) {
        CBioseq_set_Handle bsh = seh.GetSet();
        if (bsh.CanGetClass() && bsh.GetClass() == CBioseq_set::eClass_segset) {
            // this is the master sequence
            is_master_seq = true;
            // add whole loc for each part
            FOR_EACH_SEQENTRY_ON_SEQSET (it, *(bsh.GetCompleteBioseq_set())) {
                if ((*it)->IsSet()) {
                    const CBioseq_set& parts_set = (*it)->GetSet();
                    if (parts_set.CanGetClass() && parts_set.GetClass() == CBioseq_set::eClass_parts) {
                        FOR_EACH_SEQENTRY_ON_SEQSET (it2, parts_set) {
                            if ((*it2)->IsSeq()) {
                                new_loc = MakeFullLengthLocation (scope->GetBioseqHandle((*it2)->GetSeq()), scope, new_loc, first);
                            }
                        }
                    }
                }
            }
        }
    }
    if (!is_master_seq) {
        CRef <CSeq_loc> loc_part(new CSeq_loc);

        // Get best ID for location
        const CSeq_id* id = FindBestChoice(bh.GetBioseqCore()->GetId(), CSeq_id::BestRank);
        CRef <CSeq_id> new_id(new CSeq_id);
        new_id->Assign(*id);
        loc_part->SetInt().SetId(*new_id);
        loc_part->SetInt().SetFrom(0);
        loc_part->SetInt().SetTo(bh.GetInst_Length() - 1);
        if (first) {
            new_loc = loc_part;
            first = false;
        } else {
            CRef<CSeq_loc> tmp_loc;
            tmp_loc = sequence::Seq_loc_Add(*new_loc, *loc_part, 
                                            CSeq_loc::fMerge_Abutting, scope);
            new_loc = tmp_loc;
        }
    }
    return new_loc;
}

CRef<CSeq_loc> MakeFullLengthLocation(const CSeq_loc& loc, CScope* scope)
{
    // Create a location that covers the entire sequence.

    // if on segmented set, create location for each segment
    CRef<CSeq_loc> new_loc(new CSeq_loc);
    CSeq_loc_CI loc_it (loc);
    bool first = true;
    CBioseq_Handle last_bh;
    while (loc_it) {
        CBioseq_Handle bh = scope->GetBioseqHandle(loc_it.GetSeq_id());
        if (!first && bh == last_bh) {
            // skip - only one location per sequence 
        } else {
            new_loc = MakeFullLengthLocation (bh, scope, new_loc, first);
        }
        last_bh = bh;
        ++loc_it;
    }
    return new_loc;   
}


bool IsFeatureFullLength(const CSeq_feat& cf, CScope* scope)
{
    // Create a location that covers the entire sequence and do
    // a comparison.  Can't just check for the location type 
    // of the feature to be "whole" because an interval could
    // start at 0 and end at the end of the Bioseq.
    CRef<CSeq_loc> whole_loc = MakeFullLengthLocation (cf.GetLocation(), scope);

    if (sequence::Compare(*whole_loc, cf.GetLocation(), scope, sequence::fCompareOverlapping) == sequence::eSame) {
        return true;
    } else {
        return false;
    }
}


CBioSource::EGenome GenomeByOrganelle(string& organelle, bool strip, NStr::ECase use_case)
{
    string match = "";
    
    CBioSource::EGenome genome = CBioSource::GetGenomeByOrganelle (organelle, use_case, true);
    if (genome != CBioSource::eGenome_unknown) {
        match = CBioSource::GetOrganelleByGenome (genome);
        if (strip && !NStr::IsBlank(match)) {
            organelle = organelle.substr(match.length());
            NStr::TruncateSpacesInPlace(organelle);
        }
    }
        
    return genome;
}


bool IsmRNA(CBioseq_Handle bsh)
{
    bool is_mRNA = false;
    for (CSeqdesc_CI desc(bsh, CSeqdesc::e_Molinfo); desc && !is_mRNA; ++desc) {
        if (desc->GetMolinfo().CanGetBiomol()
            && desc->GetMolinfo().GetBiomol() == CMolInfo::eBiomol_mRNA) {
            is_mRNA = true;
        }
    }
    return is_mRNA;
}


bool IsmRNA(CBioseq_set_Handle bsh)
{
    bool is_mRNA = false;
    if (bsh.CanGetClass() && bsh.GetClass() == CBioseq_set::eClass_segset) {
        CSeq_entry_Handle seh = bsh.GetParentEntry();

        for (CSeqdesc_CI desc(seh, CSeqdesc::e_Molinfo, 1); desc && !is_mRNA; ++desc) {
            if (desc->GetMolinfo().CanGetBiomol()
                && desc->GetMolinfo().GetBiomol() == CMolInfo::eBiomol_mRNA) {
                is_mRNA = true;
            }
        }
    }
    return is_mRNA;
}


const CBioSource* GetAssociatedBioSource(CBioseq_set_Handle bh)
{
    CSeq_entry_Handle seh = bh.GetParentEntry();
    CSeqdesc_CI desc_ci (seh, CSeqdesc::e_Source, 1);

    if (desc_ci) {
        return &(desc_ci->GetSource());
    } else {
        seh = seh.GetParentEntry();
    
        if (seh && seh.IsSet()) {
            return GetAssociatedBioSource(seh.GetSet());
        }
    }
    return NULL;        
}


const CBioSource* GetAssociatedBioSource(CBioseq_Handle bh)
{
    CSeqdesc_CI desc_ci (bh, CSeqdesc::e_Source, 1);

    if (desc_ci) {
        return &(desc_ci->GetSource());
    } else {
        CSeq_entry_Handle seh = bh.GetParentEntry();
        seh = seh.GetParentEntry();
    
        if (seh && seh.IsSet()) {
            return GetAssociatedBioSource(seh.GetSet());
        }
    }
    return NULL;
}


bool IsArtificialSyntheticConstruct (const CBioSource *bsrc)
{
    if (bsrc 
        && bsrc->CanGetOrigin() && bsrc->GetOrigin() == CBioSource::eOrigin_artificial
        && bsrc->CanGetOrg() && bsrc->GetOrg().CanGetTaxname() && NStr::EqualNocase (bsrc->GetOrg().GetTaxname(), "synthetic construct")) {
        return true;
    } else {
        return false;
    }
}


bool IsArtificialSyntheticConstruct (CBioseq_Handle bsh)
{
    return IsArtificialSyntheticConstruct (GetAssociatedBioSource(bsh));
}


bool IsArtificialSyntheticConstruct (CBioseq_set_Handle bsh)
{
    CSeq_entry_Handle seh = bsh.GetParentEntry();
    CSeqdesc_CI desc_ci (seh, CSeqdesc::e_Source);
    while (desc_ci) {
        if (IsArtificialSyntheticConstruct (&(desc_ci->GetSource()))) {
            return true;
        }
        ++desc_ci;
    }
    
    return IsArtificialSyntheticConstruct (GetAssociatedBioSource(bsh));
}

CRef<CAuthor> ConvertMltoSTD( const string& token)
{
    string last, initials, suffix;
    s_SplitMLAuthorName(token, last, initials, suffix);

    if ( ! last.empty() ) {
        CRef < CAuthor > au(new CAuthor); 
        au->SetName().SetName().SetLast(last);
        if(initials.size()) {
            au->SetName().SetName().SetInitials(initials);
        }
        if(suffix.size()) {
            au->SetName().SetName().SetSuffix(suffix);
        }
        return au;
    }
    return CRef<CAuthor>( 0 );
}


bool ConvertAuthorContainerMlToStd( CAuth_list& authors )
{
    CAuth_list::C_Names* names = new CAuth_list::C_Names;
    CAuth_list::C_Names::TStd& names_std = names->SetStd();
    NON_CONST_ITERATE( CAuth_list::C_Names::TMl, author, authors.SetNames().SetMl() ) {
        names_std.push_back( ConvertMltoSTD( *author ) );
    }
    authors.SetNames( *names );
    return true;
}


END_SCOPE(objects)
END_NCBI_SCOPE
