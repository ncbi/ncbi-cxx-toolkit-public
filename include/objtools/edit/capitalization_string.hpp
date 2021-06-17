
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
 * Authors: Justin Foley
 */

#ifndef _OBJTOOLS_EDIT_CAPITALIZATION_STRING_HPP_
#define _OBJTOOLS_EDIT_CAPITALIZATION_STRING_HPP_

#include <objtools/cleanup/capitalization_string.hpp>

// Temporary forwarding header.
//
// WARNING!
// The functions previously defined in src/objtools/edit/capitalization_string.cpp
// have been moved to src/cleanup/capitalization_string.cpp.
// Please update your code accordingly.
//

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(edit)

using ECapChange = objects::ECapChange;

#define CAPITALIZATION_STRING_ALIAS_ENUM_VALUE(enum_name) \
    static const auto enum_name = objects::enum_name;

CAPITALIZATION_STRING_ALIAS_ENUM_VALUE(eCapChange_none)
CAPITALIZATION_STRING_ALIAS_ENUM_VALUE(eCapChange_tolower)
CAPITALIZATION_STRING_ALIAS_ENUM_VALUE(eCapChange_toupper)
CAPITALIZATION_STRING_ALIAS_ENUM_VALUE(eCapChange_firstcap_restlower)
CAPITALIZATION_STRING_ALIAS_ENUM_VALUE(eCapChange_firstcap_restnochange)
CAPITALIZATION_STRING_ALIAS_ENUM_VALUE(eCapChange_firstlower_restnochange)
CAPITALIZATION_STRING_ALIAS_ENUM_VALUE(eCapChange_capword_afterspace)
CAPITALIZATION_STRING_ALIAS_ENUM_VALUE(eCapChange_capword_afterspacepunc)

#undef CAPITALIZATION_STRING_ALIAS_ENUM_VALUE


#define CAPITALIZATION_STRING_ALIAS_FUNCTION(function_name) \
    NCBI_DEPRECATED static auto& function_name = objects::function_name;

CAPITALIZATION_STRING_ALIAS_FUNCTION(FixCapitalizationInString)
//CAPITALIZATION_STRING_ALIAS_FUNCTION(FixAbbreviationsInElement)
CAPITALIZATION_STRING_ALIAS_FUNCTION(FixOrgNames)
CAPITALIZATION_STRING_ALIAS_FUNCTION(FindOrgNames)
CAPITALIZATION_STRING_ALIAS_FUNCTION(RemoveFieldNameFromString)
CAPITALIZATION_STRING_ALIAS_FUNCTION(GetStateAbbreviation)
CAPITALIZATION_STRING_ALIAS_FUNCTION(FixStateAbbreviationsInCitSub)
CAPITALIZATION_STRING_ALIAS_FUNCTION(FixUSAAbbreviationInAffil)
CAPITALIZATION_STRING_ALIAS_FUNCTION(FixStateAbbreviationsInAffil)
CAPITALIZATION_STRING_ALIAS_FUNCTION(FixupMouseStrain)
CAPITALIZATION_STRING_ALIAS_FUNCTION(InsertMissingSpacesAfterCommas)
CAPITALIZATION_STRING_ALIAS_FUNCTION(InsertMissingSpacesAfterNo)
CAPITALIZATION_STRING_ALIAS_FUNCTION(FixCapitalizationInElement)
CAPITALIZATION_STRING_ALIAS_FUNCTION(FixShortWordsInElement)
CAPITALIZATION_STRING_ALIAS_FUNCTION(FindReplaceString_CountryFixes)
CAPITALIZATION_STRING_ALIAS_FUNCTION(CapitalizeAfterApostrophe)
CAPITALIZATION_STRING_ALIAS_FUNCTION(FixAffiliationShortWordsInElement)
CAPITALIZATION_STRING_ALIAS_FUNCTION(FixOrdinalNumbers)
CAPITALIZATION_STRING_ALIAS_FUNCTION(FixKnownAbbreviationsInElement)
CAPITALIZATION_STRING_ALIAS_FUNCTION(CapitalizeSAfterNumber)
CAPITALIZATION_STRING_ALIAS_FUNCTION(ResetCapitalization)
CAPITALIZATION_STRING_ALIAS_FUNCTION(FixCountryCapitalization)

#undef CAPITALIZATION_STRING_ALIAS_FUNCTION

// Special case due to default parameter
NCBI_DEPRECATED static void FixAbbreviationsInElement(string& result, bool fix_end_of_sentence=true)
{
    objects::FixAbbreviationsInElement(result, fix_end_of_sentence);
}

END_SCOPE(edit)
END_SCOPE(objects)
END_NCBI_SCOPE

#endif // _OBJTOOLS_EDIT_CAPITALIZATION_STRING_HPP_
