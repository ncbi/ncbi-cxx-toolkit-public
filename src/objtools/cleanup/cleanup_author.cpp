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
 * Author:  Colleen Bollin
 *
 * File Description:
 *   Code for cleaning up CAuthor
 *
 */
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <serial/serialbase.hpp>

#include <objects/biblio/Author.hpp>
#include <objects/general/Name_std.hpp>
#include <objects/general/Person_id.hpp>

#include <objtools/cleanup/cleanup.hpp>
#include "cleanup_utils.hpp"


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


bool CCleanup::CleanupAuthor(CAuthor& author, bool fix_initials)
{
    bool any_change = false;
    size_t n;
    if (author.IsSetName()) {
        switch (author.GetName().Which()) {
        case CPerson_id::e_Name:
            any_change = s_CleanupNameStdBC(author.SetName().SetName(), fix_initials);
            break;
        case CPerson_id::e_Ml:
            n = author.GetName().GetMl().size();
            NStr::TruncateSpacesInPlace(author.SetName().SetMl());
            if (n != author.GetName().GetMl().size() ) { 
                any_change = true;
            }
            if (NStr::IsBlank(author.GetName().GetMl())) {
                author.ResetName();
            }
            break;
        case CPerson_id::e_Str:
            n = author.GetName().GetStr().size();
            NStr::TruncateSpacesInPlace(author.SetName().SetStr());
            if (n != author.GetName().GetStr().size() ) { 
                any_change = true;
            }
            if (NStr::IsBlank(author.GetName().GetStr())) {
                author.ResetName();
            }
            break;
        case CPerson_id::e_Consortium:
            n = author.GetName().GetConsortium().size();
            NStr::TruncateSpacesInPlace(author.SetName().SetConsortium());
            if (n != author.GetName().GetConsortium().size() ) { 
                any_change = true;
            }
            if (NStr::IsBlank(author.GetName().GetConsortium())) {
                author.ResetName();
            }
            break;
        default:
            break;
        }
    }
    return any_change;
}


bool CCleanup::s_CleanupNameStdBC ( CName_std& name, bool fix_initials )
{
    bool any_change = false;
    // there's a lot of shuffling around (e.g. adding and removing
    // periods in initials), so we can't determine
    // if we've actually changed anything until we get to the end of 
    // this function.
    CRef<CName_std> original_name( new CName_std );
    original_name->Assign( name );

    // if initials starts with uppercase, we remember to 
    // upcase the whole thing later
    bool upcaseinits = false;
    if( name.IsSetInitials() && isupper( name.GetInitials()[0] ) ) {
        upcaseinits = true;
    }

    string first_initials;
    string sDot(".");
    if ( ! name.IsSetSuffix() && name.IsSetInitials() ) {
        s_ExtractSuffixFromInitials(name);
    }

    if (name.IsSetFirst()) {
        size_t n = name.GetFirst().size();
        NStr::TruncateSpacesInPlace(name.SetFirst());
        if (NStr::IsBlank(name.GetFirst())) {
            name.ResetFirst();
        }
    }
    
    if( name.IsSetInitials() ) {
        NStr::ReplaceInPlace( name.SetInitials(), sDot, kEmptyStr);
        NStr::TruncateSpacesInPlace( name.SetInitials(), NStr::eTrunc_Begin );
    }
    if( name.IsSetLast() ) {
        NStr::TruncateSpacesInPlace( name.SetLast(), NStr::eTrunc_Begin );
    }
    if( name.IsSetMiddle() ) {
        NStr::TruncateSpacesInPlace( name.SetMiddle(), NStr::eTrunc_Begin );
    }
    s_FixEtAl( name );

    // extract initials from first name
    // like in C: FirstNameToInitials (first, first_initials, sizeof (first_initials) - 1);

    if ( name.IsSetFirst() ) {
        const string &first = name.GetFirst();
        string::size_type next_pos = 0;
        while ( next_pos < first.length() ) {
            // skip initial spaces and hyphens
            next_pos = first.find_first_not_of(" -", next_pos);
            if( string::npos == next_pos ) break;
            // if we hit an letter after that, copy the letter to inits
            if( isalpha( first[next_pos] ) ) {
                first_initials += first[next_pos];
            }
            // find next space or hyphen
            next_pos = first.find_first_of(" -", next_pos);
            if( string::npos == next_pos ) break;
            // if it's a hyphen, copy it
            if( first[next_pos] == '-' ) {
                first_initials += '-';
            }
        }
    }
    
    // continue extracting initials from middle name, but only if existing Initials field is empty
    if ((!name.IsSetInitials() || NStr::IsBlank(name.GetInitials()))
        && name.IsSetMiddle()) {
        const string &middle = name.GetMiddle();
        string::size_type next_pos = 0;
        while ( next_pos < middle.length() ) {
            // skip initial spaces and hyphens
            next_pos = middle.find_first_not_of(" -", next_pos);
            if( string::npos == next_pos ) break;
            // if we hit an letter after that, copy the  to inits
            if( isalpha( middle[next_pos] ) ) {
                first_initials += middle[next_pos];
            }
            // find next space or hyphen
            next_pos = middle.find_first_of(" -", next_pos);
            if( string::npos == next_pos ) break;
            // if it's a hyphen, copy it
            if( middle[next_pos] == '-' ) {
                first_initials += '-';
            }
        }
    }

    if (!name.IsSetInitials() || NStr::IsBlank(name.GetInitials())) {
        if (!first_initials.empty()) {
            name.SetInitials(first_initials);
            first_initials.clear();
        }
    } else if (fix_initials) {
        string & initials = name.SetInitials();

        // skip part of initials that matches first_initials
        string::size_type initials_first_good_idx = 0;
        for( ; initials_first_good_idx < initials.length() &&
                initials_first_good_idx < first_initials.length() && 
                toupper(initials[initials_first_good_idx]) == toupper(first_initials[initials_first_good_idx]) ;
            ++initials_first_good_idx )
        {
            // do nothing
        }

        if( initials_first_good_idx > 0 ) {
            initials.erase( 0, initials_first_good_idx );
        }
    }

    // like in C: nsp = TabbedStringToNameStdPtr (str, fixInitials);

    if( fix_initials && ! first_initials.empty() ) {
        name.SetInitials(
            first_initials + (name.IsSetInitials() ? name.GetInitials() : kEmptyStr));
    }
    if( name.IsSetInitials() ) {
        string & initials = name.SetInitials();
        NStr::ReplaceInPlace( initials, " ", "" );
        NStr::ReplaceInPlace( initials, ",", "." );
        NStr::ReplaceInPlace( initials, ".ST.", ".St." );

        string new_initials;
        string::const_iterator initials_iter = initials.begin();
        // modify initials.  New version will be built in new_initials
        for( ; initials_iter != initials.end(); ++initials_iter ) {
            const char ch = *initials_iter;
            switch( ch ) {
            case '-':
                // keep hyphens
                new_initials += '-';
                break;
            case '.':
            case ' ':
                // erase periods and spaces
                break;
            default:
                // other characters: keep them, BUT...
                new_initials += ch;

                if( (initials_iter + 1) != initials.end()) {
                    const char next_char = *(initials_iter + 1);
                    if (! islower(next_char) ) {
                        // if next character is not lower, add period
                        new_initials += '.';
                    }
                }
            }
        }
            
        if( initials != new_initials ) {
            initials.swap(new_initials); // swap is faster than assignment
            new_initials.clear();
        }

        // add period if string is not empty and doesn't end with a period
        if( ! initials.empty() && ! NStr::EndsWith(initials, ".") ) {
            initials += '.';
        }
    }

    // side effect of C Toolkit converting to tabbed string and back
    // is that some fields are removed
    if ( name.IsSetFull()) {
        name.ResetFull();
    }
    if (name.IsSetTitle()) {
        name.ResetTitle();
    }

    if(name.IsSetLast()) {
        // add dot to "et al"
        if ( NStr::Equal(name.GetLast(), "et al") ) {
            name.SetLast("et al.");
        }

        CleanVisString(name.SetLast());
        if (NStr::IsBlank(name.GetLast())) {
            name.ResetLast();
        }
    }

    if(name.IsSetFirst()) {
        CleanVisString(name.SetFirst());
        NStr::ReplaceInPlace(name.SetFirst(), sDot, "");
        if (NStr::IsBlank(name.GetFirst())) {
            name.ResetFirst();
        }
    }

    if(name.IsSetMiddle()) {
        CleanVisString(name.SetMiddle());
        if (NStr::IsBlank(name.GetMiddle())) {
            name.ResetMiddle();
        }
    }

    if(name.IsSetInitials()) {
        if (upcaseinits && islower(name.GetInitials()[0]) ) {
            name.SetInitials()[0] = toupper(name.GetInitials()[0]);
        }
        CleanVisString(name.SetInitials());
        if (NStr::IsBlank(name.GetInitials())) {
            name.ResetInitials();
        }
    }

    if (name.IsSetSuffix()) {
        NStr::ReplaceInPlace( name.SetSuffix(), sDot, kEmptyStr );
        NStr::TruncateSpacesInPlace(name.SetSuffix(), NStr::eTrunc_Begin );

        name.FixSuffix(name.SetSuffix());

        CleanVisString(name.SetSuffix());
        if (NStr::IsBlank(name.GetSuffix())) {
            name.ResetSuffix();
        }
    }

    s_FixEtAl( name );

    if( ! name.IsSetLast() ) {
        name.SetLast(kEmptyCStr);
    }

    name.ExtractSuffixFromLastName();

    if( name.IsSetInitials() && (!name.IsSetSuffix() || NStr::IsBlank(name.GetSuffix()))) {
        string & initials = name.SetInitials();
        if( NStr::EndsWith(initials, ".Jr.") || NStr::EndsWith(initials, ".Sr.") ) {
            name.SetSuffix(initials.substr( initials.length() - 3 ) );
            initials.resize( initials.length() - 3 );
            NStr::TruncateSpacesInPlace( initials );
            if (NStr::IsBlank(initials)) {
                name.ResetInitials();
            }
        }
    }

    return(!original_name->Equals(name));
}

// mapping of wrong suffixes to the correct ones.
typedef SStaticPair<const char*, const char*> TStringPair;
static const TStringPair bad_sfxs[] = {
    { "1d"  , "I" },
    { "1st" , "I" },
    { "2d"  , "II" },
    { "2nd" , "II" },
    { "3d"  , "III" },
    { "3rd" , "III" },
    { "4th" , "IV" },
    { "5th" , "V" },
    { "6th" , "VI" },
    //{ "I."  , "I" }, // presumably commented out since it resembles initials
    { "II." , "II" },
    { "III.", "III" },
    { "IV." , "IV" },
    { "Jr"  , "Jr." },
    { "Sr"  , "Sr." },    
    //{ "V."  , "V" }, // presumably commented out since it resembles initials
    { "VI." , "VI" }
};
typedef CStaticArrayMap<string, string> TSuffixMap;
DEFINE_STATIC_ARRAY_MAP_WITH_COPY(TSuffixMap, sc_BadSuffixes, bad_sfxs);

void CCleanup::s_ExtractSuffixFromInitials(CName_std& name)
{
    _ASSERT( name.IsSetInitials()  &&  ! name.IsSetSuffix() );

    string& initials = name.SetInitials();

    if (initials.find('.') == NPOS) {
        return;
    }

// this macro is arguably more convenient than a function
#define EXTRACTSUFFIXFROMINITIALS( OLD, NEW ) \
    if( NStr::EndsWith(initials, OLD) ) { \
        initials.resize( initials.length() - strlen(OLD) ); \
        name.SetSuffix(NEW); \
        return; \
    }

    EXTRACTSUFFIXFROMINITIALS( "III",  "III" )
    EXTRACTSUFFIXFROMINITIALS( "III.", "III" )
    EXTRACTSUFFIXFROMINITIALS( "Jr",   "Jr" )
    EXTRACTSUFFIXFROMINITIALS( "2nd",  "II" )
    EXTRACTSUFFIXFROMINITIALS( "IV",   "IV" )
    EXTRACTSUFFIXFROMINITIALS( "IV.",  "IV" )

#undef EXTRACTSUFFIXFROMINITIALS
}

void CCleanup::s_FixEtAl(CName_std& name)
{
    if (!name.IsSetLast() || !name.IsSetInitials()) {
        return;
    }
    if (name.IsSetFirst() && !NStr::Equal(name.GetFirst(), "a") &&
        !NStr::IsBlank(name.GetFirst()) ) {
        return;
    }
    const string& init = name.GetInitials();
    if( NStr::Equal(name.GetLast(), "et") &&
        ( NStr::Equal(init, "al")  || 
          NStr::Equal(init, "al.") ||
          NStr::Equal(init, "Al.") )) {
        name.ResetInitials();
        name.ResetFirst();
        name.SetLast("et al." );
    }
}




END_SCOPE(objects)
END_NCBI_SCOPE
