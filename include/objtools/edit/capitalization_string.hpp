#ifndef _CAPITALIZATION_INSTRINGS_HPP_
#define _CAPITALIZATION_INSTRINGS_HPP_
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
 *  and reliability of the software and data,  the NLM and the U.S.
 *  Government do not and cannot warrant the performance or results that
 *  may be obtained by using this software or data. The NLM and the U.S.
 *  Government disclaim all warranties,  express or implied,  including
 *  warranties of performance,  merchantability or fitness for any particular
 *  purpose.
 *
 *  Please cite the author in any work or product based on this material.
 *
 * ===========================================================================
 *
 * Authors:  Andrea Asztalos, Igor Filippov
 */

#include <corelib/ncbistd.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objects/biblio/Cit_sub.hpp>
#include <objects/biblio/Affil.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(edit)

enum ECapChange {
    eCapChange_none = 0,                /// no change
    eCapChange_tolower,                 /// change each letter to lower case
    eCapChange_toupper,                 /// change each letter to upper case
    eCapChange_firstcap_restlower,      /// capitalize the first letter, the rest is lower case
    eCapChange_firstcap_restnochange,   /// capitalize the first letter, the rest is not changed
    eCapChange_firstlower_restnochange, /// first letter is lower case, the rest is not changed
    eCapChange_capword_afterspace,      /// capitalize the first letter and letters after spaces
    eCapChange_capword_afterspacepunc   /// capitalize first letter and letters after spaces or punctuations
};

/// Capitalize the string according to desired capitalization option
NCBI_XOBJEDIT_EXPORT void FixCapitalizationInString (objects::CSeq_entry_Handle seh, string& str, ECapChange capchange_opt);
NCBI_XOBJEDIT_EXPORT void FixAbbreviationsInElement(string& result, bool fix_end_of_sentence = true);
NCBI_XOBJEDIT_EXPORT void FixOrgNames(objects::CSeq_entry_Handle seh, string& result);
NCBI_XOBJEDIT_EXPORT void FindOrgNames(objects::CSeq_entry_Handle seh, vector<string>& taxnames);

NCBI_XOBJEDIT_EXPORT void RemoveFieldNameFromString( const string& field_name, string& str);

NCBI_XOBJEDIT_EXPORT void GetStateAbbreviation(string& state);
NCBI_XOBJEDIT_EXPORT bool FixStateAbbreviationsInCitSub(CCit_sub& sub);
NCBI_XOBJEDIT_EXPORT bool FixStateAbbreviationsInAffil(CAffil& affil);
/// This function does not check whether the taxname starts with "Mus musculus", it only corrects the mouse strain value
NCBI_XOBJEDIT_EXPORT bool FixupMouseStrain(string& strain);

NCBI_XOBJEDIT_EXPORT void InsertMissingSpacesAfterCommas(string& result);
NCBI_XOBJEDIT_EXPORT void InsertMissingSpacesAfterNo(string& result);
NCBI_XOBJEDIT_EXPORT void FixCapitalizationInElement(string& result);
NCBI_XOBJEDIT_EXPORT void FixShortWordsInElement(string& result);
NCBI_XOBJEDIT_EXPORT void FindReplaceString_CountryFixes(string& result);
NCBI_XOBJEDIT_EXPORT void CapitalizeAfterApostrophe(string& input);
NCBI_XOBJEDIT_EXPORT void FixAffiliationShortWordsInElement(string& result);
NCBI_XOBJEDIT_EXPORT void FixOrdinalNumbers(string& result);
NCBI_XOBJEDIT_EXPORT void FixKnownAbbreviationsInElement(string& result);
NCBI_XOBJEDIT_EXPORT void CapitalizeSAfterNumber(string& result);
NCBI_XOBJEDIT_EXPORT void ResetCapitalization(string& result, bool first_is_upper);
NCBI_XOBJEDIT_EXPORT void FixCountryCapitalization(string& result);

END_SCOPE(edit)
END_SCOPE(objects)
END_NCBI_SCOPE

#endif   // _CAPITALIZATION_INSTRINGS_HPP_

